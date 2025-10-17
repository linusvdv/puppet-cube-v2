#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <thread>
#include <vector>
#include <parallel_hashmap/phmap.h>

#include "corner_orientation.hpp"
#include "cube.hpp"
#include "logger.hpp"
#include "settings.hpp"


constexpr int kSizeLegalMap = 256;
constexpr int kNumLegalCornerConfigurations = 11382336;


struct Corners {
    uint16_t orientation;
    uint16_t position;
    std::array<uint8_t, kNumCorners> protruding;

    bool operator==(const Corners& other) const {
        return std::tie(orientation, position, protruding) ==
               std::tie(other.orientation, other.position, other.protruding);
    }

    friend std::size_t hash_value(const Corners& c) { // NOLINT
        constexpr int kMagicVal = 0x9e3779b9;
        std::size_t h_1 = std::hash<uint16_t>{}(c.orientation);
        std::size_t h_2 = std::hash<uint16_t>{}(c.position);

        // Hash the array
        std::size_t h_3 = 0;
        for (uint8_t val : c.protruding) {
            h_3 ^= std::hash<uint8_t>{}(val) + kMagicVal + (h_3 << 6) + (h_3 >> 2); // NOLINT
        }

        // Combine all hashes
        std::size_t seed = h_1;
        seed ^= h_2 + kMagicVal + (seed << 6) + (seed >> 2); // NOLINT
        seed ^= h_3 + kMagicVal + (seed << 6) + (seed >> 2); // NOLINT

        return seed;
    }
};

using ParallelCorners = phmap::parallel_flat_hash_set<Corners,
    phmap::priv::hash_default_hash<Corners>,
    phmap::priv::hash_default_eq<Corners>,
    phmap::priv::Allocator<Corners>,
    12, std::mutex>; // NOLINT


// pre initialise legal map
void LegalMapInitialisation (std::array<bool, kSizeLegalMap>& legal_map) {
    for (int i = 0; i < kSizeLegalMap; i++) {
        if ((!bool(i >> 0 & 1) && !bool(i >> 4 & 1)) ||   // x                  NOLINT
            (!bool(i >> 2 & 1) && !bool(i >> 6 & 1)) ||   // x                  NOLINT
            (!bool(i >> 1 & 1) && !bool(i >> 3 & 1)) ||   // y                  NOLINT
            (!bool(i >> 5 & 1) && !bool(i >> 7 & 1)) ||   // y                  NOLINT
            (!bool(i >> 0 & 1) && !bool(i >> 1 & 1) &&    // diagonal           NOLINT
             !bool(i >> 6 & 1) && !bool(i >> 7 & 1)) ||   //                    NOLINT
            (!bool(i >> 2 & 1) && !bool(i >> 3 & 1) &&    // diagonal           NOLINT
             !bool(i >> 4 & 1) && !bool(i >> 5 & 1))) {   //                    NOLINT
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


void ParallelCornerHeuristic(const std::vector<uint16_t>& corner_orientation, const std::vector<uint16_t>& corner_position,
                             const std::array<bool, kSizeLegalMap>& legal_map, const ParallelCorners& last, const ParallelCorners& current, ParallelCorners& next,
                             std::vector<uint16_t>& corner_heuristic, std::atomic<int>& cnt, int depth, int thread_idx, int num_threads) {
    int corner_cnt = 0;
    for (const Corners& corners : current) {
        corner_cnt++;
        if (corner_cnt % num_threads != thread_idx) {
            continue;
        }

        uint16_t legal_moves = 0;
        for (int rotation = 0; rotation < kNumRotations; rotation++) {
            Corners next_corners = Rotate(corner_orientation, corner_position, corners, rotation);
            if (last.contains(next_corners) || current.contains(next_corners)) {
                if (rotation%4 <= 1 && rotation < 12) { // NOLINT
                    legal_moves |= 1 << ((rotation+1)/2);
                }
                continue;
            }
            if (!IsLegal(legal_map, next_corners.protruding)) {
                continue;
            }
            if (rotation%4 <= 1 && rotation < 12) { // NOLINT
                legal_moves |= 1 << ((rotation+1)/2);
            }

            next.insert(next_corners);
        }
        corner_heuristic[(corners.orientation*kNumCornerPositions) + corners.position] = depth;
        corner_heuristic[(corners.orientation*kNumCornerPositions) + corners.position] |= legal_moves << 8; // NOLINT
        int current_cnt = cnt++;
        if (current_cnt % (kNumLegalCornerConfigurations / 20) == 0) { // NOLINT
            LOG_EXTRA(SkipSpace(current_cnt / (kNumLegalCornerConfigurations / 100)), "%");
        }
    }
}


std::vector<uint16_t> CornerHeuristicInitialization(const std::vector<uint16_t>& corner_orientation, const std::vector<uint16_t>& corner_position) {
    std::vector<uint16_t> corner_heuristic(kNumCornerHeuristic, 0);

    std::array<bool, kSizeLegalMap> legal_map;
    LegalMapInitialisation(legal_map);

    Corners solved_state(0, 0, {{0b000, 0b001, 0b010, 0b011, 0b100, 0b101, 0b110, 0b111}});  // NOLINT

    std::atomic<int> cnt = 0;
    ParallelCorners last = {};
    ParallelCorners current = {solved_state};
    ParallelCorners next = {};
    int depth = 0;
    do {
        {
            std::vector<std::jthread> threads;
            for (int j = 0; j < Settings::GetNumThreads(); j++) {
                threads.push_back(std::jthread(
                    ParallelCornerHeuristic, std::ref(corner_orientation), std::ref(corner_position),
                    std::ref(legal_map), std::ref(last), std::ref(current), std::ref(next),
                    std::ref(corner_heuristic), std::ref(cnt), depth, j, Settings::GetNumThreads()
                ));
            }
        }

        depth++;
        std::swap(last, current);
        std::swap(current, next);
        next = {};
    } while (!current.empty());

    if (cnt != kNumLegalCornerConfigurations) {
        LOG_CRITICAL("Found", cnt, "number of legal conrer configurations instead of", kNumLegalCornerConfigurations);
    }
    LOG_EXTRA(cnt, "legal corner configurations");
    LOG_EXTRA("max depth:", depth-1);

    return corner_heuristic;
}
