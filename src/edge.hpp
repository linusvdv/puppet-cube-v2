#pragma once
#include <cstdint>


constexpr int kNumEdges = 12;


namespace edge {
void Init();
void Rotate(uint32_t& pos, uint8_t& sym, uint16_t& orient, uint8_t rot);
uint8_t GetHeuristic(uint32_t pos, uint16_t orient);
}
