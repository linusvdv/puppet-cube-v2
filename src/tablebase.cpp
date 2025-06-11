#include <unordered_set>

#include "cube.h"


std::unordered_set<Cube::State> TablebasePrecomputation (std::unordered_set<Cube::State>& previous, std::unordered_set<Cube::State>& current) {
    std::unordered_set<Cube::State> next;

    for (const Cube::State& position : current) {
        for (uint8_t rotation = 0; rotation < kNumRotations; rotation++) {
            Cube::State next_position = position;
            if (!next_position.Rotate(rotation)) {
                continue;
            }

            if (previous.contains(next_position) || current.contains(next_position)) {
                continue;
            }
            next.insert(next_position);
        }
    }

    return next;
}
