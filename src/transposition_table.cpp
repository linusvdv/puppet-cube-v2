#include <atomic>
#include <cassert>
#include <cstddef>
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
    std::fill(
        reinterpret_cast<uint64_t*>(tt.data()),
        reinterpret_cast<uint64_t*>(&tt[tt_size]),
        kDefaultTTEntry
    );
}
void TranspositionTable::Clear(size_t thread_idx, size_t num_threads) {
    std::fill(
        reinterpret_cast<uint64_t*>(&tt[tt_size*(thread_idx)/num_threads]),
        reinterpret_cast<uint64_t*>(&tt[tt_size*(thread_idx+1)/num_threads]),
        kDefaultTTEntry
    );
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
        return InTT::kHighDepth;
    }
    return InTT::kTrue;
}
