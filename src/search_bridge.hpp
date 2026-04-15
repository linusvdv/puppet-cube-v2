#pragma once
#include <stop_token>

#include "search.hpp"


struct RegState {
    uint16_t corner_orientation = -1;
    uint16_t corner_position = -1;
    uint16_t edge_orientation = -1;
    uint32_t edge_position_1 = -1;
    uint32_t edge_position_2 = -1;
};


void CudaConstMemInitialize ();


void CudaConstMemChangeCurDepth (uint8_t depth);


void DeviceLeafManager (std::atomic<bool>& stoken, SharedLeafStates& shared_leaf_states, SharedLeafSolution& shared_leaf_solution,
                        uint64_t& num_gpu_positions, const int& thread_idx, uint8_t current_depth, int current_scramble);
