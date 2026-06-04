#include <algorithm>
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
#include "utils.hpp"
#include "settings.hpp"


std::vector<uint16_t> Cube::corner_orientations;
std::vector<uint16_t> Cube::corner_positions;
std::vector<uint16_t> Cube::corner_heuristics;

std::vector<uint16_t> Cube::edge_orientations;
std::vector<uint32_t> Cube::edge_positions;
std::vector<uint8_t> Cube::edge_heuristics;


uint8_t GetRevRotation(uint8_t rotation) {
    if (rotation % 2 == 0) {
        return rotation + 1;
    }
    return rotation - 1;
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
        [&](){CornerOrientationInitialization(corner_orientations);}, "[1/6] Corner Orientations");

    LoadOrGenerate("corner_positions.bin", corner_positions, kCornerPositionsSize,
        [&](){CornerPositionInitialization(corner_positions);}, "[2/6] Corner Positions");

    LoadOrGenerate("corner_heuristics.bin", corner_heuristics, kNumCornerHeuristic,
        [&](){CornerHeuristicInitialization(corner_orientations, corner_positions, corner_heuristics);}, "[3/6] Corner Heuristics");

    LoadOrGenerate("edge_orientations.bin", edge_orientations, kEdgeOrientationSize,
        [&](){EdgeOrientationInitialization(edge_orientations);}, "[4/6] Edge Orientations");

    LoadOrGenerate("edge_positions.bin", edge_positions, kEdgePositionsSize,
        [&](){EdgePositionInitialization(edge_positions);}, "[5/6] Edge Positions");

    LoadOrGenerate("edge_heuristics.bin", edge_heuristics, kNumEdgeHeuristic,
        [&](){EdgeHeuristicInitialization(edge_orientations, edge_positions, 0, 0, edge_heuristics);}, "[6/6] Edge Heuristics");
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
    if (Settings::UseCuda()) {
        LOG_EXTRA("Start Cube Uploading Precomputation to Device");
        UploadCubeComputationToDevices(corner_orientations, corner_positions, corner_heuristics, edge_orientations, edge_positions, edge_heuristics);
        LOG_INFO("Cube Precomputation Uploaded to Device");
        LOG_MEMORY();
    }
    #endif // USE_CUDA
}


uint16_t Cube::GetCurCornerHeuristic(const State& state) {
    uint16_t corner_orientation = state.hash_2 >> 20;      // 12 bites         NOLINT
    uint16_t corner_position = state.hash_1;               // 16 bites         NOLINT
    return uint8_t(corner_heuristics[(corner_orientation*kNumCornerPositions) + corner_position] & ((uint16_t(1) << 8) - 1)); // NOLINT
}


void Cube::SetCurCornerHeuristic(const State& state) {
    cur_corner_heuristic_ = GetCurCornerHeuristic(state);
}

void Cube::SetCurEdgeHeuristic1(const State& state) {
    uint32_t orientation = state.hash_3 >> 20; // NOLINT
    uint32_t position = state.hash_2 & ((uint32_t(1) << 20) - 1); // NOLINT
    cur_edge_heuristic_1_ = edge_heuristics[(orientation*kNumEdgePositions) + position];
}

void Cube::SetCurEdgeHeuristic2(const State& state) {
    uint32_t orientation = state.hash_3 >> 20; // NOLINT
    orientation |= (std::popcount(orientation)%2) << (kNumEdges-1); // get last bit using even num bits parity
    uint32_t orientation_r = 0;
    for (int i = 1; i < kNumEdges; i++) {
        orientation_r |= ((orientation >> i) & uint32_t(1)) << (kNumEdges-1-i);
    }

    uint32_t position = state.hash_3 & ((uint32_t(1) << 20) - 1); // NOLINT
    uint32_t position_r = 0;
    uint32_t temp = kNumEdgePositions;
    for (int i = kNumEdges-1; i >= 6; i--) { // NOLINT
        temp /= i+1;
        position_r *= i+1;
        position_r += i - ((position / temp) % (i + 1)); // NOLINT
    }
    cur_edge_heuristic_2_ = edge_heuristics[(orientation_r*kNumEdgePositions) + position_r];
}


uint8_t Cube::GetMaxHeuristic(const State& state) {
    if (cur_corner_heuristic_ == uint8_t(-1)) {
        SetCurCornerHeuristic(state);
    }
    if (cur_edge_heuristic_1_ == uint8_t(-1)) {
        SetCurEdgeHeuristic1(state);
    }
    if (cur_edge_heuristic_2_ == uint8_t(-1)) {
        SetCurEdgeHeuristic2(state);
    }
    return std::max({cur_corner_heuristic_, cur_edge_heuristic_1_, cur_edge_heuristic_2_});
}


float Cube::GetAppHeuristic(const State& state) {
    if (cur_corner_heuristic_ == uint8_t(-1)) {
        SetCurCornerHeuristic(state);
    }
    if (cur_edge_heuristic_1_ == uint8_t(-1)) {
        SetCurEdgeHeuristic1(state);
    }
    if (cur_edge_heuristic_2_ == uint8_t(-1)) {
        SetCurEdgeHeuristic2(state);
    }
    return cur_corner_heuristic_ + cur_edge_heuristic_1_ + cur_edge_heuristic_2_;
}
