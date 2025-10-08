#pragma once
#include "cube.hpp"
#include "search.cuh"
#include "logger.hpp"


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
        LOG_EXTRA("Start Tablebase Uploading Precomutation to Device");
        UploadTablebaseToDevice(tablebase.back());
        LOG_INFO("Tablebase Precomutation Uploaded to Device");
        #endif // USE_CUDA
    }

    static std::vector<std::vector<State>> tablebase;
};
