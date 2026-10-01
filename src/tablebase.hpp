#pragma once
#include "cube.hpp"


namespace tablebase {
constexpr int kBucketSize = 2;


void Init();
bool Contains(const State& state);
bool Contains(const State& state, int depth);
}
