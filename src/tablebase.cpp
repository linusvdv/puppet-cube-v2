#include <chrono>
#include <thread>
#include <vector>

#include "BCHTSet.hpp"
#include "cube.hpp"
#include "logger.hpp"
#include "settings.hpp"
#include "tablebase.hpp"



void Tablebase (const TablebasePrecomputation& previous, const TablebasePrecomputation& current, TablebasePrecomputation& next, int thread_idx, int num_threads) {
    int count = 0;
    for (const Cube::State& position : current) {
        count++;
        if (count%num_threads != thread_idx) {
            continue;
        }
        for (uint8_t rotation = 0; rotation < kNumRotations; rotation++) {
            Cube::State next_position = position;
            if (!next_position.Rotate(rotation)) {
                continue;
            }

            if (previous.contains(next_position) || current.contains(next_position)) {
                continue;
            }
            next.insert(next_position);
        }
    }
}


void TimeTablebaseCPU(std::vector<std::vector<Cube::State>>& tablebase, std::vector<TablebasePrecomputation>& tablebase_precomputation) {
    // only time largest tb_depth

    LOG_ALL("Create test date for timing tablebase CPU");
    std::vector<Cube::State> random_positions;
    for (const Cube::State& state : tablebase_precomputation[Settings::tb_depth]) {
        random_positions.push_back(state);
    }
    for (const Cube::State& state : tablebase_precomputation[Settings::tb_depth+1]) {
        random_positions.push_back(state);
    }

    for (size_t i = 0; i < random_positions.size(); i++) {
        std::swap(random_positions[i], random_positions[rand()%random_positions.size()]);
    }

    // Time phmap
    LOG_ALL("Start timing of phmap");
    std::chrono::time_point phmap_time = std::chrono::high_resolution_clock::now(); // get the current time
 
    size_t phmap_hit = 0;
    size_t phmap_miss = 0;
    for (const Cube::State& state : random_positions) {
        if (tablebase_precomputation[Settings::tb_depth].contains(state)) {
            phmap_hit++;
        }
        else {
            phmap_miss++;
        }
    }

    std::chrono::time_point phmap_since_epoch = std::chrono::high_resolution_clock::now(); // get the duration since epoch
    std::chrono::milliseconds phmap_millis = std::chrono::duration_cast<std::chrono::milliseconds>(phmap_since_epoch - phmap_time);

    LOG_ALL("hits:", phmap_hit, "miss:", phmap_miss);
    LOG_ALL("Time duration for phmap:", phmap_millis.count());

    LOG_ALL("Start timing of BCHT");
    std::chrono::time_point BCHT_time = std::chrono::high_resolution_clock::now(); // get the current time

    size_t BCHT_hit = 0;
    size_t BCHT_miss = 0;
    for (const Cube::State& state : random_positions) {
        if (BCHTSetContains(tablebase[Settings::tb_depth-1], state)) {
            BCHT_hit++;
        }
        else {
            BCHT_miss++;
        }
    }

    std::chrono::time_point BCHT_since_epoch = std::chrono::high_resolution_clock::now(); // get the current time 
    std::chrono::milliseconds BCHT_millis = std::chrono::duration_cast<std::chrono::milliseconds>(BCHT_since_epoch - BCHT_time);

    LOG_ALL("hits:", BCHT_hit, "miss:", BCHT_miss);
    LOG_ALL("Time duration for BCHT:", BCHT_millis.count());
    LOG_MEMORY();
}


std::vector<std::vector<Cube::State>> TablebaseInitialization() {
    std::vector<TablebasePrecomputation> tablebase_precomputation = {{}};
    TablebasePrecomputation starting_set;
    starting_set.insert(Cube::State(0, 0, 0, 0, kNumEdgePositions-1));
    tablebase_precomputation.push_back(starting_set);
    for (int i = 1; i <= Settings::tb_depth; i++) {
        tablebase_precomputation.push_back({});
        // start multiple threads
        {
            std::vector<std::jthread> threads;
            for (int j = 0; j < Settings::num_threads; j++) {
                threads.push_back(std::jthread(Tablebase, std::ref(tablebase_precomputation[i-1]), std::ref(tablebase_precomputation[i]), std::ref(tablebase_precomputation[i+1]), j, Settings::num_threads));
            }
        }
        LOG_ALL("Tablebase depth", i, ":", tablebase_precomputation.back().size());
    }
    LOG_MEMORY();

    // only second last tb
    std::vector<std::vector<Cube::State>> tablebase;
    for (TablebasePrecomputation tablebase_layer : tablebase_precomputation) {
        if (tablebase_layer.empty()) { // get rid of the first dummy element
            continue;
        }
        tablebase.push_back(BuildBCHTSet(tablebase_layer));
    }
    LOG_MEMORY();

    if (Settings::should_peformance_test) {
        TimeTablebaseCPU(tablebase, tablebase_precomputation);
    }

    return tablebase;
}
