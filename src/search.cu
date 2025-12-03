#include <cstdint>
#include <queue>
#include <stop_token>
#include <vector>

#include "cube.cuh"
#include "cube.hpp"
#include "logger.hpp"
#include "utils.hpp"
#include "cuda_memory_transfer.cuh"
#include "search.hpp"
#include "search_rotations.cuh"
#include "settings.hpp"


constexpr uint8_t kLeafThreadSize = 2;


__global__ void DeviceLeafSearch (uint64_t* d_num_positions_leafs, uint8_t* d_leaf_thread_idxs, std::pair<DState, uint8_t>* d_starting_positions,
                                  uint8_t* d_rotation_idxs, uint8_t* d_best_depths, URotations* d_urotations, uint8_t tb_depth) {
    // get current leaf thread idx
    size_t index = threadIdx.x + (size_t(blockIdx.x) * blockDim.x);
    uint8_t leaf_thread_idx = (kLeafThreadSize * index) + d_leaf_thread_idxs[index];

    // load from global memory
    uint8_t rotation_idx = d_rotation_idxs[leaf_thread_idx];
    DState state = d_starting_positions[leaf_thread_idx].first;
    uint8_t depth_offset = d_starting_positions[leaf_thread_idx].second;
    uint8_t best_depth = d_best_depths[leaf_thread_idx];
    URotations rotations = d_urotations[leaf_thread_idx];
    uint64_t num_positions = d_num_positions_leafs[leaf_thread_idx];

    // do the rotations such that state is again at the outcome state it was previously (somewhere in the tree)
    for (int i = 0; i < rotation_idx && rotation_idx != uint8_t(-1); i++) {
        state = DCube::Rotate(state, rotations[i]).state;
    }

    // make a constant number of position during each kernal function call
    constexpr int kNumPosBatchSize = 10000;
    for (int cur_pos_batch = 0; cur_pos_batch < kNumPosBatchSize; cur_pos_batch++) {
        if (rotation_idx == uint8_t(-1)) {
            // newly solved position
            if (d_rotation_idxs[leaf_thread_idx] != uint8_t(-1)) {
                // save back to global memory;
                d_rotation_idxs[leaf_thread_idx] = uint8_t(-1);
                d_best_depths[leaf_thread_idx] = best_depth;
                d_urotations[leaf_thread_idx] = rotations; // not really necessary
                d_num_positions_leafs[leaf_thread_idx] = num_positions;
            }

            // get new leaf_thread_idx
            d_leaf_thread_idxs[index] = (d_leaf_thread_idxs[index] + 1) % kLeafThreadSize;
            leaf_thread_idx = (kLeafThreadSize * index) + d_leaf_thread_idxs[index];

            // load from global memory
            rotation_idx = d_rotation_idxs[leaf_thread_idx];
            state = d_starting_positions[leaf_thread_idx].first;
            depth_offset = d_starting_positions[leaf_thread_idx].second;
            best_depth = d_best_depths[leaf_thread_idx];
            rotations = d_urotations[leaf_thread_idx];
            num_positions = d_num_positions_leafs[leaf_thread_idx];

            // do the rotations such that state is again at the outcome state it was previously (somewhere in the tree)
            for (int i = 0; i < rotation_idx && rotation_idx != uint8_t(-1); i++) {
                state = DCube::Rotate(state, rotations[i]).state;
            }
            continue;
        }

        // the goal is to search further in the dfs (from the leaf position) and stop if an improvement to the best_depth is not posible any more
        // for each loop cycle it will look at a new position or undo the move it has done during the dfs.
        // the search is structured in a way that the current moment of the search can be saved to global memory and the kernal stops.
        // at the next kernal start the search will continue from the previous search
        // this cube is now during a search phase with the starting position of leaf_thread_idx
        // the state is the current position of the search after all rotations from the leaf starting position

        uint8_t rotation = rotations[rotation_idx] & (uint8_t(-1)>>1);
        // newly visited position
        if (rotation == 0) {
            num_positions++;
        }

        // finished with the rotations of the current position
        if (rotation == kNumRotations) {  // NOLINT
            rotations[rotation_idx] = 0;
            rotation_idx--;
            continue;
        }

        // undo the rotation to continue the search on the next subtree
        bool rev = (rotations[rotation_idx] ^ rotation) != 0;
        if (rev) {
            rotation = DGetRevRotation(rotation);
        }

        // do the rotation
        DRotateReturn next_pos = DCube::Rotate(state, rotation);
        // it is garantied that the undo rotation of a cube is always possible in this leaf search
        // if it is an illegal search skip this rotation
        if (!next_pos.isLegal) {
            rotations[rotation_idx]++;
            continue;
        }
        state = next_pos.state;
        // prepare the next rotation
        rotations[rotation_idx] ^= uint8_t(1<<7);  // NOLINT

        // undo rotation done increase to next rotation
        if (rev) {
            rotations[rotation_idx]++;
            continue;
        }

        // go inside the next position
        rotation_idx++;

        // not able to improve the current leaf search skip this node
        DCube cube;
        if (max(tb_depth, cube.GetMaxHeuristic(state)) + rotation_idx + depth_offset >= best_depth) {
            rotations[rotation_idx] = kNumRotations;
            continue;
        }

        // check if the current state is in tablebase and is therefore a new best solution
        if (DCube::DTablebaseContains(state)) {
            uint8_t depth = rotation_idx + tb_depth + depth_offset;
            best_depth = min(depth, best_depth);
            printf("NEW best: %d position_idx %d\n", int(best_depth), int(leaf_thread_idx));
        }
    }

    // save back to global memory;
    d_rotation_idxs[leaf_thread_idx] = uint8_t(-1);
    d_best_depths[leaf_thread_idx] = best_depth;
    d_urotations[leaf_thread_idx] = rotations; // not really necessary
    d_num_positions_leafs[leaf_thread_idx] = num_positions;

    d_leaf_thread_idxs[index] = leaf_thread_idx;
}


