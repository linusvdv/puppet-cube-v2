#include <cstdio>
#include <cuda.h>
#include <cuda_device_runtime_api.h>


__global__ void PrintTest() {
    printf("Hi from GPU\n");
}

void Search () {
    PrintTest<<<2,2>>>();
    cudaDeviceSynchronize();
}
