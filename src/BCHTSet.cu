#include "cube.hpp"
#include "BCHTSet.cuh"
#include "BCHTSet.hpp"


struct SplitMix128 {
    uint64_t state_low;
    uint64_t state_high;

    static constexpr uint64_t kMulA = 0x2545f4914f6cdd1dULL;
    static constexpr uint64_t kMulB = 0x9e3779b97f4a7c15ULL;

    constexpr SplitMix128(uint64_t seed_low, uint64_t seed_high) : state_low(seed_low), state_high(seed_high) {}

    static __device__ uint64_t Mix64(uint64_t num) {
        num ^= num >> 31;  // NOLINT
        num *= kMulA;
        num ^= num >> 33;  // NOLINT
        num *= kMulB;
        num ^= num >> 28;  // NOLINT
        return num;
    }

    __device__ uint32_t MixInput(const Cube::State& state, const uint32_t& num_buckets) const {
        uint64_t combined = Mix64(state.hash_1 ^ state_high) ^ (state.hash_2 ^ state_low);
        return uint32_t(combined % num_buckets);
    }
};


// 2 independent hashers with different seeds
__device__ constexpr SplitMix128 hasher1(0x123456789abcdef0ULL, 0xfedcba9876543210ULL);  // NOLINT
__device__ constexpr SplitMix128 hasher2(0x0f1e2d3c4b5a6978ULL, 0x87654321abcdef09ULL);  // NOLINT


__device__ bool DBCHTSetContains(const Cube::State* d_tablebase, const size_t& d_tablebase_size, const Cube::State& key) {
    uint32_t num_buckets = d_tablebase_size / kBucketSize;
    for (int j = 0; j < kBucketSize; j++) {
        if (d_tablebase[(hasher1.MixInput(key, num_buckets) * kBucketSize) + j] == key) {
            return true;
        }
    }
    for (int j = 0; j < kBucketSize; j++) {
        if (d_tablebase[(hasher2.MixInput(key, num_buckets) * kBucketSize) + j] == key) {
            return true;
        }
    }
    return false;
}
