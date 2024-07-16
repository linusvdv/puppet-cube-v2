#include <sys/types.h>
#include <array>
#include <cassert>
#include <cstdint>
#include <map>
#include <iostream>
#include <queue>
#include <string>


struct Piece {
    // direction away from the origin
    std::array<int, 3> position;
    // directions where the piece is bigger than the edge pieces
    std::array<int, 3> protuding;
    // distinguish the different orientations
    std::array<int, 3> orientation;
};


// without slice moves
const int kNumRotations = 12;
enum Rotations {
    kL,
    kLc,
    kR,
    kRc,
    kU,
    kUc,
    kD,
    kDc,
    kF,
    kFc,
    kB,
    kBc,
};


struct PieceRotationMatrix {
    // to check which are effected by current_position rotation
    int relevant_index;
    int relevant_value;

    std::array<std::array<int, 3>, 3> full_rotation;
};


const std::array<PieceRotationMatrix, kNumRotations> kPieceRotationMatrices = {{
    // L
    {0, -1, {{{ 1, 0, 0}, { 0, 0,-1}, { 0,  1, 0}}}},
    // L'
    {0, -1, {{{ 1, 0, 0}, { 0, 0, 1}, { 0, -1, 0}}}},
    // R
    {0,  1, {{{ 1, 0, 0}, { 0, 0, 1}, { 0, -1, 0}}}},
    // R'
    {0,  1, {{{ 1, 0, 0}, { 0, 0,-1}, { 0,  1, 0}}}},

    // U
    {1,  1, {{{ 0, 0,-1}, { 0, 1, 0}, { 1,  0, 0}}}},
    // U'
    {1,  1, {{{ 0, 0, 1}, { 0, 1, 0}, {-1,  0, 0}}}},
    // D
    {1, -1, {{{ 0, 0, 1}, { 0, 1, 0}, {-1,  0, 0}}}},
    // D'
    {1, -1, {{{ 0, 0,-1}, { 0, 1, 0}, { 1,  0, 0}}}},

    // F
    {2,  1, {{{ 0, 1, 0}, {-1, 0, 0}, { 0,  0, 1}}}},
    // F'
    {2,  1, {{{ 0,-1, 0}, { 1, 0, 0}, { 0,  0, 1}}}},
    // B
    {2, -1, {{{ 0,-1, 0}, { 1, 0, 0}, { 0,  0, 1}}}},
    // B'
    {2, -1, {{{ 0, 1, 0}, {-1, 0, 0}, { 0,  0, 1}}}},
}};


const int kNumPieces = 8;


struct PositionValue {
    bool found = false;
    int shortest_solution = 0;
    std::array<bool, kNumRotations> legal_move = {};
};


std::array<int, 3> IntMat3MultWithVec3(const std::array<std::array<int, 3>, 3>& mat3, const std::array<int, 3>& vec3) {
    return {{mat3[0][0]*vec3[0] + mat3[0][1]*vec3[1] + mat3[0][2]*vec3[2],
             mat3[1][0]*vec3[0] + mat3[1][1]*vec3[1] + mat3[1][2]*vec3[2],
             mat3[2][0]*vec3[0] + mat3[2][1]*vec3[1] + mat3[2][2]*vec3[2]
            }};
}

std::string PrintNegSpace(int num) {
    if (num >= 0) {
        return " " + std::to_string(num);
    }
    return std::to_string(num);
}

void DebugPiecesOut (std::array<Piece, kNumPieces> pieces) {
    for (Piece piece : pieces) {
        std::cout << "{{" << PrintNegSpace(piece.position[0]) << ","
                          << PrintNegSpace(piece.position[1]) << ","
                          << PrintNegSpace(piece.position[2]) << "}, {"
                          << PrintNegSpace(piece.protuding[0]) << ","
                          << PrintNegSpace(piece.protuding[1]) << ","
                          << PrintNegSpace(piece.protuding[2]) << "}, {"
                          << PrintNegSpace(piece.orientation[0]) << ","
                          << PrintNegSpace(piece.orientation[1]) << ","
                          << PrintNegSpace(piece.orientation[2]) << "}}," << std::endl;
    }
    std::cout << std::endl;
}


const unsigned int kEightFac = 40320; // 8!
const unsigned int kNumOrientations = 88179840; // 8! * 3^7


