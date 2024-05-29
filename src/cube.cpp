#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cmath>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>


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

    MeshInitialisationOneCorner();
    MeshInitialisationTwoCorners();
}


Cube::~Cube() {
    // glDeleteVertexArrays(1, &vertex_array_object_);
    // glDeleteBuffers(1, &vertex_buffer_objects_);
    // glDeleteBuffers(1, &element_buffer_object_);
}


void Cube::Draw(Shader shader) const {
    // black outline
    glEnable(GL_LINE_SMOOTH);
    for (int i = 0; i < pieces_.size(); i++) {
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
    }
    glDisable(GL_BLEND);
}


void Cube::MeshInitialisationOneCorner() {
    // corner one direction facing out
    int current_triangle_size = meshes_[3].points.size() / 3;
    const int num_edges_radius_lower = 8; // from area to 45 degree yellow
    const int num_edges_radius_middle= 8; // from area to 45 degree green
    const float begin_angle = atan(1 / 1);
    const float middle_angle = acos((1+2*sqrt(2)) / (sqrt(2)*3)); // 45 degree from corner
    const float end_angle = asin(1 / (sqrt(2)*3)); // edge
    const float length = 0.2*sqrt(2)*3;

    for (int i = 0; i < num_edges_radius_lower; i++) {
        float angle = begin_angle + (middle_angle - begin_angle) / (num_edges_radius_lower) * (i+1);

        // points
        meshes_[3].points.insert(meshes_[3].points.end(),
                {-float(length * cos(angle)), float(length * sin(angle)), -0.2, // outer yellow
                 -float(length * cos(angle)), float(length * sin(angle)),  0.2, // inner
                 -float(length * cos(angle)), -0.2, float(length * sin(angle)), // outer green
                 -float(length * cos(angle)),  0.2, float(length * sin(angle))}); // inner

        // triangles
        int last_batch = current_triangle_size + i*4;
        meshes_[3].triangles[0].insert(meshes_[3].triangles[0].end(), // yellow
                {last_batch + 0, last_batch - 4, 3, // outer
                 last_batch + 0, last_batch + 1, last_batch - 4, // smooth
                 last_batch + 1, last_batch - 4, last_batch - 3, // smooth
                 last_batch + 1, last_batch - 3, 8}); // inner
                                                      //
        meshes_[3].triangles[2].insert(meshes_[3].triangles[2].end(), // orange
                {last_batch + 2, last_batch - 2, 6, // outer
                 last_batch + 2, last_batch + 3, last_batch - 2, // smooth
                 last_batch + 3, last_batch - 2, last_batch - 1, // smooth
                 last_batch + 3, last_batch - 1, 8}); // inner

        // lines
        meshes_[3].lines.insert(meshes_[3].lines.end(),
                {last_batch + 0, last_batch - 4,
                 last_batch + 2, last_batch - 2});
    }

    current_triangle_size = meshes_[3].points.size() / 3;
    for (int i = 0; i < num_edges_radius_middle; i++) {
        float angle = middle_angle + (end_angle - middle_angle) / (num_edges_radius_middle) * (i+1);

        // points
        meshes_[3].points.insert(meshes_[3].points.end(),
                {-float(length * cos(angle)), float(length * sin(angle)), -0.2, // outer yellow
                 -float(length * cos(angle)), float(length * sin(angle)),  0.2, // inner
                 -float(length * cos(angle)), -0.2, float(length * sin(angle)), // outer green
                 -float(length * cos(angle)),  0.2, float(length * sin(angle))}); // inner

        // triangles
        int last_batch = current_triangle_size + i*4;
        meshes_[3].triangles[1].insert(meshes_[3].triangles[1].end(), // yellow side
                {last_batch + 0, last_batch - 4, 3, // outer
                 last_batch + 0, last_batch + 1, last_batch - 4, // smooth
                 last_batch + 1, last_batch - 4, last_batch - 3, // smooth
                 last_batch + 1, last_batch - 3, 8, // inner
                 // orange side
                 last_batch + 2, last_batch - 2, 6, // outer
                 last_batch + 2, last_batch + 3, last_batch - 2, // smooth
                 last_batch + 3, last_batch - 2, last_batch - 1, // smooth
                 last_batch + 3, last_batch - 1, 8}); // inner

        // lines
        meshes_[3].lines.insert(meshes_[3].lines.end(),
                {last_batch + 0, last_batch - 4,
                 last_batch + 2, last_batch - 2});
    }
    current_triangle_size = meshes_[3].points.size() / 3;
    meshes_[3].triangles[1].insert(meshes_[3].triangles[1].end(),
            {current_triangle_size - 4, 3, 7,
             current_triangle_size - 2, 6, 7});

    current_triangle_size = meshes_[3].points.size() / 3;
    const int num_square = 8;
    for (int i = 0; i <= num_square; i++) { // both sides inclusive
        float angle_i = end_angle - (2 * end_angle) / num_square * i;
        for (int j = 0; j <= num_square; j++) {
            float angle_j = end_angle - (2 * end_angle) / num_square * j;
            meshes_[3].points.insert(meshes_[3].points.end(),
                    {-float(length * std::max(cos(angle_i), cos(angle_j))), length * sin(angle_j), length * sin(angle_i)});

            // area
            int last_batch = meshes_[3].points.size() / 3 - 1;
            if (j != 0 && i != 0) {
                meshes_[3].triangles[1].insert(meshes_[3].triangles[1].end(),
                        {last_batch, last_batch - 1, last_batch - num_square-1,
                         last_batch - 1, last_batch - num_square-1, last_batch - num_square-2});
            }
            
            // outer
            if (j == num_square && i != 0) {
                meshes_[3].triangles[1].insert(meshes_[3].triangles[1].end(),
                        {last_batch, last_batch - num_square-1, 7});
                meshes_[3].lines.insert(meshes_[3].lines.end(),
                        {last_batch, last_batch - num_square-1});
            }
            if (i == num_square && j != 0) { 
                meshes_[3].triangles[1].insert(meshes_[3].triangles[1].end(),
                        {last_batch, last_batch - 1, 7});
                meshes_[3].lines.insert(meshes_[3].lines.end(),
                        {last_batch, last_batch - 1});
            }

            // inner
            if (j == 0 && i > 1) {
                meshes_[3].triangles[1].insert(meshes_[3].triangles[1].end(),
                        {last_batch, last_batch - num_square-1, current_triangle_size});
            }
            if (i == 0 && j > 1) { 
                meshes_[3].triangles[1].insert(meshes_[3].triangles[1].end(),
                        {last_batch, last_batch - 1, current_triangle_size});
            }
        }
    }

    int last_batch = meshes_[3].points.size() / 3 - 1;
    meshes_[3].lines.insert(meshes_[3].lines.end(),
            {last_batch, 7});
}


