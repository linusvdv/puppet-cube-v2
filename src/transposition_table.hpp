#pragma once
#include <atomic>
#include <cassert>
#include <cstdint>
#include <vector>

#include "cube.hpp"


constexpr uint64_t kDefaultTTEntry = ~uint64_t(-1);
constexpr uint64_t kTTCheckHashMask = (1ULL << (64-8)) - 1ULL;

enum class InTT {
    kTrue,
    kFalse,
    kCollision,
    kHighDepth
};


// it is guarantied that the index + the tt_check_hash can identify a position uniquely
class TranspositionTable {
public:
    static void Initialize(uint64_t size_mb);

    // resets the Transposition Table
    static void Clear();
    static void Clear(size_t thread_idx, size_t num_threads);


    static std::vector<std::atomic<uint64_t>>& GetTT() {
        return tt;
    }

    // returns true if the state is already in the TT with the current or lower depth and the search of this position can be skipped
    // if the current state has been visited with a higher depth the depth will be decreased
    //      (this is not guarantied as there is no CAS,
    //      but this is not important as it would search this position twice but will never skip a position which it has not jet visited with this depth)
    // if the current state collides with another state the first state will stay
    static InTT ContainsState(const State& state, const uint8_t& depth);


    // TEST: check speedup when tt_size is guarantied to be a power of 2
    template<bool overwrite_existing_entries=false>
    static bool InsertState(const State& state, const uint8_t& depth) {
        // this guaranties that kDefaultTTEntry can never be intended as a TT entry
        assert(depth != uint8_t(-1));

        // convert the state to a key and check_hash which uniquely define a cube
        uint64_t tt_key;
        uint64_t tt_check_hash;
        GetTTstate(state, tt_key, tt_check_hash);
        uint64_t value_idx = tt[tt_key].load(std::memory_order_relaxed);

        // it is expected that this part does not guarantie that the minimum depth is in the tt
        // if a higher depth is stored the position has to be reevaluated
        // the correctness of the algorithm is still guarantied
        if (value_idx == kDefaultTTEntry) { // no entry in TT
            tt[tt_key].store(tt_check_hash | (uint64_t(depth)<<(64-8)), std::memory_order_relaxed);
            return false;
        }
        if ((value_idx&kTTCheckHashMask) != tt_check_hash) { // other entry in TT
            if constexpr (overwrite_existing_entries) {
                tt[tt_key].store(tt_check_hash | (uint64_t(depth)<<(64-8)), std::memory_order_relaxed);
            }
            return false;
        }
        // check for smaller depth
        if (depth < (value_idx>>(64-8))) {
            tt[tt_key].store(tt_check_hash | (uint64_t(depth)<<(64-8)), std::memory_order_relaxed);
            return false;
        }
        return true;
    }


private:
    static uint64_t tt_size;
    static std::vector<std::atomic<uint64_t>> tt;  // NOLINT


    static void GetTTstate(const State& state, uint64_t& tt_key, uint64_t& tt_check_hash);
};
