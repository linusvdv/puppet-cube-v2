#include <cassert>
#include <vector>
#include <cuda.h>

#include "BCHTSet.cuh"
#include "cube.cuh"
#include "cube.hpp"
#include "cuda_memory_transfer.cuh"
#include "settings.hpp"


std::vector<DCube> d_cubes_on_diff_devices;


void DCubeInitialization() {
    d_cubes_on_diff_devices.assign(Settings::GetDeviceCount(), DCube());
}


DCube GetDCube(int gpu_device_idx) {
    return d_cubes_on_diff_devices[gpu_device_idx];
}

__device__ uint8_t DGetRevRotation(uint8_t rotation) {
    if (rotation % 2 == 0) {
        return rotation + 1;
    }
    return rotation - 1;
}


void DCube::UploadComputationToDevice(
    const std::vector<uint16_t>& corner_orientations,
    const std::vector<uint16_t>& corner_positions,
    const std::vector<uint16_t>& corner_heuristics,

    const std::vector<uint16_t>& edge_orientations,
    const std::vector<uint32_t>& edge_positions,
    const std::vector<uint8_t>& edge_heuristics,
    int gpu_device_idx) {
    // upload it to the correct device
    cudaSetDevice(gpu_device_idx);

    // corner precomputation
    UploadToDevice(corner_orientations, d_corner_orientations);
    UploadToDevice(corner_positions, d_corner_positions);
    UploadToDevice(corner_heuristics, d_corner_heuristics);

    // edge precomputation
    UploadToDevice(edge_orientations, d_edge_orientations);
    UploadToDevice(edge_positions, d_edge_positions);
    UploadToDevice(edge_heuristics, d_edge_heuristics);
}


void DCube::UploadTablebaseToDevice(const std::vector<State>& tablebase, int gpu_device_idx) {
    // upload it to the correct device
    cudaSetDevice(gpu_device_idx);

    UploadToDevice(tablebase, d_tablebase);
    d_tablebase_size = tablebase.size();
}

void UploadCubeComputationToDevices(
    const std::vector<uint16_t>& corner_orientations,
    const std::vector<uint16_t>& corner_positions,
    const std::vector<uint16_t>& corner_heuristics,

    const std::vector<uint16_t>& edge_orientations,
    const std::vector<uint32_t>& edge_positions,
    const std::vector<uint8_t>& edge_heuristics) {
    // upload on every device
    for (int i = 0; i < Settings::GetDeviceCount(); i++) {
        d_cubes_on_diff_devices[i].UploadComputationToDevice(corner_orientations, corner_positions, corner_heuristics, edge_orientations, edge_positions, edge_heuristics, i);
    }
}


void UploadTablebaseToDevices(const std::vector<State>& tablebebase) {
    for (int i = 0; i < Settings::GetDeviceCount(); i++) {
        d_cubes_on_diff_devices[i].UploadTablebaseToDevice(tablebebase, i);
    }
}


__device__ constexpr uint8_t kDLegalMoveIndex[kDNumRotations] = {
    8, 9, 8, 9, 10, 11, 10, 11, 12, 13, 12, 13, 0, 0, 0, 0, 0, 0
};


__device__ bool DCube::RotateRef(DState& state, const uint8_t& rotation) const {
    if (kDLegalMoveIndex[rotation] != 0 && ((d_corner_heuristics[(state.corner_orientation*kDNumCornerPositions) + state.corner_position] >> kDLegalMoveIndex[rotation]) & 1) == 0) {
        return false;
    }
    state.corner_orientation = d_corner_orientations[(state.corner_orientation*kDNumRotations) + rotation];
    state.corner_position = d_corner_positions[(state.corner_position*kDNumRotations) + rotation];
    state.edge_orientation = d_edge_orientations[(state.edge_orientation*kDNumRotations) + rotation];
    state.edge_position_1 = d_edge_positions[(state.edge_position_1*kDNumRotations) + rotation];
    state.edge_position_2 = d_edge_positions[(state.edge_position_2*kDNumRotations) + rotation];
    return true;
}


