#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "cube.h"
#include "logger.h"


constexpr int kNumCornerOrientation = 2187;  // 3^7
constexpr int kCornerOrientationSize = kNumCornerOrientation * kNumRotations; // 3^7 * 18


int OrientationsToHash (std::array<uint8_t, kNumCorners>& orientations) {
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


std::array<uint8_t, kNumCorners> HashToOrientations (int hash) {
    std::array<uint8_t, kNumCorners> orientations;
    int acc = 0;
    for (int i = kNumCorners-2; i >= 0; i--) {
        orientations[i] = 1 << (hash % 3);
        acc += (hash % 3);
        hash /= 3;
    }
    int last_orientation = (3 - (acc%3)) % 3;
    orientations[kNumCorners-1] = 1 << last_orientation;
    return orientations;
}


// swap bit shift_1 with bit shift_2
template <size_t shift_1, size_t shift_2>
uint8_t SwapBits (uint8_t bits) {
    return bits ^ ((((bits >> shift_1) ^ (bits >> shift_2)) & 1) * ((1 << shift_1) | (1 << shift_2)));
}


template <size_t shift_1, size_t shift_2>
void Rotation (std::array<uint8_t, kNumCorners>& orientations, std::array<int, 4> swap) {
    uint8_t temp = orientations[swap[3]];
    orientations[swap[3]] = orientations[swap[2]];
    orientations[swap[2]] = orientations[swap[1]];
    orientations[swap[1]] = orientations[swap[0]];
    orientations[swap[0]] = temp;
    for (int idx : swap) {
        orientations[idx] = SwapBits<shift_1, shift_2>(orientations[idx]);
    }
}


std::array<uint8_t, kNumCorners> Rotate (std::array<uint8_t, kNumCorners> orientations, Rotations rotation) {
    constexpr std::array<std::array<int, 4>, 12> swaps = {{
        {0, 1, 3, 2},  // R
        {0, 2, 3, 1},  // R'
        {4, 6, 7, 5},  // L
        {4, 5, 7, 6},  // L'
        {0, 4, 5, 1},  // U
        {0, 1, 5, 4},  // U'
        {2, 3, 7, 6},  // D
        {2, 6, 7, 3},  // D'
        {0, 2, 6, 4},  // F
        {0, 4, 6, 2},  // F'
        {1, 5, 7, 3},  // B
        {1, 3, 7, 5},  // B'
    }};

    if (rotation == kR || rotation == kM) {
        Rotation<1, 2>(orientations, swaps[0]);
    }
    if (rotation == kRc || rotation == kMc) {
        Rotation<1, 2>(orientations, swaps[1]);
    }
    if (rotation == kL || rotation == kMc) {
        Rotation<1, 2>(orientations, swaps[2]);
    }
    if (rotation == kLc || rotation == kM) {
        Rotation<1, 2>(orientations, swaps[3]);
    }

    if (rotation == kU || rotation == kE) {
        Rotation<0, 2>(orientations, swaps[4]);
    }
    if (rotation == kUc || rotation == kEc) {
        Rotation<0, 2>(orientations, swaps[5]);
    }
    if (rotation == kD || rotation == kEc) {
        Rotation<0, 2>(orientations, swaps[6]);
    }
    if (rotation == kDc || rotation == kE) {
        Rotation<0, 2>(orientations, swaps[7]);
    }

    if (rotation == kF || rotation == kSc) {
        Rotation<0, 1>(orientations, swaps[8]);
    }
    if (rotation == kFc || rotation == kS) {
        Rotation<0, 1>(orientations, swaps[9]);
    }
    if (rotation == kB || rotation == kS) {
        Rotation<0, 1>(orientations, swaps[10]);
    }
    if (rotation == kBc || rotation == kSc) {
        Rotation<0, 1>(orientations, swaps[11]);
    }

    return orientations;
}


// precomputation of the corner orientations
std::vector<uint8_t> CornerOrientationInitialization () {
    std::vector<uint8_t> corner_orientation(kCornerOrientationSize, 0);

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
    std::array<uint8_t, kNumCorners> orientations;

    for (int i = 0; i < kNumCornerOrientation; i++) {
        orientations = HashToOrientations(i);

        for (uint8_t j = 0; j < kNumRotations; j++) {
            Rotations rotation = static_cast<Rotations>(j);

            std::array<uint8_t, kNumCorners> rotated = Rotate(orientations, rotation);
            int hash = OrientationsToHash(rotated);

            if (hash >= kCornerOrientationSize) {
                LOG_CRITICAL("Calculated hash too big");
            }
            corner_orientation[(i*kNumRotations) + j] = hash;
        }
    }

    return corner_orientation;
}
