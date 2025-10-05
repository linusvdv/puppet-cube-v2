#include "cube.hpp"
#include "DFS.hpp"
#include "logger.hpp"
#include "settings.hpp"
#include "tablebase.hpp"

#ifdef USE_CUDA
#include "search.cuh"
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
    LOG_MEMORY();

    // time BCHT on GPU
    #ifdef USE_CUDA
    TimeBCHTGPU();
    #endif

    TimeDFS();
    LOG_MEMORY();

    return 0;
}
