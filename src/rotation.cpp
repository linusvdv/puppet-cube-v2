#include <cstdint>
#include <random>
#include <vector>

#include "actions.h"
#include "rotation.h"
#include "cube.h"
#include "settings.h"


// get all legal rotations
std::vector<Rotations> GetLegalRotations (Cube& cube) {
    std::vector<Rotations> legal_rotations;

    cube.GetPositionData();

    // make list of legal moves and checking if the moves are allowed
    for (int i = 0; i <= int(Rotations::kBc); i++) {
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
Rotations GetRandomRotation (Cube& cube, std::mt19937& rng) {
    std::vector<Rotations> legal_rotation = GetLegalRotations(cube);

    // get a uniform distribution
    std::uniform_int_distribution<std::mt19937::result_type> distribution(0, legal_rotation.size()-1);
    return legal_rotation[distribution(rng)];
}


// add num_rotations random legal rotations to actions
uint64_t RandomRotations (Setting settings, Cube& cube, Actions& actions, int num_rotations, std::mt19937& rng, bool should_push) {
    uint64_t total_rotations = 0;
    while (total_rotations < uint64_t(num_rotations) || cube.GetCornerHeuristic() < settings.min_coner_heuristic) {
        Rotations random_rotation = GetRandomRotation(cube, rng);
        if (should_push) {
            actions.Push(Action(Instructions::kRotation, random_rotation), settings);
        }
        cube = Rotate(cube, random_rotation);
        total_rotations++;
    }
    return total_rotations;
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
    { 4,  5,  0,  1,  6,  7,  2,  3}, // M  -  R  + L'
    { 2,  3,  6,  7,  0,  1,  4,  5}, // M' -  R' + L 
    { 1,  5,  3,  7,  0,  4,  2,  6}, // E  -  U  + D'
    { 4,  0,  6,  2,  5,  1,  7,  3}, // E' -  U' + D
    { 1,  3,  0,  2,  5,  7,  4,  6}, // S  -  F' + B
    { 2,  0,  3,  1,  6,  4,  7,  5}, // S' -  F  + B'
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
    {-1, -1,  7, -1, -1,  2, -1, 10, -1, -1,  5, -1}, // B'
    { 2,  0,  3,  1, -1, -1, -1, -1, 10,  8, 11,  9}, // M  -  R  + L'
    { 1,  3,  0,  2, -1, -1, -1, -1,  9, 11,  8, 10}, // M' -  R' + L
    { 4, -1, -1,  6,  8,  0, 11,  3,  5, -1, -1,  7}, // E  -  U  + D'
    { 5, -1, -1,  7,  0,  8,  3, 11,  4, -1, -1,  6}, // E' -  U' + D
    {-1,  4,  5, -1,  9, 10,  1,  2, -1,  6,  7, -1}, // S  -  F' + B
    {-1,  6,  7, -1,  1,  2,  9, 10, -1,  4,  5, -1}, // S' -  F  + B'
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
            case kM:
            case kMc:
                rotated_cube.corners[i].orientation = SwapBits<1, 2>(cube.corners[i].orientation);
                break;
            case kU:
            case kUc:
            case kD:
            case kDc:
            case kE:
            case kEc:
                rotated_cube.corners[i].orientation = SwapBits<0, 2>(cube.corners[i].orientation);
                break;
            case kF:
            case kFc:
            case kB:
            case kBc:
            case kS:
            case kSc:
                rotated_cube.corners[i].orientation = SwapBits<0, 1>(cube.corners[i].orientation);
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
        rotated_cube.edges[i].orientation = uint8_t(!bool(cube.edges[i].orientation));
    }
    return rotated_cube;
}
