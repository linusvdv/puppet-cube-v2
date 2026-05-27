#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdint>
#include <map>
#include <functional>
#include <set>

#include "cube.hpp"
#include "edge.hpp"
#include "logger.hpp"
#include "rotation.hpp"
#include "settings.hpp"
#include "utils.hpp"

namespace {
constexpr int kNumTotalEdgePositions = Factorial(12);
constexpr int kNumPositions = 9985968;
constexpr int kNumOrientations = 1 << (kNumEdges-1);
constexpr int kNumSymmetries = Factorial(3) * (1<<3);
constexpr int kNumSymmetryChange = 0; // TODO:

constexpr std::array<uint64_t, kNumEdges+1> kFactorials = []{
    std::array<uint64_t, kNumEdges+1> arr{};
    for (int i = 0; i <= kNumEdges; i++) {
        arr[i] = Factorial(i);
    }
    return arr;
}();

// position in 3D space of the edges
std::map<Vec3i, int, std::greater<>> xyz_to_idx_pos;
std::array<Vec3i, kNumEdges> idx_to_xyz_pos;

// symmetries
std::array<Mat3i, kNumSymmetries> idx_to_mat_symmetry;
std::map<Mat3i, int> mat_to_idx_symmetry;
std::array<int, kNumSymmetries> idx_symmetry_reverse;

// faster rotations by only looking at the indices
std::array<std::array<int, kNumEdges>, kNumRotations> idx_rotations;

// edge positions
constexpr uint32_t kPositionShift = 24;
constexpr uint32_t kPositionMask = (1<<kPositionShift)-1;

// symmetry position
std::vector<std::array<int, kNumEdges>> position_symmetry_change(kNumSymmetries);
std::vector<uint32_t> position_to_symmetry_position(kNumTotalEdgePositions, uint32_t(-1)); // [position] -> symmetry position, symmetry
std::vector<std::array<uint32_t, kNumSymmetries>> symmetry_position_to_position(kNumPositions); // [symmetry_position][symmetry] -> position
}


