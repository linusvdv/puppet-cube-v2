#pragma once
#include <string>


class Settings {
public:
    Settings(int argc, char *argv[]);

    // this path should be equivelant to path/to/puppet-cube-v2/
    static std::string root_path;

    // graphical user interface
    static bool gui;

    // search
    static int num_threads;
};
