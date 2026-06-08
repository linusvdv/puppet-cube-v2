#include <algorithm>
#include <cstdint>
#include <map>
#include <numeric>
#include <unordered_map>

#include "logger.hpp"
#include "utils.hpp"


namespace edge {
constexpr uint8_t kNumSym = Factorial(3) * (1<<3);
constexpr uint32_t kNumLehmerPos = Factorial(12);
constexpr uint32_t kNumPos = 9985968;
constexpr uint32_t kPosShift = 24;
constexpr uint32_t kPosMask = (1<<kPosShift)-1;
constexpr uint16_t kNumSymChange = 981; // this is unfortunatly more than 256 (which would fit in uint8_t and could therefore be packed in a uint32_t with the position)
constexpr uint16_t kNumOrient = 1 << (kNumEdges-1);

constexpr std::array<uint32_t, kNumEdges+1> kFactorials = []{
    std::array<uint32_t, kNumEdges+1> arr{};
    for (int i = 0; i <= kNumEdges; i++) {
        arr[i] = Factorial(i);
    }
    return arr;
}();


// this is the data used for rotation
std::vector<std::array<uint8_t, kNumRot>> rotation_change;
std::vector<std::array<uint64_t, kNumRot>> position_change;
std::vector<std::array<uint16_t, kNumSym>> symmetry_change;
std::vector<std::array<std::array<uint16_t, kNumRot>, kNumSym>> orientation_change;

void InitXYZPos(std::map<Vec3i, uint8_t, std::greater<>>& xyz_to_idx_pos, std::array<Vec3i, kNumEdges>& idx_to_xyz_pos) {
    uint8_t cnt = 0;
    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            for (int k = -1; k <= 1; k++) {
                if (int(i != 0) + int(j != 0) + int(k != 0) == 2) {
                    xyz_to_idx_pos[{i, j, k}] = cnt;
                    idx_to_xyz_pos[cnt++] = {i, j, k};
                }
            }
        }
    }
}


void InitMatSym(std::array<Mat3i, kNumSym>& idx_to_mat_sym, std::map<Mat3i, uint8_t>& mat_to_idx_sym) {
    std::array<int, 3> perm = {0, 1, 2};
    uint8_t cnt = 0;
    do {
        for (int flipsign = 0; flipsign < (1<<3); flipsign++) {
            Mat3i mat = {};
            for (int i = 0; i < 3; i++) {
                mat[i][perm[i]] = ((flipsign >> i) & 1) == 0 ? 1 : -1;
            }
            idx_to_mat_sym[cnt] = mat;
            mat_to_idx_sym[mat] = cnt++;
        }
    } while (std::next_permutation(perm.begin(), perm.end()));
}


Vec3i RotateXYZPiece(const Vec3i& pos, uint8_t rot) {
    RotRep rot_rep = idx_to_rot_rep[rot];
    for (int i = 0; i < 3; i++) {
        if (rot_rep.matrix[i][i] != 0) {
            // quarter slice moves
            if (rot_rep.activate == 0 && pos[i] == 0) {
                return pos;
            }
            // quarter non slice moves
            if (rot_rep.activate != 0 &&
                rot_rep.activate != pos[i]) {
                return pos;
            }
        }
    }
    return MatVecMul(rot_rep.matrix, pos);
}


void InitRotationChange(const std::array<Mat3i, kNumSym>& idx_to_mat_sym) {
    rotation_change = std::vector<std::array<uint8_t, kNumRot>>(kNumSym);
    for (uint8_t sym = 0; sym < kNumSym; sym++) {
        for (uint8_t rot = 0; rot < kNumRot; rot++) {
            RotRep rot_rep = idx_to_rot_rep[rot];
            Mat3i mat = MatMul(idx_to_mat_sym[sym],
                MatMul(rot_rep.matrix,
                MatTrans(idx_to_mat_sym[sym])));
            int activate = rot_rep.activate;
            for (int i = 0; i < 3; i++) {
                if (rot_rep.matrix[i][i] == 1) {
                    for (int j = 0; j < 3; j++) {
                        if (idx_to_mat_sym[sym][j][i] != 0) {
                            activate *= idx_to_mat_sym[sym][j][i];
                        }
                    }
                }
            }
            rotation_change[sym][rot] = mat_to_rot_rep[{mat, activate}].index;
        }
    }
}


uint32_t PosPermToLehmerPos(const std::array<uint8_t, kNumEdges>& pos_perm) {
    uint32_t result = 0;
    uint16_t available = (1<<kNumEdges) - 1; // 12 bits set = {0..11} available

    for (int i = 0; i < kNumEdges; i++) {
        // Count how many available elements are less than perm[i]
        uint16_t below = available & ((1U << pos_perm[i]) - 1);
        int idx = std::popcount(below);

        result += idx * kFactorials[kNumEdges-1 - i];
        available &= ~(1U << pos_perm[i]);
    }
    return result;
}


