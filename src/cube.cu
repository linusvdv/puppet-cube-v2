#include <cassert>
#include <vector>
#include <cuda.h>

#include "cube.cuh"
#include "cube.hpp"
#include "cuda_memory_transfer.cuh"
#include "settings.hpp"


__constant__ uint16_t* d_corner_orientations = nullptr;
__constant__ uint16_t* d_corner_positions = nullptr;
__constant__ uint16_t* d_corner_heuristics = nullptr;

__constant__ uint16_t* d_edge_orientations = nullptr;
__constant__ uint32_t* d_edge_positions = nullptr;
__constant__ uint8_t* d_edge_heuristics = nullptr;

__constant__ DState* d_tablebase = nullptr;
__constant__ size_t d_tablebase_size = 0;


void UploadCubeComputationToDevices(
    const std::vector<uint16_t>& corner_orientations,
    const std::vector<uint16_t>& corner_positions,
    const std::vector<uint16_t>& corner_heuristics,

    const std::vector<uint16_t>& edge_orientations,
    const std::vector<uint32_t>& edge_positions,
    const std::vector<uint8_t>& edge_heuristics) {
    // upload on every device
    for (int i = 0; i < Settings::GetDeviceCount(); i++) {
        // upload it to the correct device
        cudaSetDevice(i);

        // corner precomputation
        UploadToDeviceSymbol(corner_orientations, d_corner_orientations);
        UploadToDeviceSymbol(corner_positions, d_corner_positions);
        UploadToDeviceSymbol(corner_heuristics, d_corner_heuristics);

        // edge precomputation
        UploadToDeviceSymbol(edge_orientations, d_edge_orientations);
        UploadToDeviceSymbol(edge_positions, d_edge_positions);
        UploadToDeviceSymbol(edge_heuristics, d_edge_heuristics);
    }
}


void UploadTablebaseToDevices(const std::vector<State>& tablebebase) {
    for (int i = 0; i < Settings::GetDeviceCount(); i++) {
        // upload it to the correct device
        cudaSetDevice(i);

        UploadToDeviceSymbol(tablebebase, d_tablebase);
        MemcpyToSymbol(tablebebase.size(), d_tablebase_size);
    }
}


__constant__ DState* d_random_positions = nullptr;
__constant__ size_t d_random_positions_size = 0;
size_t random_positions_size = 0;


void UploadRandomPositionsToDevice(const std::vector<State>& random_positions) {
    UploadToDeviceSymbol(random_positions, d_random_positions);
    random_positions_size = random_positions.size();
    cudaError_t err = cudaMemcpyToSymbol(d_random_positions_size, &random_positions_size, sizeof(random_positions_size));
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
}
