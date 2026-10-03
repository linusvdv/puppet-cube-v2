#pragma once
#include <parallel_hashmap/phmap.h>
#include <barrier>
#include <condition_variable>
#include <optional>
#include <queue>

#include "cube.hpp"


struct SharedLeafStates {
    std::queue<std::shared_ptr<std::vector<std::pair<State, uint8_t>>>> shared_ptrs;
    std::atomic<bool> finished_depth = false;
    std::mutex mtx;
    std::condition_variable cv;
    uint8_t depth;
    int scramble_idx;
};


struct SharedLeafSolution {
    std::atomic<bool> finished{false};
    State state;
};


struct SharedSearch {
    std::atomic<bool> finished{false};

    std::optional<std::barrier<>> start_work;
    std::optional<std::barrier<>> done_work;

    std::atomic<uint64_t> leaf_cnt{0};
};


using LocalBuffer = std::shared_ptr<std::vector<std::pair<State, uint8_t>>>;


void CpuLeafManager (SharedSearch& shared_search, SharedLeafStates& shared_leaf_states, SharedLeafSolution& shared_leaf_solution);


void SearchManager();
