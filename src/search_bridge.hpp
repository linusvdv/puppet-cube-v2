#pragma once
#include <stop_token>

#include "cube.hpp"
#include "search.hpp"


struct StartingPosition {
    uint8_t depth;
    State state;
};


struct RegState {
    uint16_t corner_orientation = -1;
    uint16_t corner_position = -1;
    uint16_t edge_orientation = -1;
    uint32_t edge_position_1 = -1;
    uint32_t edge_position_2 = -1;
};


void DeviceLeafManager (std::stop_token stocken, SharedLeafStates& shared_leaf_states,
                        uint64_t& num_positions_leaf, VisitedMap& visited_leaf,
                        std::atomic<uint8_t>& atomic_best_depth, std::pair<State, uint8_t>& best_endstate_leafs,
                        const int& thread_idx);
