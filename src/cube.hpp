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
    // 18 bits
    #pragma pack(push, 1)
    struct State {
        uint16_t corner_orientation = -1; // 12 bits
        uint16_t corner_position = -1;    // 16 bits
        uint16_t edge_orientation = -1;   // 11 bits
        uint32_t edge_position_1 = -1;    // 20 bits
        uint32_t edge_position_2 = -1;    // 20 bits

        std::strong_ordering operator<=>(const State&) const = default;

        bool Rotate(uint8_t rotation);

        friend std::size_t hash_value(const Cube::State& s) {
            constexpr int kMagicVal = 0x9e3779b9;
            std::size_t h1 = std::hash<uint16_t>{}(s.corner_orientation);
            std::size_t h2 = std::hash<uint16_t>{}(s.corner_position);
            std::size_t h3 = std::hash<uint16_t>{}(s.edge_orientation);
            std::size_t h4 = std::hash<uint32_t>{}(s.edge_position_1);
            std::size_t h5 = std::hash<uint32_t>{}(s.edge_position_2);

            // Combine hashes (standard method)
            std::size_t seed = h1;
            seed ^= h2 + kMagicVal + (seed << 6) + (seed >> 2);
            seed ^= h3 + kMagicVal + (seed << 6) + (seed >> 2);
            seed ^= h4 + kMagicVal + (seed << 6) + (seed >> 2);
            seed ^= h5 + kMagicVal + (seed << 6) + (seed >> 2);

            return seed;
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
