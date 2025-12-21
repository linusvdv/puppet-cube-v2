// Same as cube.hpp for GPU
#pragma once
#include <compare>
#include <cuda.h>
#include <cuda_runtime_api.h>

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


__device__ uint8_t DGetRevRotation(uint8_t rotation);


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
    uint16_t corner_orientation = -1;  // 12 bites
    uint16_t corner_position = -1;     // 16 bites
    uint16_t edge_orientation = -1;    // 11 bites
    uint32_t edge_position_1 = -1;     // 20 bites
    uint32_t edge_position_2 = -1;     // 20 bites


    __device__ constexpr DState(const uint16_t& corner_orientation,  // 12 bites
                                const uint16_t& corner_position,     // 16 bites
                                const uint16_t& edge_orientation,    // 11 bites
                                const uint32_t& edge_position_1,     // 20 bites
                                const uint32_t& edge_position_2)     // 20 bites
        : corner_orientation(corner_orientation),
          corner_position(corner_position),
          edge_orientation(edge_orientation),
          edge_position_1(edge_position_1),
          edge_position_2(edge_position_2)
    {}


    // Default not legal State
    __device__ constexpr DState() {}


    DState(const State& host_state) {
        corner_orientation = host_state.corner_orientation;
        corner_position = host_state.corner_position;
        edge_orientation = host_state.edge_orientation;
        edge_position_1 = host_state.edge_position_1;
        edge_position_2 = host_state.edge_position_2;
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
        return Mix64(uint64_t(corner_position) ^ hash_low) ^ Mix64(((uint64_t(corner_orientation) << 52) | (uint64_t(edge_position_1) << 32) | (uint64_t(edge_orientation) << 20) | uint64_t(edge_position_2)) ^ hash_high);
    }
};


__device__ constexpr DState kDSolvedState = DState(0, 0, 0, 0, kDNumEdgePositions-1);


struct DRotateReturn {
    bool isLegal;
    DState state;
};


class DCube {
public:
    __host__ void UploadComputationToDevice(
        const std::vector<uint16_t>& corner_orientations,
        const std::vector<uint16_t>& corner_positions,
        const std::vector<uint16_t>& corner_heuristics,

        const std::vector<uint16_t>& edge_orientations,
        const std::vector<uint32_t>& edge_positions,
        const std::vector<uint8_t>& edge_heuristics,
        int gpu_device_idx);

    __host__ void UploadTablebaseToDevice(const std::vector<State>& tablebebase, int gpu_device_idx);


    __device__ DRotateReturn Rotate(const DState& prev_state, const uint8_t& rotation);

    __device__ bool DTablebaseContains(const DState& state);

    uint16_t* d_corner_orientations = nullptr;
    uint16_t* d_corner_positions = nullptr;
    uint16_t* d_corner_heuristics = nullptr;

    uint16_t* d_edge_orientations = nullptr;
    uint32_t* d_edge_positions = nullptr;
    uint8_t* d_edge_heuristics = nullptr;

    DState* d_tablebase = nullptr;
    size_t d_tablebase_size = 0;
};


class DHeuristics {
public:
    __device__ uint8_t GetMaxHeuristic(const DState& state, const DCube& dcube);
    __device__ uint8_t GetAppHeuristic(const DState& state, const DCube& dcube);

    __device__ void SetCurCornerHeuristic(const DState& state, const DCube& dcube);
    __device__ void SetCurEdgeHeuristic1(const DState& state, const DCube& dcube);
    __device__ void SetCurEdgeHeuristic2(const DState& state, const DCube& dcube);

private:
    uint8_t cur_corner_heuristic_ = -1;
    uint8_t cur_edge_heuristic_1_ = -1;
    uint8_t cur_edge_heuristic_2_ = -1;
};


DCube GetDCube(int gpu_device_idx);
