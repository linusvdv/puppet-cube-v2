#include <atomic>
#include <cstdint>
#include <queue>
#include <stack>
#include <thread>
#include <parallel_hashmap/phmap.h>

#include "BCHTSet.hpp"
#include "cube.hpp"
#include "logger.hpp"
#include "random_position.hpp"
#include "settings.hpp"
#include "tablebase.hpp"


struct PQSearch {
    uint8_t value;
    uint8_t depth;
    State state;

    std::strong_ordering operator<=>(const PQSearch&) const = default;
};


using VisitedMap = phmap::flat_hash_map<State, uint8_t>;


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


void SolveSearch(std::vector<Rotations>& search_rotations, int depth, State state, const VisitedMap& visited_search, const VisitedMap& visited_leaf) {
    for (int i = depth-1; i >= 0; i--) {
        for (uint8_t rotation = 0; rotation < kNumRotations; rotation++) {
            State next_state = Cube::Rotate(state, rotation).second;
            if (auto vis = visited_search.find(next_state); vis != visited_search.end() && vis->second == i) {
                search_rotations.push_back(Rotations(GetRevRotation(rotation)));
                state = next_state;
                break;
            }
            if (auto vis = visited_leaf.find(next_state); vis != visited_leaf.end() && vis->second == i) {
                search_rotations.push_back(Rotations(GetRevRotation(rotation)));
                state = next_state;
                break;
            }
        }
    }
}


// return true if a solution is contained
/*
bool LeafSearch (const State& state, uint64_t& num_positions, uint8_t depth, uint8_t& best_sol, State& best_endstate, phmap::flat_hash_map<State, VisitedInfo>& visited, uint64_t& leaft_search_positions) {
    num_positions++;
    leaft_search_positions++;
    if (BCHTSetContains(Tablebase::tablebase.back(), state)) {
        if (depth + Settings::GetTBDepth() < best_sol) {
            best_sol = depth + Settings::GetTBDepth();
            best_endstate = state;
            LOG_EXTRA("best sol:", int(best_sol), "num_positions:", num_positions, "leaf_search:", leaft_search_positions);
            LOG_MEMORY();
            return true;
        }
    }
    Cube cube;
    if (std::max(cube.GetMaxHeuristic(state), Settings::GetTBDepth()) + depth >= best_sol) {
        return false;
    }
    bool is_solution = false;
    for (uint8_t rotation = 0; rotation < kNumRotations; rotation++) {
        std::pair<bool, State> next = Cube::Rotate(state, rotation);
        if (next.first) {
            bool res = LeafSearch(next.second, num_positions, depth+1, best_sol, best_endstate, visited, leaft_search_positions);
            if (res) {
                auto find_visited = visited.find(next.second);
                if (find_visited == visited.end()) {
                    visited.insert({next.second, {uint8_t(depth+1), rotation}}); // found new solution
                    is_solution = true;
                }
                else if (find_visited->second.depth > uint8_t(depth+1)) {
                    find_visited->second = {uint8_t(depth+1), rotation};
                    is_solution = true;
                }
            }
        }
    }
    return is_solution;
}
*/


void LeafManager (std::stop_token stocken, uint64_t& num_positions_leaf, VisitedMap& visited_leaf, std::atomic<uint8_t>& atomic_best_depth, std::pair<State, uint8_t>& best_endstate_leafs,
                  std::atomic<std::shared_ptr<phmap::flat_hash_map<State, uint8_t>>>& shared_leaf_states, const uint64_t& leaf_batch_size) {
    return;
    std::shared_ptr<phmap::flat_hash_map<State, uint8_t>> local_buffer;

    while (!stocken.stop_requested()) {
        std::shared_ptr<phmap::flat_hash_map<State, uint8_t>> shared_data = shared_leaf_states.load(std::memory_order_acquire);
        if (shared_data->size() >= leaf_batch_size) {
            std::shared_ptr<phmap::flat_hash_map<State, uint8_t>> new_empty_buffer = std::make_shared<phmap::flat_hash_map<State, uint8_t>>();
            local_buffer = shared_leaf_states.exchange(new_empty_buffer, std::memory_order_acquire);

            // TODO: Do some local buffer leaf calc
        }
    }
}


