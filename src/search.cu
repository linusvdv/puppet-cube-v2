#include <cuda_runtime_api.h>
#include <cuda.h>
#include <cub/cub.cuh>
#include <driver_types.h>
#include <cassert>
#include <cstdint>
#include <queue>
#include <stop_token>
#include <vector>

#include "BCHTSet.cuh"
#include "BCHTSet.hpp"
#include "cube.cuh"
#include "cube.hpp"
#include "logger.hpp"
#include "utils.hpp"
#include "cuda_memory_transfer.cuh"
#include "search.hpp"
#include "search_rotations.cuh"
#include "settings.hpp"
#include "tablebase.hpp"


__device__ inline uint8_t GetMaxHeuristic(uint16_t& corner_orientation, uint16_t& corner_position,
                                                   uint16_t& edge_orientation, uint32_t& edge_position_1, uint32_t& edge_position_2,
                                                   uint16_t& corner_heuristic, uint8_t& edge_heuristic_1, uint8_t& edge_heuristic_2) {
    if (corner_heuristic == uint16_t(-1)) {
        corner_heuristic = d_corner_heuristics[(corner_orientation*kNumCornerPositions) + corner_position];
    }
    if (edge_heuristic_1 == uint8_t(-1)) {
        edge_heuristic_1 = d_edge_heuristics[(edge_orientation*kNumEdgePositions) + edge_position_1];
    }
    if (edge_heuristic_2 == uint8_t(-1)) {
        uint32_t orientation = edge_orientation;
        orientation |= (__popc(orientation)%2) << (kNumEdges-1); // get last bit using even num bits parity
        uint32_t orientation_r = 0;
        for (int i = 1; i < kNumEdges; i++) {
            orientation_r |= ((orientation >> i) & uint32_t(1)) << (kNumEdges-1-i);
        }

        uint32_t position = edge_position_2;
        uint32_t position_r = 0;
        uint32_t temp = kNumEdgePositions;
        for (int i = kNumEdges-1; i >= 6; i--) { // NOLINT
            temp /= i+1;
            position_r *= i+1;
            position_r += i - ((position / temp) % (i + 1));
        }
        edge_heuristic_2 = d_edge_heuristics[(orientation_r*kNumEdgePositions) + position_r];
    }
    return max(uint8_t(corner_heuristic & ((uint16_t(1) << 8) - 1)), max(edge_heuristic_1, edge_heuristic_2));
}


__device__ inline bool DRotate(uint16_t& corner_orientation, uint16_t& corner_position,
                               uint16_t& edge_orientation, uint32_t& edge_position_1, uint32_t& edge_position_2,
                               uint16_t& corner_heuristic, uint8_t& edge_heuristic_1, uint8_t& edge_heuristic_2,
                               uint16_t& prev_corner_orientation, uint16_t& prev_corner_position,
                               uint16_t& prev_edge_orientation, uint32_t& prev_edge_position_1, uint32_t& prev_edge_position_2,
                               uint16_t& prev_corner_heuristic, uint8_t& prev_edge_heuristic_1, uint8_t& prev_edge_heuristic_2,
                               const uint8_t& rotation, const bool& rev) {
    // check legality only on front moves and when not doing slice moves
    if (!rev && rotation < 12) {
        if (corner_heuristic == uint16_t(-1)) {
            corner_heuristic = d_corner_heuristics[(corner_orientation*kNumCornerPositions) + corner_position];
        }
        if (((corner_heuristic >> (rotation / 4 * 2 + rotation%2 + 8)) & 1) == 0) { // get important rotation bit
            return false;
        }
    }
    if (!rev) {
        prev_corner_orientation = corner_orientation;
        prev_corner_position = corner_position;
        prev_edge_orientation = edge_orientation;
        prev_edge_position_1 = edge_position_1;
        prev_edge_position_2 = edge_position_2;
        prev_corner_heuristic = corner_heuristic;
        prev_edge_heuristic_1 = edge_heuristic_1;
        prev_edge_heuristic_2 = edge_heuristic_2;
    }
    corner_heuristic = uint16_t(-1);
    edge_heuristic_1 = uint8_t(-1);
    edge_heuristic_2 = uint8_t(-1);
    corner_orientation = d_corner_orientations[(corner_orientation*kNumRotations) + rotation];
    corner_position = d_corner_positions[(corner_position*kNumRotations) + rotation];
    edge_orientation = d_edge_orientations[(edge_orientation*kNumRotations) + rotation];
    edge_position_1 = d_edge_positions[(edge_position_1*kNumRotations) + rotation];
    edge_position_2 = d_edge_positions[(edge_position_2*kNumRotations) + rotation];
    return true;
}


