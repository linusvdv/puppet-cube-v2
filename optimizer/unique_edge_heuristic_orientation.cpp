#include <array>
#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <vector>


int main() {
    const std::string path = "../precomputation/edge_heuristic.bin";

    // Reusing your constants
    constexpr int kNumOrientations = 2048;
    constexpr int kNumPositions = 9985968;

    std::vector<std::array<uint64_t, kNumOrientations/16>> heuristic(kNumPositions); // 10 GB
    std::cout << "Loading file..." << std::endl;
    if (std::FILE* file = std::fopen(path.c_str(), "rb")) {
        if (std::fread(heuristic.data(), sizeof(heuristic[0]), kNumPositions, file) != kNumPositions) {
            std::cerr << "Failed to read full file!" << std::endl;
            return 1;
        }
        std::fclose(file);
    } else {
        std::cerr << "Could not open file: " << path << std::endl;
        return 1;
    }

    std::map<uint64_t, uint64_t> edge_heuristic_orientation_cnt;
    for (int i = 0; i < kNumPositions; i++) {
        for (int j = 0; j < kNumOrientations/16; j++) {
            edge_heuristic_orientation_cnt[heuristic[i][j]]++;
        }
        if (i % 10000 == 0) {
            std::cout << i << " / " << kNumPositions << " : " << edge_heuristic_orientation_cnt.size() << "\n";
        }
    }
    std::cout << "number of destinkt edge heuristic orientations:" << edge_heuristic_orientation_cnt.size() << "\n";
}
