#include <cstdint>
#include <iostream>
#include <queue>
#include <string>
#include <parallel_hashmap/phmap.h>


#include "actions.h"
#include "cube.h"
#include "error_handler.h"
#include "rotation.h"
#include "tablebase.h"


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


struct CubeSearch {
    // memory optimized representation of the cube
    Cube::Hash hash;

    uint8_t heuristic;
    int8_t depth;

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


CubeSearch GetCubeSearch (Cube& cube, int8_t depth) {
    CubeSearch cube_search;
    cube_search.hash = cube.GetHash();
    cube_search.heuristic = cube.GetCornerHeuristic() + cube.GetEdgeHeuristic1() + cube.GetEdgeHeuristic2() + depth;
    cube_search.depth = depth;
    return cube_search;
}


bool Search (ErrorHandler error_handler, phmap::parallel_flat_hash_map<CubeMapVisited, std::pair<uint8_t, Rotations>>& visited, Cube start_cube, CubeSearch& tablebase_cube, uint64_t& num_positions) {
    std::priority_queue<CubeSearch> search_queue;
    search_queue.push(GetCubeSearch(start_cube, 0));
    visited.insert({{start_cube.GetHash()}, {0, Rotations(-1)}});

    const int not_found_sol = 1e9;
    int max_depth = not_found_sol;
    while (!search_queue.empty()) {
        CubeSearch cube_search = search_queue.top();
        search_queue.pop();
        num_positions++;

        // search next cubes
        Cube cube = DecodeHash(cube_search.hash);
        if (cube_search.depth + (std::max(std::max({int(cube.GetCornerHeuristic()), int(cube.GetEdgeHeuristic1()), int(cube.GetEdgeHeuristic2())}) - GetTablebaseDepth(), 0)) >= max_depth) {
            continue;
        }

        // cube in tablebase
        if (TablebaseContainsOuter(cube.GetHash())) {
            max_depth = cube_search.depth;
            tablebase_cube = cube_search;
            error_handler.Handle(ErrorHandler::Level::kExtra, "search.cpp", "Found solution of depth " + std::to_string(cube_search.depth + GetTablebaseDepth()) + " visiting " + std::to_string(num_positions) + " positions");
            std::cout << "PQ:  " << sizeof(CubeSearch) * search_queue.size() << std::endl;
            std::cout << "Map: " << (sizeof(CubeMapVisited) + sizeof(std::pair<uint8_t, Rotations>)) * visited.size() << std::endl;
        }

        if (num_positions >= 7000000 && max_depth != not_found_sol) {
            return true;
        }

        auto visited_it = visited.find({cube.GetHash()});
        if (visited_it != visited.end() && visited_it->second.first < cube_search.depth) {
            continue;
        }

        for (Rotations rotation : GetLegalRotations(cube)) {
            Cube next_cube = Rotate(cube, rotation);
            auto visited_it = visited.find({next_cube.GetHash()});
            if (visited_it != visited.end() && visited_it->second.first <= cube_search.depth+1) {
                continue;
            }

            search_queue.push(GetCubeSearch(next_cube, cube_search.depth+1));
            visited[{next_cube.GetHash()}] = {cube_search.depth+1, rotation};
        }
    }

    // this does at the moment not happen
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
