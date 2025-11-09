#include <cstddef>
#include <random>
#include <vector>

#include "cube.hpp"
#include "random_position.hpp"
#include "settings.hpp"


std::vector<State> RandomPositions (const size_t& num_elements, size_t seed_offset) {
    std::vector<State> random_positions;

    for (size_t i = 0; i < num_elements; i++) {
        std::mt19937 gen(seed_offset + i);
        std::uniform_int_distribution<int> dist(0, kNumRotations-1);

        State state = State(0, 0, 0, 0, kNumEdgePositions-1);
        for (int j = 0; j < Settings::GetScramblingDepth(); j++) {
            std::pair<bool, State> res = Cube::Rotate(state, dist(gen));
            state = res.second;
        }
        random_positions.push_back(state);
    }
    return random_positions;
}
