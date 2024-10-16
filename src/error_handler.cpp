#include <array>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>

#include <GLFW/glfw3.h>


#include "error_handler.h"


const std::array<int, 7> kColor = {
    41,
    31,
    33,
    32,
    0,
    0,
    0
};


void ErrorHandler::Handle(Level level, std::string file, std::string message) const {
    if (level > error_level) {
        return;
    }

    // set color
    std::cout << "\033[" << kColor[level] << "m";

    std::time_t time = std::time(nullptr);
    std::cout << "[" << std::strtok(std::ctime(&time), "\n") << "] ";
    
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
        case Level::kExtra:
            std::cout << "Extra: ";
            break;
        case Level::kMemory:
            std::cout << "Memory: ";
            break;
    }

    if (level <= Level::kWarning) {
        std::cout << "in file: " << file << " --- ";
    }

    std::cout << message;

    std::cout << "\033[0m";

    std::cout << std::endl;

    if (level == Level::kCriticalError) {
        exit(-1);
    }
}
