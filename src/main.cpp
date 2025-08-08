#include "cube.hpp"
#include "logger.hpp"
#include "settings.hpp"

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

    Cube::TablebaseInitialization();
    LOG_INFO("Tablebase Initialized");

    #ifdef USE_CUDA
    Cube::Initialize();
    #endif

    return 0;
}
