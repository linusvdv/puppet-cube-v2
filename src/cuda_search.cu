#include <cstdio>
#include <cuda.h>
#include <cuda_device_runtime_api.h>
#include <driver_types.h>
#include <vector>

#include "cube.hpp"
#include "cuda_search.cuh"


class CudaSearch {
public:
    static uint16_t* d_corner_orientations;
    static uint16_t* d_corner_positions;
    static uint16_t* d_corner_heuristics;

    static uint16_t* d_edge_orientations;
    static uint32_t* d_edge_positions;
    static uint8_t* d_edge_heuristics;

    static Cube::State* d_tablebase;  // outer_layer
};


uint16_t* CudaSearch::d_corner_orientations = nullptr;
uint16_t* CudaSearch::d_corner_positions = nullptr;
uint16_t* CudaSearch::d_corner_heuristics = nullptr;

uint16_t* CudaSearch::d_edge_orientations = nullptr;
uint32_t* CudaSearch::d_edge_positions = nullptr;
uint8_t* CudaSearch::d_edge_heuristics = nullptr;

Cube::State* CudaSearch::d_tablebase = nullptr;


__global__ void PrintTest() {
    printf("Hi from GPU\n");
}


void Search () {
    PrintTest<<<2,2>>>();
    cudaDeviceSynchronize();
}


void UploadCubeComputationToDevice(
    const std::vector<uint16_t>& corner_orientations,
    const std::vector<uint16_t>& corner_positions,
    const std::vector<uint16_t>& corner_heuristics,

    const std::vector<uint16_t>& edge_orientations,
    const std::vector<uint32_t>& edge_positions,
    const std::vector<uint8_t>& edge_heuristics
    ) {

    // corner orientation
    cudaMalloc((void **)&CudaSearch::d_corner_orientations, sizeof(uint16_t)*corner_orientations.size());
    cudaMemcpy(CudaSearch::d_corner_orientations, corner_orientations.data(), sizeof(uint16_t)*corner_orientations.size(), cudaMemcpyHostToDevice);

    // corner positions
    cudaMalloc((void **)&CudaSearch::d_corner_positions, sizeof(uint16_t)*corner_positions.size());
    cudaMemcpy(CudaSearch::d_corner_positions, corner_positions.data(), sizeof(uint16_t)*corner_positions.size(), cudaMemcpyHostToDevice);

    // corner heuristics
    cudaMalloc((void **)&CudaSearch::d_corner_heuristics, sizeof(uint16_t)*corner_heuristics.size());
    cudaMemcpy(CudaSearch::d_corner_heuristics, corner_heuristics.data(), sizeof(uint16_t)*corner_heuristics.size(), cudaMemcpyHostToDevice);

    // edge orientation
    cudaMalloc((void **)&CudaSearch::d_edge_orientations, sizeof(uint16_t)*edge_orientations.size());
    cudaMemcpy(CudaSearch::d_edge_orientations, edge_orientations.data(), sizeof(uint16_t)*edge_orientations.size(), cudaMemcpyHostToDevice);

    // edge positions
    cudaMalloc((void **)&CudaSearch::d_edge_positions, sizeof(uint16_t)*edge_positions.size());
    cudaMemcpy(CudaSearch::d_edge_positions, edge_positions.data(), sizeof(uint16_t)*edge_positions.size(), cudaMemcpyHostToDevice);

    // edge heuristics
    cudaMalloc((void **)&CudaSearch::d_edge_heuristics, sizeof(uint16_t)*edge_heuristics.size());
    cudaMemcpy(CudaSearch::d_edge_heuristics, edge_heuristics.data(), sizeof(uint16_t)*edge_heuristics.size(), cudaMemcpyHostToDevice);
}


void UplaodTablebaseToDevice(const std::vector<Cube::State>& tablebebase) {
    cudaMalloc((void **)&CudaSearch::d_tablebase, sizeof(Cube::State)*tablebebase.size());
    cudaMemcpy(CudaSearch::d_tablebase, tablebebase.data(), sizeof(Cube::State)*tablebebase.size(), cudaMemcpyHostToDevice);
}
