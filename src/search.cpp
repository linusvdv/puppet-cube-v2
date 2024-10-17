#include <cstdint>
#include <deque>
#include <iomanip>
#include <iostream>
#include <queue>
#include <sstream>
#include <string>
#include <parallel_hashmap/phmap.h>
#include <nadeau.h>


#include "actions.h"
#include "cube.h"
#include "error_handler.h"
#include "rotation.h"
#include "tablebase.h"


#pragma pack(push, 1)
struct CubeMapVisited {
    // memory optimized representation of the cube
    Cube::Hash hash;

    bool operator==(const CubeMapVisited& position) const {
        return hash.hash_1 == position.hash.hash_1 && hash.hash_2 == position.hash.hash_2;
    }

    friend size_t hash_value(const CubeMapVisited& position) { // NOLINT
        return phmap::HashState::combine(0, position.hash.hash_1, position.hash.hash_2);
    }
};
#pragma pack(pop)


#pragma pack(push, 1)
struct CubeSearch {
    // memory optimized representation of the cube
    Cube::Hash hash;

    uint8_t heuristic;
    uint8_t depth;

    // this is a value between 0 and 7 describing how often this position is been visited
    uint8_t visited_time;

    // sort priority_queue smaller to larger
    bool operator<(const CubeSearch& cube_search) const {
        if (heuristic != cube_search.heuristic) {
            return heuristic > cube_search.heuristic;
        }
        if (hash.hash_1 != cube_search.hash.hash_1) {
            return hash.hash_1 > cube_search.hash.hash_1;
        }
        return hash.hash_2 > cube_search.hash.hash_2;
    }
};
#pragma pack(pop)


CubeSearch GetCubeSearch (Cube& cube, uint8_t depth, uint8_t visited_time) {
    CubeSearch cube_search;
    cube_search.hash = cube.GetHash();
    cube_search.heuristic = cube.GetCornerHeuristic() + cube.GetEdgeHeuristic1() + cube.GetEdgeHeuristic2() + depth + visited_time;
    cube_search.depth = depth;
    cube_search.visited_time = visited_time;
    return cube_search;
}


void ShowMemory (ErrorHandler error_handler, phmap::parallel_flat_hash_map<CubeMapVisited, std::pair<uint8_t, Rotations>>& visited, std::priority_queue<CubeSearch, std::deque<CubeSearch>>& search_queue) {
    const int indent = 8;
    std::stringstream out;
    out << "\n";
    out << std::setw(indent) << "" << "PQ:  " << 12 * search_queue.size() << " = 12 * " << search_queue.size() << " = " << 12 * search_queue.size() / 1000000 << " MB" << std::endl;
    out << std::setw(indent) << "" << "Map: " << 11 * visited.size() << " = 11 * " << visited.size() << " = " << 11 * visited.size() / 1000000 << " MB" << std::endl;
    out << std::setw(indent) << "" << "Map capacity: " << 11 * visited.capacity() << " = 11 * " << visited.capacity() << " = " << 11 * visited.capacity() / 1000000 << " MB" << std::endl;
    out << std::setw(indent) << "" << "current: " << getCurrentRSS() << " = " << getCurrentRSS() / 1000000 << " MB" << std::endl;
    out << std::setw(indent) << "" << "peak: " << getPeakRSS() << " = " << getPeakRSS() / 1000000 << " MB";
    error_handler.Handle(ErrorHandler::Level::kMemory, "search.cpp", out.str());
}


