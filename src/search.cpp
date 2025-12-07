#include <atomic>
#include <cstdint>
#include <mutex>
#include <queue>
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


struct PQSearch {
    uint8_t value;
    uint8_t depth;
    State state;

    std::strong_ordering operator<=>(const PQSearch&) const = default;
};


void SolveTB(std::stack<Rotations>& tb_rotations, int tb_layer, State state) {
    for (int layer = tb_layer - 1; layer >= 0; layer--) {
        for (uint8_t rotation = 0; rotation < kNumRotations; rotation++) {
            State next_state = Cube::Rotate(state, rotation).second;
            if (BCHTSetContains(Tablebase::tablebase[layer], next_state)) {
                state = next_state;
                tb_rotations.push(Rotations(GetRevRotation(rotation)));
                break;
            }
        }
    }
}


void SolveSearch(std::vector<Rotations>& search_rotations, int depth, State state, const VisitedMap& visited_search, const std::vector<VisitedMap>& visited_leaf_threads) {
    for (int i = depth-1; i >= 0; i--) {
        for (uint8_t rotation = 0; rotation < kNumRotations; rotation++) {
            State next_state = Cube::Rotate(state, rotation).second;
            if (auto vis = visited_search.find(next_state); vis != visited_search.end() && vis->second == i) {
                search_rotations.push_back(Rotations(GetRevRotation(rotation)));
                state = next_state;
                break;
            }
            bool stopped = false;
            for (const VisitedMap& visited_leaf : visited_leaf_threads) {
                if (auto vis = visited_leaf.find(next_state); vis != visited_leaf.end() && vis->second == i) {
                    search_rotations.push_back(Rotations(GetRevRotation(rotation)));
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
    if (std::max(cube.GetMaxHeuristic(state), Settings::GetTBDepth()) + depth >= best_depth) {
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
    #ifdef USE_CUDA
    if (Settings::UseCuda()) {
        DeviceLeafManager(stocken, shared_leaf_states, mtx, num_positions_leaf, visited_leaf,
                          atomic_best_depth, best_endstate_leafs, leaf_batch_size, thread_idx);
        return;
    }
    #endif

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


void Search (uint64_t& num_positions_search, VisitedMap& visited_search, std::atomic<uint8_t>& atomic_best_depth,
             std::pair<State, uint8_t>& best_endstate_search, const State& starting_position,
             std::atomic<std::shared_ptr<phmap::flat_hash_map<State, uint8_t>>>& shared_leaf_states, const uint64_t& leaf_batch_size) {
    uint8_t best_depth = atomic_best_depth;

    // check if it already in tablebase
    for (int i = 0; i <= Settings::GetTBDepth(); i++) {
        if (BCHTSetContains(Tablebase::tablebase[i], starting_position)) {
            LOG_EXTRA("Position in tablebase");
            atomic_best_depth = i;
            best_endstate_search = {starting_position, i};
            return;
        }
    }

    // priority queue sorted after the heuristic value
    std::priority_queue<PQSearch, std::vector<PQSearch>, std::greater<>> pq_search;
    Cube start_cube;
    pq_search.push({start_cube.GetMaxHeuristic(starting_position), 0, starting_position});
    visited_search.insert({starting_position, 0});
    num_positions_search++;

    // local buffer for leaf search
    std::shared_ptr<phmap::flat_hash_map<State, uint8_t>> local_buffer = std::make_shared<phmap::flat_hash_map<State, uint8_t>>();

    // start of search
    while (num_positions_search < Settings::GetNumPositions() && !pq_search.empty()) {
        PQSearch pq_top = pq_search.top();
        pq_search.pop();

        // rotate to the next position
        for (uint8_t rotation = 0; rotation < kNumRotations; rotation++) {
            std::pair<bool, State> next_state = Cube::Rotate(pq_top.state, rotation);
            if (!next_state.first) {
                continue;
            }

            // tablebase
            uint8_t depth = pq_top.depth + 1 + Settings::GetTBDepth();
            if (BCHTSetContains(Tablebase::tablebase.back(), next_state.second)) {
                if (depth < best_depth) {
                    // set new enstate
                    best_endstate_search = {next_state.second, depth};
                    AtomicMin(atomic_best_depth, depth);

                    best_depth = atomic_best_depth;

                    visited_search[next_state.second] = depth;
                    LOG_EXTRA("best sol:", int(depth), "num_positions:", num_positions_search);
                    LOG_MEMORY();
                }
            }

            // due to the heuristic it is not possible to solve the current state in fewer moves than the current best solution
            Cube next_cube;
            if (std::max(next_cube.GetMaxHeuristic(next_state.second), Settings::GetTBDepth()) + pq_top.depth + 1 >= best_depth) {
                continue;
            }

            // add to visited states
            auto find_visited = visited_search.find(next_state.second);
            if (find_visited == visited_search.end()) {
                visited_search.insert({next_state.second, pq_top.depth + 1});
            }
            else if (pq_top.depth + 1 < find_visited->second) {
                find_visited->second = pq_top.depth + 1;
            }
            else {
                continue;
            }

            num_positions_search++;

            if (next_cube.GetMaxHeuristic(next_state.second) + pq_top.depth + 1 > best_depth - 4 && best_depth < 30 + Settings::GetTBDepth()) {
                auto find_local_buffer = local_buffer->find(next_state.second);
                if (find_local_buffer == local_buffer->end()) {
                    local_buffer->insert({next_state.second, pq_top.depth+1});
                }
                else {
                    find_local_buffer->second = pq_top.depth+1;
                }

                // swap local buffer with shared leaf states when it is swaped with an empty one
                if (local_buffer->size() >= leaf_batch_size) {
                    auto shared_data = shared_leaf_states.load(std::memory_order_acquire);
                    while (!shared_data->empty()) {
                        std::this_thread::yield(); // prevent busy spin burn
                        shared_data = shared_leaf_states.load(std::memory_order_acquire);
                    }

                    local_buffer = shared_leaf_states.exchange(local_buffer, std::memory_order_acq_rel);

                    best_depth = atomic_best_depth;
                }
            }
            else {
                pq_search.push({uint8_t(next_cube.GetAppHeuristic(next_state.second)+pq_top.depth+1), uint8_t(pq_top.depth+1), next_state.second});
            }
        }
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
}


void SearchManager () {
    if (Settings::GetNumRuns() <= 0) {
        return;
    }

    // start timing
    std::chrono::time_point start_time = std::chrono::high_resolution_clock::now();

    // random starting positions
    std::vector<State> random_positions = RandomPositions(Settings::GetNumRuns(), 0);
    LOG_EXTRA("Calculated RandomPositions");

    // accumulated positions for information purposes
    uint64_t acc_total_num_positions = 0;
    uint64_t acc_depth = 0;

    for (size_t i = 0; i < random_positions.size(); i++) {

        uint64_t total_num_positions = 0;
        uint64_t num_positions_search = 0;
        uint64_t num_positions_leaf = 0;

        VisitedMap visited_search;

        std::atomic<uint8_t> atomic_best_depth = uint8_t(-1);

        std::pair<State, uint8_t> best_endstate_search = {State(), -1};
        std::pair<State, uint8_t> best_endstate_leaf = {State(), -1};

        // sening data from search to LeafManager
        std::atomic<std::shared_ptr<phmap::flat_hash_map<State, uint8_t>>> shared_leaf_states;
        shared_leaf_states.store(
            std::make_shared<phmap::flat_hash_map<State, uint8_t>>(),
            std::memory_order_release
        );
        uint64_t leaf_batch_size = kBlockDim * 46 * 16;

        // Start LeafManager on a seperate thread
        std::vector<std::jthread> leaf_manager_threads;
        std::vector<uint64_t> num_positions_leaf_threads(Settings::GetNumThreads(), 0);
        std::vector<VisitedMap> visited_leaf_threads(Settings::GetNumThreads());
        std::vector<std::pair<State, uint8_t>> best_endstate_leaf_threads(Settings::GetNumThreads(), {State(), -1});
        std::mutex mtx;
        for (int i = 0; i < Settings::GetNumThreads(); i++) {
            leaf_manager_threads.push_back(std::jthread(LeafManager, std::ref(num_positions_leaf_threads[i]), std::ref(visited_leaf_threads[i]),
                                                        std::ref(atomic_best_depth), std::ref(best_endstate_leaf_threads[i]),
                                                        std::ref(shared_leaf_states), leaf_batch_size, i, std::ref(mtx)));
        }

        // Search
        Search(num_positions_search, visited_search, atomic_best_depth, best_endstate_search, random_positions[i], shared_leaf_states, leaf_batch_size);

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
            if (best_endstate_leaf.second > best_endstate_leaf_threads[i].second) {
                best_endstate_leaf = best_endstate_leaf_threads[i];
            }
        }

        total_num_positions = num_positions_search + num_positions_leaf;

        if (num_positions_search < Settings::GetNumPositions()) {
            LOG_EXTRA("Optimal solution found!");
        }

        // Construct solution
        acc_depth += atomic_best_depth;
        acc_total_num_positions += total_num_positions;
        if (atomic_best_depth != uint8_t(-1)) {
            // state between search and tablebase
            std::pair<State, uint8_t> best_endstate;
            if (best_endstate_search.second <= best_endstate_leaf.second) {
                best_endstate = best_endstate_search;
            }
            else {
                best_endstate = best_endstate_leaf;
            }

            // already in TB
            if (atomic_best_depth <= Settings::GetTBDepth()) {
                std::stack<Rotations> tb_rotations;
                SolveTB(tb_rotations, atomic_best_depth, best_endstate.first);
                LOG_EXTRA(tb_rotations);
            }
            // mix of TB and Search
            else {
                // Tablebase
                std::stack<Rotations> tb_rotations;
                SolveTB(tb_rotations, Settings::GetTBDepth(), best_endstate.first);

                // Search
                std::vector<Rotations> search_rotations;
                SolveSearch(search_rotations, atomic_best_depth-Settings::GetTBDepth(), best_endstate.first, visited_search, visited_leaf_threads);

                LOG_EXTRA(tb_rotations, search_rotations);
            }
        }
        else {
            LOG_EXTRA("Solution not found!");
        }

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
