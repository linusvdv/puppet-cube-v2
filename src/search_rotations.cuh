#include <cstdint>


constexpr int kURotationSize = 32;


struct URotations {
    uint64_t data[4];

    struct ByteRef {
        uint64_t* word;
        uint8_t   shift;

        // read
        __host__ __device__
        operator uint8_t() const {
            return uint8_t((*word >> shift) & 0xFF);
        }

        // write
        __host__ __device__
        void set(uint8_t v) {
            *word = (*word & ~(uint64_t(0xFF) << shift)) |
                    (uint64_t(v) << shift);
        }

        __host__ __device__
        ByteRef& operator=(uint8_t v) { set(v); return *this; }

        // prefix ++
        __host__ __device__
        ByteRef& operator++() {
            set(uint8_t(*this + 1));
            return *this;
        }

        // postfix ++
        __host__ __device__
        uint8_t operator++(int) {
            uint8_t old = *this;
            set(uint8_t(old + 1));
            return old;
        }

        // prefix --
        __host__ __device__
        ByteRef& operator--() {
            set(uint8_t(*this - 1));
            return *this;
        }

        // postfix --
        __host__ __device__
        uint8_t operator--(int) {
            uint8_t old = *this;
            set(uint8_t(old - 1));
            return old;
        }

        // unary operators
        __host__ __device__
        uint8_t operator~() const { return uint8_t(~uint8_t(*this)); }

        __host__ __device__
        uint8_t operator+() const { return uint8_t(*this); }

        __host__ __device__
        uint8_t operator-() const { return uint8_t(-uint8_t(*this)); }


        // compound assignment operators

        __host__ __device__ ByteRef& operator+=(uint8_t v) { set(uint8_t(*this + v)); return *this; }
        __host__ __device__ ByteRef& operator-=(uint8_t v) { set(uint8_t(*this - v)); return *this; }
        __host__ __device__ ByteRef& operator*=(uint8_t v) { set(uint8_t(*this * v)); return *this; }
        __host__ __device__ ByteRef& operator/=(uint8_t v) { set(uint8_t(*this / v)); return *this; }
        __host__ __device__ ByteRef& operator%=(uint8_t v) { set(uint8_t(*this % v)); return *this; }

        __host__ __device__ ByteRef& operator&=(uint8_t v) { set(uint8_t(*this & v)); return *this; }
        __host__ __device__ ByteRef& operator|=(uint8_t v) { set(uint8_t(*this | v)); return *this; }
        __host__ __device__ ByteRef& operator^=(uint8_t v) { set(uint8_t(*this ^ v)); return *this; }

        __host__ __device__ ByteRef& operator<<=(uint8_t v) { set(uint8_t(*this << v)); return *this; }
        __host__ __device__ ByteRef& operator>>=(uint8_t v) { set(uint8_t(*this >> v)); return *this; }

    };

    // non-const index
    __host__ __device__
    ByteRef operator[](size_t idx) {
        return ByteRef{ &data[idx / 8], uint8_t((idx % 8) * 8) };
    }

    // const index
    __host__ __device__
    uint8_t operator[](size_t idx) const {
        return uint8_t(data[idx/8] >> ((idx % 8) * 8));
    }
};
