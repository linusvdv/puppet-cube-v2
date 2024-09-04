#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <ostream>


#include "renderer.h"
#include "error_handler.h"
#include "settings.h"
#include "shader.h"
#include "actions.h"


static void ErrorCallback (int error, const char* description) {
    // error message
    std::cout << "\033[41m";
    std::time_t time = std::time(nullptr);
    std::cout << "[" << std::strtok(std::ctime(&time), "\n") << "] ";
    std::cout << "CRITICAL ERROR: in file window_manager.cpp --- " << error << " " << description;
    std::cout << "\033[0m" << std::endl;
    exit(-1);
}


static void KeyCallback (GLFWwindow* window, int key, int scancode [[maybe_unused]], int action, int mods [[maybe_unused]]) {
    Setting* settings = (Setting *)glfwGetWindowUserPointer(window);
    // close window
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    // pause / resume
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
        settings->should_rotate = !settings->should_rotate;
    }
}


static void CursorPosCallback (GLFWwindow* window, double xpos, double ypos) {
    Setting* settings = (Setting *)glfwGetWindowUserPointer(window);
    const float full_rotation = 360;

    // rotation of cube
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


static void ScrollCallback (GLFWwindow* window, double xoffset [[maybe_unused]], double yoffset) {
    Setting* settings = (Setting *)glfwGetWindowUserPointer(window);

    // soom
    settings->soom *= 1+yoffset/6;

    // max soom factor
    if (settings->soom > 4) {
        settings->soom = 4;
    }
    else if (settings->soom < 1.F/4) {
        settings->soom = 1.F/4;
    }
}


// resize of window
void FramebufferSizeCallback (GLFWwindow* window [[maybe_unused]], int width, int height) {
    int square = std::min(width, height);
    glViewport((width - square)/2, (height - square)/2, square, square);
}


void WindowManager (ErrorHandler error_handler, Setting settings, Actions& actions) {
    glfwSetErrorCallback(ErrorCallback);

    // initialising GLFW
    if (glfwInit() == 0) {
        error_handler.Handle(ErrorHandler::Level::kCriticalError, "window_manager.cpp", "glfw initialization failed");
    }

    // create window with OpenGL 3.x or higher
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    GLFWwindow* window = glfwCreateWindow(640, 640, "Puppet Cube V2", NULL, NULL);
    if (window == nullptr) {
        error_handler.Handle(ErrorHandler::Level::kCriticalError, "window_manager.cpp", "glfw creation of window failed");
    }

    // access settings in callbacks
    glfwSetWindowUserPointer(window, (void *)&settings);

    glfwSetKeyCallback(window, KeyCallback);
    glfwSetScrollCallback(window, ScrollCallback);
    glfwSetCursorPosCallback(window, CursorPosCallback);
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);

    glfwMakeContextCurrent(window);


    // initialising GLAD
    if (gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0) {
        error_handler.Handle(ErrorHandler::Level::kCriticalError, "window_manager.cpp", "glad initialization failed");
    }


    // shader path
    std::string vertex_shader_path = "src/shader.vert";
    std::string fragment_shader_path = "src/shader.frag";
    vertex_shader_path.insert(0, settings.rootPath);
    fragment_shader_path.insert(0, settings.rootPath);

    // initialise shader
    Shader shader(error_handler, vertex_shader_path.c_str(), fragment_shader_path.c_str());

    // neded for transparence
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // draw ontop of further objects
    glEnable(GL_DEPTH_TEST);
    shader.Use();

    // rendering
    Renderer renderer;

    while (glfwWindowShouldClose(window) == 0) {
        // clear background
        glClearColor(0.2F, 0.3F, 0.3F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // rotate the model
        renderer.Rotate(settings, actions);
        // render the model
        renderer.Draw(settings);

        // display the current buffer
        glfwSwapBuffers(window);

        // get inputs
        glfwPollEvents();
    }

    // stop the program
    actions.stop = true;
    glfwTerminate();
}