std::array<uint8_t, kNumEdges> LehmerPosToPosPerm(uint32_t lehmer_pos) {
    std::array<uint8_t, kNumEdges> pos_perm;
    uint16_t available = (1<<kNumEdges) - 1; // 12 bits set = {0..11} available

    for (int i = kNumEdges-1; i >= 0; i--) {
        int idx = lehmer_pos / kFactorials[i];
        lehmer_pos %= kFactorials[i];

        // Find idx-th set bit in available
        uint16_t mask = available;
        for (int k = 0; k < idx; k++) {
            mask &= mask - 1; // clear lowest set bit
        }
        int bit = std::countr_zero(mask);

        pos_perm[kNumEdges-1 - i] = bit;
        available &= ~(1U << bit);
    }
    return pos_perm;
}


std::array<uint8_t, kNumEdges> SymmetryPositionRotation(const std::array<std::array<uint8_t, kNumEdges>, kNumSym>& idx_piece_sym,
                                                        const std::array<uint8_t, kNumEdges>& pos_perm, uint8_t sym) {
    std::array<uint8_t, kNumEdges> res{};
    for (int i = 0; i < kNumEdges; i++) {
        res[idx_piece_sym[sym][i]] = idx_piece_sym[sym][pos_perm[i]];
    }
    return res;
}


uint32_t RotatePieces(const std::array<std::array<uint8_t, kNumEdges>, kNumRot>& idx_piece_rot, uint32_t lehmer_pos, uint8_t rot) {
    std::array<uint8_t, kNumEdges> pos_perm = LehmerPosToPosPerm(lehmer_pos);
    for (uint8_t& edge_piece : pos_perm) {
        edge_piece = idx_piece_rot[rot][edge_piece];
    }
    return PosPermToLehmerPos(pos_perm);
}