void DeviceLeafManager (std::stop_token& stocken, std::atomic<std::shared_ptr<phmap::flat_hash_map<State, uint8_t>>>& shared_leaf_states,
                        std::mutex& mtx, uint64_t& num_positions_leaf, VisitedMap& visited_leaf,
                        std::atomic<uint8_t>& atomic_best_depth, std::pair<State, uint8_t>& best_endstate_leafs,
                        [[maybe_unused]] const uint64_t& leaf_batch_size, const int& thread_idx) {
    // Device informations will only be on the device
    std::vector<uint8_t> leaf_thread_idxs(leaf_batch_size, 0);
    uint8_t* d_leaf_thread_idxs;
    UploadToDevice(leaf_thread_idxs, d_leaf_thread_idxs);

    // updated for each position
    std::vector<uint8_t> rotation_idxs(leaf_batch_size*kLeafThreadSize, uint8_t(-1));
    std::vector<std::pair<State, uint8_t>> starting_positions(leaf_batch_size*kLeafThreadSize);
    std::vector<uint8_t> best_depths(leaf_batch_size*kLeafThreadSize, atomic_best_depth);
    std::vector<URotations> urotations(leaf_batch_size*kLeafThreadSize, {0, 0, 0, 0});
    std::vector<uint64_t> num_positions_leafs(leaf_batch_size*kLeafThreadSize, 0);

    // device updated
    uint8_t* d_rotation_idxs;
    std::pair<DState, uint8_t>* d_starting_positions;
    uint8_t* d_best_depths;
    URotations* d_urotations;
    uint64_t* d_num_positions_leafs;

    // allocate on device update
    MallocOnDevice(rotation_idxs, d_rotation_idxs);
    MallocOnDevice(starting_positions, d_starting_positions);
    MallocOnDevice(best_depths, d_best_depths);
    MallocOnDevice(urotations, d_urotations);
    MallocOnDevice(num_positions_leafs, d_num_positions_leafs);

    // local buffer
    std::queue<std::pair<State, uint8_t>> local_position_queue;

    while (true) {
        uint64_t finished_positions_idx = 0;
        uint8_t cur_best_depth = atomic_best_depth;
        for (uint64_t i = 0; i < leaf_batch_size*kLeafThreadSize; i++) {
            // better solution
            if (best_depths[i] < cur_best_depth) {
                // do a CPU search for this position
                LeafSearch(starting_positions[i].first, starting_positions[i].second, cur_best_depth,
                           best_endstate_leafs, visited_leaf,
                           num_positions_leaf, atomic_best_depth, thread_idx);
                // mark as finished
                rotation_idxs[i] = uint8_t(-1);
            }

            // update best depth
            best_depths[i] = cur_best_depth;

            // not yet finished with calculation
            if (rotation_idxs[i] != uint8_t(-1)) {
                continue;
            }

            if (local_position_queue.empty()) {
                std::shared_ptr<phmap::flat_hash_map<State, uint8_t>> local_buffer;
                while (true) {
                    {
                        // load from shared threads
                        std::lock_guard lock(mtx);
                        local_buffer = shared_leaf_states.load(std::memory_order_acquire);
                        if (local_buffer->size() > 0) {
                            std::shared_ptr<phmap::flat_hash_map<State, uint8_t>> new_empty_buffer = std::make_shared<phmap::flat_hash_map<State, uint8_t>>();
                            local_buffer = shared_leaf_states.exchange(new_empty_buffer, std::memory_order_acquire);
                            break;
                        }
                    }
                    // check if CPU search is already finished
                    if (stocken.stop_requested()) {
                        finished_positions_idx++;
                        break;
                    }
                    std::this_thread::yield(); // prevent busy spin burn
                }

                // insert all elements into local_position_queue
                for (const auto& new_pos : *local_buffer) {
                    local_position_queue.push(new_pos);
                }
            }

            // write new position
            rotation_idxs[i] = 0;
            starting_positions[i] = local_position_queue.front();
            local_position_queue.pop();
            urotations[i] = {0, 0, 0, 0};
        }

        // finished all positions
        if (finished_positions_idx == leaf_batch_size*kLeafThreadSize) {
            break;
        }

        // copy all to the GPU
        // during this time other threads can still do computation
        MemcpyToDevice(rotation_idxs, d_rotation_idxs);
        MemcpyToDevice(starting_positions, d_starting_positions);
        MemcpyToDevice(best_depths, d_best_depths);
        MemcpyToDevice(urotations, d_urotations);
        MemcpyToDevice(num_positions_leafs, d_num_positions_leafs);

        // only leaf_batch_size threads
        size_t grid_dim = ((leaf_batch_size-1)/kBlockDim)+1;
        DeviceLeafSearch<<<grid_dim, kBlockDim>>>(d_num_positions_leafs, d_leaf_thread_idxs, d_starting_positions,
                            d_rotation_idxs, d_best_depths, d_urotations, Settings::GetTBDepth());
        cudaError_t err = cudaGetLastError(); // launch of Device
        if (err != cudaSuccess) {
            LOG_CRITICAL("CUDA error:", cudaGetErrorString(err));
        }
        err = cudaDeviceSynchronize(); // end of Device
        if (err != cudaSuccess) {
            LOG_CRITICAL("CUDA error:", cudaGetErrorString(err));
        }

        // copy all from the GPU
        MemcpyFromDevice(rotation_idxs, d_rotation_idxs);
        MemcpyFromDevice(starting_positions, d_starting_positions);
        MemcpyFromDevice(best_depths, d_best_depths);
        MemcpyFromDevice(urotations, d_urotations);
        MemcpyFromDevice(num_positions_leafs, d_num_positions_leafs);
    }

    // free all memory
    FreeCudaPointer(d_rotation_idxs);
    FreeCudaPointer(d_starting_positions);
    FreeCudaPointer(d_best_depths);
    FreeCudaPointer(d_urotations);
    FreeCudaPointer(d_num_positions_leafs);

    // free device only memory
    FreeCudaPointer(d_leaf_thread_idxs);
}
