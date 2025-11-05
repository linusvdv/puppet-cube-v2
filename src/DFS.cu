#include <cuda.h>
#include <driver_types.h>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "cube.cuh"
#include "cube.hpp"
#include "cuda_memory_transfer.cuh"
#include "logger.hpp"
#include "settings.hpp"


struct DFSStack {
    int8_t depth;
    int8_t rotation;
    DState state;
};


__global__ void DFSGlobal(DState* d_random_position, size_t num_random_position, size_t* d_num_nodes_gpu, size_t* d_num_tb_hits_gpu, int max_depth) {
    size_t index = threadIdx.x + (size_t(blockIdx.x) * blockDim.x);

    if (index >= num_random_position) {
        return;
    }

    // device fixed max size stack in registes
    DFSStack dfs_stack[kMaxDFSDepth+1];
    int8_t dfs_stack_idx = 0;
    dfs_stack[dfs_stack_idx] = {0, 0, d_random_position[index]};

    size_t cur_num_nodes_gpu = 0;
    size_t cur_num_tb_hits_gpu = 0;

    cur_num_nodes_gpu++;
    if (DCube::DTablebaseContains(dfs_stack[dfs_stack_idx].state)) {
        cur_num_tb_hits_gpu++;
    }
    while (dfs_stack_idx >= 0) {
        int8_t cur_depth = dfs_stack[dfs_stack_idx].depth;
        DRotateReturn next = DCube::Rotate(dfs_stack[dfs_stack_idx].state, dfs_stack[dfs_stack_idx].rotation++);

        if (dfs_stack[dfs_stack_idx].rotation >= kNumRotations) {
            dfs_stack_idx--;
        }

        if (next.isLegal) {
            cur_num_nodes_gpu++;
            if (DCube::DTablebaseContains(next.state)) {
                cur_num_tb_hits_gpu++;
            }
            DCube cube;
            cube.GetMaxHeuristic(next.state);
            if (cur_depth+1 < max_depth) {
                dfs_stack[++dfs_stack_idx] = {int8_t(cur_depth+1), 0, next.state};
            }
        }
    }
    d_num_nodes_gpu[index] = cur_num_nodes_gpu;
    d_num_tb_hits_gpu[index] = cur_num_tb_hits_gpu;
}


void GPUDFS(const std::vector<State>& random_position, std::vector<size_t>& num_nodes_gpu, std::vector<size_t>& num_tb_hits_gpu) {
    // upload random_position
    DState* d_random_position = nullptr;
    size_t* d_num_nodes_gpu = nullptr;
    size_t* d_num_tb_hits_gpu = nullptr;
    LOG_MEMORY();
    UploadToDevice(random_position, d_random_position);
    UploadToDevice(num_nodes_gpu, d_num_nodes_gpu);
    UploadToDevice(num_tb_hits_gpu, d_num_tb_hits_gpu);
    LOG_MEMORY();

    size_t grid_dim = (random_position.size()/kBlockDim)+1;
    LOG_EXTRA("grid dim:", grid_dim, "block dim", kBlockDim);

    DFSGlobal<<<grid_dim, kBlockDim>>>(d_random_position, random_position.size(), d_num_nodes_gpu, d_num_tb_hits_gpu, Settings::GetDFSDepth());
    cudaError_t err = cudaDeviceSynchronize();
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
