#pragma once
#include <atomic>
#include <cuda/atomic>

#include "cuda_memory_transfer.cuh"
#include "cube.cuh"
#include "logger.hpp"
#include "settings.hpp"
#include "transposition_table.hpp"


namespace transposition_table {
__constant__ cuda::atomic<uint64_t, cuda::thread_scope_device>* d_tt_d;
__constant__ uint64_t d_tt_d_size;
// FIX: Multiple gpus
cuda::atomic<uint64_t, cuda::thread_scope_device>* tt_d;
uint64_t tt_d_size;

__device__ inline void DGetTTHash(const State& state, uint8_t depth, uint64_t& idx, uint64_t& value) {
    DGetStateHash1(d_tt_d_size>>8, idx, value, state);
    idx = (idx<<8) | (value >> 56);
    value = (value << 8) | depth;
}

__device__ inline bool DInsert(const State& state, uint8_t depth) {
    uint64_t idx;
    uint64_t value;
    DGetTTHash(state, depth, idx, value);
    uint64_t tt_value = d_tt_d[idx].load(cuda::memory_order_relaxed);

    // it is expected that this part does not guarantie that the minimum depth is in the tt
    // if a higher depth is stored the position has to be reevaluated
    // the correctness of the algorithm is still guarantied
    if ((tt_value == kDefaultTTEntry) || // no entry in TT
        ((tt_value>>8) != (value>>8)) || // other entry in TT
        (uint8_t(tt_value) < depth)) { // position with higher depth
        d_tt_d[idx].store(value, cuda::memory_order_relaxed);
        return false;
    }
    return true;
}


__device__ inline void DErase(const State& state) {
    uint64_t idx;
    uint64_t value;
    DGetTTHash(state, 0, idx, value);
    uint64_t tt_value = d_tt_d[idx].load(cuda::memory_order_relaxed);

    // it is expected that this part does not guarantie that the minimum depth is in the tt
    // if a higher depth is stored the position has to be reevaluated
    // the correctness of the algorithm is still guarantied
    if (tt_value == kDefaultTTEntry) { // no entry in TT
        return;
    }
    if ((tt_value>>8) != (value>>8)) { // other entry in TT
        return;
    }
    d_tt_d[idx].store(kDefaultTTEntry, cuda::memory_order_relaxed);
}


__global__ void DClearD() {
    uint64_t index = (uint64_t)threadIdx.x + ((uint64_t)blockIdx.x * blockDim.x);
    if (index >= d_tt_d_size) {
        return;
    }
    d_tt_d[index].store(kDefaultTTEntry, cuda::memory_order_relaxed);
}


inline void ClearD(cudaStream_t& cuda_stream) {
    DClearD<<<((tt_d_size-1)/kBlockDim)+1, kBlockDim, 0, cuda_stream>>>();
}


inline void InitDevice(cudaStream_t& cuda_stream) {
    uint64_t num_elements = (uint64_t(Settings::GetTTDeviceSize())*1024*1024) / sizeof(std::atomic<uint64_t>); // NOLINT
    constexpr uint64_t kMinTTSize = (1<<6) * (1<<8);
    if (num_elements < kMinTTSize) {
        LOG_ERROR("Device Trabsposition Table too small");
        num_elements = kMinTTSize;
        LOG_WARNING("Set Device Transposition Table size to ", num_elements*sizeof(std::atomic<uint64_t>) / 1024 / 1024, "MB");
    }
    // the number of elements has to have 8 zeros at the end
    num_elements &= ~uint64_t((1ULL<<8) - 1);
    tt_d_size = num_elements;

    MallocOnDeviceStream(tt_d, num_elements, cuda_stream);
    MemcpyToSymbolStream(tt_d, d_tt_d, cuda_stream);
    MemcpyToSymbolStream(num_elements, d_tt_d_size, cuda_stream);
}


inline void FreeDevice(cudaStream_t& cuda_stream) {
    FreeCudaPointerStream(tt_d, cuda_stream);
}
}
