#include <cassert>
#include <vector>
#include <cuda.h>

#include "cube.cuh"
#include "cube.hpp"
#include "logger.hpp"


__device__ uint16_t* d_corner_orientations = nullptr;
__device__ uint16_t* d_corner_positions = nullptr;
__device__ uint16_t* d_corner_heuristics = nullptr;

__device__ uint16_t* d_edge_orientations = nullptr;
__device__ uint32_t* d_edge_positions = nullptr;
__device__ uint8_t* d_edge_heuristics = nullptr;

__device__ DState* d_tablebase = nullptr;
__device__ size_t d_tablebase_size = 0;


template<typename T>
void UploadToDevice(const std::vector<T>& data, T*& d_pointer) {
    T* temp_pointer = nullptr;
    cudaError_t err = cudaMalloc((void **)&temp_pointer, sizeof(T)*data.size());
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
    err = cudaMemcpy(temp_pointer, data.data(), sizeof(T)*data.size(), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
    err = cudaMemcpyToSymbol(d_pointer, &temp_pointer, sizeof(T*));
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
}


void DCube::UploadComputationToDevice(
    const std::vector<uint16_t>& corner_orientations,
    const std::vector<uint16_t>& corner_positions,
    const std::vector<uint16_t>& corner_heuristics,

    const std::vector<uint16_t>& edge_orientations,
    const std::vector<uint32_t>& edge_positions,
    const std::vector<uint8_t>& edge_heuristics
    ) {

    // corner precomutation
    UploadToDevice(corner_orientations, d_corner_orientations);
    UploadToDevice(corner_positions, d_corner_positions);
    UploadToDevice(corner_heuristics, d_corner_heuristics);

    // edge precomutation
    UploadToDevice(edge_orientations, d_edge_orientations);
    UploadToDevice(edge_positions, d_edge_positions);
    UploadToDevice(edge_heuristics, d_edge_heuristics);
}


void UploadTablebaseToDevice(const std::vector<State>& tablebebase) {
    static_assert(sizeof(State) == sizeof(DState));
    static_assert(alignof(State) == alignof(DState));

    DState* temp_pointer = nullptr;
    cudaError_t err = cudaMalloc((void **)&temp_pointer, sizeof(State)*tablebebase.size());
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
    err = cudaMemcpy(temp_pointer, tablebebase.data(), sizeof(State)*tablebebase.size(), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
    err = cudaMemcpyToSymbol(d_tablebase, &temp_pointer, sizeof(DState*));
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }

    size_t temp_size = tablebebase.size();
    err = cudaMemcpyToSymbol(d_tablebase_size, &temp_size, sizeof(temp_size));
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
}


__device__ constexpr uint8_t kDLegalMoveIndex[kDNumRotations] = {
    8, 9, 8, 9, 10, 11, 10, 11, 12, 13, 12, 13, 0, 0, 0, 0, 0, 0
};


__device__ DRotateReturn DCube::Rotate(const DState& prev_state, const uint8_t& rotation) {
    uint16_t corner_orientation = prev_state.hash_2 >> 20;      // 12 bites
    uint16_t corner_position = prev_state.hash_1;               // 16 bites
    uint16_t edge_orientation = prev_state.hash_3 >> 20;        // 11 bites
    uint32_t edge_position_1 = prev_state.hash_2 & ((1<<20)-1); // 20 bites
    uint32_t edge_position_2 = prev_state.hash_3 & ((1<<20)-1); // 20 bites
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
