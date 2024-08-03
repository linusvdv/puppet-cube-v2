#include <algorithm>
#include <cstddef>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <numbers>


#include "renderer.h"
#include "mesh.h"
#include "rotation.h"
#include "settings.h"


// helper funktion to get the original rotation matrix
glm::mat4 GetRotationMatrix (float rotation_x, float rotation_y, float rotation_z) {
    return glm::rotate(glm::rotate(glm::rotate(glm::mat4(1.0), glm::radians(rotation_x), {1, 0, 0}),
                glm::radians(rotation_y), {0, 1, 0}), glm::radians(rotation_z), {0, 0, 1});
}

// solved visual state of the cube
const std::array<Renderer::Piece, Renderer::kNumPieces> kSolvedPieces = {{
    {0, { 0,  1,  0}, GetRotationMatrix(  0.0F,   0.0F,   0.0F)},
    {0, { 0,  0,  1}, GetRotationMatrix( 90.0F,   0.0F,   0.0F)},
    {0, {-1,  0,  0}, GetRotationMatrix(  0.0F,   0.0F,  90.0F)},
    {0, { 0,  0, -1}, GetRotationMatrix(-90.0F,   0.0F,   0.0F)},
    {0, { 1,  0,  0}, GetRotationMatrix(  0.0F,   0.0F, -90.0F)},
    {0, { 0, -1,  0}, GetRotationMatrix(180.0F,   0.0F,   0.0F)},
    {1, { 0,  1,  1}, GetRotationMatrix(  0.0F,   0.0F,   0.0F)},
    {1, {-1,  1,  0}, GetRotationMatrix(  0.0F, -90.0F,   0.0F)},
    {1, { 0,  1, -1}, GetRotationMatrix(  0.0F, 180.0F,   0.0F)},
    {1, { 1,  1,  0}, GetRotationMatrix(  0.0F,  90.0F,   0.0F)},
    {1, { 1,  0,  1}, GetRotationMatrix(  0.0F,   0.0F, -90.0F)},
    {1, {-1,  0,  1}, GetRotationMatrix(  0.0F,   0.0F,  90.0F)},
    {1, {-1,  0, -1}, GetRotationMatrix(  0.0F, 180.0F, -90.0F)},
    {1, { 1,  0, -1}, GetRotationMatrix(  0.0F, 180.0F,  90.0F)},
    {1, { 0, -1,  1}, GetRotationMatrix(  0.0F,   0.0F, 180.0F)},
    {1, {-1, -1,  0}, GetRotationMatrix(  0.0F, -90.0F, 180.0F)},
    {1, { 0, -1, -1}, GetRotationMatrix(  0.0F, 180.0F, 180.0F)},
    {1, { 1, -1,  0}, GetRotationMatrix(  0.0F,  90.0F, 180.0F)},
    {2, { 1,  1,  1}, GetRotationMatrix(  0.0F,   0.0F,   0.0F)},
    {3, {-1,  1,  1}, GetRotationMatrix(  0.0F,   0.0F,   0.0F)},
    {3, { 1, -1,  1}, GetRotationMatrix( 90.0F,  90.0F,   0.0F)},
    {3, { 1,  1, -1}, GetRotationMatrix(-90.0F,   0.0F, -90.0F)},
    {4, {-1,  1, -1}, GetRotationMatrix(  0.0F,   0.0F,   0.0F)},
    {4, {-1, -1,  1}, GetRotationMatrix(  0.0F,  90.0F,  90.0F)},
    {4, { 1, -1, -1}, GetRotationMatrix(-90.0F,   0.0F, -90.0F)},
    {5, {-1, -1, -1}, GetRotationMatrix(180.0F, -90.0F,   0.0F)}
}};

