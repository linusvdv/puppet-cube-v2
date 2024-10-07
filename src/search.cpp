#include <cstdint>
#include <queue>
#include <string>
#include <parallel_hashmap/phmap.h>


#include "actions.h"
#include "cube.h"
#include "error_handler.h"
#include "rotation.h"
#include "tablebase.h"


struct CubeMapVisited {
    unsigned int corner_hash;
    uint64_t edge_hash;

    bool operator==(const CubeMapVisited& position) const {
        return corner_hash == position.corner_hash && edge_hash == position.edge_hash;
    }

    friend size_t hash_value(const CubeMapVisited& position) { // NOLINT
        return phmap::HashState::combine(0, position.corner_hash, position.edge_hash);
    }
};


struct CubeSearch {
    int heuristic;
    unsigned int corner_hash;
    uint64_t edge_hash;

    int depth;

    static constexpr int kDepthWeight = 1;
    bool operator<(const CubeSearch& cube_search) const {
        // sort priority_queue smaller to larger
        if (kDepthWeight*heuristic+depth != kDepthWeight*cube_search.heuristic+cube_search.depth) {
            return kDepthWeight*heuristic+depth > kDepthWeight*cube_search.heuristic+cube_search.depth;
        }
        if (corner_hash != cube_search.corner_hash) {
            return corner_hash > cube_search.corner_hash;
        }
        return edge_hash > cube_search.edge_hash;
    }
};


CubeSearch GetCubeSearch (Cube& cube, int depth) {
    CubeSearch cube_search;
    cube_search.corner_hash = cube.GetCornerHash();
    cube_search.edge_hash = cube.GetEdgeHash();
    cube_search.heuristic = cube.GetHeuristicFunction() + cube.GetEdgeHeuristic1() + cube.GetEdgeHeuristic2();
    cube_search.depth = depth;
    return cube_search;
}


bool Search (ErrorHandler error_handler, phmap::flat_hash_map<CubeMapVisited, std::pair<int, Rotations>>& visited, Cube start_cube, CubeSearch& tablebase_cube, uint64_t& num_positions) {
    std::priority_queue<CubeSearch> search_queue;
    search_queue.push(GetCubeSearch(start_cube, 0));
    visited.insert({{start_cube.GetCornerHash(), start_cube.GetEdgeHash()}, {0, Rotations(-1)}});

    const int not_found_sol = 1e9;
    int max_depth = not_found_sol;
    while (!search_queue.empty()) {
        CubeSearch cube_search = search_queue.top();
        search_queue.pop();
        num_positions++;

        // search next cubes
        Cube cube = DecodeHash(cube_search.corner_hash, cube_search.edge_hash);
        if (cube_search.depth + (std::max(std::max({int(cube.GetHeuristicFunction()), int(cube.GetEdgeHeuristic1()), int(cube.GetEdgeHeuristic2())}) - GetTablebaseDepth(), 0)) >= max_depth) {
            continue;
        }

        // cube in tablebase
        if (TablebaseContainsOuter(cube_search.corner_hash, cube_search.edge_hash)) {
            max_depth = cube_search.depth;
            tablebase_cube = cube_search;
            error_handler.Handle(ErrorHandler::Level::kExtra, "search.cpp", "Found solution of depth " + std::to_string(cube_search.depth + GetTablebaseDepth()) + " visiting " + std::to_string(num_positions) + " positions");
        }

        if (num_positions >= 7000000 && max_depth != not_found_sol) {
            return true;
        }

        auto visited_it = visited.find({cube_search.corner_hash, cube_search.edge_hash});
        if (visited_it != visited.end() && visited_it->second.first < cube_search.depth) {
            continue;
        }

        for (Rotations rotation : GetLegalRotations(cube)) {
            Cube next_cube = Rotate(cube, rotation);
            auto visited_it = visited.find({next_cube.GetCornerHash(), next_cube.GetEdgeHash()});
            if (visited_it != visited.end() && visited_it->second.first <= cube_search.depth+1) {
                continue;
            }

            search_queue.push(GetCubeSearch(next_cube, cube_search.depth+1));
            visited[{next_cube.GetCornerHash(), next_cube.GetEdgeHash()}] = {cube_search.depth+1, rotation};
        }
    }

    // this does at the moment not happen
    return false;
}


bool Solve (ErrorHandler error_handler, Actions& actions, Cube start_cube, uint64_t& num_positions) {
    phmap::flat_hash_map<CubeMapVisited, std::pair<int, Rotations>> visited;

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

    Cube cube = DecodeHash(tablebase_cube.corner_hash, tablebase_cube.edge_hash);
    TablebaseSolve(cube, actions, TablebaseDepth(cube)+1, num_positions);

    while (true) {
        Rotations rotation = visited[{cube.GetCornerHash(), cube.GetEdgeHash()}].second;
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
