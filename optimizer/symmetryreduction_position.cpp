#include <bits/stdc++.h>
#include <limits>

constexpr uint64_t Factorial(int n) {
    return n <= 1 ? 1 : n * Factorial(n-1);
}

constexpr int kTotalNumEdgePositions = Factorial(12);
constexpr int kNumSymmetry = Factorial(3) * 1<<3;
constexpr int kNumPieces = 12;
constexpr int kNumRotations = 18;

constexpr auto kFactorials = []{
    std::array<uint64_t, kNumPieces+1> arr{};
    for (int i = 0; i <= kNumPieces; i++) {
        arr[i] = Factorial(i);
    }
    return arr;
}();


// map the current position to next position
// -1 marks no change in rotation direction
constexpr std::array<std::array<int8_t, kNumPieces>, kNumRotations> kEdgeRotation =
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


struct RotationRepresentations {
    std::string name;
    std::array<std::array<int, 3>, 3> matrix;
    int activate; // which pieces are affected
    uint8_t index; // internal representation
};


std::array<RotationRepresentations, 18> rotations = {{
    // right left
    {
        "R",
        {{
            { 1,  0,  0},
            { 0,  0,  1},
            { 0, -1,  0}}},
        1,
        0
    },
    {
        "R'",
        {{
            { 1,  0,  0},
            { 0,  0, -1},
            { 0,  1,  0}}},
        1,
        1
    },
    {
        "M",
        {{
            { 1,  0,  0},
            { 0,  0,  1},
            { 0, -1,  0}}},
        0,
        12
    },
    {
        "M'",
        {{
            { 1,  0,  0},
            { 0,  0, -1},
            { 0,  1,  0}}},
        0,
        13
    },
    {
        "L'",
        {{
            { 1,  0,  0},
            { 0,  0,  1},
            { 0, -1,  0}}},
        -1,
        3
    },
    {
        "L",
        {{
            { 1,  0,  0},
            { 0,  0, -1},
            { 0,  1,  0}}},
        -1,
        2
    },

    // up down
    {
        "U",
        {{
            { 0,  0, -1},
            { 0,  1,  0},
            { 1,  0,  0}}},
        1,
        4
    },
    {
        "U'",
        {{
            { 0,  0,  1},
            { 0,  1,  0},
            {-1,  0,  0}}},
        1,
        5
    },
    {
        "E",
        {{
            { 0,  0, -1},
            { 0,  1,  0},
            { 1,  0,  0}}},
        0,
        14
    },
    {
        "E'",
        {{
            { 0,  0,  1},
            { 0,  1,  0},
            {-1,  0,  0}}},
        0,
        15
    },
    {
        "D'",
        {{
            { 0,  0, -1},
            { 0,  1,  0},
            { 1,  0,  0}}},
        -1,
        7
    },
    {
        "D",
        {{
            { 0,  0,  1},
            { 0,  1,  0},
            {-1,  0,  0}}},
        -1,
        6
    },

    // front back
    {
        "F",
        {{
            { 0,  1,  0},
            {-1,  0,  0},
            { 0,  0,  1}}},
        1,
        8
    },
    {
        "F'",
        {{
            { 0, -1,  0},
            { 1,  0,  0},
            { 0,  0,  1}}},
        1,
        9
    },
    {
        "S'",
        {{
            { 0,  1,  0},
            {-1,  0,  0},
            { 0,  0,  1}}},
        0,
        17
    },
    {
        "S",
        {{
            { 0, -1,  0},
            { 1,  0,  0},
            { 0,  0,  1}}},
        0,
        16
    },
    {
        "B'",
        {{
            { 0,  1,  0},
            {-1,  0,  0},
            { 0,  0,  1}}},
        -1,
        11
    },
    {
        "B",
        {{
            { 0, -1,  0},
            { 1,  0,  0},
            { 0,  0,  1}}},
        -1,
        10
    },
}};


void PrintRotationMatrices(const std::vector<std::array<std::array<int, 3>, 3>>& symmetry_matrix) {
    for (int i = 0; i < symmetry_matrix.size(); i++) {
        std::cout << "Matrix " << i << ":\n";
        for (const auto& row : symmetry_matrix[i]) {
            for (const int val : row) {
                std::cout << std::setw(4) << val;
            }
            std::cout << "\n";
        }
        std::cout << "\n";
    }
}


