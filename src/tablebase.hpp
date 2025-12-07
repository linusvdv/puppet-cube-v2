#pragma once
#include <parallel_hashmap/phmap.h>

#include "cube.hpp"
#include "settings.hpp"
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
        if (Settings::UseCuda()) {
            LOG_EXTRA("Start Tablebase Uploading Precomputation to Device");
            UploadTablebaseToDevice(tablebase.back());
            LOG_INFO("Tablebase Precomputation Uploaded to Device");
        }
        #endif // USE_CUDA
    }

    static std::vector<std::vector<State>> tablebase;
};