void Cube::MeshInitialisationTwoCorners() {
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

        int last_batch = current_triangle_size + i*4;
        meshes_[4].triangles[1].insert(meshes_[4].triangles[1].end(),
                {last_batch + 2, last_batch + 3, last_batch - 1,
                 last_batch + 2, last_batch - 2, last_batch - 1,
                 last_batch + 3, last_batch - 1, 7,
                 last_batch + 2, last_batch - 2, 5});

        meshes_[4].triangles[2].insert(meshes_[4].triangles[2].end(),
                {last_batch + 0, last_batch + 1, last_batch - 3,
                 last_batch + 0, last_batch - 4, last_batch - 3,
                 last_batch + 1, last_batch - 3, 7,
                 last_batch + 0, last_batch - 4, 5});

        meshes_[4].lines.insert(meshes_[4].lines.end(),
                {last_batch - 4, last_batch + 0, 
                 last_batch - 2, last_batch + 2,});
    }

    // middle
    meshes_[4].points.insert(meshes_[4].points.end(),
            {float(length * cos(end_angle)), -0.2, float(length * sin(end_angle))});
    meshes_[4].points.insert(meshes_[4].points.end(),
            {float(length * cos(end_angle)), 0.2, float(length * sin(end_angle))});

    int last_batch = current_triangle_size + num_edges_radius/2*4;
    meshes_[4].triangles[1].insert(meshes_[4].triangles[1].end(),
            {last_batch + 0, last_batch + 1, last_batch - 1,
             last_batch + 0, last_batch - 2, last_batch - 1,
             last_batch + 1, last_batch - 1, 7,
             last_batch + 0, last_batch - 2, 5});

    meshes_[4].triangles[2].insert(meshes_[4].triangles[2].end(),
            {last_batch + 0, last_batch + 1, last_batch - 3,
             last_batch + 0, last_batch - 4, last_batch - 3,
             last_batch + 1, last_batch - 3, 7,
             last_batch + 0, last_batch - 4, 5});

    meshes_[4].lines.insert(meshes_[4].lines.end(),
            {last_batch - 4, last_batch + 0, 
             last_batch - 2, last_batch + 0,});
}
