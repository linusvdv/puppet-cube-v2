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


// hashing State such that it is recostructable
__device__ inline void GetStateHash1(const uint64_t tb_size, uint64_t& idx, uint64_t& value, const State& state) {
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
__device__ inline void GetStateHash2(const uint64_t tb_size, uint64_t& idx, uint64_t& value, const State& state) {
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
