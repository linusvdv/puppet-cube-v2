#include <cstddef>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

#include "corner.hpp"
#include "cube.hpp"
#include "random_position.hpp"
#include "edge.hpp"
#include "rotation.hpp"
#include "settings.hpp"


void MultithreadRandomPosition(std::vector<State>& random_positions, int seed_start, int seed_end, int seed_offset) {
    for (int i = seed_start; i < seed_end; i++) {
        std::mt19937 gen(i + seed_offset);
        std::uniform_int_distribution<int> dist(0, kNumRot-1);

        State local_state = kSolvedState;
        for (int j = 0; j < Settings::GetScramblingDepth(); j++) {
            uint8_t rotation = dist(gen);

            uint64_t corner_heuristic = corner::GetHeuristic(local_state.corner_pos, local_state.corner_orient);
            if (((corner_heuristic >> (8+2*rotation)) & 3) == 3) { // illegal rotation
                continue;
            }

            corner::Rotate(local_state.corner_pos, local_state.corner_orient, rotation);
            edge::Rotate(local_state.edge_pos, local_state.edge_sym, local_state.edge_orient, rotation);
        }

        while (uint8_t(corner::GetHeuristic(local_state.corner_pos, local_state.corner_orient)) < Settings::GetMinCornerHeuristic()) {
            uint8_t rotation = dist(gen);

            uint64_t corner_heuristic = corner::GetHeuristic(local_state.corner_pos, local_state.corner_orient);
            if (((corner_heuristic >> (8+2*rotation)) & 3) == 3) { // illegal rotation
                continue;
            }

            corner::Rotate(local_state.corner_pos, local_state.corner_orient, rotation);
            edge::Rotate(local_state.edge_pos, local_state.edge_sym, local_state.edge_orient, rotation);
        }

        random_positions[i] = local_state;
    }
}


std::vector<State> RandomPositions (const int& num_elements, int seed_offset) {
    std::vector<State> random_positions(num_elements);

    {
        std::vector<std::jthread> threads;
        threads.reserve(Settings::GetNumThreads());
        for (int thread = 0; thread < Settings::GetNumThreads(); thread++) {
            threads.emplace_back(MultithreadRandomPosition,
                                 std::ref(random_positions),
                                 ((num_elements/Settings::GetNumThreads())+1)*thread,
                                 std::min(((num_elements/Settings::GetNumThreads())+1)*(thread+1), num_elements),
                                 seed_offset);
        }
    }
    return random_positions;
}
