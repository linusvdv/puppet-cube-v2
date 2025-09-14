#pragma once
#include <vector>

#include "cube.hpp"


void GPUDFS(const std::vector<Cube::State>& random_position, std::vector<size_t>& num_nodes_gpu, std::vector<size_t>& num_tb_hits_gpu);