namespace edge {
// these lookup tables are all precomputed / are stored in a file
// needed for a rotation
std::vector<std::array<uint8_t, kNumRotations>> rotation_changes(kNumSymmetries); // [symmetry][rotation] -> rotation
std::vector<std::array<uint32_t, kNumRotations>> edge_positions(kNumPositions); // [position][rotation] -> (symmetry_change << 24) | position
std::vector<std::array<uint16_t, kNumSymmetries>> symmetry_changes(kNumSymmetryChange); // [symmetry_change][symmetry] -> symmetry, relative symmetry
std::vector<std::array<std::array<uint16_t, kNumRotations>, kNumSymmetries>> edge_orientations(kNumOrientations); // [orientation][symmetry_change][rotation]


Vec3i RotateXYZPiece(const Vec3i& position, uint8_t rotation) {
    RotationRepresentations cur_rotation = index_to_rotation_representations[rotation];
    for (int i = 0; i < 3; i++) {
        if (cur_rotation.matrix[i][i] != 0) {
            if (cur_rotation.activate == 0 && position[i] == 0) {
                return position;
            }
            if (cur_rotation.activate != 0 &&
                cur_rotation.activate != position[i]) {
                return position;
            }
        }
    }
    return MatVecMul(cur_rotation.matrix, position);
}


uint64_t LehmerCode(const std::array<uint8_t, kNumEdges>& perm) {
    uint64_t result = 0;
    uint32_t available = (1<<kNumEdges) - 1; // 12 bits set = {0..11} available

    for (int i = 0; i < kNumEdges; i++) {
        // Count how many available elements are less than perm[i]
        uint32_t below = available & ((1U << perm[i]) - 1);
        int idx = std::popcount(below);

        result += idx * kFactorials[kNumEdges-1 - i];
        available &= ~(1U << perm[i]);
    }
    return result;
}


std::array<uint8_t, kNumEdges> DecompressedLehmerCode(uint64_t position) {
    std::array<uint8_t, kNumEdges> result;
    uint32_t available = (1<<kNumEdges) - 1; // 12 bits set = {0..11} available

    for (int i = kNumEdges-1; i >= 0; i--) {
        int idx = position / kFactorials[i];
        position %= kFactorials[i];

        // Find idx-th set bit in available
        uint32_t mask = available;
        for (int k = 0; k < idx; k++) {
            mask &= mask - 1; // clear lowest set bit
        }
        int bit = std::countr_zero(mask);

        result[kNumEdges-1 - i] = bit;
        available &= ~(1U << bit);
    }
    return result;
}


uint32_t RotatePieces(uint32_t position, uint8_t rotation) {
    std::array<uint8_t, kNumEdges> decompressed_lehmer_code = DecompressedLehmerCode(position);
    for (uint8_t& edge_piece : decompressed_lehmer_code) {
        edge_piece = idx_rotations[rotation][edge_piece];
    }
    return LehmerCode(decompressed_lehmer_code);
}


std::array<uint8_t, kNumEdges> SymmetryPositionRotation(std::array<uint8_t, kNumEdges>& position, uint8_t symmetry) {
    std::array<uint8_t, kNumEdges> res{};
    for (int i = 0; i < kNumEdges; i++) {
        res[position_symmetry_change[symmetry][i]] = position_symmetry_change[symmetry][position[i]];
    }
    return res;
}


void SymmetryPositionInit() {
    for (int i = 0; i < kNumSymmetries; i++) {
        for (int j = 0; j < kNumEdges; j++) {
            position_symmetry_change[i][j] = xyz_to_idx_pos[MatVecMul(idx_to_mat_symmetry[i], idx_to_xyz_pos[j])];
        }
    }

    // set all symmetry_position_to_position elements to -1 such that a invalid symmetry position can easily be checked
    // a position is invalid if it is identical to a other symmetry position (e.g. starting position with different symmetries)
    std::fill(symmetry_position_to_position[0].data(),
              symmetry_position_to_position[0].data() + (kNumPositions * kNumSymmetries),
              uint32_t(-1));

    std::array<uint8_t, kNumEdges> position_permutations;
    for (int i = 0; i < kNumEdges; i++) {
        position_permutations[i] = i;
    }

    std::set<std::array<int, kNumSymmetries>> different_symmetries;
    uint32_t cnt = 0;
    uint32_t progress = 0;
    do {
        if (position_to_symmetry_position[LehmerCode(position_permutations)] != uint32_t(-1)) {
            continue;
        }
        std::array<int, kNumSymmetries> cur_different_symmetries;
        std::unordered_map<uint32_t, int> pos_which_symmetry;
        int pos_which_symmetry_cnt = 0;
        for (int i = 0; i < kNumSymmetries; i++) {
            uint64_t lehmer_code = LehmerCode(SymmetryPositionRotation(position_permutations, i));
            if (pos_which_symmetry.contains(lehmer_code)) {
                cur_different_symmetries[i] = pos_which_symmetry[lehmer_code];
            }
            else {
                pos_which_symmetry[lehmer_code] = pos_which_symmetry_cnt;
                cur_different_symmetries[i] = pos_which_symmetry_cnt;
                pos_which_symmetry_cnt++;
            }

            if (position_to_symmetry_position[lehmer_code] != uint32_t(-1)) {
                continue;
            }
            progress++;
            if (progress % 1000000 == 0) {
                LOG_EXTRA(progress, "/", kNumTotalEdgePositions);
            }
            position_to_symmetry_position[lehmer_code] = cnt | (idx_symmetry_reverse[i] << 24);
            symmetry_position_to_position[cnt][idx_symmetry_reverse[i]] = lehmer_code;
        }
        different_symmetries.insert(cur_different_symmetries);
        cnt++;
    } while (std::next_permutation(position_permutations.begin(), position_permutations.end()));
    if (cnt != kNumPositions) {
        LOG_CRITICAL("wrong symmetry position count");
    }
    LOG_ALL("Different Symmetries:", different_symmetries.size(), different_symmetries);
}


// symmetry * rotation
std::vector<std::array<uint8_t, kNumRotations>> RotationChangesInit() {
    std::vector<std::array<uint8_t, kNumRotations>> rotation_changes_init(kNumSymmetries);
    for (int symmetry = 0; symmetry < kNumSymmetries; symmetry++) {
        for (int i = 0; i < kNumRotations; i++) {
            RotationRepresentations rotation = index_to_rotation_representations[i];
            Mat3i matrix = MatMul(idx_to_mat_symmetry[symmetry],
                MatMul(rotation.matrix,
                MatTrans(idx_to_mat_symmetry[symmetry])));
            int activate = rotation.activate;
            for (int i = 0; i < 3; i++) {
                if (rotation.matrix[i][i] == 1) {
                    for (int j = 0; j < 3; j++) {
                        if (idx_to_mat_symmetry[symmetry][j][i] != 0) {
                            activate *= idx_to_mat_symmetry[symmetry][j][i];
                        }
                    }
                }
            }
            rotation_changes_init[symmetry][i] = matrix_to_rotation_representations[{matrix, activate}].index;
        }
    }
    LOG_EXTRA(rotation_changes_init);
    return rotation_changes_init;
}


// [position][rotation] -> (symmetry_change << 24) | position
// [symmetry_change][symmetry] -> symmetry, relative symmetry
std::vector<std::array<uint32_t, kNumRotations>> EdgePositionsInit() {
    std::vector<std::array<uint32_t, kNumRotations>> edge_positions_init(kNumPositions);

    int symmetry_change_cnt = 0;
    std::map<std::array<uint16_t, kNumSymmetries>, int> symmetry_changes_contains;
    symmetry_changes.clear();

    for (int sym_position = 0; sym_position < kNumPositions; sym_position++) {
        for (int sym_rotation = 0; sym_rotation < kNumRotations; sym_rotation++) {
            std::array<uint16_t, kNumSymmetries> cur_symmmetry_changes;
            cur_symmmetry_changes.fill(uint16_t(-1));
            uint32_t std_next_sym_position = -1;

            for (int sym = 0; sym < kNumSymmetries; sym++) {
                uint32_t position = symmetry_position_to_position[sym_position][sym];
                // invalid position as it is the same as another symmetry
                if (position == uint32_t(-1)) {
                    continue;
                }
                uint32_t rotation = rotation_changes[idx_symmetry_reverse[sym]][sym_rotation];

                uint32_t next_position = RotatePieces(position, rotation);
                uint32_t next_sym_position = position_to_symmetry_position[next_position] & kPositionMask;
                uint32_t next_sym = position_to_symmetry_position[next_position] >> kPositionShift;

                cur_symmmetry_changes[sym] = next_sym;
                if (std_next_sym_position != uint32_t(-1)) {
                    if (next_sym_position != std_next_sym_position) {
                        LOG_CRITICAL("Not same position");
                    }
                }
                else {
                    std_next_sym_position = next_sym_position;
                }
            }
            if (symmetry_changes_contains.contains(cur_symmmetry_changes)) {
                edge_positions_init[sym_position][sym_rotation] = (symmetry_changes_contains[cur_symmmetry_changes] << kPositionShift) | std_next_sym_position;
            }
            else {
                symmetry_changes.push_back(cur_symmmetry_changes);
                symmetry_changes_contains[cur_symmmetry_changes] = symmetry_change_cnt;
                edge_positions_init[sym_position][sym_rotation] = (symmetry_change_cnt << kPositionShift) | std_next_sym_position;
                symmetry_change_cnt++;
            }

        }
        if (sym_position % 100000 == 0) {
            LOG_EXTRA(sym_position, "/", kNumPositions);
        }
    }

    LOG_INFO("Symmetry Change CNT:", symmetry_change_cnt);
    LOG_EXTRA("symmetry_changes:", symmetry_changes);
    return edge_positions_init;
}


void Init() {
    // init position in 3D space of the edges
    for (int i = 1; i >= -1; i--) {
        for (int j = 1; j >= -1; j--) {
            for (int k = 1; k >= -1; k--) {
                if (int(i != 0) + int(j != 0) + int(k!= 0) == 2) {
                    xyz_to_idx_pos.emplace(Vec3i{i, j, k}, 0);
                }
            }
        }
    }
    assert(xyz_to_idx_pos.size() == kNumEdges);
    int cnt = 0;
    for (auto& [xyz, idx] : xyz_to_idx_pos) {
        idx = cnt;
        idx_to_xyz_pos[cnt++] = xyz;
    }

    // symmetries
    std::array<int, 3> perm = {0, 1, 2};
    cnt = 0;
    do {
        for (int flipsign = 0; flipsign < (1<<3); flipsign++) {
            Mat3i matrix = {};
            for (int i = 0; i < 3; i++) {
                matrix[i][perm[i]] = ((flipsign >> i) & 1) == 0 ? 1 : -1;
            }
            idx_to_mat_symmetry[cnt] = matrix;
            mat_to_idx_symmetry[matrix] = cnt++;
        }
    } while (std::next_permutation(perm.begin(), perm.end()));
    for (int i = 0; i < kNumSymmetries; i++) {
        idx_symmetry_reverse[i] = mat_to_idx_symmetry[MatTrans(idx_to_mat_symmetry[i])];
    }
    LOG_EXTRA("symmetry: ", idx_to_mat_symmetry);

    // generate the idx rotation
    for (int rotation = 0; rotation < kNumRotations; rotation++) {
        for (int i = 0; i < kNumEdges; i++) {
            idx_rotations[rotation][i] = xyz_to_idx_pos[RotateXYZPiece(idx_to_xyz_pos[i], rotation)];
        }
    }

    SymmetryPositionInit();
    RotationChangesInit();
    EdgePositionsInit();
}

// most important one!
void Rotate(uint32_t& position, uint8_t& symmetry, uint16_t& orientation, uint8_t rotation) {
    // change rotation relative to symmetry
    rotation = rotation_changes[symmetry][rotation];
    // position lookup (pos + symmetry change)
    uint32_t packed = edge_positions[position][rotation];
    uint32_t symmetry_change = packed >> kPositionShift;
    position = packed & kPositionMask;
    // change symmetry abs
    uint16_t packed2 = symmetry_changes[symmetry_change][symmetry];
    symmetry = uint8_t(packed2);
    // orientation lookup
    orientation = edge_orientations[orientation][uint8_t(packed2>>8)][rotation];
}
}


int main (int argc, char *argv[]) {
    // settings initialization
    Settings(argc, argv);
    RotationInit();
    edge::Init();
    LOG_INFO("Finished Edge");
}
