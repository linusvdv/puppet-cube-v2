/*
#include "cube.hpp"
#include "logger.hpp"
#include "duplicate_rotations.hpp"

std::array<uint64_t, kDuplicateRotationDataSize> DuplicateRotations::data = {0, 0, 0, 0, 0, 0};

void DuplicateRotations::Initialize() {
    std::vector<std::pair<Rotations, Rotations>> unnecessary_rotatations = {
        // R, M, L
        {kR, kRc},
        {kRc, kR},
        {kM, kMc},
        {kMc, kM},
        {kL, kLc},
        {kLc, kL},
            // same as slice moves
        {kLc, kR}, {kR, kLc},
        {kL, kRc}, {kRc, kL},
        {kMc, kR}, {kR, kMc},
        {kM, kRc}, {kRc, kM},
            // two opposite rotations which end up in the same cube
        {kM, kM},    // kMc, kMc
        {kM, kR},    // kR, kM
        {kMc, kRc}, // kRc, kMc
        {kM, kL},   // kL, kM
        {kMc, kLc}, // kLc, kMc
        {kL, kR},   // kR, kL
        {kLc, kRc}, // kRc, kLc

        // U, E, D
        {kU, kUc},
        {kUc, kU},
        {kE, kEc},
        {kEc, kE},
        {kD, kDc},
        {kDc, kD},
            // same as slice moves
        {kDc, kU}, {kU, kDc},
        {kD, kUc}, {kUc, kD},
        {kEc, kU}, {kU, kEc},
        {kE, kUc}, {kUc, kE},
            // two opposite rotations which end up in the same cube
        {kE, kE},    // kEc, kEc
        {kE, kU},    // kU, kE
        {kEc, kUc}, // kUc, kEc
        {kE, kD},   // kD, kE
        {kEc, kDc}, // kDc, kEc
        {kD, kU},   // kU, kD
        {kDc, kUc}, // kUc, kDc

        // B, S, F
        {kB, kBc},
        {kBc, kB},
        {kS, kSc},
        {kSc, kS},
        {kF, kFc},
        {kFc, kF},
            // same as slice moves
        {kFc, kB}, {kB, kFc},
        {kF, kBc}, {kBc, kF},
        {kSc, kB}, {kB, kSc},
        {kS, kBc}, {kBc, kS},
            // two opposite rotations which end up in the same cube
        {kS, kS},    // kSc, kSc
        {kS, kB},    // kB, kS
        {kSc, kBc}, // kBc, kSc
        {kS, kF},   // kF, kS
        {kSc, kFc}, // kFc, kSc
        {kF, kB},   // kB, kF
        {kFc, kBc}, // kBc, kFc
    };
    for (auto& [first, second] : unnecessary_rotatations) {
        int index = (int(first) * kNumRot) + int(second);
        data[index/64] |= uint64_t(1) << (index%64);
    }
}
*/
