#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <string>
#include <vector>

#include "logger.hpp"
#include "settings.hpp"


using Vec3i = std::array<int, 3>;
using Mat3i = std::array<std::array<int, 3>, 3>;


constexpr uint64_t Factorial(int n) {
    return n <= 1 ? 1 : n * Factorial(n-1);
}


template <typename T>
inline void AtomicMin(std::atomic<T>& lhs, T rhs) {
    static_assert(std::is_integral_v<T>, "AtomicMin requires integral type");

    T old = lhs.load(std::memory_order_relaxed);

    while (old > rhs &&
           !lhs.compare_exchange_weak(
               old, rhs,
               std::memory_order_relaxed,
               std::memory_order_relaxed))
    {}
}


template<typename T, std::size_t N>
std::array<std::array<T, N>, N> MatMul(const std::array<std::array<T, N>, N>& lhs,
                                         const std::array<std::array<T, N>, N>& rhs) {
    std::array<std::array<T, N>, N> res = {};
    for (std::size_t i = 0; i < N; i++) {
        for (std::size_t j = 0; j < N; j++) {
            for (std::size_t k = 0; k < N; k++) {
                res[i][j] += lhs[i][k] * rhs[k][j];
            }
        }
    }
    return res;
}


template<typename T, std::size_t N>
std::array<std::array<T, N>, N> MatTrans(const std::array<std::array<T, N>, N>& mat) {
    std::array<std::array<T, N>, N> res = {};
    for (std::size_t i = 0; i < N; i++) {
        for (std::size_t j = 0; j < N; j++) {
            res[i][j] = mat[j][i];
        }
    }
    return res;
}


template<typename T, std::size_t N>
std::array<T, N> MatVecMul(const std::array<std::array<T, N>, N>& mat,
                             const std::array<T, N>& vec) {
    std::array<T, N> res = {};
    for (std::size_t i = 0; i < N; i++) {
        for (std::size_t j = 0; j < N; j++) {
            res[i] += mat[i][j] * vec[j];
        }
    }
    return res;
}


// place where the precomputation is stored
inline std::string GetFilePath (std::string file_name) {
    // path/to/puppet-cube-v2/precomputation/file_name
    return Settings::GetRootPath() + "precomputation/" + file_name;
}


// if there is no file storing the precomputation run the precomputation
template<typename T, typename Generator>
void LoadOrGenerate(const std::string file_name, std::vector<T>& target, size_t expected_size,
                    Generator&& generate_func, const std::string& step_tag) {
    const std::string path = GetFilePath(file_name);
    if (std::FILE* file = std::fopen(path.c_str(), "rb")) {
        // read content of file
        target.resize(expected_size);
        if (std::fread(target.data(), sizeof(T), expected_size, file) == expected_size) {
            LOG_ALL(step_tag, "read from file");
            LOG_MEMORY();
        }
        else {
            LOG_CRITICAL(step_tag, "was not able to read file", path);
        }
        std::fclose(file);
    }
    // opening of the file failed
    else {
        // do the precomputation
        LOG_ALL(step_tag, "precompute ...");
        generate_func();
        LOG_MEMORY();

        if (target.size() != expected_size) {
            LOG_CRITICAL(step_tag, "Wrong precomputation size:", target.size(), "/", expected_size);
        }

        // save to file
        if (std::FILE* file = std::fopen(path.c_str(), "wb")) {
            if (std::fwrite(target.data(), sizeof(T), expected_size, file) != expected_size) {
                LOG_ERROR(step_tag, "failed to write full file");
            }
            std::fclose(file);
        }
        else {
            LOG_ERROR(step_tag, "not able to save precomputation to file");
        }
    }
}
