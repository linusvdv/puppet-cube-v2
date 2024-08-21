#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "actions.h"
#include "rotation.h"
#include "error_handler.h"
#include "cube.h"


// get all legal rotations
std::vector<Rotations> GetLegalRotations (ErrorHandler& error_handler, Cube& cube) {
    std::vector<Rotations> legal_rotations;

    cube.GetPositionData();

    // make list of legal moves and checking if the moves are allowed
    for (int i = 0; i < kNumRotations; i++) {
        if (i <= int(Rotations::kBc)) {
            if ((i%4 <= 1 && (cube.position_data >> (i/2+i%4) & 1) == 0) ||
                (i%4 > 1 && (cube.position_data >> (i/2+i%4-3) & 1) == 0)) {
                continue;
            }
        }
        legal_rotations.push_back(Rotations(i));
    }

    return legal_rotations;
}


// get a random legal rotation
Rotations GetRandomRotation (ErrorHandler& error_handler, Cube& cube, std::mt19937& rng) {
    std::vector<Rotations> legal_rotation = GetLegalRotations(error_handler, cube);

    // get a uniform distribution
    std::uniform_int_distribution<std::mt19937::result_type> distribution(0, legal_rotation.size()-1);
    return legal_rotation[distribution(rng)];
}


// add num_rotations random legal rotations to actions
void RandomRotations (ErrorHandler& error_handler, Cube& cube, Actions& actions, int num_rotations, std::mt19937& rng) {
    for (int i = 0; i < num_rotations; i++) {
        Rotations random_rotation = GetRandomRotation(error_handler, cube, rng);
        actions.Push(Action(Instructions::kRotation, random_rotation));
        cube = Rotate(cube, random_rotation);
    }
}


// map the current position to next position
// -1 marks no change in rotation direction
constexpr std::array<std::array<int8_t, Cube::kNumCorners>, kNumRotations> kCornerRotation =
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
    {-1, -1, -1, -1,  6,  4,  7,  5}, // B
    {-1, -1, -1, -1, -1, -1, -1, -1}, // M
    {-1, -1, -1, -1, -1, -1, -1, -1}, // M'
    {-1, -1, -1, -1, -1, -1, -1, -1}, // E
    {-1, -1, -1, -1, -1, -1, -1, -1}, // E'
    {-1, -1, -1, -1, -1, -1, -1, -1}, // S
    {-1, -1, -1, -1, -1, -1, -1, -1}  // S'
}};


// map the current position to next position
// -1 marks no change in rotation direction
constexpr std::array<std::array<int8_t, Cube::kNumEdges>, kNumRotations> kEdgeRotation =
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
    {-1, -1,  7, -1, -1,  2, -1, 10, -1, -1,  5, -1}, // B
    {-1, -1, -1, -1,  6,  4,  7,  5, -1, -1, -1, -1}, // M
    {-1, -1, -1, -1,  5,  7,  4,  6, -1, -1, -1, -1}, // M'
    {-1,  2, 10, -1, -1, -1, -1, -1, -1,  1,  9, -1}, // E
    {-1,  9,  1, -1, -1, -1, -1, -1, -1, 10,  2, -1}, // E'
    { 3, -1, -1, 11, -1, -1, -1, -1,  0, -1, -1,  8}, // S
    { 8, -1, -1,  0, -1, -1, -1, -1, 11, -1, -1,  3}  // S'
}};

// map the current position to next position
// -1 marks no change in rotation direction
constexpr std::array<std::array<int8_t, Cube::kNumEdges>, kNumRotations> kCenterRotation =
{{
    {-1,  2,  4,  1,  3, -1}, // M
    {-1,  3,  1,  4,  2, -1}, // M'
    { 3, -1,  0,  5, -1,  2}, // E
    { 2, -1,  5,  0, -1,  3}, // E'
    { 4,  0, -1, -1,  5,  1}, // S
    { 1,  5, -1, -1,  0,  4}, // S'
}};


// swap bit shift_1 with bit shift_2
template <size_t shift_1, size_t shift_2>
uint8_t SwapBits (uint8_t bits) {
    return bits ^ ((((bits >> shift_1) ^ (bits >> shift_2)) & 1) * ((1 << shift_1) | (1 << shift_2)));
}


// rotate the corners
Cube Rotate (const Cube& cube, Rotations rotation) {
    Cube rotated_cube;
    // corners
    for (unsigned int i = 0; i < Cube::kNumCorners; i++) {
        // chage the position of the corner
        if (kCornerRotation[rotation][cube.corners[i].position] == -1) {
            rotated_cube.corners[i] = cube.corners[i];
            continue;
        }
        rotated_cube.corners[i].position = kCornerRotation[rotation][cube.corners[i].position];

        // rotate the protruding pieces and their orientation
        switch (rotation) {
            case kR:
            case kRc:
            case kL:
            case kLc:
                rotated_cube.corners[i].orientation = SwapBits<1, 2>(cube.corners[i].orientation);
                break;
            case kU:
            case kUc:
            case kD:
            case kDc:
                rotated_cube.corners[i].orientation = SwapBits<0, 2>(cube.corners[i].orientation);
                break;
            case kF:
            case kFc:
            case kB:
            case kBc:
                rotated_cube.corners[i].orientation = SwapBits<0, 1>(cube.corners[i].orientation);
                break;
            default:
                break;
        }
    }
    // edges 
    for (unsigned int i = 0; i < Cube::kNumEdges; i++) {
        // chage the position of the corner
        if (kEdgeRotation[rotation][cube.edges[i].position] == -1) {
            rotated_cube.edges[i] = cube.edges[i];
            continue;
        }
        rotated_cube.edges[i].position = kEdgeRotation[rotation][cube.edges[i].position];

        // rotate the protruding pieces and their orientation
        switch (rotation) {
            case kR:
            case kRc:
            case kL:
            case kLc:
            case kM:
            case kMc:
                rotated_cube.edges[i].orientation = SwapBits<1, 2>(cube.edges[i].orientation);
                break;
            case kU:
            case kUc:
            case kD:
            case kDc:
            case kE:
            case kEc:
                rotated_cube.edges[i].orientation = SwapBits<0, 2>(cube.edges[i].orientation);
                break;
            case kF:
            case kFc:
            case kB:
            case kBc:
            case kS:
            case kSc:
                rotated_cube.edges[i].orientation = SwapBits<0, 1>(cube.edges[i].orientation);
                break;
        }
    }
    for (unsigned int i = 0; i < Cube::kNumCenters; i++) {
        if (rotation < Rotations::kM || kCenterRotation[rotation-Rotations::kM][cube.centers[i].position] == -1) {
            rotated_cube.centers[i] = cube.centers[i];
            continue;
        }
        rotated_cube.centers[i].position = kCenterRotation[rotation-Rotations::kM][cube.centers[i].position];
    }
    return rotated_cube;
}
