#include <cuda_runtime_api.h>
#include <driver_types.h>
#include <cassert>
#include <cstdint>
#include <queue>
#include <stop_token>
#include <vector>

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


__global__ void DeviceLeafSearch (uint64_t* d_num_positions_leafs, const std::pair<DState, uint8_t>* d_starting_positions,
                                  uint8_t* d_rotation_idxs, uint8_t* d_best_depths, URotations* d_urotations, uint8_t tb_depth, const uint64_t leaf_batch_size,
                                  DCube dcube) {
    // get current leaf thread idx
    size_t index = threadIdx.x + (size_t(blockIdx.x) * blockDim.x);
    if (index >= leaf_batch_size) {
        return;
    }

    // load from global memory
    uint8_t rotation_idx = d_rotation_idxs[index];
    DState state = d_starting_positions[index].first;
    uint8_t depth_offset = d_starting_positions[index].second;
    uint8_t best_depth = d_best_depths[index];
    URotations rotations = d_urotations[index];
    uint64_t num_positions = d_num_positions_leafs[index];

    // do the rotations such that state is again at the outcome state it was previously (somewhere in the tree)
    for (int i = 0; i < rotation_idx && rotation_idx != uint8_t(-1); i++) {
        if (rotations.At(i) > kNumRotations) {
            state = dcube.Rotate(state, rotations.At(i) ^ uint8_t(1<<7)).state;
        }
    }
    if (rotation_idx != uint8_t(-1) && rotations.At(rotation_idx) > kNumRotations) {
        state = dcube.Rotate(state, rotations.At(rotation_idx) ^ uint8_t(1<<7)).state;
    }

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

        uint8_t rotation = rotations.At(rotation_idx) & (uint8_t(-1)>>1);

        // finished with the rotations of the current position
        if (rotation == kNumRotations) {  // NOLINT
            rotations.Set(rotation_idx, 0);
            rotation_idx--;
            continue;
        }

        // undo the rotation to continue the search on the next subtree
        bool rev = (rotations.At(rotation_idx) ^ rotation) != 0;
        if (rev) {
            rotation = DGetRevRotation(rotation);
        }

        // do the rotation
        DRotateReturn next_pos = dcube.Rotate(state, rotation);
        // it is garantied that the undo rotation of a cube is always possible in this leaf search
        // if it is an illegal search skip this rotation
        if (!next_pos.isLegal) {
            rotations.Add(rotation_idx, 1);
            continue;
        }
        state = next_pos.state;
        // prepare the next rotation
        rotations.BitXOR(rotation_idx, 1<<7);  // NOLINT

        // undo rotation done increase to next rotation
        if (rev) {
            rotations.Add(rotation_idx, 1);
            continue;
        }

        // go inside the next position
        rotation_idx++;
        num_positions++;

        // not able to improve the current leaf search skip this node
        DHeuristics heuristics;
        // check if the current state is in tablebase and is therefore a new best solution
        if (dcube.DTablebaseContains(state)) {
            uint8_t depth = rotation_idx + tb_depth + depth_offset;
            best_depth = min(depth, best_depth);
            rotations.Set(rotation_idx, kNumRotations);
            continue;
        }

        if (max(tb_depth+1, heuristics.GetMaxHeuristic(state, dcube)) + rotation_idx + depth_offset >= best_depth) {
            rotations.Set(rotation_idx, kNumRotations);
            continue;
        }
    }

    // save back to global memory;
    d_rotation_idxs[index] = rotation_idx;
    d_best_depths[index] = best_depth;
    d_urotations[index] = rotations; // not really necessary
    d_num_positions_leafs[index] = num_positions;
}


