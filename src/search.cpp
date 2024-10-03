#include <cstdint>
#include <iostream>
#include <queue>
#include <parallel_hashmap/phmap.h>


#include "actions.h"
#include "cube.h"
#include "rotation.h"


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
    cube_search.heuristic = cube.GetHeuristicFunction() + cube.GetEdgeHeuristic();
    cube_search.depth = depth;
    return cube_search;
}


void Search (Actions& actions, Cube start_cube, uint64_t& num_positions) {
    phmap::flat_hash_map<CubeMapVisited, std::pair<int, Rotations>> visited;

    std::priority_queue<CubeSearch> search_queue;
    search_queue.push(GetCubeSearch(start_cube, 0));
    visited.insert({{start_cube.GetCornerHash(), start_cube.GetEdgeHash()}, {0, Rotations(-1)}});

    while (!search_queue.empty()) {
        CubeSearch cube_search = search_queue.top();
        search_queue.pop();
        num_positions++;

        // solved cube
        if (num_positions >= 4000000) {
            break;
        }

        // search next cubes
        Cube cube = DecodeHash(cube_search.corner_hash, cube_search.edge_hash);
        for (Rotations rotation : GetLegalRotations(cube)) {
            Cube next_cube = Rotate(cube, rotation);
            if (visited.contains({next_cube.GetCornerHash(), next_cube.GetEdgeHash()})) {
                if (visited[{next_cube.GetCornerHash(), next_cube.GetEdgeHash()}].first <= cube_search.depth+1) {
                    continue;
                }
            }

            search_queue.push(GetCubeSearch(next_cube, cube_search.depth+1));
            visited[{next_cube.GetCornerHash(), next_cube.GetEdgeHash()}] = {cube_search.depth+1, rotation};
        }
    }

    Cube cube = Cube();
    while (true) {
        Rotations rotation = visited[{cube.GetCornerHash(), cube.GetEdgeHash()}].second;
        if (rotation == Rotations(-1)) {
            return;
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
