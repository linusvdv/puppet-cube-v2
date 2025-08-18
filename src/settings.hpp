#pragma once
#include <string>


class Settings {
public:
    Settings(int argc, char *argv[]);

    // this path should be equivalent to path/to/puppet-cube-v2/
    static std::string root_path;

    // performance testing for optimisation purposes
    static bool should_peformance_test;

    // search
    static int num_threads;

    // tb_depth
    // tb_depth_gpu
    static int tb_depth;
    static int tb_depth_gpu;
};
