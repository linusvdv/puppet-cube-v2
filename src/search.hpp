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


bool LeafSearch (const State& state, uint8_t depth, uint8_t& best_depth, std::pair<State, uint8_t>& best_endstate, uint64_t& leaft_search_positions, std::atomic<uint8_t>& atomic_best_depth, const int& thread_idx);


void SearchManager();
