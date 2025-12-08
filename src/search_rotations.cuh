#pragma once

constexpr int kURotationSize = 32;


struct URotations {
    uint64_t data[4];

    // const index
    __host__ __device__
    uint8_t At(size_t idx) const {
        return data[idx/8] >> ((idx % 8) * 8);
    }

    __host__ __device__
    void BitOR(size_t idx, uint8_t value) {
        data[idx/8] |= uint64_t(value) << ((idx % 8) * 8);
    }

    __host__ __device__
    void BitXOR(size_t idx, uint8_t value) {
        data[idx/8] ^= uint64_t(value) << ((idx % 8) * 8);
    }

    __host__ __device__
    void Set(size_t idx, uint8_t value) {
        data[idx/8] &= ~(uint64_t(uint8_t(-1)) << ((idx % 8) * 8));
        BitOR(idx, value);
    }

    __host__ __device__
    void Add(size_t idx, uint8_t value) {
        Set(idx, At(idx)+value);
    }
};
