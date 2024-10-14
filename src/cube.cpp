#include <sys/types.h>
#include <bit>
#include <cassert>
#include <cstdint>
#include <vector>

#include "cube.h"
#include "error_handler.h"
#include "settings.h"


std::vector<uint16_t> position_data_table;
std::vector<uint8_t> edge_data_table;


// initialize position data
void InitializePositionData (ErrorHandler& error_handler, Setting& settings) {
    position_data_table = std::vector<uint16_t>(kNumPositions, 0);

    // get file location
    std::string corner_data_path = "position_data/corner-data.bin";
    corner_data_path.insert(0, settings.rootPath);

    // read file
    if (std::FILE* file = std::fopen(corner_data_path.c_str(), "rb")) {
        if (std::fread(position_data_table.data(), sizeof(position_data_table[0]), position_data_table.size(), file) != kNumPositions) {
            error_handler.Handle(ErrorHandler::kError, "cube.cpp", "not all positions found in corner-data.bin file");
        }
        std::fclose(file);
    }
    else {
        error_handler.Handle(ErrorHandler::kCriticalError, "cube.cpp", "corner-data.bin file not found");
    }

    error_handler.Handle(ErrorHandler::kInfo, "cube.cpp", "corner data initialized");
}


// initialize position data
void InitializeEdgeData (ErrorHandler& error_handler, Setting& settings) {
    edge_data_table = std::vector<uint8_t>(kNumEdgePositions, 0);

    // get file location
    std::string edge_data_path = "position_data/edge-data.bin";
    edge_data_path.insert(0, settings.rootPath);

    // read file
    if (std::FILE* file = std::fopen(edge_data_path.c_str(), "rb")) {
        if (std::fread(edge_data_table.data(), sizeof(edge_data_table[0]), edge_data_table.size(), file) != kNumEdgePositions) {
            error_handler.Handle(ErrorHandler::kError, "cube.cpp", "not all positions found in edge-data.bin file");
        }
        std::fclose(file);
    }
    else {
        error_handler.Handle(ErrorHandler::kCriticalError, "cube.cpp", "edge-data.bin file not found");
    }

    error_handler.Handle(ErrorHandler::kInfo, "cube.cpp", "edge data initialized");
}


uint16_t Cube::GetPositionData () {
    // get position hash and legal_move_data
    if (!got_position_data) {
        unsigned int position_hash = GetCornerHash();
        position_data = position_data_table[position_hash];
        got_position_data= true;
    }
    return position_data;
}


Cube::Cube () {
    for (unsigned int i = 0; i < kNumCorners; i++) {
        corners[i].position = i;
        corners[i].orientation = 1;
    }
    for (unsigned int i = 0; i < kNumEdges; i++) {
        edges[i].position = i;
        edges[i].orientation = 0;
    }
}


unsigned int Cube::GetCornerHash () {
    // if you already calculated position hash use the calculated position
    if (calculated_corner_hash_) {
        return corner_hash_;
    }
    calculated_corner_hash_ = true;

    unsigned int hash = 0;
    unsigned int orientation_hash = 0;

    // convert positions from 8^8 to 8!
    // this is possible because all indices only appear once
    std::array<bool, kNumCorners> accessed;
    accessed.fill(false);

    for (unsigned int i = 0; i < kNumCorners - 1; i++) {
        // position
        hash *= kNumCorners - i;
        unsigned int corner_index = 0;
        for (int j = 0; j < corners[i].position; j++) {
            corner_index += uint32_t(!accessed[j]);
        }
        accessed[corners[i].position] = true;
        hash += corner_index;

        // orientation
        orientation_hash *= 3;
        orientation_hash += std::countr_zero(corners[i].orientation);
    }

    hash += orientation_hash * kEightFac;
    corner_hash_ = hash;
    return hash;
}


// decode corners
void DecodeCornerHash (Cube& cube, unsigned int hash) {
    // decode orientation
    unsigned int orientation_hash = hash / kEightFac;
    for (int i = Cube::kNumCorners - 2; i >= 0; i--) {
        cube.corners[i].orientation = 1 << (orientation_hash % 3);
        orientation_hash /= 3;
    }

    // decode position
    unsigned int position_hash = hash % kEightFac;
    std::array<bool, Cube::kNumCorners> accessed;
    accessed.fill(false);
    int shift = kEightFac;
    for (unsigned int i = 0; i < Cube::kNumCorners; i++) {
        shift /= Cube::kNumCorners - i;

        int corner_index = position_hash / shift;
        position_hash %= shift;

        for (unsigned int j = 0; j < Cube::kNumCorners; j++) {
            if (!accessed[j]) {
                corner_index--;
            }
            if (corner_index == -1) {
                cube.corners[i].position = j;
                accessed[j] = true;
                break;
            }
        }
    }
}


