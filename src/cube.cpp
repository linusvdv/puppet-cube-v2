#include <cstdint>
#include <vector>

#include "corner_heuristic.h"
#include "corner_orientation.h"
#include "corner_position.h"
#include "cube.h"
#include "edge_orientation.h"
#include "logger.h"


std::vector<uint16_t> Cube::corner_orientation;
std::vector<uint16_t> Cube::corner_position;
std::vector<uint16_t> Cube::corner_heuristic;
std::vector<uint16_t> Cube::edge_orientation;


void Cube::Initialize() {
    LOG_ALL("[1/7] Corner Orientation Initialization ...");
    corner_orientation = CornerOrientationInitialization();
    LOG_MEMORY();

    LOG_ALL("[2/7] Corner Position Initialization ...");
    corner_position = CornerPositionInitialization();
    LOG_MEMORY();

    LOG_ALL("[3/7] Corner Heuristic Initialization ...");
    corner_heuristic = CornerHeuristicInitialization(corner_orientation, corner_position);
    LOG_MEMORY();

    LOG_ALL("[4/7] Edge Orientation Initialization ...");
    edge_orientation = EdgeOrientationInitialization();
    LOG_MEMORY();
}
