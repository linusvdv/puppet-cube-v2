#pragma once
#include "rotation.hpp"


struct RegRotations {
    int8_t idx = -1;
    int8_t finish_idx = 0;
    uint8_t finish_rot = kNumRot;
    uint64_t d1 = 0;
    uint64_t d2 = 0;
};


__host__ __device__ inline uint8_t RotationsAt(const RegRotations& reg_rotations) {
    if (reg_rotations.idx < 8) {
        return uint8_t(reg_rotations.d1 >> (8 * reg_rotations.idx));
    }
    return uint8_t(reg_rotations.d2 >> (8 * (reg_rotations.idx-8)));
}


__host__ __device__ inline uint8_t RotationsAtPrev(const RegRotations& reg_rotations) {
    if (reg_rotations.idx <= 0) {
        return uint8_t(-1);
    }
    if (reg_rotations.idx-1 < 8) {
        return uint8_t(reg_rotations.d1 >> (8 * (reg_rotations.idx-1)));
    }
    return uint8_t(reg_rotations.d2 >> (8 * ((reg_rotations.idx-1)-8)));
}


template<uint8_t idx>
__host__ __device__ inline uint8_t RotationsAt(const RegRotations& reg_rotations) {
    if constexpr (idx < 8) {
        return uint8_t(reg_rotations.d1 >> (8 * idx));
    }
    return uint8_t(reg_rotations.d2 >> (8 * (idx-8)));
}


__host__ __device__ inline void RotationsSet(RegRotations& reg_rotations, const uint8_t& value) {
    if (reg_rotations.idx < 8) {
        reg_rotations.d1 ^= uint64_t(uint8_t(reg_rotations.d1 >> (8 * reg_rotations.idx)) ^ value) << (8 * reg_rotations.idx);
    }
    else {
        reg_rotations.d2 ^= uint64_t(uint8_t(reg_rotations.d2 >> (8 * (reg_rotations.idx-8))) ^ value) << (8 * (reg_rotations.idx-8));
    }
}


// it is guarantied that a rotation add does not overflow into the next idx (this code does not account for it!)
__host__ __device__ inline void RotationsAdd(RegRotations& reg_rotations, const uint8_t& value) {
    if (reg_rotations.idx < 8) {
        reg_rotations.d1 += uint64_t(value) << (8 * reg_rotations.idx);
    }
    else {
        reg_rotations.d2 += uint64_t(value) << (8 * (reg_rotations.idx-8));
    }
}


__host__ __device__ inline void RotationsXOR(RegRotations& reg_rotations, const uint8_t& value) {
    if (reg_rotations.idx < 8) {
        reg_rotations.d1 ^= uint64_t(value) << (8 * reg_rotations.idx);
    }
    else {
        reg_rotations.d2 ^= uint64_t(value) << (8 * (reg_rotations.idx-8));
    }
}
