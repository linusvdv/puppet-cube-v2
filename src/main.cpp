#include "cube.hpp"
#include "info.hpp"
#include "logger.hpp"
#include "search.hpp"
#include "settings.hpp"
#include "tablebase.hpp"

#ifdef USE_CUDA
#include "info_bridge.hpp"
#include "search_bridge.hpp"
#endif


int main (int argc, char *argv[]) {
    // settings initialization
    Settings(argc, argv);

    LOG_INFO("Puppet Cube V2 by Linus VandeVondele");
    LOG_MEMORY();

    if (Settings::GetHardwareInfo()) {
        GetHostInfo();
    }
    #ifdef USE_CUDA
    if (Settings::GetHardwareInfo() && Settings::UseCuda()) {
        GetDeviceInfo();
    }
    #endif // USE_CUDA

    Cube::Initialize();
    LOG_INFO("Cube Initialized");
    Cube::UploadComputationToDevice();

    Tablebase::Initialize();
    LOG_INFO("Tablebase Initialized");
    Tablebase::UploadComputationToDevice();
    LOG_MEMORY();

    #ifdef USE_CUDA
    if (Settings::UseCuda()) {
        CudaConstMemInitialize();
        LOG_INFO("Cuda Constant Memory Inizialized");
        LOG_MEMORY();
    }
    #endif // USE_CUDA

    SearchManager();
    LOG_INFO("Search Computed");
    LOG_MEMORY();
    return 0;
}
