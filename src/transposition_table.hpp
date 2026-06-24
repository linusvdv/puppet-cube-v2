#pragma once
#include <atomic>
#include <vector>

#include "cube.hpp"


namespace transposition_table {
enum class InTT : uint8_t {
    kTrue,
    kFalse,
    kCollision,
    kHighDepth
};


constexpr uint64_t kDefaultTTEntry = ~uint64_t(0);

extern std::vector<std::atomic<uint64_t>> tt; // NOLINT


inline void GetTTHash(const State& state, uint8_t depth, uint64_t& idx, uint64_t& value) {
    GetStateHash1(tt.size()>>8, idx, value, state);
    idx = (idx<<8) | uint8_t(value);
    value = (value & (~uint64_t(~uint8_t(0)))) | depth;
}


void Init();
InTT Contains(const State& state, const uint8_t& depth);


// returns true if the state is alread in the TT with the current or lower depth
template<bool overwrite_existing_entries=false>
inline bool Insert(const State& state, const uint8_t& depth) {
    uint64_t idx;
    uint64_t value;
    GetTTHash(state, depth, idx, value);
    uint64_t tt_value = tt[idx].load(std::memory_order_relaxed);

    // it is expected that this part does not guarantie that the minimum depth is in the tt
    // if a higher depth is stored the position has to be reevaluated
    // the correctness of the algorithm is still guarantied
    if (tt_value == kDefaultTTEntry) { // no entry in TT
        tt[idx].store(value, std::memory_order_relaxed);
        return false;
    }
    if ((tt_value>>8) != (value>>8)) { // other entry in TT
        tt[idx].store(value, std::memory_order_relaxed);
        return false;
    }
    if (uint8_t(tt_value) > depth) { // position with higher depth
        tt[idx].store(value, std::memory_order_relaxed);
        return false;
    }
    return true;
}
}
