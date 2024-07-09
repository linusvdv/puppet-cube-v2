#pragma once

#include <array>
#include <queue>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "mesh.h"
#include "settings.h"
#include "shader.h"


class Cube {
public:
    static const int kNumRotations = 18;
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
        kM,
        kMc,
        kE,
        kEc,
        kS,
        kSc
    };

    std::queue<Rotations> nextRotations;

    Cube();
    ~Cube();
    void Draw(Setting settings) const;

    void AddRotation(Rotations rotation) {
        nextRotations.push(rotation);
    }
    bool should_rotate = true;

    void Rotate(Setting settings);


private:
    // GL
    unsigned int vertex_array_object_;
    unsigned int vertex_buffer_objects_;
    unsigned int element_buffer_object_;

    unsigned int piece_data_location_;
    unsigned int rotations_location_;

    CubeMesh cube_mesh_;

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
        std::array<int, 3> orientation;
        glm::mat4 rotation;
        bool current_rotation = false;
    };

    static const unsigned int kNumPieces = 26;
    std::array<Piece, kNumPieces> pieces_ = {{
        {0, { 0,  1,  0}, GetRotationMatrix(  0.0,   0.0,   0.0)},
        {0, { 0,  0,  1}, GetRotationMatrix( 90.0,   0.0,   0.0)},
        {0, {-1,  0,  0}, GetRotationMatrix(  0.0,   0.0,  90.0)},
        {0, { 0,  0, -1}, GetRotationMatrix(-90.0,   0.0,   0.0)},
        {0, { 1,  0,  0}, GetRotationMatrix(  0.0,   0.0, -90.0)},
        {0, { 0, -1,  0}, GetRotationMatrix(180.0,   0.0,   0.0)},
        {1, { 0,  1,  1}, GetRotationMatrix(  0.0,   0.0,   0.0)},
        {1, {-1,  1,  0}, GetRotationMatrix(  0.0, -90.0,   0.0)},
        {1, { 0,  1, -1}, GetRotationMatrix(  0.0, 180.0,   0.0)},
        {1, { 1,  1,  0}, GetRotationMatrix(  0.0,  90.0,   0.0)},
        {1, { 1,  0,  1}, GetRotationMatrix(  0.0,   0.0, -90.0)},
        {1, {-1,  0,  1}, GetRotationMatrix(  0.0,   0.0,  90.0)},
        {1, {-1,  0, -1}, GetRotationMatrix(  0.0, 180.0, -90.0)},
        {1, { 1,  0, -1}, GetRotationMatrix(  0.0, 180.0,  90.0)},
        {1, { 0, -1,  1}, GetRotationMatrix(  0.0,   0.0, 180.0)},
        {1, {-1, -1,  0}, GetRotationMatrix(  0.0, -90.0, 180.0)},
        {1, { 0, -1, -1}, GetRotationMatrix(  0.0, 180.0, 180.0)},
        {1, { 1, -1,  0}, GetRotationMatrix(  0.0,  90.0, 180.0)},
        {2, { 1,  1,  1}, GetRotationMatrix(  0.0,   0.0,   0.0)},
        {3, {-1,  1,  1}, GetRotationMatrix(  0.0,   0.0,   0.0)},
        {3, { 1, -1,  1}, GetRotationMatrix( 90.0,  90.0,   0.0)},
        {3, { 1,  1, -1}, GetRotationMatrix(-90.0,   0.0, -90.0)},
        {4, {-1,  1, -1}, GetRotationMatrix(  0.0,   0.0,   0.0)},
        {4, {-1, -1,  1}, GetRotationMatrix(  0.0,  90.0,  90.0)},
        {4, { 1, -1, -1}, GetRotationMatrix(-90.0,   0.0, -90.0)},
        {5, {-1, -1, -1}, GetRotationMatrix(180.0, -90.0,   0.0)}
    }};


    Rotations current_rotation_;
    glm::vec3 current_rotation_vector_;
    bool started_current_rotation_ = false;
    float elapsed_time_since_last_rotation_ = 0;
    float rotation_angle_ = 0;
    float last_time_ = glfwGetTime();
};
