#pragma once

#include <random>
#include <cstdint>

#include "actions.h"
#include "cube.h"
#include "settings.h"


class Actions;


// different rotations using standart notation
// c means counterclockwise
const int kNumRotations = 18;
enum Rotations : uint8_t {
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


// get all legal rotations
std::vector<Rotations> GetLegalRotations (Cube& cube);


// rotation of the cube (not visual)
Cube Rotate (const Cube& cube, Rotations rotation);


// get num_rotations random legal rotations
uint64_t RandomRotations (Setting settings, Cube& cube, Actions& actions, int num_rotations, std::mt19937& rng, bool should_push);
