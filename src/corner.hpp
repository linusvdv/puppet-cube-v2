#pragma once
#include <cstdint>

#include "utils.hpp"


namespace corner {
constexpr int kNumCorners = 8;
constexpr uint16_t kNumPos = Factorial(kNumCorners);
constexpr uint16_t kNumOrient = Power(3, 7);


void Init();
uint64_t GetHeuristic(uint16_t pos, uint16_t orient);
void Rotate(uint16_t& pos, uint16_t& orient, uint8_t rot);
}
