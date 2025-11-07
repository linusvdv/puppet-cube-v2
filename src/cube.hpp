#pragma once
#include <algorithm>
#include <bit>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <vector>
#include "logger.hpp"


constexpr int kNumCorners = 8;
constexpr int kNumEdges = 12;
constexpr int kNumRotations = 18;

constexpr int kNumCornerOrientation = 2187;  // 3^7
constexpr int kCornerOrientationSize = kNumCornerOrientation * kNumRotations; // 3^7 * 18
constexpr int kNumCornerPositions = 40320;  // 8!
constexpr int kCornerPositionsSize = kNumCornerPositions * kNumRotations;
constexpr int kNumCornerHeuristic = kNumCornerOrientation * kNumCornerPositions;

constexpr int kNumEdgeOrientation = 2048;  // 2^11
constexpr int kEdgeOrientationSize = kNumEdgeOrientation * kNumRotations;  // 2^11 * 18
constexpr int kNumEdgePositions = 665280;  // 12! / 6!
constexpr int kEdgePositionsSize = kNumEdgePositions * kNumRotations;  // 12! / 6! * 18
constexpr int kNumEdgeHeuristic = kNumEdgePositions * kNumEdgeOrientation;


enum Rotations : uint8_t {
    kR,
    kRc,
    kL,
    kLc,
    kU,
    kUc,
    kD,
    kDc,
    kF,
    kFc,
    kB,
    kBc,
    kM,
    kMc,
    kE,
    kEc,
    kS,
    kSc
};


uint8_t GetRevRotation(uint8_t rotation);


// 10 bytes
struct State {
    uint16_t hash_1 = -1;
    uint32_t hash_2 = -1;
    uint32_t hash_3 = -1;

    constexpr State(const uint16_t& corner_orientation,  // 12 bites
                    const uint16_t& corner_position,     // 16 bites
                    const uint16_t& edge_orientation,    // 11 bites
                    const uint32_t& edge_position_1,     // 20 bites
                    const uint32_t& edge_position_2) {   // 20 bites
        // hash 1
        hash_1 = corner_position;    // 16 bites

        // hash 2
        hash_2 = corner_orientation; // 12 bites
        hash_2 <<= 20; // NOLINT
        hash_2 |= edge_position_1;   // 20 bites

        // hash 3
        hash_3 = edge_orientation;   // 11 bites
        hash_3 <<= 20; // NOLINT
        hash_3 |= edge_position_2;   // 20 bites
    }

    // Default not legal State
    constexpr State() {}

    std::strong_ordering operator<=>(const State&) const = default;

    static constexpr uint64_t kMulA = 0x2545f4914f6cdd1dULL;
    static constexpr uint64_t kMulB = 0x9e3779b97f4a7c15ULL;

    static constexpr uint64_t kXORlow1 = 0x123456789abcdef0ULL;
    static constexpr uint64_t kXORhigh1 = 0xfedcba9876543210ULL;
    static constexpr uint64_t kXORlow2 = 0x0f1e2d3c4b5a6978ULL;
    static constexpr uint64_t kXORhigh2 = 0x87654321abcdef09ULL;

    static uint64_t Mix64(uint64_t num) {
        num ^= num >> 31;  // NOLINT
        num *= kMulA;
        num ^= num >> 33;  // NOLINT
        num *= kMulB;
        num ^= num >> 28;  // NOLINT
        return num;
    }

    template<uint64_t hash_low, uint64_t hash_high>
    uint64_t SplitMix64() const {
        return Mix64(uint64_t(hash_1) ^ hash_low) ^ Mix64(((uint64_t(hash_2) << 32) | uint64_t(hash_3)) ^ hash_high); // NOLINT
    }

    // Used for phmap
    friend std::size_t hash_value(const State& state) {  // NOLINT
        return state.SplitMix64<kXORlow1, kXORhigh1>(); // NOLINT
    }
};


constexpr State kSolvedState = State(0, 0, 0, 0, kNumEdgePositions-1);


// A cube instance is not long living
class Cube {
public:
    // corner and edge precomputation
    static void Initialize();

    static void UploadComputationToDevice();

    static std::pair<bool, State> Rotate(const State& prev_state, const uint8_t& rotation);

    Cube(){}

    uint16_t GetMaxHeuristic(const State& state);
    uint16_t GetAppHeuristic(const State& state);

private:
    // precomputation
    static std::vector<uint16_t> corner_orientations;
    static std::vector<uint16_t> corner_positions;
    static std::vector<uint16_t> corner_heuristics;

    static std::vector<uint16_t> edge_orientations;
    static std::vector<uint32_t> edge_positions;
    static std::vector<uint8_t> edge_heuristics;

    void SetCurCornerHeuristic(const State& state);
    void SetCurEdgeHeuristic1(const State& state);
    void SetCurEdgeHeuristic2(const State& state);

    uint16_t cur_corner_heuristic_ = -1;
    uint8_t cur_edge_heuristic_1_ = -1;
    uint8_t cur_edge_heuristic_2_ = -1;
};
