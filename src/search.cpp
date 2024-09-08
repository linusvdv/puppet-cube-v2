#include <cstdint>
#include <queue>
#include <vector>
#include <X11/extensions/randr.h>
#include <parallel_hashmap/phmap.h>


#include "actions.h"
#include "cube.h"
#include "error_handler.h"
#include "rotation.h"
#include "search.h"


// positions reached from the solved cube after a specific number of moves
std::vector<phmap::flat_hash_set<std::pair<unsigned int, uint64_t>, PositionHash>> tablebase;


// iterative deepening depth first search with heuristic function
bool Search (ErrorHandler error_handler, Actions& actions, Cube& cube, int depth,
        uint64_t& num_positions, CubeHashMap& visited) {
    // check if this position has been already visited
    auto saved_depth = visited.find({cube.GetCornerHash(), cube.GetEdgeHash()});
    if (saved_depth != visited.end() && saved_depth->second >= depth) {
        return false;
    }

    num_positions++;

    // solved positions
    for (auto& tb : tablebase) {
        if (tb.contains({cube.GetCornerHash(), cube.GetEdgeHash()})){
            return true;
        }
    }

    // stop if the solved position cannot be reached in the time
    // TODO: heuristic function
    if (depth <= 0 || actions.stop) {
        return false;
    }

    visited[{cube.GetCornerHash(), cube.GetEdgeHash()}] = depth;

    // dfs
    std::vector<Rotations> legal_rotations = GetLegalRotations(cube);
    for (Rotations rotation : legal_rotations) {
        Cube next_cube = Rotate(cube, rotation);
        if (Search(error_handler, actions, next_cube, depth-1, num_positions, visited)) {
            actions.solve.push(rotation);
            return true;
        }
    }

    return false;
}


// this function will use a BFS to find all positions of specific depth
void TablebaseInitialisation (ErrorHandler error_handler, int depth) {
    std::queue<std::pair<unsigned int, uint64_t>> current;

    // solved position
    current.push({0, 0});
    tablebase.push_back(phmap::flat_hash_set<std::pair<unsigned int, uint64_t>, PositionHash>());
    tablebase[0].insert({0, 0});

    for (int i = 0; i < depth; i++) {
        tablebase.push_back(phmap::flat_hash_set<std::pair<unsigned int, uint64_t>, PositionHash>());
        std::queue<std::pair<unsigned int, uint64_t>> next;

        // go over all positions of this depth
        while (!current.empty()) {
            Cube cube = DecodeHash(current.front().first, current.front().second);
            current.pop();

            // do all moves
            std::vector<Rotations> legal_rotations = GetLegalRotations(cube);
            for (Rotations rotation : legal_rotations) {
                Cube next_cube = Rotate(cube, rotation);
                std::pair<unsigned int, uint64_t> next_hash = {next_cube.GetCornerHash(), next_cube.GetEdgeHash()};

                // check if the position is not already searched
                if (tablebase[i+1].contains(next_hash) || tablebase[i].contains(next_hash) || (i > 0 && tablebase[i-1].contains(next_hash))) {
                    continue;
                }

                tablebase[i+1].insert(next_hash);

                if (i != depth-1) {
                    next.push(next_hash);
                }
            }
        }

        current = next;
    }
    error_handler.Handle(ErrorHandler::Level::kInfo, "search.cpp", "table base initialized");
}