// go to previous position if in prev registers
__device__ inline void DUndoRotate(uint16_t& corner_orientation, uint16_t& corner_position,
                                   uint16_t& edge_orientation, uint32_t& edge_position_1, uint32_t& edge_position_2,
                                   uint16_t& corner_heuristic, uint8_t& edge_heuristic_1, uint8_t& edge_heuristic_2,
                                   uint16_t& prev_corner_orientation, uint16_t& prev_corner_position,
                                   uint16_t& prev_edge_orientation, uint32_t& prev_edge_position_1, uint32_t& prev_edge_position_2,
                                   uint16_t& prev_corner_heuristic, uint8_t& prev_edge_heuristic_1, uint8_t& prev_edge_heuristic_2,
                                   uint8_t& rotation_idx, uint64_t& rotations_1, uint64_t& rotations_2) {
    rotation_idx--;
    if (prev_corner_orientation != uint16_t(-1) && rotation_idx != uint8_t(-1)) {
        corner_orientation = prev_corner_orientation;
        corner_position = prev_corner_position;
        edge_orientation = prev_edge_orientation;
        edge_position_1 = prev_edge_position_1;
        edge_position_2 = prev_edge_position_2;
        corner_heuristic = prev_corner_heuristic;
        edge_heuristic_1 = prev_edge_heuristic_1;
        edge_heuristic_2 = prev_edge_heuristic_2;
        prev_corner_orientation = uint16_t(-1); // only one that needs to be reset this indecates that all are not usefull
        RotationsXOR(rotations_1, rotations_2, rotation_idx, 1<<7);  // NOLINT
        RotationsAdd(rotations_1, rotations_2, rotation_idx, 1);
        if (RotationsAt(rotations_1, rotations_2, rotation_idx) == kNumRotations) {
            RotationsSet(rotations_1, rotations_2, rotation_idx, 0);
            rotation_idx--;
        }
    }
}


__device__ inline void DRevRotation(uint8_t& rotation) {
    if (rotation % 2 == 0) {
        rotation += 1;
    }
    else {
        rotation -= 1;
    }
}


__constant__ uint32_t num_gpu_threads;
__constant__ uint8_t cur_depth;
__constant__ uint8_t tb_depth;
__device__ unsigned long long num_gpu_positions;


__device__ inline uint32_t GetNumRotationsLeft(const DState& state, const uint8_t& rotation) {
    uint32_t corner_heuristic = d_corner_heuristics[(state.hash_1*kNumCornerPositions) + (state.hash_2 >> 20)];
    uint32_t cnt = 0;
    for (uint8_t rot = (rotation ^ uint8_t(1 << 7)) + 1; rot < kNumRotations; rot++) {
        if (((corner_heuristic >> (rot / 4 * 2 + rot%2 + 8)) & 1) == 0) { // get important rotation bit
            cnt++;
        }
    }
    return cnt;
}


