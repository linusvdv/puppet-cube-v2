#include <queue>
#include <set>

#include "BCHTSet.hpp"
#include "cube.hpp"
#include "settings.hpp"
#include "tablebase.hpp"


struct PQSearch {
    int value;
    int depth;
    State state;

    std::strong_ordering operator<=>(const PQSearch&) const = default;
};


int Search(const State& starting_position) {
    for (int i = 0; i <= Settings::GetTBDepth(); i++) {
        if (BCHTSetContains(Tablebase::tablebase[i], starting_position)) {
            return i;
        }
    }

    std::priority_queue<PQSearch, std::vector<PQSearch>, std::greater<>> pq_search;
    std::set<State> visited;

    Cube start_cube;
    pq_search.push({start_cube.GetMaxHeuristic(starting_position), 0, starting_position});
    visited.insert(starting_position);

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
            }
        }
    }
}