void DeviceLeafManager (std::stop_token& stocken, std::atomic<std::shared_ptr<phmap::flat_hash_map<State, uint8_t>>>& shared_leaf_states,
                        std::mutex& mtx, uint64_t& num_positions_leaf, VisitedMap& visited_leaf,
                        std::atomic<uint8_t>& atomic_best_depth, std::pair<State, uint8_t>& best_endstate_leafs,
                        const uint64_t& leaf_batch_size, const int& thread_idx) {
    // set the device for this thread
    int gpu_device_idx = thread_idx % Settings::GetDeviceCount();
    cudaError_t err = cudaSetDevice(gpu_device_idx);
    if (err != cudaSuccess) {
        LOG_CRITICAL(cudaGetErrorString(err));
    }

    // create stream
    cudaStream_t cuda_stream;
    cudaStreamCreate(&cuda_stream);

    // updated for each position
    std::vector<uint8_t> rotation_idxs(leaf_batch_size, uint8_t(-1));
    std::vector<std::pair<State, uint8_t>> starting_positions(leaf_batch_size);
    std::vector<uint8_t> best_depths(leaf_batch_size, atomic_best_depth);
    std::vector<URotations> urotations(leaf_batch_size, {0, 0, 0, 0});
    std::vector<uint64_t> num_positions_leafs(leaf_batch_size, 0);

    // pin host code
    HostRegister(rotation_idxs);
    HostRegister(starting_positions);
    HostRegister(best_depths);
    HostRegister(urotations);
    HostRegister(num_positions_leafs);

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
    bool first_finished_position = true;

    uint64_t cur_finished_split_idx = 0;

    while (true) {
        std::queue<uint64_t> finished_positions;

        uint8_t cur_best_depth = atomic_best_depth;
        for (uint64_t i = 0; i < leaf_batch_size; i++) {
            // better solution
            if (best_depths[i] < cur_best_depth) {
                // do a CPU search for this position
                LeafSearch(starting_positions[i].first, starting_positions[i].second, cur_best_depth,
                           best_endstate_leafs, visited_leaf,
                           num_positions_leaf, atomic_best_depth, thread_idx);
                auto find_visited = visited_leaf.find(starting_positions[i].first);
                if (find_visited == visited_leaf.end()) {
                    visited_leaf.insert(starting_positions[i]); // found new solution
                }
                else if (find_visited->second > starting_positions[i].second) {
                    find_visited->second = starting_positions[i].second;
                }
                // mark as finished
                rotation_idxs[i] = uint8_t(-1);
            }

            // not yet finished with calculation
            if (rotation_idxs[i] != uint8_t(-1)) {
                continue;
            }

            // finished with calculation
            num_positions_leaf += num_positions_leafs[i];
            num_positions_leafs[i] = 0;

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
                        finished_positions.push(i);
                        if (first_finished_position) {
                            first_finished_position = false;
                        }
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
            if (local_position_queue.empty()) {
                continue;
            }

            // write new position
            num_positions_leafs[i]++; // add starting position
            rotation_idxs[i] = 0;
            starting_positions[i] = local_position_queue.front();
            local_position_queue.pop();
            urotations[i] = {0, 0, 0, 0};
        }
        // update all best depths
        cur_best_depth = atomic_best_depth;
        for (uint64_t i = 0; i < leaf_batch_size; i++) {
            best_depths[i] = cur_best_depth;
        }

        // finished all positions
        if (finished_positions.size() == leaf_batch_size) {
            break;
        }

        if (!first_finished_position) {
            uint64_t last_splitable_cnt = 0;
            while (finished_positions.size() >= kNumRotations-1 && last_splitable_cnt < leaf_batch_size) {
                if (rotation_idxs[cur_finished_split_idx] == uint8_t(-1) ||
                    rotation_idxs[cur_finished_split_idx] == 0) {
                    last_splitable_cnt++;

                    cur_finished_split_idx++;
                    cur_finished_split_idx %= leaf_batch_size;
                    continue;
                }
                last_splitable_cnt = 0;

                // split up
                // it is guarantied that rotation_idx > 0
                std::pair<State, uint8_t> starting_position = starting_positions[cur_finished_split_idx];
                // insert into visited_leaf such that it can be traced back
                auto find_visited = visited_leaf.find(starting_position.first);
                if (find_visited == visited_leaf.end()) {
                    visited_leaf.insert(starting_position); // found new solution
                }
                else if (find_visited->second > starting_position.second) {
                    find_visited->second = starting_position.second;
                }

                URotations urotation = urotations[cur_finished_split_idx];

                uint8_t rotation = urotation.At(0) ^ uint8_t(1<<7); // as it is a rev move (else rotation_idx == 0)
                if (rotation >= kNumRotations) {
                    LOG_EXTRA("UROTATIONS", urotation.data[0], urotation.data[1], urotation.data[2], urotation.data[3]);
                    LOG_CRITICAL("Rotation is too big", int(rotation), "rotation_idx:", int(rotation_idxs[cur_finished_split_idx]));
                }

                // do the current rotation
                std::pair<State, uint8_t> new_starting_position = {Cube::Rotate(starting_position.first, rotation).second, starting_position.second+1};

                // shift urotation by a move
                URotations new_urotation = {0, 0, 0, 0};
                for (int i = 0; i < kURotationSize-1; i++) {
                    new_urotation.Set(i, urotation.At(i+1));
                }

                // update old starting state
                starting_positions[cur_finished_split_idx] = new_starting_position;
                urotations[cur_finished_split_idx] = new_urotation;
                rotation_idxs[cur_finished_split_idx]--;

                for (int rot = rotation+1; rot < kNumRotations; rot++) {
                    std::pair<bool, State> next_rot = Cube::Rotate(starting_position.first, rot);
                    if (!next_rot.first) {
                        continue;
                    }

                    if (BCHTSetContains(Tablebase::tablebase.back(), next_rot.second)) {
                        uint8_t tot_depth = starting_position.second+1 + Settings::GetTBDepth();
                        if (tot_depth < std::min(uint8_t(atomic_best_depth), cur_best_depth)) {
                            best_endstate_leafs = {next_rot.second, tot_depth};
                            cur_best_depth = std::min(cur_best_depth, tot_depth);
                            AtomicMin(atomic_best_depth, tot_depth);
                            LOG_EXTRA("best sol", SkipSpace(thread_idx), ":", int(best_endstate_leafs.second), "leaf_search:", num_positions_leaf);
                            LOG_MEMORY();
                            continue;
                        }
                    }

                    // new empty index
                    int next_idx = finished_positions.front();
                    finished_positions.pop();

                    // ability to trace back the real starting position of a new best solution
                    starting_positions[next_idx] = {next_rot.second, starting_position.second+1};
                    urotations[next_idx] = {0, 0, 0, 0};
                    rotation_idxs[next_idx] = 0;
                    num_positions_leafs[next_idx] = 1;
                }
            }
        }

        // copy all to the GPU
        // during this time other threads can still do computation
        MemcpyToDeviceStream(rotation_idxs, d_rotation_idxs, cuda_stream);
        MemcpyToDeviceStream(starting_positions, d_starting_positions, cuda_stream);
        MemcpyToDeviceStream(best_depths, d_best_depths, cuda_stream);
        MemcpyToDeviceStream(urotations, d_urotations, cuda_stream);
        MemcpyToDeviceStream(num_positions_leafs, d_num_positions_leafs, cuda_stream);

        // only leaf_batch_size threads
        size_t grid_dim = ((leaf_batch_size-1)/kBlockDim)+1;
        DeviceLeafSearch<<<grid_dim, kBlockDim, 0, cuda_stream>>>(d_num_positions_leafs, d_starting_positions,
                            d_rotation_idxs, d_best_depths, d_urotations, Settings::GetTBDepth(), leaf_batch_size, GetDCube(gpu_device_idx));
        cudaError_t err = cudaGetLastError(); // launch of Device
        if (err != cudaSuccess) {
            LOG_CRITICAL("CUDA error:", cudaGetErrorString(err));
        }

        // copy all from the GPU as soon an kernals are finished
        MemcpyFromDeviceStream(rotation_idxs, d_rotation_idxs, cuda_stream);
        MemcpyFromDeviceStream(best_depths, d_best_depths, cuda_stream);
        MemcpyFromDeviceStream(urotations, d_urotations, cuda_stream);
        MemcpyFromDeviceStream(num_positions_leafs, d_num_positions_leafs, cuda_stream);

        err = cudaStreamSynchronize(cuda_stream); // wait until all memcpy is done
        if (err != cudaSuccess) {
            LOG_CRITICAL("CUDA error:", cudaGetErrorString(err));
        }
    }

    LOG_EXTRA(SkipSpace("#"), thread_idx, "finished with all kernels");

    // free all memory
    FreeCudaPointer(d_rotation_idxs);
    FreeCudaPointer(d_starting_positions);
    FreeCudaPointer(d_best_depths);
    FreeCudaPointer(d_urotations);
    FreeCudaPointer(d_num_positions_leafs);

    // Unpin host data
    cudaHostUnregister(rotation_idxs.data());
    cudaHostUnregister(starting_positions.data());
    cudaHostUnregister(best_depths.data());
    cudaHostUnregister(urotations.data());
    cudaHostUnregister(num_positions_leafs.data());

    cudaStreamDestroy(cuda_stream);
}
