#pragma once

#include <string>
#include <utility>


#include "error_handler.h"


class Setting {
public:
    std::string rootPath;
    bool gui = true;

    // mouse rotation
    std::pair<float, float> rotation = {140, 30};
    std::pair<double, double> last_position = {0, 0};

    // scroll
    double scroll = 1;

    Setting(ErrorHandler error_handler, int argc, char *argv[]);
};
