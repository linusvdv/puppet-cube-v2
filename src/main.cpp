#include "cube.hpp"
#include "logger.hpp"
#include "settings.hpp"
#include "tablebase.hpp"

#ifdef USE_CUDA
#include "cuda_search.cuh"
#endif


int main (int argc, char *argv[]) {
    LOG_INFO("Puppet Cube V2 by Linus VandeVondele");
    LOG_MEMORY();

    // settings initialization
    Settings(argc, argv);

    Cube::Initialize();
    LOG_INFO("Cube Initialized");
    Cube::UploadComputationToDevice();

    Tablebase::Initialize();
    LOG_INFO("Tablebase Initialized");
    Tablebase::UploadComputationToDevice();

    #ifdef USE_CUDA
    if (Settings::GetShouldPerformanceTest()) {
        LOG_EXTRA("Start timing on GPU");
        std::chrono::time_point gpu_time = std::chrono::high_resolution_clock::now(); // get the current time

        TimeBCHTtable();

        std::chrono::time_point gpu_since_epoch = std::chrono::high_resolution_clock::now(); // get the duration since epoch
        std::chrono::milliseconds gpu_millis = std::chrono::duration_cast<std::chrono::milliseconds>(gpu_since_epoch - gpu_time);
        LOG_ALL("Time duration on GPU:", gpu_millis.count());
    }
    #endif

    return 0;
}
