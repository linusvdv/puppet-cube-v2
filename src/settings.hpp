#pragma once
#include <cstddef>
#include <cstdint>
#include <string>


constexpr size_t kBlockDim = 32;
constexpr int kMaxDFSDepth = 6;


class Settings {
public:
    Settings(int argc, char *argv[]);

    static std::string GetRootPath() {
        return root_path;
    }

    static bool GetTestBCHT() {
        return test_bcht;
    }

    static bool GetTestDFS() {
        return test_dfs;
    }

    static int GetDFSDepth() {
        return dfs_depth;
    }

    static int GetScramblingDepth() {
        return scrambling_depth;
    }

    static int GetNumThreads() {
        return num_threads;
    }

    static size_t GetNumPositions() {
        return num_positions;
    }

    static size_t GetNumRuns() {
        return num_runs;
    }

    static uint8_t GetTBDepth() {
        return tb_depth;
    }

    static int GetTBDepthGPU() {
        return tb_depth_gpu;
    }

    static size_t GetNumDFSPositions() {
        return num_dfs_positions;
    }

    static bool GetLogInfo() {
        return log_info;
    }

    static bool UseCuda() {
        return use_cuda;
    }


private:
    // this path should be equivalent to path/to/puppet-cube-v2/
    static std::string root_path;
    static bool use_cuda;

    static bool log_info;

    // performance testing for optimisation purposes
    static bool test_bcht;
    static bool test_dfs;

    static int dfs_depth;
    static int scrambling_depth;

    // search
    static int num_threads;
    static size_t num_positions;
    static size_t num_runs;

    // tb_depth
    // tb_depth_gpu
    static int tb_depth;
    static int tb_depth_gpu;
    static size_t num_dfs_positions;
};
