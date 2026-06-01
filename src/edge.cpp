#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdint>
#include <map>
#include <functional>
#include <numeric>
#include <random>

#include "cube.hpp"
#include "edge.hpp"
#include "logger.hpp"
#include "rotation.hpp"
#include "settings.hpp"
#include "utils.hpp"

// TODO: only assign the vecors space when the are needed for the generation of the precomputation, delete it afterwads, and else do not generate it at all
namespace {
constexpr int kNumTotalEdgePositions = Factorial(12);
constexpr int kNumPositions = 9985968;
constexpr int kNumOrientations = 1 << (kNumEdges-1);
constexpr int kNumSymmetries = Factorial(3) * (1<<3);
constexpr int kNumSymmetryChange = 981; // this is unfortunatly more than 256 (which would fit in uint8_t and could therefore be packed in a uint32_t with the position

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
// symmetry_multiply [symmetry(change)][symmetry] -> symmetry
std::array<std::array<int, kNumSymmetries>, kNumSymmetries> symmetry_multiply;

// faster rotations by only looking at the indices
std::array<std::array<int, kNumEdges>, kNumRotations> idx_rotations;

// edge positions
constexpr uint64_t kPositionShift = 24;
constexpr uint64_t kPositionMask = (1<<kPositionShift)-1;

// symmetry position
std::vector<std::array<int, kNumEdges>> position_symmetry_change(kNumSymmetries);
std::vector<uint32_t> position_to_symmetry_position(kNumTotalEdgePositions, uint32_t(-1)); // [position] -> symmetry position, symmetry
std::vector<std::array<uint32_t, kNumSymmetries>> symmetry_position_to_position(kNumPositions); // [symmetry_position][symmetry] -> position

// it is possible to have multiple symmetries which represent the same position (e.g. solved state has all symmetries the same)
// the lowest symmetry is then used
// there are 91 symmetries so that means it fits in uint8_t
std::vector<uint8_t> symmetry_position_active_symmetries(kNumPositions);
std::map<std::array<uint8_t, kNumSymmetries>, uint8_t> active_symmetries_map;
std::vector<std::array<uint8_t, kNumSymmetries>> symmetries_to_active_symmetries; // [which_active_symmetry][symmetry] -> symmetry
uint8_t active_symmetries_map_cnt = 0;
}


namespace edge {
// these lookup tables are all precomputed / are stored in a file
// needed for a rotation
std::vector<std::array<uint8_t, kNumRotations>> rotation_changes(kNumSymmetries); // [symmetry][rotation] -> rotation
std::vector<std::array<uint64_t, kNumRotations>> edge_positions(kNumPositions); // [position][rotation] -> (symmetry_change << 24) | position
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

