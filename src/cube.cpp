#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include "BCHTSet.hpp"
#include "corner_heuristic.hpp"
#include "corner_orientation.hpp"
#include "corner_position.hpp"
#include "cube.hpp"
#include "edge_heuristic.hpp"
#include "edge_orientation.hpp"
#include "edge_position.hpp"
#include "logger.hpp"
#include "settings.hpp"
#include "tablebase.hpp"


std::vector<uint16_t> Cube::corner_orientations;
std::vector<uint16_t> Cube::corner_positions;
std::vector<uint16_t> Cube::corner_heuristics;

std::vector<uint16_t> Cube::edge_orientations;
std::vector<uint32_t> Cube::edge_positions;
std::vector<uint8_t> Cube::edge_heuristics;

std::vector<Cube::Tablebase> Cube::tablebase;


std::string GetFilePath (std::string file_name) {
    file_name.insert(0, Settings::root_path);
    return file_name;
}


template<typename T, typename Generator>
void LoadOrGenerate(const std::string file_name, std::vector<T>& target, size_t expected_size,
                    Generator&& generate_func, const std::string& step_tag) {
    const std::string path = GetFilePath(file_name);
    if (std::FILE* file = std::fopen(path.c_str(), "rb")) {
        target.resize(expected_size);
        if (std::fread(target.data(), sizeof(T), expected_size, file) == expected_size) {
            LOG_ALL(step_tag, "read from file");
            LOG_MEMORY();
        }
        else {
            LOG_CRITICAL(step_tag, "was not able to read file", path);
        }
        std::fclose(file);
    }
    else {
        LOG_ALL(step_tag, "precompute ...");
        target = generate_func();
        LOG_MEMORY();

        if (target.size() != expected_size) {
            LOG_CRITICAL(step_tag, "Wrong precomputation size:", target.size(), "/", expected_size);
        }

        if (std::FILE* file = std::fopen(path.c_str(), "wb")) {
            if (fwrite(target.data(), sizeof(T), expected_size, file) != expected_size) {
                LOG_ERROR(step_tag, "failed to write full file");
            }
            std::fclose(file);
        }
        else {
            LOG_ERROR(step_tag, "not able to save precomputation to file");
        }
    }
}


void Cube::Initialize() {
    LoadOrGenerate("corner_orientations.bin", corner_orientations, kCornerOrientationSize,
        [](){return CornerOrientationInitialization();}, "[1/6] Corner Orientations");

    LoadOrGenerate("corner_positions.bin", corner_positions, kCornerPositionsSize,
        [](){return CornerPositionInitialization();}, "[2/6] Corner Positions");


    LoadOrGenerate("corner_heuristics.bin", corner_heuristics, kNumCornerHeuristic,
        [](){return CornerHeuristicInitialization(corner_orientations, corner_positions);}, "[3/6] Corner Heuristics");

    LoadOrGenerate("edge_orientations.bin", edge_orientations, kEdgeOrientationSize,
        [](){return EdgeOrientationInitialization();}, "[4/6] Edge Orientations");

    LoadOrGenerate("edge_positions.bin", edge_positions, kEdgePositionsSize,
        [](){return EdgePositionInitialization();}, "[5/6] Edge Positions");

    LoadOrGenerate("edge_heuristics.bin", edge_heuristics, kNumEdgeHeuristic,
        [](){return EdgeHeuristicInitialization(edge_orientations, edge_positions, 0, 0);}, "[6/6] Edge Heuristics");
}


void Cube::TablebaseInitialization() {
    tablebase = {{}};
    Cube::Tablebase starting_set;
    starting_set.insert(State(0, 0, 0, 0, kNumEdgePositions-1));
    tablebase.push_back(starting_set);
    for (int i = 1; i <= 8; i++) {
        tablebase.push_back({});
        // start multiple threads
        {
            std::vector<std::jthread> threads;
            for (int j = 0; j < Settings::num_threads; j++) {
                threads.push_back(std::jthread(TablebasePrecomputation, std::ref(tablebase[i-1]), std::ref(tablebase[i]), std::ref(tablebase[i+1]), j, Settings::num_threads));
            }
        }
        LOG_ALL("Tablebase depth", i, ":", tablebase.back().size());
    }
    LOG_MEMORY();

    // only second last tb
    std::vector<State> bcht_tb = BuildBCHTSet(tablebase[6]);

    std::vector<State> random_positions;
    for (const State& state : tablebase[6]) {
        random_positions.push_back(state);
    }
    for (const State& state : tablebase[7]) {
        random_positions.push_back(state);
    }

    for (int i = 0; i < random_positions.size(); i++) {
        std::swap(random_positions[i], random_positions[rand()%random_positions.size()]);
    }


    // Time them
    LOG_ALL("Start timing of phmap");
    namespace sc = std::chrono;

    auto phmap_time = sc::system_clock::now(); // get the current time
 
    size_t phmap_hit = 0;
    size_t phmap_miss = 0;
    for (const State& state : random_positions) {
        if (tablebase[7].contains(state)) {
            phmap_hit++;
        }
        else {
            phmap_miss++;
        }
    }

    auto phmap_since_epoch = phmap_time.time_since_epoch(); // get the duration since epoch

    // I don't know what system_clock returns
    // I think it's uint64_t nanoseconds since epoch
    // Either way this duration_cast will do the right thing
    auto phmap_millis = sc::duration_cast<sc::milliseconds>(phmap_since_epoch);

    LOG_ALL("hits:", phmap_hit, "miss:", phmap_miss);
    LOG_ALL("Time duration for phmap:", phmap_millis.count());

    LOG_ALL("Start timing of BCHT");
    auto BCHT_time = sc::system_clock::now(); // get the current time
 
    size_t BCHT_hit = 0;
    size_t BCHT_miss = 0;
    for (const State& state : random_positions) {
        if (BCHTSetContains(bcht_tb, state)) {
            BCHT_hit++;
        }
        else {
            BCHT_miss++;
        }
    }

    auto BCHT_since_epoch = BCHT_time.time_since_epoch(); // get the duration since epoch

    // I don't know what system_clock returns
    // I think it's uint64_t nanoseconds since epoch
    // Either way this duration_cast will do the right thing
    auto BCHT_millis = sc::duration_cast<sc::milliseconds>(BCHT_since_epoch);

    LOG_ALL("hits:", BCHT_hit, "miss:", BCHT_miss);
    LOG_ALL("Time duration for BCHT:", BCHT_millis.count());
}



Cube::Cube() {
    cube_ = State(0, 0, 0, 0, kNumEdgePositions-1);
}


constexpr std::array<uint8_t, kNumRotations> kLegalMoveIndex = {
    8, 9, 8, 9, 10, 11, 10, 11, 12, 13, 12, 13, 0, 0, 0, 0, 0, 0
};


bool Cube::State::Rotate(uint8_t rotation) {
    if (kLegalMoveIndex[rotation] != 0 && ((corner_heuristics[(corner_orientation*kNumCornerPositions) + corner_position] >> kLegalMoveIndex[rotation]) & 1) == 0) {
        return false;
    }
    corner_orientation = corner_orientations[(corner_orientation*kNumRotations) + rotation];
    corner_position = corner_positions[(corner_position*kNumRotations) + rotation];
    edge_orientation = edge_orientations[(edge_orientation*kNumRotations) + rotation];
    edge_position_1 = edge_positions[(edge_position_1*kNumRotations) + rotation];
    edge_position_2 = edge_positions[(edge_position_2*kNumRotations) + rotation];
    return true;
}

void Cube::UploadComputationToDevice() {
    #ifdef USE_CUDA
    
    #endif // USE_CUDA
}
