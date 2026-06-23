#pragma once
#include <compare>
#include <cstdint>

struct State {
    uint32_t edge_pos;      // 9985968 -> 24 bits
    uint16_t corner_pos;    //   40320 -> 16 bits
    uint16_t corner_orient; //    2187 -> 12 bits
    uint16_t edge_orient;   //    2048 -> 11 bits
    uint8_t edge_sym;       //      48 ->  6 bits
    std::strong_ordering operator<=>(const State&) const = default;
};


// David Stafford Mix13
// standart implementation of a SplitMix64 found in the paper: https://dl.acm.org/doi/epdf/10.1145/2714064.2660195
// This version of hashing is reverable (so no loss of data)
inline void SplitMix64(uint64_t& num) {
    num ^= num >> 30;               // NOLINT
    num *= 0xbf58476d1ce4e5b9ULL;   // NOLINT
    num ^= num >> 27;               // NOLINT
    num *= 0x94d049bb133111ebULL;   // NOLINT
    num ^= num >> 31;               // NOLINT
}


// neutral element for each index is: index ^ 63
constexpr uint64_t kNeurtralElementXOR = 63;


// hashing State such that it is recostructable
inline void GetStateHash1(const uint64_t tb_size, uint64_t& idx, uint64_t& value, const State& state) {
    value = state.edge_pos;
    value |= uint64_t(state.corner_pos) << 24;      // NOLINT
    value |= uint64_t(state.corner_orient) << 40;   // NOLINT
    value |= uint64_t(state.edge_orient) << 52;     // NOLINT
    value ^= uint64_t(state.edge_sym) << (58); // NOLINT this overlaps with the previous value so edge_sym has to be able to be retrieved later

    SplitMix64(value);
    // TODO: Do this as a multiplication and shifting for speedup
    idx = value % tb_size;

    // this only works if the tb_size is bigger than 2**6
    // as now we can reconstruct the state symmetry
    value ^= state.edge_sym;
}

// hashing State such that it is recostructable
inline void GetStateHash2(const uint64_t tb_size, uint64_t& idx, uint64_t& value, const State& state) {
    value = state.edge_pos;
    value |= uint64_t(state.corner_pos) << 24;      // NOLINT
    value |= uint64_t(state.corner_orient) << 40;   // NOLINT
    value |= uint64_t(state.edge_orient) << 52;     // NOLINT
    value ^= uint64_t(state.edge_sym) << (58); // NOLINT this overlaps with the previous value so edge_sym has to be able to be retrieved later

    value ^= 0x5555555555555555ULL; // NOLINT

    SplitMix64(value);
    // TODO: Do this as a multiplication and shifting for speedup
    idx = value % tb_size;

    // this only works if the tb_size is bigger than 2**6
    // as now we can reconstruct the state symmetry
    value ^= state.edge_sym;
}

constexpr State kSolvedState{uint32_t(0), uint16_t(0), uint16_t(0), uint16_t(0), uint8_t(0)};
constexpr State kNonLegalState{uint32_t(~0U), uint16_t(~0U), uint16_t(~0U), uint16_t(~0U), uint8_t(~0U)};
