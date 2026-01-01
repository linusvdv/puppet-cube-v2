#pragma once
#include "cube.cuh"
#include "BCHTSet.hpp"


constexpr uint64_t kDXORlow1 = 0x123456789abcdef0ULL;
constexpr uint64_t kDXORhigh1 = 0xfedcba9876543210ULL;
constexpr uint64_t kDXORlow2 = 0x0f1e2d3c4b5a6978ULL;
constexpr uint64_t kDXORhigh2 = 0x87654321abcdef09ULL;


__device__ inline bool DBCHTSetContains(const DState* d_tablebase, const size_t& d_tablebase_size, const DState& key) {
    uint32_t num_buckets = d_tablebase_size / kBucketSize;
    uint64_t bucked_idx = key.SplitMix64<kDXORlow1, kDXORhigh1>()%num_buckets;
    for (int j = 0; j < kBucketSize; j++) {
        if (d_tablebase[(bucked_idx*kBucketSize) + j] == key) {
            return true;
        }
        if (d_tablebase[(bucked_idx*kBucketSize) + j].IsDefault()) {
            return false;
        }
    }
    bucked_idx = key.SplitMix64<kDXORlow2, kDXORhigh2>()%num_buckets;
    for (int j = 0; j < kBucketSize; j++) {
        if (d_tablebase[(bucked_idx*kBucketSize) + j] == key) {
            return true;
        }
        if (d_tablebase[(bucked_idx*kBucketSize) + j].IsDefault()) {
            return false;
        }
    }
    return false;
}