__global__ void DeviceLeafSearch (uint8_t* d_rotation_idxs, uint8_t* d_starting_depths,  // general information
                                  uint64_t* d_rotations_1, uint64_t* d_rotations_2,  // rotations
                                  uint16_t* d_corner_orientations, uint16_t* d_corner_positions, uint16_t* d_edge_orientations, uint32_t* d_edge_positions_1, uint32_t* d_edge_positions_2,
                                  DState* d_starting_states,
                                  int* first_sol, DState* sol_state, // only one element
                                  uint32_t* ps_finished_pos, uint32_t* ps_num_rotations // prefix sums for split
                                  ) {
    // get current leaf thread idx
    uint32_t index = threadIdx.x + (blockIdx.x * blockDim.x);
    if (index >= num_gpu_threads) {
        return;
    }

    // load all global memory to registers
    // all accesses are coaleased

    // general information
    uint8_t rotation_idx = d_rotation_idxs[index];
    uint8_t starting_depth = d_starting_depths[index];
    uint16_t num_position_thread = 0;

    // rotations
    uint64_t rotations_1 = d_rotations_1[index];
    uint64_t rotations_2 = d_rotations_2[index];

    // state
    uint16_t corner_orientation = d_corner_orientations[index];
    uint16_t corner_position = d_corner_positions[index];
    uint16_t edge_orientation = d_edge_orientations[index];
    uint32_t edge_position_1 = d_edge_positions_1[index];
    uint32_t edge_position_2 = d_edge_positions_2[index];
    // heuristics
    uint16_t corner_heuristic = -1;
    uint8_t edge_heuristic_1 = -1;
    uint8_t edge_heuristic_2 = -1;

    // previous state
    uint16_t prev_corner_orientation = -1;
    uint16_t prev_corner_position = -1;
    uint16_t prev_edge_orientation = -1;
    uint32_t prev_edge_position_1 = -1;
    uint32_t prev_edge_position_2 = -1;
    // previous rotations
    uint16_t prev_corner_heuristic = -1;
    uint8_t prev_edge_heuristic_1 = -1;
    uint8_t prev_edge_heuristic_2 = -1;

    // make a constant number of position during each kernal function call
    // this could be way to high
    constexpr int kNumPosBatchSize = 1000;
    for (int cur_pos_batch = 0; cur_pos_batch < kNumPosBatchSize || rotation_idx == 0; cur_pos_batch++) {
        if (rotation_idx == uint8_t(-1)) {
            break;
        }

        // the goal is to search further in the dfs (from the leaf position) and stop if an improvement to the best_depth is not posible any more
        // for each loop cycle it will look at a new position or undo the move it has done during the dfs.
        // the search is structured in a way that the current moment of the search can be saved to global memory and the kernal stops.
        // at the next kernal start the search will continue from the previous search
        // this cube is now during a search phase with the starting position of leaf_thread_idx
        // the state is the current position of the search after all rotations from the leaf starting position

        uint8_t rotation = RotationsAt(rotations_1, rotations_2, rotation_idx) & (uint8_t(-1)>>1);

        // undo the rotation to continue the search on the next subtree
        bool rev = (RotationsAt(rotations_1, rotations_2, rotation_idx) ^ rotation) != 0;
        if (rev) {
            DRevRotation(rotation);
        }

        // do the rotation
        // if it is an illegal search skip this rotation
        if (!DRotate(corner_orientation, corner_position, edge_orientation, edge_position_1, edge_position_2,
                     corner_heuristic, edge_heuristic_1, edge_heuristic_2,
                     prev_corner_orientation, prev_corner_position, prev_edge_orientation, prev_edge_position_1, prev_edge_position_2,
                     prev_corner_heuristic, prev_edge_heuristic_1, prev_edge_heuristic_2,
                     rotation, rev)) {
            RotationsAdd(rotations_1, rotations_2, rotation_idx, 1);
            if ((rotation+1) == kNumRotations) {
                RotationsSet(rotations_1, rotations_2, rotation_idx, 0);
                DUndoRotate(corner_orientation, corner_position, edge_orientation, edge_position_1, edge_position_2,
                            corner_heuristic, edge_heuristic_1, edge_heuristic_2,
                            prev_corner_orientation, prev_corner_position, prev_edge_orientation, prev_edge_position_1, prev_edge_position_2, 
                            prev_corner_heuristic, prev_edge_heuristic_1, prev_edge_heuristic_2,
                            rotation_idx, rotations_1, rotations_2);
            }
            continue;
        }
        // prepare the next rotation
        RotationsXOR(rotations_1, rotations_2, rotation_idx, 1<<7);  // NOLINT

        // undo rotation done increase to next rotation
        if (rev) {
            RotationsAdd(rotations_1, rotations_2, rotation_idx, 1);
            if (RotationsAt(rotations_1, rotations_2, rotation_idx) == kNumRotations) {
                RotationsSet(rotations_1, rotations_2, rotation_idx, 0);
                DUndoRotate(corner_orientation, corner_position, edge_orientation, edge_position_1, edge_position_2,
                            corner_heuristic, edge_heuristic_1, edge_heuristic_2,
                            prev_corner_orientation, prev_corner_position, prev_edge_orientation, prev_edge_position_1, prev_edge_position_2, 
                            prev_corner_heuristic, prev_edge_heuristic_1, prev_edge_heuristic_2,
                            rotation_idx, rotations_1, rotations_2);
            }
            continue;
        }

        // go inside the next position
        rotation_idx++;
        num_position_thread++;

        // not able to improve the current leaf search skip this node
        uint8_t max_heuristic = GetMaxHeuristic(corner_orientation, corner_position, edge_orientation, edge_position_1, edge_position_2,
                                                corner_heuristic, edge_heuristic_1, edge_heuristic_2);

        // check if the current state is in tablebase and is therefore a new best solution
        if (max_heuristic <= tb_depth && DBCHTSetContains(d_tablebase, d_tablebase_size, DState(corner_orientation, corner_position, edge_orientation, edge_position_1, edge_position_2))) {
            int check_first_sol = atomicOr(first_sol, 1);
            if (check_first_sol == 0) { // first solution
                *sol_state = d_starting_states[index];
            }
            break;
        }

        // not possible with the current heuristic
        if (max(tb_depth+1, max_heuristic) + rotation_idx + starting_depth >= cur_depth) {
            DUndoRotate(corner_orientation, corner_position, edge_orientation, edge_position_1, edge_position_2,
                        corner_heuristic, edge_heuristic_1, edge_heuristic_2,
                        prev_corner_orientation, prev_corner_position, prev_edge_orientation, prev_edge_position_1, prev_edge_position_2, 
                        prev_corner_heuristic, prev_edge_heuristic_1, prev_edge_heuristic_2,
                        rotation_idx, rotations_1, rotations_2);
            continue;
        }
    }

    // add num_positions
    __shared__ uint64_t acc_num_pos[kBlockDim];
    acc_num_pos[threadIdx.x] = num_position_thread;
    for (uint32_t stride = blockDim.x/2; stride >= 1; stride /= 2) {
        __syncthreads();
        if (threadIdx.x < stride) {
            acc_num_pos[threadIdx.x] += acc_num_pos[threadIdx.x + stride];
        }
    }
    if (threadIdx.x == 0) {
        atomicAdd(&num_gpu_positions, static_cast<unsigned long long>(acc_num_pos[0]));
    }

    // 1 if the position is finished 0 otherwise
    ps_finished_pos[index] = int(rotation_idx == uint8_t(-1));

    if (rotation_idx == uint8_t(-1)) {
        // remove finished positions
        rotations_1 = 0;
        rotations_2 = 0;

        ps_num_rotations[index] = 0;
    }
    else {
        ps_num_rotations[index] = GetNumRotationsLeft(d_starting_states[index], RotationsAt(rotations_1, rotations_2, 0));
    }

    // save back to global memory;
    d_rotation_idxs[index] = rotation_idx;
    d_starting_depths[index] = starting_depth;

    // rotations
    d_rotations_1[index] = rotations_1;
    d_rotations_2[index] = rotations_2;

    // state
    d_corner_orientations[index] = corner_orientation;
    d_corner_positions[index] = corner_position;
    d_edge_orientations[index] = edge_orientation;
    d_edge_positions_1[index] = edge_position_1;
    d_edge_positions_2[index] = edge_position_2;

    // revers and heuristic are not stored as the compution is nearly as fast as the global access itself
}


