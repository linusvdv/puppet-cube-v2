#pragma once
#include <cstdint>
#include <vector>


void EdgeHeuristicInitialization(const std::vector<uint16_t>& edge_orientation, const std::vector<uint32_t>& edge_position, uint16_t solved_orientation, uint32_t solved_position, std::vector<uint8_t>& edge_heuristic);
