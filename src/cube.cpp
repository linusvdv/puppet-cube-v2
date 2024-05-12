#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


#include "cube.h"
#include "shader.h"


void Cube::Draw(Shader our_shader) const {
    for (int i = 0; i < kNumEdges; i++) {
        for (int j = 0; j < 2; j++) {
            // vertex buffer objects
            unsigned int vbo;
            glGenBuffers(1, &vbo);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(edge_mesh_[i][j].points), edge_mesh_[i][j].points, GL_STATIC_DRAW);


            // vertex array object
            unsigned int vao;
            glGenVertexArrays(1, &vao);
            glBindVertexArray(vao);


            // element buffer objects
            unsigned int ebo;
            glGenBuffers(1, &ebo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(edge_mesh_[i][j].triangles), edge_mesh_[i][j].triangles, GL_STATIC_DRAW);


            // linking vertex attributes
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);

            // unbind
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);

            // rotation of pieces
            glm::mat4 trans = glm::mat4(1.0F);
            trans = glm::scale(trans, glm::vec3(0.5, 0.5, 0.5));
            trans = glm::rotate(trans, (float)glfwGetTime(), glm::vec3(0.0, 1.0, 0.0));
            trans = glm::rotate(trans, 0.0F, glm::vec3(0.0, 0.0, 1.0));
            trans = glm::rotate(trans, 0.0F, glm::vec3(0.0, 1.0, 0.0));
            trans = glm::rotate(trans, 0.0F, glm::vec3(1.0, 0.0, 0.0));

            unsigned int transform_loc = glGetUniformLocation(our_shader.ID, "transform");
            glUniformMatrix4fv(transform_loc, 1, GL_FALSE, glm::value_ptr(trans));

            unsigned int color_loc = glGetUniformLocation(our_shader.ID, "aColor");
            glUniform4f(color_loc, edge_mesh_[i][j].color[0], edge_mesh_[i][j].color[1], edge_mesh_[i][j].color[2], edge_mesh_[i][j].color[3]);


            glBindVertexArray(vao);

            // fill mode
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

            // Wireframe mode
            // glUniform4f(color_loc, 0, 0, 0, 0);
            // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            // glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

            glDeleteVertexArrays(1, &vao);
            glDeleteBuffers(1, &vbo);
            glDeleteBuffers(1, &ebo);
        }
    }
}
