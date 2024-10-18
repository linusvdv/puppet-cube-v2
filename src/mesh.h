#pragma once

#include <array>
#include <vector>

#include <glm/glm.hpp>


struct Vertex {
    // position data
    unsigned int piece_index;
    glm::vec4 position;

    // color data
    glm::vec4 normal;
    float color[3];
};


struct CubeMesh {
    // all different vertices
    std::vector<Vertex> vertices;

    // index to the vertices forming a triangle
    std::vector<std::array<int, 3>> triangles;

    // index to all lines
    std::vector<std::array<int, 2>> lines;
};


CubeMesh CubeMeshInitialisation();


const unsigned int kNumPieces = 26;
const unsigned int kNumPieceTypes = 6;


enum Colors {
    kYellow, 
    kOrange,
    kGreen,
    kRed,
    kBlue,
    kWhite,
    kBlack
};

const int kNumColors = 7;
const float kColors[kNumColors][3] = {
    {1.0, 1.0, 0.0},
    {1.0, 0.5, 0.0},
    {0.0, 1.0, 0.0},
    {1.0, 0.0, 0.0},
    {0.0, 0.0, 1.0},
    {1.0, 1.0, 1.0},
    {0.0, 0.0, 0.0}
};
