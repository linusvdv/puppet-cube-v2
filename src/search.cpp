#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <stack>
#include <thread>

#include "BCHTSet.hpp"
#include "cube.hpp"
#include "logger.hpp"
#include "utils.hpp"
#include "random_position.hpp"
#include "search.hpp"
#include "settings.hpp"
#include "tablebase.hpp"


#ifdef USE_CUDA
#include "search_bridge.hpp"
#endif  // USE_CUDA


using Frontier = phmap::parallel_flat_hash_map<State, uint8_t, phmap::priv::hash_default_hash<State>, phmap::priv::hash_default_eq<State>, phmap::priv::Allocator<std::pair<State, uint8_t>>, 8, std::mutex>;


void SolveTB(std::vector<Rotations>& tb_rotations, int tb_layer, State state) {
    for (int layer = tb_layer - 1; layer >= 0; layer--) {
        for (uint8_t rotation = 0; rotation < kNumRotations; rotation++) {
            State next_state = Cube::Rotate(state, rotation).second;
            if (BCHTSetContains(Tablebase::tablebase[layer], next_state)) {
                state = next_state;
                tb_rotations.push_back(Rotations(rotation));
                break;
            }
        }
    }
}


// check if the state is already in tablebase
// return layer
// else return -1
int GetTBLayer(const State& state) {
    for (int i = 0; i <= Settings::GetTBDepth(); i++) {
        if (BCHTSetContains(Tablebase::tablebase[i], state)) {
            LOG_EXTRA("Position in tablebase");
            return i;
        }
    }
    return -1;
}


void SolveSearch(std::stack<Rotations>& search_rotations, int depth, State state, const VisitedMap& visited_search, const std::vector<VisitedMap>& visited_leaf_threads) {
    for (int i = depth-1; i >= 0; i--) {
        for (uint8_t rotation = 0; rotation < kNumRotations; rotation++) {
            State next_state = Cube::Rotate(state, rotation).second;
            if (auto vis = visited_search.find(next_state); vis != visited_search.end() && vis->second == i) {
                search_rotations.push(Rotations(GetRevRotation(rotation)));
                state = next_state;
                break;
            }
            bool stopped = false;
            for (const VisitedMap& visited_leaf : visited_leaf_threads) {
                if (auto vis = visited_leaf.find(next_state); vis != visited_leaf.end() && vis->second == i) {
                    search_rotations.push(Rotations(GetRevRotation(rotation)));
                    state = next_state;
                    stopped = true;
                    break;
                }
            }
            if (stopped) {
                break;
            }
        }
    }
}


// return true if a solution is contained
bool LeafSearch (const State& state, uint8_t depth, uint8_t& best_depth, std::pair<State, uint8_t>& best_endstate, VisitedMap& visited, uint64_t& leaft_search_positions, std::atomic<uint8_t>& atomic_best_depth, const int& thread_idx) {
    leaft_search_positions++;
    if (BCHTSetContains(Tablebase::tablebase.back(), state)) {
        if (depth + Settings::GetTBDepth() < best_depth) {
            best_endstate = {state, depth + Settings::GetTBDepth()};
            best_depth = std::min(best_depth, uint8_t(depth + Settings::GetTBDepth()));
            AtomicMin(atomic_best_depth, uint8_t(depth + Settings::GetTBDepth()));
            LOG_EXTRA("best sol", SkipSpace(thread_idx), ":", int(best_endstate.second), "leaf_search:", leaft_search_positions);
            LOG_MEMORY();
            return true;
        }
    }
    Cube cube;
    if (std::max(cube.GetMaxHeuristic(state), uint8_t(Settings::GetTBDepth()+1)) + depth >= best_depth) {
        return false;
    }
    bool is_solution = false;
    for (uint8_t rotation = 0; rotation < kNumRotations; rotation++) {
        std::pair<bool, State> next = Cube::Rotate(state, rotation);
        if (next.first) {
            bool res = LeafSearch(next.second, depth+1, best_depth, best_endstate, visited, leaft_search_positions, atomic_best_depth, thread_idx);
            if (res) {
                auto find_visited = visited.find(next.second);
                if (find_visited == visited.end()) {
                    visited.insert({next.second, uint8_t(depth+1)}); // found new solution
                }
                else if (find_visited->second > uint8_t(depth+1)) {
                    find_visited->second = uint8_t(depth+1);
                }
                is_solution = true;
            }
        }
    }
    return is_solution;
}


