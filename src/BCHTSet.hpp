#pragma once
#include <vector>

#include "cube.hpp"
#include "tablebase.hpp"


constexpr int kBucketSize = 2;
constexpr float kLoadFacor = 0.8;


std::vector<PackedState> BuildBCHTSet(const TablebasePrecomputation& tablebase);


bool BCHTSetContains(const std::vector<PackedState>& table, const State& key);
