#include <cstdint>
#include <thread>
#include <vector>

#include "corner_heuristic.h"
#include "corner_orientation.h"
#include "corner_position.h"
#include "cube.h"
#include "edge_orientation.h"
#include "edge_position.h"
#include "logger.h"
#include "settings.h"
#include "tablebase.h"


std::vector<uint16_t> Cube::corner_orientations;
std::vector<uint16_t> Cube::corner_positions;
tbb::concurrent_vector<uint16_t> Cube::corner_heuristics;

std::vector<uint16_t> Cube::edge_orientations;
std::vector<uint32_t> Cube::edge_positions;
std::vector<Cube::Tablebase> Cube::tablebase;


void Cube::Initialize() {
    LOG_ALL("[1/7] Corner Orientation Initialization ...");
    corner_orientations = CornerOrientationInitialization();
    LOG_MEMORY();

    LOG_ALL("[2/7] Corner Position Initialization ...");
    corner_positions = CornerPositionInitialization();
    LOG_MEMORY();

    LOG_ALL("[3/7] Corner Heuristic Initialization ...");
    corner_heuristics = CornerHeuristicInitialization(corner_orientations, corner_positions);
    LOG_MEMORY();

    LOG_ALL("[4/7] Edge Orientation Initialization ...");
    edge_orientations = EdgeOrientationInitialization();
    LOG_MEMORY();

    LOG_ALL("[5/7] Edge Position Initialization ...");
    edge_positions = EdgePositionInitialization();
    LOG_MEMORY();
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
        LOG_ALL("Depth", i, ":", tablebase.back().size());
    }
    LOG_MEMORY();
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
