#pragma once
#include <vector>

#include <parallel_hashmap/phmap.h>

#include "cube.hpp"


void UploadCubeComputationToDevice(
    std::vector<uint16_t>& corner_orientations,
    std::vector<uint16_t>& corner_positions,
    std::vector<uint16_t>& corner_heuristics,

    std::vector<uint16_t>& edge_orientations,
    std::vector<uint32_t>& edge_positions,
    std::vector<uint8_t>& edge_heuristics
    );
