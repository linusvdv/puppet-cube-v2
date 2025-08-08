#pragma once
#include <cstdint>
#include <vector>


std::vector<uint8_t> EdgeHeuristicInitialization(const std::vector<uint16_t>& edge_orientation, const std::vector<uint32_t>& edge_position, uint16_t solved_orientation, uint32_t solved_position);
