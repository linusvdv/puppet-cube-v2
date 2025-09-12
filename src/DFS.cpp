#include <cstddef>
#include <vector>
#include <thread>

#include "cube.hpp"
#include "logger.hpp"
#include "random_position.hpp"
#include "settings.hpp"

#ifdef USE_CUDA
#include "DFS.cuh"
#endif


constexpr size_t kNumRandomPositions = 10000000;


void DFS(Cube::State current, size_t& cnt, int depth) {
    cnt++;
    if (depth == 0) {
        return;
    }
    for (int rotation = 0; rotation < kNumRotations; rotation++) {
        Cube::State next = current;
        if (next.Rotate(rotation)) {
            DFS(next, cnt, depth-1);
        }
    }
}


void MultiDFS(const std::vector<Cube::State>& random_position, std::vector<size_t>& num_nodes_cpu, int thread_idx, int num_threads) {
    for (size_t i = thread_idx; i < random_position.size(); i += num_threads) {
        DFS(random_position[i], num_nodes_cpu[i], Settings::GetDFSDepth());
    }
}


void TimeDFS() {
    if (!Settings::GetTestDFS()) {
        return;
    }
    std::vector<Cube::State> random_positions = RandomPositions(kNumRandomPositions); 

    std::vector<size_t> num_nodes_cpu(random_positions.size(), 0);

    // Time phmap multithreads
    LOG_EXTRA("Start timing of DFS multithreads");
    std::chrono::time_point dfs_start_time = std::chrono::high_resolution_clock::now(); // get the current time

    {
        std::vector<std::jthread> threads;
        for (int i = 0; i < Settings::GetNumThreads(); i++) {
            threads.push_back(std::jthread(MultiDFS, std::ref(random_positions), std::ref(num_nodes_cpu), i, Settings::GetNumThreads()));
        }
    }

    std::chrono::time_point dfs_end_time = std::chrono::high_resolution_clock::now(); // get the duration since epoch
    std::chrono::milliseconds dfs_time_multi = std::chrono::duration_cast<std::chrono::milliseconds>(dfs_end_time - dfs_start_time);
    LOG_ALL("Time duration for DFS multithreads:", dfs_time_multi.count());

    #ifdef USE_CUDA
    std::vector<size_t> num_nodes_gpu(random_positions.size(), 1e9);
    // Time phmap multithreads
    LOG_EXTRA("Start timing of DFS GPU");
    std::chrono::time_point dfs_start_time_gpu = std::chrono::high_resolution_clock::now(); // get the current time

    GPUDFS(random_positions, num_nodes_gpu);

    std::chrono::time_point dfs_end_time_gpu = std::chrono::high_resolution_clock::now(); // get the duration since epoch
    std::chrono::milliseconds dfs_time_multi_gpu = std::chrono::duration_cast<std::chrono::milliseconds>(dfs_end_time_gpu - dfs_start_time_gpu);
    LOG_ALL("Time duration for DFS GPU:", dfs_time_multi_gpu.count());
    #endif // USE_CUDA

    if (num_nodes_cpu == num_nodes_gpu) {
        LOG_INFO("Correct DFS");
    }
    else {
        LOG_ERROR("Not the same elements");
        for (size_t i = 0; i < num_nodes_cpu.size(); i++) {
            LOG_EXTRA("#", i, "CPU:", num_nodes_cpu[i], "GPU:", num_nodes_gpu[i]);
        }
    }
}