    uint32_t cnt = 0;
    uint32_t progress = 0;
    do {
        uint64_t lehmer_code_default = LehmerCode(position_permutations);
        if (position_to_symmetry_position[lehmer_code_default] != uint32_t(-1)) {
            continue;
        }
        std::array<uint8_t, kNumSymmetries> cur_active_symmetries;
        std::unordered_map<uint32_t, uint8_t> lehman_to_active_symmetry;
        for (int sym = 0; sym < kNumSymmetries; sym++) {
            uint64_t lehmer_code = LehmerCode(SymmetryPositionRotation(position_permutations, idx_symmetry_reverse[sym]));
            // active symmetry
            if (lehman_to_active_symmetry.contains(lehmer_code)) {
                cur_active_symmetries[sym] = lehman_to_active_symmetry[lehmer_code];
            }
            else {
                lehman_to_active_symmetry[lehmer_code] = sym;
                cur_active_symmetries[sym] = sym;
            }

            if (position_to_symmetry_position[lehmer_code] != uint32_t(-1)) {
                continue;
            }

            progress++;
            if (progress % 1000000 == 0) {
                LOG_EXTRA(progress, "/", kNumTotalEdgePositions);
            }
            position_to_symmetry_position[lehmer_code] = cnt | (sym << kPositionShift);
            symmetry_position_to_position[cnt][sym] = lehmer_code;
        }
        if (active_symmetries_map.contains(cur_active_symmetries)) {
            symmetry_position_active_symmetries[position_to_symmetry_position[lehmer_code_default]&kPositionMask] = active_symmetries_map[cur_active_symmetries];
        }
        else {
            active_symmetries_map[cur_active_symmetries] = active_symmetries_map_cnt;

            symmetries_to_active_symmetries.push_back(cur_active_symmetries);

            symmetry_position_active_symmetries[position_to_symmetry_position[lehmer_code_default]&kPositionMask] = active_symmetries_map_cnt;
            active_symmetries_map_cnt++;
        }
        cnt++;
    } while (std::next_permutation(position_permutations.begin(), position_permutations.end()));
    if (cnt != kNumPositions) {
        LOG_CRITICAL("wrong symmetry position count");
    }
    LOG_ALL("Active symmetries:", active_symmetries_map_cnt, symmetries_to_active_symmetries);
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
std::vector<std::array<uint64_t, kNumRotations>> EdgePositionsInit() {
    std::vector<std::array<uint64_t, kNumRotations>> edge_positions_init(kNumPositions);

    int symmetry_change_cnt = 0;
    std::map<std::array<uint16_t, kNumSymmetries>, int> symmetry_changes_contains;
    symmetry_changes.clear();

    for (uint32_t sym_position = 0; sym_position < kNumPositions; sym_position++) {
        for (int sym_rotation = 0; sym_rotation < kNumRotations; sym_rotation++) {
            uint32_t position = symmetry_position_to_position[sym_position][0];
            uint32_t next_position = RotatePieces(position, sym_rotation);

            uint32_t next_sym_position = position_to_symmetry_position[next_position] & kPositionMask;
            uint8_t next_pos_acitve_symmetry = symmetry_position_active_symmetries[next_sym_position];
            uint32_t next_sym_change = position_to_symmetry_position[next_position] >> kPositionShift;

            std::array<uint16_t, kNumSymmetries> cur_symmmetry_changes;
            cur_symmmetry_changes.fill(uint16_t(-1));
            // fast transition possible if both have all symmetries different
            for (int sym = 0; sym < kNumSymmetries; sym++) {
                cur_symmmetry_changes[sym] = symmetries_to_active_symmetries[next_pos_acitve_symmetry][symmetry_multiply[next_sym_change][sym]];
            }

            if (!symmetry_changes_contains.contains(cur_symmmetry_changes)) {
                symmetry_changes.push_back(cur_symmmetry_changes);
                symmetry_changes_contains[cur_symmmetry_changes] = symmetry_change_cnt;
                symmetry_change_cnt++;
            }
            edge_positions_init[sym_position][sym_rotation] = (uint64_t(symmetry_changes_contains[cur_symmmetry_changes]) << kPositionShift) | next_sym_position;
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

    // symmetry multiply
    for (int i = 0; i < kNumSymmetries; i++) {
        for (int j = 0; j < kNumSymmetries; j++) {
            symmetry_multiply[i][j] = mat_to_idx_symmetry[MatMul(idx_to_mat_symmetry[i], idx_to_mat_symmetry[j])];
        }
    }


    // generate the idx rotation
    for (int rotation = 0; rotation < kNumRotations; rotation++) {
        for (int i = 0; i < kNumEdges; i++) {
            idx_rotations[rotation][i] = xyz_to_idx_pos[RotateXYZPiece(idx_to_xyz_pos[i], rotation)];
        }
    }


    std::array<uint8_t, kNumEdges> pos;
    std::iota(pos.begin(), pos.end(), 0);
    int rotation = name_to_rotation_representations["M"].index;
    for (uint8_t& edge_piece : pos) {
        edge_piece = idx_rotations[rotation][edge_piece];
    }

    SymmetryPositionInit();
    // TODO: do this with load from file
    rotation_changes = RotationChangesInit();
    edge_positions = EdgePositionsInit();

}

// most important one!
void Rotate(uint32_t& position, uint8_t& symmetry, uint16_t& orientation, uint8_t rotation) {
    // change rotation relative to symmetry
    rotation = rotation_changes[symmetry][rotation];
    // position lookup (pos + symmetry change)
    uint64_t packed = edge_positions[position][rotation];
    uint32_t symmetry_change = packed >> kPositionShift;
    position = packed & kPositionMask;
    // change symmetry abs
    uint16_t packed2 = symmetry_changes[symmetry_change][symmetry];
    symmetry = uint8_t(packed2);
    // orientation lookup
    // orientation = edge_orientations[orientation][uint8_t(packed2>>8)][rotation];
}
}


void TestEdgePosition() {
    std::array<uint8_t, kNumEdges> cur_pos;
    uint16_t orientation = 0;
    std::iota(cur_pos.begin(), cur_pos.end(), 0);
    int cnt = 0;
    do {
        uint32_t cur_position = position_to_symmetry_position[edge::LehmerCode(cur_pos)] & kPositionMask;
        uint8_t cur_symmetry = position_to_symmetry_position[edge::LehmerCode(cur_pos)] >> kPositionShift;
        for (int rotation = 0; rotation < kNumRotations; rotation++) {
            uint32_t position = cur_position;
            uint8_t symmetry = cur_symmetry;
            std::array<uint8_t, kNumEdges> pos = cur_pos;
            for (uint8_t& edge_piece : pos) {
                edge_piece = idx_rotations[rotation][edge_piece];
            }
            edge::Rotate(position, symmetry, orientation, rotation);
            uint64_t lehmer_code_old = edge::LehmerCode(pos);
            uint64_t lehmer_code_new = symmetry_position_to_position[position][symmetry];
            if (lehmer_code_old != lehmer_code_new) {
                LOG_EXTRA("position", position_to_symmetry_position[lehmer_code_old] & kPositionMask, position_to_symmetry_position[lehmer_code_new] & kPositionMask);
                LOG_EXTRA("symmetry", position_to_symmetry_position[lehmer_code_old] >> kPositionShift, position_to_symmetry_position[lehmer_code_new] >> kPositionShift);
                LOG_CRITICAL("position or symmetry wrong");
            }
        }
        if (cnt % 100000 == 0) {
            LOG_EXTRA(cnt, "/", kNumTotalEdgePositions);
        }
        cnt++;
    } while(std::next_permutation(cur_pos.begin(), cur_pos.end()));
}


int main (int argc, char *argv[]) {
    // settings initialization
    Settings(argc, argv);
    RotationInit();
    edge::Init();
    LOG_INFO("Finished Edge");
    // TestEdgePosition();
    // LOG_INFO("Finished Test");
}
