#include <bits/stdc++.h>
#include <random>


constexpr uint64_t Factorial(int n) {
    return n <= 1 ? 1 : n * Factorial(n-1);
}

constexpr int kNumSymmetry = Factorial(3) * 1<<3;
constexpr int kNumPieces = 12;
constexpr int kNumRotations = 18;
constexpr int kNumSymmetryPositions = 9985968; // 24 bits needed
constexpr uint32_t kSymmetryMask = (1<<24) - 1;

const std::vector<std::string> rotation_names = {
    "R", "R'", "L", "L'", "U", "U'", "D", "D'",
    "F", "F'", "B", "B'", "M", "M'", "E", "E'", "S", "S'"};



uint32_t Rotate (
    std::vector<std::array<uint32_t, kNumRotations>>& symmetry_edge_position_rotations,
    std::array<std::array<int, kNumSymmetry>, kNumSymmetry>& symmetry_change,
    std::array<std::array<int, kNumPieces>, kNumSymmetry>& rotation_symmetry_change,
    std::array<std::array<int, kNumRotations>, kNumSymmetry>& rotation_symmetry_space,
    uint32_t state,
    uint8_t rotation
) {
    uint32_t symmetry = state >> 24;
    uint32_t position = state & kSymmetryMask;
    uint32_t symmetry_rotation = rotation_symmetry_space[symmetry][rotation];
    std::cout << int(rotation) << " -> symmetry rotation " << symmetry_rotation << std::endl;
    uint32_t state_position_rotation = symmetry_edge_position_rotations[position][symmetry_rotation];
    uint32_t cur_symmetry_change = state_position_rotation >> 24;
    uint32_t position_rotation = state_position_rotation & kSymmetryMask;
    symmetry = symmetry_change[symmetry][cur_symmetry_change];
    return position_rotation | (symmetry<<24);
}


int main() {
    std::vector<std::array<uint32_t, kNumRotations>> symmetry_edge_position_rotations(kNumSymmetryPositions);
    std::array<std::array<int, kNumSymmetry>, kNumSymmetry> symmetry_change;
    std::array<std::array<int, kNumPieces>, kNumSymmetry> rotation_symmetry_change;
    std::array<std::array<int, kNumRotations>, kNumSymmetry> rotation_symmetry_space;

    std::FILE* file_symmetry_edge_position_rotations = std::fopen("symmetry_edge_position_rotations.bin", "rb");
    std::fread(symmetry_edge_position_rotations.data(), sizeof(std::array<uint32_t, kNumRotations>), symmetry_edge_position_rotations.size(), file_symmetry_edge_position_rotations);
    std::FILE* file_symmetry_change = std::fopen("symmetry_change.bin", "rb");
    std::fread(symmetry_change.data(), sizeof(std::array<int, kNumSymmetry>), symmetry_change.size(), file_symmetry_change);
    std::FILE* file_rotation_symmetry_change = std::fopen("rotation_symmetry_change.bin", "rb");
    std::fread(rotation_symmetry_change.data(), sizeof(std::array<int, kNumPieces>), rotation_symmetry_change.size(), file_rotation_symmetry_change);
    std::FILE* file_rotation_symmetry_space = std::fopen("rotation_symmetry_space.bin", "rb");
    std::fread(rotation_symmetry_space.data(), sizeof(std::array<int, kNumRotations>), rotation_symmetry_space.size(), file_rotation_symmetry_space);

    uint32_t solved_edge_position = 0;
    uint32_t state = solved_edge_position;
    std::mt19937 gen(0);
    std::uniform_int_distribution<> dis(0,17);
    std::stack<uint8_t> rotations;
    for (int i = 0; i < 100; i++) {
        uint8_t rotation = dis(gen);
        state = Rotate(symmetry_edge_position_rotations, symmetry_change, rotation_symmetry_change, rotation_symmetry_space, state, rotation);
        std::cout << (state&kSymmetryMask) << " " << (state>>24) << "\n";
        rotations.push(rotation % 2 == 0 ? rotation + 1 : rotation - 1);
    }
    std::vector<uint8_t> v_rotations = {
        6, 3, 16, 1, 4, 2, 6, 8, 13, 17, 3, 7, 11, 6, 8, 17, 6, 17, 1
    };
    for (uint8_t rotation : v_rotations) {
        std::cout << "ROTATION: " << rotation_names[rotation] << "\n";
        state = Rotate(symmetry_edge_position_rotations, symmetry_change, rotation_symmetry_change, rotation_symmetry_space, state, rotation);
        std::cout << (state&kSymmetryMask) << " " << (state>>24) << "\n";
    }
    // /*
    // while (!rotations.empty()) {
    //     state = Rotate(symmetry_edge_position_rotations, symmetry_change, rotation_symmetry_change, rotation_symmetry_space, state, rotations.top());
    //     std::cout << (state&kSymmetryMask) << " " << (state>>24) << "\n";
    //     rotations.pop();
    // }
    // */
    for (int i = 0; i < 3; i++) {
    }
}
