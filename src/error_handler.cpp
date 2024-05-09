#include <cstdlib>
#include <iostream>
#include <string>

#include <GLFW/glfw3.h>


#include "error_handler.h"


void ErrorHandler::Handle(Level level, std::string file, std::string message) const {
    if (level > error_level) {
        return;
    }
    
    switch (level) {
        case Level::kCriticalError:
            std::cout << "CRITICAL ERROR: ";
            break;
        case Level::kError:
            std::cout << "ERROR: ";
            break;
        case Level::kWarning:
            std::cout << "WARNING: ";
            break;
        case Level::kInfo:
            std::cout << "INFO: ";
            break;
        case Level::kAll:
            break;
    }

    if (level <= Level::kWarning) {
        std::cout << "in file: " << file << " --- ";
    }

    std::cout << message << std::endl;

    if (level == Level::kCriticalError) {
        glfwTerminate();
        exit(-1);
    }
}
