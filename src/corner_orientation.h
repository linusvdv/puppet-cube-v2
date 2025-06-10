#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "cube.h"


std::array<uint8_t, kNumCorners> OrientationRotate (const std::array<uint8_t, kNumCorners>& orientations, Rotations rotation);

std::vector<uint16_t> CornerOrientationInitialization ();
