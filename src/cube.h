#pragma once

#include <array>
#include <queue>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


#include "shader.h"


class Cube {
public:
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
        kBc
    };

    Rotations current_rotation;
    float rotation_angle;
    std::queue<Rotations> nextRotations;

    Cube(Shader shader);
    ~Cube();
    void Draw(Shader shader) const;

    void Rotate(Rotations rotation);


private:
    // GL
    unsigned int vertex_array_object_;
    unsigned int vertex_buffer_objects_;
    unsigned int element_buffer_object_;

    unsigned int piece_data_location_;
    unsigned int rotations_location_;

    static glm::mat4 GetRotationMatrix(float rotation_x, float rotation_y, float rotation_z) {
        return glm::rotate(glm::rotate(glm::rotate(glm::mat4(1.0), glm::radians(rotation_x), {1, 0, 0}),
                    glm::radians(rotation_y), {0, 1, 0}), glm::radians(rotation_z), {0, 0, 1});
    }

    enum Colors {
        kYellow, 
        kOrange,
        kGreen,
        kRed,
        kBlue,
        kWhite,
        kBlack
    };

    static const int kNumColors = 7;
    const float colors_[kNumColors][4] = {
        {1.0, 1.0, 0.0, 1.0},
        {1.0, 0.5, 0.0, 1.0},
        {0.0, 1.0, 0.0, 1.0},
        {1.0, 0.0, 0.0, 1.0},
        {0.0, 0.0, 1.0, 1.0},
        {1.0, 1.0, 1.0, 1.0},
        {0.0, 0.0, 0.0, 1.0}
    };

    // types:
    // 0: center
    // 1: edge
    // 2: small corner
    // 3: one direction facing out
    // 4: two directions facing out
    // 5: big corner
    struct Piece {
        int type;
        std::vector<Colors> colors;
        glm::mat4 rotation;
        bool current_rotation;
    };

    static const unsigned int kNumPieces = 26;
    std::array<Piece, kNumPieces> pieces_ = {{
        {0, {Colors::kYellow}, GetRotationMatrix(  0.0,   0.0,   0.0), true},
        {0, {Colors::kOrange}, GetRotationMatrix( 90.0,   0.0,   0.0)},
        {0, {Colors::kGreen }, GetRotationMatrix(  0.0,   0.0,  90.0)},
        {0, {Colors::kRed   }, GetRotationMatrix(-90.0,   0.0,   0.0)},
        {0, {Colors::kBlue  }, GetRotationMatrix(  0.0,   0.0, -90.0)},
        {0, {Colors::kWhite }, GetRotationMatrix(180.0,   0.0,   0.0)},
        {1, {Colors::kYellow, Colors::kOrange}, GetRotationMatrix(  0.0,   0.0,   0.0), true},
        {1, {Colors::kYellow, Colors::kGreen }, GetRotationMatrix(  0.0, -90.0,   0.0), true},
        {1, {Colors::kYellow, Colors::kRed   }, GetRotationMatrix(  0.0, 180.0,   0.0), true},
        {1, {Colors::kYellow, Colors::kBlue  }, GetRotationMatrix(  0.0,  90.0,   0.0), true},
        {1, {Colors::kBlue,   Colors::kOrange}, GetRotationMatrix(  0.0,   0.0, -90.0)},
        {1, {Colors::kGreen,  Colors::kOrange}, GetRotationMatrix(  0.0,   0.0,  90.0)},
        {1, {Colors::kGreen,  Colors::kRed   }, GetRotationMatrix(  0.0, 180.0, -90.0)},
        {1, {Colors::kBlue,   Colors::kRed   }, GetRotationMatrix(  0.0, 180.0,  90.0)},
        {1, {Colors::kWhite,  Colors::kOrange}, GetRotationMatrix(  0.0,   0.0, 180.0)},
        {1, {Colors::kWhite,  Colors::kGreen }, GetRotationMatrix(  0.0, -90.0, 180.0)},
        {1, {Colors::kWhite,  Colors::kRed   }, GetRotationMatrix(  0.0, 180.0, 180.0)},
        {1, {Colors::kWhite,  Colors::kBlue  }, GetRotationMatrix(  0.0,  90.0, 180.0)},
        {2, {Colors::kYellow, Colors::kOrange, Colors::kBlue  }, GetRotationMatrix(  0.0,   0.0,   0.0), true},
        {3, {Colors::kYellow, Colors::kGreen,  Colors::kOrange}, GetRotationMatrix(  0.0,   0.0,   0.0), true},
        {3, {Colors::kOrange, Colors::kWhite,  Colors::kBlue  }, GetRotationMatrix( 90.0,  90.0,   0.0)},
        {3, {Colors::kBlue,   Colors::kRed,    Colors::kYellow}, GetRotationMatrix(-90.0,   0.0, -90.0), true},
        {4, {Colors::kYellow, Colors::kRed,    Colors::kGreen }, GetRotationMatrix(  0.0,   0.0,   0.0), true},
        {4, {Colors::kOrange, Colors::kGreen,  Colors::kWhite }, GetRotationMatrix(  0.0,  90.0,  90.0)},
        {4, {Colors::kBlue,   Colors::kWhite,  Colors::kRed   }, GetRotationMatrix(-90.0,   0.0, -90.0)},
        {5, {Colors::kWhite,  Colors::kGreen,  Colors::kRed   }, GetRotationMatrix(180.0, -90.0,   0.0)}
    }};


    struct Mesh {
        std::vector<float> points;
        // vector of triangles for different color
        std::vector<std::vector<int>> triangles;
        std::vector<int> lines;
    };

    void MeshInitialisation();

    std::array<Mesh, 6> meshes_ = {{
        // centers (default yellow)
        // points
        {{ 0.2,  0.6,  0.2,
           0.2,  0.6, -0.2,
          -0.2,  0.6, -0.2,
          -0.2,  0.6,  0.2,
           0.2,  0.2,  0.2,
           0.2,  0.2, -0.2,
          -0.2,  0.2, -0.2,
          -0.2,  0.2,  0.2},

        // triangles
         {{0, 1, 2,
           0, 2, 3,
           0, 4, 5,
           0, 1, 5,
           1, 5, 6,
           1, 2, 6,
           2, 6, 7,
           2, 3, 7,
           3, 7, 4,
           3, 0, 4}},

        // lines
         {0, 1,
          1, 2,
          2, 3,
          3, 0,
          0, 4,
          1, 5,
          2, 6,
          3, 7,
          4, 5,
          5, 6,
          6, 7,
          7, 4}
        },

        // edges (default yellow/orange)
        // points
        {{ 0.2,  0.6,  0.6,
           0.2,  0.6,  0.2,
          -0.2,  0.6,  0.2,
          -0.2,  0.6,  0.6,
           0.2,  0.2,  0.6,
           0.2,  0.2,  0.2,
          -0.2,  0.2,  0.2,
          -0.2,  0.2,  0.6},

        // triangles
         {{0, 1, 2,
           0, 2, 3,
           0, 5, 1,
           1, 5, 6,
           1, 2, 6,
           2, 6, 3},
          {0, 3, 7,
           0, 7, 4,
           0, 4, 5,
           4, 5, 6,
           4, 6, 7,
           3, 6, 7}},

        // lines
         {0, 1,
          1, 2,
          2, 3,
          3, 0,
          0, 4,
          1, 5,
          2, 6,
          3, 7,
          4, 5,
          5, 6,
          6, 7,
          7, 4}
        },

        // small corners (default white/orange/blue)
        // points
        {{ 0.6,  0.6,  0.6,
           0.6,  0.6,  0.2,
           0.2,  0.6,  0.2,
           0.2,  0.6,  0.6,
           0.6,  0.2,  0.6,
           0.6,  0.2,  0.2,
           0.2,  0.2,  0.2,
           0.2,  0.2,  0.6},

        // triangles
         {{0, 1, 2,
           0, 2, 3,
           1, 2, 6,
           2, 6, 3},
          {0, 3, 7,
           0, 7, 4,
           3, 6, 7,
           4, 6, 7},
          {0, 4, 5,
           0, 5, 1,
           4, 6, 5,
           1, 5, 6}},

        // lines
         {0, 1,
          1, 2,
          2, 3,
          3, 0,
          0, 4,
          1, 5,
          2, 6,
          3, 7,
          4, 5,
          5, 6,
          6, 7,
          7, 4}
        },

        // one direction facing out (default yellow/green/orange)
        // some of this initialisation is done in cube.cpp
        // points
        {{-0.2,  0.6,  0.2,
          -0.2,  0.6,  0.6,
          -1.0,  0.6,  0.6,
          -1.0,  0.6, -0.2,
          -0.6,  0.6, -0.2,
          -0.6,  0.6,  0.2,
          -0.2,  0.2,  0.2,
          -0.2,  0.2,  0.6,
          -0.6,  0.2,  0.6,
          -0.6, -0.2,  0.6,
          -1.0, -0.2,  0.6,
          -1.0, -0.2, -0.2,
         },

        // triangles
         {{0, 1, 2,
           2, 3, 4,
           4, 5, 2,
           5, 2, 0,
           0, 1, 6},
          {2, 3, 10,
           10, 11, 3},
          {6, 7, 1,
           7, 1, 2,
           7, 8, 2,
           2, 8, 9,
           2, 9, 10}},

        // lines
         {0, 1,
          1, 2,
          2, 3,
          3, 4,
          4, 5,
          5, 0,
          0, 6,
          6, 7,
          7, 1,
          7, 8,
          8, 9,
          9, 10,
          10, 11,
          2, 10,
          3, 11
         }
        },

        // corner two directions facing out (default yellow/red/green)
        // some of this initialisation is done in cube.cpp
        // points
        {{-0.2,  0.6, -0.2,
          -0.2,  0.6, -1.0,
          -1.0,  0.6, -1.0,
          -1.0,  0.6, -0.2,
          -0.2, -0.2, -1.0,
          -1.0, -0.2, -1.0,
          -1.0, -0.2, -0.2,
          -0.2,  0.2, -0.2,
          -0.2,  0.2, -0.6,
          -0.2,  0.2, -1.0,
          -0.6,  0.2, -0.2,
          -1.0,  0.2, -0.2},

        // triangles
         {{0, 1, 2,
           0, 2, 3,
           0, 7, 8,
           0, 8, 1,
           0, 7, 10,
           0, 10, 3},
          {1, 4, 5,
           1, 5, 2,
           8, 9, 1,
           4, 9, 14,
           9, 14, 15,
           4, 5, 14},
          {2, 5, 6,
           2, 6, 3,
           10, 11, 3,
           6, 11, 12,
           11, 12, 13,
           5, 6, 12}},

        // lines
         {0, 1,
          1, 2,
          2, 3,
          3, 0,
          1, 4,
          2, 5,
          3, 6,
          4, 5,
          5, 6,
          0, 7,
          7, 13,
          7, 15,
          12, 13,
          14, 15,
          12, 6,
          14, 4}
        },

        // big corners (default white/orange/blue)
        // points
        {{ 1.0,  1.0,  1.0,
           1.0,  1.0,  0.2,
           0.2,  1.0,  0.2,
           0.2,  1.0,  1.0,
           1.0,  0.2,  1.0,
           1.0,  0.2,  0.2,
           0.2,  0.2,  0.2,
           0.2,  0.2,  1.0},

        // triangles
         {{0, 1, 2,
           0, 2, 3,
           1, 2, 6,
           2, 6, 3},
          {0, 3, 7,
           0, 7, 4,
           3, 6, 7,
           4, 6, 7},
          {0, 4, 5,
           0, 5, 1,
           4, 6, 5,
           1, 5, 6}},

        // lines
         {0, 1,
          1, 2,
          2, 3,
          3, 0,
          0, 4,
          1, 5,
          2, 6,
          3, 7,
          4, 5,
          5, 6,
          6, 7,
          7, 4}
        },
    }};
};
