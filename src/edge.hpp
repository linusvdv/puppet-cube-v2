#pragma once
#include <cstdint>
#include <vector>

#include "utils.hpp"


constexpr int kNumEdges = 12;


namespace edge {
constexpr uint16_t kNumOrient = 1 << (kNumEdges-1);
constexpr uint8_t kNumSym = Factorial(3) * (1<<3);

constexpr uint32_t kPosShift = 24;
constexpr uint32_t kPosMask = (1<<kPosShift)-1;

constexpr int kNumStoredPerBucket = 16;
constexpr uint64_t kSingleHeuristicValue = 15;


// sorted lehmer codes of the position class representatives (the class id is
// the index); used to convert user input positions into the internal
// representation
extern std::vector<uint32_t> rep_lehmer_pos;


void Init();
void Rotate(uint32_t& pos, uint8_t& sym, uint16_t& orient, uint8_t rot);
uint8_t GetHeuristic(uint32_t pos, uint16_t orient);
}
