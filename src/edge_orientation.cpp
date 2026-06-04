#include <array>
#include <cstdint>
#include <queue>
#include <vector>

#include "cube.hpp"
#include "logger.hpp"


// map the current position to next position
// -1 marks no change in rotation direction
constexpr std::array<std::array<int8_t, kNumEdges>, kNumRotations> kEdgeRotation =
{{
    { 2,  0,  3,  1, -1, -1, -1, -1, -1, -1, -1, -1}, // R
    { 1,  3,  0,  2, -1, -1, -1, -1, -1, -1, -1, -1}, // R'
    {-1, -1, -1, -1, -1, -1, -1, -1,  9, 11,  8, 10}, // L
    {-1, -1, -1, -1, -1, -1, -1, -1, 10,  8, 11,  9}, // L'
    { 4, -1, -1, -1,  8,  0, -1, -1,  5, -1, -1, -1}, // U
    { 5, -1, -1, -1,  0,  8, -1, -1,  4, -1, -1, -1}, // U'
    {-1, -1, -1,  7, -1, -1,  3, 11, -1, -1, -1,  6}, // D
    {-1, -1, -1,  6, -1, -1, 11,  3, -1, -1, -1,  7}, // D'
    {-1,  6, -1, -1,  1, -1,  9, -1, -1,  4, -1, -1}, // F
    {-1,  4, -1, -1,  9, -1,  1, -1, -1,  6, -1, -1}, // F'
    {-1, -1,  5, -1, -1, 10, -1,  2, -1, -1,  7, -1}, // B
    {-1, -1,  7, -1, -1,  2, -1, 10, -1, -1,  5, -1}, // B'
    { 2,  0,  3,  1, -1, -1, -1, -1, 10,  8, 11,  9}, // M  -  R  + L'
    { 1,  3,  0,  2, -1, -1, -1, -1,  9, 11,  8, 10}, // M' -  R' + L
    { 4, -1, -1,  6,  8,  0, 11,  3,  5, -1, -1,  7}, // E  -  U  + D'
    { 5, -1, -1,  7,  0,  8,  3, 11,  4, -1, -1,  6}, // E' -  U' + D
    {-1,  4,  5, -1,  9, 10,  1,  2, -1,  6,  7, -1}, // S  -  F' + B
    {-1,  6,  7, -1,  1,  2,  9, 10, -1,  4,  5, -1}, // S' -  F  + B'
}};


uint16_t OrientationToHash(uint16_t orientations) { // remove one bit
    return (orientations & ((uint16_t(1) << (kNumEdges-1))-1));
}


uint16_t Rotate(uint16_t old_orientations, uint8_t rotation) {
    uint16_t orientations = 0;
    for (int i = 0; i < kNumEdges; i++) {
        if (kEdgeRotation[rotation][i] == -1) {
            orientations |= ((old_orientations >> i) & 1) << i;
        }
        else {
            uint16_t invert = 0;
            if (rotation < 4 || rotation == 12 || rotation == 13) { // R, L, M      NOLINT
                invert = 1;
            }
            orientations |= (((old_orientations >> i) & 1) ^ invert) << kEdgeRotation[rotation][i];
        }
    }
    return orientations;
}


void EdgeOrientationInitialization(std::vector<uint16_t>& edge_orientation) {
    edge_orientation.assign(kEdgeOrientationSize, 0);
    std::vector<bool> visited(kNumEdgeOrientation, false);

    uint16_t start_orientation = 0;
    std::queue<uint16_t> next_queue;
    next_queue.push(start_orientation);
    visited[0] = true;
    int cnt = 1;

    while (!next_queue.empty()) {
        uint16_t orientations = next_queue.front();
        next_queue.pop();
        int old_hash = OrientationToHash(orientations);

        for (uint8_t rotation = 0; rotation < kNumRotations; rotation++) {
            uint16_t rotated = Rotate(orientations, rotation);
            int hash = OrientationToHash(rotated);

            if (hash >= kNumEdgeOrientation) {
                LOG_WARNING(hash, kNumEdgeOrientation);
                LOG_WARNING(rotated);
                LOG_CRITICAL("Calculated hash too big");
            }

            edge_orientation[(old_hash*kNumRotations) + rotation] = hash;
            if (visited[hash]) {
                continue;
            }
            visited[hash] = true;
            next_queue.push(rotated);
            cnt++;
        }
    }

    if (cnt != kNumEdgeOrientation) {
        LOG_WARNING(cnt, "/", kNumEdgeOrientation);
        LOG_CRITICAL("Didn't find all");
    }
}
