#include "cube.h"


Cube::Tablebase TablebasePrecomputation (Cube::Tablebase& previous, Cube::Tablebase& current) {
    Cube::Tablebase next;

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
