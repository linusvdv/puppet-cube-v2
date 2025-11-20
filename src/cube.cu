#include <cassert>
#include <vector>
#include <cuda.h>

#include "BCHTSet.cuh"
#include "cube.cuh"
#include "cube.hpp"
#include "cuda_memory_transfer.cuh"


__device__ uint16_t* d_corner_orientations = nullptr;
__device__ uint16_t* d_corner_positions = nullptr;
__device__ uint16_t* d_corner_heuristics = nullptr;

__device__ uint16_t* d_edge_orientations = nullptr;
__device__ uint32_t* d_edge_positions = nullptr;
__device__ uint8_t* d_edge_heuristics = nullptr;

__device__ DState* d_tablebase = nullptr;
__device__ size_t d_tablebase_size = 0;




void DCube::UploadComputationToDevice(
    const std::vector<uint16_t>& corner_orientations,
    const std::vector<uint16_t>& corner_positions,
    const std::vector<uint16_t>& corner_heuristics,

    const std::vector<uint16_t>& edge_orientations,
    const std::vector<uint32_t>& edge_positions,
    const std::vector<uint8_t>& edge_heuristics
    ) {

    // corner precomputation
    UploadToDeviceSymbol(corner_orientations, d_corner_orientations);
    UploadToDeviceSymbol(corner_positions, d_corner_positions);
    UploadToDeviceSymbol(corner_heuristics, d_corner_heuristics);

    // edge precomputation
    UploadToDeviceSymbol(edge_orientations, d_edge_orientations);
    UploadToDeviceSymbol(edge_positions, d_edge_positions);
    UploadToDeviceSymbol(edge_heuristics, d_edge_heuristics);
}


void UploadTablebaseToDevice(const std::vector<State>& tablebebase) {
    UploadToDeviceSymbol(tablebebase, d_tablebase);
    UploadToSymbol(tablebebase.size(), d_tablebase_size);
}

void UploadCubeComputationToDevice(
    const std::vector<uint16_t>& corner_orientations,
    const std::vector<uint16_t>& corner_positions,
    const std::vector<uint16_t>& corner_heuristics,

    const std::vector<uint16_t>& edge_orientations,
    const std::vector<uint32_t>& edge_positions,
    const std::vector<uint8_t>& edge_heuristics
    ) {
    DCube::UploadComputationToDevice(corner_orientations, corner_positions, corner_heuristics, edge_orientations, edge_positions, edge_heuristics);
}


__device__ constexpr uint8_t kDLegalMoveIndex[kDNumRotations] = {
    8, 9, 8, 9, 10, 11, 10, 11, 12, 13, 12, 13, 0, 0, 0, 0, 0, 0
};


__device__ DRotateReturn DCube::Rotate(const DState& prev_state, const uint8_t& rotation) {
    uint16_t corner_orientation = prev_state.hash_2 >> 20;      // 12 bites         NOLINT
    uint16_t corner_position = prev_state.hash_1;               // 16 bites         NOLINT
    uint16_t edge_orientation = prev_state.hash_3 >> 20;        // 11 bites         NOLINT
    uint32_t edge_position_1 = prev_state.hash_2 & ((1<<20)-1); // 20 bites         NOLINT
    uint32_t edge_position_2 = prev_state.hash_3 & ((1<<20)-1); // 20 bites         NOLINT
    if (kDLegalMoveIndex[rotation] != 0 && ((d_corner_heuristics[(corner_orientation*kDNumCornerPositions) + corner_position] >> kDLegalMoveIndex[rotation]) & 1) == 0) {
        return {false, prev_state};
    }
    corner_orientation = d_corner_orientations[(corner_orientation*kDNumRotations) + rotation];
    corner_position = d_corner_positions[(corner_position*kDNumRotations) + rotation];
    edge_orientation = d_edge_orientations[(edge_orientation*kDNumRotations) + rotation];
    edge_position_1 = d_edge_positions[(edge_position_1*kDNumRotations) + rotation];
    edge_position_2 = d_edge_positions[(edge_position_2*kDNumRotations) + rotation];
    return {true, DState(corner_orientation, corner_position, edge_orientation, edge_position_1, edge_position_2)};
}


__device__ bool DCube::DTablebaseContains(const DState& state) {
    return DBCHTSetContains(d_tablebase, d_tablebase_size, state);
}


__device__ DState* d_random_positions = nullptr;
__device__ size_t d_random_positions_size = 0;
size_t random_positions_size = 0;


void UploadRandomPositionsToDevice(const std::vector<State>& random_positions) {
    UploadToDeviceSymbol(random_positions, d_random_positions);
    random_positions_size = random_positions.size();
    UploadToSymbol(random_positions_size, d_random_positions_size);
}


__device__ void DCube::SetCurCornerHeuristic(const DState& state) {
    uint16_t corner_orientation = state.hash_2 >> 20;      // 12 bites         NOLINT
    uint16_t corner_position = state.hash_1;               // 16 bites         NOLINT
    cur_corner_heuristic_ = d_corner_heuristics[(corner_orientation*kNumCornerPositions) + corner_position] & ((uint16_t(1) << 8) - 1); // NOLINT
}

__device__ void DCube::SetCurEdgeHeuristic1(const DState& state) {
    uint32_t orientation = state.hash_3 >> 20; // NOLINT
    uint32_t position = state.hash_2 & ((uint32_t(1) << 20) - 1); // NOLINT
    cur_edge_heuristic_1_ = d_edge_heuristics[(orientation*kNumEdgePositions) + position];
}

__device__ void DCube::SetCurEdgeHeuristic2(const DState& state) {
    uint32_t orientation = state.hash_3 >> 20; // NOLINT
    orientation |= (__popc(orientation)%2) << (kNumEdges-1); // get last bit using even num bits parity
    uint32_t orientation_r = 0;
    for (int i = 1; i < kNumEdges; i++) {
        orientation_r |= ((orientation >> i) & uint32_t(1)) << (kNumEdges-1-i);
    }

    uint32_t position = state.hash_3 & ((uint32_t(1) << 20) - 1); // NOLINT
    uint32_t position_r = 0;
    uint32_t temp = kNumEdgePositions;
    for (int i = kNumEdges-1; i >= 6; i--) { // NOLINT
        temp /= i+1;
        position_r *= i+1;
        position_r += i - ((position / temp) % (i + 1)); // NOLINT
    }
    cur_edge_heuristic_2_ = d_edge_heuristics[(orientation_r*kNumEdgePositions) + position_r];
}

__device__ uint16_t DCube::GetMaxHeuristic(const DState& state) {
    if (cur_corner_heuristic_ == uint16_t(-1)) {
        SetCurCornerHeuristic(state);
    }
    if (cur_edge_heuristic_1_ == uint8_t(-1)) {
        SetCurEdgeHeuristic1(state);
    }
    if (cur_edge_heuristic_2_ == uint8_t(-1)) {
        SetCurEdgeHeuristic2(state);
    }
    return max(max(cur_corner_heuristic_, uint16_t(cur_edge_heuristic_1_)), uint16_t(cur_edge_heuristic_2_));
}
