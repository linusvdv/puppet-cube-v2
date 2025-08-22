#include <chrono>
#include <functional>
#include <thread>
#include <vector>

#include "BCHTSet.hpp"
#include "cube.hpp"
#include "logger.hpp"
#include "settings.hpp"
#include "tablebase.hpp"


std::vector<std::vector<Cube::State>> Tablebase::tablebase = {};


void TablebaseSearch (const std::vector<Cube::State>& previous, const std::vector<Cube::State>& current, TablebasePrecomputation& next, int thread_idx, int num_threads) {
    int count = 0;
    for (const Cube::State& position : current) {
        count++;
        if (count%num_threads != thread_idx) {
            continue;
        }
        if (position == Cube::State()) {
            continue;
        }
        for (uint8_t rotation = 0; rotation < kNumRotations; rotation++) {
            Cube::State next_position = position;
            if (!next_position.Rotate(rotation)) {
                continue;
            }

            if (BCHTSetContains(previous, next_position) || BCHTSetContains(current, next_position)) {
                continue;
            }
            next.insert(next_position);
        }
    }
}


std::vector<Cube::State> TimeTablebaseCPU(std::vector<std::vector<Cube::State>>& tablebase, TablebasePrecomputation& tablebase_precomputation) {
    // only time largest tb_depth

    LOG_ALL("Create test date for timing tablebase CPU");
    std::vector<Cube::State> random_positions;
    for (const Cube::State& state : tablebase[Settings::GetTBDepth()]) {
        if (state == Cube::State()) {
            continue;
        }
        random_positions.push_back(state);
    }
    for (const Cube::State& state : tablebase[Settings::GetTBDepth()-1]) {
        if (state == Cube::State()) {
            continue;
        }
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
        if (tablebase_precomputation.contains(state)) {
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
        if (BCHTSetContains(tablebase[Settings::GetTBDepth()], state)) {
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
    return random_positions;
}


void Tablebase::Initialize() {
    std::vector<Cube::State> empty_tb_pre = BuildBCHTSet(TablebasePrecomputation({}));
    std::vector<Cube::State> starting_position_tb = BuildBCHTSet(TablebasePrecomputation({Cube::State(0, 0, 0, 0, kNumEdgePositions-1)}));

    tablebase.reserve(Settings::GetTBDepth()+1);
    tablebase.push_back(starting_position_tb);
    std::pair<std::reference_wrapper<std::vector<Cube::State>>, std::reference_wrapper<std::vector<Cube::State>>> previous_tables = {empty_tb_pre, starting_position_tb};

    TablebasePrecomputation tablebase_layer;
    for (int i = 1; i <= Settings::GetTBDepth(); i++) {
        tablebase_layer.clear();
        // start multiple threads
        {
            std::vector<std::jthread> threads;
            for (int j = 0; j < Settings::GetNumThreads(); j++) {
                threads.push_back(std::jthread(TablebaseSearch, previous_tables.first, previous_tables.second, std::ref(tablebase_layer), j, Settings::GetNumThreads()));
            }
        }
        LOG_ALL("Tablebase depth", i, "precomuted:", tablebase_layer.size(), "positions");

        tablebase.push_back(BuildBCHTSet(tablebase_layer));
        std::swap(previous_tables.first, previous_tables.second);
        previous_tables.second = tablebase.back();
        LOG_MEMORY();
    }

    if (Settings::GetShouldPerformanceTest()) {
        std::vector<Cube::State> random_positions = TimeTablebaseCPU(tablebase, tablebase_layer);
    }
}
