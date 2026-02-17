#pragma once
#include <compare>

#include "cube.hpp"


extern __constant__ uint16_t* d_corner_orientations;
extern __constant__ uint16_t* d_corner_positions;
extern __constant__ uint16_t* d_corner_heuristics;

extern __constant__ uint16_t* d_edge_orientations;
extern __constant__ uint32_t* d_edge_positions;
extern __constant__ uint8_t* d_edge_heuristics;


constexpr uint64_t kDMulA = 0x2545f4914f6cdd1dULL;
constexpr uint64_t kDMulB = 0x9e3779b97f4a7c15ULL;


// 10 bytes
struct DState {
    uint16_t hash_1 = -1;
    uint32_t hash_2 = -1;
    uint32_t hash_3 = -1;

    __device__ constexpr DState(const uint16_t& corner_orientation,  // 12 bytes
                                const uint16_t& corner_position,     // 16 bytes
                                const uint16_t& edge_orientation,    // 11 bytes
                                const uint32_t& edge_position_1,     // 20 bytes
                                const uint32_t& edge_position_2) {   // 20 bytes
        // hash 1
        hash_1 = corner_position; // 16 bytes

        // hash 2
        hash_2 = corner_orientation; // 12 bytes
        hash_2 <<= 20; // NOLINT
        hash_2 |= edge_position_1; // 20 bytes

        // hash 3
        hash_3 = edge_orientation; // 11 bytes
        hash_3 <<= 20; // NOLINT
        hash_3 |= edge_position_2; // 20 bytes
    }

    // Default not legal State
    __host__ __device__ constexpr DState() {}

    DState(const State& host_state) {
        hash_1 = host_state.hash_1;
        hash_2 = host_state.hash_2;
        hash_3 = host_state.hash_3;
    }


    std::strong_ordering operator<=>(const DState&) const = default;
};


extern __constant__ DState* d_tablebase;
extern __constant__ size_t d_tablebase_size;
