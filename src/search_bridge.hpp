#pragma once
#include <stop_token>

#include "cube.hpp"
#include "search.hpp"


void DeviceLeafManager (std::stop_token stocken, std::atomic<std::shared_ptr<phmap::flat_hash_map<State, uint8_t>>>& shared_leaf_states,
                        std::mutex& mtx, uint64_t& num_positions_leaf, VisitedMap& visited_leaf,
                        std::atomic<uint8_t>& atomic_best_depth, std::pair<State, uint8_t>& best_endstate_leafs,
                        const int& thread_idx);
