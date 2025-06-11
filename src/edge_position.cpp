#include <array>
#include <cstdint>
#include <queue>
#include <vector>

#include "cube.h"
#include "logger.h"


constexpr int kEdgePositionsSize = kNumEdgePositions * kNumRotations;  // 12! / 6! * 18
constexpr int kNumPieces = 6;


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


uint32_t PositionToHash(const std::array<uint8_t, kNumPieces>& positions) {
    uint32_t hash = 0;
    std::array<bool, kNumEdges> visited;
    visited.fill(false);

    for (int i = 0; i < kNumPieces; i++) {
        hash *= kNumEdges - i;
        int idx = 0;
        for (int j = 0; j < positions[i]; j++) {
            idx += int(!visited[j]);
        }
        visited[positions[i]] = true;
        hash += idx;
    }

    if (hash >= kNumEdgePositions) {
        LOG_CRITICAL("Calculated hash too big");
    }
    return hash;
}


std::array<uint8_t, kNumPieces> Rotate(std::array<uint8_t, kNumPieces> positions, int rotation) {
    for (uint8_t& position : positions) {
        if (kEdgeRotation[rotation][position] == -1) {
            continue;
        }
        position = kEdgeRotation[rotation][position];
    }
    return positions;
}


std::vector<uint32_t> EdgePositionInitialization() {
    std::vector<uint32_t> edge_positions(kEdgePositionsSize, 0);
    std::vector<bool> visited(kNumEdgePositions, false);

    std::array<uint8_t, kNumPieces> starting_positions = {0, 1, 2, 3, 4, 5};
    std::queue<std::array<uint8_t, kNumPieces>> next_queue;
    next_queue.push(starting_positions);
    visited[PositionToHash(starting_positions)] = true;
    int cnt = 1;

    while (!next_queue.empty()) {
        std::array<uint8_t, kNumPieces> positions = next_queue.front();
        next_queue.pop();
        uint32_t old_hash = PositionToHash(positions);

        for (uint8_t rotation = 0; rotation < kNumRotations; rotation++) {
            std::array<uint8_t, kNumPieces> rotated = Rotate(positions, rotation);
            uint32_t hash = PositionToHash(rotated);

            if (hash >= kEdgePositionsSize) {
                LOG_CRITICAL("Calculated hash too big");
            }
            edge_positions[(old_hash*kNumRotations) + rotation] = hash;
            if (visited[hash]) {
                continue;
            }
            visited[hash] = true;
            cnt++;
            next_queue.push(rotated);
        }
    }

    if (cnt != kNumEdgePositions) {
        LOG_CRITICAL("Did not find all positions", cnt);
    }
    return edge_positions;
}
