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
#include "rotation.h"
#include "settings.h"
#include "shader.h"


// render the cube
class Renderer {
public:
    Renderer ();
    ~Renderer ();

    // draw the cube
    void Draw (Setting settings) const;

    // rotate the visual model of the cube
    void Rotate (Setting settings, Actions& actions);

    static const unsigned int kNumPieces = 26;
    struct Piece {
        // types:
        // 0: center
        // 1: edge
        // 2: small corner
        // 3: one direction facing out
        // 4: two directions facing out
        // 5: big corner
        int type;
        std::array<int, 3> orientation;
        glm::mat4 rotation;
        bool current_rotation = false;
    };


private:
    // GL
    unsigned int vertex_array_object_;
    unsigned int vertex_buffer_objects_;
    unsigned int element_buffer_object_;

    // mesh of all triangles and lines
    CubeMesh cube_mesh_;

    // colors
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
    static constexpr float kColors[kNumColors][4] = {
        {1.0, 1.0, 0.0, 1.0},
        {1.0, 0.5, 0.0, 1.0},
        {0.0, 1.0, 0.0, 1.0},
        {1.0, 0.0, 0.0, 1.0},
        {0.0, 0.0, 1.0, 1.0},
        {1.0, 1.0, 1.0, 1.0},
        {0.0, 0.0, 0.0, 1.0}
    };

    // piece representation for rendering
    std::array<Piece, kNumPieces> pieces_;

    // current rotation
    Rotations current_rotation_;
    glm::vec3 current_rotation_vector_;

    // additional rotation information
    bool started_current_rotation_ = false;
    float rotation_angle_ = 0;
    bool is_scrambling_ = false; // speedup
    // timing
    float elapsed_time_since_last_rotation_ = 0;
    float last_time_ = glfwGetTime();
};
