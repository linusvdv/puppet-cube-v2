#include <cuda.h>
#include <vector>

#include "BCHTSet.cuh"
#include "cube.hpp"
#include "cuda_search.cuh"
#include "logger.hpp"
#include "settings.hpp"
#include "tablebase.hpp"


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


__device__ constexpr uint8_t kLegalMoveIndex [kNumRotations] = {
    8, 9, 8, 9, 10, 11, 10, 11, 12, 13, 12, 13, 0, 0, 0, 0, 0, 0
};

__device__ constexpr int kDNumCornerPositions = 40320;  // 8!

__device__ uint16_t* d_corner_orientations = nullptr;
__device__ uint16_t* d_corner_positions = nullptr;
__device__ uint16_t* d_corner_heuristics = nullptr;

__device__ uint16_t* d_edge_orientations = nullptr;
__device__ uint32_t* d_edge_positions = nullptr;
__device__ uint8_t* d_edge_heuristics = nullptr;

__device__ Cube::State* d_tablebase = nullptr;
__device__ size_t d_tablebebase_size = 0;

__device__ Cube::State* d_random_positions = nullptr;
__device__ size_t d_random_positions_size = 0;
size_t random_positions_size = 0;


__device__ bool Rotate(Cube::State& state, const uint8_t& rotation) {
    uint16_t corner_orientation; // 12 bytes
    uint16_t corner_position = state.hash_2; // 16 bytes
    uint16_t edge_orientation; // 11 bytes
    uint32_t edge_position_1; // 20 bytes
    uint32_t edge_position_2; // 20 bytes
    edge_position_2 = state.hash_1 & ((1ULL << 20) - 1ULL); // NOLINT
    state.hash_1 >>= 20; // NOLINT
    edge_position_1 = state.hash_1 & ((1ULL << 20) - 1ULL); // NOLINT
    state.hash_1 >>= 20; // NOLINT
    edge_orientation = state.hash_1 & ((1ULL << 11) - 1ULL); // NOLINT
    state.hash_1 >>= 11; // NOLINT
    corner_orientation = state.hash_1;
    if (kLegalMoveIndex[rotation] != 0 && ((d_corner_heuristics[(corner_orientation*kDNumCornerPositions) + corner_position] >> kLegalMoveIndex[rotation]) & 1) == 0) {
        return false;
    }
    corner_orientation = d_corner_orientations[(corner_orientation*kNumRotations) + rotation];
    corner_position = d_corner_positions[(corner_position*kNumRotations) + rotation];
    edge_orientation = d_edge_orientations[(edge_orientation*kNumRotations) + rotation];
    edge_position_1 = d_edge_positions[(edge_position_1*kNumRotations) + rotation];
    edge_position_2 = d_edge_positions[(edge_position_2*kNumRotations) + rotation];
    state.hash_1 = 0;
    state.hash_1 = uint64_t(corner_orientation); // 12 bytes
    state.hash_1 <<= 11; // NOLINT
    state.hash_1 |= uint64_t(edge_orientation); // 11 bytes
    state.hash_1 <<= 20; // NOLINT
    state.hash_1 |= uint64_t(edge_position_1); // 20 bytes
    state.hash_1 <<= 20; // NOLINT
    state.hash_1 |= uint64_t(edge_position_2); // 20 bytes
    state.hash_2 = corner_position; // 16 bytes
    return true;
}


void UploadCubeComputationToDevice(
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


void UploadTablebaseToDevice(const std::vector<Cube::State>& tablebebase) {
    UploadToDevice(tablebebase, d_tablebase);
    size_t temp_size = tablebebase.size();
    cudaError_t err = cudaMemcpyToSymbol(d_tablebebase_size, &temp_size, sizeof(temp_size));
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
}


void UploadRandomPositionsToDevice(const std::vector<Cube::State>& random_positions) {
    UploadToDevice(random_positions, d_random_positions);
    size_t temp_size = random_positions.size();
    cudaError_t err = cudaMemcpyToSymbol(d_random_positions_size, &temp_size, sizeof(temp_size));
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
    random_positions_size = temp_size;
}


constexpr size_t kBatching = 10;
__global__ void DTimeBCHTtable(unsigned long long* hit, unsigned long long* miss) {
    size_t index = threadIdx.x + (blockIdx.x * blockDim.x);
    for (size_t i = index*kBatching; i < (index+1)*kBatching && i < d_random_positions_size; i++) {
        if (DBCHTSetContains(d_tablebase, d_tablebebase_size, d_random_positions[i])) {
            atomicAdd(hit, size_t(1));
        }
        else {
            atomicAdd(miss, size_t(1));
        }
    }
}

void TimeBCHTGPU() {
    if (Settings::GetTestBCHT()) {
        LOG_EXTRA("Start timing on GPU");
        std::chrono::time_point gpu_time = std::chrono::high_resolution_clock::now(); // get the current time

        unsigned long long *d_hit;
        unsigned long long *d_miss;
        cudaError_t err = cudaMalloc(&d_hit, sizeof(unsigned long long));
        if (err != cudaSuccess) {
            LOG_CRITICAL(cudaGetErrorString(err));
        }
        err = cudaMalloc(&d_miss, sizeof(unsigned long long));
        if (err != cudaSuccess) {
            LOG_CRITICAL(cudaGetErrorString(err));
        }

        for (int i = 0; i < 10; i++) {
            unsigned long long h_hit = 0;
            unsigned long long h_miss = 0;
            err = cudaMemcpy(d_hit, &h_hit, sizeof(unsigned long long), cudaMemcpyHostToDevice);
            if (err != cudaSuccess) {
                LOG_CRITICAL(cudaGetErrorString(err));
            }
            err = cudaMemcpy(d_miss, &h_miss, sizeof(unsigned long long), cudaMemcpyHostToDevice);
            if (err != cudaSuccess) {
                LOG_CRITICAL(cudaGetErrorString(err));
            }

            DTimeBCHTtable<<<(((random_positions_size/kBatching+1)-1) / kBlockDim+1), kBlockDim>>>(d_hit, d_miss);
            cudaError_t err = cudaGetLastError();
            if (err != cudaSuccess) {
                LOG_CRITICAL("CUDA error:", cudaGetErrorString(err));
            }

            err = cudaMemcpy(&h_hit, d_hit, sizeof(unsigned long long), cudaMemcpyDeviceToHost);
            if (err != cudaSuccess) {
                LOG_CRITICAL(cudaGetErrorString(err));
            }
            err = cudaMemcpy(&h_miss, d_miss, sizeof(unsigned long long), cudaMemcpyDeviceToHost);
            if (err != cudaSuccess) {
                LOG_CRITICAL(cudaGetErrorString(err));
            }
            LOG_EXTRA("run", i, ":", h_hit, "hits", h_miss, "misses");
        }

        std::chrono::time_point gpu_since_epoch = std::chrono::high_resolution_clock::now(); // get the duration since epoch
        std::chrono::milliseconds gpu_millis = std::chrono::duration_cast<std::chrono::milliseconds>(gpu_since_epoch - gpu_time);
        LOG_ALL("Time duration on GPU:", gpu_millis.count());
    }
}
