#include <atomic>
#include <cassert>
#include <cstdint>
#include <vector>

#include "logger.hpp"
#include "transposition_table.hpp"


std::vector<std::atomic<uint64_t>> TranspositionTable::tt; // NOLINT
uint64_t TranspositionTable::tt_size;


void TranspositionTable::Initialize(uint64_t size_mb) {
    uint64_t size = (size_mb*1024*1024) / sizeof(std::atomic<uint64_t>);
    constexpr uint64_t kMinTTSize = (1LL<<24);
    if (size < kMinTTSize) {
        LOG_ERROR("Transposition Table too small");
        size = kMinTTSize;
        LOG_WARNING("Set Transposition Table size to ", kMinTTSize/1024/1024*sizeof(std::atomic<uint64_t>), "MB");
    }
    tt = std::vector<std::atomic<uint64_t>>(size);
    tt_size = size;
}


void TranspositionTable::Clear() {
    for (std::atomic<uint64_t>& entry : tt) {
        entry.store(kDefaultTTEntry, std::memory_order_relaxed);
    }
}


// David Stafford Mix13
// standart implementation of a SplitMix64 found in the paper: https://dl.acm.org/doi/epdf/10.1145/2714064.2660195
// This version of hashing is reverable (so no loss of data)
uint64_t SplitMix64(uint64_t num) {
    num ^= num >> 30;               // NOLINT
    num *= 0xbf58476d1ce4e5b9ULL;   // NOLINT
    num ^= num >> 27;               // NOLINT
    num *= 0x94d049bb133111ebULL;   // NOLINT
    num ^= num >> 31;               // NOLINT
    return num;
}


// convert the state to a key and check_hash which uniquely define a cube
void TranspositionTable::GetTTstate(const State& state, uint64_t& tt_key, uint64_t& tt_check_hash) {
    tt_key = SplitMix64((uint64_t(state.hash_2)<<32) ^ uint64_t(state.hash_3));
    tt_key ^= SplitMix64(uint64_t(state.hash_1));
    tt_check_hash = (uint64_t(state.hash_1)<<(64-8-16)) | (tt_key/tt_size);
    tt_key %= tt_size;
}


// TEST: check speedup when tt_size is guarantied to be a power of 2
bool TranspositionTable::InsertState(const State& state, const uint8_t& depth) {
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
        return false;
    }
    // check for smaller depth
    if (depth < (value_idx>>(64-8))) {
        tt[tt_key].store(tt_check_hash | (uint64_t(depth)<<(64-8)), std::memory_order_relaxed);
        return false;
    }
    return true;
}


InTT TranspositionTable::ContainsState(const State& state, const uint8_t& depth) {
    uint64_t tt_key;
    uint64_t tt_check_hash;
    GetTTstate(state, tt_key, tt_check_hash);
    uint64_t value_idx = tt[tt_key].load(std::memory_order_relaxed);

    if (value_idx == kDefaultTTEntry) { // no entry in TT
        return InTT::kFalse;
    }
    if ((value_idx&kTTCheckHashMask) != tt_check_hash) { // other entry in TT
        return InTT::kCollision;
    }
    if (depth < (value_idx>>(64-8))) {
        return InTT::kFalse;
    }
    return InTT::kTrue;
}