__global__ void MemInitialization(uint8_t* rotation_idxs, uint64_t* rotations_1, uint64_t* rotations_2) {
    // get current leaf thread idx
    uint32_t index = threadIdx.x + (blockIdx.x * blockDim.x);
    if (index >= num_gpu_threads) {
        return;
    }

    rotation_idxs[index] = -1;
    rotations_1[index] = 0;
    rotations_2[index] = 0;
}


void InitializeUploadToDevice () {
    for (int i = 0; i < Settings::GetDeviceCount(); i++) {
        // upload it to the correct device
        cudaSetDevice(i);
        MemcpyToSymbol(uint32_t(Settings::GetNumGPUThreads()), num_gpu_threads);
        MemcpyToSymbol(Settings::GetTBDepth(), tb_depth);
    }
}


void ChangeCurDepth (uint8_t depth) {
    for (int i = 0; i < Settings::GetDeviceCount(); i++) {
        // upload it to the correct device
        cudaSetDevice(i);
        MemcpyToSymbol(depth, cur_depth);
    }
}


uint64_t ResetNumGPUPositions () {
    uint64_t total_num_gpu_positions = 0;
    for (int i = 0; i < Settings::GetDeviceCount(); i++) {
        cudaSetDevice(i);
        unsigned long long cur_num_gpu_positions;
        MemcpyFromSymbol(cur_num_gpu_positions, num_gpu_positions);
        total_num_gpu_positions += cur_num_gpu_positions;
        cur_num_gpu_positions = 0;
        MemcpyToSymbol(cur_num_gpu_positions, num_gpu_positions);
    }
    return total_num_gpu_positions;
}


