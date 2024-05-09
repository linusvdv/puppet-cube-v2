#pragma once


#include <string>

#include <glad/glad.h>


class Shader {
public:
    // the program ID
    unsigned int ID;

    // constructor reads and builds the shader
    Shader(const char* vertex_path, const char* fragment_path);

    // use/activate the shader
    void Use() const {
        glUseProgram(ID);
    }

    // utility uniform functions
    void SetBool(const std::string &name, bool value) const {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
    }
    void SetInt(const std::string &name, int value) const {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
    }
    void SetFloat(const std::string &name, float value) const {
        glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
    }
};
