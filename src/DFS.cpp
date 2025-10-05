#include <cstddef>
#include <vector>
#include <thread>

#include "BCHTSet.hpp"
#include "cube.hpp"
#include "logger.hpp"
#include "random_position.hpp"
#include "settings.hpp"
#include "tablebase.hpp"

#ifdef USE_CUDA
#include "DFS.cuh"
#endif


void DFS(const Cube::State& current, size_t& cnt, size_t& tb_cnt, int depth) {
    cnt++;
    if (BCHTSetContains(Tablebase::tablebase.back(), current)) {
        tb_cnt++;
    }
    if (depth == 0) {
        return;
    }
    for (int rotation = 0; rotation < kNumRotations; rotation++) {
        Cube::State next = current;
        if (next.Rotate(rotation)) {
            DFS(next, cnt, tb_cnt, depth-1);
        }
    }
}


void MultiDFS(const std::vector<Cube::State>& random_position, std::vector<size_t>& num_nodes_cpu, std::vector<size_t>& num_tb_hits_cpu, int thread_idx, int num_threads) {
    for (size_t i = thread_idx; i < random_position.size(); i += num_threads) {
        DFS(random_position[i], num_nodes_cpu[i], num_tb_hits_cpu[i], Settings::GetDFSDepth());
    }
}


void TimeDFS() {
    if (!Settings::GetTestDFS()) {
        return;
    }
    std::vector<Cube::State> random_positions = RandomPositions(Settings::GetNumDFSPositions(), 0);

    std::vector<size_t> num_nodes_cpu(random_positions.size(), 0);
    std::vector<size_t> num_tb_hits_cpu(random_positions.size(), 0);
    LOG_MEMORY();

    // Time phmap multithreads
    LOG_EXTRA("Start timing of DFS multithreads");
    std::chrono::time_point dfs_start_time = std::chrono::high_resolution_clock::now(); // get the current time

    {
        std::vector<std::jthread> threads;
        for (int i = 0; i < Settings::GetNumThreads(); i++) {
            threads.push_back(std::jthread(MultiDFS, std::ref(random_positions), std::ref(num_nodes_cpu), std::ref(num_tb_hits_cpu), i, Settings::GetNumThreads()));
        }
    }

    std::chrono::time_point dfs_end_time = std::chrono::high_resolution_clock::now(); // get the duration since epoch
    std::chrono::milliseconds dfs_time_multi = std::chrono::duration_cast<std::chrono::milliseconds>(dfs_end_time - dfs_start_time);
    LOG_ALL("Time duration for DFS multithreads:", dfs_time_multi.count());

    #ifdef USE_CUDA
    std::vector<size_t> num_nodes_gpu(random_positions.size(), 0);
    std::vector<size_t> num_tb_hits_gpu(random_positions.size(), 0);

    // Time phmap multithreads
    LOG_EXTRA("Start timing of DFS GPU");
    std::chrono::time_point dfs_start_time_gpu = std::chrono::high_resolution_clock::now(); // get the current time

    GPUDFS(random_positions, num_nodes_gpu, num_tb_hits_gpu);

    std::chrono::time_point dfs_end_time_gpu = std::chrono::high_resolution_clock::now(); // get the duration since epoch
    std::chrono::milliseconds dfs_time_multi_gpu = std::chrono::duration_cast<std::chrono::milliseconds>(dfs_end_time_gpu - dfs_start_time_gpu);
    LOG_ALL("Time duration for DFS GPU:", dfs_time_multi_gpu.count());

    if (num_nodes_cpu == num_nodes_gpu) {
        LOG_INFO("Correct DFS");
        size_t total_num_positions = 0;
        for (size_t i = 0; i < num_nodes_cpu.size(); i++) {
            total_num_positions += num_nodes_cpu[i];
        }
        LOG_EXTRA("CPU Total num positions:", total_num_positions);
        LOG_EXTRA("CPU Positions per seconds:", int64_t(total_num_positions*1000/(dfs_time_multi.count()+1e-3)));

        LOG_EXTRA("GPU Total num positions:", total_num_positions);
        LOG_EXTRA("GPU Positions per seconds:", int64_t(total_num_positions*1000/(dfs_time_multi_gpu.count()+1e-3)));

        size_t total_real_positions = 0;
        size_t max = 0;
        for (size_t i = 0; i < num_nodes_cpu.size(); i++) {
            max = std::max(max, num_nodes_gpu[i]);
            if ((i+1)%kBlockDim == 0) {
                total_real_positions += max*kBlockDim;
                max = 0;
            }
        }
        total_real_positions += max*kBlockDim;
        LOG_EXTRA("Real GPU tablebase lookups: ", total_real_positions);
    }
    else {
        LOG_ERROR("Not the same elements");
        for (size_t i = 0; i < num_nodes_cpu.size(); i++) {
            LOG_EXTRA("#", i, "CPU:", num_nodes_cpu[i], "GPU:", num_nodes_gpu[i]);
        }
    }

    if (num_tb_hits_cpu == num_tb_hits_gpu) {
        size_t total_num_tb_hits = 0;
        for (size_t i = 0; i < num_tb_hits_cpu.size(); i++) {
            total_num_tb_hits += num_tb_hits_cpu[i];
        }
        LOG_EXTRA("Num tb hits:", total_num_tb_hits);
    }
    else {
        LOG_ERROR("Not the same elements in tb");
        for (size_t i = 0; i < num_tb_hits_cpu.size(); i++) {
            if (num_tb_hits_cpu[i] == num_tb_hits_gpu[i]) {
                LOG_EXTRA("#", i, "CPU:", num_tb_hits_cpu[i], "/", num_nodes_cpu[i], "\t\tGPU:", num_tb_hits_gpu[i], "/", num_nodes_gpu[i]);
            }
            else {
                LOG_ALL("#", i, "CPU:", num_tb_hits_cpu[i], "/", num_nodes_cpu[i], "\t\tGPU:", num_tb_hits_gpu[i], "/", num_nodes_gpu[i]);
            }
        }
    }
    #endif // USE_CUDA
}
