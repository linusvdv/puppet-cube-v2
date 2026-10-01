#pragma once
#include "corner.hpp"
#include "rotation.hpp"


namespace corner {
extern __constant__ uint16_t* d_position_change;
extern __constant__ uint16_t* d_orientation_change;
extern __constant__ uint64_t* d_heuristic;


__device__ inline void DRotate(uint16_t& pos, uint16_t& orient, uint8_t rot) {
    pos = d_position_change[(size_t(pos)*kNumRot)+rot];
    orient = d_orientation_change[(size_t(orient)*kNumRot)+rot];
}


__device__ inline uint64_t DGetHeuristic(uint16_t pos, uint16_t orient) {
    return d_heuristic[(size_t(pos)*kNumOrient)+orient];
}
}
