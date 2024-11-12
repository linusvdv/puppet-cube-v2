#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <queue>
#include <string>
#include <vector>


constexpr int kNumRotations = 18;
constexpr int kNumCorners = 8;
// number of positions on a normal 3x3
constexpr int kNumPositions = 88179840; // 8! * 3^7
constexpr int kEightFac = 40320; // 8!
constexpr int kMaxProtruding = 8;

enum Rotations {
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


struct Corner {
    // where is the piece located
    // index - position in 3D space (x y z)
    // 0 -  1  1  1
    // 1 - -1  1  1
    // 2 -  1 -1  1
    // 3 - -1 -1  1
    // ...
    // 7 - -1 -1 -1
    uint8_t position;

    // in which direction is the piece bigger than the edge pieces
    // axis where they are longer
    // 0th bit - x direction
    // 1st bit - y direction
    // 2nd bit - z direction
    uint8_t protruding;

    // distinguish the different orientations
    // 0th bit - x direction
    // 1st bit - y direction
    // 2nd bit - z direction
    uint8_t orientation = 0;
};


// map the current position to next position
// -1 marks no change in rotation direction
constexpr std::array<std::array<int8_t, kNumCorners>, kNumRotations> kCornerRotation =
{{
    { 4, -1,  0, -1,  6, -1,  2, -1}, // R
    { 2, -1,  6, -1,  0, -1,  4, -1}, // R'
    {-1,  3, -1,  7, -1,  1, -1,  5}, // L
    {-1,  5, -1,  1, -1,  7, -1,  3}, // L'
    { 1,  5, -1, -1,  0,  4, -1, -1}, // U
    { 4,  0, -1, -1,  5,  1, -1, -1}, // U'
    {-1, -1,  6,  2, -1, -1,  7,  3}, // D
    {-1, -1,  3,  7, -1, -1,  2,  6}, // D'
    { 2,  0,  3,  1, -1, -1, -1, -1}, // F
    { 1,  3,  0,  2, -1, -1, -1, -1}, // F'
    {-1, -1, -1, -1,  5,  7,  4,  6}, // B
    {-1, -1, -1, -1,  6,  4,  7,  5}, // B'
    { 4,  5,  0,  1,  6,  7,  2,  3}, // M  -  R  + L'
    { 2,  3,  6,  7,  0,  1,  4,  5}, // M' -  R' + L 
    { 1,  5,  3,  7,  0,  4,  2,  6}, // E  -  U  + D'
    { 4,  0,  6,  2,  5,  1,  7,  3}, // E' -  U' + D
    { 1,  3,  0,  2,  5,  7,  4,  6}, // S  -  F' + B
    { 2,  0,  3,  1,  6,  4,  7,  5}, // S' -  F  + B'
}};


// swap bit shift_1 with bit shift_2
template <size_t shift_1, size_t shift_2>
uint8_t SwapBits (uint8_t bits) {
    return bits ^ ((((bits >> shift_1) ^ (bits >> shift_2)) & 1) * ((1 << shift_1) | (1 << shift_2)));
}


// rotate the corners
std::array<Corner, kNumCorners> Rotate (std::array<Corner, kNumCorners> corners, Rotations rotation) {
    for (Corner& corner : corners) {
        // chage the position of the corner
        if (kCornerRotation[rotation][corner.position] == -1) {
            continue;
        }
        corner.position = kCornerRotation[rotation][corner.position];

        // rotate the protruding pieces and their orientation
        switch (rotation) {
            case kR:
            case kRc:
            case kL:
            case kLc:
            case kM:
            case kMc:
                corner.protruding = SwapBits<1, 2>(corner.protruding);
                corner.orientation = SwapBits<1, 2>(corner.orientation);
                break;
            case kU:
            case kUc:
            case kD:
            case kDc:
            case kE:
            case kEc:
                corner.protruding = SwapBits<0, 2>(corner.protruding);
                corner.orientation = SwapBits<0, 2>(corner.orientation);
                break;
            case kF:
            case kFc:
            case kB:
            case kBc:
            case kS:
            case kSc:
                corner.protruding = SwapBits<0, 1>(corner.protruding);
                corner.orientation = SwapBits<0, 1>(corner.orientation);
                break;
        }
    }
    return corners;
};


// get a unique hash for all legal corner positions and orientations
unsigned int GetPositionHash (std::array<Corner, kNumCorners>& corners) {
    unsigned int hash = 0;
    unsigned int orientation_hash = 0;

    // convert positions from 8^8 to 8!
    // this is possible because all indices only appear once
    std::array<bool, kNumCorners> accessed;
    accessed.fill(false);

    for (int i = 0; i < kNumCorners - 1; i++) {
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
    assert(hash < kNumPositions);
    return hash;
}


// convert the different protruding corners to one int
unsigned int GetProtrudingHash (std::array<Corner, kNumCorners>& corners) {
    unsigned int hash = 0;
    for (Corner corner : corners) {
        hash *= kMaxProtruding;
        hash += corner.protruding;
    }
    return hash;
}


// decode the position hash to corners
std::array<Corner, kNumCorners> DecodePositionHash (unsigned int hash, unsigned int protruding_hash) {
    std::array<Corner, kNumCorners> corners;

    // decode orientation
    unsigned int orientation_hash = hash / kEightFac;
    for (int i = kNumCorners - 2; i >= 0; i--) {
        corners[i].orientation = 1 << (orientation_hash % 3);
        orientation_hash /= 3;
    }

    // decode position
    unsigned int position_hash = hash % kEightFac;
    std::array<bool, kNumCorners> accessed;
    accessed.fill(false);
    int shift = kEightFac;
    for (int i = 0; i < kNumCorners; i++) {
        shift /= kNumCorners - i;

        int corner_index = position_hash / shift;
        position_hash %= shift;

        for (int j = 0; j < kNumCorners; j++) {
            if (!accessed[j]) {
                corner_index--;
            }
            if (corner_index == -1) {
                corners[i].position = j;
                accessed[j] = true;
                break;
            }
        }
    }

    // decode protruding
    for (int i = kNumCorners - 1; i >= 0; i--) {
        corners[i].protruding = protruding_hash % kMaxProtruding;
        protruding_hash /= kMaxProtruding;
    }

    return corners;
}


constexpr int kSizeLegalMap = 256;
std::array<bool, kSizeLegalMap> legal_map;


// pre initialise legal map
void LegalMapInitialisation () {
    for (int i = 0; i < kSizeLegalMap; i++) {
        if ((!bool(i >> 0 & 1) && !bool(i >> 4 & 1)) ||   // x
            (!bool(i >> 2 & 1) && !bool(i >> 6 & 1)) ||   // x
            (!bool(i >> 1 & 1) && !bool(i >> 3 & 1)) ||   // y
            (!bool(i >> 5 & 1) && !bool(i >> 7 & 1)) ||   // y
            (!bool(i >> 0 & 1) && !bool(i >> 1 & 1) &&    // diagonal
             !bool(i >> 6 & 1) && !bool(i >> 7 & 1)) ||
            (!bool(i >> 2 & 1) && !bool(i >> 3 & 1) &&    // diagonal
             !bool(i >> 4 & 1) && !bool(i >> 5 & 1))) {
            legal_map[i] = false;
        }
        else {
            legal_map[i] = true;
        }
    }
}


// convert four protruding pieces to a hash which can be looked at
int LegalHash (std::array<uint8_t, 4>& protruding_pieces, int idx) {
    int hash = 0;
    for (uint8_t protruding_piece : protruding_pieces) {
        for (int i = 1; i <= 2; i++) {
            hash *= 2;
            hash += (protruding_piece >> ((idx+i)%3)) & 1;
        }
    }
    return hash;
}


bool IsLegal (std::array<Corner, kNumCorners>& corners) {
    // go over all directions
    for (int i = 0; i < 3; i++) {
        // positive or negative
        for (int j = 0; j < 2; j++) {
            // get protruding pieces
            std::array<uint8_t, 4> protruding_pieces;
            protruding_pieces.fill(kMaxProtruding-1);
            for (Corner corner : corners) {
                if ((corner.position >> i & 1) == j && (corner.protruding >> i & 1) == 1) {
                    int index = (corner.position >> ((i+1)%3) & 1) + (corner.position >> ((i+2)%3) & 1)*2;
                    protruding_pieces[index] = corner.protruding;
                }
            }
            if (!legal_map[LegalHash(protruding_pieces, i)]) {
                return false;
            }
        }
    }
    return true;
}


struct NextPosition {
    uint8_t depth;
    unsigned int hash;
    unsigned int protruding_hash;
};


const int kDpDepth = 27;


void GetNumberPossibilities(std::vector<uint16_t>& positions) {
    std::vector<uint64_t> possibilities(kNumPositions, 0);
    // starting position
    possibilities[0] = 1;

    for (int dp_depth = 1; dp_depth <= kDpDepth; dp_depth++) {
        for (int position = 0; position < kNumPositions; position++) {
            // check if position is dp_depth
            if ((positions[position]>>6) == dp_depth) {

                std::array<Corner, kNumCorners> current_position = DecodePositionHash(position, 0); // unnecessary protruding hash
                for (int rotation = Rotations::kR; rotation <= Rotations::kBc; rotation++) {
                    // legal moves
                    unsigned int legal_moves = uint16_t(positions[position]<<(16-6))>>(16-6);
                    // the opposite turn is always allowed "R == L"
                    if (rotation%4 <= 1 && rotation <= Rotations::kBc) {
                        if ((legal_moves >> (rotation/2+rotation%4) & 1) == 0) {
                            continue;
                        }
                    }
                    else if (rotation <= Rotations::kBc) {
                        // check the previous IsLegal L is the same as R
                        if ((legal_moves >> (rotation/2+rotation%4-3) & 1) == 0) {
                            continue;
                        }
                    }

                    std::array<Corner, kNumCorners> next_position = Rotate(current_position, Rotations(rotation));

                    if ((positions[GetPositionHash(next_position)]>>6)==dp_depth-1) {
                        possibilities[position] += possibilities[GetPositionHash(next_position)];
                    }
                }

                // std::cout << dp_depth << ": " << possibilities[position] << std::endl;
            }
        }
    }
}


void Statistic(std::vector<uint16_t>& positions, uint64_t num_positions) {
    // interesting output
    std::cout << "Number of positions: " << num_positions << std::endl;

    uint64_t total_num_edges = 0;
    uint64_t max_heuristic = 0;
    std::vector<uint64_t> distribution(28, 0);
    std::vector<uint64_t> distribution_edges(28, 0);
    for (uint16_t position : positions) {
        if (position != 0) {
            total_num_edges += std::popcount(uint16_t(position << (16-6)));
            max_heuristic = std::max(uint64_t(position >> 6), max_heuristic);
            distribution[position>>6]++;
            distribution_edges[position>>6] += std::popcount(uint16_t(position << (16-6)));
        }
    }

    std::cout << "Total number of edges: " << (total_num_edges+3*num_positions) << "*490497638400 = 34024400694647193600" << std::endl;
    std::cout << "Average Number of legal moves: " << double(total_num_edges*2)/num_positions+6 << std::endl;
    std::cout << "Max heuristic: " << max_heuristic << std::endl;
    for (uint64_t i = 0; i <= max_heuristic; i++) {
        std::cout << i << ": " << distribution[i] << " " << double(distribution_edges[i]*2)/distribution[i]+6 << std::endl;
    }

    // GetNumberPossibilities(positions);
}


int main (int argc, char *argv[]) {
    // initialise the map of legal positions
    LegalMapInitialisation();

    // all possible position reacable with a normal 3x3
    std::vector<uint16_t> positions(kNumPositions, 0);

    // BFS
    std::queue<NextPosition> next_positions;
    // hash of solved position 0
    const unsigned int start_protruding = 0b000'001'010'011'100'101'110'111;
    next_positions.push({0, 0, start_protruding});

    positions[0] = -1; // mark as to be visited

    uint64_t num_positions = 0;

    while (!next_positions.empty()) {
        // get new position
        uint8_t depth = next_positions.front().depth;
        std::array<Corner, kNumCorners> current_position =
            DecodePositionHash(next_positions.front().hash, next_positions.front().protruding_hash);
        next_positions.pop();

        // get some information
        if (num_positions % 100000 == 0) {
            std::cout << uint32_t(depth) << " " << num_positions << " " << next_positions.size() << std::endl;
            std::cout << sizeof(uint16_t) * positions.size() / 1000000 << " MB "
                      << sizeof(NextPosition) * next_positions.size() / 1000000 << " MB" << std::endl;
        }

        unsigned int legal_moves = 0;

        // go over all legal moves
        for (int rotation = Rotations::kR; rotation <= Rotations::kBc; rotation++) {
            std::array<Corner, kNumCorners> next_position = Rotate(current_position, Rotations(rotation));

            // the opposite turn is always allowed "R == L"
            if (rotation%4 <= 1 && rotation <= Rotations::kBc) {
                if (!IsLegal(next_position)) {
                    //continue;
                }
                // check if the rotation is legal
                // add this move to legal moves
                legal_moves |= 1 << (rotation/2+rotation%4);
            }
            else if (rotation <= Rotations::kBc) {
                // check the previous IsLegal L is the same as R
                if ((legal_moves >> (rotation/2+rotation%4-3) & 1) == 0) {
                    continue;
                }
            }

            // looked at already visited positions
            unsigned int position_index = GetPositionHash(next_position);
            if (positions[position_index] != 0) {
                continue;
            }
            positions[position_index] = -1; // mark as to be visited

            // add to queue
            next_positions.push({uint8_t(depth+1), position_index, GetProtrudingHash(next_position)});
        }

        // write position data to the list
        unsigned int position_index = GetPositionHash(current_position);
        positions[position_index] = legal_moves | (depth << (12)/2);
        num_positions++;
    }

    if (argc > 1 && std::string(argv[1]) == "--statistic") {
        Statistic(positions, num_positions);
    }

    // write to file
    if (std::FILE* file = std::fopen("corner-data.bin", "wb")) {
        std::fwrite(positions.data(), sizeof(positions[0]), positions.size(), file);
        std::fclose(file);
    }
}
