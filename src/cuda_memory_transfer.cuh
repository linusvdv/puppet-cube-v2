#pragma once
#include <cuda.h>
#include <driver_types.h>
#include <vector>

#include "logger.hpp"


template<typename T1, typename T2>
concept SameDataTypeStructure =
    sizeof(T1) == sizeof(T2) &&
    alignof(T1) == alignof(T2);


template<typename T1, typename T2>
requires SameDataTypeStructure<T1, T2>
inline void MallocOnDevice(const std::vector<T1>& data, T2*& pointer) {
    cudaError_t err = cudaMalloc((void **)&pointer, sizeof(T1)*data.size());
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
}


template<typename T>
void MallocOnDevice(T*& pointer, size_t size) {
    cudaError_t err = cudaMalloc((void **)&pointer, sizeof(T)*size);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
}


template<typename T1, typename T2>
requires SameDataTypeStructure<T1, T2>
inline void MallocOnDeviceStream(const std::vector<T1>& data, T2*& pointer, const cudaStream_t& cuda_stream) {
    cudaError_t err = cudaMallocAsync((void **)&pointer, sizeof(T1)*data.size(), cuda_stream);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
}


template<typename T>
void MallocOnDeviceStream(T*& pointer, size_t size, const cudaStream_t& cuda_stream) {
    cudaError_t err = cudaMallocAsync((void **)&pointer, sizeof(T)*size, cuda_stream);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
}


template<typename T1, typename T2>
requires SameDataTypeStructure<T1, T2>
inline void MemcpyToDevice(const std::vector<T1>& data, T2*& pointer) {
    cudaError_t err = cudaMemcpy(pointer, data.data(), sizeof(T1)*data.size(), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
}


template<typename T1, typename T2>
requires SameDataTypeStructure<T1, T2>
inline void MemcpyToDeviceStream(const std::vector<T1>& data, T2*& pointer, const cudaStream_t& cuda_stream) {
    cudaError_t err = cudaMemcpyAsync(pointer, data.data(), sizeof(T1)*data.size(), cudaMemcpyHostToDevice, cuda_stream);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
}


template<typename T>
inline void MemcpyToDeviceStream(T& data, T*& pointer, const cudaStream_t& cuda_stream) {
    cudaError_t err = cudaMemcpyAsync(pointer, &data, sizeof(T), cudaMemcpyHostToDevice, cuda_stream);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
}


template<typename T>
inline void MemcpyToSymbol (const T& data, T& d_pointer) {
    cudaError_t err = cudaMemcpyToSymbol(d_pointer, &data, sizeof(T));
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
}


template<typename T>
inline void MemcpyFromSymbol (T& data, T& d_pointer) {
    cudaError_t err = cudaMemcpyFromSymbol(&data, d_pointer, sizeof(T));
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
}


template<typename T>
inline void HostRegister(std::vector<T>& data) {
    cudaError_t err = cudaHostRegister(data.data(), data.size() * sizeof(T), cudaHostRegisterDefault);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
}


template<typename T1, typename T2>
requires SameDataTypeStructure<T1, T2>
inline void MemcpyFromDevice(std::vector<T1>& data, T2*& pointer) {
    cudaError_t err = cudaMemcpy(data.data(), pointer, sizeof(T1)*data.size(), cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
}


template<typename T1, typename T2>
requires SameDataTypeStructure<T1, T2>
inline void MemcpyFromDeviceStream(std::vector<T1>& data, T2*& pointer, const cudaStream_t& cuda_stream) {
    cudaError_t err = cudaMemcpyAsync(data.data(), pointer, sizeof(T1)*data.size(), cudaMemcpyDeviceToHost, cuda_stream);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
}

template<typename T>
inline void MemcpyFromDeviceStream(T& data, T*& pointer, const cudaStream_t& cuda_stream) {
    cudaError_t err = cudaMemcpyAsync(&data, pointer, sizeof(T), cudaMemcpyDeviceToHost, cuda_stream);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
}


template<typename T>
inline void FreeCudaPointer(T& d_pointer) {
    cudaError_t err = cudaFree(d_pointer);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
    d_pointer = nullptr;
}


template<typename T1, typename T2>
requires SameDataTypeStructure<T1, T2>
void UploadToDeviceSymbol(const std::vector<T1>& data, T2*& d_pointer) {
    T2* temp_pointer = nullptr;
    MallocOnDevice(data, temp_pointer);
    MemcpyToDevice(data, temp_pointer);
    MemcpyToSymbol(temp_pointer, d_pointer);
}


template<typename T1, typename T2>
requires SameDataTypeStructure<T1, T2>
void UploadToDeviceStream(const std::vector<T1>& data, T2*& d_pointer, const cudaStream_t& cuda_stream) {
    MallocOnDeviceStream(data, d_pointer, cuda_stream);
    MemcpyToDeviceStream(data, d_pointer, cuda_stream);
}


template<typename T1, typename T2>
requires SameDataTypeStructure<T1, T2>
void DownloadFromDevice(std::vector<T1>& data, T2*& d_pointer) {
    MemcpyFromDevice(data, d_pointer);
    FreeCudaPointer(d_pointer);
}
