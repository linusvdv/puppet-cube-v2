#pragma once
#include <compare>
#include <cstdint>
#include <vector>
#include <parallel_hashmap/phmap.h>


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

class Cube {
public:
    // 10 bytes
    #pragma pack(push, 1)
    struct State {
        uint64_t hash_1 = -1;
        uint16_t hash_2 = -1;

        State(uint16_t corner_orientation, uint16_t corner_position, uint16_t edge_orientation, uint32_t edge_position_1, uint32_t edge_position_2) {
            hash_1 = 0;
            hash_1 = uint64_t(corner_orientation); // 12 bytes
            hash_1 <<= 11; // NOLINT
            hash_1 |= uint64_t(edge_orientation); // 11 bytes
            hash_1 <<= 20; // NOLINT
            hash_1 |= uint64_t(edge_position_1); // 20 bytes
            hash_1 <<= 20; // NOLINT
            hash_1 |= uint64_t(edge_position_2); // 20 bytes
            hash_2 = corner_position; // 16 bytes
        }

        State() {}

        std::strong_ordering operator<=>(const State&) const = default;

        bool Rotate(uint8_t rotation);

        friend std::size_t hash_value(const State& state) {  // NOLINT
            std::size_t h1 = std::hash<uint64_t>{}(state.hash_1 ^ 0x123456789abcdef0ULL); // NOLINT
            std::size_t h2 = std::hash<uint64_t>{}(uint64_t(state.hash_2) ^ 0xfedcba9876543210ULL); // NOLINT

            return h1 ^ h2;
        }
    };
    #pragma pack(pop)

    // corner and edge precomputation
    static void Initialize();

    static void UploadComputationToDevice();

    Cube();

private:
    // precomputation
    static std::vector<uint16_t> corner_orientations;
    static std::vector<uint16_t> corner_positions;
    static std::vector<uint16_t> corner_heuristics;

    static std::vector<uint16_t> edge_orientations;
    static std::vector<uint32_t> edge_positions;
    static std::vector<uint8_t> edge_heuristics;

    State cube_;
};
