#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>


#include "settings.h"
#include "shader.h"


static void ErrorCallback(int error, const char* description) {
    std::cerr << "ERROR: " << description << std::endl;
}


static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}


void FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}


int Renderer (Setting settings) {
    glfwSetErrorCallback(ErrorCallback);

    // initialising GLFW
    if (glfwInit() == 0) {
        return -1;
    }

    // create window with OpenGL 3.x or higher
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(640, 480, "Puppet Cube V2", NULL, NULL);
    if (window == nullptr) {
        glfwTerminate();
        return -1;
    }

    glfwSetKeyCallback(window, KeyCallback);
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);

    glfwMakeContextCurrent(window);


    // initialising GLAD
    if (gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0) {
        std::cout << "Faild to initialize GLAD" << std::endl;
        return -1;
    }


    std::string vertex_shader_path = "src/shader.vert";
    std::string fragment_shader_path = "src/shader.frag";
    vertex_shader_path.insert(0, settings.rootPath);
    fragment_shader_path.insert(0, settings.rootPath);
    Shader our_shader(vertex_shader_path.c_str(), fragment_shader_path.c_str());

    // drawing two triangle
    float vertices[] = {
        // position          colors
         0.5F,  0.5F,  0.0F, 1.0F, 0.0F, 0.0F,
         0.5F, -0.5F,  0.0F, 0.0F, 1.0F, 0.0F,
        -0.0F, -0.5F,  0.0F, 0.0F, 0.0F, 1.0F,
        -0.0F,  0.5F,  0.0F, 1.0F, 1.0F, 0.0F
    };
    unsigned int indices[] = {
        0, 1, 3,
        1, 2, 3
    };



    // vertex buffer objects
    unsigned int vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);


    // vertex array object
    unsigned int vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);


    // element buffer objects
    unsigned int ebo;
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);


    // linking vertex attributes
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);


    // Wireframe mode
    // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    while (glfwWindowShouldClose(window) == 0) {
        // clear background
        glClearColor(0.2F, 0.3F, 0.3F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);

        our_shader.Use();
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);

        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
