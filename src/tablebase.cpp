#include <cstdint>
#include <iostream>
#include <queue>
#include <vector>
#include <parallel_hashmap/phmap.h>


#include "cube.h"
#include "actions.h"


struct PositionHash {
    // memory optimized representation of the cube
    Cube::Hash hash;

    bool operator==(const PositionHash& position) const {
        return hash.hash_1 == position.hash.hash_1 && hash.hash_2 == position.hash.hash_2;
    }

    friend size_t hash_value(const PositionHash& position) { // NOLINT
        return phmap::HashState::combine(0, position.hash.hash_1, position.hash.hash_2);
    }
};


// positions reached from the solved cube after a specific number of moves
std::vector<phmap::parallel_flat_hash_set<PositionHash>> tablebase;


// check if the cube is in the outermost tablebase;
bool TablebaseContainsOuter (Cube::Hash hash) {
    return tablebase.back().contains({hash});
}


// check if the position is in the tablebase and return its depth
int TablebaseDepth (Cube& cube) {
    for (size_t i = 0; i < tablebase.size(); i++) {
        if (tablebase[i].contains({cube.GetHash()})) {
            return i;
        }
    }
    return -1;
}


// fastest solve
bool TablebaseSolve (Cube& cube, Actions& actions, int depth, uint64_t& num_positions) {
    num_positions++;
    int tb_depth = TablebaseDepth(cube);
    if (tb_depth >= depth || tb_depth == -1) {
        return false;
    }
    if (tb_depth == 0) {
        return true;
    }

    // dfs
    std::vector<Rotations> legal_rotations = GetLegalRotations(cube);
    for (Rotations rotation : legal_rotations) {
        Cube next_cube = Rotate(cube, rotation);
        if (TablebaseSolve(next_cube, actions, depth-1, num_positions)) {
            actions.solve.push(rotation);
            return true;
        }
    }
    return false;
}


// this function will use a BFS to find all positions of specific depth
void TablebaseSearch (ErrorHandler error_handler, int depth) {
    if (int(tablebase.size()-1) >= depth) {
        return;
    }

    // get duration time
    auto start_time = std::chrono::system_clock::now();
    error_handler.Handle(ErrorHandler::Level::kInfo, "search.cpp", "resize tablebase from depth " + std::to_string((tablebase.empty() ? 0 : tablebase.size()-1)) + " to " + std::to_string(depth));

    // solved position
    if (tablebase.empty()) {
        tablebase.push_back(phmap::parallel_flat_hash_set<PositionHash>());
        tablebase[0].insert({0, 0});
    }

    // search from the next depth
    for (int i = tablebase.size()-1; i < depth; i++) {
        tablebase.push_back(phmap::parallel_flat_hash_set<PositionHash>());

        // go over all positions of this depth
        for (const PositionHash& current : tablebase[i]) {
            Cube cube = DecodeHash(current.hash);

            // do all moves
            std::vector<Rotations> legal_rotations = GetLegalRotations(cube);
            for (Rotations rotation : legal_rotations) {
                Cube next_cube = Rotate(cube, rotation);
                PositionHash next_hash = {next_cube.GetHash()};

                // check if the position is not already searched
                if (tablebase[i+1].contains(next_hash) || tablebase[i].contains(next_hash) || (i > 0 && tablebase[i-1].contains(next_hash))) {
                    continue;
                }

                tablebase[i+1].insert(next_hash);
            }
        }
    }

    // get duration time
    auto end_time = std::chrono::system_clock::now();
    std::chrono::duration<double> time_duration = end_time - start_time;

    error_handler.Handle(ErrorHandler::Level::kInfo, "search.cpp", "set tablebase size to: " + std::to_string(tablebase.size()-1) + " in " + std::to_string(time_duration.count()) + " seconds");
}


int GetTablebaseDepth () {
    return tablebase.size()-1;
}