void Search (uint64_t& num_positions_search, VisitedMap& visited_search, std::atomic<uint8_t>& atomic_best_depth,
             std::pair<State, uint8_t>& best_endstate_search, const State& starting_position,
             std::atomic<std::shared_ptr<phmap::flat_hash_map<State, uint8_t>>>& shared_leaf_states, const uint64_t& leaf_batch_size) {
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
    std::shared_ptr<phmap::flat_hash_map<State, uint8_t>> local_buffer;

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
                if (depth < best_endstate_search.second) {
                    // set new enstate
                    best_endstate_search = {next_state.second, depth};
                    atomic_best_depth = depth;
                    visited_search[next_state.second] = depth;
                    LOG_EXTRA("best sol:", int(depth), "num_positions:", num_positions_search);
                    LOG_MEMORY();
                }
            }

            // due to the heuristic it is not possible to solve the current state in fewer moves than the current best solution
            Cube next_cube;
            if (std::max(next_cube.GetMaxHeuristic(next_state.second), Settings::GetTBDepth()) + pq_top.depth + 1 >= best_endstate_search.second) {
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

            if (next_cube.GetMaxHeuristic(next_state.second) + pq_top.depth + 1 > best_endstate_search.second - 2 && false) {
                // TODO: local buffer has to be updated if a lower depth needs to be searched by leafsearch
                local_buffer->insert({next_state.second, pq_top.depth+1});

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
            else {
                pq_search.push({uint8_t(next_cube.GetAppHeuristic(next_state.second)+pq_top.depth+1), uint8_t(pq_top.depth+1), next_state.second});
            }
        }
    }

    /*
    // swap local buffer with shared leaf states when it is swaped with an empty one
    if (local_buffer->size() >= leaf_batch_size) {
        auto shared_data = shared_leaf_states.load(std::memory_order_acquire);
        while (!shared_data->empty()) {
            std::this_thread::yield(); // prevent busy spin burn
            shared_data = shared_leaf_states.load(std::memory_order_acquire);
        }

        local_buffer = shared_leaf_states.exchange(local_buffer, std::memory_order_acq_rel);
    }
*/
}


void SearchManager () {
    if (Settings::GetNumRuns() <= 0) {
        return;
    }

    // start timing
    std::chrono::time_point start_time = std::chrono::high_resolution_clock::now();

    // random starting positions
    std::vector<State> random_positions = RandomPositions(Settings::GetNumRuns(), 0);

    // accumulated positions for information purposes
    uint64_t acc_total_num_positions = 0;
    uint64_t acc_depth = 0;

    for (size_t i = 0; i < random_positions.size(); i++) {

        uint64_t total_num_positions = 0;
        uint64_t num_positions_search = 0;
        uint64_t num_positions_leaf = 0;

        VisitedMap visited_search;
        VisitedMap visited_leaf;

        std::atomic<uint8_t> atomic_best_depth = -1;

        std::pair<State, uint8_t> best_endstate_search = {State(), -1};
        std::pair<State, uint8_t> best_endstate_leafs = {State(), -1};

        // sening data from search to LeafManager
        std::atomic<std::shared_ptr<phmap::flat_hash_map<State, uint8_t>>> shared_leaf_states;
        shared_leaf_states.store(
            std::make_shared<phmap::flat_hash_map<State, uint8_t>>(),
            std::memory_order_release
        );
        uint64_t leaf_batch_size = 10;

        // Start LeafManager on a seperate thread
        // TODO:
        std::jthread leaf_manager_thread(LeafManager, std::ref(num_positions_leaf), std::ref(visited_leaf), std::ref(atomic_best_depth), std::ref(best_endstate_leafs),
                                         std::ref(shared_leaf_states), leaf_batch_size);

        // Search
        Search(num_positions_search, visited_search, atomic_best_depth, best_endstate_search, random_positions[i], shared_leaf_states, leaf_batch_size);

        leaf_manager_thread.request_stop();
        leaf_manager_thread.join();

        total_num_positions = num_positions_search + num_positions_leaf;

        if (num_positions_search < Settings::GetNumPositions()) {
            LOG_EXTRA("Optimal solution found!");
        }

        // Construct solution
        if (atomic_best_depth != uint8_t(-1)) {
            // state between search and tablebase
            std::pair<State, uint8_t> best_endstate;
            if (best_endstate_search.second <= best_endstate_leafs.second) {
                best_endstate = best_endstate_search;
            }
            else {
                best_endstate = best_endstate_leafs;
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
                SolveSearch(search_rotations, atomic_best_depth-Settings::GetTBDepth(), best_endstate.first, visited_search, visited_leaf);

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
