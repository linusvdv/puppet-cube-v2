#include <cuda.h>
#include <cuda/std/bit>
#include <cuda_runtime_api.h>
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cub/cub.cuh>
#include <cassert>
#include <cstdint>
#include <nvtx3/nvtx3.hpp>
#include <queue>
#include <vector>

#include "corner.cuh"
#include "corner.hpp"
#include "cuda_memory_transfer.cuh"
#include "cube.hpp"
#include "duplicate_rotations.hpp"
#include "duplicate_rotations.cuh"
#include "edge.cuh"
#include "edge.hpp"
#include "logger.hpp"
#include "rotation.hpp"
#include "search_rotations.cuh"
#include "settings.hpp"
#include "search_bridge.hpp"
#include "tablebase.cuh"
#include "transposition_table.hpp"


constexpr RegRotations kDefaultRegRotations = RegRotations();


struct DeviceSolution {
    int flag = int(false);
    State state = kNonLegalState;
    RegRotations reg_rotations = kDefaultRegRotations;
};


constexpr DeviceSolution kDefaultDeviceSolution = DeviceSolution();


__constant__ uint32_t num_gpu_threads;
__constant__ uint8_t cur_depth;
__constant__ uint8_t tb_depth;
__constant__ uint64_t duplicate_rotations_constmem[kDuplicateRotationDataSize];


__device__ inline void DRevRotation(uint8_t& rotation) {
    if (rotation % 2 == 0) {
        rotation += 1;
    }
    else {
        rotation -= 1;
    }
}


