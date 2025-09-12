#include <cuda.h>
#include <driver_types.h>
#include <cstddef>
#include <cstdint>
#include <vector>

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
    int rotation;
    int depth;
};


// definded in cuda_search.cu
__device__ bool Rotate(Cube::State& state, const uint8_t& rotation);

__device__ constexpr size_t kDNumRotations = 18;


constexpr size_t kBatching = 10;
__global__ void DFSGlobal(Cube::State* d_random_position, size_t* d_num_nodes_gpu, DFSStack* d_dfs_stack, size_t num_random_position, int depth) {
    size_t index = threadIdx.x + (size_t(blockIdx.x) * blockDim.x);
    if (index*kBatching >= num_random_position) {
        return;
    }

    // device fixed max size stack
    size_t dfs_stack_size = depth + kBatching + 1;
    int64_t dfs_stack_idx = -1;

    size_t upper_start = (index*kBatching)+kBatching - 1;
    if (upper_start >= num_random_position) {
        upper_start = num_random_position - 1;
    }
    for (int64_t i = upper_start;  i >= int64_t(index*kBatching); i--) {
        d_dfs_stack[(++dfs_stack_idx) + (index*dfs_stack_size)] = {d_random_position[i], 0, depth};
    }

    int64_t batch_idx = 0;
    size_t currcnt = 0;
    while (dfs_stack_idx >= 0) {
        currcnt++;
        DFSStack& current = d_dfs_stack[dfs_stack_idx + (index*kBatching)];
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
        Cube::State next = current.state;
        if (Rotate(next, current.rotation++)) {
            d_dfs_stack[(++dfs_stack_idx) + (index*kBatching)] = {next, 0, current.depth-1};
        }
    }

}


void GPUDFS(const std::vector<Cube::State>& random_position, std::vector<size_t>& num_nodes_gpu) {
    // upload random_position
    Cube::State* d_random_position = nullptr;
    size_t* d_num_nodes_gpu = nullptr;
    UploadToDeviceDFS(random_position, d_random_position);
    UploadToDeviceDFS(num_nodes_gpu, d_num_nodes_gpu);

    // create d_dfs_stack
    size_t grid_dim = (random_position.size()/kBatching/kBlockDim)+1;
    size_t dfs_stack_size = Settings::GetDFSDepth() + kBatching + 1;
    DFSStack* d_dfs_stack = nullptr;
    cudaError err = cudaMalloc((void**)&d_dfs_stack, grid_dim * kBlockDim * dfs_stack_size * sizeof(DFSStack));
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }

    // start kernal
    DFSGlobal<<<grid_dim, kBlockDim>>>(d_random_position, d_num_nodes_gpu, d_dfs_stack, random_position.size(), Settings::GetDFSDepth());
    err = cudaDeviceSynchronize();
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }

    // get result
    err = cudaMemcpy(num_nodes_gpu.data(), d_num_nodes_gpu, sizeof(size_t)*num_nodes_gpu.size(), cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
}
