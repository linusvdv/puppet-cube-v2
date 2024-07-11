#include <array>
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
    int shortest_solution;
    std::array<bool, kNumRotations> legal_move;
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

std::array<std::array<int, 4>, kNumPieces> GetPositionIndex (std::array<Piece, kNumPieces> position) {
    std::array<std::array<int, 4>, kNumPieces> position_index;
    for (int i = 0; i < kNumPieces; i++) {
        int orientation = 0;
        for (; orientation < 3; orientation++) {
            if (position[i].orientation[orientation] != 0) {
                break;
            }
        }
        position_index[i] = {position[i].position[0],
            position[i].position[1],
            position[i].position[2],
            orientation};
    }
    return position_index;
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

    std::map<std::array<std::array<int, 4>, kNumPieces>, PositionValue> positions;
    std::queue<std::pair<int, std::array<Piece, kNumPieces>>> next_positions;
    next_positions.push({0, pieces});

    uint64_t num_positions = 0;

    while (!next_positions.empty()) {
        int depth = next_positions.front().first;
        std::array<Piece, kNumPieces> current_position = next_positions.front().second;
        next_positions.pop();
//        DebugPiecesOut(current_position);

        // get position index from current position
        std::array<std::array<int, 4>, kNumPieces> position_index = GetPositionIndex(current_position);

        // position already looked at
        if (positions.contains(position_index)) {
            continue;
        }
        num_positions++;
        positions[position_index] = {depth+1, {}};
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
            std::array<std::array<int, 4>, kNumPieces> position_index = GetPositionIndex(next_position);
            if (positions.contains(position_index)) {
                continue;
            }


            next_positions.push({depth+1, next_position});
        }
    }

    std::cout << num_positions << std::endl;
}