void InitPositionChangeSymmetryChange(const std::array<std::array<uint8_t, kNumEdges>, kNumRot>& idx_piece_rot,
                                      const std::array<std::array<uint8_t, kNumEdges>, kNumSym>& idx_piece_sym,
                                      const std::array<std::array<uint8_t, kNumSym>, kNumSym>& sym_mul_sym,
                                      const std::array<uint8_t, kNumSym>& sym_trans) {
    // create lehmer position
    std::vector<std::array<uint32_t, kNumSym>> pos_to_lehmer_pos(kNumPos); // [symmetry_position][symmetry] -> position
    std::vector<uint32_t> lehmer_pos_to_pos(kNumLehmerPos, uint32_t(-1));

    // active symmetry
    std::vector<uint8_t> sym_pos_active_sym(kNumPos);
    std::map<std::array<uint8_t, kNumSym>, uint8_t> active_sym_map;
    std::vector<std::array<uint8_t, kNumSym>> sym_to_active_sym;

    // precompute
    std::array<uint8_t, kNumEdges> pos_perm;
    std::iota(pos_perm.begin(), pos_perm.end(), 0);
    uint32_t cnt = 0;
    uint32_t progress = 0;
    uint8_t active_sym_map_cnt = 0;
    do {
        uint32_t lehmer_pos_default = PosPermToLehmerPos(pos_perm);
        if (lehmer_pos_to_pos[lehmer_pos_default] != uint32_t(-1)) {
            continue;
        }
        std::array<uint8_t, kNumSym> cur_active_sym;
        std::unordered_map<uint32_t, uint8_t> lehmer_pos_to_active_sym;
        for (uint32_t sym = 0; sym < kNumSym; sym++) {
            uint32_t lehmer_pos = PosPermToLehmerPos(SymmetryPositionRotation(idx_piece_sym, pos_perm, sym));
            if (lehmer_pos_to_active_sym.contains(lehmer_pos)) {
                cur_active_sym[sym] = lehmer_pos_to_active_sym[lehmer_pos];
            }
            else {
                lehmer_pos_to_active_sym[lehmer_pos] = sym;
                cur_active_sym[sym] = sym;
            }

            if (lehmer_pos_to_pos[lehmer_pos] != uint32_t(-1)) {
                continue;
            }

            progress++;
            if (progress % 1000000 == 0) {
                LOG_EXTRA(progress, "/", kNumLehmerPos);
            }
            lehmer_pos_to_pos[lehmer_pos] = cnt | (sym << kPosShift);
            pos_to_lehmer_pos[cnt][sym] = lehmer_pos;
        }
        if (active_sym_map.contains(cur_active_sym)) {
            sym_pos_active_sym[lehmer_pos_to_pos[lehmer_pos_default]&kPosMask] = active_sym_map[cur_active_sym];
        }
        else {
            active_sym_map[cur_active_sym] = active_sym_map_cnt;

            sym_to_active_sym.push_back(cur_active_sym);

            sym_pos_active_sym[lehmer_pos_to_pos[lehmer_pos_default]&kPosMask] = active_sym_map_cnt;
            active_sym_map_cnt++;
        }
        cnt++;
    } while (std::next_permutation(pos_perm.begin(), pos_perm.end()));
    if (cnt != kNumPos) {
        LOG_CRITICAL("Lehmer pos count not correct!");
    }


    int sym_change_cnt = 0;
    std::map<std::array<uint16_t, kNumSym>, int> sym_change_map;
    symmetry_change = std::vector<std::array<uint16_t, kNumSym>>(kNumSymChange);
    position_change = std::vector<std::array<uint64_t, kNumRot>>(kNumPos);
    LOG_MEMORY();

    for (uint32_t pos = 0; pos < kNumPos; pos++) {
        for (uint8_t rot = 0; rot < kNumRot; rot++) {
            uint32_t lehmer_pos = pos_to_lehmer_pos[pos][0];
            uint32_t next_lehmer_pos = RotatePieces(idx_piece_rot, lehmer_pos, rot);

            uint32_t packed = lehmer_pos_to_pos[next_lehmer_pos];
            uint32_t next_sym_position = packed & kPosMask;
            uint32_t next_sym_change = packed >> kPosShift;
            uint8_t next_pos_acitve_symmetry = sym_pos_active_sym[next_sym_position];

            std::array<uint16_t, kNumSym> cur_sym_change;
            cur_sym_change.fill(uint16_t(-1));
            for (int sym = 0; sym < kNumSym; sym++) {
                cur_sym_change[sym] = sym_to_active_sym[next_pos_acitve_symmetry][sym_mul_sym[next_sym_change][sym]];
            }

            if (!sym_change_map.contains(cur_sym_change)) {
                symmetry_change[sym_change_cnt] = cur_sym_change;
                sym_change_map[cur_sym_change] = sym_change_cnt;
                sym_change_cnt++;
            }
            position_change[pos][rot] = (uint64_t(sym_change_map[cur_sym_change]) << kPosShift) | next_sym_position;
        }
        if (pos % 100000 == 0) {
            LOG_EXTRA(pos, "/", kNumPos);
        }
    }

    // the symmetry_change shall also include the relative symmetry change and not only the absolute
    for (std::array<uint16_t, kNumSym>& cur_sym_change : symmetry_change) {
        for (int sym = 0; sym < kNumSym; sym++) {
            // C = B*A^-1 = B * A^T
            uint16_t relative_sym = sym_mul_sym[cur_sym_change[sym]][sym_trans[sym]];
            cur_sym_change[sym] |= relative_sym << 8;
        }
    }

    LOG_INFO("Symmetry Change CNT:", sym_change_cnt);
}


