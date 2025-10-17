#pragma once
#include <parallel_hashmap/phmap.h>

#include "cube.hpp"
#include "cuda_memory_transfer.cuh"
#include "search.cuh"
#include "logger.hpp"

#ifdef USE_CUDA
#include "cube_bridge.hpp"
#endif // USE_CUDA


using TablebasePrecomputation = phmap::parallel_flat_hash_set<State,
    phmap::priv::hash_default_hash<State>,
    phmap::priv::hash_default_eq<State>,
    phmap::priv::Allocator<State>,
    12, std::mutex>;


class Tablebase {
public:
    static void Initialize();

    static void UploadComputationToDevice() {
        #ifdef USE_CUDA
        LOG_EXTRA("Start Tablebase Uploading Precomputation to Device");
        UploadTablebaseToDevice(tablebase.back());
        LOG_INFO("Tablebase Precomputation Uploaded to Device");
        #endif // USE_CUDA
    }

    static std::vector<std::vector<State>> tablebase;
};
