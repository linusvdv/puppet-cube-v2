#include "BCHTSet.cuh"
#include "BCHTSet.hpp"
#include "cube.cuh"
#include "settings.hpp"
#include "cuda_memory_transfer.cuh"


__device__ constexpr uint64_t kDXORlow1 = 0x123456789abcdef0ULL;
__device__ constexpr uint64_t kDXORhigh1 = 0xfedcba9876543210ULL;
__device__ constexpr uint64_t kDXORlow2 = 0x0f1e2d3c4b5a6978ULL;
__device__ constexpr uint64_t kDXORhigh2 = 0x87654321abcdef09ULL;


__device__ bool DBCHTSetContains(const DState* d_tablebase, const size_t& d_tablebase_size, const DState& key) {
    uint32_t num_buckets = d_tablebase_size / kBucketSize;
    uint64_t h_1 = key.SplitMix64<kDXORlow1, kDXORhigh1>()%num_buckets;
    for (int j = 0; j < kBucketSize; j++) {
        DState tb_data = d_tablebase[(h_1*kBucketSize) + j];
        if (tb_data == key) {
            return true;
        }
        if (tb_data == DState()) {
            return false;
        }
    }
    uint64_t h_2 = key.SplitMix64<kDXORlow2, kDXORhigh2>()%num_buckets;
    for (int j = 0; j < kBucketSize; j++) {
        DState tb_data = d_tablebase[(h_2*kBucketSize) + j];
        if (tb_data == key) {
            return true;
        }
        if (tb_data == DState()) {
            return false;
        }
    }
    return false;
}



// =====================
// Testing purposes only
// =====================
extern __device__ DState* d_random_positions;
extern __device__ size_t d_random_positions_size;
extern size_t random_positions_size;


constexpr size_t kBatching = 10;
__global__ void DTimeBCHTtable(unsigned long long* hit, unsigned long long* miss, DCube dcube) {
    size_t index = threadIdx.x + (blockIdx.x * blockDim.x);
    for (size_t i = index*kBatching; i < (index+1)*kBatching && i < d_random_positions_size; i++) {
        if (dcube.DTablebaseContains(d_random_positions[i])) {
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

        constexpr int kNumRuns = 10;
        for (int i = 0; i < kNumRuns; i++) {
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

            DTimeBCHTtable<<<((random_positions_size/kBatching/kBlockDim)+1), kBlockDim>>>(d_hit, d_miss, GetDCube(0));
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
            LOG_EXTRA(SkipSpace("run ["), SkipSpace(i+1), SkipSpace("/"), SkipSpace(kNumRuns), "]:", h_hit, "hits", h_miss, "misses");
        }

        FreeCudaPointer(d_hit);
        FreeCudaPointer(d_miss);

        std::chrono::time_point gpu_since_epoch = std::chrono::high_resolution_clock::now(); // get the duration since epoch
        std::chrono::milliseconds gpu_millis = std::chrono::duration_cast<std::chrono::milliseconds>(gpu_since_epoch - gpu_time);
        LOG_ALL("Time duration on GPU:", gpu_millis.count());
        LOG_MEMORY();
        LOG_INFO("Time BCHT");
    }
}
