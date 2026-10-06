#pragma once
#include <parallel_hashmap/phmap.h>
#include <barrier>
#include <condition_variable>
#include <functional>
#include <optional>
#include <queue>
#include <cstdint>

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


// events to visualize the search from outside of the search (gui)
enum class SearchEventKind : uint8_t { kScramble, kSolution };

struct SearchEvent {
    SearchEventKind kind;
    size_t run_idx;
    uint8_t depth; // depth of the solution (0 for scramble events)
    std::vector<uint8_t> moves;
};

using SearchEventCallback = std::function<void(const SearchEvent&)>;


void SearchManager(const SearchEventCallback& event_callback = nullptr);
