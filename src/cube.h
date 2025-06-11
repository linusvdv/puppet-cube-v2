#pragma once
#include <cstdint>
#include <unordered_set>
#include <tuple>
#include <vector>


constexpr int kNumEdgePositions = 665280;  // 12! / 6!
constexpr int kNumCornerPositions = 40320;  // 8!
constexpr int kNumRotations = 18;
constexpr int kNumCorners = 8;
constexpr int kNumEdges = 12;


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
    struct State {
        uint16_t corner_orientation;
        uint16_t corner_position;
        uint16_t edge_orientation;
        uint32_t edge_position_1;
        uint32_t edge_position_2;

        bool operator==(const State& other) const {
            return std::tie(corner_orientation, corner_position, edge_orientation, edge_position_1, edge_position_2) ==
                std::tie(other.corner_orientation, other.corner_position, other.edge_orientation, other.edge_position_1, other.edge_position_2);
        }

        bool Rotate(uint8_t rotation);
    };

    // corner and edge precomputation
    static void Initialize();
    static void TablebaseInitialization();

    Cube();

private:
    // precomputation
    static std::vector<uint16_t> corner_orientations;
    static std::vector<uint16_t> corner_positions;
    static std::vector<uint16_t> corner_heuristics;

    static std::vector<uint16_t> edge_orientations;
    static std::vector<uint32_t> edge_positions;
    // TODO: Edge Heuristc

    static std::vector<std::unordered_set<State>> tablebase;

    State cube_;
};

namespace std {
    template<>
    struct hash<Cube::State> {
        size_t operator()(const Cube::State& s) const {
            size_t h1 = hash<uint16_t>{}(s.corner_orientation);
            size_t h2 = hash<uint16_t>{}(s.corner_position);
            size_t h3 = hash<uint16_t>{}(s.edge_orientation);
            size_t h4 = hash<uint32_t>{}(s.edge_position_1);
            size_t h5 = hash<uint32_t>{}(s.edge_position_2);

            // Combine hashes (standard method)
            size_t seed = h1;
            seed ^= h2 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h3 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h4 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h5 + 0x9e3779b9 + (seed << 6) + (seed >> 2);

            return seed;
        }
    };
}
