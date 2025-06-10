#include <cstdint>
#include <vector>

#include "corner_orientation.h"
#include "cube.h"
#include "logger.h"


std::vector<uint8_t> Cube::corner_orientation;


void Cube::Initialize() {
    LOG_ALL("[1/7] Corner Orientation Initialization ...");
    corner_orientation = CornerOrientationInitialization();
    LOG_MEMORY();
}