__global__ void DeviceLeafSearch (const uint8_t* d_starting_depths,
                                  State* d_states,
                                  RegRotations* d_reg_rotations,
                                  DeviceSolution* d_device_solution,
                                  uint64_t* d_num_position_threads) {
    // get current leaf thread idx
    uint32_t index = threadIdx.x + (blockIdx.x * blockDim.x);
    if (index >= num_gpu_threads) {
        return;
    }

    __shared__ uint64_t duplicate_rotations_sharedmem[kDuplicateRotationDataSize];
    if (threadIdx.x == 0) {
        for (int i = 0; i < kDuplicateRotationDataSize; i++) {
            duplicate_rotations_sharedmem[i] = duplicate_rotations_constmem[i];
        }
    }

    // load all global memory to registers
    // all accesses are coaleased

    // state
    State state = d_states[index];
    if (state.edge_pos == (uint32_t)-1) {
        return;
    }

    // general information
    uint16_t num_position_thread = 0;

    uint8_t starting_depth = d_starting_depths[index];

    // rotations
    RegRotations reg_rotations = d_reg_rotations[index];

    // get helpful rotations
    uint64_t full_corner_heuristic = corner::DGetHeuristic(state.corner_pos, state.corner_orient);
    uint8_t corner_heuristic = full_corner_heuristic;
    constexpr uint64_t kRotationsMask = 0x555555555ULL;
    uint64_t rotations = (full_corner_heuristic >> 8) & (full_corner_heuristic >> 9) & kRotationsMask;
    if (2 + corner_heuristic + reg_rotations.idx + starting_depth >= cur_depth) {
        rotations |= (full_corner_heuristic >> 9) & kRotationsMask;
    }
    if (1 + corner_heuristic + reg_rotations.idx + starting_depth >= cur_depth) {
        rotations |= (full_corner_heuristic >> 8) & kRotationsMask;
    }
    rotations |= rotations << 1;
    uint8_t rotation = RotationsAt(reg_rotations);
    rotations |= (1ULL<<(2*rotation))-1;


    // make a constant number of position during each kernal function call
    constexpr int kNumPosBatchSize = 200;
    for (int cur_pos_batch = 0; cur_pos_batch < kNumPosBatchSize; cur_pos_batch++) {
        // the goal is to search further in the dfs (from the leaf position) and stop if an improvement to the best_depth is not posible any more
        // for each loop cycle it will look at a new position or undo the move it has done during the dfs.
        // the search is structured in a way that the current moment of the search can be saved to global memory and the kernal stops.
        // at the next kernal start the search will continue from the previous search
        // this cube is now during a search phase with the starting position of leaf_thread_idx
        // the state is the current position of the search after all rotations from the leaf starting position

        rotation = cuda::std::countr_one(rotations) / 2;
        RotationsSet(reg_rotations, rotation+1);
        rotations |= 3ULL << (2*rotation);

        if (reg_rotations.idx < reg_rotations.finish_idx ||
            (reg_rotations.idx == reg_rotations.finish_idx && rotation >= reg_rotations.finish_rot)) {
            break;
        }

        bool rev = false;
        if (rotation >= kNumRot) {
            RotationsSet(reg_rotations, 0);
            reg_rotations.idx--;
            rotation = RotationsAt(reg_rotations)-1;
            DRevRotation(rotation);
            rev = true;
        }
        else {
            if (reg_rotations.idx > 0) {
                if (IsDuplicateRotation(RotationsAtPrev(reg_rotations)-1, rotation, duplicate_rotations_sharedmem)) {
                    continue;
                }
            }
        }
        // only edge rotation
        uint32_t prev_edge_pos = state.edge_pos;
        uint16_t prev_edge_orient = state.edge_orient;
        uint8_t prev_edge_sym = state.edge_sym;
        edge::DRotate(state.edge_pos, state.edge_sym, state.edge_orient, rotation);
        // next corner heuristic
        uint8_t max_heuristic = corner_heuristic + ((full_corner_heuristic >> (8 + 2*rotation)) & 3) - 1;
        if (!rev && max_heuristic < 14) {
            // edge heuristic
            max_heuristic = max(max_heuristic, edge::DGetHeuristic(state.edge_pos, state.edge_orient));
            // not working
            if (max_heuristic + reg_rotations.idx + 1 + starting_depth >= cur_depth) {
                state.edge_pos = prev_edge_pos;
                state.edge_orient = prev_edge_orient;
                state.edge_sym = prev_edge_sym;
                num_position_thread++;
                continue;
            }
        }
        corner::DRotate(state.corner_pos, state.corner_orient, rotation);

        if (!rev) {
            RotationsSet(reg_rotations, rotation+1);
            reg_rotations.idx++;
            num_position_thread++;
        }

        // get helpful rotations
        full_corner_heuristic = corner::DGetHeuristic(state.corner_pos, state.corner_orient);
        corner_heuristic = full_corner_heuristic;
        rotations = (full_corner_heuristic >> 8) & (full_corner_heuristic >> 9) & kRotationsMask;
        if (2 + corner_heuristic + reg_rotations.idx + starting_depth >= cur_depth) {
            rotations |= (full_corner_heuristic >> 9) & kRotationsMask;
        }
        if (1 + corner_heuristic + reg_rotations.idx + starting_depth >= cur_depth) {
            rotations |= (full_corner_heuristic >> 8) & kRotationsMask;
        }
        rotations |= rotations << 1;
        rotation = RotationsAt(reg_rotations);
        rotations |= (1ULL<<(2*rotation))-1;

        if (rev) {
            continue;
        }

        if (max_heuristic <= tb_depth && tablebase::DContains(state)) {
            bool cur_found_solution = bool(atomicCAS(&d_device_solution->flag, int(false), int(true)));
            if (!cur_found_solution) { // first solution
                d_device_solution->reg_rotations = reg_rotations;
                d_device_solution->state = state;
            }
            break;
        }

        // not possible with the current tablebase
        if (tb_depth+1 + reg_rotations.idx + starting_depth >= cur_depth) {
            rotations |= (1ULL<<(2*kNumRot))-1;
            RotationsSet(reg_rotations, (uint8_t)(kNumRot));
        }
    }

    // accumulate positions
    d_num_position_threads[index] += num_position_thread;

    // rotations
    d_reg_rotations[index] = reg_rotations;

    // state
    d_states[index] = state;
}


