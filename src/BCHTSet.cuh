#pragma once
#include <cstdint>

#include "search_bridge.hpp"
#include "cube.cuh"
#include "BCHTSet.hpp"


constexpr uint64_t kDXORlow1 = 0x123456789abcdef0ULL;
constexpr uint64_t kDXORhigh1 = 0xfedcba9876543210ULL;
constexpr uint64_t kDXORlow2 = 0x0f1e2d3c4b5a6978ULL;
constexpr uint64_t kDXORhigh2 = 0x87654321abcdef09ULL;

static __device__ inline uint64_t Mix64(uint64_t num) {
    num ^= num >> 31;  // NOLINT
    num *= kDMulA;
    num ^= num >> 33;  // NOLINT
    num *= kDMulB;
    num ^= num >> 28;  // NOLINT
    return num;
}


template<uint64_t hash_low, uint64_t hash_high>
__device__ inline uint64_t SplitMix64(const RegState& reg_state) {
    return Mix64(uint64_t(reg_state.corner_position) ^ hash_low)
         ^ Mix64(((uint64_t(reg_state.corner_orientation) << 52)
                     | (uint64_t(reg_state.edge_position_1) << 32)
                     | (uint64_t(reg_state.edge_orientation) << 20)
                     | uint64_t(reg_state.edge_position_2)) ^ hash_high);
}


__device__ inline bool DBCHTSetContains(const DState* d_tablebase, const size_t& d_tablebase_size, const RegState& reg_state) {
    uint32_t num_buckets = d_tablebase_size / kBucketSize;
    uint64_t bucked_idx = SplitMix64<kDXORlow1, kDXORhigh1>(reg_state)%num_buckets;
    for (int j = 0; j < kBucketSize; j++) {
        uint16_t cur_hash_1 = d_tablebase[(bucked_idx*kBucketSize) + j].hash_1;
        if (cur_hash_1 == uint16_t(-1)) {
            return false;
        }
        if (cur_hash_1 != reg_state.corner_position) {
            continue;
        }
        if (d_tablebase[(bucked_idx*kBucketSize) + j].hash_2 != ((uint32_t(reg_state.corner_orientation) << 20) | uint32_t(reg_state.edge_position_1))) {
            continue;
        }
        if (d_tablebase[(bucked_idx*kBucketSize) + j].hash_3 != ((uint32_t(reg_state.edge_orientation) << 20) | uint32_t(reg_state.edge_position_2))) {
            continue;
        }
        return true;
    }
    bucked_idx = SplitMix64<kDXORlow2, kDXORhigh2>(reg_state)%num_buckets;
    for (int j = 0; j < kBucketSize; j++) {
        uint16_t cur_hash_1 = d_tablebase[(bucked_idx*kBucketSize) + j].hash_1;
        if (cur_hash_1 == uint16_t(-1)) {
            return false;
        }
        if (cur_hash_1 != reg_state.corner_position) {
            continue;
        }
        if (d_tablebase[(bucked_idx*kBucketSize) + j].hash_2 != ((uint32_t(reg_state.corner_orientation) << 20) | uint32_t(reg_state.edge_position_1))) {
            continue;
        }
        if (d_tablebase[(bucked_idx*kBucketSize) + j].hash_3 != ((uint32_t(reg_state.edge_orientation) << 20) | uint32_t(reg_state.edge_position_2))) {
            continue;
        }
        return true;
    }
    return false;
}
