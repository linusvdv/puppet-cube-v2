#pragma once

#include <string>

#include <glad/glad.h>


#include "error_handler.h"


class Shader {
public:
    // the program ID
    unsigned int ID;

    // constructor reads and builds the shader
    Shader(ErrorHandler error_handler, const char* vertex_path, const char* fragment_path);

    // use/activate the shader
    void Use() const {
        glUseProgram(ID);
    }
};
