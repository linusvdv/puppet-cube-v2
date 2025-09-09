#include <cstddef>
#include <random>
#include <vector>

#include "cube.hpp"
#include "random_position.hpp"
#include "settings.hpp"


std::vector<Cube::State> RandomPositions (const size_t& num_elements) {
    std::random_device rand_d;
    std::mt19937 gen(rand_d());
    std::uniform_int_distribution<int> dist(0, kNumRotations);
    std::vector<Cube::State> random_positions;

    for (size_t i = 0; i < num_elements; i++) {
        Cube::State state = Cube::State(0, 0, 0, 0, kNumEdgePositions-1);
        for (int j = 0; j < Settings::GetScramblingDepth(); j++) {
            state.Rotate(dist(gen));
        }
        random_positions.push_back(state);
    }
    return random_positions;
}
