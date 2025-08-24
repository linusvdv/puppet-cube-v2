#pragma once
#include "cube.hpp"


__device__ bool DBCHTSetContains(const Cube::State* d_tablebase, const size_t& d_tablebase_size, const Cube::State& key);
