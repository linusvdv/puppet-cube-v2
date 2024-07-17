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
    std::array<int, 3> protruding;
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
                          << PrintNegSpace(piece.protruding[0]) << ","
                          << PrintNegSpace(piece.protruding[1]) << ","
                          << PrintNegSpace(piece.protruding[2]) << "}, {"
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


uint64_t GetProtrudingHash(std::array<Piece, kNumPieces>& position) {
    uint64_t hash = 0;
    for (int i = 0; i < kNumPieces; i++) {
        for (int j = 0; j < 3; j++) {
            hash *= 3;
            hash += position[i].protruding[j]+1;
        }
    }
    return hash;
}


std::array<Piece, kNumPieces> DecodePositionHash(uint hash, uint64_t protruding) {
    std::array<Piece, kNumPieces> position = {};
    // orientation back to front
    uint orientation_hash = hash / kEightFac;
    hash %= kEightFac;
    for (int i = kNumPieces-2; i >= 0; i--) {
        position[i].orientation = {int(orientation_hash%3==0),
                                   int(orientation_hash%3==1),
                                   int(orientation_hash%3==2)};
        orientation_hash /= 3;
    }
    // last orientation 
    position[kNumPieces-1].orientation = {-1, -1, -1};


    // position
    std::array<std::array<int, 3>, kNumPieces> corner_value = {{
        { 1,  1,  1},
        {-1,  1,  1},
        { 1, -1,  1},
        { 1,  1, -1},
        {-1,  1, -1},
        {-1, -1,  1},
        { 1, -1, -1},
        {-1, -1, -1},
    }};

    std::array<bool, kNumPieces> found;
    found.fill(false);
    int shift = kEightFac;
    for (int i = 0; i < kNumPieces; i++) {
        shift /= kNumPieces - i;

        int small_corner_index = hash / shift;
        hash %= shift;

        for (int j = 0; j < kNumPieces; j++) {
            if (!found[j]) {
                small_corner_index--;
            }
            if (small_corner_index == -1) {
                position[i].position = corner_value[j];
                found[j] = true;
                break;
            }
        }
    }
    
    // protruding
    for (int i = kNumPieces-1; i >= 0; i--) {
        for (int j = 3-1; j >= 0; j--) {
            position[i].protruding[j] = (protruding%3) - 1;
            protruding /= 3;
        }
    }
    return position;
}


struct NextPosition {
    int depth;
    uint position_hash;
    uint64_t protruding_hash;
};


int main() {
    std::array<Piece, kNumPieces> pieces = {{
        {{ 1,  1,  1}, { 0,  0,  0}, { 1,  0,  0}},     // Yellow, Orange, Blue  
        {{-1,  1,  1}, { 1,  0,  0}, { 1,  0,  0}},     // Yellow, Green,  Orange
        {{ 1, -1,  1}, { 0,  1,  0}, { 1,  0,  0}},     // Orange, White,  Blue  
        {{ 1,  1, -1}, { 0,  0,  1}, { 1,  0,  0}},     // Blue,   Red,    Yellow
        {{-1,  1, -1}, { 1,  0,  1}, { 1,  0,  0}},     // Yellow, Red,    Green 
        {{-1, -1,  1}, { 1,  1,  0}, { 1,  0,  0}},     // Orange, Green,  White 
        {{ 1, -1, -1}, { 0,  1,  1}, { 1,  0,  0}},     // Blue,   White,  Red   
        {{-1, -1, -1}, { 1,  1,  1}, {-1, -1, -1}}      // White,  Green,  Red   
                                                        // last orientation is unimportant because only legal positions are looked at
    }};

    std::array<uint16_t, kNumOrientations> positions = {};

    std::queue<NextPosition> next_positions;
    next_positions.push({0, GetPositionHash(pieces), GetProtrudingHash(pieces)});

    unsigned int position_index = GetPositionHash(pieces);
    positions[position_index] = (1 << kNumRotations)-1; // all moves are legal


    uint64_t num_positions = 1;

    while (!next_positions.empty()) {
        int depth = next_positions.front().depth;
        std::array<Piece, kNumPieces> current_position = DecodePositionHash(next_positions.front().position_hash, next_positions.front().protruding_hash);
        next_positions.pop();

        //DebugPiecesOut(current_position);

        if (num_positions % 100000 == 0) {
            std::cout << depth << " " << num_positions << " " << next_positions.size() << std::endl;
            std::cout << sizeof(*positions.begin())*positions.size()    / 1000000 << " MB "
                      << sizeof(NextPosition)*next_positions.size() / 1000000 << " MB" << std::endl;
        }

        unsigned int legal_moves = 0;

        // go over all legal moves
        for (int rotation = Rotations::kL; rotation <= Rotations::kBc; rotation++) {
            std::array<Piece, kNumPieces> next_position = current_position;
            for (Piece& piece : next_position) {
                if (piece.position[kPieceRotationMatrices[rotation].relevant_index] != kPieceRotationMatrices[rotation].relevant_value) {
                    continue;
                }
                piece.orientation = IntMat3MultWithVec3(kPieceRotationMatrices[rotation].full_rotation, piece.orientation);
                piece.position    = IntMat3MultWithVec3(kPieceRotationMatrices[rotation].full_rotation, piece.position);
                piece.protruding   = IntMat3MultWithVec3(kPieceRotationMatrices[rotation].full_rotation, piece.protruding);
            }
            
            // check if legal

            // add this move to legal moves
            // the opposite turn is always allowed "L == R'"
            if (rotation%2 == 0) {
                legal_moves |= 1 << (rotation/2);
            }

            // position already looked at
            unsigned int position_index = GetPositionHash(next_position);
            if (positions[position_index] != 0) {
                continue;
            }
            positions[position_index] = -1; // mark as to be visited


            next_positions.push({depth+1, GetPositionHash(next_position), GetProtrudingHash(next_position)});
        }

        positions[position_index] = legal_moves | (depth << kNumRotations/2);
        num_positions++;
    }

    std::cout << num_positions << std::endl;
    /*
    for (auto a : positions) {
        std::cout << a << std::endl;
    }
    */
}
