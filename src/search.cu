#include <cuda.h>
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


// TODO: change this to compution only (remove for loop)
__device__ inline uint32_t GetNumRotationsLeft(const State& state, const uint8_t& rotation) {
    uint64_t corner_heuristic = corner::DGetHeuristic(state.corner_pos, state.corner_orient);
    uint32_t cnt = 0;
    for (uint8_t rot = (rotation ^ uint8_t(1 << 7)) + 1; rot < kNumRot; rot++) {
        if (((corner_heuristic >> (8+2*rot)) & 3) != 3) {
            cnt++;
        }
    }
    return cnt;
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

    // general information
    uint16_t num_position_thread = 0;

    uint8_t starting_depth = d_starting_depths[index];

    // rotations
    RegRotations reg_rotations = d_reg_rotations[index];

    // state
    State state = d_states[index];

    // make a constant number of position during each kernal function call
    // this could be way to high
    constexpr int kNumPosBatchSize = 1000;
    for (int cur_pos_batch = 0; cur_pos_batch < kNumPosBatchSize; cur_pos_batch++) {
        // the goal is to search further in the dfs (from the leaf position) and stop if an improvement to the best_depth is not posible any more
        // for each loop cycle it will look at a new position or undo the move it has done during the dfs.
        // the search is structured in a way that the current moment of the search can be saved to global memory and the kernal stops.
        // at the next kernal start the search will continue from the previous search
        // this cube is now during a search phase with the starting position of leaf_thread_idx
        // the state is the current position of the search after all rotations from the leaf starting position
        if (reg_rotations.idx < 0 || reg_rotations.idx < reg_rotations.finish_idx) {
            break;
        }
        uint8_t rotation_at = RotationsAt(reg_rotations);
        uint8_t rotation = rotation_at & (uint8_t(-1)>>1);
        if (reg_rotations.idx == reg_rotations.finish_idx && rotation >= reg_rotations.finish_rot) {
            break;
        }

        bool passive_add_one = false;

        // undo the rotation to continue the search on the next subtree
        bool rev = (rotation_at ^ rotation) != 0;
        if (rev) {
            DRevRotation(rotation);
        }
        else {
            uint8_t prev_rotation = RotationsAtPrev(reg_rotations) & (uint8_t(-1)>>1);
            if (rotation < kNumRot && prev_rotation < kNumRot && IsDuplicateRotation(prev_rotation, rotation, duplicate_rotations_sharedmem)) {
                passive_add_one = true;
            }
        }

        // do the rotation
        // if it is an illegal search skip this rotation
        if (!passive_add_one) {
            uint64_t corner_heuristic = corner::DGetHeuristic(state.corner_pos, state.corner_orient);
            if (((corner_heuristic >> (8+2*rotation)) & 3) != 3 // check legality
                    && (rev || uint8_t(corner_heuristic) + ((corner_heuristic >> (8+2*rotation)) & 3) + reg_rotations.idx + starting_depth < cur_depth)) { // get important rotation bit
                corner::DRotate(state.corner_pos, state.corner_orient, rotation);
                edge::DRotate(state.edge_pos, state.edge_sym, state.edge_orient, rotation);
            }
            else {
                passive_add_one = true;
            }
        }
        // prepare the next rotation
        if (!passive_add_one) {
            RotationsXOR(reg_rotations, 1<<7);  // NOLINT
        }

        // undo rotation done increase to next rotation
        if (!passive_add_one && rev) {
            passive_add_one = true;
        }

        if (passive_add_one) {
            RotationsAdd(reg_rotations, 1);
            if (RotationsAt(reg_rotations) == kNumRot) {
                RotationsSet(reg_rotations, 0);
                reg_rotations.idx--;
            }
            continue;
        }

        // go inside the next position
        reg_rotations.idx++;
        num_position_thread++;

        // not able to improve the current leaf search skip this node
        uint8_t max_heuristic = max(uint8_t(corner::DGetHeuristic(state.corner_pos, state.corner_orient)),
                                    edge::DGetHeuristic(state.edge_pos, state.edge_orient));


        if (max_heuristic <= tb_depth && tablebase::DContains(state)) {
            bool cur_found_solution = bool(atomicCAS(&d_device_solution->flag, int(false), int(true)));
            if (!cur_found_solution) { // first solution
                d_device_solution->reg_rotations = reg_rotations;
                d_device_solution->state = state;
            }
            break;
        }

        // not possible with the current heuristic
        if (max(tb_depth+1, max_heuristic) + reg_rotations.idx + starting_depth >= cur_depth) {
            reg_rotations.idx--;
            continue;
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
    if (reg_rotations.idx >= 0) {
        uint8_t rotation = RotationsAt(reg_rotations) & (uint8_t(-1)>>1);
        if (reg_rotations.idx >= reg_rotations.finish_idx && (reg_rotations.idx != reg_rotations.finish_idx || rotation < reg_rotations.finish_rot)) {
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
    reg_rotations.finish_idx = -1;
    reg_rotations.finish_rot = 1;
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

    RegRotations reg_rotations = d_reg_rotations[index];
    // don't split if the current position is at the moment during rotations
    if (reg_rotations.idx == reg_rotations.finish_idx || reg_rotations.idx == reg_rotations.finish_idx+1) {
        return;
    }

    State state_start = d_state[index];
    uint8_t rotation_start = -1;

    // get to the position where you should split
    while (true) {
        uint8_t cur_rotation = RotationsAt(reg_rotations);
        // reverse rotation
        if (cur_rotation > kNumRot) {
            cur_rotation ^= uint8_t(1<<7);
            DRevRotation(cur_rotation);
            corner::DRotate(state_start.corner_pos, state_start.corner_orient, cur_rotation);
            edge::DRotate(state_start.edge_pos, state_start.edge_sym, state_start.edge_orient, cur_rotation);
        }
        if (reg_rotations.idx > reg_rotations.finish_idx+1) {
            RotationsSet(reg_rotations, 0);
            reg_rotations.idx--;
        }
        else {
            break;
        }
    }
    rotation_start = RotationsAt(reg_rotations);

    // atomic check
    uint32_t num_rotations_left = GetNumRotationsLeft(state_start, rotation_start);
    uint32_t atomic_splitmix = atomicAdd(d_atomic_splitmix, num_rotations_left);
    if (*d_num_reg_states + atomic_splitmix + num_rotations_left > num_gpu_threads) {
        return;
    }

    // increase for standard value
    d_reg_rotations[index].finish_idx++;
    d_reg_rotations[index].finish_rot = (rotation_start ^ uint8_t(1 << 7)) + 1;
    reg_rotations.finish_idx++;

    // do the splitmix
    uint64_t corner_heuristic = corner::DGetHeuristic(state_start.corner_pos, state_start.corner_orient);
    for (uint8_t rot = (rotation_start ^ uint8_t(1 << 7)) + 1; rot < kNumRot; rot++) {
        if (((corner_heuristic >> (8+2*rot)) & 3) != 3) { // get important rotation bit
            int32_t splitmix_idx = d_free_splitmix_idx[atomic_splitmix++];
            RotationsSet(reg_rotations, rot);
            reg_rotations.finish_rot = rot+1;
            d_reg_rotations[splitmix_idx] = reg_rotations;
            d_state[splitmix_idx] = state_start;
            d_starting_depths[splitmix_idx] = d_starting_depths[index];
        }
    }
}


// device memory only
__global__ void MemInitialization(RegRotations* d_reg_rotations, State* d_state, uint8_t* d_starting_depths, DeviceSolution* d_device_solution, uint64_t* d_num_position_threads, int32_t* d_atomic_offset_idx, int32_t* d_num_reg_states, int32_t* d_free_splitmix_idx,
                             bool* d_possible_splitmix_idx, int32_t* d_atomic_splitmix) {
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
    }

    d_reg_rotations[index] = kDefaultRegRotations;
    d_state[index] = kNonLegalState;
    d_starting_depths[index] = -1;
    d_num_position_threads[index] = 0;
    d_free_splitmix_idx[index] = 0;
    d_possible_splitmix_idx[index] = false;
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


void DeviceLeafManager (std::atomic<bool>& stoken, SharedLeafStates& shared_leaf_states,
                        SharedLeafSolution& shared_leaf_solution, uint64_t& num_gpu_positions, const int& thread_idx, uint8_t current_depth, int current_scramble) {
    // set the device for this thread
    int gpu_device_idx = thread_idx % Settings::GetDeviceCount();
    cudaError_t err = cudaSetDevice(gpu_device_idx);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }

    if (thread_idx == 0) {
        nvtxMark(("Cube " + std::to_string(current_scramble) + " depth " + std::to_string(current_depth)).c_str());
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
    int32_t num_reg_states = 0;
    int32_t* d_num_reg_states;
    MallocOnDeviceStream(d_num_reg_states, 1, cuda_stream);
    HostRegister(num_reg_states);

    MemInitialization<<<grid_dim, kBlockDim, 0, cuda_stream>>>(d_reg_rotations, d_states, d_starting_depths, d_device_solution, d_num_position_threads, d_atomic_offset_idx, d_num_reg_states, d_free_splitmix_idx, d_possible_splitmix_idx, d_atomic_splitmix);

    // circular queue with Settings::GetNumGPUThreads elements
    int32_t pos_queue_idx = 0;
    int32_t* d_pos_queue_idx;
    int32_t pos_queue_num_elements = 0;
    int32_t* d_pos_queue_num_elements;
    std::vector<std::pair<State, uint8_t>> position_queue(Settings::GetNumGPUThreads(), {State(), -1});
    std::pair<State, uint8_t>* d_position_queue;
    MallocOnDeviceStream(d_position_queue, Settings::GetNumGPUThreads(), cuda_stream);
    HostRegister(position_queue);
    HostRegister(pos_queue_idx);
    HostRegister(pos_queue_num_elements);
    MallocOnDeviceStream(d_pos_queue_idx, 1, cuda_stream);
    MemcpyToDeviceStream(pos_queue_idx, d_pos_queue_idx, cuda_stream);
    MallocOnDeviceStream(d_pos_queue_num_elements, 1, cuda_stream);
    MemcpyToDeviceStream(pos_queue_num_elements, d_pos_queue_num_elements, cuda_stream);


    // local buffer
    LocalBuffer local_buffer = std::make_shared<std::vector<std::pair<State, uint8_t>>>();;
    size_t local_buffer_idx = 0;

    while (local_buffer_idx != local_buffer->size() || pos_queue_num_elements != 0 || !stoken || num_reg_states != 0) {
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
                transposition_table::Insert<true>(state, current_depth-Settings::GetTBDepth()-1);
                reg_rotations.idx--;
                current_depth--;
                if (reg_rotations.idx == -1) {
                    break;
                }
                uint8_t rotation = GetRevRotation(RotationsAt(reg_rotations) & (uint8_t(-1)>>1));
                corner::Rotate(state.corner_pos, state.corner_orient, rotation);
                edge::Rotate(state.edge_pos, state.edge_sym, state.edge_orient, rotation);
            }
            shared_leaf_states.cv.notify_all();
        };
    }

    // accumulate num_positions
    uint64_t total_num_position_threads = 0;
    uint64_t* d_total_num_position_threads;
    MallocOnDeviceStream(d_total_num_position_threads, 1, cuda_stream);
    void* d_temp = nullptr;
    size_t num_temp_bytes = 0;
    cub::DeviceReduce::Sum(d_temp, num_temp_bytes, d_num_position_threads, d_total_num_position_threads, Settings::GetNumGPUThreads(), cuda_stream);
    err = cudaMallocAsync(&d_temp, num_temp_bytes, cuda_stream);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }
    cub::DeviceReduce::Sum(d_temp, num_temp_bytes, d_num_position_threads, d_total_num_position_threads, Settings::GetNumGPUThreads(), cuda_stream);
    MemcpyFromDeviceStream(total_num_position_threads, d_total_num_position_threads, cuda_stream);
    FreeCudaPointerStream(d_temp, cuda_stream);
    cudaStreamSynchronize(cuda_stream);
    num_gpu_positions += total_num_position_threads;

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
