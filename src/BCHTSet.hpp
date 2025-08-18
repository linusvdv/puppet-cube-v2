#pragma once
#include <vector>

#include "cube.hpp"
#include "tablebase.hpp"

constexpr int kBucketSize = 1;
constexpr float kLoadFacor = 0.85;


std::vector<Cube::State> BuildBCHTSet(const TablebasePrecomputation& tablebase);


bool BCHTSetContains(const std::vector<Cube::State>& table, const Cube::State& key);
