#pragma once
#include <vector>

#include <parallel_hashmap/phmap.h>

#include "cube.hpp"
#include "logger.hpp"


void UploadTablebaseToDevice(const std::vector<State>& tablebebase);


void UploadRandomPositionsToDevice(const std::vector<State>& random_positions);


void TimeBCHTGPU();
