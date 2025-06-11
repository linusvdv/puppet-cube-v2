#pragma once
#include <cstdint>
#include <vector>


constexpr int kNumCornerData = 88179840;  // 8! * 3^7
constexpr int kNumEdgeData = 42577920;  // 12! / 6! * 2^6
constexpr int kNumRotations = 18;
constexpr int kNumCorners = 8;
constexpr int kNumEdges = 12;


enum Rotations : uint8_t {
    kR = 0,
    kRc = 1,
    kL = 2,
    kLc = 3,
    kU = 4,
    kUc = 5,
    kD = 6,
    kDc = 7,
    kF = 8,
    kFc = 9,
    kB = 10,
    kBc = 11,
    kM = 12,
    kMc = 13,
    kE = 14,
    kEc = 15,
    kS = 16,
    kSc = 17
};


class Cube {
public:
    // corner and edge precomputation
    static void Initialize();

private:
    // precomputation
    static std::vector<uint16_t> corner_orientation;
    static std::vector<uint16_t> corner_position;
    static std::vector<uint16_t> corner_heuristic;

    static std::vector<uint16_t> edge_orientation;
};
