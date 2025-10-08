#include "cube.hpp"

#include "BCHTSet.cuh"
#include "BCHTSet.hpp"
#include "settings.hpp"


__device__ bool DBCHTSetContains(const DState* d_tablebase, const size_t& d_tablebase_size, const DState& key) {
    uint32_t num_buckets = d_tablebase_size / kBucketSize;
    uint64_t h_1 = key.SplitMix64<0x123456789abcdef0ULL, 0xfedcba9876543210ULL>()%num_buckets;  // NOLINT
    for (int j = 0; j < kBucketSize; j++) {
        DState tb_data = d_tablebase[(h_1*kBucketSize) + j];
        if (tb_data == key) {
            return true;
        }
        if (tb_data == DState()) {
            return false;
        }
    }
    uint64_t h_2 = key.SplitMix64<0x0f1e2d3c4b5a6978ULL, 0x87654321abcdef09ULL>()%num_buckets;  // NOLINT
    for (int j = 0; j < kBucketSize; j++) {
        DState tb_data = d_tablebase[(h_2*kBucketSize) + j];
        if (tb_data == key) {
            return true;
        }
        if (tb_data == DState()) {
            return false;
        }
    }
    return false;
}
