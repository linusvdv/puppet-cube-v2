#include <thread>
#include <vector>

#include "cube.h"
#include "logger.h"
#include "settings.h"


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

    friend std::size_t hash_value(const Edges& e) {
        constexpr int kMagicVal = 0x9e3779b9;
        std::size_t h1 = std::hash<uint16_t>{}(e.orientation);
        std::size_t h2 = std::hash<uint32_t>{}(e.position);

        // Combine hashes
        std::size_t seed = h1;
        seed ^= h2 + kMagicVal + (seed << 6) + (seed >> 2);
        return seed;
    }
};
#pragma pack(pop)


using ParallelEdge = phmap::parallel_flat_hash_set<Edges,
    phmap::priv::hash_default_hash<Edges>,
    phmap::priv::hash_default_eq<Edges>,
    phmap::priv::Allocator<Edges>,
    12, std::mutex>;


void ParallelEdgeHeuristic(const std::vector<uint16_t>& edge_orientation, const std::vector<uint32_t>& edge_position,
                           const ParallelEdge& last, const ParallelEdge& current, ParallelEdge& next,
                           std::vector<uint8_t>& edge_heuristic, std::atomic<int>& cnt, int depth, int thread_idx, int num_threads) {
    int edge_cnt = 0;
    for (const Edges& edges : current) {
        edge_cnt++;
        if (edge_cnt % num_threads != thread_idx) {
            continue;
        }

        for (int rotation = 0; rotation < kNumRotations; rotation++) {
            Edges next_edges = {edge_orientation[(edges.orientation*kNumRotations)+rotation],
                                edge_position[(edges.position*kNumRotations)+rotation]};
            if (last.contains(next_edges) || current.contains(next_edges)) {
                continue;
            }
            next.insert(next_edges);
        }
        edge_heuristic[(edges.orientation*kNumEdgePositions) + edges.position] = depth;
        int current_cnt = cnt++;
        if (current_cnt % (kNumEdgeHeuristic / 100) == 0) {
            LOG_EXTRA(current_cnt / (kNumEdgeHeuristic / 100), "%");
            LOG_MEMORY();
            if (current_cnt / (kNumEdgeHeuristic / 100) == 2) {
                LOG_CRITICAL("");
            }
        }
    }
}


std::vector<uint8_t> EdgeHeuristicInitialization(const std::vector<uint16_t>& edge_orientation, const std::vector<uint32_t>& edge_position, uint16_t solved_orientation, uint32_t solved_position) {
    std::vector<uint8_t> edge_heuristic(kEdgeHeuristicSize, 0);

    std::atomic<int> cnt = 0;
    ParallelEdge last = {};
    ParallelEdge current = {{solved_orientation, solved_position}};
    ParallelEdge next = {};
    int depth = 0;
    do {
        depth++;
        {
            std::vector<std::jthread> threads;
            for (int j = 0; j < Settings::num_threads; j++) {
                threads.push_back(std::jthread(
                    ParallelEdgeHeuristic, std::ref(edge_orientation), std::ref(edge_position),
                    std::ref(last), std::ref(current), std::ref(next),
                    std::ref(edge_heuristic), std::ref(cnt), depth, j, Settings::num_threads
                ));
            }
        }
        LOG_EXTRA("depth:", depth, "positions:", current.size());
        std::swap(last, current);
        std::swap(current, next);
        next = {};
    } while(!current.empty());

    if (cnt != kNumEdgeHeuristic) {
        LOG_CRITICAL("Found", cnt, "instead of", kNumEdgeHeuristic, "positions");
    }
    LOG_EXTRA("max depth:", depth-1);

    return edge_heuristic;
}
