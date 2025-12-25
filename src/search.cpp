#include <atomic>
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
                  std::atomic<std::shared_ptr<phmap::flat_hash_map<State, uint8_t>>>& shared_leaf_states, [[maybe_unused]] const uint64_t& leaf_batch_size, const int& thread_idx, std::mutex& mtx) {
    std::shared_ptr<phmap::flat_hash_map<State, uint8_t>> local_buffer;
    while (!stocken.stop_requested()) {
        bool is_new = false;
        {
            std::lock_guard lock(mtx);
            std::shared_ptr<phmap::flat_hash_map<State, uint8_t>> shared_data = shared_leaf_states.load(std::memory_order_acquire);
            if (shared_data->size() > 0) {
                std::shared_ptr<phmap::flat_hash_map<State, uint8_t>> new_empty_buffer = std::make_shared<phmap::flat_hash_map<State, uint8_t>>();
                local_buffer = shared_leaf_states.exchange(new_empty_buffer, std::memory_order_acquire);
                is_new = true;
            }
        }

        if (is_new) {
            for (const std::pair<State, uint8_t> starting_position : *local_buffer) {
                uint8_t best_depth = atomic_best_depth;
                LeafSearch(starting_position.first, starting_position.second, best_depth,
                        best_endstate_leafs, visited_leaf,
                        num_positions_leaf, atomic_best_depth, thread_idx);
            }
        }
        else {
            std::this_thread::yield(); // prevent busy spin burn
        }
    }
}


// This is a BFS search
void Search (uint64_t& num_positions_search, VisitedMap visited_search, std::atomic<uint8_t>& atomic_best_depth, std::pair<State, uint8_t>& best_endstate_search,
               const State& starting_position, std::atomic<std::shared_ptr<phmap::flat_hash_map<State, uint8_t>>>& shared_leaf_states,
               const uint64_t& leaf_batch_size, uint8_t depth) {
    // local buffer for leaf search
    std::shared_ptr<phmap::flat_hash_map<State, uint8_t>> local_buffer = std::make_shared<phmap::flat_hash_map<State, uint8_t>>();

    // BFS layer
    phmap::flat_hash_set<State> cur_states;
    cur_states.insert(starting_position);
    visited_search.insert({starting_position, 0});

    for (uint8_t cur_depth = 0; cur_depth < depth; cur_depth++) {
        // new bfs layer
        phmap::flat_hash_set<State> next_states;

        // iterate over all states
        for (const State& state : cur_states) {
            // rotate to the next position
            for (uint8_t rotation = 0; rotation < kNumRotations; rotation++) {
                std::pair<bool, State> next_state = Cube::Rotate(state, rotation);
                // illegal move
                if (!next_state.first) {
                    continue;
                }
                // already visited
                if (visited_search.contains(next_state.second)) {
                    continue;
                }
                num_positions_search++;

                // in tablebase
                if (BCHTSetContains(Tablebase::tablebase.back(), next_state.second)) {
                    best_endstate_search = {next_state.second, cur_depth+1};
                    visited_search.insert({next_state.second, cur_depth+1});
                    atomic_best_depth = cur_depth+1+Settings::GetTBDepth();
                    LOG_EXTRA("found solution of length ", Settings::GetTBDepth()+cur_depth+1);
                    return;
                }

                // due to the heuristic it is not possible to solve the current state in fewer moves than the current best solution
                // tablebase lookup already happend
                Cube next_cube;
                if (std::max(next_cube.GetMaxHeuristic(next_state.second), uint8_t(Settings::GetTBDepth()+1)) + cur_depth + 1 >= depth) {
                    continue;
                }

                visited_search.insert({next_state.second, cur_depth+1});

                // send the position to GPU search
                if (std::max(next_cube.GetMaxHeuristic(next_state.second), uint8_t(Settings::GetTBDepth()+1)) + cur_depth + 1 >= depth - 3 &&
                    depth < 30 + Settings::GetTBDepth() + cur_depth + 1 &&  // fits in the rotation registers
                    cur_depth + 1 > 5 &&  // more than 5 moves need to be already made
                    depth - cur_depth - 1 - Settings::GetTBDepth() < 10) { // less than 10 to go

                    auto find_local_buffer = local_buffer->find(next_state.second);
                    if (find_local_buffer == local_buffer->end()) {
                        local_buffer->insert({next_state.second, cur_depth + 1});
                    }
                    else if (find_local_buffer->second > cur_depth + 1) {
                        find_local_buffer->second = cur_depth + 1;
                    }

                    // swap local buffer with shared leaf states when it is swaped with an empty one
                    if (local_buffer->size() >= leaf_batch_size) {
                        auto shared_data = shared_leaf_states.load(std::memory_order_acquire);
                        while (!shared_data->empty()) {
                            std::this_thread::yield(); // prevent busy spin burn
                            shared_data = shared_leaf_states.load(std::memory_order_acquire);
                        }

                        local_buffer = shared_leaf_states.exchange(local_buffer, std::memory_order_acq_rel);
                    }

                    if (atomic_best_depth < depth) {
                        return;
                    }
                    continue;
                }

                next_states.insert(next_state.second);
            }
        }

        // swap new and old bfs layer
        std::swap(cur_states, next_states);
    }
}


