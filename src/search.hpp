/*
#pragma once
#include <parallel_hashmap/phmap.h>
#include <condition_variable>
#include <queue>

#include "cube.hpp"


struct SharedLeafStates {
    std::queue<std::shared_ptr<std::vector<std::pair<State, uint8_t>>>> shared_ptrs;
    std::mutex mtx;
    std::condition_variable cv;
};


struct SharedLeafSolution {
    std::atomic<bool> finished{false};
    State state;
};


using LocalBuffer = std::shared_ptr<std::vector<std::pair<State, uint8_t>>>;


bool LeafSearch (const State& state, uint8_t depth, uint8_t& best_depth, std::pair<State, uint8_t>& best_endstate, uint64_t& leaft_search_positions, std::atomic<uint8_t>& atomic_best_depth, const int& thread_idx);


void SearchManager();
*/
