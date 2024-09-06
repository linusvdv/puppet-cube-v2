#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <parallel_hashmap/phmap.h>


#include "actions.h"
#include "cube.h"
#include "error_handler.h"


struct PositionHash {
    std::size_t operator() (const std::pair<unsigned int, uint64_t>& hashes) const {
        uint64_t seed = hashes.first;
        seed ^= hashes.second + 0x9e3779b9 + (seed<<6) + (seed>>2);
        return seed;
    }
};


bool Search(ErrorHandler error_handler, Actions& actions, Cube& cube, int depth, uint64_t& num_positions, phmap::flat_hash_map<std::pair<unsigned int, uint64_t>, int, PositionHash>& visited);