// translation of the pieces away from the center
constexpr std::array<glm::vec4, kNumPieces> kPieceOffset = {
    glm::vec4( 0.F,  1.F,  0.F,  0.F),
    glm::vec4( 0.F,  1.F,  1.F,  0.F),
    glm::vec4( 1.F,  1.F,  1.F,  0.F),
    glm::vec4(-1.F,  1.F,  1.F,  0.F),
    glm::vec4(-1.F,  1.F, -1.F,  0.F),
    glm::vec4( 1.F,  1.F,  1.F,  0.F)
};


Renderer::Renderer () {
    pieces_ = kSolvedPieces;
    cube_mesh_ = CubeMeshInitialisation();

    // OpenGL objects
    glGenVertexArrays(1, &vertex_array_object_);
    glBindVertexArray(vertex_array_object_);

    glGenBuffers(1, &vertex_buffer_objects_);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_objects_);

    glGenBuffers(1, &element_buffer_object_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_buffer_object_);

    // linking vertex attributes
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
    glEnableVertexAttribArray(2);

    // unbind
    glBindVertexArray(0);
}


Renderer::~Renderer () {
    // glDeleteVertexArrays(1, &vertex_array_object_);
    // glDeleteBuffers(1, &vertex_buffer_objects_);
    // glDeleteBuffers(1, &element_buffer_object_);
}


void Renderer::Draw (Setting settings) const {
    CubeMesh rotated_cube_mesh = cube_mesh_;

    // translate the pieces
    for (Vertex& vertex : rotated_cube_mesh.vertices) {
        vertex.position += kPieceOffset[pieces_[vertex.piece_index].type] * settings.pieceOffset;
    }

    // current rotation
    glm::mat4 current_rotation = glm::mat4(1.0F);
    if (started_current_rotation_) {
        current_rotation = glm::rotate(current_rotation, rotation_angle_, current_rotation_vector_);
    }

    // view
    glm::mat4 view_rotation = glm::mat4(1.0F);
    view_rotation = glm::rotate(view_rotation, glm::radians(settings.rotation.second), glm::vec3(1.0, 0.0, 0.0)); // second
    view_rotation = glm::rotate(view_rotation, glm::radians(settings.rotation.first), glm::vec3(0.0, 1.0, 0.0)); // first
    glm::mat4 scale = glm::scale(glm::mat4(1.0F), glm::vec3(settings.soom/2));

    // calculate rotation
    for (Vertex& vertex : rotated_cube_mesh.vertices) {
        // piece rotation
        vertex.position = pieces_[vertex.piece_index].rotation * vertex.position;
        vertex.normal   = pieces_[vertex.piece_index].rotation * vertex.normal;

        // current rotation
        if (pieces_[vertex.piece_index].current_rotation) {
            vertex.position = current_rotation * vertex.position;
            vertex.normal   = current_rotation * vertex.normal;
        }

        // view rotation
        vertex.position = view_rotation * vertex.position;
        vertex.position = scale * vertex.position;
        vertex.normal   = view_rotation * vertex.normal;
    }


    // only draw specific pieces
    rotated_cube_mesh.triangles = {};
    for (std::array<int, 3> triangle : cube_mesh_.triangles) {
        // TODO: Only draw specific pieces
        if (rotated_cube_mesh.vertices[triangle[0]].piece_index == 20 || true) {
            rotated_cube_mesh.triangles.push_back(triangle);
        }
    }

    // TODO: sort triangles correctly
    std::sort(rotated_cube_mesh.triangles.begin(), rotated_cube_mesh.triangles.end(),
            [=](std::array<int, 3> first, std::array<int, 3> second){return rotated_cube_mesh.vertices[first[0] ].position[2] +
                                                                            rotated_cube_mesh.vertices[first[1] ].position[2] +
                                                                            rotated_cube_mesh.vertices[first[2] ].position[2] <
                                                                            rotated_cube_mesh.vertices[second[0]].position[2] +
                                                                            rotated_cube_mesh.vertices[second[1]].position[2] +
                                                                            rotated_cube_mesh.vertices[second[2]].position[2];});

    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex)*rotated_cube_mesh.vertices.size(), rotated_cube_mesh.vertices.data(), GL_STATIC_DRAW);
    glBindVertexArray(vertex_array_object_);

    // black outline
    glEnable(GL_LINE_SMOOTH);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, rotated_cube_mesh.lines.size() * sizeof(std::array<int, 2>), rotated_cube_mesh.lines.data(), GL_STATIC_DRAW);
    glDrawElements(GL_LINES, 2 * rotated_cube_mesh.lines.size(), GL_UNSIGNED_INT, 0);
    glDisable(GL_LINE_SMOOTH);

    // triangles
    glEnable(GL_BLEND);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, rotated_cube_mesh.triangles.size() * sizeof(std::array<int, 3>), rotated_cube_mesh.triangles.data(), GL_STATIC_DRAW);
    glDrawElements(GL_TRIANGLES, 3 * rotated_cube_mesh.triangles.size(), GL_UNSIGNED_INT, 0);
    glDisable(GL_BLEND);

    glBindVertexArray(0);
}


