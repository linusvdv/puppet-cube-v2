#pragma once
#include <atomic>
#include <cstdint>
#include <vector>

#include "cube.hpp"


constexpr uint64_t kDefaultTTEntry = ~uint64_t(-1);
constexpr uint64_t kTTCheckHashMask = (1ULL << (64-8)) - 1ULL;

enum class InTT {
    kTrue,
    kFalse,
    kCollision
};


// it is guarantied that the index + the tt_check_hash can identify a position uniquely
class TranspositionTable {
public:
    static void Initialize(uint64_t size_mb);

    // resets the Transposition Table
    static void Clear();

    static std::vector<std::atomic<uint64_t>>& GetTT() {
        return tt;
    }

    // returns true if the state is already in the TT with the current or lower depth and the search of this position can be skipped
    // if the current state has been visited with a higher depth the depth will be decreased
    //      (this is not guarantied as there is no CAS,
    //      but this is not important as it would search this position twice but will never skip a position which it has not jet visited with this depth)
    // if the current state collides with another state the first state will stay
    static bool InsertState(const State& state, const uint8_t& depth);
    static InTT ContainsState(const State& state, const uint8_t& depth);


private:
    static uint64_t tt_size;
    static std::vector<std::atomic<uint64_t>> tt;  // NOLINT


    static void GetTTstate(const State& state, uint64_t& tt_key, uint64_t& tt_check_hash);
};
