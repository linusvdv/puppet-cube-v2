#include <array>
#include <map>
#include <string>

#include "rotation.hpp"


std::map<std::string, RotRep> name_to_rot_rep;
std::array<RotRep, kNumRot> idx_to_rot_rep;
std::map<std::pair<std::array<std::array<int, 3>, 3>, int>, RotRep> mat_to_rot_rep;


constexpr std::array<RotRep, kNumRot> kRotations = {{
    // right left
    {
        "R",
        0,
        {{
            { 1,  0,  0},
            { 0,  0,  1},
            { 0, -1,  0}}},
        1
    },
    {
        "R'",
        1,
        {{
            { 1,  0,  0},
            { 0,  0, -1},
            { 0,  1,  0}}},
        1
    },
    {
        "M",
        12,
        {{
            { 1,  0,  0},
            { 0,  0,  1},
            { 0, -1,  0}}},
        0
    },
    {
        "M'",
        13,
        {{
            { 1,  0,  0},
            { 0,  0, -1},
            { 0,  1,  0}}},
        0
    },
    {
        "L'",
        3,
        {{
            { 1,  0,  0},
            { 0,  0,  1},
            { 0, -1,  0}}},
        -1
    },
    {
        "L",
        2,
        {{
            { 1,  0,  0},
            { 0,  0, -1},
            { 0,  1,  0}}},
        -1
    },

    // up down
    {
        "U",
        4,
        {{
            { 0,  0, -1},
            { 0,  1,  0},
            { 1,  0,  0}}},
        1
    },
    {
        "U'",
        5,
        {{
            { 0,  0,  1},
            { 0,  1,  0},
            {-1,  0,  0}}},
        1
    },
    {
        "E",
        14,
        {{
            { 0,  0, -1},
            { 0,  1,  0},
            { 1,  0,  0}}},
        0
    },
    {
        "E'",
        15,
        {{
            { 0,  0,  1},
            { 0,  1,  0},
            {-1,  0,  0}}},
        0
    },
    {
        "D'",
        7,
        {{
            { 0,  0, -1},
            { 0,  1,  0},
            { 1,  0,  0}}},
        -1
    },
    {
        "D",
        6,
        {{
            { 0,  0,  1},
            { 0,  1,  0},
            {-1,  0,  0}}},
        -1
    },

    // front back
    {
        "F",
        8,
        {{
            { 0,  1,  0},
            {-1,  0,  0},
            { 0,  0,  1}}},
        1
    },
    {
        "F'",
        9,
        {{
            { 0, -1,  0},
            { 1,  0,  0},
            { 0,  0,  1}}},
        1
    },
    {
        "S'",
        17,
        {{
            { 0,  1,  0},
            {-1,  0,  0},
            { 0,  0,  1}}},
        0
    },
    {
        "S",
        16,
        {{
            { 0, -1,  0},
            { 1,  0,  0},
            { 0,  0,  1}}},
        0
    },
    {
        "B'",
        11,
        {{
            { 0,  1,  0},
            {-1,  0,  0},
            { 0,  0,  1}}},
        -1
    },
    {
        "B",
        10,
        {{
            { 0, -1,  0},
            { 1,  0,  0},
            { 0,  0,  1}}},
        -1
    },
}};


void RotationInit() {
    // name to rotation representation
    for (RotRep rotation : kRotations) {
        name_to_rot_rep[rotation.name] = rotation;
    }
    // index to rotation representation
    for (RotRep rotation : kRotations) {
        idx_to_rot_rep[rotation.index] = rotation;
    }
    // index to rotation representation
    for (RotRep rotation : kRotations) {
        mat_to_rot_rep[{rotation.matrix, rotation.activate}] = rotation;
    }
}
