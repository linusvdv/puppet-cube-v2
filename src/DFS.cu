#include <cuda.h>
#include <driver_types.h>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "BCHTSet.cuh"
#include "cube.hpp"
#include "logger.hpp"
#include "settings.hpp"


template<typename T>
void UploadToDeviceDFS(const std::vector<T>& data, T*& d_pointer) {
    cudaError_t err = cudaMalloc((void **)&d_pointer, sizeof(T)*data.size());
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
    err = cudaMemcpy(d_pointer, data.data(), sizeof(T)*data.size(), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
}


struct DFSStack {
    Cube::State state;
    int depth;
};


// definded in cuda_search.cu
__device__ bool Rotate(Cube::State& state, const uint8_t& rotation);
extern __device__ Cube::State* d_tablebase;
extern __device__ size_t d_tablebebase_size;


__device__ constexpr int kDNumRotations = 18;


constexpr size_t kBatching = 1;
__global__ void DFSGlobal(Cube::State* d_random_position, size_t* d_num_nodes_gpu, size_t* d_num_tb_hits_gpu, DFSStack* d_dfs_stack, size_t num_random_position, int max_depth) {
    size_t index = threadIdx.x + (size_t(blockIdx.x) * blockDim.x);

    // device fixed max size stack
    size_t dfs_stack_size = (max_depth*kDNumRotations) + kBatching + 1;
    int64_t dfs_stack_idx = -1;
    size_t end_element = (num_random_position < (1+index)*kBatching) ? num_random_position : ((1+index)*kBatching);
    for (size_t i = index*kBatching; i < end_element; i++) { // aware that the dfs stack is the normal order so top is higher index
        d_dfs_stack[(++dfs_stack_idx) + (index*dfs_stack_size)] = {d_random_position[i], 0};
    }

    int64_t batch_idx = kBatching; // lowest dfs_stack_idx visited
    while (dfs_stack_idx >= 0) {
        if (batch_idx > dfs_stack_idx) {
            batch_idx = dfs_stack_idx;
        }
        d_num_nodes_gpu[batch_idx + (index*kBatching)]++;

        int current_depth = d_dfs_stack[dfs_stack_idx + (index*dfs_stack_size)].depth;
        Cube::State current = d_dfs_stack[dfs_stack_idx + (index*dfs_stack_size)].state;
        dfs_stack_idx--; // remove current position

        if (DBCHTSetContains(d_tablebase, d_tablebebase_size, current)) {
            d_num_tb_hits_gpu[batch_idx + (index*kBatching)]++;
        }

        if (current_depth == max_depth) {
            continue;
        }

        for (uint8_t rotation = 0; rotation < kDNumRotations; rotation++) {
            d_dfs_stack[(++dfs_stack_idx) + (index*dfs_stack_size)] = {current, current_depth+1};
            if (!Rotate(d_dfs_stack[dfs_stack_idx + (index*dfs_stack_size)].state, rotation)) {
                dfs_stack_idx--; // not a legal move
            }
        }
    }
}


void GPUDFS(const std::vector<Cube::State>& random_position, std::vector<size_t>& num_nodes_gpu, std::vector<size_t>& num_tb_hits_gpu) {
    // upload random_position
    Cube::State* d_random_position = nullptr;
    size_t* d_num_nodes_gpu = nullptr;
    size_t* d_num_tb_hits_gpu = nullptr;
    UploadToDeviceDFS(random_position, d_random_position);
    UploadToDeviceDFS(num_nodes_gpu, d_num_nodes_gpu);
    UploadToDeviceDFS(num_tb_hits_gpu, d_num_tb_hits_gpu);

    // create d_dfs_stack
    size_t grid_dim = (random_position.size()/kBatching/kBlockDim)+1;
    LOG_EXTRA("grid dim:", grid_dim, "block dim", kBlockDim);
    size_t dfs_stack_size = (Settings::GetDFSDepth()*kNumRotations) + kBatching + 1;
    DFSStack* d_dfs_stack = nullptr;
    cudaError err = cudaMalloc((void**)&d_dfs_stack, grid_dim * kBlockDim * dfs_stack_size * sizeof(DFSStack));
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }

    // start kernal
    DFSGlobal<<<grid_dim, kBlockDim>>>(d_random_position, d_num_nodes_gpu, d_num_tb_hits_gpu, d_dfs_stack, random_position.size(), Settings::GetDFSDepth());
    err = cudaDeviceSynchronize();
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }

    // get result
    err = cudaMemcpy(num_nodes_gpu.data(), d_num_nodes_gpu, sizeof(size_t)*num_nodes_gpu.size(), cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
    // get result
    err = cudaMemcpy(num_tb_hits_gpu.data(), d_num_tb_hits_gpu, sizeof(size_t)*num_tb_hits_gpu.size(), cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
}
