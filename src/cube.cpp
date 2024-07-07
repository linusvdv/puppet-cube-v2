#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstddef>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


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
    current_rotation = glm::rotate(current_rotation, (float)glfwGetTime(), glm::vec3(0.0, 1.0, 0.0));

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
