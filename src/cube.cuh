#pragma once
#include "cube.hpp"


// David Stafford Mix13
// standart implementation of a SplitMix64 found in the paper: https://dl.acm.org/doi/epdf/10.1145/2714064.2660195
// This version of hashing is reverable (so no loss of data)
__device__ inline void SplitMix64(uint64_t& num) {
    num ^= num >> 30;               // NOLINT
    num *= 0xbf58476d1ce4e5b9ULL;   // NOLINT
    num ^= num >> 27;               // NOLINT
    num *= 0x94d049bb133111ebULL;   // NOLINT
    num ^= num >> 31;               // NOLINT
}


// hashing State such that it is reconstructable
__device__ inline void GetStateHash1(const uint64_t mask, uint64_t& idx, uint64_t& value, const State& state) {
    value = state.edge_pos;
    value |= uint64_t(state.corner_pos) << 24;      // NOLINT
    value |= uint64_t(state.corner_orient) << 40;   // NOLINT
    value |= uint64_t(state.edge_orient) << 52;     // NOLINT

    SplitMix64(value);
    idx = state.edge_sym;
    value ^= idx;

    uint64_t temp = (value ^ idx) & mask;
    value ^= temp;
    idx ^= temp;
}


// hashing State for another hash
__device__ inline void GetStateHash2(const uint64_t mask, uint64_t& idx, uint64_t& value, const State& state) {
    value = state.edge_pos;
    value |= uint64_t(state.corner_pos) << 24;      // NOLINT
    value |= uint64_t(state.corner_orient) << 40;   // NOLINT
    value |= uint64_t(state.edge_orient) << 52;     // NOLINT

    value ^= 0x5555555555555555ULL; // NOLINT

    SplitMix64(value);
    idx = state.edge_sym;
    value ^= idx;

    uint64_t temp = (value ^ idx) & mask;
    value ^= temp;
    idx ^= temp;
}
