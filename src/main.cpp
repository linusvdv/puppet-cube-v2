#include <filesystem>

#include "cube.hpp"
#include "duplicate_rotations.hpp"
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
#include "info_bridge.hpp"
#include "search_bridge.hpp"
#endif


int main(int argc, char *argv[]) {
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

    // precomputation
    if (!std::filesystem::exists(GetFilePath(""))) {
        if (std::filesystem::create_directories(GetFilePath(""))) {
            LOG_ALL("Create precomputation folder for binaries");
        }
        else {
            LOG_ERROR("Failed to create folder for binaries");
        }
    }
    RotationInit();
    edge::Init();
    corner::Init();

    // Cube::Initialize();
    // DuplicateRotations::Initialize();
    // LOG_INFO("Cube Initialized");
    // Cube::UploadComputationToDevice();

    // Tablebase::Initialize();
    // LOG_INFO("Tablebase Initialized");
    // Tablebase::UploadComputationToDevice();
    // LOG_MEMORY();

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
    return 0;
}
