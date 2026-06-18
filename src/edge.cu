#include <cstdint>
#include <vector>

#include "cuda_memory_transfer.cuh"
#include "edge.hpp"
#include "rotation.hpp"


namespace edge {
// this is the data used for rotation
extern std::vector<std::array<uint8_t, kNumRot>> rotation_change;
extern std::vector<std::array<uint64_t, kNumRot>> position_change;
extern std::vector<std::array<uint16_t, kNumSym>> symmetry_change;
extern std::vector<std::array<std::array<uint16_t, kNumRot>, kNumSym>> orientation_change;

// heuristic
extern std::vector<std::array<uint32_t, kNumOrient/kNumStoredPerBucket>> heuristic_bucket;
extern std::vector<uint64_t> heuristic_value;

__constant__ uint8_t* d_rotation_change = nullptr;
__constant__ uint64_t* d_position_change = nullptr;
__constant__ uint16_t* d_symmetry_change = nullptr;
__constant__ uint16_t* d_orientation_change = nullptr;

__constant__ uint32_t* d_heuristic_bucket = nullptr;
__constant__ uint64_t* d_heuristic_value = nullptr;


void UploadPrecomputationToDevice() {
    for (int i = 0; i < Settings::GetDeviceCount(); i++) {
        // upload it to the correct device
        cudaSetDevice(i);

        // edge precomputation
        UploadToDeviceSymbol(rotation_change, d_rotation_change);
        UploadToDeviceSymbol(position_change, d_position_change);
        UploadToDeviceSymbol(symmetry_change, d_symmetry_change);
        UploadToDeviceSymbol(orientation_change, d_orientation_change);
        UploadToDeviceSymbol(heuristic_bucket, d_heuristic_bucket);
        UploadToDeviceSymbol(heuristic_value, d_heuristic_value);
    }
}
}
