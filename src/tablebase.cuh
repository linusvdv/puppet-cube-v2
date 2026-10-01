#pragma once
#include "cube.cuh"
#include "cube.hpp"
#include "tablebase.hpp"


namespace tablebase {
extern __constant__ uint64_t* d_tablebase;
extern __constant__ uint64_t d_tablebase_size;

__device__ inline bool DContains(const State& state) {
    uint64_t idx1;
    uint64_t value1;
    DGetStateHash1(d_tablebase_size, idx1, value1, state);
    for (int j = 0; j < kBucketSize; j++) {
        uint64_t save_value = d_tablebase[(idx1*kBucketSize) + j];
        if (save_value == value1) { // already in tablebase
            return true;
        }
        if (save_value == (idx1 ^ kNeurtralElementXOR)) { // empty
            return false;
        }
    }
    uint64_t idx2;
    uint64_t value2;
    DGetStateHash2(d_tablebase_size, idx2, value2, state);
    for (int j = 0; j < kBucketSize; j++) {
        uint64_t save_value = d_tablebase[((idx2+d_tablebase_size)*kBucketSize) + j];
        if (save_value == value2) { // already in tablebase
            return true;
        }
        if (save_value == (idx2 ^ kNeurtralElementXOR)) { // empty
            return false;
        }
    }
    return false;
}
}
