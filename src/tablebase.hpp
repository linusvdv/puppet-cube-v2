#pragma once
#include "cube.hpp"
#include "cuda_search.cuh"
#include "logger.hpp"


using TablebasePrecomputation = phmap::parallel_flat_hash_set<Cube::State,
    phmap::priv::hash_default_hash<Cube::State>,
    phmap::priv::hash_default_eq<Cube::State>,
    phmap::priv::Allocator<Cube::State>,
    12, std::mutex>;


struct Tablebase {
    static void Initialize();

    static void UploadComputationToDevice() {
        #ifdef USE_CUDA
        LOG_EXTRA("Start Tablebase Uploading Precomutation to Device");
        UploadTablebaseToDevice(tablebase.back());
        LOG_INFO("Tablebase Precomutation Uploaded to Device");
        #endif // USE_CUDA
    }

private:
    static std::vector<std::vector<Cube::State>> tablebase;
};