void InitOrientationChange(const std::array<Vec3i, kNumEdges>& idx_to_xyz_pos,
                           const std::array<Mat3i, kNumSym>& idx_to_mat_sym,
                           const std::array<std::array<uint8_t, kNumEdges>, kNumRot>& idx_piece_rot,
                           const std::array<std::array<uint8_t, kNumEdges>, kNumSym>& idx_piece_sym) {
    std::array<std::map<Vec3i, uint16_t>, kNumEdges> xyz_to_idx_orient;
    std::array<std::array<Vec3i, 2>, kNumEdges> idx_to_xyz_orient;
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

    std::array<std::array<uint16_t, kNumRot>, kNumOrient> orient_rot;
    for (uint16_t i = 0; i < kNumOrient; i++) {
        uint16_t orient = i | (uint16_t(std::popcount(i)%2 == 1) << (kNumEdges-1)); // this is flipping the last bit to the correct place
        for (int rot = 0; rot < kNumRot; rot++) {
            uint16_t next_orient = 0;
            for (uint8_t j = 0; j < kNumEdges; j++) {
                Vec3i edge_orient = idx_to_xyz_orient[j][(orient>>j)&1];
                Vec3i next_edge_orient = edge_orient;
                uint8_t next_pos = idx_piece_rot[rot][j];
                if (next_pos != j) { // changing this piece with the rotation
                    next_edge_orient = MatVecMul(idx_to_rot_rep[rot].matrix, edge_orient);
                }
                next_orient |= xyz_to_idx_orient[next_pos][next_edge_orient] << next_pos;
            }
            orient_rot[i][rot] = next_orient & (kNumOrient-1);
        }
    }

    std::array<uint16_t, kNumSym> default_orientation_change;
    std::array<std::array<uint16_t, kNumOrient>, kNumSym> default_to_sym_orien;
    for (uint16_t i = 0; i < kNumOrient; i++) {
        uint16_t orientation = i | (uint16_t(std::popcount(i)%2 == 1) << (kNumEdges-1)); // this is flipping the last bit to the correct place
        for (int sym = 0; sym < kNumSym; sym++) {
            uint16_t next_orientation = 0;
            for (int j = 0; j < kNumEdges; j++) {
                Vec3i edge_orient = idx_to_xyz_orient[j][(orientation>>j)&1];
                uint16_t next_pos = idx_piece_sym[sym][j];
                Vec3i next_edge_orient = MatVecMul(idx_to_mat_sym[sym], edge_orient);;
                if ((next_orientation & xyz_to_idx_orient[next_pos][next_edge_orient] << next_pos) != 0) {
                    LOG_CRITICAL("WRONG");
                }
                next_orientation |= xyz_to_idx_orient[next_pos][next_edge_orient] << next_pos;
            }
            if (i == 0) {
                default_orientation_change[sym] = next_orientation;
            }
            next_orientation ^= default_orientation_change[sym];
            next_orientation &= kNumOrient-1;
            default_to_sym_orien[sym][i] = next_orientation;
        }
    }


    orientation_change.assign(kNumOrient, {});
    for (uint16_t orientation = 0; orientation < kNumOrient; orientation++) {
        for (uint8_t rel_sym = 0; rel_sym < kNumSym; rel_sym++) {
            for (uint8_t rot = 0; rot < kNumRot; rot++) {
                uint16_t next_orient = orient_rot[orientation][rot];
                uint16_t next_orient_sym = default_to_sym_orien[rel_sym][next_orient];
                orientation_change[orientation][rel_sym][rot] = next_orient_sym;
            }
        }
    }
}


void Init() {
    // piece position
    std::map<Vec3i, uint8_t, std::greater<>> xyz_to_idx_pos;
    std::array<Vec3i, kNumEdges> idx_to_xyz_pos;
    InitXYZPos(xyz_to_idx_pos, idx_to_xyz_pos);

    // symmetries
    std::array<Mat3i, kNumSym> idx_to_mat_sym;
    std::map<Mat3i, uint8_t> mat_to_idx_sym;
    InitMatSym(idx_to_mat_sym, mat_to_idx_sym);

    // idx piece rotation
    std::array<std::array<uint8_t, kNumEdges>, kNumRot> idx_piece_rot;
    for (uint8_t rot = 0; rot < kNumRot; rot++) {
        for (int i = 0; i < kNumEdges; i++) {
            idx_piece_rot[rot][i] = xyz_to_idx_pos[RotateXYZPiece(idx_to_xyz_pos[i], rot)];
        }
    }
    // idx piece symmetry
    std::array<std::array<uint8_t, kNumEdges>, kNumSym> idx_piece_sym;
    for (uint8_t sym = 0; sym < kNumSym; sym++) {
        for (int i = 0; i < kNumEdges; i++) {
            idx_piece_sym[sym][i] = xyz_to_idx_pos[MatVecMul(idx_to_mat_sym[sym], idx_to_xyz_pos[i])];
        }
    }
    // symmetry muliply symmetry
    std::array<std::array<uint8_t, kNumSym>, kNumSym> sym_mul_sym;
    for (int i = 0; i < kNumSym; i++) {
        for (int j = 0; j < kNumSym; j++) {
            sym_mul_sym[i][j] = mat_to_idx_sym[MatMul(idx_to_mat_sym[i], idx_to_mat_sym[j])];
        }
    }
    // transpose symmetry
    std::array<uint8_t, kNumSym> sym_trans;
    for (int i = 0; i < kNumSym; i++) {
        sym_trans[i] = mat_to_idx_sym[MatTrans(idx_to_mat_sym[i])];
    }

    LoadOrGenerate("[? / ?] Rotation Change", [&](){InitRotationChange(idx_to_mat_sym);},
                   "edge_rotation_change.bin", rotation_change, kNumSym);
    LoadMultipleOrGenerate("[? / ?] Position Change, Symmetry Change", [&](){InitPositionChangeSymmetryChange(idx_piece_rot, idx_piece_sym, sym_mul_sym, sym_trans);},
                           "edge_position_change.bin", position_change, kNumPos,
                           "edge_symmetry_change.bin", symmetry_change, kNumSymChange);
    LoadOrGenerate("[? / ?] Orientation Change", [&](){InitOrientationChange(idx_to_xyz_pos, idx_to_mat_sym, idx_piece_rot, idx_piece_sym);},
                   "edge_orientation_change.bin", orientation_change, kNumOrient);
}
}
