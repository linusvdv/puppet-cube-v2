#include <cstdint>
#include <queue>
#include <stack>
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


struct VisitedInfo {
    uint8_t depth;
    uint8_t rotation;
};


void SolveTB(std::stack<Rotations>& rev_moves, int tb_layer, State state) {
    for (int layer = tb_layer - 1; layer >= 0; layer--) {
        for (uint8_t rotation = 0; rotation < kNumRotations; rotation++) {
            State next_state = Cube::Rotate(state, rotation).second;
            if (BCHTSetContains(Tablebase::tablebase[layer], next_state)) {
                state = next_state;
                rev_moves.push(Rotations(GetRevRotation(rotation)));
                break;
            }
        }
    }
}


// return true if a solution is contained
bool LeafSearch(const State& state, uint64_t& num_positions, uint8_t depth, uint8_t& best_sol, State& best_endstate, phmap::flat_hash_map<State, VisitedInfo>& visited, uint64_t& leaft_search_positions) {
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


int Search(const State& starting_position, uint64_t& num_positions) {
    for (int i = 0; i <= Settings::GetTBDepth(); i++) {
        if (BCHTSetContains(Tablebase::tablebase[i], starting_position)) {
            LOG_EXTRA("Position in tablebase");
            State best_endstate = starting_position;
            std::stack<Rotations> rev_moves;
            SolveTB(rev_moves, i, best_endstate);
            return i;
        }
    }

    std::priority_queue<PQSearch, std::vector<PQSearch>, std::greater<>> pq_search;
    phmap::flat_hash_map<State, VisitedInfo> visited;

    Cube start_cube;
    pq_search.push({start_cube.GetMaxHeuristic(starting_position), 0, starting_position});
    visited.insert({starting_position, {0, uint8_t(-1)}});
    num_positions++;
    uint8_t best_sol = uint8_t(-1);
    State best_endstate;
    uint64_t leaft_search_positions = 0;

    while (num_positions < Settings::GetNumPositions() && !pq_search.empty()) {
        PQSearch pq_top = pq_search.top();
        pq_search.pop();

        for (uint8_t rotation = 0; rotation < kNumRotations; rotation++) {
            std::pair<bool, State> next_position = Cube::Rotate(pq_top.state, rotation);
            if (BCHTSetContains(Tablebase::tablebase.back(), next_position.second)) {
                uint8_t curr_sol = pq_top.depth + 1 + Settings::GetTBDepth();
                if (curr_sol < best_sol) {
                    best_sol = curr_sol;
                    best_endstate = next_position.second;
                    visited[next_position.second] = {curr_sol, rotation};
                    LOG_EXTRA("best sol:", int(best_sol), "num_positions:", num_positions, "leaf_search:", leaft_search_positions);
                    LOG_MEMORY();
                }
            }

            Cube next_cube;
            if (std::max(next_cube.GetMaxHeuristic(next_position.second), Settings::GetTBDepth()) + pq_top.depth + 1 >= best_sol) {
                continue;
            }
            auto find_visited = visited.find(next_position.second);
            if (next_cube.GetMaxHeuristic(next_position.second)+pq_top.depth+1 > best_sol - 2) {
                if (find_visited == visited.end()) {
                    if (LeafSearch(next_position.second, num_positions, pq_top.depth+1, best_sol, best_endstate, visited, leaft_search_positions)) {
                        visited.insert({next_position.second, {uint8_t(pq_top.depth+1), rotation}}); // found new solution
                    }
                }
                else if (find_visited->second.depth > uint8_t(pq_top.depth+1)) {
                    if (LeafSearch(next_position.second, num_positions, pq_top.depth+1, best_sol, best_endstate, visited, leaft_search_positions)) {
                        find_visited->second = {uint8_t(pq_top.depth+1), rotation};
                    }
                }
            }
            else {
                auto find_visited = visited.find(next_position.second);
                if (find_visited == visited.end()) {
                    pq_search.push({uint8_t(next_cube.GetAppHeuristic(next_position.second)+pq_top.depth+1), uint8_t(pq_top.depth+1), next_position.second});
                    visited.insert({next_position.second, {uint8_t(pq_top.depth+1), rotation}}); // found new solution
                }
                else if (find_visited->second.depth > uint8_t(pq_top.depth+1)) {
                    pq_search.push({uint8_t(next_cube.GetAppHeuristic(next_position.second)+pq_top.depth+1), uint8_t(pq_top.depth+1), next_position.second});
                    find_visited->second = {uint8_t(pq_top.depth+1), rotation};
                }
                num_positions++;
            }
        }
    }

    if (pq_search.empty()) {
        LOG_EXTRA("Optimal solution found!");
    }
    if (best_sol == uint8_t(-1)) {
        LOG_EXTRA("Solution not found!");
        return -1;
    }

    std::stack<Rotations> rev_rotations;
    SolveTB(rev_rotations, Settings::GetTBDepth(), best_endstate);

    std::vector<Rotations> search_rotations;
    for (int i = 0; i < best_sol-Settings::GetTBDepth(); i++) {
        if (!visited.contains(best_endstate)) {
            LOG_ERROR("NOT Contained");
        }
        uint8_t rotation = visited[best_endstate].rotation;
        if (rotation == uint8_t(-1)) {
            LOG_ERROR("Start state");
        }
        best_endstate = Cube::Rotate(best_endstate, GetRevRotation(rotation)).second;
        search_rotations.push_back(Rotations(GetRevRotation(rotation)));
    }

    LOG_EXTRA(rev_rotations, search_rotations);

    return best_sol;
}


void MainSearch() {
    if (Settings::GetNumRuns() <= 0) {
        return;
    }

    std::chrono::time_point start_time = std::chrono::high_resolution_clock::now(); // get the current time
    std::vector<State> random_positions = RandomPositions(Settings::GetNumRuns(), 0);
    uint64_t total_num_positions = 0;
    uint64_t acc_depth = 0;
    for (size_t i = 0; i < random_positions.size(); i++) {
        uint64_t num_positions = 0;
        int depth = Search(random_positions[i], num_positions);
        total_num_positions += num_positions;
        acc_depth += depth;
        LOG_ALL(SkipSpace("["), SkipSpace(i+1), SkipSpace("/"), SkipSpace(Settings::GetNumRuns()), "] Depth:", depth, "num_positions:", num_positions);
        LOG_MEMORY();
    }
    std::chrono::time_point since_epoch = std::chrono::high_resolution_clock::now(); // get the duration since epoch
    std::chrono::milliseconds millis = std::chrono::duration_cast<std::chrono::milliseconds>(since_epoch - start_time);
    LOG_ALL("Average time:", millis.count()/Settings::GetNumRuns(), "ms");
    LOG_ALL("Average depth:", acc_depth/Settings::GetNumRuns());
    LOG_ALL("Average number of positions:", total_num_positions/Settings::GetNumRuns());
    LOG_ALL("Positions per seconds:", total_num_positions * 1000 / millis.count());
}
