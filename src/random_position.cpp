#include <cstddef>
#include <iostream>
#include <random>
#include <vector>

#include "corner.hpp"
#include "cube.hpp"
#include "random_position.hpp"
#include "edge.hpp"
#include "rotation.hpp"
#include "settings.hpp"


std::vector<State> RandomPositions (const size_t& num_elements, size_t seed_offset) {
    std::vector<State> random_positions;

    for (size_t i = 0; i < num_elements; i++) {
        std::mt19937 gen(seed_offset + i);
        std::uniform_int_distribution<int> dist(0, kNumRot-1);

        State state = kSolvedState;
        for (int j = 0; j < Settings::GetScramblingDepth(); j++) {
            uint8_t rotation = dist(gen);

            uint64_t corner_heuristic = corner::GetHeuristic(state.corner_pos, state.corner_orient);
            if (((corner_heuristic >> (8+2*rotation)) & 3) == 3) { // illegal rotation
                continue;
            }

            corner::Rotate(state.corner_pos, state.corner_orient, rotation);
            edge::Rotate(state.edge_pos, state.edge_sym, state.edge_orient, rotation);
        }

        while (uint8_t(corner::GetHeuristic(state.corner_pos, state.corner_orient)) < Settings::GetMinCornerHeuristic()) {
            uint8_t rotation = dist(gen);

            uint64_t corner_heuristic = corner::GetHeuristic(state.corner_pos, state.corner_orient);
            if (((corner_heuristic >> (8+2*rotation)) & 3) == 3) { // illegal rotation
                continue;
            }

            corner::Rotate(state.corner_pos, state.corner_orient, rotation);
            edge::Rotate(state.edge_pos, state.edge_sym, state.edge_orient, rotation);
        }

        random_positions.push_back(state);
    }
    return random_positions;
}
