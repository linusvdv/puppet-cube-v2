#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstddef>
#include <cstdlib>
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
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));

    // unbind
    glBindVertexArray(0);
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
        /*
        std::cout << rotated_cube_mesh.vertices.size() << ": ";
        std::cout << indices[0] << " " << indices[1] << "\n";
        std::cout << rotated_cube_mesh.vertices[indices[0]].position[0] << " "
                  << rotated_cube_mesh.vertices[indices[0]].position[1] << " "
                  << rotated_cube_mesh.vertices[indices[0]].position[2] << " "
                  << rotated_cube_mesh.vertices[indices[0]].position[3] << " "
                  << rotated_cube_mesh.vertices[indices[0]].color[0]    << " "
                  << rotated_cube_mesh.vertices[indices[0]].color[1]    << " "
                  << rotated_cube_mesh.vertices[indices[0]].color[2]    << "\n";
        std::cout << rotated_cube_mesh.vertices[indices[1]].position[0] << " "
                  << rotated_cube_mesh.vertices[indices[1]].position[1] << " "
                  << rotated_cube_mesh.vertices[indices[1]].position[2] << " "
                  << rotated_cube_mesh.vertices[indices[1]].position[3] << " "
                  << rotated_cube_mesh.vertices[indices[1]].color[0]    << " "
                  << rotated_cube_mesh.vertices[indices[1]].color[1]    << " "
                  << rotated_cube_mesh.vertices[indices[1]].color[2]    << "\n\n";
    }
    std::cout << std::flush;
    std::cout << (rotated_cube_mesh.vertices.data())->position[0] << std::endl;
    exit(0);*/
    }

    // black outline
    glEnable(GL_LINE_SMOOTH);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex)*rotated_cube_mesh.vertices.size(), rotated_cube_mesh.vertices.data(), GL_STATIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, line_indices.size() * sizeof(unsigned int), line_indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(vertex_array_object_);
    glDrawElements(GL_LINES, static_cast<unsigned int>(line_indices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

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


//    std::cout << "it should work" << std::endl;

/*    for (size_t i = 0; i < pieces_.size(); i++) {
        unsigned int piece_data = ( i ) |                       // piece index
                ( Colors::kBlack << 5 ) |                       // color index
                ( (int)pieces_[i].current_rotation << 8 );      // effected by current rotation
        glUniform1ui(piece_data_location_, piece_data);

        Mesh mesh = meshes_[pieces_[i].type];
        // send the vertex buffer objects
        glBufferData(GL_ARRAY_BUFFER, sizeof(float)*mesh.points.size(), mesh.points.data(), GL_STATIC_DRAW);

        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(int)*mesh.lines.size(), mesh.lines.data(), GL_STATIC_DRAW);
        glBindVertexArray(vertex_array_object_);

        glDrawElements(GL_LINES, mesh.lines.size(), GL_UNSIGNED_INT, 0);
    }
    glDisable(GL_LINE_SMOOTH);

    glEnable(GL_BLEND);
    // pieces
    for (size_t i = 0; i < pieces_.size(); i++) {
        Mesh mesh = meshes_[pieces_[i].type];
        // send the vertex buffer objects
        glBufferData(GL_ARRAY_BUFFER, sizeof(float)*mesh.points.size(), mesh.points.data(), GL_STATIC_DRAW);

        // go over all colors of this piece type
        for (int color_index = 0; color_index < int(pieces_[i].colors.size()); color_index++) {
            // convert the piece index, color index and effected by rotation of current move into one unsigned int
            unsigned int piece_data = ( i ) |                                       // piece index
                                      (   pieces_[i].colors[color_index] << 5 ) |   // color index
                                      ( (int)pieces_[i].current_rotation << 8 );    // effected by current rotation
            glUniform1ui(piece_data_location_, piece_data);

            // send the element buffer objects
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(int)*mesh.triangles[color_index].size(), mesh.triangles[color_index].data(), GL_STATIC_DRAW);

            glBindVertexArray(vertex_array_object_);

            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glDrawElements(GL_TRIANGLES, mesh.triangles[color_index].size(), GL_UNSIGNED_INT, 0);
        }
    }*/
    //glDisable(GL_BLEND);
}