__global__ void GetNewStates(std::pair<State, uint8_t>* d_position_queue, const int32_t* d_pos_queue_idx, const int32_t* d_pos_queue_num_elements, int32_t* atomic_offset_idx,
                             int32_t* d_free_splitmix_idx,
                             bool* d_possible_splitmix_idx,
                             uint8_t* d_starting_depths,
                             State* d_states,
                             RegRotations* d_reg_rotations) {
    uint32_t index = threadIdx.x + (blockIdx.x * blockDim.x);
    if (index >= num_gpu_threads) {
        return;
    }

    // still has stuff to do
    RegRotations reg_rotations = d_reg_rotations[index];
    if (d_states[index].edge_pos != (uint32_t)-1) {
        uint8_t rotation = RotationsAt(reg_rotations);
        if (reg_rotations.idx > reg_rotations.finish_idx ||
                (reg_rotations.idx == reg_rotations.finish_idx && rotation < reg_rotations.finish_rot)) {
            d_possible_splitmix_idx[index] = true;
            return;
        }
    }

    // index calculation
    d_possible_splitmix_idx[index] = false;
    int32_t current_offset_idx = atomicAdd(atomic_offset_idx, 1);

    // this thread has nothing to do anymore
    if (current_offset_idx >= *d_pos_queue_num_elements) {
        d_free_splitmix_idx[current_offset_idx - *d_pos_queue_num_elements] = index;
        return;
    }

    // add new position
    reg_rotations.d1 = 0;
    reg_rotations.d2 = 0;
    reg_rotations.idx = 0;
    reg_rotations.finish_idx = 0;
    reg_rotations.finish_rot = kNumRot;
    d_reg_rotations[index] = reg_rotations;
    int32_t pos_queue_idx = (*d_pos_queue_idx + current_offset_idx) % num_gpu_threads;
    State reg_state = d_position_queue[pos_queue_idx].first;
    d_states[index] = reg_state;
    d_starting_depths[index] = d_position_queue[pos_queue_idx].second;
}


__global__ void PostGetNewStates (int32_t* d_pos_queue_idx, int32_t* d_pos_queue_num_elements, int32_t* d_atomic_offset_idx, int32_t* d_num_reg_states, int32_t* d_atomic_splitmix) {
    uint32_t index = threadIdx.x + (blockIdx.x * blockDim.x);
    if (index == 0) {
        //                   already existing positions              newly added positions
        *d_num_reg_states = (num_gpu_threads - *d_atomic_offset_idx) + min(*d_atomic_offset_idx, *d_pos_queue_num_elements);
        *d_atomic_offset_idx = min(*d_atomic_offset_idx, *d_pos_queue_num_elements);
        *d_pos_queue_num_elements -= *d_atomic_offset_idx;
        *d_pos_queue_idx = (*d_pos_queue_idx + *d_atomic_offset_idx) % num_gpu_threads;
        *d_atomic_offset_idx = 0;
        *d_atomic_splitmix = 0;
    }
}


