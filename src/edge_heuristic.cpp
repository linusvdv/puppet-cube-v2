#include <queue>
#include <vector>

#include "cube.h"
#include "logger.h"


constexpr int kNumEdgeHeuristic = kNumEdgePositions * kNumEdgeOrientation;
constexpr int kEdgeHeuristicSize = kNumEdgeHeuristic;


#pragma pack(push, 1)
struct Edges {
    uint16_t orientation;
    uint32_t position;

    bool operator==(const Edges& other) const {
        return std::tie(orientation, position) ==
        std::tie(other.orientation, other.position);
    }
};
#pragma pack(pop)




std::vector<uint8_t> EdgeHeuristicInitialization(const std::vector<uint16_t>& edge_orientation, const std::vector<uint32_t>& edge_position, uint16_t solved_orientation, uint32_t solved_position) {
    std::vector<uint8_t> edge_heuristic(kEdgeHeuristicSize, 0);

    int cnt = 0;
    std::queue<Edges> next_queue;
    next_queue.push({solved_orientation, solved_position});
    int depth = 0;
    Edges last_depth_element = {solved_orientation, solved_position};
    Edges last_pushed_element = {solved_orientation, solved_position};

    while (!next_queue.empty()) {
        Edges current = next_queue.front();
        next_queue.pop();

        for (int rotation = 0; rotation < kNumRotations; rotation++) {
            Edges next = {edge_orientation[(current.orientation*kNumRotations)+rotation],
                                edge_position[(current.position*kNumRotations)+rotation]};
            if (edge_heuristic[(next.orientation*kNumEdgePositions) + next.position] != 0 ||
                next == Edges(solved_orientation, solved_position)) {
                continue;
            }

            edge_heuristic[(next.orientation*kNumEdgePositions) + next.position] = -1;
            next_queue.push(next);
            last_pushed_element = next;
        }

        edge_heuristic[(current.orientation*kNumEdgePositions) + current.position] = depth;
        int current_cnt = cnt++;
        if (current_cnt % (kNumEdgeHeuristic / 100) == 0) {
            LOG_EXTRA(current_cnt / (kNumEdgeHeuristic / 100), "%");
            LOG_EXTRA(next_queue.size());
            LOG_MEMORY();
        }
        if (current == last_depth_element) {
            last_depth_element = last_pushed_element;
            LOG_EXTRA("finished depth:", depth, "cnt:", cnt);
            depth++;
        }
    }

    if (cnt != kNumEdgeHeuristic) {
        LOG_CRITICAL("Found", cnt, "instead of", kNumEdgeHeuristic, "positions");
    }
    LOG_EXTRA("max depth:", depth-1);

    return edge_heuristic;
}
