#include <cstdint>

#include "utils.hpp"
#include "cuda_memory_transfer.cuh"
#include "search.hpp"


void DeviceLeafManager (const std::vector<std::pair<State, uint8_t>>& starting_positions, uint64_t& num_positions_leaf, VisitedMap& visited_leaf,
                        std::atomic<uint8_t>& atomic_best_depth, std::pair<State, uint8_t>& best_endstate_leafs,
                        [[maybe_unused]] const uint64_t& leaf_batch_size, const int& thread_idx) {
    uint64_t sp_size = starting_positions.size();

    std::pair<State, uint8_t>* d_starting_positions;
    UploadToDevice(starting_positions, d_starting_positions);

    std::vector<uint64_t> num_positions_leafs(sp_size, 0);
    uint64_t* d_num_positions_leafs;
    UploadToDevice(num_positions_leafs, d_num_positions_leafs);

    std::vector<uint8_t> best_depths(sp_size, atomic_best_depth);
    uint8_t* d_best_depths;
    UploadToDevice(best_depths, d_best_depths);

    // TODO: host

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
        for (const std::pair<State, uint8_t>& starting_position : starting_positions) {
            uint8_t best_depth = atomic_best_depth;
            LeafSearch(starting_position.first, starting_position.second, best_depth,
                       best_endstate_leafs, visited_leaf,
                       num_positions_leaf, atomic_best_depth, thread_idx);
        }
    }
}
