#pragma once
#include <cstdint>

#include "utils.hpp"


constexpr int kNumEdges = 12;


namespace edge {
constexpr uint16_t kNumOrient = 1 << (kNumEdges-1);
constexpr uint8_t kNumSym = Factorial(3) * (1<<3);

constexpr int kNumStoredPerBucket = 16;


void Init();
void Rotate(uint32_t& pos, uint8_t& sym, uint16_t& orient, uint8_t rot);
uint8_t GetHeuristic(uint32_t pos, uint16_t orient);
}
