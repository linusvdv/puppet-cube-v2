#pragma once
#include <atomic>
#include <stop_token>

#include "cube.hpp"
#include "search.hpp"


struct RegState {
    uint16_t corner_orientation = -1;
    uint16_t corner_position = -1;
    uint16_t edge_orientation = -1;
    uint32_t edge_position_1 = -1;
    uint32_t edge_position_2 = -1;
};


struct SharedLeafSolution {
    std::atomic<bool> finished{false};
    State state;
    uint8_t best_depth;
};


void CudaConstMemInitialize ();


void CudaConstMemChangeCurDepth (uint8_t depth);


void DeviceLeafManager (std::stop_token stocken, SharedLeafStates& shared_leaf_states, VisitedMap& visited_leaf, std::atomic<uint8_t>& atomic_best_depth,
                        SharedLeafSolution& shared_leaf_solution, uint64_t& num_gpu_positions, const int& thread_idx);
