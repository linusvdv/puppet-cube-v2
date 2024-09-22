// caluclates the heuristic function for 6 edge pieces
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <queue>
#include <vector>


const int kNumPieces = 6;  // only edges that are used for the heuristic funtion
const int kNumEdges = 12;  // all edges
const int kNumRotations = 18;
const int kNumPositions = 42577920; // fac(12) / fac(6) * 2^6


enum Rotations : unsigned int {
    kR, kRc,
    kL, kLc,

    kU, kUc,
    kD, kDc,

    kF, kFc,
    kB, kBc,
// slice moves
    kM, kMc,
    kE, kEc,
    kS, kSc
};


struct Piece {
    // where is the piece located
    // index - position in 3D space (x y z)
    // 0 to 11
    uint8_t position;

    // distinguish the different orientations
    bool orientation = false;
};


// map the current position to next position
// -1 marks no change in rotation direction
constexpr std::array<std::array<int8_t, kNumEdges>, kNumRotations> kEdgeRotation =
{{
    { 2,  0,  3,  1, -1, -1, -1, -1, -1, -1, -1, -1}, // R
    { 1,  3,  0,  2, -1, -1, -1, -1, -1, -1, -1, -1}, // R'
    {-1, -1, -1, -1, -1, -1, -1, -1,  9, 11,  8, 10}, // L
    {-1, -1, -1, -1, -1, -1, -1, -1, 10,  8, 11,  9}, // L'
    { 4, -1, -1, -1,  8,  0, -1, -1,  5, -1, -1, -1}, // U
    { 5, -1, -1, -1,  0,  8, -1, -1,  4, -1, -1, -1}, // U'
    {-1, -1, -1,  7, -1, -1,  3, 11, -1, -1, -1,  6}, // D
    {-1, -1, -1,  6, -1, -1, 11,  3, -1, -1, -1,  7}, // D'
    {-1,  6, -1, -1,  1, -1,  9, -1, -1,  4, -1, -1}, // F
    {-1,  4, -1, -1,  9, -1,  1, -1, -1,  6, -1, -1}, // F'
    {-1, -1,  5, -1, -1, 10, -1,  2, -1, -1,  7, -1}, // B
    {-1, -1,  7, -1, -1,  2, -1, 10, -1, -1,  5, -1}, // B'
    { 2,  0,  3,  1, -1, -1, -1, -1, 10,  8, 11,  9}, // M  -  R  + L'
    { 1,  3,  0,  2, -1, -1, -1, -1,  9, 11,  8, 10}, // M' -  R' + L
    { 4, -1, -1,  6,  8,  0, 11,  3,  5, -1, -1,  7}, // E  -  U  + D'
    { 5, -1, -1,  7,  0,  8,  3, 11,  4, -1, -1,  6}, // E' -  U' + D
    {-1,  4,  5, -1,  9, 10,  1,  2, -1,  6,  7, -1}, // S  -  F' + B
    {-1,  6,  7, -1,  1,  2,  9, 10, -1,  4,  5, -1}, // S' -  F  + B'
}};


std::vector<Piece> Rotate (std::vector<Piece>& edges, Rotations rotation) {
    // edges 
    std::vector<Piece> rotated_edges(kNumPieces);
    for (unsigned int i = 0; i < kNumPieces; i++) {
        // chage the position of the corner
        if (kEdgeRotation[rotation][edges[i].position] == -1) {
            rotated_edges[i] = edges[i];
            continue;
        }
        rotated_edges[i].position = kEdgeRotation[rotation][edges[i].position];
        rotated_edges[i].orientation = !edges[i].orientation;
    }
    return rotated_edges;
}


// unique hash for every edge combination
uint64_t GetEdgeHash (std::vector<Piece>& edges) {
    uint64_t hash = 0;

    // convert edges to 12!/2
    // this is possible because all indices only appear once
    std::array<bool, kNumEdges> accessed;
    accessed.fill(false);

    // position
    for (unsigned int i = 0; i < kNumPieces; i++) {
        hash *= kNumEdges - i;
        unsigned int edge_index = 0;
        for (int j = 0; j < edges[i].position; j++) {
            edge_index += uint32_t(!accessed[j]);
        }
        accessed[edges[i].position] = true;
        hash += edge_index;
    }

    // orientation has only one bit
    for (unsigned int i = 0; i < kNumPieces; i++) {
        hash <<= 1;
        hash |= uint64_t(edges[i].orientation);
    }

    assert(hash < kNumPositions);
    return hash;
}


struct NextEdge {
    std::vector<Piece> edges;
    uint8_t depth;
};


int main () {
    std::vector<Piece> start_edges;
    for (uint8_t i = 0; i < kNumPieces; i++) {
        start_edges.push_back({i});
    }

    uint64_t num_positions = 0;
    std::vector<uint8_t> edge_position(kNumPositions, uint8_t(-1));
    std::queue<NextEdge> next_edges;
    next_edges.push({start_edges, 0});
    edge_position[0] = 0;

    while (!next_edges.empty()) {
        std::vector<Piece> edges = next_edges.front().edges;
        uint8_t depth = next_edges.front().depth;
        next_edges.pop();
        num_positions++;

        for (unsigned int rotation = kR; rotation <= kSc; rotation++) {
            std::vector<Piece> next_edge = Rotate(edges, Rotations(rotation));
            int next_hash = GetEdgeHash(next_edge);
            if (edge_position[next_hash] == uint8_t(-1)) {
                edge_position[next_hash] = uint8_t(-2);
                next_edges.push({next_edge, uint8_t(depth+1)});
            }
        }
        int current_hash = GetEdgeHash(edges);
        edge_position[current_hash] = depth;
        if (num_positions%100000 == 0) {
            std::cout << num_positions << " " << int(depth) << std::endl;
        }
    }
    std::cout << num_positions << std::endl;

    // write to file
    if (std::FILE* file = std::fopen("edge-data.bin", "wb")) {
        std::fwrite(edge_position.data(), sizeof(edge_position[0]), edge_position.size(), file);
        std::fclose(file);
    }
}
