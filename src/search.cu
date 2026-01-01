#include <cuda_runtime_api.h>
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
    if ((!rev && rotation < 12) || rev) {
        corner_heuristic = uint16_t(-1);
    }
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


__global__ void DeviceLeafSearch (uint64_t* d_num_positions_leafs, const std::pair<DState, uint8_t>* d_starting_positions,
                                  uint8_t* d_rotation_idxs, uint8_t* d_best_depths, uint64_t* d_rotations_1, uint64_t* d_rotations_2, uint8_t tb_depth, const uint64_t num_gpu_threads) {
    // get current leaf thread idx
    uint32_t index = threadIdx.x + (blockIdx.x * blockDim.x);
    if (index >= num_gpu_threads) {
        return;
    }

    // load from global memory
    uint8_t rotation_idx = d_rotation_idxs[index];
    uint8_t depth_offset = d_starting_positions[index].second;
    uint8_t best_depth = d_best_depths[index];
    uint64_t num_positions = d_num_positions_leafs[index];

    // rotations
    uint64_t rotations_1 = d_rotations_1[index];
    uint64_t rotations_2 = d_rotations_2[index];

    // state
    uint16_t corner_orientation = d_starting_positions[index].first.hash_2 >> 20;
    uint16_t corner_position = d_starting_positions[index].first.hash_1;
    uint16_t edge_orientation = d_starting_positions[index].first.hash_3 >> 20;
    uint32_t edge_position_1 = d_starting_positions[index].first.hash_2 & ((uint32_t(1) << 20)-1);
    uint32_t edge_position_2 = d_starting_positions[index].first.hash_3 & ((uint32_t(1) << 20)-1);
    // heuristics
    uint16_t corner_heuristic = uint16_t(-1);
    uint8_t edge_heuristic_1 = uint8_t(-1);
    uint8_t edge_heuristic_2 = uint8_t(-1);

    // state
    uint16_t prev_corner_orientation = uint16_t(-1);
    uint16_t prev_corner_position = uint16_t(-1);
    uint16_t prev_edge_orientation = uint16_t(-1);
    uint32_t prev_edge_position_1 = uint32_t(-1);
    uint32_t prev_edge_position_2 = uint32_t(-1);
    // rotations
    uint16_t prev_corner_heuristic = uint16_t(-1);
    uint8_t prev_edge_heuristic_1 = uint8_t(-1);
    uint8_t prev_edge_heuristic_2 = uint8_t(-1);

    // do the rotations such that state is again at the outcome state it was previously (somewhere in the tree)
    for (uint8_t i = 0; i <= rotation_idx && rotation_idx != uint8_t(-1); i++) {
        uint8_t rotation = RotationsAt(rotations_1, rotations_2, i);
        if (rotation > kNumRotations) {
            DRotate(corner_orientation, corner_position, edge_orientation, edge_position_1, edge_position_2,
                    corner_heuristic, edge_heuristic_1, edge_heuristic_2,
                    prev_corner_orientation, prev_corner_position, prev_edge_orientation, prev_edge_position_1, prev_edge_position_2,
                    prev_corner_heuristic, prev_edge_heuristic_1, prev_edge_heuristic_2,
                    rotation ^ uint8_t(1<<7), false);
        }
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

        uint8_t rotation = RotationsAt(rotations_1, rotations_2, rotation_idx) & (uint8_t(-1)>>1);

        // finished with the rotations of the current position
        if (rotation == kNumRotations) {
            RotationsSet(rotations_1, rotations_2, rotation_idx, 0);
            rotation_idx--;
            continue;
        }

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
            if (rotation == kNumRotations) {
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
            if (rotation == kNumRotations) {
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
        num_positions++;

        // not able to improve the current leaf search skip this node
        uint8_t max_heuristic = GetMaxHeuristic(corner_orientation, corner_position, edge_orientation, edge_position_1, edge_position_2,
                                                corner_heuristic, edge_heuristic_1, edge_heuristic_2);

        // check if the current state is in tablebase and is therefore a new best solution
        if (max_heuristic <= tb_depth && DBCHTSetContains(d_tablebase, d_tablebase_size, DState(corner_orientation, corner_position, edge_orientation, edge_position_1, edge_position_2))) {
            uint8_t depth = rotation_idx + tb_depth + depth_offset;
            best_depth = min(depth, best_depth);
            DUndoRotate(corner_orientation, corner_position, edge_orientation, edge_position_1, edge_position_2,
                        corner_heuristic, edge_heuristic_1, edge_heuristic_2,
                        prev_corner_orientation, prev_corner_position, prev_edge_orientation, prev_edge_position_1, prev_edge_position_2, 
                        prev_corner_heuristic, prev_edge_heuristic_1, prev_edge_heuristic_2,
                        rotation_idx, rotations_1, rotations_2);
            continue;
        }

        if (max(tb_depth+1, max_heuristic) + rotation_idx + depth_offset >= best_depth) {
            DUndoRotate(corner_orientation, corner_position, edge_orientation, edge_position_1, edge_position_2,
                        corner_heuristic, edge_heuristic_1, edge_heuristic_2,
                        prev_corner_orientation, prev_corner_position, prev_edge_orientation, prev_edge_position_1, prev_edge_position_2, 
                        prev_corner_heuristic, prev_edge_heuristic_1, prev_edge_heuristic_2,
                        rotation_idx, rotations_1, rotations_2);
            continue;
        }
    }

    // save back to global memory;
    d_rotation_idxs[index] = rotation_idx;
    d_best_depths[index] = best_depth;
    d_rotations_1[index] = rotations_1;
    d_rotations_2[index] = rotations_2;
    d_num_positions_leafs[index] = num_positions;
}


// add new starting_positions from cpu
bool AddStartingPositions (std::queue<std::pair<State, uint8_t>>& local_position_queue,
                           std::stop_token& stocken, SharedLeafStates& shared_leaf_states,
                           std::vector<std::pair<State, uint8_t>>& starting_positions,
                           std::vector<uint8_t>& rotation_idxs,
                           std::vector<uint64_t>& num_positions_leafs
                           ) {
    // check if CPU search is already finished
    // it is guarantied that there is no positions in shared_leaf_states comeing after this point
    if (local_position_queue.empty() && stocken.stop_requested()) {
        return true;
    }

    for (int i = 0; i < Settings::GetNumGPUThreads(); i++) {
        // not yet finished with calculation
        if (rotation_idxs[i] != uint8_t(-1)) {
            continue;
        }

        // get new cpu data
        if (local_position_queue.empty()) {
            std::shared_ptr<phmap::flat_hash_map<State, uint8_t>> local_buffer;

            std::lock_guard<std::mutex> lock(shared_leaf_states.mtx);
            if (shared_leaf_states.shared_ptrs.empty()) {
                break;
            }

            local_buffer = std::move(shared_leaf_states.shared_ptrs.front());
            shared_leaf_states.shared_ptrs.pop();
            shared_leaf_states.cv.notify_one();

            // insert all elements into local_position_queue
            for (const auto& new_pos : *local_buffer) {
                local_position_queue.push(new_pos);
            }
        }

        // write new position
        num_positions_leafs[i]++; // add starting position
        rotation_idxs[i] = 0;
        starting_positions[i] = local_position_queue.front();
        local_position_queue.pop();
    }
    return false;
}


// split starting positions that are left in finished starting_positions
void SplitStartingPositions (uint64_t& cur_split_idx, VisitedMap& visited_leaf,
                             std::atomic<uint8_t>& atomic_best_depth, std::pair<State, uint8_t>& best_endstate_leafs, const int& thread_idx,
                             std::vector<uint8_t>& rotation_idxs,
                             std::vector<uint64_t>& rotations_1,
                             std::vector<uint64_t>& rotations_2,
                             std::vector<std::pair<State, uint8_t>>& starting_positions,
                             std::vector<uint64_t>& num_positions_leafs, uint64_t num_positions_leaf
                             ) {
    uint64_t finished_cnt = 0;
    std::vector<bool> finished(Settings::GetNumGPUThreads(), false);
    for (int i = 0; i < Settings::GetNumGPUThreads(); i++) {
        if (rotation_idxs[i] == uint8_t(-1)) {
            finished[i] = true;
            finished_cnt++;
        }
    }

    int next_idx = 0;

    uint64_t stop_split_idx = (cur_split_idx + Settings::GetNumGPUThreads() - 1) % Settings::GetNumGPUThreads();
    while (stop_split_idx != cur_split_idx && finished_cnt > kNumRotations) {
        if (finished[cur_split_idx]) {
            cur_split_idx++;
            cur_split_idx %= Settings::GetNumGPUThreads();
            continue;
        }

        if (rotation_idxs[cur_split_idx] == uint8_t(-1) ||
            rotation_idxs[cur_split_idx] == 0) {
            // LOG_WARNING("Should never occur", rotation_idxs[cur_split_idx]);
            cur_split_idx++;
            cur_split_idx %= Settings::GetNumGPUThreads();
            continue;
        }

        // split up
        // it is guarantied that rotation_idx > 0
        std::pair<State, uint8_t> starting_position = starting_positions[cur_split_idx];
        // insert into visited_leaf such that it can be traced back
        auto find_visited = visited_leaf.find(starting_position.first);
        if (find_visited == visited_leaf.end()) {
            visited_leaf.insert(starting_position); // found new solution
        }
        else if (find_visited->second > starting_position.second) {
            find_visited->second = starting_position.second;
        }

        uint64_t rotation_1 = rotations_1[cur_split_idx];
        uint64_t rotation_2 = rotations_2[cur_split_idx];

        uint8_t rotation = RotationsAt(rotation_1, rotation_2, 0) ^ uint8_t(1<<7); // as it is a rev move (else rotation_idx == 0)
        if (rotation >= kNumRotations) {
            LOG_EXTRA("UROTATIONS", rotation_1, rotation_2);
            LOG_CRITICAL("Rotation is too big", int(rotation), "rotation_idx:", int(rotation_idxs[cur_split_idx]));
        }

        // do the current rotation
        std::pair<State, uint8_t> new_starting_position = {Cube::Rotate(starting_position.first, rotation).second, starting_position.second+1};

        // shift urotation by a move
        for (int i = 0; i < 16-1; i++) {
            RotationsSet(rotations_1[cur_split_idx], rotations_2[cur_split_idx], i,
                         RotationsAt(rotation_1, rotation_2, i+1));
        }

        // update old starting state
        starting_positions[cur_split_idx] = new_starting_position;
        rotation_idxs[cur_split_idx]--;

        for (int rot = rotation+1; rot < kNumRotations; rot++) {
            std::pair<bool, State> next_rot = Cube::Rotate(starting_position.first, rot);
            if (!next_rot.first) {
                continue;
            }

            if (BCHTSetContains(Tablebase::tablebase.back(), next_rot.second)) {
                uint8_t tot_depth = starting_position.second+1 + Settings::GetTBDepth();
                if (tot_depth < uint8_t(atomic_best_depth)) {
                    best_endstate_leafs = {next_rot.second, tot_depth};
                    AtomicMin(atomic_best_depth, tot_depth);
                    LOG_EXTRA("best sol", SkipSpace(thread_idx), ":", int(best_endstate_leafs.second), "leaf_search:", num_positions_leaf);
                    LOG_MEMORY();
                    continue;
                }
            }

            // it is guarantied that there is a next idx
            while (!finished[next_idx]) {
                next_idx++;
            }

            // ability to trace back the real starting position of a new best solution
            starting_positions[next_idx] = {next_rot.second, starting_position.second+1};
            rotations_1[next_idx] = 0;
            rotations_2[next_idx] = 0;
            rotation_idxs[next_idx] = 0;
            num_positions_leafs[next_idx] = 1;

            next_idx++;
            finished_cnt--;
        }
    }
}

// remove the associated data from finished starting_positions
// handel new found best solutions
void FinishedStartingPositions (std::atomic<uint8_t>& atomic_best_depth, VisitedMap& visited_leaf,
                                std::pair<State, uint8_t>& best_endstate_leafs, uint64_t& num_positions_leaf, const int& thread_idx,
                                std::vector<uint8_t>& rotation_idxs,
                                std::vector<std::pair<State, uint8_t>>& starting_positions,
                                std::vector<uint8_t>& best_depths,
                                std::vector<uint64_t>& rotations_1,
                                std::vector<uint64_t>& rotations_2,
                                std::vector<uint64_t>& num_positions_leafs
                                ) {
    uint8_t cur_best_depth = atomic_best_depth;
    for (int i = 0; i < Settings::GetNumGPUThreads(); i++) {
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

        rotations_1[i] = 0;
        rotations_2[i] = 0;
    }
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

    // create stream
    cudaStream_t cuda_stream;
    cudaStreamCreate(&cuda_stream);

    // updated for each position
    std::vector<uint8_t> rotation_idxs(Settings::GetNumGPUThreads(), uint8_t(-1));
    std::vector<std::pair<State, uint8_t>> starting_positions(Settings::GetNumGPUThreads());
    std::vector<uint8_t> best_depths(Settings::GetNumGPUThreads(), atomic_best_depth);
    std::vector<uint64_t> rotations_1(Settings::GetNumGPUThreads(), 0);
    std::vector<uint64_t> rotations_2(Settings::GetNumGPUThreads(), 0);
    std::vector<uint64_t> num_positions_leafs(Settings::GetNumGPUThreads(), 0);

    // pin host code
    HostRegister(rotation_idxs);
    HostRegister(starting_positions);
    HostRegister(best_depths);
    HostRegister(rotations_1);
    HostRegister(rotations_2);
    HostRegister(num_positions_leafs);

    // device updated
    uint8_t* d_rotation_idxs;
    std::pair<DState, uint8_t>* d_starting_positions;
    uint8_t* d_best_depths;
    uint64_t* d_rotations_1;
    uint64_t* d_rotations_2;
    uint64_t* d_num_positions_leafs;

    // allocate on device update
    MallocOnDevice(rotation_idxs, d_rotation_idxs);
    MallocOnDevice(starting_positions, d_starting_positions);
    MallocOnDevice(best_depths, d_best_depths);
    MallocOnDevice(rotations_1, d_rotations_1);
    MallocOnDevice(rotations_2, d_rotations_2);
    MallocOnDevice(num_positions_leafs, d_num_positions_leafs);

    // local buffer
    std::queue<std::pair<State, uint8_t>> local_position_queue;

    // split positions
    uint64_t cur_split_idx = 0;

    while (true) {
        bool cpu_stop = AddStartingPositions(local_position_queue, stocken, shared_leaf_states, starting_positions, rotation_idxs, num_positions_leafs);

        // update all best depths
        uint8_t cur_best_depth = atomic_best_depth;
        for (int i = 0; i < Settings::GetNumGPUThreads(); i++) {
            best_depths[i] = cur_best_depth;
        }

        // check finished all positions
        if (cpu_stop) {
            bool has_work = false;
            for (int i = 0; i < Settings::GetNumGPUThreads(); i++) {
                // not yet finished with calculation
                if (rotation_idxs[i] != uint8_t(-1)) {
                    has_work = true;
                    break;
                }
            }
            if (!has_work) {
                break;
            }
        }

        // copy all to the GPU
        // during this time other threads can still do computation
        MemcpyToDeviceStream(rotation_idxs, d_rotation_idxs, cuda_stream);
        MemcpyToDeviceStream(starting_positions, d_starting_positions, cuda_stream);
        MemcpyToDeviceStream(best_depths, d_best_depths, cuda_stream);
        MemcpyToDeviceStream(rotations_1, d_rotations_1, cuda_stream);
        MemcpyToDeviceStream(rotations_2, d_rotations_2, cuda_stream);
        MemcpyToDeviceStream(num_positions_leafs, d_num_positions_leafs, cuda_stream);

        // only leaf_batch_size threads
        cudaEvent_t evt;
        cudaEventCreateWithFlags(&evt, cudaEventDisableTiming);
        size_t grid_dim = ((Settings::GetNumGPUThreads()-1)/kBlockDim)+1;
        DeviceLeafSearch<<<grid_dim, kBlockDim, 0, cuda_stream>>>(d_num_positions_leafs, d_starting_positions,
                            d_rotation_idxs, d_best_depths, d_rotations_1, d_rotations_2, Settings::GetTBDepth(), Settings::GetNumGPUThreads());
        cudaEventRecord(evt, cuda_stream);

        // wait kernal finished
        while (cudaEventQuery(evt) == cudaErrorNotReady) {
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }

        // copy all from the GPU as soon an kernals are finished
        MemcpyFromDeviceStream(rotation_idxs, d_rotation_idxs, cuda_stream);
        MemcpyFromDeviceStream(best_depths, d_best_depths, cuda_stream);
        MemcpyFromDeviceStream(rotations_1, d_rotations_1, cuda_stream);
        MemcpyFromDeviceStream(rotations_2, d_rotations_2, cuda_stream);
        MemcpyFromDeviceStream(num_positions_leafs, d_num_positions_leafs, cuda_stream);

        err = cudaStreamSynchronize(cuda_stream); // wait until all memcpy is done
        if (err != cudaSuccess) {
            LOG_CRITICAL("CUDA error:", cudaGetErrorString(err));
        }

        FinishedStartingPositions(atomic_best_depth, visited_leaf, best_endstate_leafs, num_positions_leaf, thread_idx, rotation_idxs, starting_positions, best_depths, rotations_1, rotations_2, num_positions_leafs);
        SplitStartingPositions(cur_split_idx, visited_leaf, atomic_best_depth, best_endstate_leafs, thread_idx, rotation_idxs, rotations_1, rotations_2, starting_positions, num_positions_leafs, num_positions_leaf);
    }

    LOG_EXTRA(SkipSpace("#"), thread_idx, "finished with all kernels");

    // free all memory
    FreeCudaPointer(d_rotation_idxs);
    FreeCudaPointer(d_starting_positions);
    FreeCudaPointer(d_best_depths);
    FreeCudaPointer(d_rotations_1);
    FreeCudaPointer(d_rotations_2);
    FreeCudaPointer(d_num_positions_leafs);

    // Unpin host data
    cudaHostUnregister(rotation_idxs.data());
    cudaHostUnregister(starting_positions.data());
    cudaHostUnregister(best_depths.data());
    cudaHostUnregister(rotations_1.data());
    cudaHostUnregister(rotations_2.data());
    cudaHostUnregister(num_positions_leafs.data());

    cudaStreamDestroy(cuda_stream);
}
