#pragma once
#include <parallel_hashmap/phmap.h>

#include "cube.hpp"


using VisitedMap = phmap::flat_hash_map<State, uint8_t>;


bool LeafSearch (const State& state, uint8_t depth, uint8_t& best_depth, std::pair<State, uint8_t>& best_endstate, VisitedMap& visited, uint64_t& leaft_search_positions, std::atomic<uint8_t>& atomic_best_depth, const int& thread_idx);


void SearchManager();
