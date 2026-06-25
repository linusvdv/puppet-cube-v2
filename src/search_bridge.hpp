#pragma once
#include "search.hpp"


void CudaConstMemInitialize ();


void CudaConstMemChangeCurDepth (uint8_t depth);


void DeviceLeafManager (std::atomic<bool>& stoken, SharedLeafStates& shared_leaf_states, SharedLeafSolution& shared_leaf_solution,
                        uint64_t& num_gpu_positions, const int& thread_idx, uint8_t current_depth, int current_scramble);
