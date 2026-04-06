#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>


constexpr size_t kBlockDim = 64;
constexpr int kMaxDFSDepth = 6;


class Settings {
private:
    // general informations
    // this path should be equivalent to path/to/puppet-cube-v2/
    static std::string root_path;
    static bool use_cuda;
    static int device_count;
    static bool hardware_info;

    // search starting position
    static size_t num_runs;
    static size_t run_offset;
    static int scrambling_depth;
    static int min_corner_heuristic;

    // search
    // cpu parallel threads
    static int num_threads;
    // cpu parallel threads for handling gpus
    static int num_gpu_upload_threads;
    // gpu threads per kernal launch
    static int num_gputhreads;
    // batch size for each transfer
    static int num_positions_per_batch;
    // max parallel transfer batches (size of queue)
    static int num_parallel_batches;
    // size of the transposition table in MB
    static int tt_size;

    // tablebase
    static int tb_depth;

    // performance testing for optimisation purposes
    static bool test_bcht;

    static bool test_dfs;
    static int dfs_depth;
    static size_t num_dfs_positions;

    static void SetDefault(std::vector<std::string>& arguments);


public:
    Settings(int argc, char *argv[]);

    // general informations
    static std::string GetRootPath() {
        return root_path;
    }
    static bool UseCuda() {
        return use_cuda;
    }
    static int GetDeviceCount() {
        return device_count;
    }
    static bool GetHardwareInfo() {
        return hardware_info;
    }

    // search starting position
    static size_t GetNumRuns() {
        return num_runs;
    }
    static size_t GetRunOffset() {
        return run_offset;
    }
    static int GetScramblingDepth() {
        return scrambling_depth;
    }
    static int GetMinCornerHeuristic() {
        return min_corner_heuristic;
    }

    // search
    static int GetNumThreads() {
        return num_threads;
    }
    static int GetNumGPUUploadThreads() {
        return num_gpu_upload_threads;
    }
    static int GetNumGPUThreads() {
        return num_gputhreads;
    }
    static int GetNumPositionsPerBatch() {
        return num_positions_per_batch;
    }
    static int GetNumParallelBatches() {
        return num_parallel_batches;
    }
    static int GetTTSize() {
        return tt_size;
    }

    // tablebase
    static uint8_t GetTBDepth() {
        return tb_depth;
    }
};
