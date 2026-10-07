#pragma once
#include "rotation.hpp"
#include "edge.hpp"


namespace edge {
extern __constant__ uint8_t* d_rotation_change;
extern __constant__ uint64_t* d_position_change;
extern __constant__ uint16_t* d_symmetry_change;
extern __constant__ uint16_t* d_orientation_change;

#ifdef REDUCE_MEMORY
extern __constant__ uint8_t* d_position_heuristic;
extern __constant__ uint8_t* d_orientation_heuristic;
#else
extern __constant__ uint32_t* d_heuristic_bucket;
extern __constant__ uint64_t* d_heuristic_value;
#endif


__device__ inline void DRotate(uint32_t& pos, uint8_t& sym, uint16_t& orient, uint8_t rot) {
    // change rotation relative to symmetry
    rot = d_rotation_change[(size_t(sym)*kNumRot)+rot];
    // position lookup (pos + symmetry change)
    uint64_t packed = d_position_change[(size_t(pos)*kNumRot)+rot];
    uint32_t sym_change = packed >> kPosShift;
    pos = packed & kPosMask;
    // change symmetry
    uint16_t packed_sym = d_symmetry_change[(size_t(sym_change)*kNumSym)+sym];
    uint8_t rel_sym = uint8_t(packed_sym>>8); // NOLINT
    sym = uint8_t(packed_sym);
    // orientation lookup
    orient = d_orientation_change[(((size_t(orient)*kNumSym)+rel_sym)*kNumRot)+rot]; // this rotation is not correct
}


#ifdef REDUCE_MEMORY
__device__ inline uint8_t DGetHeuristic(uint32_t pos, uint16_t orient) {
    uint8_t pos_heuristic = d_position_heuristic[pos];
    uint8_t orient_heuristic = d_orientation_heuristic[orient];
    return pos_heuristic > orient_heuristic ? pos_heuristic : orient_heuristic;
}
#else
__device__ inline uint8_t DGetHeuristic(uint32_t pos, uint16_t orient) {
    return d_heuristic_value[d_heuristic_bucket[(size_t(pos)*(kNumOrient/kNumStoredPerBucket))+(orient/kNumStoredPerBucket)]]
        >> ((orient%kNumStoredPerBucket) * 4) & kSingleHeuristicValue;
}
#endif
}
