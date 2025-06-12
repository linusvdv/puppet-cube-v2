#pragma once
#include <cstdint>
#include <tuple>
#include <vector>
#include <parallel_hashmap/phmap.h>
#include <tbb/concurrent_vector.h>


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
    #pragma pack(push, 1)
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

    using Tablebase = phmap::parallel_flat_hash_set<State,
        phmap::priv::hash_default_hash<State>,
        phmap::priv::hash_default_eq<State>,
        phmap::priv::Allocator<State>,
        12, std::mutex>;


    // corner and edge precomputation
    static void Initialize();
    static void TablebaseInitialization();

    Cube();

private:
    // precomputation
    static std::vector<uint16_t> corner_orientations;
    static std::vector<uint16_t> corner_positions;
    static tbb::concurrent_vector<uint16_t> corner_heuristics;

    static std::vector<uint16_t> edge_orientations;
    static std::vector<uint32_t> edge_positions;
    // TODO: Edge Heuristc

    static std::vector<Tablebase> tablebase;

    State cube_;
};
