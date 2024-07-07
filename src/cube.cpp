#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstddef>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <ostream>


#include "cube.h"
#include "mesh.h"
#include "settings.h"


Cube::Cube(Shader shader) {
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
//    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
//    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
    glEnableVertexAttribArray(1);

    // unbind
    // glBindVertexArray(0);
}


Cube::~Cube() {
    // glDeleteVertexArrays(1, &vertex_array_object_);
    // glDeleteBuffers(1, &vertex_buffer_objects_);
    // glDeleteBuffers(1, &element_buffer_object_);
}


void Cube::Draw(Setting settings) const {
    // current rotation
    glm::mat4 current_rotation = glm::mat4(1.0F);
    current_rotation = glm::rotate(current_rotation, (float)glfwGetTime(), glm::vec3(0.0, 1.0, 0.0));

    // view
    glm::mat4 view_rotation = glm::mat4(1.0F);
    view_rotation = glm::scale(view_rotation, glm::vec3(settings.scroll/2));
    view_rotation = glm::rotate(view_rotation, glm::radians(settings.rotation.second), glm::vec3(1.0, 0.0, 0.0)); // second
    view_rotation = glm::rotate(view_rotation, glm::radians(settings.rotation.first), glm::vec3(0.0, 1.0, 0.0)); // first

    // calculate rotation
    CubeMesh rotated_cube_mesh = cube_mesh_;
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
        vertex.normal   = view_rotation * vertex.normal;
    }

    std::vector<int> line_indices;
    for (std::array<int, 2> indices : rotated_cube_mesh.lines) {
        line_indices.push_back(indices[0]);
        line_indices.push_back(indices[1]);
    }

    // black outline
    glEnable(GL_LINE_SMOOTH);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex)*rotated_cube_mesh.vertices.size(), rotated_cube_mesh.vertices.data(), GL_STATIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, line_indices.size() * sizeof(unsigned int), line_indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(vertex_array_object_);
    glDrawElements(GL_LINES, static_cast<unsigned int>(line_indices.size()), GL_UNSIGNED_INT, 0);

    glDisable(GL_LINE_SMOOTH);

    std::vector<int> triangle_indices;
    for (std::array<int, 3> indices : rotated_cube_mesh.triangles) {
        triangle_indices.push_back(indices[0]);
        triangle_indices.push_back(indices[1]);
        triangle_indices.push_back(indices[2]);
    }


    glEnable(GL_BLEND);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex)*rotated_cube_mesh.vertices.size(), rotated_cube_mesh.vertices.data(), GL_STATIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, triangle_indices.size() * sizeof(unsigned int), triangle_indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(vertex_array_object_);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDrawElements(GL_TRIANGLES, triangle_indices.size(), GL_UNSIGNED_INT, 0);
    glDisable(GL_BLEND);
}