void LeafManager (std::stop_token stocken, uint64_t& num_positions_leaf, VisitedMap& visited_leaf, std::atomic<uint8_t>& atomic_best_depth, std::pair<State, uint8_t>& best_endstate_leafs,
                  SharedLeafStates& shared_leaf_states, const int& thread_idx) {
    LocalBuffer local_buffer;
    while (!stocken.stop_requested()) {
        {
            std::lock_guard<std::mutex> lock(shared_leaf_states.mtx);
            if (shared_leaf_states.shared_ptrs.empty()) {
                continue;
            }
            local_buffer = std::move(shared_leaf_states.shared_ptrs.front());
            shared_leaf_states.shared_ptrs.pop();
            shared_leaf_states.cv.notify_one();
        }

        for (const std::pair<State, uint8_t>& starting_position : *local_buffer) {
            uint8_t best_depth = atomic_best_depth;
            LeafSearch(starting_position.first, starting_position.second, best_depth,
                       best_endstate_leafs, visited_leaf,
                       num_positions_leaf, atomic_best_depth, thread_idx);
        }
    }
}


void FrontierInsert(Frontier& next_frontier, const State& state, uint8_t cur_depth) {
    next_frontier.try_emplace_l(state,
                                [cur_depth](Frontier::value_type& existing) {
                                    existing.second = std::min(cur_depth, existing.second);
                                },
                                cur_depth
                                );
}


void VisitedMapInsert(VisitedMap& visited_map, const State& state, uint8_t cur_depth) {
    visited_map.try_emplace_l(state,
                              [cur_depth](VisitedMap::value_type& existing) {
                              existing.second = std::min(cur_depth, existing.second);
                              },
                              cur_depth
                              );
}


void DFSNextFrontierSearch (const State& state, VisitedMap& visited_search, Frontier& next_frontier,
                            std::atomic<uint8_t>& atomic_best_depth, std::pair<State, uint8_t>& best_endstate_search,
                            LocalBuffer& local_buffer,
                            SharedLeafStates& shared_leaf_states,
                            uint64_t& num_positions_search, uint8_t cur_depth, uint8_t depth) {
    if (atomic_best_depth < depth) {
        return;
    }

    bool frontier_insert = false;
    // rotate to the next position
    for (uint8_t rotation = 0; rotation < kNumRotations; rotation++) {
        std::pair<bool, State> next_state = Cube::Rotate(state, rotation);
        // illegal move
        if (!next_state.first) {
            continue;
        }
        num_positions_search++;

        // already visited
        auto find_visited = visited_search.find(next_state.second);
        if (find_visited != visited_search.end() && find_visited->second <= cur_depth+1) {
            continue;
        }

        // in tablebase
        if (BCHTSetContains(Tablebase::tablebase.back(), next_state.second)) {
            best_endstate_search = {next_state.second, cur_depth+1};
            VisitedMapInsert(visited_search, next_state.second, cur_depth+1);
            atomic_best_depth = cur_depth+1+Settings::GetTBDepth();
            LOG_EXTRA("found solution of length ", Settings::GetTBDepth()+cur_depth+1);
            return;
        }

        // due to the heuristic it is not possible to solve the next state in fewer moves than the depth
        // this means that the state has to be again part of the new frontier
        Cube next_cube;
        if (std::max(next_cube.GetMaxHeuristic(next_state.second), uint8_t(Settings::GetTBDepth()+1)) + cur_depth + 1 >= depth) {
            frontier_insert = true;
            continue;
        }

        // send the position to GPU search
        // this means that the state has to be again part of the new frontier
        if (std::max(next_cube.GetMaxHeuristic(next_state.second), uint8_t(Settings::GetTBDepth()+1)) + cur_depth + 1 >= depth - 3 &&
            depth - cur_depth - 1 - Settings::GetTBDepth() < 16 &&  // fits in the rotation registers
            cur_depth + 1 > 5 &&  // more than 5 moves need to be already made
            depth - cur_depth - 1 - Settings::GetTBDepth() < 12) { // this value can be tweeked to have more cpu calculation needed

            local_buffer->push_back({next_state.second, cur_depth + 1});

            // insert local buffer when there is space
            if (local_buffer->size() >= size_t(Settings::GetNumPositionsPerBatch())) {
                std::unique_lock<std::mutex> lock(shared_leaf_states.mtx);
                shared_leaf_states.cv.wait(lock, [&] {
                    return int(shared_leaf_states.shared_ptrs.size()) <= Settings::GetNumParallelBatches() || atomic_best_depth < depth;
                });
                shared_leaf_states.shared_ptrs.push(std::move(local_buffer));
                local_buffer = std::make_shared<std::vector<std::pair<State, uint8_t>>>();
            }

            frontier_insert = true;
            continue;
        }

        // insert into visited_search
        VisitedMapInsert(visited_search, next_state.second, cur_depth+1);

        // Do further DFS
        DFSNextFrontierSearch(next_state.second, visited_search, next_frontier, atomic_best_depth, best_endstate_search, local_buffer, shared_leaf_states, num_positions_search, cur_depth+1, depth);
    }

    if (frontier_insert) {
        FrontierInsert(next_frontier, state, cur_depth);
    }
}


