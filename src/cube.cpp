#include <sys/types.h>
#include <bit>

#include "cube.h"


Cube::Cube () {
    for (unsigned int i = 0; i < kNumCorners; i++) {
        corners[i].position = i;
        corners[i].orientation = 1;
    }
    for (unsigned int i = 0; i < kNumEdges; i++) {
        edges[i].position = i;
        edges[i].orientation = 1;
    }
}


unsigned int Cube::GetPositionHash () {
    // if you already calculated position hash use the calculated position
    if (calculated_position_hash_) {
        return position_hash_;
    }
    calculated_position_hash_ = true;

    unsigned int hash = 0;
    unsigned int orientation_hash = 0;

    // convert positions from 8^8 to 8!
    // this is possible because all indices only appear once
    std::array<bool, kNumCorners> accessed;
    accessed.fill(false);

    for (unsigned int i = 0; i < kNumCorners - 1; i++) {
        // position
        hash *= kNumCorners - i;
        unsigned int corner_index = 0;
        for (int j = 0; j < corners[i].position; j++) {
            corner_index += uint(!accessed[j]);
        }
        accessed[corners[i].position] = true;
        hash += corner_index;

        // orientation
        orientation_hash *= 3;
        orientation_hash += std::countr_zero(corners[i].orientation);
    }

    hash += orientation_hash * kEightFac;
    position_hash_ = hash;
    return hash;
}
