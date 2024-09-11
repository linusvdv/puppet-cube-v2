#include <cstdint>
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
    if (depth - std::max(cube.GetHeuristicFunction() - int(tablebase.size()-1), 0) <= 0 || actions.stop) {
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
void TablebaseSearch (ErrorHandler error_handler, int depth) {
    std::queue<std::pair<unsigned int, uint64_t>> current;
    if (int(tablebase.size()-1) >= depth) {
        return;
    }

    // get duration time
    auto start_time = std::chrono::system_clock::now();
    error_handler.Handle(ErrorHandler::Level::kInfo, "search.cpp", "resize tablebase from depth " + std::to_string(tablebase.size()-1) + " to " + std::to_string(depth));

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

    error_handler.Handle(ErrorHandler::Level::kInfo, "search.cpp", "set tablebase size to: " + std::to_string(tablebase.size()-1));
}


bool Solve (ErrorHandler error_handler, Actions& actions, Cube& cube, std::vector<int>& search_depths, std::vector<uint64_t>& all_num_positions, int max_depth = 1000) {
    // calculate all start positions until specific depth
    TablebaseInitialisation(error_handler, 1);


    for (int search_depth = 0; search_depth <= max_depth; search_depth++) {
        if (actions.stop) {
            break;
        }

        error_handler.Handle(ErrorHandler::Level::kAll, "search_manager.cpp",  "depth " + std::to_string(search_depth));
        uint64_t num_positions = 0;
        CubeHashMap visited;

        if (Search(error_handler, actions, cube, search_depth, num_positions, visited)) {
            error_handler.Handle(ErrorHandler::Level::kAll, "search_manager.cpp",  "Found solution of depth " + std::to_string(search_depth) + " visiting " + std::to_string(num_positions) + " positions");

            // statistic
            search_depths.push_back(search_depth);
            all_num_positions.push_back(num_positions);

            // show solution
            while (!actions.solve.empty()) {
                actions.Push(Action(Instructions::kRotation, actions.solve.top()));
                actions.solve.pop();
            }
            break;
        }
    }

    error_handler.Handle(ErrorHandler::Level::kWarning, "search.cpp", "did not find a solution of current position within depth " + std::to_string(max_depth));
}
