#include <array>
#include <cassert>
#include <cstdint>
#include <queue>
#include <vector>

#include "corner_orientation.h"
#include "cube.h"
#include "logger.h"


constexpr int kNumCornerOrientation = 2187;  // 3^7
constexpr int kNumCornerHeuristic = kNumCornerOrientation * kNumCornerPositions;
constexpr int kSizeLegalMap = 256;
constexpr int kNumLegalCornerConfigurations = 11382336;


struct Corners {
    uint16_t orientation;
    uint16_t position;
    std::array<uint8_t, kNumCorners> protruding;
};


// pre initialise legal map
void LegalMapInitialisation (std::array<bool, kSizeLegalMap>& legal_map) {
    for (int i = 0; i < kSizeLegalMap; i++) {
        if ((!bool(i >> 0 & 1) && !bool(i >> 4 & 1)) ||   // x
            (!bool(i >> 2 & 1) && !bool(i >> 6 & 1)) ||   // x
            (!bool(i >> 1 & 1) && !bool(i >> 3 & 1)) ||   // y
            (!bool(i >> 5 & 1) && !bool(i >> 7 & 1)) ||   // y
            (!bool(i >> 0 & 1) && !bool(i >> 1 & 1) &&    // diagonal
             !bool(i >> 6 & 1) && !bool(i >> 7 & 1)) ||
            (!bool(i >> 2 & 1) && !bool(i >> 3 & 1) &&    // diagonal
             !bool(i >> 4 & 1) && !bool(i >> 5 & 1))) {
            legal_map[i] = false;
        }
        else {
            legal_map[i] = true;
        }
    }
}


// convert four protruding pieces to a hash which can be looked at
int LegalHash (const std::array<uint8_t, 4>& protruding_pieces, int idx) {
    int hash = 0;
    for (uint8_t protruding_piece : protruding_pieces) {
        for (int i = 1; i <= 2; i++) {
            hash *= 2;
            hash += (protruding_piece >> ((idx+i)%3)) & 1;
        }
    }
    return hash;
}


bool IsLegal (const std::array<bool, kSizeLegalMap>& legal_map, const std::array<uint8_t, kNumCorners>& protruding) {
    // go over all directions
    for (int i = 0; i < 3; i++) {
        // positive or negative
        for (int j = 0; j < 2; j++) {
            // get protruding pieces
            std::array<uint8_t, 4> protruding_pieces;
            protruding_pieces.fill(kNumCorners-1);
            for (int k = 0; k < kNumCorners; k++) {
                if ((k >> i & 1) == j && (protruding[k] >> i & 1) == 1) {
                    int index = (k >> ((i+1)%3) & 1) + ((k >> ((i+2)%3) & 1)*2);
                    protruding_pieces[index] = protruding[k];
                }
            }
            if (!legal_map[LegalHash(protruding_pieces, i)]) {
                return false;
            }
        }
    }
    return true;
}


Corners Rotate (const std::vector<uint16_t>& corner_orientation, const std::vector<uint16_t>& corner_position, Corners corners, int rotation) {
    corners.orientation = corner_orientation[(corners.orientation*kNumRotations) + rotation];
    corners.position = corner_position[(corners.position*kNumRotations) + rotation];
    corners.protruding = OrientationRotate(corners.protruding, static_cast<Rotations>(rotation));
    return corners;
}


std::vector<uint16_t> CornerHeuristicInitialization(const std::vector<uint16_t>& corner_orientation, const std::vector<uint16_t>& corner_position) {
    std::vector<uint16_t> corner_heuristic(kNumCornerHeuristic, 0);

    std::array<bool, kSizeLegalMap> legal_map;
    LegalMapInitialisation(legal_map);

    Corners solved_state(0, 0, {{0b000, 0b001, 0b010, 0b011, 0b100, 0b101, 0b110, 0b111}});  // NOLINT

    std::queue<Corners> next_queue;
    next_queue.push(solved_state);
    int depth = 0;
    Corners depth_increase = solved_state;
    bool needs_depth_increase = false;

    int count = 1;
    int level_count = 1;

    while (!next_queue.empty()) {
        Corners current = next_queue.front();
        next_queue.pop();

        if (current.orientation == depth_increase.orientation && current.position == depth_increase.position) {
            LOG_EXTRA(depth, "level_count:", level_count);
            level_count = 0;
            depth++;
            needs_depth_increase = true;
        }

        uint16_t legal_moves = 0;
        for (int rotation = 0; rotation < kNumRotations; rotation++) {
            Corners next = Rotate(corner_orientation, corner_position, current, rotation);

            if ((next.orientation == 0 && next.position == 0) ||
                (corner_heuristic[(int(next.orientation)*kNumCornerPositions) + int(next.position)] != 0)) {
                if (rotation%2 == 0 && rotation < 12) {
                    legal_moves |= 1 << (rotation/2);
                }
                continue;
            }
            if (!IsLegal(legal_map, next.protruding)) {
                continue;
            }
            if (rotation%2 == 0 && rotation < 12) {
                legal_moves |= 1 << (rotation/2);
            }

            level_count++;
            count++;
            if (count % (kNumLegalCornerConfigurations / 20) == 0) {
                LOG_EXTRA(count / (kNumLegalCornerConfigurations / 100), "%");
            }

            corner_heuristic[(next.orientation*kNumCornerPositions) + next.position] = depth;
            next_queue.push(next);

            if (needs_depth_increase) {
                depth_increase = next;
                needs_depth_increase = false;
            }
        }
        corner_heuristic[(current.orientation*kNumCornerPositions) + current.position] |= legal_moves << 8;
    }

    if (count != kNumLegalCornerConfigurations) {
        LOG_CRITICAL("Found", count, "number of legal conrer configurations instead of", kNumLegalCornerConfigurations);
    }
    LOG_EXTRA(count, "legal corner configurations");
    LOG_EXTRA("max depth:", depth-1);

    return corner_heuristic;
}
