#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "corner_heuristic.hpp"
#include "corner_orientation.hpp"
#include "corner_position.hpp"
#include "cube.hpp"
#include "cube_bridge.hpp"
#include "edge_heuristic.hpp"
#include "edge_orientation.hpp"
#include "edge_position.hpp"
#include "logger.hpp"
#include "settings.hpp"


std::vector<uint16_t> Cube::corner_orientations;
std::vector<uint16_t> Cube::corner_positions;
std::vector<uint16_t> Cube::corner_heuristics;

std::vector<uint16_t> Cube::edge_orientations;
std::vector<uint32_t> Cube::edge_positions;
std::vector<uint8_t> Cube::edge_heuristics;


// place where the precomputation is stored
std::string GetFilePath (std::string file_name) {
    // path/to/puppet-cube-v2/precomputation/file_name
    return Settings::GetRootPath() + "precomputation/" + file_name;
}


// if there is no file storing the precomputation run the precomputation
template<typename T, typename Generator>
void LoadOrGenerate(const std::string file_name, std::vector<T>& target, size_t expected_size,
                    Generator&& generate_func, const std::string& step_tag) {
    const std::string path = GetFilePath(file_name);
    if (std::FILE* file = std::fopen(path.c_str(), "rb")) {
        // read content of file
        target.resize(expected_size);
        if (std::fread(target.data(), sizeof(T), expected_size, file) == expected_size) {
            LOG_ALL(step_tag, "read from file");
            LOG_MEMORY();
        }
        else {
            LOG_CRITICAL(step_tag, "was not able to read file", path);
        }
        std::fclose(file);
    }
    // opening of the file failed
    else {
        // do the precomputation
        LOG_ALL(step_tag, "precompute ...");
        target = generate_func();
        LOG_MEMORY();

        if (target.size() != expected_size) {
            LOG_CRITICAL(step_tag, "Wrong precomputation size:", target.size(), "/", expected_size);
        }

        // save to file
        if (std::FILE* file = std::fopen(path.c_str(), "wb")) {
            if (std::fwrite(target.data(), sizeof(T), expected_size, file) != expected_size) {
                LOG_ERROR(step_tag, "failed to write full file");
            }
            std::fclose(file);
        }
        else {
            LOG_ERROR(step_tag, "not able to save precomputation to file");
        }
    }
}


void Cube::Initialize() {
    if (!std::filesystem::exists(GetFilePath(""))) {
        if (std::filesystem::create_directories(GetFilePath(""))) {
            LOG_ALL("Create precomputation folder for binaries");
        }
        else {
            LOG_ERROR("Failed to create folder for binaries");
        }
    }

    LoadOrGenerate("corner_orientations.bin", corner_orientations, kCornerOrientationSize,
        [](){return CornerOrientationInitialization();}, "[1/6] Corner Orientations");

    LoadOrGenerate("corner_positions.bin", corner_positions, kCornerPositionsSize,
        [](){return CornerPositionInitialization();}, "[2/6] Corner Positions");


    LoadOrGenerate("corner_heuristics.bin", corner_heuristics, kNumCornerHeuristic,
        [](){return CornerHeuristicInitialization(corner_orientations, corner_positions);}, "[3/6] Corner Heuristics");

    LoadOrGenerate("edge_orientations.bin", edge_orientations, kEdgeOrientationSize,
        [](){return EdgeOrientationInitialization();}, "[4/6] Edge Orientations");

    LoadOrGenerate("edge_positions.bin", edge_positions, kEdgePositionsSize,
        [](){return EdgePositionInitialization();}, "[5/6] Edge Positions");

    LoadOrGenerate("edge_heuristics.bin", edge_heuristics, kNumEdgeHeuristic,
        [](){return EdgeHeuristicInitialization(edge_orientations, edge_positions, 0, 0);}, "[6/6] Edge Heuristics");
}


constexpr std::array<uint8_t, kNumRotations> kLegalMoveIndex = {
    8, 9, 8, 9, 10, 11, 10, 11, 12, 13, 12, 13, 0, 0, 0, 0, 0, 0
};


std::pair<bool, State> Cube::Rotate(const State& prev_state, const uint8_t& rotation) {
    uint16_t corner_orientation = prev_state.hash_2 >> 20;      // 12 bites         NOLINT
    uint16_t corner_position = prev_state.hash_1;               // 16 bites         NOLINT
    uint16_t edge_orientation = prev_state.hash_3 >> 20;        // 11 bites         NOLINT
    uint32_t edge_position_1 = prev_state.hash_2 & ((1<<20)-1); // 20 bites         NOLINT
    uint32_t edge_position_2 = prev_state.hash_3 & ((1<<20)-1); // 20 bites         NOLINT
    if (kLegalMoveIndex[rotation] != 0 && ((corner_heuristics[(corner_orientation*kNumCornerPositions) + corner_position] >> kLegalMoveIndex[rotation]) & 1) == 0) {
        return {false, prev_state};
    }
    corner_orientation = corner_orientations[(corner_orientation*kNumRotations) + rotation];
    corner_position = corner_positions[(corner_position*kNumRotations) + rotation];
    edge_orientation = edge_orientations[(edge_orientation*kNumRotations) + rotation];
    edge_position_1 = edge_positions[(edge_position_1*kNumRotations) + rotation];
    edge_position_2 = edge_positions[(edge_position_2*kNumRotations) + rotation];
    return {true, State(corner_orientation, corner_position, edge_orientation, edge_position_1, edge_position_2)};
}


void Cube::UploadComputationToDevice() {
    #ifdef USE_CUDA
    LOG_EXTRA("Start Cube Uploading Precomputation to Device");
    UploadCubeComputationToDevice(corner_orientations, corner_positions, corner_heuristics, edge_orientations, edge_positions, edge_heuristics);
    LOG_INFO("Cube Precomputation Uploaded to Device");
    LOG_MEMORY();
    #endif // USE_CUDA
}
