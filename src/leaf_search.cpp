#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <thread>
#include <utility>

#include "corner.hpp"
#include "cube.hpp"
#include "duplicate_rotations.hpp"
#include "edge.hpp"
#include "rotation.hpp"
#include "search.hpp"
#include "settings.hpp"
#include "tablebase.hpp"
#include "transposition_table.hpp"


namespace {


// solution check cadence of the dfs (same as the number of positions per gpu kernel call)
constexpr int kNumPosBatchSize = 200;
// sleep time between queue polls when there is no work
constexpr auto kLeafPollInterval = std::chrono::microseconds(100);


void CpuLeafDFS (State state, uint8_t starting_depth, uint8_t cur_depth,
                 SharedLeafSolution& shared_leaf_solution, SharedLeafStates& shared_leaf_states,
                 uint64_t& num_positions) {
    const uint8_t tb_depth = Settings::GetTBDepth();

    // rotations
    // slots[i] is the next rotation to try at dfs level i (for i < idx it is the rotation done at level i + 1)
    uint8_t slots[16] = {};
    int8_t idx = 0;

    // get helpful rotations
    uint64_t full_corner_heuristic = corner::GetHeuristic(state.corner_pos, state.corner_orient);
    uint8_t corner_heuristic = full_corner_heuristic;
    constexpr uint64_t kRotationsMask = 0x555555555ULL;
    uint64_t rotations = (full_corner_heuristic >> 8) & (full_corner_heuristic >> 9) & kRotationsMask;
    if (2 + corner_heuristic + idx + starting_depth >= cur_depth) {
        rotations |= (full_corner_heuristic >> 9) & kRotationsMask;
    }
    if (1 + corner_heuristic + idx + starting_depth >= cur_depth) {
        rotations |= (full_corner_heuristic >> 8) & kRotationsMask;
    }
    rotations |= rotations << 1;
    uint8_t rotation = slots[idx];
    rotations |= (1ULL<<(2*rotation))-1;

    // the goal is to search further in the dfs (from the leaf position) and stop if an improvement to the best_depth is not possible any more
    // for each loop cycle it will look at a new position or undo the move it has done during the dfs
    // in contrast to the gpu search the dfs runs to completion (no work splitting and no resumability)
    // the state is the current position of the search after all rotations from the leaf starting position
    int cur_pos_batch = 0;
    while (true) {
        // check with the same cadence as a gpu kernel run if a solution was already found
        if (++cur_pos_batch >= kNumPosBatchSize) {
            cur_pos_batch = 0;
            if (shared_leaf_solution.finished.load(std::memory_order_acquire)) {
                return;
            }
        }

        rotation = std::countr_one(rotations) / 2;
        slots[idx] = rotation+1;
        rotations |= 3ULL << (2*rotation);

        // the whole dfs is finished
        if (idx == 0 && rotation >= kNumRot) {
            break;
        }

        bool rev = false;
        // no rotation left at this level -> undo the rotation done one level below
        if (rotation >= kNumRot) {
            slots[idx] = 0;
            idx--;
            rotation = GetRevRotation(slots[idx]-1);
            rev = true;
        }
        else {
            if (idx > 0) {
                if (DuplicateRotations::IsDuplicate(slots[idx-1]-1, rotation)) {
                    continue;
                }
            }
        }

        // only edge rotation
        uint32_t prev_edge_pos = state.edge_pos;
        uint16_t prev_edge_orient = state.edge_orient;
        uint8_t prev_edge_sym = state.edge_sym;
        edge::Rotate(state.edge_pos, state.edge_sym, state.edge_orient, rotation);
        // next corner heuristic
        uint8_t max_heuristic = corner_heuristic + ((full_corner_heuristic >> (8 + 2*rotation)) & 3) - 1;
        if (!rev && max_heuristic < 14) {
            // edge heuristic
            max_heuristic = std::max(max_heuristic, edge::GetHeuristic(state.edge_pos, state.edge_orient));
            if (max_heuristic + idx + 1 + starting_depth >= cur_depth) {
                state.edge_pos = prev_edge_pos;
                state.edge_orient = prev_edge_orient;
                state.edge_sym = prev_edge_sym;
                num_positions++;
                continue;
            }
        }
        corner::Rotate(state.corner_pos, state.corner_orient, rotation);

        if (!rev) {
            slots[idx] = rotation+1;
            idx++;
            num_positions++;
        }

        // get helpful rotations
        full_corner_heuristic = corner::GetHeuristic(state.corner_pos, state.corner_orient);
        corner_heuristic = full_corner_heuristic;
        rotations = (full_corner_heuristic >> 8) & (full_corner_heuristic >> 9) & kRotationsMask;
        if (2 + corner_heuristic + idx + starting_depth >= cur_depth) {
            rotations |= (full_corner_heuristic >> 9) & kRotationsMask;
        }
        if (1 + corner_heuristic + idx + starting_depth >= cur_depth) {
            rotations |= (full_corner_heuristic >> 8) & kRotationsMask;
        }
        rotations |= rotations << 1;
        rotation = slots[idx];
        rotations |= (1ULL<<(2*rotation))-1;

        if (rev) {
            continue;
        }

        // in tablebase -> found a solution
        if (max_heuristic <= tb_depth && tablebase::Contains(state)) {
            // first solution wins
            bool winner = false;
            {
                std::lock_guard<std::mutex> lock(shared_leaf_states.mtx);
                if (!shared_leaf_solution.finished.load(std::memory_order_relaxed)) {
                    shared_leaf_solution.state = state;
                    shared_leaf_solution.finished.store(true, std::memory_order_release);
                    shared_leaf_states.cv.notify_all();
                    winner = true;
                }
            }
            // walk the path backwards and insert every position into the tt for the reconstruction
            if (winner) {
                while (true) {
                    transposition_table::Insert<true>(state, 2*(cur_depth-Settings::GetTBDepth()-1));
                    idx--;
                    cur_depth--;
                    if (idx == -1) {
                        break;
                    }
                    uint8_t back_rotation = GetRevRotation(slots[idx]-1);
                    corner::Rotate(state.corner_pos, state.corner_orient, back_rotation);
                    edge::Rotate(state.edge_pos, state.edge_sym, state.edge_orient, back_rotation);
                }
            }
            break;
        }

        // not possible with the current tablebase
        if (tb_depth+1 + idx + starting_depth >= cur_depth) {
            rotations |= (1ULL<<(2*kNumRot))-1;
            slots[idx] = kNumRot;
        }
    }
}


}


