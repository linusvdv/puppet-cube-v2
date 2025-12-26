#pragma once
#include <stop_token>

#include "cube.hpp"
#include "search.hpp"


void DeviceLeafManager (std::stop_token stocken, SharedLeafStates& shared_leaf_states,
                        uint64_t& num_positions_leaf, VisitedMap& visited_leaf,
                        std::atomic<uint8_t>& atomic_best_depth, std::pair<State, uint8_t>& best_endstate_leafs,
                        const int& thread_idx);