__global__ void SplitMixStates (RegRotations* d_reg_rotations, State* d_state, uint8_t* d_starting_depths, bool* d_possible_splitmix_idx, int32_t* d_free_splitmix_idx, int32_t* d_num_reg_states, int32_t* d_atomic_splitmix) {
    uint32_t index = threadIdx.x + (blockIdx.x * blockDim.x);
    if (index >= num_gpu_threads) {
        return;
    }
    if (num_gpu_threads - *d_num_reg_states == 0) {
        return;
    }
    if (!d_possible_splitmix_idx[index]) {
        return;
    }
    State state_start = d_state[index];
    if (state_start.edge_pos == (uint32_t)-1) {
        return;
    }

    RegRotations reg_rotations = d_reg_rotations[index];
    // don't split if the current position is at the moment during rotations
    if (reg_rotations.idx <= reg_rotations.finish_idx + 1) {
        return;
    }

    uint8_t starting_depth = d_starting_depths[index];

    // get to the position where you should split
    while (true) {
        RotationsSet(reg_rotations, 0);
        reg_rotations.idx--;
        uint8_t cur_rotation = RotationsAt(reg_rotations)-1;
        // stop earlier if going inside is already finished
        if (reg_rotations.idx <= reg_rotations.finish_idx && cur_rotation+1 >= reg_rotations.finish_rot) {
            d_reg_rotations[index].finish_idx++;
            d_reg_rotations[index].finish_rot = kNumRot;
            return;
        }
        // reverse rotation
        DRevRotation(cur_rotation);
        corner::DRotate(state_start.corner_pos, state_start.corner_orient, cur_rotation);
        edge::DRotate(state_start.edge_pos, state_start.edge_sym, state_start.edge_orient, cur_rotation);

        if (reg_rotations.idx <= reg_rotations.finish_idx) {
            break;
        }
    }

    // usefull rotations
    uint64_t full_corner_heuristic = corner::DGetHeuristic(state_start.corner_pos, state_start.corner_orient);
    uint8_t corner_heuristic = full_corner_heuristic;
    constexpr uint64_t kRotationsMask = 0x555555555ULL;
    uint64_t rotations = (full_corner_heuristic >> 8) & (full_corner_heuristic >> 9) & kRotationsMask;
    if (2 + corner_heuristic + reg_rotations.idx + starting_depth >= cur_depth) {
        rotations |= (full_corner_heuristic >> 9) & kRotationsMask;
    }
    if (1 + corner_heuristic + reg_rotations.idx + starting_depth >= cur_depth) {
        rotations |= (full_corner_heuristic >> 8) & kRotationsMask;
    }
    rotations |= rotations << 1;
    uint8_t rotation = RotationsAt(reg_rotations);
    rotations |= (1ULL<<(2*rotation))-1;

    // atomic check
    uint32_t num_rotations_left = kNumRot - (cuda::std::popcount(rotations) / 2);
    uint32_t atomic_splitmix = atomicAdd(d_atomic_splitmix, num_rotations_left);
    if (*d_num_reg_states + atomic_splitmix + num_rotations_left > num_gpu_threads) {
        return;
    }

    // increase for standard value
    d_reg_rotations[index].finish_idx++;
    d_reg_rotations[index].finish_rot = kNumRot;

    // do the splitmix
    while (true) {
        rotation = cuda::std::countr_one(rotations) / 2;
        if (rotation >= kNumRot) {
            break;
        }
        int32_t splitmix_idx = d_free_splitmix_idx[atomic_splitmix++];
        RotationsSet(reg_rotations, rotation);
        reg_rotations.finish_rot = rotation+1;
        d_reg_rotations[splitmix_idx] = reg_rotations;
        d_state[splitmix_idx] = state_start;
        d_starting_depths[splitmix_idx] = d_starting_depths[index];
        rotations |= 3ULL << (2*rotation);
    }
}


// device memory only
__global__ void MemInitialization(RegRotations* d_reg_rotations, State* d_state, uint8_t* d_starting_depths,
        DeviceSolution* d_device_solution, uint64_t* d_num_position_threads, int32_t* d_atomic_offset_idx,
        int32_t* d_num_reg_states, int32_t* d_free_splitmix_idx,
        bool* d_possible_splitmix_idx, int32_t* d_atomic_splitmix,
        int32_t* d_pos_queue_idx, int32_t* d_pos_queue_num_elements,
        std::pair<State, uint8_t>* d_position_queue) {
    // get current leaf thread idx
    uint32_t index = threadIdx.x + (blockIdx.x * blockDim.x);
    if (index >= num_gpu_threads) {
        return;
    }

    if (index == 0) {
        *d_device_solution = kDefaultDeviceSolution;
        *d_atomic_offset_idx = 0;
        *d_num_reg_states = 0;
        *d_atomic_splitmix = 0;
        *d_pos_queue_idx = 0;
        *d_pos_queue_num_elements = 0;
    }

    d_reg_rotations[index] = kDefaultRegRotations;
    d_state[index] = kNonLegalState;
    d_starting_depths[index] = -1;
    d_num_position_threads[index] = 0;
    d_free_splitmix_idx[index] = 0;
    d_possible_splitmix_idx[index] = false;
    d_position_queue[index].first = kNonLegalState;
    d_position_queue[index].second = -1;
}


void CudaConstMemInitialize () {
    for (int i = 0; i < Settings::GetDeviceCount(); i++) {
        // upload it to the correct device
        cudaSetDevice(i);
        MemcpyToSymbol(uint32_t(Settings::GetNumGPUThreads()), num_gpu_threads);
        MemcpyToSymbol(Settings::GetTBDepth(), tb_depth);
        MemcpyToSymbol(DuplicateRotations::GetData(), duplicate_rotations_constmem);
    }
}


