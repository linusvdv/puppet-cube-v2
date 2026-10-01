#pragma once

#include "cube.hpp"
#include "duplicate_rotations.hpp"


__device__ inline bool IsDuplicateRotation(const uint8_t& last, const uint8_t& current, const uint64_t duplicate_move_data[kDuplicateRotationDataSize]) {
    if (last == uint8_t(-1)) {
        return false;
    }
    int index = (int(last)*kNumRot)+int(current);
    return ((duplicate_move_data[index/64]>>(index%64)) & uint64_t(1)) != uint64_t(0);
}
