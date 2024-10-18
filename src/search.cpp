#include <atomic>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <parallel_hashmap/phmap.h>
#include <nadeau.h>
#include <concurrentqueue.h>


#include "actions.h"
#include "cube.h"
#include "error_handler.h"
#include "parallel_hashmap/phmap_fwd_decl.h"
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

    // this is a value from 0 to 4 describing how often this position is been visited
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


using VisitedMap = phmap::parallel_flat_hash_map<CubeMapVisited, std::pair<uint8_t, Rotations>,
            phmap::priv::hash_default_hash<CubeMapVisited>, phmap::priv::hash_default_eq<CubeMapVisited>,
            phmap::priv::Allocator<std::pair<CubeMapVisited, std::pair<uint8_t, Rotations>>>,
            8, std::mutex>;

using SearchQueue = std::vector<moodycamel::ConcurrentQueue<CubeSearch>>;


void ShowMemory (ErrorHandler error_handler, VisitedMap& visited) {
    const int indent = 8;
    std::stringstream out;
    out << "\n";
    out << std::setw(indent) << "" << "Map: " << 11 * visited.size() << " = 11 * " << visited.size() << " = " << 11 * visited.size() / 1000000 << " MB" << std::endl; // NOLINT
    out << std::setw(indent) << "" << "Map capacity: " << 11 * visited.capacity() << " = 11 * " << visited.capacity() << " = " << 11 * visited.capacity() / 1000000 << " MB" << std::endl; // NOLINT
    out << std::setw(indent) << "" << "current: " << getCurrentRSS() << " = " << getCurrentRSS() / 1000000 << " MB" << std::endl; // NOLINT
    out << std::setw(indent) << "" << "peak: " << getPeakRSS() << " = " << getPeakRSS() / 1000000 << " MB"; // NOLINT
    error_handler.Handle(ErrorHandler::Level::kMemory, "search.cpp", out.str());
}


constexpr int kNotFoundSol = 1e9;
constexpr int kMaxPositions = 10000000;
constexpr int kNumThreads = 24;

void Search (ErrorHandler error_handler, VisitedMap& visited, SearchQueue& search_queue,
             std::atomic<int>& max_depth, std::mutex& max_depth_mutex, CubeSearch& tablebase_cube,
             std::atomic<uint64_t>& num_positions, std::atomic<uint64_t>& search_queue_size) {
    while (num_positions < kMaxPositions) {
        // get new position from priority_queue
        CubeSearch cube_search;
        bool found = false;
        for (moodycamel::ConcurrentQueue<CubeSearch>& queue : search_queue) {
            if(!queue.try_dequeue(cube_search)) {
                continue;
            }
            found = true;
            break;
        }
        if (!found) {
            // has searched through all positions
            if (search_queue_size == 0) {
                return;
            }
            continue;
        }

        ++num_positions;
        Cube cube = DecodeHash(cube_search.hash);

        // check if it is posible to solve the current cube im this amount of moves
        if (cube_search.depth + (std::max(std::max({cube.GetCornerHeuristic(), int(cube.GetEdgeHeuristic1()), int(cube.GetEdgeHeuristic2())}) - GetTablebaseDepth(), 0)) >= max_depth) {
            --search_queue_size;
            continue;
        }

        // cube in tablebase
        // if it exists a new shortest path exists
        if (TablebaseContainsOuter(cube.GetHash())) {
            std::lock_guard<std::mutex> guard(max_depth_mutex);
            // improved depth
            if (cube_search.depth < max_depth) {
                max_depth = cube_search.depth;
                tablebase_cube = cube_search;
                ShowMemory(error_handler, visited);
                error_handler.Handle(ErrorHandler::Level::kExtra, "search.cpp", "Found solution of depth " + std::to_string(cube_search.depth + GetTablebaseDepth()) + " visiting " + std::to_string(num_positions) + " positions");
            }
        }

        // searched a branch to depth 100
        if (cube_search.depth >= 100) {
            --search_queue_size;
            continue;
        }

        Cube::Hash cube_hash = cube.GetHash();
        // check if position has already been searched
        bool already_visited = false;
        auto already_visited_lamda = [&already_visited, cube_search](const VisitedMap::value_type& value) {already_visited = value.second.first < cube_search.depth;};
        visited.if_contains({cube_hash}, already_visited_lamda);
        if (already_visited) {
            --search_queue_size;
            continue;
        }

        // go over next moves
        for (Rotations rotation : GetLegalRotations(cube)) {
            Cube next_cube = Rotate(cube, rotation);

            // check if next position is not already in the tablebase
            Cube::Hash next_cube_hash = next_cube.GetHash();
            bool already_visited = false;
            auto already_visited_lamda = [&already_visited, cube_search](const VisitedMap::value_type& value) {already_visited = value.second.first <= cube_search.depth+1;};
            visited.if_contains({next_cube_hash}, already_visited_lamda);
            if (already_visited) {
                continue;
            }

            // add to search if the next cube is visited_times better than current cube
            CubeSearch next = GetCubeSearch(next_cube, cube_search.depth+1, 0);
            if (cube_search.visited_time==0 ? (next.heuristic <= cube_search.heuristic) : (next.heuristic == cube_search.heuristic)) {
                search_queue[next.heuristic].enqueue(next);
                visited.try_emplace_l({next_cube_hash}, [cube_search, rotation](VisitedMap::value_type& value){value.second = {cube_search.depth+1, rotation}; }, std::make_pair(cube_search.depth+1, rotation));
                ++search_queue_size;
            }
        }

        if (cube_search.visited_time < 4) {
            CubeSearch temp_cube_search = GetCubeSearch(cube, cube_search.depth, cube_search.visited_time+1);
            search_queue[temp_cube_search.heuristic].enqueue(temp_cube_search);
            ++search_queue_size;
        }
        --search_queue_size;
    }
}


bool Solve (ErrorHandler error_handler, Actions& actions, Cube start_cube, uint64_t& num_positions) {
    int tb_depth = TablebaseDepth(start_cube);
    if (tb_depth != -1) {
        TablebaseSolve(start_cube, actions, tb_depth+1, num_positions);
        return true;
    }


    // initialize starting position
    CubeSearch tablebase_cube;
    // TODO: num elements
    SearchQueue search_queue(140);
    CubeSearch start_cube_search = GetCubeSearch(start_cube, 0, 0);
    search_queue[start_cube_search.heuristic].enqueue(start_cube_search);
    std::atomic<uint64_t> search_queue_size = 1;

    VisitedMap visited;
    visited.insert({{start_cube.GetHash()}, {0, Rotations(-1)}});

    // best found depth
    std::atomic<int> max_depth = kNotFoundSol;
    std::mutex max_depth_mutex;
    std::atomic<uint64_t> num_positions_atomic = num_positions;
    
    // start multiple threads
    {
        std::vector<std::jthread> threads;
        for (int i = 0; i < kNumThreads; i++) {
            threads.push_back(std::jthread(Search, error_handler, std::ref(visited), std::ref(search_queue), std::ref(max_depth),
                    std::ref(max_depth_mutex), std::ref(tablebase_cube), std::ref(num_positions_atomic), std::ref(search_queue_size)));
        }
    }
    num_positions = num_positions_atomic;

    ShowMemory(error_handler, visited);
    if (max_depth != kNotFoundSol && num_positions < kMaxPositions) {
        error_handler.Handle(ErrorHandler::Level::kInfo, "search.cpp", "found optimal solution");
    }
    if (max_depth == kNotFoundSol) {
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