void CudaConstMemChangeCurDepth (uint8_t depth) {
    for (int i = 0; i < Settings::GetDeviceCount(); i++) {
        // upload it to the correct device
        cudaSetDevice(i);
        MemcpyToSymbol(depth, cur_depth);
    }
    cudaDeviceSynchronize();
}


void UploadBatchesToDevice (SharedLeafStates& shared_leaf_states,
        LocalBuffer& local_buffer, size_t& local_buffer_idx,
        std::vector<std::pair<State, uint8_t>>& position_queue,
        int32_t& pos_queue_idx, int32_t& pos_queue_num_elements,
        int32_t*& d_pos_queue_idx, int32_t*& d_pos_queue_num_elements,
        std::pair<State, uint8_t>* d_position_queue, const cudaStream_t& cuda_stream) {

    int new_num_elements = 0;
    while (pos_queue_num_elements + new_num_elements < Settings::GetNumGPUThreads()) {
        // get from shared
        if (local_buffer_idx >= local_buffer->size()) {
            std::lock_guard<std::mutex> lock(shared_leaf_states.mtx);
            if (shared_leaf_states.shared_ptrs.empty()) {
                break;
            }
            local_buffer = std::move(shared_leaf_states.shared_ptrs.front());
            local_buffer_idx = 0;
            shared_leaf_states.shared_ptrs.pop();
            shared_leaf_states.cv.notify_one();
        }

        // get num elements to copy
        int num_elements_local_buffer = local_buffer->size() - local_buffer_idx;
        int copy_num_elements = std::min(num_elements_local_buffer, Settings::GetNumGPUThreads() - pos_queue_num_elements - new_num_elements);
        if (((pos_queue_idx + pos_queue_num_elements + new_num_elements) % Settings::GetNumGPUThreads()) + copy_num_elements >= Settings::GetNumGPUThreads()) {
            copy_num_elements = Settings::GetNumGPUThreads() - ((pos_queue_idx + pos_queue_num_elements + new_num_elements) % Settings::GetNumGPUThreads());
        }

        // copy these elements to the host pos_queue
        assert(local_buffer_idx + copy_num_elements <= local_buffer->size());
        assert(pos_queue_num_elements + new_num_elements + copy_num_elements <= Settings::GetNumGPUThreads());
        assert(((pos_queue_idx + pos_queue_num_elements + new_num_elements) % Settings::GetNumGPUThreads()) + copy_num_elements <= Settings::GetNumGPUThreads());
        std::copy(local_buffer->begin()+local_buffer_idx,
                  local_buffer->begin()+local_buffer_idx+copy_num_elements,
                  position_queue.begin()+((pos_queue_idx + pos_queue_num_elements + new_num_elements) % Settings::GetNumGPUThreads()));

        new_num_elements += copy_num_elements;
        local_buffer_idx += copy_num_elements;
    }

    // copy to device wrap around
    if (((pos_queue_idx + pos_queue_num_elements) % Settings::GetNumGPUThreads()) + new_num_elements > Settings::GetNumGPUThreads()) {
        int copy_num_elements = Settings::GetNumGPUThreads() - ((pos_queue_idx + pos_queue_num_elements) % Settings::GetNumGPUThreads());
        cudaError_t err = cudaMemcpyAsync(d_position_queue + ((pos_queue_idx + pos_queue_num_elements) % Settings::GetNumGPUThreads()),
                        position_queue.data() + ((pos_queue_idx + pos_queue_num_elements) % Settings::GetNumGPUThreads()),
                        sizeof(std::pair<State, uint8_t>)*copy_num_elements, cudaMemcpyHostToDevice, cuda_stream);
        pos_queue_num_elements += copy_num_elements;
        new_num_elements -= copy_num_elements;
        if (err != cudaSuccess) {
            LOG_CRITICAL(cudaGetErrorString(err));
        }
    }

    // copy to device wrap around
    int copy_num_elements = new_num_elements;
    cudaError_t err = cudaMemcpyAsync(d_position_queue + ((pos_queue_idx + pos_queue_num_elements) % Settings::GetNumGPUThreads()),
                    position_queue.data() + ((pos_queue_idx + pos_queue_num_elements) % Settings::GetNumGPUThreads()),
                    sizeof(std::pair<State, uint8_t>)*copy_num_elements, cudaMemcpyHostToDevice, cuda_stream);
    pos_queue_num_elements += copy_num_elements;
    new_num_elements -= copy_num_elements;
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }

    MemcpyToDeviceStream(pos_queue_idx, d_pos_queue_idx, cuda_stream);
    MemcpyToDeviceStream(pos_queue_num_elements, d_pos_queue_num_elements, cuda_stream);
}


