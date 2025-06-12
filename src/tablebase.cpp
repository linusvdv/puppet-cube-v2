#include "cube.h"


void TablebasePrecomputation (const Cube::Tablebase& previous, const Cube::Tablebase& current, Cube::Tablebase& next, int thread_idx, int num_threads) {
    int count = 0;
    for (const Cube::State& position : current) {
        count++;
        if (count%num_threads != thread_idx) {
            continue;
        }
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
}