constexpr int kNumHeuristicLayers = 60;

void FrontierSearch (uint64_t& num_positions_search, VisitedMap& visited_search, std::atomic<uint8_t>& atomic_best_depth, std::pair<State, uint8_t>& best_endstate_search,
               SharedLeafStates& shared_leaf_states,
               uint8_t depth, std::vector<std::vector<std::vector<std::pair<State, uint8_t>>>>& cur_frontier, Frontier& next_frontier, int thread_idx) {
    // local buffer for leaf search
    LocalBuffer local_buffer = std::make_shared<std::vector<std::pair<State, uint8_t>>>();

    for (int i = 0; i < kNumHeuristicLayers; i++) {
        for (int j = 0; j < Settings::GetNumThreads(); j++) {
            // BFS layer
            for (int idx = thread_idx; idx < int(cur_frontier[i][j].size()); idx += Settings::GetNumThreads()) {
                    if (atomic_best_depth < depth) {
                        break;
                    }
                    DFSNextFrontierSearch(cur_frontier[i][j][idx].first, visited_search, next_frontier, atomic_best_depth, best_endstate_search, local_buffer, shared_leaf_states, num_positions_search, cur_frontier[i][j][idx].second, depth);
            }
        }
    }

    // insert element if the search is not finished with the current level
    if (local_buffer->size() > 0) {
        std::unique_lock<std::mutex> lock(shared_leaf_states.mtx);
        shared_leaf_states.cv.wait(lock, [&] {
            return int(shared_leaf_states.shared_ptrs.size()) <= Settings::GetNumParallelBatches() || atomic_best_depth < depth;
        });
        shared_leaf_states.shared_ptrs.push(std::move(local_buffer));
        local_buffer = std::make_shared<std::vector<std::pair<State, uint8_t>>>();
    }
}


void FrontierSort (const Frontier& next_frontier, std::vector<std::vector<std::vector<std::pair<State, uint8_t>>>>& cur_frontier, int thread_idx) {
    // BFS layer
    int idx = 0;
    for (const std::pair<const State, uint8_t>& position : next_frontier) {
        if (idx % Settings::GetNumThreads() == thread_idx) {
            Cube cube;
            uint8_t heuristic = cube.GetAppHeuristic(position.first);
            if (heuristic >= kNumHeuristicLayers) {
                LOG_CRITICAL("heuristic too big", heuristic);
            }
            cur_frontier[heuristic][thread_idx].push_back(position);
        }
        idx++;
    }
}


