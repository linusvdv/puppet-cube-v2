#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <queue>
#include <vector>

#include "cube.h"
#include "logger.h"


int OrientationsToHash (const std::array<uint8_t, kNumCorners>& orientations) {
    int hash = 0;
    for (int i = 0; i < kNumCorners-1; i++) {
        hash *= 3;
        hash += std::countr_zero(orientations[i]);
    }
    if (hash >= kNumCornerOrientation) {
        LOG_CRITICAL("Calculated hash too big");
    }
    return hash;
}


// swap bit shift_1 with bit shift_2
template <size_t shift_1, size_t shift_2>
uint8_t SwapBits (uint8_t bits) {
    return bits ^ ((((bits >> shift_1) ^ (bits >> shift_2)) & 1) * ((1 << shift_1) | (1 << shift_2)));
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


std::array<uint8_t, kNumCorners> OrientationRotate (const std::array<uint8_t, kNumCorners>& old_orientations, Rotations rotation) {
    std::array<uint8_t, kNumCorners> orientations;
    for (int i = 0; i < kNumCorners; i++) {
        if (kCornerRotation[rotation][i] == -1) {
            orientations[i] = old_orientations[i];
        }
        else {
            orientations[kCornerRotation[rotation][i]] = old_orientations[i];
        }
    }
    for (int i = 0; i < kNumCorners; i++) {
        // chage the position of the corner
        if (kCornerRotation[rotation][i] == -1) {
            continue;
        }

        // rotate the protruding pieces and their orientation
        switch (rotation) {
            case kR:
            case kRc:
            case kL:
            case kLc:
            case kM:
            case kMc:
                orientations[i] = SwapBits<1, 2>(orientations[i]);
                break;
            case kU:
            case kUc:
            case kD:
            case kDc:
            case kE:
            case kEc:
                orientations[i] = SwapBits<0, 2>(orientations[i]);
                break;
            case kF:
            case kFc:
            case kB:
            case kBc:
            case kS:
            case kSc:
                orientations[i] = SwapBits<0, 1>(orientations[i]);
                break;
        }
    }
    return orientations;
}


// precomputation of the corner orientations
std::vector<uint16_t> CornerOrientationInitialization () {
    std::vector<uint16_t> corner_orientation(kCornerOrientationSize, 0);
    std::vector<bool> visited(kNumCornerOrientation, false);

    // distinguish the different orientations
    // 0th bit - x direction
    // 1st bit - y direction
    // 2nd bit - z direction
    //
    // index - position in 3D space (x y z)
    // 0 -  1  1  1
    // 1 - -1  1  1
    // 2 -  1 -1  1
    // 3 - -1 -1  1
    // ...
    // 7 - -1 -1 -1
    std::array<uint8_t, kNumCorners> start_orientations = {1, 1, 1, 1, 1, 1, 1, 1};
    std::queue<std::array<uint8_t, kNumCorners>> next_queue;
    next_queue.push(start_orientations);
    visited[0] = true;
    int cnt = 1;

    while (!next_queue.empty()) {
        std::array<uint8_t, kNumCorners> orientations = next_queue.front();
        next_queue.pop();
        int old_hash = OrientationsToHash(orientations);

        for (uint8_t j = 0; j < kNumRotations; j++) {
            Rotations rotation = static_cast<Rotations>(j);

            std::array<uint8_t, kNumCorners> rotated = OrientationRotate(orientations, rotation);
            int hash = OrientationsToHash(rotated);

            if (hash >= kCornerOrientationSize) {
                LOG_CRITICAL("Calculated hash too big");
            }
            corner_orientation[(old_hash*kNumRotations) + j] = hash;
            if (visited[hash]) {
                continue;
            }
            visited[hash] = true;
            next_queue.push(rotated);
            cnt++;
        }
    }

    if (cnt != kNumCornerOrientation) {
        LOG_WARNING(cnt, "/", kNumCornerOrientation);
        LOG_CRITICAL("Didn't find all");
    }
    return corner_orientation;
}
