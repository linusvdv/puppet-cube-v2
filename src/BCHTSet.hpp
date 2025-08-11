#pragma once
#include <vector>

#include "cube.hpp"

constexpr int kBucketSize = 16;
constexpr float kLoadFacor = 0.9;


std::vector<Cube::State> BuildBCHTSet(const Cube::Tablebase& tablebase);


bool BCHTSetContains(const std::vector<Cube::State>& table, const Cube::State& key);