void SearchManager () {
    if (Settings::GetNumRuns() <= 0) {
        return;
    }

    int num_leaf_threads = Settings::GetNumThreads();
    #ifdef USE_CUDA
    if (Settings::UseCuda()) {
        num_leaf_threads = Settings::GetNumGPUUploadThreads();
    }
    #endif

    // start timing
    std::chrono::time_point start_time = std::chrono::high_resolution_clock::now();

    // random starting positions
    LOG_EXTRA("Start calculating random positions");
    std::vector<State> random_positions = RandomPositions(Settings::GetNumRuns(), Settings::GetRunOffset());
    LOG_EXTRA("Calculated random positions");

    // accumulated positions for information purposes
    uint64_t acc_total_num_positions = 0;
    uint64_t acc_depth = 0;

    for (size_t random_positions_idx = 0; random_positions_idx < random_positions.size(); random_positions_idx++) {
        // already in TB
        if (GetTBLayer(random_positions[random_positions_idx]) >= 0) {
            std::vector<Rotations> tb_rotations;
            SolveTB(tb_rotations, GetTBLayer(random_positions[random_positions_idx]), random_positions[random_positions_idx]);
            LOG_EXTRA("Proven optimal solution");
            LOG_EXTRA("Position already in tablebase");
            LOG_EXTRA("solution moves:", tb_rotations);
            LOG_ALL(SkipSpace("["), SkipSpace(random_positions_idx+1), SkipSpace("/"), SkipSpace(Settings::GetNumRuns()), "] Depth:", GetTBLayer(random_positions[random_positions_idx]), "num_positions: 0");
            continue;
        }

        // information purposes
        uint64_t total_num_positions = 0;
        uint64_t num_positions_search = 0;
        uint64_t num_positions_leaf = 0;

        // construction of optimal solution
        std::atomic<uint8_t> atomic_best_depth = uint8_t(-1);
        std::pair<State, uint8_t> best_endstate_search = {State(), -1};
        std::vector<std::pair<State, uint8_t>> best_endstate_leaf_threads(num_leaf_threads, {State(), -1});

        // these need to be exchanged of every deepening step
        std::vector<VisitedMap> visited_leaf_threads_final;

        // keep over the different depths
        // the frontier is moved to the position exactly before the cuts (stop because of max_heuristic or send to gpu)
        std::vector<std::vector<std::vector<std::pair<State, uint8_t>>>> cur_frontier(kNumHeuristicLayers, std::vector<std::vector<std::pair<State, uint8_t>>>(Settings::GetNumThreads()));
        cur_frontier[0][0].push_back({random_positions[random_positions_idx], 0});

        VisitedMap visited_search;
        visited_search[random_positions[random_positions_idx]] = 0;

        // sening data from search to LeafManager
        SharedLeafStates shared_leaf_states;

        // go over the different depths (iterative deepening)
        Cube heuristic;
        int max_heuristic = heuristic.GetMaxHeuristic(random_positions[random_positions_idx]);
        for (int id_depth = max_heuristic+1; true; id_depth++) {
            LOG_EXTRA("Start with depth", id_depth);
            atomic_best_depth = id_depth;

            // Start LeafManagers on seperate threads
            std::vector<std::jthread> leaf_manager_threads;
            std::vector<VisitedMap> visited_leaf_threads(num_leaf_threads);
            std::vector<uint64_t> num_positions_leaf_threads(num_leaf_threads, 0);

            #ifdef USE_CUDA
            SharedLeafSolution shared_leaf_solution = {{false}, State(), uint8_t(-1)};
            if (Settings::UseCuda()) {
                CudaConstMemChangeCurDepth(id_depth);

                for (int i = 0; i < Settings::GetNumGPUUploadThreads(); i++) {
                    leaf_manager_threads.push_back(std::jthread(DeviceLeafManager, std::ref(shared_leaf_states),
                                                                std::ref(visited_leaf_threads[i]),
                                                                std::ref(atomic_best_depth),
                                                                std::ref(shared_leaf_solution),
                                                                std::ref(num_positions_leaf_threads[i]), i));
                }
            }
            #endif
            // is always off if it is compiled without cuda
            if (!Settings::UseCuda()) {
                for (int i = 0; i < Settings::GetNumThreads(); i++) {
                    leaf_manager_threads.push_back(std::jthread(LeafManager, std::ref(num_positions_leaf_threads[i]), std::ref(visited_leaf_threads[i]),
                                                                std::ref(atomic_best_depth), std::ref(best_endstate_leaf_threads[i]),
                                                                std::ref(shared_leaf_states), i));
                }
            }

            // Search
            Frontier next_frontier;
            {
                std::vector<std::jthread> frontier_search_threads;
                for (int i = 0; i < Settings::GetNumThreads(); i++) {
                    frontier_search_threads.push_back(std::jthread(FrontierSearch, std::ref(num_positions_search), std::ref(visited_search), std::ref(atomic_best_depth),
                                                                   std::ref(best_endstate_search), std::ref(shared_leaf_states),
                                                                   id_depth, std::ref(cur_frontier), std::ref(next_frontier), i));
                }
            }
            cur_frontier = std::vector<std::vector<std::vector<std::pair<State, uint8_t>>>>(kNumHeuristicLayers, std::vector<std::vector<std::pair<State, uint8_t>>>(Settings::GetNumThreads()));
            {
                std::vector<std::jthread> frontier_sort;
                for (int i = 0; i < Settings::GetNumThreads(); i++) {
                    frontier_sort.push_back(std::jthread(FrontierSort, std::ref(next_frontier), std::ref(cur_frontier), i));
                }
            }

            // wait until queue is empty
            {
                std::unique_lock<std::mutex> lock(shared_leaf_states.mtx);
                shared_leaf_states.cv.wait(lock, [&] {
                    return shared_leaf_states.shared_ptrs.empty() || atomic_best_depth < id_depth;
                });
            }
            LOG_EXTRA("start with finishing search");

            // Stop LeafManager
            for (int i = 0; i < num_leaf_threads; i++) {
                leaf_manager_threads[i].request_stop();
            }
            for (int i = 0; i < num_leaf_threads; i++) {
                leaf_manager_threads[i].join();
                num_positions_leaf += num_positions_leaf_threads[i];
            }
            LOG_ERROR("Num position", num_positions_leaf+num_positions_search);

            // found optimal solution
            if (atomic_best_depth < id_depth) {
                atomic_best_depth = id_depth-1;
                LOG_EXTRA("Proven optimal solution");
                std::swap(visited_leaf_threads_final, visited_leaf_threads);
                break;
            }
        }

        total_num_positions = num_positions_search + num_positions_leaf;
        acc_depth += atomic_best_depth;
        acc_total_num_positions += total_num_positions;

        /*
        // Construct solution
        std::pair<State, uint8_t> best_endstate = best_endstate_search;
        for (int i = 0; i < num_leaf_threads; i++) {
            if (best_endstate_leaf_threads[i].second < best_endstate.second) {
                best_endstate = best_endstate_leaf_threads[i];
            }
        }

        // Tablebase
        std::vector<Rotations> tb_rotations;
        SolveTB(tb_rotations, Settings::GetTBDepth(), best_endstate.first);
        // Search
        std::stack<Rotations> search_rotations;
        SolveSearch(search_rotations, atomic_best_depth-Settings::GetTBDepth(), best_endstate.first, visited_search, visited_leaf_threads_final);

        LOG_EXTRA("solution moves:", search_rotations, tb_rotations);
        */

        LOG_ALL(SkipSpace("["), SkipSpace(random_positions_idx+1), SkipSpace("/"), SkipSpace(Settings::GetNumRuns()), "] Depth:", int(atomic_best_depth), "num_positions:", total_num_positions);
        LOG_EXTRA("total number positions:", total_num_positions, "search positions", num_positions_search, "leaf positions", num_positions_leaf);
        LOG_MEMORY();
    }

    // get the duration in milliseconds
    std::chrono::time_point since_epoch = std::chrono::high_resolution_clock::now();
    std::chrono::milliseconds millis = std::chrono::duration_cast<std::chrono::milliseconds>(since_epoch - start_time);

    // Some informations
    LOG_ALL("Average time:", millis.count()/Settings::GetNumRuns(), "ms");
    LOG_ALL("Average depth:", acc_depth/Settings::GetNumRuns());
    LOG_ALL("Average number of positions:", acc_total_num_positions/Settings::GetNumRuns());
    LOG_ALL("Positions per seconds:", acc_total_num_positions * 1000 / millis.count());
}
