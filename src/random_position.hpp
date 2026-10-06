#pragma once
#include <cstdint>
#include <vector>

#include "cube.hpp"


void MultithreadRandomPosition(std::vector<State>& random_positions, std::vector<std::vector<uint8_t>>& scrambles,
                               int seed_start, int seed_end, int seed_offset);

// generates random starting positions and additionally collects the applied scramble rotations
std::vector<State> RandomPositions (const int& num_elements, int seed_offset, std::vector<std::vector<uint8_t>>& scrambles);
