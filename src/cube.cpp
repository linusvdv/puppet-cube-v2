#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cmath>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


#include "cube.h"
#include "shader.h"


void Cube::MeshInitialisation() {
    // corner two directions facing out
    int current_triangle_size = meshes_[4].points.size() / 3;
    const int num_edges_radius = 20; // multible of two
    const float begin_angle = asin(1 / (sqrt(2) * 3));
    const float end_angle = asin(1 / sqrt(2));
    const float length = -0.2*sqrt(2)*3;

    for (int i = 0; i < num_edges_radius/2; i++) {
        float angle = begin_angle + (end_angle - begin_angle) / (num_edges_radius/2) * i;

        // left bottom
        meshes_[4].points.insert(meshes_[4].points.end(),
                {float(length * cos(angle)), -0.2, float(length * sin(angle))});
        // left top 
        meshes_[4].points.insert(meshes_[4].points.end(),
                {float(length * cos(angle)),  0.2, float(length * sin(angle))});
        // right bottom
        meshes_[4].points.insert(meshes_[4].points.end(),
                {float(length * sin(angle)), -0.2, float(length * cos(angle))});
        // right top
        meshes_[4].points.insert(meshes_[4].points.end(),
                {float(length * sin(angle)),  0.2, float(length * cos(angle))});

        if (i == 0) {
            continue;
        }

        meshes_[4].triangles[1].push_back(current_triangle_size + i*4 + 2);
        meshes_[4].triangles[1].push_back(current_triangle_size + i*4 + 3);
        meshes_[4].triangles[1].push_back(current_triangle_size + i*4 - 1);

        meshes_[4].triangles[1].push_back(current_triangle_size + i*4 + 2);
        meshes_[4].triangles[1].push_back(current_triangle_size + i*4 - 2);
        meshes_[4].triangles[1].push_back(current_triangle_size + i*4 - 1);

        meshes_[4].triangles[1].push_back(current_triangle_size + i*4 + 3);
        meshes_[4].triangles[1].push_back(current_triangle_size + i*4 - 1);
        meshes_[4].triangles[1].push_back(7);

        meshes_[4].triangles[1].push_back(current_triangle_size + i*4 + 2);
        meshes_[4].triangles[1].push_back(current_triangle_size + i*4 - 2);
        meshes_[4].triangles[1].push_back(5);


        meshes_[4].triangles[2].push_back(current_triangle_size + i*4 + 0);
        meshes_[4].triangles[2].push_back(current_triangle_size + i*4 + 1);
        meshes_[4].triangles[2].push_back(current_triangle_size + i*4 - 3);

        meshes_[4].triangles[2].push_back(current_triangle_size + i*4 + 0);
        meshes_[4].triangles[2].push_back(current_triangle_size + i*4 - 4);
        meshes_[4].triangles[2].push_back(current_triangle_size + i*4 - 3);

        meshes_[4].triangles[2].push_back(current_triangle_size + i*4 + 1);
        meshes_[4].triangles[2].push_back(current_triangle_size + i*4 - 3);
        meshes_[4].triangles[2].push_back(7);

        meshes_[4].triangles[2].push_back(current_triangle_size + i*4 + 0);
        meshes_[4].triangles[2].push_back(current_triangle_size + i*4 - 4);
        meshes_[4].triangles[2].push_back(5);


        meshes_[4].lines.push_back(current_triangle_size + i*4 - 4);
        meshes_[4].lines.push_back(current_triangle_size + i*4 + 0);
        meshes_[4].lines.push_back(current_triangle_size + i*4 - 2);
        meshes_[4].lines.push_back(current_triangle_size + i*4 + 2);
    }

    // middle
    meshes_[4].points.insert(meshes_[4].points.end(),
            {float(length * cos(end_angle)), -0.2, float(length * sin(end_angle))});
    meshes_[4].points.insert(meshes_[4].points.end(),
            {float(length * cos(end_angle)), 0.2, float(length * sin(end_angle))});

    meshes_[4].triangles[1].push_back(current_triangle_size + num_edges_radius/2*4 + 0);
    meshes_[4].triangles[1].push_back(current_triangle_size + num_edges_radius/2*4 + 1);
    meshes_[4].triangles[1].push_back(current_triangle_size + num_edges_radius/2*4 - 1);

    meshes_[4].triangles[1].push_back(current_triangle_size + num_edges_radius/2*4 + 0);
    meshes_[4].triangles[1].push_back(current_triangle_size + num_edges_radius/2*4 - 2);
    meshes_[4].triangles[1].push_back(current_triangle_size + num_edges_radius/2*4 - 1);

    meshes_[4].triangles[1].push_back(current_triangle_size + num_edges_radius/2*4 + 1);
    meshes_[4].triangles[1].push_back(current_triangle_size + num_edges_radius/2*4 - 1);
    meshes_[4].triangles[1].push_back(7);

    meshes_[4].triangles[1].push_back(current_triangle_size + num_edges_radius/2*4 + 0);
    meshes_[4].triangles[1].push_back(current_triangle_size + num_edges_radius/2*4 - 2);
    meshes_[4].triangles[1].push_back(5);


    meshes_[4].triangles[2].push_back(current_triangle_size + num_edges_radius/2*4 + 0);
    meshes_[4].triangles[2].push_back(current_triangle_size + num_edges_radius/2*4 + 1);
    meshes_[4].triangles[2].push_back(current_triangle_size + num_edges_radius/2*4 - 3);

    meshes_[4].triangles[2].push_back(current_triangle_size + num_edges_radius/2*4 + 0);
    meshes_[4].triangles[2].push_back(current_triangle_size + num_edges_radius/2*4 - 4);
    meshes_[4].triangles[2].push_back(current_triangle_size + num_edges_radius/2*4 - 3);

    meshes_[4].triangles[2].push_back(current_triangle_size + num_edges_radius/2*4 + 1);
    meshes_[4].triangles[2].push_back(current_triangle_size + num_edges_radius/2*4 - 3);
    meshes_[4].triangles[2].push_back(7);

    meshes_[4].triangles[2].push_back(current_triangle_size + num_edges_radius/2*4 + 0);
    meshes_[4].triangles[2].push_back(current_triangle_size + num_edges_radius/2*4 - 4);
    meshes_[4].triangles[2].push_back(5);

    meshes_[4].lines.push_back(current_triangle_size + num_edges_radius/2*4 - 4);
    meshes_[4].lines.push_back(current_triangle_size + num_edges_radius/2*4 + 0);
    meshes_[4].lines.push_back(current_triangle_size + num_edges_radius/2*4 - 2);
    meshes_[4].lines.push_back(current_triangle_size + num_edges_radius/2*4 + 0);
}


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
    glBindVertexArray(0);


    // piece data location
    piece_data_location_ = glGetUniformLocation(shader.ID, "piece_data");

    // colors
    unsigned int colors_index = glGetUniformLocation(shader.ID, "colors");
    glUniform4fv(colors_index, kNumColors, *colors_);

    glm::mat4 rotations[kNumPieces];
    for (int i = 0; i < kNumPieces; i++) {
        rotations[i] = pieces_[i].rotation;
    }
    rotations_location_ = glGetUniformLocation(shader.ID, "rotations");
    glUniformMatrix4fv(rotations_location_, kNumPieces, GL_FALSE, glm::value_ptr(rotations[0]));

    MeshInitialisation();
}



Cube::~Cube() {
    // glDeleteVertexArrays(1, &vertex_array_object_);
    // glDeleteBuffers(1, &vertex_buffer_objects_);
    // glDeleteBuffers(1, &element_buffer_object_);
}



void Cube::Draw(Shader shader) const {
    for (int i = 0; i < pieces_.size(); i++) {

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
