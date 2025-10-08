#include <cuda.h>
#include <vector>

#include "BCHTSet.cuh"
#include "cube.cuh"
#include "search.cuh"
#include "logger.hpp"
#include "settings.hpp"
#include "tablebase.hpp"


template<typename T1, typename T2>
void UploadToDevice(const std::vector<T1>& data, T2*& d_pointer) {
    static_assert(sizeof(T1) == sizeof(T2));
    static_assert(alignof(T1) == alignof(T2));

    T2* temp_pointer = nullptr;
    cudaError_t err = cudaMalloc((void **)&temp_pointer, sizeof(T1)*data.size());
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
    err = cudaMemcpy(temp_pointer, data.data(), sizeof(T1)*data.size(), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
    err = cudaMemcpyToSymbol(d_pointer, &temp_pointer, sizeof(T2*));
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
}


__device__ DState* d_random_positions = nullptr;
__device__ size_t d_random_positions_size = 0;

extern __device__ DState* d_tablebase;
extern __device__ size_t d_tablebase_size;

size_t random_positions_size = 0;


void UploadRandomPositionsToDevice(const std::vector<State>& random_positions) {
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
        if (DBCHTSetContains(d_tablebase, d_tablebase_size, d_random_positions[i])) {
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
        LOG_MEMORY();
    }
}