void PrintXyzToCompact(const std::map<std::array<int, 3>, int, std::greater<>>& xyz_to_compact) {
    std::cout << "xyz to compact:\n";
    for (const auto& [vec, idx] : xyz_to_compact) {
        std::cout << "[" << std::setw(2) << vec[0] << ", "
                         << std::setw(2) << vec[1] << ", "
                         << std::setw(2) << vec[2] << "] -> " << idx << "\n";
    }
    std::cout << "\n";
}


void PrintRotationSymmetryChange(const std::array<std::array<int, kNumPieces>, kNumSymmetry>& rotation_symmetry_change) {
    for (int i = 0; i < kNumSymmetry; i++) {
        std::cout << "Symmetry " << i << ": ";
        for (int j = 0; j < kNumPieces; j++) {
            std::cout << std::setw(3) << rotation_symmetry_change[i][j];
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}


void PrintSymmetryChange(const std::array<std::array<int, kNumSymmetry>, kNumSymmetry>& symmetry_change) {
    for (int i = 0; i < kNumSymmetry; i++) {
        std::cout << "Symmetry change " << std::setw(2) << i << ": ";
        for (int j = 0; j < kNumSymmetry; j++) {
            std::cout << std::setw(3) << symmetry_change[i][j];
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}

std::array<std::array<int, 3>, 3> MatMul(const std::array<std::array<int, 3>, 3>& lhs,
                                         const std::array<std::array<int, 3>, 3>& rhs) {
    std::array<std::array<int, 3>, 3> result = {};
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            for (int k = 0; k < 3; k++) {
                result[i][j] += lhs[i][k] * rhs[k][j];
            }
        }
    }
    return result;
}


std::array<std::array<int, 3>, 3> MatTrans(const std::array<std::array<int, 3>, 3>& mat) {
    std::array<std::array<int, 3>, 3> result = {};
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            result[i][j] = mat[j][i];
        }
    }
    return result;
}


std::array<int, 3> MatVecMul(const std::array<std::array<int, 3>, 3>& mat,
                             const std::array<int, 3>& vec) {
    std::array<int, 3> result = {};
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            result[i] += mat[i][j] * vec[j];
        }
    }
    return result;
}


std::array<int, kNumPieces> Decompress(uint64_t n) {
    std::array<int, kNumPieces> result;
    uint32_t available = (1<<kNumPieces) - 1; // 12 bits set = {0..11} available

    for (int i = kNumPieces-1; i >= 0; i--) {
        int idx = n / kFactorials[i];
        n %= kFactorials[i];

        // Find idx-th set bit in available
        uint32_t mask = available;
        for (int k = 0; k < idx; k++) {
            mask &= mask - 1; // clear lowest set bit
        }
        int bit = __builtin_ctz(mask);

        result[kNumPieces-1 - i] = bit;
        available &= ~(1U << bit);
    }
    return result;
}


uint64_t Compress(const std::array<int, kNumPieces>& perm) {
    uint64_t result = 0;
    uint32_t available = (1<<kNumPieces) - 1; // 12 bits set = {0..11} available

    for (int i = 0; i < kNumPieces; i++) {
        // Count how many available elements are less than perm[i]
        uint32_t below = available & ((1U << perm[i]) - 1);
        int idx = __builtin_popcount(below);

        result += idx * kFactorials[kNumPieces-1 - i];
        available &= ~(1U << perm[i]);
    }
    return result;
}