void SearchManager () {
    if (Settings::GetNumRuns() <= 0) {
        return;
    }

    uint64_t leaf_batch_size = kBlockDim * 46 * 16;

    // start timing
    std::chrono::time_point start_time = std::chrono::high_resolution_clock::now();

    // random starting positions
    LOG_EXTRA("Start calculating random positions");
    std::vector<State> random_positions = RandomPositions(Settings::GetNumRuns(), 0);
    LOG_EXTRA("Calculated random positions");

    // accumulated positions for information purposes
    uint64_t acc_total_num_positions = 0;
    uint64_t acc_depth = 0;

    for (size_t i = 0; i < random_positions.size(); i++) {
        // already in TB
        if (GetTBLayer(random_positions[i]) >= 0) {
            std::vector<Rotations> tb_rotations;
            SolveTB(tb_rotations, GetTBLayer(random_positions[i]), random_positions[i]);
            LOG_EXTRA("Proven optimal solution");
            LOG_EXTRA("Position already in tablebase");
            LOG_EXTRA("solution moves:", tb_rotations);
            LOG_ALL(SkipSpace("["), SkipSpace(i+1), SkipSpace("/"), SkipSpace(Settings::GetNumRuns()), "] Depth:", GetTBLayer(random_positions[i]), "num_positions: 0");
            continue;
        }

        // information purposes
        uint64_t total_num_positions = 0;
        uint64_t num_positions_search = 0;
        uint64_t num_positions_leaf = 0;

        // construction of optimal solution
        std::atomic<uint8_t> atomic_best_depth = uint8_t(-1);
        std::pair<State, uint8_t> best_endstate_search = {State(), -1};
        std::vector<std::pair<State, uint8_t>> best_endstate_leaf_threads(Settings::GetNumThreads(), {State(), -1});

        // these need to be exchanged of every deepening step
        std::vector<VisitedMap> visited_leaf_threads_final(Settings::GetNumThreads());
        VisitedMap visited_search_final;

        // sening data from search to LeafManager
        std::atomic<std::shared_ptr<phmap::flat_hash_map<State, uint8_t>>> shared_leaf_states;
        shared_leaf_states.store(
            std::make_shared<phmap::flat_hash_map<State, uint8_t>>(),
            std::memory_order_release
        );

        // Start LeafManager on a seperate thread
        Cube heuristic;
        int max_heuristic = heuristic.GetMaxHeuristic(random_positions[i]);
        for (int id_depth = max_heuristic+1; true; id_depth++) {
            LOG_EXTRA("Start with depth", id_depth);
            atomic_best_depth = id_depth;
            VisitedMap visited_search;
            std::vector<VisitedMap> visited_leaf_threads(Settings::GetNumThreads());

            std::vector<std::jthread> leaf_manager_threads;
            std::vector<uint64_t> num_positions_leaf_threads(Settings::GetNumThreads(), 0);
            std::mutex mtx;
            #ifdef USE_CUDA
            if (Settings::UseCuda()) {
                for (int i = 0; i < Settings::GetNumThreads(); i++) {
                    leaf_manager_threads.push_back(std::jthread(DeviceLeafManager, std::ref(shared_leaf_states), std::ref(mtx),
                                                                std::ref(num_positions_leaf_threads[i]), std::ref(visited_leaf_threads[i]),
                                                                std::ref(atomic_best_depth), std::ref(best_endstate_leaf_threads[i]), leaf_batch_size, i));
                }
            }
            #endif
            // is always off if it is compiled without cuda
            if (!Settings::UseCuda()) {
                for (int i = 0; i < Settings::GetNumThreads(); i++) {
                    leaf_manager_threads.push_back(std::jthread(LeafManager, std::ref(num_positions_leaf_threads[i]), std::ref(visited_leaf_threads[i]),
                                                                std::ref(atomic_best_depth), std::ref(best_endstate_leaf_threads[i]),
                                                                std::ref(shared_leaf_states), leaf_batch_size, i, std::ref(mtx)));
                }
            }

            // Search
            Search(num_positions_search, visited_search, atomic_best_depth, best_endstate_search, random_positions[i], shared_leaf_states, leaf_batch_size, id_depth);

            // wait until all shared position are empty
            auto shared_data = shared_leaf_states.load(std::memory_order_acquire);
            while (!shared_data->empty()) {
                shared_data = shared_leaf_states.load(std::memory_order_acquire);
            }
            LOG_EXTRA("start with finishing search");

            // Stop LeafManager
            for (int i = 0; i < Settings::GetNumThreads(); i++) {
                leaf_manager_threads[i].request_stop();
            }
            for (int i = 0; i < Settings::GetNumThreads(); i++) {
                leaf_manager_threads[i].join();
                num_positions_leaf += num_positions_leaf_threads[i];
            }

            // found optimal solution
            if (atomic_best_depth < id_depth) {
                LOG_EXTRA("Proven optimal solution");
                std::swap(visited_leaf_threads_final, visited_leaf_threads);
                std::swap(visited_search_final, visited_search);
                break;
            }
        }

        total_num_positions = num_positions_search + num_positions_leaf;
        acc_depth += atomic_best_depth;
        acc_total_num_positions += total_num_positions;

        // Construct solution
        std::pair<State, uint8_t> best_endstate = best_endstate_search;
        for (int i = 0; i < Settings::GetNumThreads(); i++) {
            if (best_endstate_leaf_threads[i].second < best_endstate.second) {
                best_endstate = best_endstate_leaf_threads[i];
            }
        }

        // Tablebase
        std::vector<Rotations> tb_rotations;
        SolveTB(tb_rotations, Settings::GetTBDepth(), best_endstate.first);
        // Search
        std::stack<Rotations> search_rotations;
        SolveSearch(search_rotations, atomic_best_depth-Settings::GetTBDepth(), best_endstate.first, visited_search_final, visited_leaf_threads_final);

        LOG_EXTRA("solution moves:", search_rotations, tb_rotations);

        LOG_ALL(SkipSpace("["), SkipSpace(i+1), SkipSpace("/"), SkipSpace(Settings::GetNumRuns()), "] Depth:", int(atomic_best_depth), "num_positions:", total_num_positions);
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