void DeviceLeafManager (int gpu_idx, SharedSearch& shared_search, SharedLeafStates& shared_leaf_states, SharedLeafSolution& shared_leaf_solution,
        cudaStream_t& cuda_stream,
        size_t& grid_dim,
        RegRotations* d_reg_rotations,
        State* d_states,
        uint8_t* d_starting_depths,
        int32_t* d_atomic_offset_idx,
        int32_t* d_free_splitmix_idx,
        bool* d_possible_splitmix_idx,
        int32_t* d_atomic_splitmix,
        uint64_t* d_num_position_threads,
        DeviceSolution& device_solution,
        DeviceSolution* d_device_solution,
        int32_t& num_reg_states,
        int32_t* d_num_reg_states,
        int32_t& pos_queue_idx,
        int32_t* d_pos_queue_idx,
        int32_t& pos_queue_num_elements,
        int32_t* d_pos_queue_num_elements,
        std::vector<std::pair<State, uint8_t>>& position_queue,
        std::pair<State, uint8_t>* d_position_queue,
        uint64_t* d_total_num_position_threads,
        void* d_temp,
        size_t& num_temp_bytes) {
    while (true) {
        shared_search.start_work->arrive_and_wait();
        // stopping of the program
        if (shared_search.finished.load()) {
            break;
        }

        // get data from main thread
        uint8_t solution_depth = shared_leaf_states.depth;
        MemcpyToSymbolStream(solution_depth, cur_depth, cuda_stream);
        int scramble_idx = shared_leaf_states.scramble_idx;
        if (gpu_idx == 0) {
            nvtxMark(("Cube " + std::to_string(scramble_idx) + " depth " + std::to_string(solution_depth)).c_str());
        }

        // resetting everything
        MemInitialization<<<grid_dim, kBlockDim, 0, cuda_stream>>>(d_reg_rotations, d_states, d_starting_depths, d_device_solution, d_num_position_threads, d_atomic_offset_idx, d_num_reg_states, d_free_splitmix_idx, d_possible_splitmix_idx, d_atomic_splitmix, d_pos_queue_idx, d_pos_queue_num_elements, d_position_queue);
        num_reg_states = 0;
        pos_queue_idx = 0;
        pos_queue_num_elements = 0;
        device_solution = DeviceSolution();
        std::fill(position_queue.begin(), position_queue.end(), std::make_pair(kNonLegalState, uint8_t(-1)));

        // local buffer
        LocalBuffer local_buffer = std::make_shared<std::vector<std::pair<State, uint8_t>>>();;
        size_t local_buffer_idx = 0;

        while (local_buffer_idx != local_buffer->size() || pos_queue_num_elements != 0 || !shared_leaf_states.finished_depth.load() || num_reg_states != 0) {
            GetNewStates<<<grid_dim, kBlockDim, 0, cuda_stream>>>(d_position_queue, d_pos_queue_idx, d_pos_queue_num_elements, d_atomic_offset_idx, d_free_splitmix_idx, d_possible_splitmix_idx, d_starting_depths, d_states, d_reg_rotations);
            PostGetNewStates<<<1, 1, 0, cuda_stream>>>(d_pos_queue_idx, d_pos_queue_num_elements, d_atomic_offset_idx, d_num_reg_states, d_atomic_splitmix);
            SplitMixStates<<<grid_dim, kBlockDim, 0, cuda_stream>>>(d_reg_rotations, d_states, d_starting_depths, d_possible_splitmix_idx, d_free_splitmix_idx, d_num_reg_states, d_atomic_splitmix);
            MemcpyFromDeviceStream(pos_queue_idx, d_pos_queue_idx, cuda_stream);
            MemcpyFromDeviceStream(pos_queue_num_elements, d_pos_queue_num_elements, cuda_stream);
            MemcpyFromDeviceStream(device_solution, d_device_solution, cuda_stream);
            MemcpyFromDeviceStream(num_reg_states, d_num_reg_states, cuda_stream);
            cudaStreamSynchronize(cuda_stream);
            DeviceLeafSearch<<<grid_dim, kBlockDim, 0, cuda_stream>>>(d_starting_depths, d_states, d_reg_rotations, d_device_solution, d_num_position_threads);
            UploadBatchesToDevice(shared_leaf_states, local_buffer, local_buffer_idx, position_queue, pos_queue_idx, pos_queue_num_elements, d_pos_queue_idx, d_pos_queue_num_elements, d_position_queue, cuda_stream);
            if (shared_leaf_solution.finished.load(std::memory_order_acquire)) {
                break;
            }
            if (bool(device_solution.flag)) {
                break;
            }
        }
        MemcpyFromDeviceStream(device_solution, d_device_solution, cuda_stream);
        cudaStreamSynchronize(cuda_stream);
        if (bool(device_solution.flag)) {
            bool expected = false;
            if (shared_leaf_solution.finished.compare_exchange_strong(expected, true, std::memory_order_acq_rel, std::memory_order_acquire)) {
                State state = device_solution.state;
                shared_leaf_solution.state = state;
                RegRotations reg_rotations = device_solution.reg_rotations;
                while (true) {
                    transposition_table::Insert<true>(state, 2*(solution_depth-Settings::GetTBDepth()-1));
                    reg_rotations.idx--;
                    solution_depth--;
                    if (reg_rotations.idx == -1) {
                        break;
                    }
                    uint8_t rotation = GetRevRotation(RotationsAt(reg_rotations)-1);
                    corner::Rotate(state.corner_pos, state.corner_orient, rotation);
                    edge::Rotate(state.edge_pos, state.edge_sym, state.edge_orient, rotation);
                }
                shared_leaf_states.cv.notify_all();
            };
        }

        // accumulate num_positions
        uint64_t total_num_position_threads = 0;
        cub::DeviceReduce::Sum(d_temp, num_temp_bytes, d_num_position_threads, d_total_num_position_threads, Settings::GetNumGPUThreads(), cuda_stream);
        MemcpyFromDeviceStream(total_num_position_threads, d_total_num_position_threads, cuda_stream);
        cudaStreamSynchronize(cuda_stream);
        shared_search.leaf_cnt.fetch_add(total_num_position_threads);

        // finished
        shared_search.done_work->arrive_and_wait();
    }
}


