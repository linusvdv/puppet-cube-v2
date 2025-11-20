#pragma once
#include <vector>

#include "cube.hpp"
#include "search.hpp"


void DeviceLeafManager (const std::vector<std::pair<State, uint8_t>>& starting_positions, uint64_t& num_positions_leaf, VisitedMap& visited_leaf,
                        std::atomic<uint8_t>& atomic_best_depth, std::pair<State, uint8_t>& best_endstate_leafs,
                        [[maybe_unused]] const uint64_t& leaf_batch_size, const int& thread_idx);
