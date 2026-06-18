#pragma once
#include <cstdint>

#include "utils.hpp"


namespace corner {
constexpr int kNumCorners = 8;
constexpr uint16_t kNumPos = Factorial(kNumCorners);
constexpr uint16_t kNumOrient = Power(3, 7);
void Init();
}
