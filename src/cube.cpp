#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


#include "cube.h"
#include "shader.h"


Cube::Cube(Shader shader) {
    // OpenGL objects
    glGenVertexArrays(1, &vertex_array_object_);
    glBindVertexArray(vertex_array_object_);

    glGenBuffers(1, &vertex_buffer_objects_);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_objects_);

    glGenBuffers(1, &element_buffer_object_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_buffer_object_);

    // linking vertex attributes
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // unbind
    // glBindBuffer(GL_ARRAY_BUFFER, 0);
    // glBindVertexArray(0);


    // colors
    glUniform3fv(glGetUniformLocation(shader.ID, "colors"), 6, *colors_);

    piece_data_location_ = glGetUniformLocation(shader.ID, "piece_data");


    unsigned int colors_index = glGetUniformLocation(shader.ID, "colors");
    glUniform4fv(colors_index, kNumColors, *colors_);

    glm::mat4 rotations[kNumPieces];
    for (int i = 0; i < kNumPieces; i++) {
        rotations[i] = pieces_[i].rotation;
    }
    rotations_location_ = glGetUniformLocation(shader.ID, "rotations");
    glUniformMatrix4fv(rotations_location_, kNumPieces, GL_FALSE, glm::value_ptr(rotations[0]));
    // (current_rotation_axis)
    // (view_rotation)
}


/*
Cube::~Cube() {
    glDeleteVertexArrays(1, &vertex_array_object_);
    glDeleteBuffers(1, &vertex_buffer_objects_);
    glDeleteBuffers(1, &element_buffer_object_);
}
*/


void Cube::Draw(Shader shader) const {
    for (int i = 0; i < pieces_.size(); i++) {
        if (pieces_[i].type > 1) { // TODO: corners
            continue;
        }

        Mesh mesh = meshes_[pieces_[i].type];

        // send the vertex buffer objects
        glBufferData(GL_ARRAY_BUFFER, sizeof(float)*mesh.points.size(), mesh.points.data(), GL_STATIC_DRAW);

        // go over all colors of this piece type
        for (int color_index = 0; color_index < pieces_[i].colors.size(); color_index++) {
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


        unsigned int piece_data = ( i ) |                       // piece index
                ( Colors::kBlack << 5 ) |                       // color index
                ( (int)pieces_[i].current_rotation << 8 );      // effected by current rotation
        glUniform1ui(piece_data_location_, piece_data);

        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(int)*mesh.lines.size(), mesh.lines.data(), GL_STATIC_DRAW);
        glBindVertexArray(vertex_array_object_);

        glDrawElements(GL_LINES, mesh.lines.size(), GL_UNSIGNED_INT, 0);
    }
}
