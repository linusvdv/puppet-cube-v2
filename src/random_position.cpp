#include <cstddef>
#include <random>
#include <vector>

#include "cube.hpp"
#include "random_position.hpp"
#include "settings.hpp"


std::vector<State> RandomPositions (const size_t& num_elements, size_t seed) {
    std::mt19937 gen(seed);
    std::uniform_int_distribution<int> dist(0, kNumRotations);
    std::vector<State> random_positions;

    for (size_t i = 0; i < num_elements; i++) {
        State state = State(0, 0, 0, 0, kNumEdgePositions-1);
        for (int j = 0; j < Settings::GetScramblingDepth(); j++) {
            state = Cube::Rotate(state, dist(gen)).second;
        }
        random_positions.push_back(state);
    }
    return random_positions;
}
