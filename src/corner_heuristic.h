#pragma once
#include <cstdint>
#include <tbb/concurrent_vector.h>
#include <vector>


tbb::concurrent_vector<uint16_t> CornerHeuristicInitialization(const std::vector<uint16_t>& corner_orientation, const std::vector<uint16_t>& corner_position);
