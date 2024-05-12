#pragma once

#include <array>
#include <vector>


#include "shader.h"


struct EdgeMeshes {
    float color[4];
    float points[12]; // always three coorinates after each other
    int triangles[6];
};


struct CornerMeshes {
    float color[4];
    float points[18]; // always three coorinates after each other
    int triangles[12];
};


struct Piece {
    int index;
    int orienation;
    std::array<int, 3> rotation;
};


class Cube {
public:
    static constexpr int kNumEdges = 4;
    static constexpr int kNumCorners = 8;

    std::array<Piece, kNumEdges> edges;
    std::array<Piece, kNumCorners> corners;

    void Draw(Shader our_shader) const;

private:
    const std::array<std::array<EdgeMeshes, 2>, kNumEdges> edge_mesh_ = {{
        //         color          points         triangles
        {{ {{1.0, 1.0, 0.0, 1.0}, { 0.2,  0.6,  0.2,
                                    0.2,  0.6,  0.6,
                                   -0.2,  0.6,  0.6,
                                   -0.2,  0.6,  0.2}, {0, 1, 3,
                                                       1, 2, 3}},     // yellow
           {{1.0, 0.5, 0.0, 1.0}, { 0.2,  0.6,  0.6,
                                    0.2,  0.2,  0.6,
                                   -0.2,  0.2,  0.6,
                                   -0.2,  0.6,  0.6}, {0, 1, 3,
                                                       1, 2, 3}} }},  // orange

        {{ {{0.0, 0.0, 1.0, 1.0}, { 0.6,  0.2,  0.2,
                                    0.6,  0.2,  0.6,
                                    0.6, -0.2,  0.6,
                                    0.6, -0.2,  0.2}, {0, 1, 3,
                                                       1, 2, 3}},     // blue
           {{1.0, 0.5, 0.0, 1.0}, { 0.6,  0.2,  0.6,
                                    0.6, -0.2,  0.6,
                                    0.2, -0.2,  0.6,
                                    0.2,  0.2,  0.6}, {0, 1, 3,
                                                       1, 2, 3}} }},  // orange

       {{  {{1.0, 1.0, 1.0, 1.0}, { 0.2, -0.6,  0.2,
                                    0.2, -0.6,  0.6,
                                   -0.2, -0.6,  0.6,
                                   -0.2, -0.6,  0.2}, {0, 1, 3,
                                                       1, 2, 3}},     // white
           {{1.0, 0.5, 0.0, 1.0}, { 0.2, -0.6,  0.6,
                                    0.2, -0.2,  0.6,
                                   -0.2, -0.2,  0.6,
                                   -0.2, -0.6,  0.6}, {0, 1, 3,
                                                       1, 2, 3}} }},  // orange


        {{ {{0.0, 1.0, 0.0, 1.0}, {-0.6,  0.2,  0.2,
                                   -0.6,  0.2,  0.6,
                                   -0.6, -0.2,  0.6,
                                   -0.6, -0.2,  0.2}, {0, 1, 3,
                                                       1, 2, 3}},     // green
           {{1.0, 0.5, 0.0, 1.0}, {-0.6,  0.2,  0.6,
                                   -0.6, -0.2,  0.6,
                                   -0.2, -0.2,  0.6,
                                   -0.2,  0.2,  0.6}, {0, 1, 3,
                                                       1, 2, 3}} }},  // orange
     }};

    const std::array<std::array<CornerMeshes, 3>, kNumCorners> corner_mesh_ = {};
};
