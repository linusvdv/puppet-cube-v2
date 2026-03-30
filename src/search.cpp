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
bool LeafSearch (const State& state, uint8_t depth, uint8_t& best_depth, VisitedMap& visited, uint64_t& leaft_search_positions, SharedLeafSolution& shared_leaf_solution, SharedLeafStates& shared_leaf_states) {
    leaft_search_positions++;
    if (BCHTSetContains(Tablebase::tablebase.back(), state)) {
        if (depth + Settings::GetTBDepth() < best_depth) {
            bool expected = false;
            if (shared_leaf_solution.finished.compare_exchange_strong(expected, true, std::memory_order_acq_rel, std::memory_order_acquire)) {
                shared_leaf_states.cv.notify_all();
                shared_leaf_solution.state = state;
            }
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
            bool res = LeafSearch(next.second, depth+1, best_depth, visited, leaft_search_positions, shared_leaf_solution, shared_leaf_states);
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


void LeafManager (std::stop_token stocken, uint8_t& best_depth, uint64_t& num_positions_leaf, VisitedMap& visited_leaf, SharedLeafSolution& shared_leaf_solution,
                  SharedLeafStates& shared_leaf_states) {
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
            if (shared_leaf_solution.finished.load(std::memory_order_acquire)) {
                break;
            }
            LeafSearch(starting_position.first, starting_position.second, best_depth,
                       visited_leaf, num_positions_leaf, shared_leaf_solution, shared_leaf_states);
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
                            SharedLeafSolution& shared_leaf_solution,
                            LocalBuffer& local_buffer,
                            SharedLeafStates& shared_leaf_states,
                            uint64_t& num_positions_search, uint8_t cur_depth, uint8_t depth) {
    if (shared_leaf_solution.finished.load(std::memory_order_acquire)) {
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
            bool expected = false;
            if (shared_leaf_solution.finished.compare_exchange_strong(expected, true, std::memory_order_acq_rel, std::memory_order_acquire)) {
                shared_leaf_states.cv.notify_all();
                shared_leaf_solution.state = state;
            }
            VisitedMapInsert(visited_search, next_state.second, cur_depth+1);
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
            depth - cur_depth - 1 - Settings::GetTBDepth() < 13) { // this value can be tweeked to have more cpu calculation needed

            local_buffer->push_back({next_state.second, cur_depth + 1});

            // insert local buffer when there is space
            if (local_buffer->size() >= size_t(Settings::GetNumPositionsPerBatch())) {
                std::unique_lock<std::mutex> lock(shared_leaf_states.mtx);
                shared_leaf_states.cv.wait(lock, [&] {
                    return int(shared_leaf_states.shared_ptrs.size()) <= Settings::GetNumParallelBatches() || shared_leaf_solution.finished.load(std::memory_order_acquire);
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
        DFSNextFrontierSearch(next_state.second, visited_search, next_frontier, shared_leaf_solution, local_buffer, shared_leaf_states, num_positions_search, cur_depth+1, depth);
    }

    if (frontier_insert) {
        FrontierInsert(next_frontier, state, cur_depth);
    }
}


void FrontierSearch (uint64_t& num_positions_search, VisitedMap& visited_search, SharedLeafSolution& shared_leaf_solution,
               SharedLeafStates& shared_leaf_states,
               uint8_t depth, const Frontier& cur_frontier, Frontier& next_frontier, std::atomic<long long>& atmoic_idx) {
    // local buffer for leaf search
    LocalBuffer local_buffer = std::make_shared<std::vector<std::pair<State, uint8_t>>>();

    // get the correct frontier element
    auto frontier_it = cur_frontier.begin();
    long long frontier_idx = 0;
    while (true) {
        long long cur_idx = atmoic_idx++;
        if (size_t(cur_idx) >= cur_frontier.size()) {
            break;
        }
        frontier_it = std::next(frontier_it, cur_idx-frontier_idx);
        frontier_idx = cur_idx;
        if (shared_leaf_solution.finished.load(std::memory_order_acquire)) {
            break;
        }

        DFSNextFrontierSearch(frontier_it->first, visited_search, next_frontier, shared_leaf_solution, local_buffer, shared_leaf_states, num_positions_search, frontier_it->second, depth);
    }

    // insert element if the search is not finished with the current level
    if (local_buffer->size() > 0) {
        std::unique_lock<std::mutex> lock(shared_leaf_states.mtx);
        shared_leaf_states.cv.wait(lock, [&] {
            return int(shared_leaf_states.shared_ptrs.size()) <= Settings::GetNumParallelBatches() || shared_leaf_solution.finished.load(std::memory_order_acquire);
        });
        shared_leaf_states.shared_ptrs.push(std::move(local_buffer));
        local_buffer = std::make_shared<std::vector<std::pair<State, uint8_t>>>();
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
        SharedLeafStates shared_leaf_states;

        // these need to be exchanged of every deepening step
        std::vector<VisitedMap> visited_leaf_threads_final;

        // keep over the different depths
        // the frontier is moved to the position exactly before the cuts (stop because of max_heuristic or send to gpu)
        Frontier cur_frontier;
        cur_frontier.insert({random_positions[random_positions_idx], 0});

        VisitedMap visited_search;
        visited_search[random_positions[random_positions_idx]] = 0;

        // go over the different depths (iterative deepening)
        Cube heuristic;
        int max_heuristic = heuristic.GetMaxHeuristic(random_positions[random_positions_idx]);
        uint8_t id_depth = max_heuristic+1;
        SharedLeafSolution shared_leaf_solution = {{false}, State()};

        for (; true; id_depth++) {
            LOG_EXTRA("Start with depth", id_depth);

            // Start LeafManagers on seperate threads
            std::vector<std::jthread> leaf_manager_threads;
            std::vector<VisitedMap> visited_leaf_threads(num_leaf_threads);
            std::vector<uint64_t> num_positions_leaf_threads(num_leaf_threads, 0);

            #ifdef USE_CUDA
            if (Settings::UseCuda()) {
                CudaConstMemChangeCurDepth(id_depth);

                for (int i = 0; i < Settings::GetNumGPUUploadThreads(); i++) {
                    leaf_manager_threads.push_back(std::jthread(DeviceLeafManager, std::ref(shared_leaf_states),
                                                                std::ref(visited_leaf_threads[i]),
                                                                std::ref(shared_leaf_solution),
                                                                std::ref(num_positions_leaf_threads[i]), i, id_depth));
                }
            }
            #endif
            // is always off if it is compiled without cuda
            if (!Settings::UseCuda()) {
                for (int i = 0; i < Settings::GetNumThreads(); i++) {
                    leaf_manager_threads.push_back(std::jthread(LeafManager, std::ref(id_depth), std::ref(num_positions_leaf_threads[i]),
                                                                std::ref(visited_leaf_threads[i]),
                                                                std::ref(shared_leaf_solution),
                                                                std::ref(shared_leaf_states)));
                }
            }

            // Search
            Frontier next_frontier;
            std::atomic<long long> atmoic_frontier_idx = 0;
            {
                std::vector<std::jthread> frontier_search_threads;
                for (int i = 0; i < Settings::GetNumThreads(); i++) {
                    frontier_search_threads.push_back(std::jthread(FrontierSearch, std::ref(num_positions_search), std::ref(visited_search), std::ref(shared_leaf_solution), std::ref(shared_leaf_states),
                                                                   id_depth, std::ref(cur_frontier), std::ref(next_frontier), std::ref(atmoic_frontier_idx)));
                }
            }
            std::swap(cur_frontier, next_frontier);

            // wait until queue is empty
            {
                std::unique_lock<std::mutex> lock(shared_leaf_states.mtx);
                shared_leaf_states.cv.wait(lock, [&] {
                    return shared_leaf_states.shared_ptrs.empty() || shared_leaf_solution.finished.load(std::memory_order_acquire);
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
            LOG_EXTRA("Num position", num_positions_leaf+num_positions_search);

            // found optimal solution
            if (shared_leaf_solution.finished.load(std::memory_order_acquire)) {
                LOG_EXTRA("Proven optimal solution");
                std::swap(visited_leaf_threads_final, visited_leaf_threads);
                break;
            }
        }

        id_depth--;
        total_num_positions = num_positions_search + num_positions_leaf;
        acc_depth += id_depth;
        acc_total_num_positions += total_num_positions;

        // Tablebase
        std::vector<Rotations> tb_rotations;
        SolveTB(tb_rotations, Settings::GetTBDepth(), shared_leaf_solution.state);
        // Search
        std::stack<Rotations> search_rotations;
        SolveSearch(search_rotations, id_depth-Settings::GetTBDepth(), shared_leaf_solution.state, visited_search, visited_leaf_threads_final);

        LOG_EXTRA("solution moves:", search_rotations, tb_rotations);

        LOG_ALL(SkipSpace("["), SkipSpace(random_positions_idx+1), SkipSpace("/"), SkipSpace(Settings::GetNumRuns()), "] Depth:", int(id_depth), "num_positions:", total_num_positions);
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
