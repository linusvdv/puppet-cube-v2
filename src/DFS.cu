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


struct DFSShared {
    DState start;
    uint8_t idx;
    int8_t dfs_stack_idx;
    int8_t depths[kMaxDFSDepth];
    int8_t rotations[kMaxDFSDepth];
    size_t cur_num_nodes_gpu;
    size_t cur_num_tb_hits_gpu;
};


__global__ void DFSGlobal(DState* d_random_position, size_t num_random_position, size_t* d_num_nodes_gpu, size_t* d_num_tb_hits_gpu, int max_depth) {
    size_t index = threadIdx.x + (size_t(blockIdx.x) * blockDim.x);

    if (index >= num_random_position) {
        return;
    }

    __shared__ DFSShared dfs_shared[kBlockDim];
    dfs_shared[threadIdx.x] = {d_random_position[index], uint8_t(threadIdx.x), 0, {0}, {0}, 0, 0};
    dfs_shared[threadIdx.x].cur_num_nodes_gpu++;
    if (DCube::DTablebaseContains(dfs_shared[threadIdx.x].start)) {
        dfs_shared[threadIdx.x].cur_num_tb_hits_gpu++;
    }

    __shared__ int needs_compute;
    __shared__ int finished_compute;

    // device fixed max size stack in registes
    DFSStack dfs_stack[kMaxDFSDepth+1];
    int8_t dfs_stack_idx;

    while (true) {
        size_t acc_num_nodes_gpu = 0;
        size_t acc_num_tb_his_gpu = 0;
        uint8_t cur_idx;

        // load from dfs_shared
        {
            __syncthreads();
            if (threadIdx.x == 0) {
                needs_compute = 0;
                finished_compute = blockIdx.x-1;
            }

            dfs_stack_idx = dfs_shared[threadIdx.x].dfs_stack_idx;
            DState loading_state = dfs_shared[threadIdx.x].start;
            acc_num_nodes_gpu = dfs_shared[threadIdx.x].cur_num_nodes_gpu;
            acc_num_tb_his_gpu = dfs_shared[threadIdx.x].cur_num_tb_hits_gpu;
            cur_idx = dfs_shared[threadIdx.x].idx;
            int loading_idx = 0;
            for (int i = 0; i < kMaxDFSDepth && loading_idx <= dfs_stack_idx; i++) {
                if (dfs_shared[threadIdx.x].depths[loading_idx] >= i) { // nomal rotation
                    dfs_stack[loading_idx].state = loading_state;
                    loading_state = DCube::Rotate(loading_state, dfs_shared[threadIdx.x].rotations[loading_idx]).state;
                    loading_idx++;
                }
                else {
                    loading_state = DCube::Rotate(loading_state, uint8_t(kDNumRotations-1)).state;
                }
            }
        }

        // do progress on the current
        constexpr int kBatchSync = 1000;
        for (int i = 0; i < kBatchSync && dfs_stack_idx >= 0; i++) {
            int8_t cur_depth = dfs_stack[dfs_stack_idx].depth;
            DRotateReturn next = DCube::Rotate(dfs_stack[dfs_stack_idx].state, dfs_stack[dfs_stack_idx].rotation++);

            if (dfs_stack[dfs_stack_idx].rotation >= kNumRotations) {
                dfs_stack_idx--;
            }

            if (next.isLegal) {
                acc_num_nodes_gpu++;
                if (DCube::DTablebaseContains(next.state)) {
                    acc_num_tb_his_gpu++;
                }
                if (cur_depth+1 < max_depth) {
                    dfs_stack[++dfs_stack_idx] = {int8_t(cur_depth+1), 0, next.state};
                }
            }
        }

        {
            __syncthreads();
            int new_idx;
            if (dfs_stack_idx >= 0) { // needs_compute
                 new_idx = atomicAdd(&needs_compute, 1);
            }
            else {
                new_idx = atomicAdd(&finished_compute, -1);
            }

            // upload to shared memory
            dfs_shared[new_idx].start = dfs_stack[0].state;
            dfs_shared[new_idx].idx = cur_idx;
            dfs_shared[new_idx].dfs_stack_idx = dfs_stack_idx;
            dfs_shared[new_idx].cur_num_nodes_gpu = acc_num_nodes_gpu;
            dfs_shared[new_idx].cur_num_tb_hits_gpu = acc_num_tb_his_gpu;
            for (int i = 0; i < kMaxDFSDepth; i++) {
                dfs_shared[new_idx].depths[i] = dfs_stack[i].depth;
                dfs_shared[new_idx].rotations[i] = dfs_stack[i].rotation;
            }

            __syncthreads();
            if (needs_compute == 0) {
                break;
            }
        }
    }

    d_num_nodes_gpu[size_t(dfs_shared[threadIdx.x].idx) + (size_t(blockIdx.x) * blockDim.x)] = dfs_shared[threadIdx.x].cur_num_nodes_gpu;
    d_num_tb_hits_gpu[size_t(dfs_shared[threadIdx.x].idx) + (size_t(blockIdx.x) * blockDim.x)] = dfs_shared[threadIdx.x].cur_num_tb_hits_gpu;
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
