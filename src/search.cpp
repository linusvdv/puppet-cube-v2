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
    int value;
    int depth;
    State state;

    std::strong_ordering operator<=>(const PQSearch&) const = default;
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
    phmap::flat_hash_map<State, int> visited;

    Cube start_cube;
    pq_search.push({start_cube.GetMaxHeuristic(starting_position), 0, starting_position});
    visited.insert({starting_position, -1});
    num_positions++;
    int best_sol = 100;
    State best_endstate;

    while (num_positions < Settings::GetNumPositions() && !pq_search.empty()) {
        PQSearch pq_top = pq_search.top();
        pq_search.pop();

        for (uint8_t rotation = 0; rotation < kNumRotations; rotation++) {
            std::pair<bool, State> next_position = Cube::Rotate(pq_top.state, rotation);
            if (BCHTSetContains(Tablebase::tablebase.back(), next_position.second)) {
                int curr_sol = pq_top.depth + 1 + Settings::GetTBDepth();
                if (curr_sol < best_sol) {
                    best_sol = curr_sol;
                    best_endstate = next_position.second;
                    visited.insert({next_position.second, rotation});
                    LOG_EXTRA("best sol:", best_sol, "num_positions:", num_positions);
                }
            }

            Cube next_cube;
            if (!visited.contains(next_position.second)) {
                if (std::max(int(next_cube.GetMaxHeuristic(next_position.second)), Settings::GetTBDepth()) + pq_top.depth + 1 >= best_sol) {
                    continue;
                }
                pq_search.push({next_cube.GetAppHeuristic(next_position.second)+pq_top.depth+1, pq_top.depth+1, next_position.second});
                visited.insert({next_position.second, rotation});
                num_positions++;
            }
        }
    }

    if (pq_search.empty()) {
        LOG_EXTRA("Optimal solution found!");
    }
    if (best_sol == 100) {
        LOG_EXTRA("Solution not found!");
        return -1;
    }

    std::stack<Rotations> rev_rotations;
    SolveTB(rev_rotations, Settings::GetTBDepth(), best_endstate);

    std::vector<Rotations> search_rotations;
    for (int i = 0; i < best_sol-Settings::GetTBDepth(); i++) {
        uint8_t rotation = visited[best_endstate];
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
