#include <algorithm>
#include <bit>
#include <bitset>
#include <cassert>
#include <cstdint>
#include <map>
#include <functional>
#include <memory>
#include <numeric>
#include <random>
#include <vector>

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
std::vector<uint8_t> symmetry_position_active_cnt(kNumPositions);

// edge orientation
std::array<std::map<Vec3i, uint16_t>, kNumEdges> xyz_to_idx_orient; // [position_idx][xyz_orient] -> idx_orient (0 not flipped, 1 flipped)
std::array<std::array<Vec3i, 2>, kNumEdges> idx_to_xyz_orient; // [position_idx][idx_orient] -> xyz_orient

// these three lookups could be done if you do not want to do the one combined edge lookup
std::array<std::array<uint16_t, kNumRotations>, kNumOrientations> orientation_rotation; // [orientation][rotation] -> orientation
std::array<std::array<uint16_t, kNumOrientations>, kNumSymmetries> default_to_sym_orientation; // [sym][orientation] -> orientation
std::array<std::array<uint16_t, kNumOrientations>, kNumSymmetries> sym_to_default_orientation; // [sym][orientation] -> orientation
}


namespace edge {
// these lookup tables are all precomputed / are stored in a file
// needed for a rotation
std::vector<std::array<uint8_t, kNumRotations>> rotation_changes(kNumSymmetries); // [symmetry][rotation] -> rotation
std::vector<std::array<uint64_t, kNumRotations>> edge_positions(kNumPositions); // [position][rotation] -> (symmetry_change << 24) | position
std::vector<std::array<uint8_t, kNumSymmetries>> symmetry_changes(kNumSymmetryChange); // [symmetry_change][symmetry] -> symmetry, relative symmetry
std::vector<std::array<std::array<std::array<uint16_t, kNumRotations>, kNumSymmetries>, kNumOrientations>> edge_orientations(kNumSymmetries); // edge_orientations[symmetry][orientation][next_symmetry][rotation]


Vec3i RotateXYZPiece(const Vec3i& position, uint8_t rotation) {
    RotationRepresentations cur_rotation = index_to_rotation_representations[rotation];
    for (int i = 0; i < 3; i++) {
        if (cur_rotation.matrix[i][i] != 0) {
            // quarter slice moves
            if (cur_rotation.activate == 0 && position[i] == 0) {
                return position;
            }
            // quarter non slice moves
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
    uint8_t active_symmetries_map_cnt = 0;
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
    std::map<std::array<uint8_t, kNumSymmetries>, int> symmetry_changes_contains;
    symmetry_changes.clear();

    for (uint32_t sym_position = 0; sym_position < kNumPositions; sym_position++) {
        for (int sym_rotation = 0; sym_rotation < kNumRotations; sym_rotation++) {
            uint32_t position = symmetry_position_to_position[sym_position][0];
            uint32_t next_position = RotatePieces(position, sym_rotation);

            uint32_t next_sym_position = position_to_symmetry_position[next_position] & kPositionMask;
            uint8_t next_pos_acitve_symmetry = symmetry_position_active_symmetries[next_sym_position];
            uint32_t next_sym_change = position_to_symmetry_position[next_position] >> kPositionShift;

            std::array<uint8_t, kNumSymmetries> cur_symmmetry_changes;
            cur_symmmetry_changes.fill(uint8_t(-1));
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

    // TODO: do this with load from file
    SymmetryPositionInit();
    rotation_changes = RotationChangesInit();
    edge_positions = EdgePositionsInit();

    // edge orientation
    // ================
    for (int i = 0; i < kNumEdges; i++) {
        for (int j = 0; j < 2; j++) {
            Vec3i edge_pos = idx_to_xyz_pos[i];
            Vec3i edge_orient = {0, 0, 0};
            for (int k = 0; k < 3; k++) {
                if (edge_pos[k] == 0) { // there is only one zero
                    edge_orient[(k+j+1)%3] = edge_pos[(k+j+1)%3]; // the standard orientation is the next axis after the 0 element
                    idx_to_xyz_orient[i][j] = edge_orient;
                    xyz_to_idx_orient[i][edge_orient] = j;
                    break;
                }
            }
        }
    }

    // orientation rotation
    for (uint16_t i = 0; i < kNumOrientations; i++) {
        uint16_t orientation = i | (uint16_t(std::popcount(i)%2 == 1) << (kNumEdges-1)); // this is flipping the last bit to the correct place
        for (int rotation = 0; rotation < kNumRotations; rotation++) {
            uint16_t next_orientation = 0;
            for (int j = 0; j < kNumEdges; j++) {
                Vec3i edge_orient = idx_to_xyz_orient[j][(orientation>>j)&1];
                Vec3i next_edge_orient = edge_orient;
                uint16_t next_pos = idx_rotations[rotation][j];
                if (next_pos != j) { // changing this piece with the rotation
                    next_edge_orient = MatVecMul(index_to_rotation_representations[rotation].matrix, edge_orient);
                }
                next_orientation |= xyz_to_idx_orient[next_pos][next_edge_orient] << next_pos;
            }
            orientation_rotation[i][rotation] = next_orientation & (kNumOrientations-1);
        }
    }
    // LOG_EXTRA("orientation_rotation", orientation_rotation);

    std::array<uint16_t, kNumSymmetries> default_orientation_change;
    for (uint16_t i = 0; i < kNumOrientations; i++) {
        uint16_t orientation = i | (uint16_t(std::popcount(i)%2 == 1) << (kNumEdges-1)); // this is flipping the last bit to the correct place
        for (int sym = 0; sym < kNumSymmetries; sym++) {
            uint16_t next_orientation = 0;
            for (int j = 0; j < kNumEdges; j++) {
                Vec3i edge_orient = idx_to_xyz_orient[j][(orientation>>j)&1];
                uint16_t next_pos = position_symmetry_change[sym][j];
                Vec3i next_edge_orient = MatVecMul(idx_to_mat_symmetry[sym], edge_orient);;
                if ((next_orientation & xyz_to_idx_orient[next_pos][next_edge_orient] << next_pos) != 0) {
                    LOG_CRITICAL("WRONG");
                }
                next_orientation |= xyz_to_idx_orient[next_pos][next_edge_orient] << next_pos;
            }
            if (i == 0) {
                default_orientation_change[sym] = next_orientation;
            }
            next_orientation ^= default_orientation_change[sym];
            next_orientation &= kNumOrientations-1;
            default_to_sym_orientation[sym][i] = next_orientation;
            sym_to_default_orientation[sym][next_orientation] = i;
        }
    }
    // LOG_EXTRA("default_to_sym_orientation", default_to_sym_orientation);

    for (uint8_t symmetry = 0; symmetry < kNumSymmetries; symmetry++) {
        for (uint16_t orientation = 0; orientation < kNumOrientations; orientation++) {
            for (uint8_t next_symmetry = 0; next_symmetry < kNumSymmetries; next_symmetry++) {
                for (uint8_t rotation = 0; rotation < kNumRotations; rotation++) {
                    uint16_t default_orientation = sym_to_default_orientation[symmetry][orientation];
                    uint16_t next_orientation = orientation_rotation[default_orientation][rotation];
                    uint16_t next_orientation_sym = default_to_sym_orientation[next_symmetry][next_orientation];
                    edge_orientations[symmetry][orientation][next_symmetry][rotation] = next_orientation_sym;
                }
            }
        }
    }

    std::vector<uint8_t> symmetries_to_active_symmetries_cnt;
    for (std::array<uint8_t, kNumSymmetries>& cur : symmetries_to_active_symmetries) {
        uint8_t cnt = std::count(cur.begin(), cur.end(), 0);
        symmetries_to_active_symmetries_cnt.push_back(cnt);
    }
    for (int i = 0; i < kNumPositions; i++) {
        symmetry_position_active_cnt[i] = symmetries_to_active_symmetries_cnt[symmetry_position_active_symmetries[i]];
    }
}

// most important one!
void Rotate(uint32_t& position, uint8_t& symmetry, uint16_t& orientation, uint8_t rotation) {
    // change rotation relative to symmetry
    uint8_t sym_rotation = rotation_changes[symmetry][rotation];
    // position lookup (pos + symmetry change)
    uint64_t packed = edge_positions[position][sym_rotation];
    uint32_t symmetry_change = packed >> kPositionShift;
    position = packed & kPositionMask;
    // change symmetry abs
    uint8_t next_symmetry = symmetry_changes[symmetry_change][symmetry];
    // orientation lookup
    orientation = edge_orientations[symmetry][orientation][next_symmetry][rotation]; // this rotation is not correct
    symmetry = next_symmetry;
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


void TestEdgeOrientationPosition1() {
    uint32_t position = 0;
    uint8_t symmetry = 0;
    uint16_t orientation = 0;
    for (int i = 0; i < kNumRotations; i++) {
        LOG_EXTRA("Rotation:", index_to_rotation_representations[i].name);
        uint32_t pos = position;
        uint8_t sym = symmetry;
        uint16_t ori = orientation;
        edge::Rotate(pos, sym, ori, i);
        LOG_EXTRA("pos:", pos, "sym:", sym, "ori:", ori);
    }
}


void TestEdgeOrientationPosition2(int seed, int num_rotations) {
    uint32_t position = 0;
    uint8_t symmetry = 0;
    uint16_t orientation = 0;

    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> gen(0, kNumRotations-1);

    std::vector<int> random_rotations;
    for (int i = 0; i < num_rotations; i++) { // generate 10 rotations
        int rotation = gen(rng);
        random_rotations.push_back(rotation);
        LOG_EXTRA(index_to_rotation_representations[rotation].name);
    }

    std::vector<int> ori_sym(num_rotations);
    for (int i = 0; i < kNumSymmetries; i++) {
        uint32_t pos = position;
        uint8_t sym = symmetry;
        uint16_t ori = orientation;
        for (int j = 0; j < num_rotations; j++) {
            edge::Rotate(pos, sym, ori, edge::rotation_changes[i][random_rotations[j]]);
            if (i == 0) {
                ori_sym[j] = ori;
            }
            if (ori_sym[j] != ori) {
                LOG_EXTRA("i:", i, "j:", j, ori, ori_sym[j]);
                std::array<uint8_t, kNumSymmetries> cur_active_symmetries = symmetries_to_active_symmetries[symmetry_position_active_symmetries[pos]];
                LOG_EXTRA(cur_active_symmetries);
                for (int i = 0; i < kNumSymmetries; i++) {
                    if (cur_active_symmetries[i] == 0) {
                        LOG_ALL(i, ":", default_to_sym_orientation[i][ori]);
                    }
                }
            }
        }
    }
}


// reverse moves always results in same position
void TestEdge(int seed, int num_positions) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<uint32_t> gen_pos(0, kNumPositions-1);
    std::uniform_int_distribution<uint8_t> gen_sym(0, kNumSymmetries-1);
    std::uniform_int_distribution<uint16_t> gen_ori(0, kNumOrientations-1);
    std::uniform_int_distribution<uint8_t> gen_rotation(0, kNumRotations-1);

    for (int i = 0; i < num_positions; i++) {
        uint32_t pos = gen_pos(rng);
        uint8_t sym = symmetries_to_active_symmetries[symmetry_position_active_symmetries[pos]][gen_sym(rng)];
        uint16_t ori = gen_ori(rng);
        uint8_t rotation = gen_rotation(rng);
        uint32_t cur_pos = pos;
        uint8_t cur_sym = sym;
        uint16_t cur_ori = ori;
        edge::Rotate(cur_pos, cur_sym, cur_ori, rotation);
        edge::Rotate(cur_pos, cur_sym, cur_ori, rotation + (rotation % 2 == 0 ? 1 : -1));
        if (cur_pos != pos || cur_sym != sym || cur_ori != ori) {
            LOG_ERROR("pos:", cur_pos, "!=", pos);
            LOG_ERROR("sym:", cur_sym, "!=", sym);
            LOG_ERROR("ori:", cur_ori, "!=", ori);
        }
    }
}

void TestEdge2(int seed, int num_positions) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<uint32_t> gen_pos(0, kNumPositions-1);
    std::uniform_int_distribution<uint8_t> gen_rotation(0, kNumRotations-1);

    for (int i = 0; i < num_positions; i++) {
        uint32_t pos = gen_pos(rng);
        uint8_t rotation = gen_rotation(rng);
        std::vector<int> check_all_ori(kNumOrientations, 0);
        for (int ori = 0; ori < kNumOrientations; ori++) {
            uint32_t cur_pos = pos;
            uint8_t sym = 0;
            uint16_t cur_ori = ori;
            edge::Rotate(cur_pos, sym, cur_ori, rotation);
            if (check_all_ori[cur_ori] == 1) {
                LOG_CRITICAL("twice mapped to the same orientation");
            }
            check_all_ori[cur_ori] = 1;
        }
    }
}


void Precompute() {
    std::unique_ptr<std::bitset<uint64_t(kNumPositions)*kNumOrientations>> last = std::make_unique<std::bitset<uint64_t(kNumPositions)*kNumOrientations>>();
    std::unique_ptr<std::bitset<uint64_t(kNumPositions)*kNumOrientations>> curr = std::make_unique<std::bitset<uint64_t(kNumPositions)*kNumOrientations>>();
    std::unique_ptr<std::bitset<uint64_t(kNumPositions)*kNumOrientations>> next = std::make_unique<std::bitset<uint64_t(kNumPositions)*kNumOrientations>>();
    curr->set(0);
    uint64_t found = 1;
    uint64_t level = 0;
    uint64_t level_cnt = 1;
    while (found < uint64_t(kNumPositions)*kNumOrientations) {
        #if defined(__GNUC__) || defined(__clang__) // fast way to find the next element
        for (uint64_t i = curr->_Find_first(); i < curr->size(); i = curr->_Find_next(i)) {
        #else
        for (uint64_t i = 0; i < curr->size(); i++) {
            if (!(*curr)[i]) {
                continue;
            }
        #endif
            // get position
            uint32_t position = i % kNumPositions;
            uint16_t orientation = i / kNumPositions;
            for (int rotation = 0; rotation < kNumRotations; rotation++) {
                uint8_t symmetry = 0;
                uint32_t next_position = position;
                uint8_t next_symmetry = symmetry;
                uint16_t next_orientation = orientation;
                edge::Rotate(next_position, next_symmetry, next_orientation, rotation);
                uint64_t next_idx = (uint64_t(next_orientation)*kNumPositions)+next_position;
                if ((*last)[next_idx] ||
                    (*curr)[next_idx] ||
                    (*next)[next_idx]) {
                    continue;
                }
                next->set(next_idx);
                level++;
                found++;
                if (symmetry_position_active_cnt[next_position] > 1) {
                    for (int sym = 0; sym < kNumSymmetries; sym++) {
                        if (symmetries_to_active_symmetries[symmetry_position_active_symmetries[next_position]][sym] == 0) {
                            uint64_t next_idx_sym = (uint64_t(default_to_sym_orientation[sym][next_orientation])*kNumPositions)+next_position;
                            if ((*next)[next_idx_sym]) {
                                continue;
                            }
                            next->set(next_idx_sym);
                            level++;
                            found++;
                        }
                    }
                }
            }
        }
        LOG_ALL("Level", level_cnt, ":", level);
        std::swap(last, curr);
        std::swap(curr, next);
        next->reset();
        level_cnt++;
        level = 0;
    }
}


int main (int argc, char *argv[]) {
    // settings initialization
    Settings(argc, argv);
    RotationInit();
    edge::Init();
    LOG_INFO("Finished Edge");
    // TestEdgePosition();
    // LOG_INFO("Finished Test");
    // TestEdgeOrientationPosition1();
    // TestEdgeOrientationPosition2(0, 1000);
    // TestEdge(0, 100000);
    // TestEdge2(0, 10000);
    Precompute();
}