Cube::Hash Cube::GetHash () {
    if (calculated_hash_) {
        return hash_;
    }
    calculated_hash_ = true;

    unsigned int corner_hash = GetCornerHash();
    uint64_t edge_hash = GetEdgeHash();
    hash_.hash_1 = corner_hash;
    hash_.hash_2 = uint8_t(edge_hash);
    hash_.hash_1 |= (edge_hash >> 8) << 28; // NOLINT
    return hash_;
}


// unique hash for every edge combination
uint64_t Cube::GetEdgeHash () {
    // if you already calculated edge hash use the calculated hash
    if (calculated_edge_hash_) {
        return edge_hash_;
    }
    calculated_edge_hash_ = true;

    uint64_t hash = 0;

    // convert edges to 12!
    std::array<bool, kNumEdges> accessed;
    accessed.fill(false);

    // position
    // this conversion could ignore the two last edges 
    // for decoding from the position the last two edges are easily stored
    for (unsigned int i = 0; i < kNumEdges; i++) {
        hash *= kNumEdges - i;
        unsigned int edge_index = 0;
        for (int j = 0; j < edges[i].position; j++) {
            edge_index += uint32_t(!accessed[j]);
        }
        accessed[edges[i].position] = true;
        hash += edge_index;
    }

    // orientation has only one bit
    for (unsigned int i = 0; i < kNumEdges; i++) {
        hash <<= 1;
        hash |= edges[i].orientation & 1;
    }

    edge_hash_ = hash;
    return hash;
}


// decode edges
void DecodeEdgesHash (Cube& cube, uint64_t hash) {
    // decode orientation
    for (int i = Cube::kNumEdges-1; i >= 0; i--) {
        cube.edges[i].orientation = hash & 1;
        hash >>= 1;
    }

    // decode position
    std::array<bool, Cube::kNumEdges> accessed;
    accessed.fill(false);


    const int k_twelve_fac = 479001600;
    assert(hash < k_twelve_fac);
    int shift = k_twelve_fac;
    for (unsigned int i = 0; i < Cube::kNumEdges; i++) {
        shift /= Cube::kNumEdges - i;

        int edge_index = hash / shift;
        hash %= shift;

        for (unsigned int j = 0; j < Cube::kNumEdges; j++) {
            if (!accessed[j]) {
                edge_index--;
            }
            if (edge_index == -1) {
                cube.edges[i].position = j;
                accessed[j] = true;
                break;
            }
        }
    }
}


uint8_t Cube::GetEdgeHeuristic1 () {
    if (calculated_edge_heuristic1_) {
        return edge_heuristic1_;
    }
    calculated_edge_heuristic1_ = true;

    // this is for the first half - 6 pieces
    uint64_t hash = 0;
    static const int kNumPieces = 6; // only half of the pieces are important

    // convert edges to 12!/6!*2^6
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

    edge_heuristic1_ = edge_data_table[hash];
    return edge_heuristic1_;
}


uint8_t Cube::GetEdgeHeuristic2 () {
    if (calculated_edge_heuristic2_) {
        return edge_heuristic2_;
    }
    calculated_edge_heuristic2_ = true;

    static const int kNumPieces = 6; // only half of the pieces are important
    //
    // second half
    uint64_t hash = 0;
    std::array<bool, kNumEdges> accessed;
    accessed.fill(false);

    // position
    for (unsigned int i = kNumEdges-1; i >= kNumPieces; i--) {
        hash *= i + 1;
        unsigned int edge_index = 0;
        for (int j = 0; j < edges[i].position; j++) {
            edge_index += uint32_t(!accessed[j]);
        }
        accessed[edges[i].position] = true;
        hash += i - edge_index;
    }

    // orientation has only one bit
    for (unsigned int i = kNumEdges-1; i >= kNumPieces; i--) {
        hash <<= 1;
        hash |= uint64_t(edges[i].orientation);
    }

    // NOTE: this adds the two functions together
    edge_heuristic2_ = edge_data_table[hash];
    return edge_heuristic2_;
}


// get from the hash to the cube
Cube DecodeHash (Cube::Hash hash) {
    Cube new_cube;
    DecodeCornerHash(new_cube, (hash.hash_1 << 36) >> 36); // NOLINT
    DecodeEdgesHash(new_cube, ((hash.hash_1 >> 28) << 8) | uint64_t(hash.hash_2)); // NOLINT
    return new_cube;
}


bool Cube::IsSolved () {
    return GetCornerHash() == 0 && GetEdgeHash() == 0;
}