void CpuLeafManager (SharedSearch& shared_search, SharedLeafStates& shared_leaf_states, SharedLeafSolution& shared_leaf_solution) {
    while (true) {
        shared_search.start_work->arrive_and_wait();
        // stopping of the program
        if (shared_search.finished.load()) {
            break;
        }

        // get data from main thread
        uint8_t solution_depth = shared_leaf_states.depth;

        // resetting everything of the last depth iteration
        uint64_t local_leaf_cnt = 0;
        // the batches are pushed without a cv notification (the gpu leaf managers poll as well)
        // thus the workers poll the queue instead of waiting on the cv
        while (true) {
            if (shared_leaf_solution.finished.load(std::memory_order_acquire)) {
                break;
            }

            LocalBuffer local_buffer;
            {
                std::lock_guard<std::mutex> lock(shared_leaf_states.mtx);
                // get from shared
                if (!shared_leaf_states.shared_ptrs.empty()) {
                    local_buffer = std::move(shared_leaf_states.shared_ptrs.front());
                    shared_leaf_states.shared_ptrs.pop();
                    shared_leaf_states.cv.notify_one();
                }
            }

            if (local_buffer) {
                for (const auto& [state, starting_depth] : *local_buffer) {
                    if (shared_leaf_solution.finished.load(std::memory_order_acquire)) {
                        break;
                    }
                    CpuLeafDFS(state, starting_depth, solution_depth, shared_leaf_solution, shared_leaf_states, local_leaf_cnt);
                }
                continue;
            }

            // no batches left and the depth is finished
            if (shared_leaf_states.finished_depth.load()) {
                break;
            }

            std::this_thread::sleep_for(kLeafPollInterval);
        }

        // accumulate num_positions
        shared_search.leaf_cnt.fetch_add(local_leaf_cnt);
        // finished
        shared_search.done_work->arrive_and_wait();
    }
}
