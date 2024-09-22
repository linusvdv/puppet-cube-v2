#include <sys/types.h>
#include <bit>
#include <cassert>
#include <cstdint>
#include <vector>

#include "cube.h"
#include "error_handler.h"
#include "settings.h"


std::vector<uint16_t> position_data_table;


// initialize position data
void InitializePositionData (ErrorHandler& error_handler, Setting& settings) {
    position_data_table = std::vector<uint16_t>(kNumPositions, 0);

    // get file location
    std::string legal_move_path = "position_data/corner-data.bin";
    legal_move_path.insert(0, settings.rootPath);

    // read file
    if (std::FILE* file = std::fopen(legal_move_path.c_str(), "rb")) {
        if (std::fread(position_data_table.data(), sizeof(position_data_table[0]), position_data_table.size(), file) != kNumPositions) {
            error_handler.Handle(ErrorHandler::kError, "cube.cpp", "not all positions found in legal_moves.bin file");
        }
        std::fclose(file);
    }
    else {
        error_handler.Handle(ErrorHandler::kCriticalError, "cube.cpp", "legal_moves.bin file not found");
    }

    error_handler.Handle(ErrorHandler::kInfo, "cube.cpp", "position data initialized");
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


// unique hash for every edge combination
uint64_t Cube::GetEdgeHash () {
    // if you already calculated edge hash use the calculated hash
    if (calculated_edge_hash_) {
        return edge_hash_;
    }
    calculated_edge_hash_ = true;

    uint64_t hash = 0;

    // convert edges to 12!/2
    // this is possible because all indices only appear once
    std::array<bool, kNumEdges> accessed;
    accessed.fill(false);

    // position
    // this conversion could ignore the two last edges 
    // for decoding from the position the last two edges are easily stored
    for (unsigned int i = 0; i < kNumEdges - 1; i++) {
        hash *= kNumEdges - i;
        unsigned int edge_index = 0;
        for (int j = 0; j < edges[i].position; j++) {
            edge_index += uint32_t(!accessed[j]);
        }
        accessed[edges[i].position] = true;
        hash += edge_index;
    }

    // orientation has only one bit
    for (unsigned int i = 0; i < kNumEdges - 1; i++) {
        hash <<= 1;
        hash |= edges[i].orientation & 1;
    }

    edge_hash_ = hash;
    return hash;
}


// decode edges
void DecodeEdgesHash (Cube& cube, uint64_t hash) {
    // decode orientation
    for (int i = Cube::kNumEdges-2; i >= 0; i--) {
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


// get from the hash to the cube
Cube DecodeHash (unsigned int corner_hash, uint64_t edge_hash) {
    Cube new_cube;
    DecodeCornerHash(new_cube, corner_hash);
    DecodeEdgesHash(new_cube, edge_hash);
    return new_cube;
}


bool Cube::IsSolved () {
    return GetCornerHash() == 0 && GetEdgeHash() == 0;
}