struct PieceRotationMatrix {
    // to check which are effected by current rotation
    int relevant_index;
    int relevant_value;

    // rotation of the pieces as int
    std::array<std::array<int, 3>, 3> full_rotation;
    // around which axis is the rotation
    glm::vec3 rotation_axis;
};


// rotation data
const std::array<PieceRotationMatrix, kNumRotations> kPieceRotationMatrices = {{
    // R
    {0, 1, {{{ 1, 0, 0}, { 0, 0, 1}, { 0, -1, 0}}}, {-1.F, 0.F, 0.F}},
    // R'
    {0, 1, {{{ 1, 0, 0}, { 0, 0,-1}, { 0,  1, 0}}}, { 1.F, 0.F, 0.F}},
    // L
    {0, -1, {{{ 1, 0, 0}, { 0, 0,-1}, { 0,  1, 0}}}, { 1.F, 0.F, 0.F}},
    // L'
    {0, -1, {{{ 1, 0, 0}, { 0, 0, 1}, { 0, -1, 0}}}, {-1.F, 0.F, 0.F}},

    // U
    {1, 1, {{{ 0, 0,-1}, { 0, 1, 0}, { 1,  0, 0}}}, { 0.F,-1.F, 0.F}},
    // U'
    {1, 1, {{{ 0, 0, 1}, { 0, 1, 0}, {-1,  0, 0}}}, { 0.F, 1.F, 0.F}},
    // D
    {1, -1, {{{ 0, 0, 1}, { 0, 1, 0}, {-1,  0, 0}}}, { 0.F, 1.F, 0.F}},
    // D'
    {1, -1, {{{ 0, 0,-1}, { 0, 1, 0}, { 1,  0, 0}}}, { 0.F,-1.F, 0.F}},

    // F
    {2, 1, {{{ 0, 1, 0}, {-1, 0, 0}, { 0,  0, 1}}}, { 0.F, 0.F,-1.F}},
    // F'
    {2, 1, {{{ 0,-1, 0}, { 1, 0, 0}, { 0,  0, 1}}}, { 0.F, 0.F, 1.F}},
    // B
    {2, -1, {{{ 0,-1, 0}, { 1, 0, 0}, { 0,  0, 1}}}, { 0.F, 0.F, 1.F}},
    // B'
    {2, -1, {{{ 0, 1, 0}, {-1, 0, 0}, { 0,  0, 1}}}, { 0.F, 0.F,-1.F}},

    // M
    {0, 0, {{{ 1, 0, 0}, { 0, 0,-1}, { 0,  1, 0}}}, { 1.F, 0.F, 0.F}},
    // M'
    {0, 0, {{{ 1, 0, 0}, { 0, 0, 1}, { 0, -1, 0}}}, {-1.F, 0.F, 0.F}},

    // E
    {1, 0, {{{ 0, 0, 1}, { 0, 1, 0}, {-1,  0, 0}}}, { 0.F, 1.F, 0.F}},
    // E'
    {1, 0, {{{ 0, 0,-1}, { 0, 1, 0}, { 1,  0, 0}}}, { 0.F,-1.F, 0.F}},

    // S
    {2, 0, {{{ 0, 1, 0}, {-1, 0, 0}, { 0,  0, 1}}}, { 0.F, 0.F,-1.F}},
    // S'
    {2, 0, {{{ 0,-1, 0}, { 1, 0, 0}, { 0,  0, 1}}}, { 0.F, 0.F, 1.F}},
}};


