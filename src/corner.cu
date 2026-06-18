#include <vector>

#include "corner.hpp"
#include "cuda_memory_transfer.cuh"
#include "rotation.hpp"

namespace corner{
extern std::vector<std::array<uint16_t, kNumRot>> position_change;
extern std::vector<std::array<uint16_t, kNumRot>> orientation_change;
extern std::vector<std::array<uint64_t, kNumOrient>> heuristic;

__constant__ uint16_t* d_position_change = nullptr;
__constant__ uint16_t* d_orientation_change = nullptr;
__constant__ uint64_t* d_heuristic = nullptr;


void UploadPrecomputationToDevice() {
    for (int i = 0; i < Settings::GetDeviceCount(); i++) {
        // upload it to the correct device
        cudaSetDevice(i);

        // corner precomputation
        UploadToDeviceSymbol(position_change, d_position_change);
        UploadToDeviceSymbol(orientation_change, d_orientation_change);
        UploadToDeviceSymbol(heuristic, d_heuristic);
    }
}
}
