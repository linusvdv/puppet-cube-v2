#pragma once

#include <string>


#include "error_handler.h"


class Setting {
public:
    std::string rootPath;
    bool gui = true;

    Setting(ErrorHandler error_handler, int argc, char *argv[]);
};
