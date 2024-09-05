#include <sys/types.h>
#include <bit>
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
    std::string legal_move_path = "legal-move-generation/legal_moves.bin";
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
        unsigned int position_hash = GetPositionHash();
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


unsigned int Cube::GetPositionHash () {
    // if you already calculated position hash use the calculated position
    if (calculated_position_hash_) {
        return position_hash_;
    }
    calculated_position_hash_ = true;

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
    position_hash_ = hash;
    return hash;
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
    for (unsigned int i = 0; i < kNumEdges - 2; i++) {
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

    return hash;
}


bool Cube::IsSolved () {
    return GetPositionHash() == 0 && GetEdgeHash() == 0;
}
