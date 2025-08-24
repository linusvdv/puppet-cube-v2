#pragma once
#include <string>


constexpr size_t kBlockDim = 128;


class Settings {
public:
    Settings(int argc, char *argv[]);

    static std::string GetRootPath() {
        return root_path;
    }

    static bool GetShouldPerformanceTest() {
        return should_performance_test;
    }

    static int GetNumThreads() {
        return num_threads;
    }

    static int GetTBDepth() {
        return tb_depth;
    }

    static int GetTBDepthGPU() {
        return tb_depth_gpu;
    }

private:
    // this path should be equivalent to path/to/puppet-cube-v2/
    static std::string root_path;

    // performance testing for optimisation purposes
    static bool should_performance_test;

    // search
    static int num_threads;

    // tb_depth
    // tb_depth_gpu
    static int tb_depth;
    static int tb_depth_gpu;
};
