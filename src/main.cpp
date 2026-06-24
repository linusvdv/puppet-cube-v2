#include <filesystem>

#include "corner.hpp"
#include "edge.hpp"
#include "info.hpp"
#include "logger.hpp"
#include "rotation.hpp"
#include "search.hpp"
#include "settings.hpp"
#include "tablebase.hpp"
#include "transposition_table.hpp"
#include "utils.hpp"

#ifdef USE_CUDA
#include "corner_bridge.hpp"
#include "edge_bridge.hpp"
#include "info_bridge.hpp"
#include "search_bridge.hpp"
#include "tablebase_bridge.hpp"
#endif


int main(int argc, char *argv[]) {
    LOG_INFO("Puppet Cube V2 by Linus VandeVondele");

    // settings initialization
    Settings(argc, argv);
    LOG_MEMORY();

    if (Settings::GetHardwareInfo()) {
        GetHostInfo();
    }
    #ifdef USE_CUDA
    if (Settings::GetHardwareInfo() && Settings::UseCuda()) {
        GetDeviceInfo();
    }
    #endif // USE_CUDA

    // precomputation
    if (!std::filesystem::exists(GetFilePath(""))) {
        if (std::filesystem::create_directories(GetFilePath(""))) {
            LOG_ALL("Create precomputation folder for precomputation");
        }
        else {
            LOG_CRITICAL("Failed to create folder for precomputation");
        }
    }
    RotationInit();
    edge::Init();
    corner::Init();
    tablebase::Init();
    LOG_INFO("Loaded Precomputation");

    #ifdef USE_CUDA
    LOG_EXTRA("Start Edge Precomputation Uploading to Device");
    edge::UploadPrecomputationToDevice();
    LOG_EXTRA("Start Corner Precomputation Uploading to Device");
    corner::UploadPrecomputationToDevice();
    LOG_EXTRA("Start Tablebase Precomputation Uploading to Device");
    tablebase::UploadPrecomputationToDevice();
    LOG_INFO("Precomputation Uploaded to Device");
    LOG_MEMORY();
    #endif // USE_CUDA

    return 0;

    /*
    TranspositionTable::Initialize(Settings::GetTTSize());
    LOG_INFO("Transposition Table Initialized");
    LOG_MEMORY();

    #ifdef USE_CUDA
    if (Settings::UseCuda()) {
        CudaConstMemInitialize();
        LOG_INFO("Cuda Constant Memory Initialized");
        LOG_MEMORY();
    }
    #endif // USE_CUDA

    SearchManager();
    LOG_INFO("Search Computed");
    LOG_MEMORY();
    */
    return 0;
}
