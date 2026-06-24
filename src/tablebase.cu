#include <vector>

#include "cuda_memory_transfer.cuh"
#include "tablebase.hpp"
#include "settings.hpp"

namespace tablebase {
extern std::vector<std::vector<std::array<uint64_t, kBucketSize>>> tablebase_depths;

__constant__ uint64_t* d_tablebase = nullptr;
__constant__ uint64_t d_tablebase_size;


void UploadPrecomputationToDevice() {
    for (int i = 0; i < Settings::GetDeviceCount(); i++) {
        // upload it to the correct device
        cudaSetDevice(i);

        // corner precomputation
        UploadToDeviceSymbol(tablebase_depths.back(), d_tablebase);
        MemcpyToSymbol(tablebase_depths.back().size(), d_tablebase_size);
    }
}
}
