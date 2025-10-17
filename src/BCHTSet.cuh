#pragma once
#include "cube.cuh"
#include "cube.hpp"


__device__ bool DBCHTSetContains(const DState* d_tablebase, const size_t& d_tablebase_size, const DState& key);
