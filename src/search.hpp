#pragma once
#include <parallel_hashmap/phmap.h>

#include "concurrentqueue.h"
#include "cube.hpp"


using VisitedMap = phmap::parallel_flat_hash_map<State, uint8_t, phmap::priv::hash_default_hash<State>, phmap::priv::hash_default_eq<State>, phmap::priv::Allocator<std::pair<State, uint8_t>>, 8, std::mutex>;
using SharedLeafStates = moodycamel::ConcurrentQueue<std::shared_ptr<phmap::flat_hash_map<State, uint8_t>>>;


bool LeafSearch (const State& state, uint8_t depth, uint8_t& best_depth, std::pair<State, uint8_t>& best_endstate, VisitedMap& visited, uint64_t& leaft_search_positions, std::atomic<uint8_t>& atomic_best_depth, const int& thread_idx);


void SearchManager();
