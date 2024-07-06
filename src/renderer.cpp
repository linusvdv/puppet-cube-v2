#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <ostream>


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


static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    Setting* settings = (Setting *)glfwGetWindowUserPointer(window);
    const float full_rotation = 360;

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        float rotation_x_offset = xpos - settings->last_position.first;
        float rotation_y_offset = ypos - settings->last_position.second;

        settings->rotation.first += rotation_x_offset * (((settings->rotation.second > full_rotation/4) ||
                                                         (settings->rotation.second < -full_rotation/4)) ? -1 : 1);
        if (settings->rotation.first > full_rotation/2) {
            settings->rotation.first -= full_rotation;
        }
        else if (settings->rotation.first < -full_rotation/2) {
            settings->rotation.first += full_rotation;
        }

        settings->rotation.second += rotation_y_offset;
        if (settings->rotation.second > full_rotation/2) {
            settings->rotation.second -= full_rotation;
        }
        else if (settings->rotation.second < -full_rotation/2) {
            settings->rotation.second += full_rotation;
        }
    }

    settings->last_position = {xpos, ypos};
}


static void ScrollCallback(GLFWwindow* window, double xoffset [[maybe_unused]], double yoffset) {
    Setting* settings = (Setting *)glfwGetWindowUserPointer(window);

    settings->scroll *= 1+yoffset/6;

    if (settings->scroll > 4) {
        settings->scroll = 4;
    }
    if (settings->scroll < 1.F/4) {
        settings->scroll = 1.F/4;
    }
}


void FramebufferSizeCallback(GLFWwindow* window [[maybe_unused]], int width, int height) {
    int square = std::min(width, height);
    glViewport((width - square)/2, (height - square)/2, square, square);
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

    glfwSetWindowUserPointer(window, (void *)&settings);

    glfwSetKeyCallback(window, KeyCallback);
    glfwSetScrollCallback(window, ScrollCallback);
    glfwSetCursorPosCallback(window, CursorPosCallback);
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

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    shader.Use();


    Cube cube(shader);

    while (glfwWindowShouldClose(window) == 0) {
        // clear background
        glClearColor(0.2F, 0.3F, 0.3F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        cube.Draw(settings);

        glfwSwapBuffers(window);

        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
