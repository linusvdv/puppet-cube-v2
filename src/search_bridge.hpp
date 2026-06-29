#pragma once
#include "search.hpp"


void CudaConstMemInitialize ();


void CudaConstMemChangeCurDepth (uint8_t depth);


void DeviceLeafManagerInit (int gpu_idx, SharedSearch& shared_search,
        SharedLeafStates& shared_leaf_states, SharedLeafSolution& shared_leaf_solution);