int main() {
    // matrix of all symmetries
    std::vector<std::array<std::array<int, 3>, 3>> symmetry_matrix;
    std::vector<int> perm = {0, 1, 2};
    do {
        for (int flipsign = 0; flipsign < 1<<3; flipsign++) {
            std::array<std::array<int, 3>, 3> matrix = {};
            for (int i = 0; i < 3; i++) {
                matrix[i][perm[i]] = ((flipsign >> i) & 1) == 0 ? 1 : -1;
            }
            symmetry_matrix.push_back(matrix);
        }
    } while (std::next_permutation(perm.begin(), perm.end()));
    assert(symmetry_matrix.size() == kNumSymmetry);
    PrintRotationMatrices(symmetry_matrix);

    // matrix multiplication for symmetry change after rotation
    std::map<std::array<std::array<int, 3>, 3>, int> matrix_index;
    for (int i = 0; i < kNumSymmetry; i++) {
        matrix_index[symmetry_matrix[i]] = i;
    }
    std::array<std::array<int, kNumSymmetry>, kNumSymmetry> symmetry_change;
    for (int i = 0; i < kNumSymmetry; i++) { // matrix multiplication symmetry change
        for (int j = 0; j < kNumSymmetry; j++) { // current symmetry
            symmetry_change[i][j] = matrix_index[MatMul(symmetry_matrix[i], symmetry_matrix[j])];
        }
    }
    PrintSymmetryChange(symmetry_change);

    // xyz to compact
    std::map<std::array<int, 3>, int, std::greater<>> xyz_to_compact;
    for (auto base : {
        std::array<int, 3>{ 0,  1, 1},    // 3 permutations
        std::array<int, 3>{-1,  0, 1},    // 6 permutations
        std::array<int, 3>{-1, -1, 0}}) { // 3 permutations
        do {
            xyz_to_compact[base];
        } while (std::next_permutation(base.begin(), base.end()));
    }
    int compact_idx = 0;
    for (auto& [vec, idx] : xyz_to_compact) {
        idx = compact_idx++;
    }
    assert(xyz_to_compact.size() == kNumPieces);
    PrintXyzToCompact(xyz_to_compact);

    // rotation symmetry change
    std::array<std::array<int, kNumPieces>, kNumSymmetry> rotation_symmetry_change;
    for (int i = 0; i < kNumSymmetry; i++) {
        for (auto& [vec, idx] : xyz_to_compact) {
            rotation_symmetry_change[i][idx] = xyz_to_compact[MatVecMul(symmetry_matrix[i], vec)];
        }
    }
    PrintRotationSymmetryChange(rotation_symmetry_change);

    // TODO: Rotations
    // check if the new rotation format is correct (which it is)
    for (RotationRepresentations rotation :  rotations) {
        for (auto& [vec, idx] : xyz_to_compact) {
            bool rotate = false;
            for (int i = 0; i < 3; i++) {
                if (rotation.matrix[i][i] != 0) {
                    if (rotation.activate == 0) {
                        if (vec[i] == -1 || vec[i] == 1) {
                            rotate = true;
                        }
                    }
                    else if (vec[i] == rotation.activate) {
                        rotate = true;
                    }
                }
            }
            if (!rotate) {
                continue;
            }
            int from = idx;
            int to = xyz_to_compact[MatVecMul(rotation.matrix, vec)];
            int rotation_idx = rotation.index;
            if (kEdgeRotation[rotation_idx][from] != to) {
                std::cout << "WRONG! " << from << " -> " << to << " [" << rotation_idx << "]\n";
            }
            else {
                std::cout << "CORRECT: " << from << " -> " << to << " [" << rotation_idx << "]\n";
            }
        }
    }
    std::array<std::array<int, kNumRotations>, kNumSymmetry> rotation_symmetry_space;
    for (RotationRepresentations rotation : rotations) {
        for (std::array<std::array<int, 3>, 3> symmetry : symmetry_matrix) {
            // inefficent but who cares
            bool corr = false;
            RotationRepresentations changed = {
                "",
                MatMul(symmetry, MatMul(rotation.matrix, MatTrans(symmetry))),
                rotation.activate,
                255
            };
            for (int i = 0; i < 3; i++) {
                if (rotation.matrix[i][i] == 1) {
                    for (int j = 0; j < 3; j++) {
                        if (symmetry[j][i] != 0) {
                            changed.activate = rotation.activate * symmetry[j][i];
                        }
                    }
                }
            }
            for (RotationRepresentations check_rotation : rotations) {
                if (changed.matrix == check_rotation.matrix && changed.activate == check_rotation.activate) {
                    rotation_symmetry_space[matrix_index[symmetry]][check_rotation.index] = rotation.index;
                    std::cout << rotation.name << " " << matrix_index[symmetry] << " -> " << check_rotation.name << "\n";
                    corr = true;
                    break;
                }
            }
            if (!corr) {
                std::cout << rotation.name << " " << matrix_index[symmetry] << " -> " << "WRONG!" << "\n";
            }
        }
    }

    // create a lookup table from the position representation a symmetry and additionally save which symmetry transition was used
    // 24 bits for new index and the other bits for the symmetry
    std::vector<uint32_t> position_to_symmetry_idx(kTotalNumEdgePositions, std::numeric_limits<uint32_t>::max());
    uint32_t cur_symmetry_cnt = 0;
    std::vector<uint8_t> symmetry_cnt;
    constexpr uint32_t kSymmetryMask = (1<<24) - 1; // as the number of unique symmetry position is 9985968
    uint64_t progress = 0;
    for (uint64_t pos = 0; pos < kTotalNumEdgePositions; pos++) {
        if (position_to_symmetry_idx[pos] != std::numeric_limits<uint32_t>::max()) {
            symmetry_cnt[position_to_symmetry_idx[pos] & kSymmetryMask]++;
            continue;
        }
        std::array<int, kNumPieces> decompressed = Decompress(pos);
        for (uint32_t symmetry = 0; symmetry < kNumSymmetry; symmetry++) {
            std::array<int, kNumPieces> symmetry_decompressed{};
            for (int i = 0; i < kNumPieces; i++) {
                symmetry_decompressed[rotation_symmetry_change[symmetry][i]] = rotation_symmetry_change[symmetry][decompressed[i]];
            }
            uint64_t symmetry_compacted = Compress(symmetry_decompressed);
            if (position_to_symmetry_idx[symmetry_compacted] != std::numeric_limits<uint32_t>::max()) {
                continue; // already in one of the other symmetries (for example starting position all symmetries are identical)
            }
            progress++;
            if (progress % 1000000 == 0) {
                std::cout << progress << " / " << kTotalNumEdgePositions << "\n";
            }
            position_to_symmetry_idx[symmetry_compacted] = cur_symmetry_cnt | (symmetry << 24);
        }
        symmetry_cnt.push_back(1);
        cur_symmetry_cnt++;
    }
    assert(std::accumulate(symmetry_cnt.begin(), symmetry_cnt.end(), 0) == kTotalNumEdgePositions);
    std::cout << cur_symmetry_cnt << "\n";

    // create legal rotations
    std::array<uint32_t, kNumRotations> default_rot;
    default_rot.fill(std::numeric_limits<uint32_t>::max());
    std::vector<std::array<uint32_t, kNumRotations>> symmetry_edge_position_rotations(cur_symmetry_cnt, default_rot);
    for (uint64_t pos = 0; pos < kTotalNumEdgePositions; pos++) {
        if ((position_to_symmetry_idx[pos] >> 24) != 0) { // first symmetry
            continue;
        }
        uint32_t cur_idx = position_to_symmetry_idx[pos] & kSymmetryMask;

        std::array<int, kNumPieces> cur_state = Decompress(pos);
        for (int rotation = 0; rotation < kNumRotations; rotation++) {
            // do the rotation
            std::array<int, kNumPieces> next_state;
            for (int i = 0; i < kNumPieces; i++) {
                if (kEdgeRotation[rotation][cur_state[i]] == -1) {
                    next_state[i] = cur_state[i];
                }
                else {
                    next_state[i] = kEdgeRotation[rotation][cur_state[i]];
                }
            }
            uint64_t compacted = Compress(next_state);
            uint32_t next_idx_and_symmetry_change = position_to_symmetry_idx[compacted];
            symmetry_edge_position_rotations[cur_idx][rotation] = next_idx_and_symmetry_change;
        }
    }

    for (auto& a : symmetry_edge_position_rotations) {
        for (auto& b : a) {
            assert(b != std::numeric_limits<uint32_t>::max());
        }
    }

    // store to file
    std::FILE* file_symmetry_edge_position_rotations = std::fopen("symmetry_edge_position_rotations.bin", "wb");
    std::fwrite(symmetry_edge_position_rotations.data(), sizeof(std::array<uint32_t, kNumRotations>), symmetry_edge_position_rotations.size(), file_symmetry_edge_position_rotations);
    std::FILE* file_symmetry_change = std::fopen("symmetry_change.bin", "wb");
    std::fwrite(symmetry_change.data(), sizeof(std::array<int, kNumSymmetry>), symmetry_change.size(), file_symmetry_change);
    std::FILE* file_rotation_symmetry_change = std::fopen("rotation_symmetry_change.bin", "wb");
    std::fwrite(rotation_symmetry_change.data(), sizeof(std::array<int, kNumPieces>), rotation_symmetry_change.size(), file_rotation_symmetry_change);
    std::FILE* file_rotation_symmetry_space = std::fopen("rotation_symmetry_space.bin", "wb");
    std::fwrite(rotation_symmetry_space.data(), sizeof(std::array<int, kNumRotations>), rotation_symmetry_space.size(), file_rotation_symmetry_space);

    std::cout << "finished writing to file\n";
}
