#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstddef>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>


#include "cube.h"
#include "mesh.h"
#include "settings.h"


const std::array<glm::vec4, kNumPieces> kPieceOffset =
    {glm::vec4( 0.F,  1.F,  0.F,  0.F),
     glm::vec4( 0.F,  1.F,  1.F,  0.F),
     glm::vec4( 1.F,  1.F,  1.F,  0.F),
     glm::vec4(-1.F,  1.F,  1.F,  0.F),
     glm::vec4(-1.F,  1.F, -1.F,  0.F),
     glm::vec4( 1.F,  1.F,  1.F,  0.F),
    };


Cube::Cube() {
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


Cube::~Cube() {
    // glDeleteVertexArrays(1, &vertex_array_object_);
    // glDeleteBuffers(1, &vertex_buffer_objects_);
    // glDeleteBuffers(1, &element_buffer_object_);
}


void Cube::Draw(Setting settings) const {
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
    glm::mat4 scale = glm::scale(glm::mat4(1.0F), glm::vec3(settings.scroll/2));

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

    rotated_cube_mesh.triangles = {};
    for (std::array<int, 3> triangle : cube_mesh_.triangles) {
        if (rotated_cube_mesh.vertices[triangle[0]].piece_index == 20 || true) {
            rotated_cube_mesh.triangles.push_back(triangle);
        }
    }

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

    std::array<std::array<int, 3>, 3> full_rotation;
    glm::vec3 rotation_axis;
};


const std::array<PieceRotationMatrix, Cube::kNumRotations> kPieceRotationMatrices = {{
    // L
    {0, -1, {{{ 1, 0, 0}, { 0, 0,-1}, { 0,  1, 0}}}, { 1.F, 0.F, 0.F}},
    // L'
    {0, -1, {{{ 1, 0, 0}, { 0, 0, 1}, { 0, -1, 0}}}, {-1.F, 0.F, 0.F}},
    // R
    {0, 1, {{{ 1, 0, 0}, { 0, 0, 1}, { 0, -1, 0}}}, {-1.F, 0.F, 0.F}},
    // R'
    {0, 1, {{{ 1, 0, 0}, { 0, 0,-1}, { 0,  1, 0}}}, { 1.F, 0.F, 0.F}},

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

    // F
    {2, 0, {{{ 0, 1, 0}, {-1, 0, 0}, { 0,  0, 1}}}, { 0.F, 0.F,-1.F}},
    // F'
    {2, 0, {{{ 0,-1, 0}, { 1, 0, 0}, { 0,  0, 1}}}, { 0.F, 0.F, 1.F}},
}};


std::array<int, 3> IntMat3MultWithVec3(const std::array<std::array<int, 3>, 3>& mat3, const std::array<int, 3>& vec3) {
    return {{mat3[0][0]*vec3[0] + mat3[0][1]*vec3[1] + mat3[0][2]*vec3[2],
             mat3[1][0]*vec3[0] + mat3[1][1]*vec3[1] + mat3[1][2]*vec3[2],
             mat3[2][0]*vec3[0] + mat3[2][1]*vec3[1] + mat3[2][2]*vec3[2]
            }};
}


void Cube::Rotate(Setting settings) {
    float old_time = last_time_;
    last_time_ = glfwGetTime();

    if (!should_rotate) {
        return;
    }

    if (!started_current_rotation_) {
        if (elapsed_time_since_last_rotation_ < settings.min_elapsed_time_since_last_rotation / 4) {
            elapsed_time_since_last_rotation_ += last_time_ - old_time;
            return;
        }

        if (nextRotations.empty()) {
            return;
        }

        started_current_rotation_ = true;
        current_rotation_ = nextRotations.front();
        nextRotations.pop();
        nextRotations.push(current_rotation_);

        current_rotation_vector_ = kPieceRotationMatrices[current_rotation_].rotation_axis;

        for (Piece& piece : pieces_) {
            // this piece is affected by current rotation
            if (piece.orientation[kPieceRotationMatrices[current_rotation_].relevant_index] == kPieceRotationMatrices[current_rotation_].relevant_value) {
                piece.current_rotation = true;
            }
        }
    }


    if (rotation_angle_ > M_PIf / 2) {
        rotation_angle_ = M_PIf / 2;
        glm::mat4 current_rotation = glm::mat4(1.0F);
        current_rotation = glm::rotate(current_rotation, rotation_angle_, current_rotation_vector_);

        started_current_rotation_ = false;
        rotation_angle_ = 0;
        elapsed_time_since_last_rotation_ = 0;

        for (Piece& piece : pieces_) {
            if (!piece.current_rotation) {
                continue;
            }
            piece.current_rotation = false;
            piece.orientation = IntMat3MultWithVec3(kPieceRotationMatrices[current_rotation_].full_rotation, piece.orientation);
            piece.rotation = current_rotation * piece.rotation;
        }
        return;
    }

    rotation_angle_ += (last_time_ - old_time) *2;
}
