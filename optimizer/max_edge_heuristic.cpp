#include <bits/stdc++.h>
#include <chrono>
#include <cstdint>
#include <vector>

constexpr uint64_t Factorial(int n) {
    return n <= 1 ? 1 : n * Factorial(n-1);
}

constexpr uint64_t kTotalNumEdgePositions = Factorial(12);
constexpr uint8_t kNumRotations = 18;
constexpr int kNumEdges = 12;
constexpr int kNumEdgeOrientation = 2048;  // 2^11
constexpr int kEdgeOrientationSize = kNumEdgeOrientation * kNumRotations;  // 2^11 * 18

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


uint32_t PositionToHash(const std::array<uint8_t, kNumEdges>& positions) {
    uint32_t hash = 0;
    std::array<bool, kNumEdges> visited;
    visited.fill(false);

    for (int i = 0; i < kNumEdges; i++) {
        hash *= kNumEdges - i;
        int idx = 0;
        for (int j = 0; j < positions[i]; j++) {
            idx += int(!visited[j]);
        }
        visited[positions[i]] = true;
        hash += idx;
    }
    return hash;
}

std::array<uint8_t, kNumEdges> Rotate(std::array<uint8_t, kNumEdges> positions, int rotation) {
    for (uint8_t& position : positions) {
        if (kEdgeRotation[rotation][position] == -1) {
            continue;
        }
        position = kEdgeRotation[rotation][position];
    }
    return positions;
}

// get memory in MB
void PrintMemory() {
    size_t t_size = 0;
    size_t resident = 0;
    std::ifstream buffer("/proc/self/statm");
    buffer >> t_size >> resident;
    buffer.close();

    long page_size = sysconf(_SC_PAGE_SIZE);
    std::cout << "VIRT: " << t_size * page_size / (1024 * 1024) << " MB \t\tRES: " << resident * page_size / (1024 * 1024) << " MB\n";
};


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


std::vector<uint16_t> EdgeOrientationInitialization() {
    std::vector<uint16_t> edge_orientation(kEdgeOrientationSize, 0);
    std::vector<bool> visited(kNumEdgeOrientation, false);

    uint16_t start_orientation = 0;
    std::queue<uint16_t> next_queue;
    next_queue.push(start_orientation);
    visited[0] = true;

    while (!next_queue.empty()) {
        uint16_t orientations = next_queue.front();
        next_queue.pop();
        int old_hash = OrientationToHash(orientations);

        for (uint8_t rotation = 0; rotation < kNumRotations; rotation++) {
            uint16_t rotated = Rotate(orientations, rotation);
            int hash = OrientationToHash(rotated);

            edge_orientation[(old_hash*kNumRotations) + rotation] = hash;
            if (visited[hash]) {
                continue;
            }
            visited[hash] = true;
            next_queue.push(rotated);
        }
    }
    return edge_orientation;
}


template<class T>
void DoNotOptimize(T const& val) {
    asm volatile("" : : "r,m"(val) : "memory");
}


void FullEdge(std::vector<std::array<uint32_t, kNumRotations>>& sorted_edge_rotations, std::vector<uint16_t>& edge_orientation) {
    auto start_time = std::chrono::system_clock::now();

    // TODO: constexpr uint64_t fullEdgeSize = kTotalNumEdgePositions * kNumEdgeOrientation / 32;
    constexpr uint64_t kFullEdgeSize = kTotalNumEdgePositions / 32;
    std::vector<uint32_t> full_edge_visited(kFullEdgeSize);
    std::vector<uint32_t> full_edge_to_visit(kFullEdgeSize);
    full_edge_to_visit[0] |= 1; // starting position
    uint64_t changed = 1;

    for (int depth = 0; changed != 0; depth++) {
        changed = 0;
        for (uint64_t i = 0; i < kFullEdgeSize; i++) {
            // no rotation
            if (((full_edge_to_visit[i] ^ full_edge_visited[i]) & full_edge_to_visit[i]) == 0) {
                continue;
            }
            for (int j = 0; j < 32; j++) {
                // not this rotation
                if (((((full_edge_to_visit[i] ^ full_edge_visited[i]) & full_edge_to_visit[i])>>j) & 1) == 0) {
                    continue;
                }
                changed++;
                uint64_t index = (i * 32) + j;
                // TODO: uint32_t rotation_idx = index >> 11;
                uint32_t position_idx = index;
                uint32_t orientation_idx = index & ((1<<11)-1);
                for (uint8_t rotation = 0; rotation < kNumRotations; rotation++) {
                    uint64_t next_orientation = edge_orientation[(orientation_idx*kNumRotations)+rotation];
                    DoNotOptimize(next_orientation);
                    uint64_t next_position = sorted_edge_rotations[position_idx][rotation];
                    // TODO: uint64_t next_index = (next_position << 11) | next_orientation
                    uint64_t next_index = next_position;

                    // already visited
                    if ((full_edge_to_visit[next_index/32] >> (next_index%32) & 1) == 1) {
                        continue;
                    }
                    // add to visited (which becomes the new to_visit
                    full_edge_visited[next_index/32] |= 1 << (next_index%32);
                }
            }
        }
        std::swap(full_edge_to_visit, full_edge_visited);
        for (uint64_t i = 0; i < kFullEdgeSize; i++) {
            full_edge_to_visit[i] |= full_edge_visited[i];
        }
        std::cout << "depth " << depth << " cnt " << changed << "\n";
        auto cur_time = std::chrono::system_clock::now();
        std::cout << "depth" << depth << " time: " << std::chrono::duration_cast<std::chrono::seconds>(cur_time - start_time).count() << " s\n";
        start_time = cur_time;
        PrintMemory();
    }
}


