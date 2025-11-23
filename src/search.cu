#include <algorithm>
#include <cstdint>

#include "cube.cuh"
#include "cube.hpp"
#include "logger.hpp"
#include "utils.hpp"
#include "cuda_memory_transfer.cuh"
#include "search.hpp"
#include "settings.hpp"


struct URotations {
    uint8_t raw[32];  // NOLINT
    uint32_t packed[8];  // NOLINT
};


__global__ void DeviceLeafSearch (std::pair<DState, uint8_t>* d_starting_positions, uint64_t* d_num_positions_leafs, uint8_t* d_best_depths, uint64_t sp_size, uint8_t tb_depth) {
    size_t index = threadIdx.x + (size_t(blockIdx.x) * blockDim.x);
    if (index >= sp_size) {
        return;
    }

    URotations rotations;
    for (int i = 0; i < 32; i++) {
        rotations.raw[i] = 0;
    }
    uint8_t rotation_idx = 0;
    DState state = d_starting_positions[index].first;
    uint64_t num_positions = d_num_positions_leafs[index];
    uint8_t best_depth = d_best_depths[index];
    uint8_t depth_offset = d_starting_positions[index].second;

    num_positions++;

    if (DCube::DTablebaseContains(state)) {
        uint8_t depth = rotation_idx + tb_depth + d_starting_positions[index].second;
        d_best_depths[index] = min(depth, d_best_depths[index]);
        printf("ALREADY in tb: %d\n", int(best_depth));
        return;
    }
    while (rotation_idx != uint8_t(-1)) {
        uint8_t rotation = rotations.raw[rotation_idx] & (uint8_t(-1)>>1);

        if (rotation == kNumRotations) {  // NOLINT
            rotations.raw[rotation_idx] = 0;
            rotation_idx--;
            continue;
        }

        bool rev = (rotations.raw[rotation_idx] ^ rotation) != 0;
        if (rev) {
            rotation = DGetRevRotation(rotation);
        }

        DRotateReturn next_pos = DCube::Rotate(state, rotation);
        if (!next_pos.isLegal) {
            rotations.raw[rotation_idx]++;
            continue;
        }
        state = next_pos.state;

        rotations.raw[rotation_idx] ^= uint8_t(1<<7);  // NOLINT
        if (!rev) {
            DCube cube;
            num_positions++;
            rotation_idx++;

            if (max(tb_depth, cube.GetMaxHeuristic(state)) + rotation_idx + depth_offset < best_depth) {
                if (DCube::DTablebaseContains(state)) {
                    uint8_t depth = rotation_idx + tb_depth + d_starting_positions[index].second;
                    d_best_depths[index] = min(depth, d_best_depths[index]);
                    best_depth = min(depth, best_depth);
                    printf("NEW best: %d\n", int(best_depth));
                }
            }
            else {
                rotations.raw[rotation_idx] = kNumRotations;
                continue;
            }
        }
        else {
            rotations.raw[rotation_idx]++;
        }
    }

    d_num_positions_leafs[index] = num_positions;
}


void DeviceLeafManager (const std::vector<std::pair<State, uint8_t>>& starting_positions, uint64_t& num_positions_leaf, VisitedMap& visited_leaf,
                        std::atomic<uint8_t>& atomic_best_depth, std::pair<State, uint8_t>& best_endstate_leafs,
                        [[maybe_unused]] const uint64_t& leaf_batch_size, const int& thread_idx) {
    uint64_t sp_size = starting_positions.size();

    std::pair<DState, uint8_t>* d_starting_positions;
    UploadToDevice(starting_positions, d_starting_positions);

    std::vector<uint64_t> num_positions_leafs(sp_size, 0);
    uint64_t* d_num_positions_leafs;
    UploadToDevice(num_positions_leafs, d_num_positions_leafs);

    std::vector<uint8_t> best_depths(sp_size, atomic_best_depth);
    uint8_t* d_best_depths;
    UploadToDevice(best_depths, d_best_depths);

    size_t grid_dim = (sp_size/kBlockDim)+1;
    DeviceLeafSearch<<<grid_dim, kBlockDim>>>(d_starting_positions, d_num_positions_leafs, d_best_depths, sp_size, Settings::GetTBDepth());
    cudaDeviceSynchronize();
    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        LOG_CRITICAL("CUDA error:", cudaGetErrorString(err));
    }

    DownloadFromDevice(num_positions_leafs, d_num_positions_leafs);
    for (uint64_t num_positions : num_positions_leafs) {
        num_positions_leaf += num_positions;
    }

    DownloadFromDevice(best_depths, d_best_depths);
    uint8_t best_depth = uint8_t(-1);
    for (uint8_t depth : best_depths) {
        best_depth = std::min(best_depth, depth);
    }

    // new solution do it on the CPU to get the path
    if (best_depth < atomic_best_depth) {
        LOG_ALL("New best sol (GPU):", int(best_depth));
        for (const std::pair<State, uint8_t>& starting_position : starting_positions) {
            uint8_t best_depth = atomic_best_depth;
            LeafSearch(starting_position.first, starting_position.second, best_depth,
                       best_endstate_leafs, visited_leaf,
                       num_positions_leaf, atomic_best_depth, thread_idx);
        }
    }

    err = cudaFree(d_starting_positions);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
}
