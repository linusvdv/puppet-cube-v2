#pragma once
#include <cstdint>
#include <vector>
#include "cube.hpp"

void UploadCubeComputationToDevice(
    const std::vector<uint16_t>& corner_orientations,
    const std::vector<uint16_t>& corner_positions,
    const std::vector<uint16_t>& corner_heuristics,

    const std::vector<uint16_t>& edge_orientations,
    const std::vector<uint32_t>& edge_positions,
    const std::vector<uint8_t>& edge_heuristics
    );


void UploadTablebaseToDevice(const std::vector<State>& tablebebase);


void UploadRandomPositionsToDevice(const std::vector<State>& random_positions);
