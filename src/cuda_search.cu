#include <cstdio>
#include <cuda.h>
#include <cuda_device_runtime_api.h>
#include <driver_types.h>
#include <vector>

#include "BCHTSet.cuh"
#include "cube.hpp"
#include "cuda_search.cuh"
#include "logger.hpp"
#include "settings.hpp"


class CudaSearch {
public:
    static uint16_t* d_corner_orientations;
    static uint16_t* d_corner_positions;
    static uint16_t* d_corner_heuristics;

    static uint16_t* d_edge_orientations;
    static uint32_t* d_edge_positions;
    static uint8_t* d_edge_heuristics;

    static Cube::State* d_tablebase;  // outer_layer
    static size_t d_tablebebase_size;

    static Cube::State* d_random_positions; // only for performance test
    static size_t d_random_positions_size;
};


uint16_t* CudaSearch::d_corner_orientations = nullptr;
uint16_t* CudaSearch::d_corner_positions = nullptr;
uint16_t* CudaSearch::d_corner_heuristics = nullptr;

uint16_t* CudaSearch::d_edge_orientations = nullptr;
uint32_t* CudaSearch::d_edge_positions = nullptr;
uint8_t* CudaSearch::d_edge_heuristics = nullptr;

Cube::State* CudaSearch::d_tablebase = nullptr;
size_t CudaSearch::d_tablebebase_size = 0;

Cube::State* CudaSearch::d_random_positions = nullptr;
size_t CudaSearch::d_random_positions_size = 0;



void UploadCubeComputationToDevice(
    const std::vector<uint16_t>& corner_orientations,
    const std::vector<uint16_t>& corner_positions,
    const std::vector<uint16_t>& corner_heuristics,

    const std::vector<uint16_t>& edge_orientations,
    const std::vector<uint32_t>& edge_positions,
    const std::vector<uint8_t>& edge_heuristics
    ) {

    // corner orientation
    cudaError_t err = cudaMalloc((void **)&CudaSearch::d_corner_orientations, sizeof(uint16_t)*corner_orientations.size());
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
    err = cudaMemcpy(CudaSearch::d_corner_orientations, corner_orientations.data(), sizeof(uint16_t)*corner_orientations.size(), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }

    // corner positions
    err = cudaMalloc((void **)&CudaSearch::d_corner_positions, sizeof(uint16_t)*corner_positions.size());
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
    err = cudaMemcpy(CudaSearch::d_corner_positions, corner_positions.data(), sizeof(uint16_t)*corner_positions.size(), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }

    // corner heuristics
    err = cudaMalloc((void **)&CudaSearch::d_corner_heuristics, sizeof(uint16_t)*corner_heuristics.size());
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
    err = cudaMemcpy(CudaSearch::d_corner_heuristics, corner_heuristics.data(), sizeof(uint16_t)*corner_heuristics.size(), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }

    // edge orientation
    err = cudaMalloc((void **)&CudaSearch::d_edge_orientations, sizeof(uint16_t)*edge_orientations.size());
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
    err = cudaMemcpy(CudaSearch::d_edge_orientations, edge_orientations.data(), sizeof(uint16_t)*edge_orientations.size(), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }

    // edge positions
    err = cudaMalloc((void **)&CudaSearch::d_edge_positions, sizeof(uint32_t)*edge_positions.size());
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
    err = cudaMemcpy(CudaSearch::d_edge_positions, edge_positions.data(), sizeof(uint32_t)*edge_positions.size(), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }

    // edge heuristics
    err = cudaMalloc((void **)&CudaSearch::d_edge_heuristics, sizeof(uint8_t)*edge_heuristics.size());
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
    err = cudaMemcpy(CudaSearch::d_edge_heuristics, edge_heuristics.data(), sizeof(uint8_t)*edge_heuristics.size(), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
}


void UploadTablebaseToDevice(const std::vector<Cube::State>& tablebebase) {
    cudaMalloc((void **)&CudaSearch::d_tablebase, sizeof(Cube::State)*tablebebase.size());
    cudaMemcpy(CudaSearch::d_tablebase, tablebebase.data(), sizeof(Cube::State)*tablebebase.size(), cudaMemcpyHostToDevice);
    CudaSearch::d_tablebebase_size = tablebebase.size();
}


void UploadRandomPositionsToDevice(const std::vector<Cube::State>& random_positions) {
    cudaMalloc((void **)&CudaSearch::d_random_positions, sizeof(Cube::State)*random_positions.size());
    cudaMemcpy(CudaSearch::d_random_positions, random_positions.data(), sizeof(Cube::State)*random_positions.size(), cudaMemcpyHostToDevice);
    CudaSearch::d_random_positions_size = random_positions.size();
}


__global__ void DTimeBCHTtable(Cube::State* d_tablebase, size_t d_tablebebase_size, Cube::State* d_random_positions, size_t d_random_positions_size, unsigned long long* hit, unsigned long long* miss) {
    size_t index = threadIdx.x + (blockIdx.x * blockDim.x);
    if (index >= d_random_positions_size) {
        return;
    }
    if (DBCHTSetContains(d_tablebase, d_tablebebase_size, d_random_positions[index])) {
        atomicAdd(hit, size_t(1));
    }
    else {
        atomicAdd(miss, size_t(1));
    }
}

void TimeBCHTtable() {
    unsigned long long *d_hit;
    unsigned long long *d_miss;
    cudaMalloc(&d_hit, sizeof(unsigned long long));
    cudaMalloc(&d_miss, sizeof(unsigned long long));

    for (int i = 0; i < 10; i++) {
        unsigned long long h_hit = 0;
        unsigned long long h_miss = 0;
        cudaMemcpy(d_hit, &h_hit, sizeof(unsigned long long), cudaMemcpyHostToDevice);
        cudaMemcpy(d_miss, &h_miss, sizeof(unsigned long long), cudaMemcpyHostToDevice);

        DTimeBCHTtable<<<((CudaSearch::d_random_positions_size-1) / kBlockDim+1), kBlockDim>>>(CudaSearch::d_tablebase, CudaSearch::d_tablebebase_size, CudaSearch::d_random_positions, CudaSearch::d_random_positions_size, d_hit, d_miss);
        cudaError_t err = cudaGetLastError();
        if (err != cudaSuccess) {
            printf("CUDA error: %s\n", cudaGetErrorString(err));
        }

        cudaMemcpy(&h_hit, d_hit, sizeof(unsigned long long), cudaMemcpyDeviceToHost);
        cudaMemcpy(&h_miss, d_miss, sizeof(unsigned long long), cudaMemcpyDeviceToHost);
        LOG_ALL(h_hit, "hits", h_miss, "misses");
    }
}
