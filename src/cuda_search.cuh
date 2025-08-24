#pragma once
#include <vector>

#include <parallel_hashmap/phmap.h>

#include "cube.hpp"


void UploadCubeComputationToDevice(
    const std::vector<uint16_t>& corner_orientations,
    const std::vector<uint16_t>& corner_positions,
    const std::vector<uint16_t>& corner_heuristics,

    const std::vector<uint16_t>& edge_orientations,
    const std::vector<uint32_t>& edge_positions,
    const std::vector<uint8_t>& edge_heuristics
    );


void UploadTablebaseToDevice(const std::vector<Cube::State>& tablebebase);


void UploadRandomPositionsToDevice(const std::vector<Cube::State>& random_positions);


void TimeBCHTtable();
