#include <cuda.h>
#include <cuda_device_runtime_api.h>
#include <cstddef>
#include <vector>

#include "cube.hpp"
#include "logger.hpp"
#include "settings.hpp"


template<typename T>
void UploadToDevice(const std::vector<T>& data, T*& d_pointer) {
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
    int rotation;
    int depth;
};


// definded in cuda_search.cu
__device__ bool Rotate(Cube::State& state, const uint8_t& rotation);

__device__ constexpr int kDNumRotations = 18;


constexpr size_t kBatching = 10;
__global__ void DFSGlobal(Cube::State* d_random_position, size_t* d_num_nodes_gpu, size_t num_random_position, int depth) {
    size_t index = threadIdx.x + (blockIdx.x * blockDim.x);

    // device fixed max size stack
    int dfs_stack_size = depth + kBatching + 1;
    DFSStack* dfs_stack = (DFSStack*) std::malloc(dfs_stack_size * sizeof(DFSStack));
    int dfs_stack_idx = -1;

    size_t upper_start = (index*kBatching)+kBatching - 1;
    if (upper_start >= num_random_position) {
        upper_start = num_random_position - 1;
    }
    for (int i = upper_start;  i >= int(index*kBatching); i--) {
        dfs_stack[++dfs_stack_idx] = {d_random_position[i], 0, depth};
    }

    int batch_idx = 0;
    int currcnt = 0;
    Cube::State next = dfs_stack[0].state;
    while (dfs_stack_idx >= 0) {
        currcnt++;
        DFSStack& current = dfs_stack[dfs_stack_idx];
        if (current.depth == 0) {
            dfs_stack_idx--;
            continue;
        }
        if (current.rotation >= kDNumRotations) {
            dfs_stack_idx--;
            if (current.depth == depth) {
                d_num_nodes_gpu[(index*kBatching)+batch_idx] = currcnt;
                currcnt = 0;
                batch_idx++;
            }
            continue;
        }
        next = current.state;
        if (Rotate(next, current.rotation++)) {
            dfs_stack[++dfs_stack_idx] = {next, 0, current.depth-1};
        }
    }
}


void GPUDFS(const std::vector<Cube::State>& random_position, std::vector<size_t>& num_nodes_gpu) {
    Cube::State* d_random_position = nullptr;
    size_t* d_num_nodes_gpu = nullptr;
    UploadToDevice(random_position, d_random_position);
    UploadToDevice(num_nodes_gpu, d_num_nodes_gpu);
    DFSGlobal<<<(random_position.size()/kBatching/kBlockDim)+1, kBlockDim>>>(d_random_position, d_num_nodes_gpu, random_position.size(), Settings::GetDFSDepth());
    cudaDeviceSynchronize();
}
