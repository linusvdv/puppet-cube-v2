#include <cstdint>
#include <queue>
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


int Search(const State& starting_position, uint64_t& num_positions) {
    for (int i = 0; i <= Settings::GetTBDepth(); i++) {
        if (BCHTSetContains(Tablebase::tablebase[i], starting_position)) {
            return i;
        }
    }

    std::priority_queue<PQSearch, std::vector<PQSearch>, std::greater<>> pq_search;
    phmap::flat_hash_set<State> visited;

    Cube start_cube;
    pq_search.push({start_cube.GetMaxHeuristic(starting_position), 0, starting_position});
    visited.insert(starting_position);
    num_positions++;

    while (true) {
        PQSearch pq_top = pq_search.top();
        pq_search.pop();

        for (uint8_t rotation = 0; rotation < kNumRotations; rotation++) {
            std::pair<bool, State> next_position = Cube::Rotate(pq_top.state, rotation);
            if (BCHTSetContains(Tablebase::tablebase.back(), next_position.second)) {
                return pq_top.depth + 1 + Settings::GetTBDepth();
            }

            Cube next_cube;
            if (!visited.contains(next_position.second)) {
                pq_search.push({next_cube.GetAppHeuristic(next_position.second)+pq_top.depth+1, pq_top.depth+1, next_position.second});
                visited.insert(next_position.second);
                num_positions++;
            }
        }
    }
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
