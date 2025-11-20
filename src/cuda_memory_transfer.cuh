#pragma once
#include <cuda.h>
#include <cuda_device_runtime_api.h>
#include <cuda_runtime_api.h>
#include <driver_types.h>
#include <vector>

#include "logger.hpp"


// UploadToDeviceSymbol
template<typename T>
void UploadToDeviceSymbol(const std::vector<T>& data, T*& d_pointer) {
    T* temp_pointer = nullptr;
    cudaError_t err = cudaMalloc((void **)&temp_pointer, sizeof(T)*data.size());
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
    err = cudaMemcpy(temp_pointer, data.data(), sizeof(T)*data.size(), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
    err = cudaMemcpyToSymbol(d_pointer, &temp_pointer, sizeof(T*));
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
}


template<typename T1, typename T2>
void UploadToDeviceSymbol(const std::vector<T1>& data, T2*& d_pointer) {
    static_assert(sizeof(T1) == sizeof(T2));
    static_assert(alignof(T1) == alignof(T2));

    T2* temp_pointer = nullptr;
    cudaError_t err = cudaMalloc((void **)&temp_pointer, sizeof(T1)*data.size());
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
    err = cudaMemcpy(temp_pointer, data.data(), sizeof(T1)*data.size(), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
    err = cudaMemcpyToSymbol(d_pointer, &temp_pointer, sizeof(T2*));
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
}


// UploadToDevice
template<typename T1, typename T2>
void UploadToDevice(const std::vector<T1>& data, T2*& d_pointer) {
    static_assert(sizeof(T1) == sizeof(T2));
    static_assert(alignof(T1) == alignof(T2));

    cudaError_t err = cudaMalloc((void **)&d_pointer, sizeof(T1)*data.size());
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
    err = cudaMemcpy(d_pointer, data.data(), sizeof(T1)*data.size(), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
}


// UploadToSymbol
template<typename T>
void UploadToSymbol(const T& data, T& d_pointer) {
    cudaError_t err = cudaMemcpyToSymbol(d_pointer, &data, sizeof(T));
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
}


// DownloadFromDevice
template<typename T1, typename T2>
void DownloadFromDevice(std::vector<T1>& data, T2*& d_pointer) {
    static_assert(sizeof(T1) == sizeof(T2));
    static_assert(alignof(T1) == alignof(T2));

    cudaError_t err = cudaMemcpy(data.data(), d_pointer, sizeof(T1)*data.size(), cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
    err = cudaFree(d_pointer);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
    d_pointer = nullptr;
}
