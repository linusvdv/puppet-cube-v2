#include <cuda_runtime.h>

#include "logger.hpp"


void GetDeviceInfo() {
    int device_count;
    cudaError_t err = cudaGetDeviceCount(&device_count);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }

    LOG_ALL("Device Count:", device_count);
    for (int dev = 0; dev < device_count; dev++) {
        cudaDeviceProp prop;
        cudaError_t err = cudaGetDeviceProperties(&prop, dev);
        if (err != cudaSuccess) {
            LOG_CRITICAL(cudaGetErrorString(err));
        }

        LOG_ALL("Device Nr", SkipSpace(dev), ":", prop.name);
        LOG_EXTRA("Compute capability:", SkipSpace(prop.major), SkipSpace("."), prop.minor);
        LOG_EXTRA("Multiprocessors (SM):", prop.multiProcessorCount);
        LOG_EXTRA("Total global memory (bytes):", prop.totalGlobalMem);
        LOG_EXTRA("Shared mem per block (bytes):", prop.sharedMemPerBlock);
        LOG_EXTRA("Registers per block:", prop.regsPerBlock);
        LOG_EXTRA("Warp size:", prop.warpSize);
        LOG_EXTRA("Max threads per block:", prop.maxThreadsPerBlock);
        LOG_EXTRA("Max threads per SM:", prop.maxThreadsPerMultiProcessor);
        LOG_EXTRA("Total const memory (bytes):", prop.totalConstMem);
        LOG_EXTRA("Memory bus width (bits):", prop.memoryBusWidth);
        LOG_EXTRA("L2 cache size (bytes):", prop.l2CacheSize);
        LOG_EXTRA("Integrated GPU:", prop.integrated, (prop.integrated == 1 ? "[integrated (motherboard) GPU]" : "[discrete (card) component]"));
        LOG_EXTRA("Can map host memory:", (prop.canMapHostMemory == 1 ? "true" : "false"));
        LOG_EXTRA("Concurrent kernels:", (prop.concurrentKernels == 1 ? "true" : "false"));
        LOG_EXTRA("ECC enabled:", (prop.ECCEnabled == 1 ? "true" : "false"));
        LOG_EXTRA("Async engine count:", prop.asyncEngineCount);
        LOG_EXTRA("Unified addressing:", (prop.unifiedAddressing == 1 ? "true" : "false"));
    }
}