void DeviceLeafManager (std::stop_token stocken, SharedLeafStates& shared_leaf_states,
                        uint64_t& num_positions_leaf, VisitedMap& visited_leaf,
                        std::atomic<uint8_t>& atomic_best_depth, std::pair<State, uint8_t>& best_endstate_leafs,
                        const int& thread_idx) {
    // set the device for this thread
    int gpu_device_idx = thread_idx % Settings::GetDeviceCount();
    cudaError_t err = cudaSetDevice(gpu_device_idx);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }

    size_t grid_dim = ((Settings::GetNumGPUThreads()-1)/kBlockDim)+1;

    // create stream
    cudaStream_t cuda_stream;
    cudaStreamCreate(&cuda_stream);

    // device memory only
    uint8_t* rotation_idxs; MallocOnDevice(rotation_idxs, Settings::GetNumGPUThreads());
    uint8_t* starting_depths; MallocOnDevice(starting_depths, Settings::GetNumGPUThreads());
    uint64_t* rotations_1; MallocOnDevice(rotations_1, Settings::GetNumGPUThreads());
    uint64_t* rotations_2; MallocOnDevice(rotations_2, Settings::GetNumGPUThreads());
    uint16_t* corner_orientations; MallocOnDevice(corner_orientations, Settings::GetNumGPUThreads());
    uint16_t* corner_position; MallocOnDevice(corner_position, Settings::GetNumGPUThreads());
    uint16_t* edge_orientations; MallocOnDevice(edge_orientations, Settings::GetNumGPUThreads());
    uint32_t* edge_position_1; MallocOnDevice(edge_position_1, Settings::GetNumGPUThreads());
    uint32_t* edge_position_2; MallocOnDevice(edge_position_2, Settings::GetNumGPUThreads());
    DState* starting_states; MallocOnDevice(starting_states, Settings::GetNumGPUThreads());
    uint32_t* ps_finished_pos; MallocOnDevice(ps_finished_pos, Settings::GetNumGPUThreads());
    uint32_t* ps_num_rotations; MallocOnDevice(ps_num_rotations, Settings::GetNumGPUThreads());
    MemInitialization<<<grid_dim, kBlockDim, 0, cuda_stream>>>(rotation_idxs, rotations_1, rotations_2);

    // copied after every kernal
    int first_sol = 0;
    DState sol_state = DState();
    std::vector<std::pair<DState, uint8_t>> position_queue(Settings::GetNumGPUThreads(), {DState(), 0});

    // pin host code
    HostRegister(position_queue);

    // device updated
    int* d_first_sol;
    DState* d_sol_state;
    std::pair<State, uint8_t>* d_position_queue;

    // allocate on device update
    MallocOnDevice(d_first_sol, 1);
    MemcpyToDeviceStream(first_sol, d_first_sol, cuda_stream);
    MallocOnDevice(d_sol_state, 1);
    MemcpyToDeviceStream(sol_state, d_sol_state, cuda_stream);
    MallocOnDevice(position_queue, d_position_queue);

    // local buffer
    std::shared_ptr<phmap::flat_hash_map<State, uint8_t>> local_buffer;
    auto local_buffer_ptr = (*local_buffer).begin();

    while (true) {
        for (std::pair<DState, uint8_t>& cur_position_queue : position_queue) {
            if (cur_position_queue.first.hash_1 != uint16_t(-1)) {
                continue;
            }
            if (local_buffer_ptr == (*local_buffer).end()) {
                std::lock_guard<std::mutex> lock(shared_leaf_states.mtx);
                if (!shared_leaf_states.shared_ptrs.empty()) {
                    local_buffer = std::move(shared_leaf_states.shared_ptrs.front());
                    local_buffer_ptr = (*local_buffer).begin();
                    shared_leaf_states.shared_ptrs.pop();
                    shared_leaf_states.cv.notify_one();
                }
                else {
                    break;
                }
            }
            cur_position_queue = *(local_buffer_ptr++);
        }
    }

    LOG_EXTRA(SkipSpace("#"), thread_idx, "finished with all kernels");

    // free all memory
    FreeCudaPointer(d_rotation_idxs);

    // Unpin host data
    cudaHostUnregister(rotation_idxs.data());

    cudaStreamDestroy(cuda_stream);
}
