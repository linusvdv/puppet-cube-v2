#pragma once

__host__ __device__ inline uint8_t RotationsAt(const uint64_t& rotations_1, const uint64_t& rotations_2, const uint8_t& idx) {
    if (idx < 8) {
        return uint8_t(rotations_1 >> (8 * idx));
    }
    return uint8_t(rotations_2 >> (8 * (idx-8)));
}


__host__ __device__ inline void RotationsSet(uint64_t& rotations_1, uint64_t& rotations_2, const uint8_t& idx, const uint8_t& value) {
    if (idx < 8) {
        rotations_1 ^= uint64_t(uint8_t(rotations_1 >> (8 * idx)) ^ value) << (8 * idx);
    }
    else {
        rotations_2 ^= uint64_t(uint8_t(rotations_2 >> (8 * (idx-8))) ^ value) << (8 * (idx-8));
    }
}


// it is guarantied that a rotation add does not overflow into the next idx (this code does not account for it!)
__host__ __device__ inline void RotationsAdd(uint64_t& rotations_1, uint64_t& rotations_2, const uint8_t& idx, const uint8_t& value) {
    if (idx < 8) {
        rotations_1 += uint64_t(value) << (8 * idx);
    }
    else {
        rotations_2 += uint64_t(value) << (8 * (idx-8));
    }
}


__host__ __device__ inline void RotationsXOR(uint64_t& rotations_1, uint64_t& rotations_2, const uint8_t& idx, const uint8_t& value) {
    if (idx < 8) {
        rotations_1 ^= uint64_t(value) << (8 * idx);
    }
    else {
        rotations_2 ^= uint64_t(value) << (8 * (idx-8));
    }
}
