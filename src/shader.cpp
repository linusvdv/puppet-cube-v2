#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <glad/glad.h>


#include "error_handler.h"
#include "shader.h"


// get error of shader compilation
void CheckCompileErrors(ErrorHandler error_handler, unsigned int shader, std::string type) {
    const int log_length = 1024;
    int success;
    char info_log[log_length];
    // compile the program
    if (type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (success == 0) {
            glGetShaderInfoLog(shader, log_length, NULL, info_log);
            error_handler.Handle(ErrorHandler::Level::kCriticalError, "shader.cpp", "shader compilation of type " + type + " failed: " + std::string(info_log));
        }
        error_handler.Handle(ErrorHandler::kInfo, "shader.cpp", type + " shader compiled");
    }
    // link the program
    else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (success == 0) {
            glGetShaderInfoLog(shader, log_length, NULL, info_log);
            error_handler.Handle(ErrorHandler::Level::kCriticalError, "shader.cpp", "shader linking of type " + type + " failed: " + std::string(info_log));
        }
        error_handler.Handle(ErrorHandler::kInfo, "shader.cpp", type + " shader linked");
    }
}


Shader::Shader(ErrorHandler error_handler, const char* vertex_path, const char* fragment_path) {
    // retrieve the vertex/fragment source code from filePath
    std::string vertex_code;
    std::string fragment_code;
    std::ifstream vertex_shader_file;
    std::ifstream fragment_shader_file;
    // ensure ifstream objects can throw exceptions:
    vertex_shader_file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    fragment_shader_file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try {
        vertex_shader_file.open(vertex_path);
        fragment_shader_file.open(fragment_path);

        std::stringstream vertex_shader_stream;
        std::stringstream fragment_shader_stream;

        vertex_shader_stream << vertex_shader_file.rdbuf();
        fragment_shader_stream << fragment_shader_file.rdbuf();

        vertex_code = vertex_shader_stream.str();
        fragment_code = fragment_shader_stream.str();
    }
    catch (std::ifstream::failure &exception) {
        error_handler.Handle(ErrorHandler::Level::kCriticalError, "shader.cpp", "faild to load shader: " + std::string(exception.what()));
    }
    const char* vertex_shader_code = vertex_code.c_str();
    const char* fragment_shader_code = fragment_code.c_str();


    // compile shader
    unsigned int vertex;
    unsigned int fragment;
    // vertex shader
    vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vertex_shader_code, NULL);
    glCompileShader(vertex);
    CheckCompileErrors(error_handler, vertex, "VERTEX");
    // fragment shader
    fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fragment_shader_code, NULL);
    glCompileShader(fragment);
    CheckCompileErrors(error_handler, fragment, "FRAGMENT");
    // shader Program
    ID = glCreateProgram();
    glAttachShader(ID, vertex);
    glAttachShader(ID, fragment);
    glLinkProgram(ID);
    CheckCompileErrors(error_handler, ID, "PROGRAM");
    // delete unnecessary shaders
    glDeleteShader(vertex);
    glDeleteShader(fragment);
};
