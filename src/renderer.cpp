#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


#include "cube.h"
#include "error_handler.h"
#include "settings.h"
#include "shader.h"


static void ErrorCallback(int error, const char* description) {
    std::cout << "ERROR: in file rendere.cpp --- " << error << " " << description << std::endl;
}


static void KeyCallback(GLFWwindow* window, int key, int scancode [[maybe_unused]], int action, int mods [[maybe_unused]]) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}


void FramebufferSizeCallback(GLFWwindow* window [[maybe_unused]], int width, int height) {
    glViewport(0, 0, std::min(width, height), std::min(width, height));
}


int Renderer (ErrorHandler error_handler, Setting settings) {
    glfwSetErrorCallback(ErrorCallback);

    // initialising GLFW
    if (glfwInit() == 0) {
        error_handler.Handle(ErrorHandler::Level::kCriticalError, "renderer.cpp", "glfw initialization failed");
    }

    // create window with OpenGL 3.x or higher
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(640, 640, "Puppet Cube V2", NULL, NULL);
    if (window == nullptr) {
        error_handler.Handle(ErrorHandler::Level::kCriticalError, "renderer.cpp", "glfw creation of window failed");
    }

    glfwSetKeyCallback(window, KeyCallback);
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);

    glfwMakeContextCurrent(window);


    // initialising GLAD
    if (gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0) {
        error_handler.Handle(ErrorHandler::Level::kCriticalError, "renderer.cpp", "glad initialization failed");
    }


    std::string vertex_shader_path = "src/shader.vert";
    std::string fragment_shader_path = "src/shader.frag";
    vertex_shader_path.insert(0, settings.rootPath);
    fragment_shader_path.insert(0, settings.rootPath);

    Shader shader(error_handler, vertex_shader_path.c_str(), fragment_shader_path.c_str());

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LINE_SMOOTH);
    shader.Use();


    Cube cube(shader);

    while (glfwWindowShouldClose(window) == 0) {
        // clear background
        glClearColor(0.2F, 0.3F, 0.3F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // rotation
        glm::mat4 trans = glm::mat4(1.0F);
        trans = glm::rotate(trans, (float)glfwGetTime(), glm::vec3(0.0, 1.0, 0.0));

        unsigned int transform_loc = glGetUniformLocation(shader.ID, "current_rotation_axis");
        glUniformMatrix4fv(transform_loc, 1, GL_FALSE, glm::value_ptr(trans));

        // view
        glm::mat4 view_rotation = glm::mat4(1.0F);
        view_rotation = glm::scale(view_rotation, glm::vec3(0.5, 0.5, 0.5));
        view_rotation = glm::rotate(view_rotation, glm::radians( 30.0F), glm::vec3(1.0, 0.0, 0.0)); // second
        view_rotation = glm::rotate(view_rotation, glm::radians(-40.0F), glm::vec3(0.0, 1.0, 0.0)); // first
        unsigned int view_rotation_loc = glGetUniformLocation(shader.ID, "view_rotation");
        glUniformMatrix4fv(view_rotation_loc, 1, GL_FALSE, glm::value_ptr(view_rotation));

        cube.Draw(shader);

        glfwSwapBuffers(window);

        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
