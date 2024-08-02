#pragma once
#include <random>

#include "actions.h"
#include "error_handler.h"
#include "settings.h"
#include "cube.h"


class Actions;


// different rotations using standart notation
// c means counterclockwise
const int kNumRotations = 18;
enum Rotations : unsigned int {
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


// read file of legal moves and huristic funciton
void InitializeLegalMoves (ErrorHandler& error_handler, Setting& settings);


// rotation of the cube (not visual)
Cube Rotate (const Cube& cube, Rotations rotation);


// get a random rotation
void RandomRotations (ErrorHandler& error_handler, Cube& cube, Actions& actions, int num_rotations, std::mt19937& rng);
