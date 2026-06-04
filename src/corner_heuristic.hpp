#pragma once
#include <cstdint>
#include <vector>


void CornerHeuristicInitialization(const std::vector<uint16_t>& corner_orientation, const std::vector<uint16_t>& corner_position, std::vector<uint16_t>& corner_heuristic);