bool Search (ErrorHandler error_handler, phmap::parallel_flat_hash_map<CubeMapVisited, std::pair<uint8_t, Rotations>>& visited, Cube start_cube, CubeSearch& tablebase_cube, uint64_t& num_positions) {
    // initialize starting position
    std::priority_queue<CubeSearch, std::deque<CubeSearch>> search_queue;
    search_queue.push(GetCubeSearch(start_cube, 0, 0));
    visited.insert({{start_cube.GetHash()}, {0, Rotations(-1)}});

    // best found depth
    const int not_found_sol = 1e9;
    int max_depth = not_found_sol;

    while (!search_queue.empty()) {
        // get new position from priority_queue
        CubeSearch cube_search = search_queue.top();
        search_queue.pop();
        num_positions++;
        Cube cube = DecodeHash(cube_search.hash);

        // check if it is posible to solve the current cube im this amount of moves
        if (cube_search.depth + (std::max(std::max({cube.GetCornerHeuristic(), int(cube.GetEdgeHeuristic1()), int(cube.GetEdgeHeuristic2())}) - GetTablebaseDepth(), 0)) >= max_depth) {
            continue;
        }

        // cube in tablebase
        // if it exists a new shortest path exists
        if (TablebaseContainsOuter(cube.GetHash())) {
            max_depth = cube_search.depth;
            tablebase_cube = cube_search;
            ShowMemory(error_handler, visited, search_queue);
            error_handler.Handle(ErrorHandler::Level::kExtra, "search.cpp", "Found solution of depth " + std::to_string(cube_search.depth + GetTablebaseDepth()) + " visiting " + std::to_string(num_positions) + " positions");
        }

        // finish search
        if (num_positions >= 100000000 && max_depth != not_found_sol) {
            ShowMemory(error_handler, visited, search_queue);
            return true;
        }

        // searched a branch to depth 100
        if (cube_search.depth >= 100) {
            continue;
        }

        // check ig position has already been searched
        auto visited_it = visited.find({cube.GetHash()});
        if (visited_it != visited.end() && visited_it->second.first < cube_search.depth) {
            continue;
        }

        // go over next moves
        for (Rotations rotation : GetLegalRotations(cube)) {
            Cube next_cube = Rotate(cube, rotation);

            // check if next position is not already in the tablebase
            auto visited_it = visited.find({next_cube.GetHash()});
            if (visited_it != visited.end() && visited_it->second.first <= cube_search.depth+1) {
                continue;
            }

            // add to search if the next cube is visited_times-2 better than current cube
            // -2 comes form best improvement possible in a position
            CubeSearch next = GetCubeSearch(next_cube, cube_search.depth+1, 0);
            if (cube_search.visited_time==0 ? (next.heuristic <= cube_search.heuristic) : (next.heuristic == cube_search.heuristic)) {
                search_queue.push(next);
                visited[{next_cube.GetHash()}] = {cube_search.depth+1, rotation};
            }
        }

        if (cube_search.visited_time < 4) {
            search_queue.push(GetCubeSearch(cube, cube_search.depth, cube_search.visited_time+1));
        }
    }

    if (max_depth != not_found_sol) {
        error_handler.Handle(ErrorHandler::Level::kInfo, "search.cpp", "found optimal solution");
        return true;
    }
    return false;
}


bool Solve (ErrorHandler error_handler, Actions& actions, Cube start_cube, uint64_t& num_positions) {
    phmap::parallel_flat_hash_map<CubeMapVisited, std::pair<uint8_t, Rotations>> visited;

    int tb_depth = TablebaseDepth(start_cube);
    if (tb_depth != -1) {
        TablebaseSolve(start_cube, actions, tb_depth+1, num_positions);
        return true;
    }

    CubeSearch tablebase_cube;
    if (!Search(error_handler, visited, start_cube, tablebase_cube, num_positions)) {
        error_handler.Handle(ErrorHandler::Level::kWarning, "search.cpp", "Did not find a solution within " + std::to_string(num_positions) + " positions");
        return false;
    }

    Cube cube = DecodeHash(tablebase_cube.hash);
    TablebaseSolve(cube, actions, TablebaseDepth(cube)+1, num_positions);

    while (true) {
        Rotations rotation = visited[{cube.GetHash()}].second;
        if (rotation == Rotations(-1)) {
            return true;
        }
        actions.solve.push(rotation);

        Rotations counter_rotation;
        if (rotation%2==0) {
            counter_rotation = Rotations(int(rotation)+1);
        }
        else {
            counter_rotation = Rotations(int(rotation)-1);
        }

        cube = Rotate(cube, counter_rotation);
    }
}