std::array<int, 3> IntMat3MultWithVec3 (const std::array<std::array<int, 3>, 3>& mat3, const std::array<int, 3>& vec3) {
    return {{mat3[0][0]*vec3[0] + mat3[0][1]*vec3[1] + mat3[0][2]*vec3[2],
             mat3[1][0]*vec3[0] + mat3[1][1]*vec3[1] + mat3[1][2]*vec3[2],
             mat3[2][0]*vec3[0] + mat3[2][1]*vec3[1] + mat3[2][2]*vec3[2]
            }};
}


void Renderer::Rotate (Setting settings, Actions& actions) {
    // get time difference
    float old_time = last_time_;
    last_time_ = glfwGetTime();

    // visual rotation can be stopped
    if (!settings.should_rotate) {
        return;
    }

    // no current rotation
    if (!started_current_rotation_) {
        // elapsed time after finishing last rotation
        if (elapsed_time_since_last_rotation_ < (settings.min_elapsed_time_since_last_rotation) / (is_scrambling_ ? settings.scrambling_multiplier : 1)) {
            elapsed_time_since_last_rotation_ += last_time_ - old_time;
            return;
        }

        // get action from other thread
        Action new_action;
        if (!actions.TryPop(new_action)) {
            return;
        }

        switch (new_action.instruction) {
            case Instructions::kRotation:
                // set new current rotation
                current_rotation_ = new_action.rotation;
                started_current_rotation_ = true;

                current_rotation_vector_ = kPieceRotationMatrices[current_rotation_].rotation_axis;

                for (Piece& piece : pieces_) {
                    // this piece is affected by current rotation
                    if (piece.orientation[kPieceRotationMatrices[current_rotation_].relevant_index] == kPieceRotationMatrices[current_rotation_].relevant_value) {
                        piece.current_rotation = true;
                    }
                }
                break;
            case Instructions::kReset:
                // reset to solved position
                pieces_ = kSolvedPieces;
                return;
            case Instructions::kIsScrambling:
                // increases visual rotation speed
                is_scrambling_ = true;
                return;
            case Instructions::kIsSolving:
                // sets visual rotation speed back to normal
                is_scrambling_ = false;
                return;
        }
    }

    // change rotation angle based on elapsed time
    rotation_angle_ += (last_time_ - old_time) * settings.rotation_speed *
        (is_scrambling_ ? settings.scrambling_multiplier : 1);


    // finished rotation
    if (rotation_angle_ > float(std::numbers::pi) / 2) {
        // make visual rotation 
        rotation_angle_ = float(std::numbers::pi) / 2;
        glm::mat4 current_rotation = glm::mat4(1.0F);
        current_rotation = glm::rotate(current_rotation, rotation_angle_, current_rotation_vector_);

        // reset current rotation data
        started_current_rotation_ = false;
        rotation_angle_ = 0;
        elapsed_time_since_last_rotation_ = 0;

        for (size_t i = 0; i < kNumPieces; i++) {
            if (!pieces_[i].current_rotation) {
                continue;
            }
            pieces_[i].current_rotation = false;
            pieces_[i].rotation = current_rotation * pieces_[i].rotation;
            // rotation orientation
            pieces_[i].orientation = IntMat3MultWithVec3(kPieceRotationMatrices[current_rotation_].full_rotation, pieces_[i].orientation);
        }
        return;
    }
}
