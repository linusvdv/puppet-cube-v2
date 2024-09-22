#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <queue>
#include <string>
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


// check if the position is in the tablebase and return its depth
int TablebaseDepth (Cube& cube) {
    for (size_t i = 0; i < tablebase.size(); i++) {
        if (tablebase[i].contains({cube.GetCornerHash(), cube.GetEdgeHash()})) {
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


// iterative deepening depth first search with heuristic function
bool Search (ErrorHandler error_handler, Actions& actions, Cube& cube, int depth, int real_depth,
        uint64_t& num_positions, CubeHashMap& visited) {
    // check if this position has been already visited
    auto saved_depth = visited.find({cube.GetCornerHash(), cube.GetEdgeHash()});
    if (saved_depth != visited.end() && saved_depth->second >= real_depth) {
        return false;
    }

    num_positions++;

    // solved positions
    if (tablebase.back().contains({cube.GetCornerHash(), cube.GetEdgeHash()})){
        TablebaseSolve(cube, actions, tablebase.size(), num_positions);
        return true;
    }

    // stop if the solved position cannot be reached in the time
    // TODO: heuristic function
    if (real_depth - std::max(cube.GetHeuristicFunction() - int(tablebase.size()-1), 0) < 0 || actions.stop || depth <= 0) {
        return false;
    }

    visited[{cube.GetCornerHash(), cube.GetEdgeHash()}] = real_depth;

    int heuristic = cube.GetHeuristicFunction();
    // dfs
    std::vector<Rotations> legal_rotations = GetLegalRotations(cube);
    for (Rotations rotation : legal_rotations) {
        Cube next_cube = Rotate(cube, rotation);
        int new_heuristic = next_cube.GetHeuristicFunction();
        int decrease = 1;
        if (new_heuristic > heuristic) {
            decrease++;
        }
        if (Search(error_handler, actions, next_cube, depth-decrease, real_depth-1, num_positions, visited)) {
            actions.solve.push(rotation);
            return true;
        }
    }

    return false;
}


// this function will use a BFS to find all positions of specific depth
void TablebaseSearch (ErrorHandler error_handler, int depth) {
    std::queue<std::pair<unsigned int, uint64_t>> current;
    if (int(tablebase.size()-1) >= depth) {
        return;
    }

    // get duration time
    auto start_time = std::chrono::system_clock::now();
    error_handler.Handle(ErrorHandler::Level::kInfo, "search.cpp", "resize tablebase from depth " + std::to_string((tablebase.empty() ? 0 : tablebase.size()-1)) + " to " + std::to_string(depth));

    // solved position
    if (tablebase.empty()) {
        tablebase.push_back(phmap::flat_hash_set<std::pair<unsigned int, uint64_t>, PositionHash>());
        tablebase[0].insert({0, 0});
    }

    // search from the next depth
    for (int i = tablebase.size()-1; i < depth; i++) {
        tablebase.push_back(phmap::flat_hash_set<std::pair<unsigned int, uint64_t>, PositionHash>());

        // go over all positions of this depth
        for (const std::pair<unsigned int, uint64_t>& current : tablebase[i]) {
            Cube cube = DecodeHash(current.first, current.second);

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
            }
        }
    }

    // get duration time
    auto end_time = std::chrono::system_clock::now();
    std::chrono::duration<double> time_duration = end_time - start_time;

    error_handler.Handle(ErrorHandler::Level::kInfo, "search.cpp", "set tablebase size to: " + std::to_string(tablebase.size()-1) + " in " + std::to_string(time_duration.count()) + " seconds");
}


void SolveCorners (Actions& actions, Cube& cube) {
    int heuristic = cube.GetHeuristicFunction();
    for (int i = heuristic-1; i >= 0; i--) {
        std::vector<Rotations> legal_rotations = GetLegalRotations(cube);
        for (Rotations rotation : legal_rotations) {
            Cube next_cube = Rotate(cube, rotation);
            if (next_cube.GetHeuristicFunction() == i) {
                cube = next_cube;
                actions.solve.push(rotation);
                break;
            }
        }
    }
}


bool Solve (ErrorHandler error_handler, Actions& actions, Cube& cube, uint64_t& num_positions, int max_depth) {
    CubeHashMap visited;

    SolveCorners(actions, cube);

    for (int search_depth = tablebase.size()-1; search_depth <= max_depth; search_depth++) {
        if (actions.stop) {
            return false;
        }

        // calculate all start positions until specific depth
        int tablebase_depth = std::min((search_depth+1)/2, 7);
        TablebaseSearch(error_handler, tablebase_depth);

        // already exists in tablebase
        int tb_depth = TablebaseDepth(cube);
        if (tb_depth != -1) {
            TablebaseSolve(cube, actions, tb_depth+1, num_positions);
            return true;
        }

        if (Search(error_handler, actions, cube, search_depth - (tablebase.size()-1), search_depth, num_positions, visited)) {
            return true;
        }
    }

    error_handler.Handle(ErrorHandler::Level::kWarning, "search.cpp", "did not find a solution of current position within depth " + std::to_string(max_depth));
    actions.solve = std::stack<Rotations>();
    return false;
}
