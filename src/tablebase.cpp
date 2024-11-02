#include <cstdint>
#include <string>
#include <thread>
#include <vector>
#include <parallel_hashmap/phmap.h>


#include "cube.h"
#include "actions.h"
#include "error_handler.h"
#include "settings.h"


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
using Tablebase = phmap::parallel_flat_hash_set<PositionHash,
        phmap::priv::hash_default_hash<PositionHash>,
        phmap::priv::hash_default_eq<PositionHash>,
        phmap::priv::Allocator<PositionHash>,
        12, std::mutex>;
std::vector<Tablebase> tablebase;


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


void ParallelTablebaseIncrease(int depth, int thread_id, int num_threads) {
    // go over all positions of this depth
    auto current_it = tablebase[depth].begin();
    if (current_it == tablebase[depth].end()) {
        return;
    }

    for (int i = 0; i < thread_id; i++) {
        if (++current_it == tablebase[depth].end()) {
            return;
        }
    }

    while (true) {
        Cube cube = DecodeHash(current_it->hash);

        // do all moves
        std::vector<Rotations> legal_rotations = GetLegalRotations(cube);
        for (Rotations rotation : legal_rotations) {
            Cube next_cube = Rotate(cube, rotation);
            PositionHash next_hash = {next_cube.GetHash()};

            // check if the position is not already searched
            if (tablebase[depth+1].contains(next_hash) || tablebase[depth].contains(next_hash) || (depth > 0 && tablebase[depth-1].contains(next_hash))) {
                continue;
            }

            tablebase[depth+1].lazy_emplace_l(std::move(next_hash), []([[maybe_unused]]Tablebase::value_type& value){}, [next_hash](const Tablebase::constructor& ctor){ctor(std::move(next_hash));});
        }

        for (int i = 0; i < num_threads; i++) {
            if (++current_it == tablebase[depth].end()) {
                return;
            }
        }
    }
}


// this function will use a BFS to find all positions of specific depth
void TablebaseSearch (ErrorHandler error_handler, Setting& settings, int depth) {
    if (int(tablebase.size()-1) >= depth) {
        return;
    }

    // get duration time
    auto start_time = std::chrono::system_clock::now();
    error_handler.Handle(ErrorHandler::Level::kInfo, "search.cpp", "resize tablebase from depth " + std::to_string((tablebase.empty() ? 0 : tablebase.size()-1)) + " to " + std::to_string(depth));

    // solved position
    if (tablebase.empty()) {
        tablebase.push_back(Tablebase());
        tablebase[0].insert({0, 0});
    }

    // search from the next depth
    for (int i = tablebase.size()-1; i < depth; i++) {
        tablebase.push_back(Tablebase());
        // start multiple threads
        {
            std::vector<std::jthread> threads;
            for (int j = 0; j < settings.num_threads; j++) {
                threads.push_back(std::jthread(ParallelTablebaseIncrease, i, j, settings.num_threads));
            }
        }
        error_handler.Handle(ErrorHandler::Level::kExtra, "tablebase.cpp", "tablebase size depth " + std::to_string(i+1) + ": " + std::to_string(tablebase.back().size()));
    }

    // get duration time
    auto end_time = std::chrono::system_clock::now();
    std::chrono::duration<double> time_duration = end_time - start_time;

    error_handler.Handle(ErrorHandler::Level::kInfo, "search.cpp", "set tablebase size to: " + std::to_string(tablebase.size()-1) + " in " + std::to_string(time_duration.count()) + " seconds");
}


int GetTablebaseDepth () {
    return tablebase.size()-1;
}
