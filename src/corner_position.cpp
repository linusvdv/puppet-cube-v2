#include <array>
#include <cstdint>
#include <vector>

#include "cube.h"
#include "logger.h"


constexpr int kNumCornerPositions = 40320;  // 8!
constexpr int kCornerPositionsSize = kNumCornerPositions * kNumRotations;


std::array<uint8_t, kNumCorners> HashToPosition (int hash) {
    std::array<uint8_t, kNumCorners> positions;
    std::array<bool, kNumCorners> visited;
    visited.fill(false);

    int shift = kNumCornerPositions;
    for (int i = 0; i < kNumCorners; i++) {
        shift /= kNumCorners - i;

        int cur = hash / shift;
        hash %= shift;

        for (int j = 0; j < kNumCorners; j++) {
            if (!visited[j]) {
                cur--;
            }
            if (cur == -1) {
                positions[i] = j;
                visited[j] = true;
                break;
            }
        }
    }
    return positions;
}


int PositionToHash (std::array<uint8_t, kNumCorners>& positions) {
    int hash = 0;
    std::array<bool, kNumCorners> visited;
    visited.fill(false);

    for (int i = 0; i < kNumCorners; i++) {
        hash *= kNumCorners - i;
        int idx = 0;
        for (int j = 0; j < positions[i]; j++) {
            idx += int(!visited[j]);
        }
        visited[positions[i]] = true;
        hash += idx;
    }

    if (hash >= kNumCornerPositions) {
        LOG_CRITICAL("Calculated hash too big");
    }
    return hash;
}


// map the current position to next position
// -1 marks no change in rotation direction
constexpr std::array<std::array<int8_t, kNumCorners>, kNumRotations> kCornerRotation =
{{
    { 4, -1,  0, -1,  6, -1,  2, -1}, // R
    { 2, -1,  6, -1,  0, -1,  4, -1}, // R'
    {-1,  3, -1,  7, -1,  1, -1,  5}, // L
    {-1,  5, -1,  1, -1,  7, -1,  3}, // L'
    { 1,  5, -1, -1,  0,  4, -1, -1}, // U
    { 4,  0, -1, -1,  5,  1, -1, -1}, // U'
    {-1, -1,  6,  2, -1, -1,  7,  3}, // D
    {-1, -1,  3,  7, -1, -1,  2,  6}, // D'
    { 2,  0,  3,  1, -1, -1, -1, -1}, // F
    { 1,  3,  0,  2, -1, -1, -1, -1}, // F'
    {-1, -1, -1, -1,  5,  7,  4,  6}, // B
    {-1, -1, -1, -1,  6,  4,  7,  5}, // B'
    { 4,  5,  0,  1,  6,  7,  2,  3}, // M  -  R  + L'
    { 2,  3,  6,  7,  0,  1,  4,  5}, // M' -  R' + L 
    { 1,  5,  3,  7,  0,  4,  2,  6}, // E  -  U  + D'
    { 4,  0,  6,  2,  5,  1,  7,  3}, // E' -  U' + D
    { 1,  3,  0,  2,  5,  7,  4,  6}, // S  -  F' + B
    { 2,  0,  3,  1,  6,  4,  7,  5}, // S' -  F  + B'
}};


std::array<uint8_t, kNumCorners> Rotate(std::array<uint8_t, kNumCorners> positions, uint8_t rotation) {
    for (uint8_t& position : positions) {
        if (kCornerRotation[rotation][position] == -1) {
            continue;
        }
        position = kCornerRotation[rotation][position];
    }
    return positions;
}


std::vector<uint8_t> CornerPositionInitialization() {
    std::vector<uint8_t> corner_positions(kCornerPositionsSize, 0);

    // index - position in 3D space (x y z)
    // 0 -  1  1  1
    // 1 - -1  1  1
    // 2 -  1 -1  1
    // 3 - -1 -1  1
    // ...
    // 7 - -1 -1 -1
    std::array<uint8_t, kNumCorners> positions;

    for (int i = 0; i < kNumCornerPositions; i++) {
        positions = HashToPosition(i);

        for (uint8_t rotation = 0; rotation < kNumRotations; rotation++) {

            std::array<uint8_t, kNumCorners> rotated = Rotate(positions, rotation);
            int hash = PositionToHash(rotated);

            if (hash >= kCornerPositionsSize) {
                LOG_CRITICAL("Calculated hash too big");
            }
            corner_positions[(i*kNumRotations) + rotation] = hash;
        }
    }

    return corner_positions;
}
