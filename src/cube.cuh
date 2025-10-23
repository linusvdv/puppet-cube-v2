// Same as cube.hpp for GPU
#pragma once
#include <compare>
#include <cuda.h>

#include "cube.hpp"


__device__ constexpr int kDNumCorners = 8;
__device__ constexpr int kDNumEdges = 12;
__device__ constexpr int kDNumRotations = 18;

__device__ constexpr int kDNumCornerOrientation = 2187;  // 3^7
__device__ constexpr int kDCornerOrientationSize = kDNumCornerOrientation * kDNumRotations; // 3^7 * 18
__device__ constexpr int kDNumCornerPositions = 40320;  // 8!
__device__ constexpr int kDCornerPositionsSize = kDNumCornerPositions * kDNumRotations;
__device__ constexpr int kDNumCornerHeuristic = kDNumCornerOrientation * kDNumCornerPositions;

__device__ constexpr int kDNumEdgeOrientation = 2048;  // 2^11
__device__ constexpr int kDEdgeOrientationSize = kDNumEdgeOrientation * kDNumRotations;  // 2^11 * 18
__device__ constexpr int kDNumEdgePositions = 665280;  // 12! / 6!
__device__ constexpr int kDEdgePositionsSize = kDNumEdgePositions * kDNumRotations;  // 12! / 6! * 18
__device__ constexpr int kDNumEdgeHeuristic = kDNumEdgePositions * kDNumEdgeOrientation;


enum DRotations : uint8_t {
    kDR,
    kDRc,
    kDL,
    kDLc,
    kDU,
    kDUc,
    kDD,
    kDDc,
    kDF,
    kDFc,
    kDB,
    kDBc,
    kDM,
    kDMc,
    kDE,
    kDEc,
    kDS,
    kDSc
};


__device__ constexpr uint64_t kDMulA = 0x2545f4914f6cdd1dULL;
__device__ constexpr uint64_t kDMulB = 0x9e3779b97f4a7c15ULL;


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
    __device__ constexpr DState() {}

    DState(const State& host_state) {
        hash_1 = host_state.hash_1;
        hash_2 = host_state.hash_2;
        hash_3 = host_state.hash_3;
    }


    std::strong_ordering operator<=>(const DState&) const = default;


    static __device__ uint64_t Mix64(uint64_t num) {
        num ^= num >> 31;  // NOLINT
        num *= kDMulA;
        num ^= num >> 33;  // NOLINT
        num *= kDMulB;
        num ^= num >> 28;  // NOLINT
        return num;
    }

    template<uint64_t hash_low, uint64_t hash_high>
    __device__ uint64_t SplitMix64() const {
        return Mix64(uint64_t(hash_1) ^ hash_low) ^ Mix64(((uint64_t(hash_2) << 32) | uint64_t(hash_3)) ^ hash_high);
    }
};


__device__ constexpr DState kDSolvedState = DState(0, 0, 0, 0, kDNumEdgePositions-1);


struct DRotateReturn {
    bool isLegal;
    DState state;
};


class DCube {
public:
    static __host__ void UploadComputationToDevice(
        const std::vector<uint16_t>& corner_orientations,
        const std::vector<uint16_t>& corner_positions,
        const std::vector<uint16_t>& corner_heuristics,

        const std::vector<uint16_t>& edge_orientations,
        const std::vector<uint32_t>& edge_positions,
        const std::vector<uint8_t>& edge_heuristics
        );

    __device__ static DRotateReturn Rotate(const DState& prev_state, uint8_t rotation);

    __device__ static bool DTablebaseContains(const DState& state);
};