unsigned int GetPositionHash (std::array<Piece, kNumPieces>& position) {
    unsigned int hash = 0;
    const std::map<std::array<int, 3>, unsigned int> corner_index = {
        {{ 1,  1,  1}, 0},
        {{-1,  1,  1}, 1},
        {{ 1, -1,  1}, 2},
        {{ 1,  1, -1}, 3},
        {{-1,  1, -1}, 4},
        {{-1, -1,  1}, 5},
        {{ 1, -1, -1}, 6},
        {{-1, -1, -1}, 7}
    };

    std::array<bool, kNumPieces> used;
    used.fill(false);

    unsigned int orientation_hash = 0;
    for (int i = 0; i < kNumPieces-1; i++) {
        // index compressed to 8!
        // 0 to 8!-1
        unsigned int corner = corner_index.find(position[i].position)->second;
        unsigned int small_corner_index = 0;
        for (int j = 0; j < corner; j++) {
            small_corner_index += u_int(!used[j]);
        }
        used[corner] = true;
        hash *= kNumPieces-i;
        hash += small_corner_index;

        // piece orientation
        int orientation = 0;
        for (; orientation < 3; orientation++) {
            if (position[i].orientation[orientation] != 0) {
                break;
            }
        }
        orientation_hash *= 3;
        orientation_hash += orientation;
    }

    hash += orientation_hash * kEightFac;

    assert(hash < kNumOrientations);
    return hash;
}


int main() {
    std::array<Piece, kNumPieces> pieces = {{
        {{ 1,  1,  1}, { 0,  0,  0}, { 1,  0,  0}},     // Yellow, Orange, Blue  
        {{-1,  1,  1}, { 1,  0,  0}, { 1,  0,  0}},     // Yellow, Green,  Orange
        {{ 1, -1,  1}, { 0,  1,  0}, { 1,  0,  0}},     // Orange, White,  Blue  
        {{ 1,  1, -1}, { 0,  0,  1}, { 1,  0,  0}},     // Blue,   Red,    Yellow
        {{-1,  1, -1}, { 1,  0,  1}, { 1,  0,  0}},     // Yellow, Red,    Green 
        {{-1, -1,  1}, { 1,  1,  0}, { 1,  0,  0}},     // Orange, Green,  White 
        {{ 1, -1, -1}, { 0,  1,  1}, { 1,  0,  0}},     // Blue,   White,  Red   
        {{-1, -1, -1}, { 1,  1,  1}, { 1,  0,  0}}      // White,  Green,  Red   
    }};

    std::array<PositionValue, kNumOrientations> positions = {};

    std::queue<std::pair<int, std::array<Piece, kNumPieces>>> next_positions;
    next_positions.push({0, pieces});

    unsigned int position_index = GetPositionHash(pieces);
    positions[position_index] = {true, 0, {}};


    uint64_t num_positions = 1;

    while (!next_positions.empty()) {
        int depth = next_positions.front().first;
        std::array<Piece, kNumPieces> current_position = next_positions.front().second;
        next_positions.pop();
//        DebugPiecesOut(current_position);

        if (num_positions % 100000 == 0) {
            std::cout << depth << " " << positions.size() << " " << next_positions.size() << std::endl;
            std::cout << sizeof(*positions.begin())*positions.size()    / 1000000 << " MB "
                      << sizeof(current_position)*next_positions.size() / 1000000 << " MB" << std::endl;
        }

        // go over all legal moves
        for (int rotation = Rotations::kL; rotation <= Rotations::kBc; rotation++) {
            std::array<Piece, kNumPieces> next_position = current_position;
            for (Piece& piece : next_position) {
                if (piece.position[kPieceRotationMatrices[rotation].relevant_index] != kPieceRotationMatrices[rotation].relevant_value) {
                    continue;
                }
                piece.orientation = IntMat3MultWithVec3(kPieceRotationMatrices[rotation].full_rotation, piece.orientation);
                piece.position    = IntMat3MultWithVec3(kPieceRotationMatrices[rotation].full_rotation, piece.position);
                piece.protuding   = IntMat3MultWithVec3(kPieceRotationMatrices[rotation].full_rotation, piece.protuding);
            }
            
            // check if legal

            // position already looked at
            unsigned int position_index = GetPositionHash(next_position);
            if (positions[position_index].found) {
                continue;
            }
            positions[position_index] = {true, depth+1, {}};
            num_positions++;


            next_positions.push({depth+1, next_position});
        }
    }

    std::cout << num_positions << std::endl;
}