void DeviceLeafManagerInit (int gpu_idx, SharedSearch& shared_search,
        SharedLeafStates& shared_leaf_states, SharedLeafSolution& shared_leaf_solution) {
    // set the device for this thread
    cudaError_t err = cudaSetDevice(gpu_idx);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }

    size_t grid_dim = ((Settings::GetNumGPUThreads()-1)/kBlockDim)+1;

    // create stream
    cudaStream_t cuda_stream;
    cudaStreamCreate(&cuda_stream);

    // device memory only
    RegRotations* d_reg_rotations;
    MallocOnDeviceStream(d_reg_rotations, Settings::GetNumGPUThreads(), cuda_stream);
    State* d_states;
    MallocOnDeviceStream(d_states, Settings::GetNumGPUThreads(), cuda_stream);
    uint8_t* d_starting_depths;
    MallocOnDeviceStream(d_starting_depths, Settings::GetNumGPUThreads(), cuda_stream);
    int32_t* d_atomic_offset_idx;
    MallocOnDeviceStream(d_atomic_offset_idx, 1, cuda_stream);
    int32_t* d_free_splitmix_idx;
    MallocOnDeviceStream(d_free_splitmix_idx, Settings::GetNumGPUThreads(), cuda_stream);
    bool* d_possible_splitmix_idx;
    MallocOnDeviceStream(d_possible_splitmix_idx, Settings::GetNumGPUThreads(), cuda_stream);
    int32_t* d_atomic_splitmix;
    MallocOnDeviceStream(d_atomic_splitmix, 1, cuda_stream);

    // copied at the end of the leaf manager
    uint64_t* d_num_position_threads;
    MallocOnDeviceStream(d_num_position_threads, Settings::GetNumGPUThreads(), cuda_stream);

    // copied after every kernal
    DeviceSolution device_solution;
    DeviceSolution* d_device_solution;
    MallocOnDeviceStream(d_device_solution, 1, cuda_stream);
    HostRegister(device_solution);
    int32_t num_reg_states;
    int32_t* d_num_reg_states;
    MallocOnDeviceStream(d_num_reg_states, 1, cuda_stream);
    HostRegister(num_reg_states);

    // circular queue with Settings::GetNumGPUThreads elements
    int32_t pos_queue_idx;
    int32_t* d_pos_queue_idx;
    int32_t pos_queue_num_elements;
    int32_t* d_pos_queue_num_elements;
    std::vector<std::pair<State, uint8_t>> position_queue(Settings::GetNumGPUThreads());
    std::pair<State, uint8_t>* d_position_queue;
    MallocOnDeviceStream(d_position_queue, Settings::GetNumGPUThreads(), cuda_stream);
    HostRegister(position_queue);
    HostRegister(pos_queue_idx);
    HostRegister(pos_queue_num_elements);
    MallocOnDeviceStream(d_pos_queue_idx, 1, cuda_stream);
    MallocOnDeviceStream(d_pos_queue_num_elements, 1, cuda_stream);

    uint64_t* d_total_num_position_threads;
    MallocOnDeviceStream(d_total_num_position_threads, 1, cuda_stream);
    void* d_temp = nullptr;
    size_t num_temp_bytes = 0;
    cub::DeviceReduce::Sum(d_temp, num_temp_bytes, d_num_position_threads, d_total_num_position_threads, Settings::GetNumGPUThreads(), cuda_stream);
    err = cudaMallocAsync(&d_temp, num_temp_bytes, cuda_stream);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }

    DeviceLeafManager(gpu_idx, shared_search, shared_leaf_states, shared_leaf_solution, cuda_stream, grid_dim, d_reg_rotations, d_states,
            d_starting_depths, d_atomic_offset_idx, d_free_splitmix_idx, d_possible_splitmix_idx, d_atomic_splitmix, d_num_position_threads,
            device_solution, d_device_solution, num_reg_states, d_num_reg_states, pos_queue_idx, d_pos_queue_idx, pos_queue_num_elements,
            d_pos_queue_num_elements, position_queue, d_position_queue, d_total_num_position_threads, d_temp, num_temp_bytes);

    FreeCudaPointerStream(d_temp, cuda_stream);

    cudaHostUnregister(&device_solution);
    cudaHostUnregister(&num_reg_states);
    cudaHostUnregister(position_queue.data());
    cudaHostUnregister(&pos_queue_idx);
    cudaHostUnregister(&pos_queue_num_elements);
    FreeCudaPointerStream(d_reg_rotations, cuda_stream);
    FreeCudaPointerStream(d_states, cuda_stream);
    FreeCudaPointerStream(d_starting_depths, cuda_stream);
    FreeCudaPointerStream(d_atomic_offset_idx, cuda_stream);
    FreeCudaPointerStream(d_free_splitmix_idx, cuda_stream);
    FreeCudaPointerStream(d_possible_splitmix_idx, cuda_stream);
    FreeCudaPointerStream(d_atomic_splitmix, cuda_stream);
    FreeCudaPointerStream(d_num_position_threads, cuda_stream);
    FreeCudaPointerStream(d_device_solution, cuda_stream);
    FreeCudaPointerStream(d_num_reg_states, cuda_stream);
    FreeCudaPointerStream(d_position_queue, cuda_stream);
    FreeCudaPointerStream(d_pos_queue_idx, cuda_stream);
    FreeCudaPointerStream(d_pos_queue_num_elements, cuda_stream);
    FreeCudaPointerStream(d_total_num_position_threads, cuda_stream);
    cudaStreamSynchronize(cuda_stream);
    cudaStreamDestroy(cuda_stream);
}