int main() {
    auto start_time = std::chrono::system_clock::now();
    std::vector<uint16_t> edge_orientation = EdgeOrientationInitialization();
    auto edge_orientation_time = std::chrono::system_clock::now();
    std::cout << "edge_orientation time: " << std::chrono::duration_cast<std::chrono::seconds>(edge_orientation_time - start_time).count() << " s\n";
    PrintMemory();

    std::vector<std::array<uint32_t, kNumRotations>> rotations(kTotalNumEdgePositions);
    std::vector<uint32_t> sorted_index(kTotalNumEdgePositions, uint32_t(-1));
    uint32_t idx = 0;
    std::vector<uint64_t> distribution;
    distribution.push_back(1);
    sorted_index[0] = idx++; // starting position still has index 0
    std::vector<std::array<uint8_t, kNumEdges>> current_level = {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}};
    std::vector<std::array<uint8_t, kNumEdges>> next_level;
    auto init_time = std::chrono::system_clock::now();
    std::cout << "init time: " << std::chrono::duration_cast<std::chrono::seconds>(init_time - edge_orientation_time).count() << " s\n";
    PrintMemory();

    while (!current_level.empty()) {
        distribution.push_back(0);
        for (std::array<uint8_t, kNumEdges>& position : current_level) {
            for (uint8_t rotation = 0; rotation < kNumRotations; rotation++) {
                std::array<uint8_t, kNumEdges> next_position = Rotate(position, rotation);
                uint32_t position_hash = PositionToHash(position);
                uint32_t next_position_hash = PositionToHash(next_position);

                rotations[position_hash][rotation] = next_position_hash;
                // get next position
                if (sorted_index[next_position_hash] != uint32_t(-1)) {
                    continue;
                }
                sorted_index[next_position_hash] = idx++;
                distribution.back()++;
                next_level.push_back(next_position);
            }
        }
        std::swap(current_level, next_level);
        std::vector<std::array<uint8_t, kNumEdges>>().swap(next_level);
    }

    for (int i = 0; i < distribution.size()-1; i++) {
        std::cout << "depth " << i << " " << distribution[i] << "\n";
    }
    auto rotation_time = std::chrono::system_clock::now();
    std::cout << "rotation time: " << std::chrono::duration_cast<std::chrono::seconds>(rotation_time - init_time).count() << " s\n";
    PrintMemory();

    std::vector<std::array<uint32_t, kNumRotations>> sorted_edge_rotations(kTotalNumEdgePositions);
    for (uint64_t i = 0; i < kTotalNumEdgePositions; i++) {
        std::array<uint32_t, kNumRotations> converted_rotations;
        for (uint8_t rotation = 0; rotation < kNumRotations; rotation++) {
            converted_rotations[rotation] = sorted_index[rotations[i][rotation]];
        }
        sorted_edge_rotations[sorted_index[i]] = converted_rotations;
    }

    auto sorted_edge_time = std::chrono::system_clock::now();
    std::cout << "sorted_edge time: " << std::chrono::duration_cast<std::chrono::seconds>(sorted_edge_time - rotation_time).count() << " s\n";
    PrintMemory();

    // reset all data except for sorted_rotations
    std::vector<std::array<uint32_t, kNumRotations>>().swap(rotations);
    std::vector<uint32_t>().swap(sorted_index);
    std::vector<uint64_t>().swap(distribution);
    std::vector<std::array<uint8_t, kNumEdges>>().swap(current_level);
    std::vector<std::array<uint8_t, kNumEdges>>().swap(next_level);

    auto clear_edge_time = std::chrono::system_clock::now();
    std::cout << "clear_edge time: " << std::chrono::duration_cast<std::chrono::seconds>(clear_edge_time - sorted_edge_time).count() << " s\n";
    PrintMemory();

    FullEdge(sorted_edge_rotations, edge_orientation);
}
