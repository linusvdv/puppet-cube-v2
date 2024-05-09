#pragma once

#include <string>


class Setting {
public:
    std::string rootPath;
    bool gui = true;

    Setting(int argc, char *argv[]);
};