__device__ bool DCube::DTablebaseContains(const DState& state) {
    return DBCHTSetContains(d_tablebase, d_tablebase_size, state);
}


__device__ DState* d_random_positions = nullptr;
__device__ size_t d_random_positions_size = 0;
size_t random_positions_size = 0;


void UploadRandomPositionsToDevice(const std::vector<State>& random_positions) {
    std::vector<DState> dstate_random_positions;
    dstate_random_positions.reserve(random_positions.size());
    for (const State& state : random_positions) {
        dstate_random_positions.emplace_back(state);
    }

    UploadToDeviceSymbol(dstate_random_positions, d_random_positions);
    random_positions_size = random_positions.size();
    cudaError_t err = cudaMemcpyToSymbol(d_random_positions_size, &random_positions_size, sizeof(random_positions_size));
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
}


__device__ void DHeuristics::SetCurCornerHeuristic(const DState& state, const DCube& dcube) {
    cur_corner_heuristic_ = uint8_t(dcube.d_corner_heuristics[(state.corner_orientation*kNumCornerPositions) + state.corner_position] & ((uint16_t(1) << 8) - 1)); // NOLINT
}


__device__ void DHeuristics::SetCurEdgeHeuristic1(const DState& state, const DCube& dcube) {
    cur_edge_heuristic_1_ = dcube.d_edge_heuristics[(state.edge_orientation*kNumEdgePositions) + state.edge_position_1];
}


__device__ void DHeuristics::SetCurEdgeHeuristic2(const DState& state, const DCube& dcube) {
    uint32_t orientation = state.edge_orientation; // NOLINT
    orientation |= (__popc(orientation)%2) << (kNumEdges-1); // get last bit using even num bits parity
    uint32_t orientation_r = 0;
    for (int i = 1; i < kNumEdges; i++) {
        orientation_r |= ((orientation >> i) & uint32_t(1)) << (kNumEdges-1-i);
    }

    uint32_t position = state.edge_position_2; // NOLINT
    uint32_t position_r = 0;
    uint32_t temp = kNumEdgePositions;
    for (int i = kNumEdges-1; i >= 6; i--) { // NOLINT
        temp /= i+1;
        position_r *= i+1;
        position_r += i - ((position / temp) % (i + 1)); // NOLINT
    }
    cur_edge_heuristic_2_ = dcube.d_edge_heuristics[(orientation_r*kNumEdgePositions) + position_r];
}


__device__ uint8_t DHeuristics::GetMaxHeuristic(const DState& state, const DCube& dcube) {
    if (cur_corner_heuristic_ == uint8_t(-1)) {
        SetCurCornerHeuristic(state, dcube);
    }
    if (cur_edge_heuristic_1_ == uint8_t(-1)) {
        SetCurEdgeHeuristic1(state, dcube);
    }
    if (cur_edge_heuristic_2_ == uint8_t(-1)) {
        SetCurEdgeHeuristic2(state, dcube);
    }
    return max(max(cur_corner_heuristic_, cur_edge_heuristic_1_), cur_edge_heuristic_2_);
}


__device__ uint8_t DHeuristics::GetAppHeuristic(const DState& state, const DCube& dcube) {
    if (cur_corner_heuristic_ == uint8_t(-1)) {
        SetCurCornerHeuristic(state, dcube);
    }
    if (cur_edge_heuristic_1_ == uint8_t(-1)) {
        SetCurEdgeHeuristic1(state, dcube);
    }
    if (cur_edge_heuristic_2_ == uint8_t(-1)) {
        SetCurEdgeHeuristic2(state, dcube);
    }
    return cur_corner_heuristic_ + cur_edge_heuristic_1_ + cur_edge_heuristic_2_;
}


__device__ bool IsSameState(const DState& state, const DStatePacked& state_packed) {
    if (state_packed.hash_1 != state.corner_position) {
        return false;
    }
    if (state_packed.hash_2 != ((uint32_t(state.corner_orientation) << 20) | uint32_t(state.edge_position_1))) {
        return false;
    }
    if (state_packed.hash_2 != ((uint32_t(state.edge_orientation) << 20) | uint32_t(state.edge_position_2))) {
        return false;
    }
    return true;
}
