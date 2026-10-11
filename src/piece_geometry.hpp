#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <map>

#include "rotation.hpp"
#include "utils.hpp"


// shared piece position and symmetry construction used by the edge, corner
// and position_input modules (pure functions, cheap to rebuild)
namespace piece_geometry {


template <std::size_t N>
inline constexpr std::array<uint32_t, N+1> kFactorials = []{
    std::array<uint32_t, N+1> arr{};
    for (std::size_t i = 0; i <= N; i++) {
        arr[i] = uint32_t(Factorial(int(i)));
    }
    return arr;
}();


// rotate a grid position with a rotation (unchanged if the position is not
// part of the rotated layer)
inline Vec3i RotateXYZPiece(const Vec3i& pos, uint8_t rot) {
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


// edge positions: all grid positions with exactly two nonzero coordinates
inline void InitXYZPos(std::map<Vec3i, uint8_t, std::greater<>>& xyz_to_idx_pos,
                       std::array<Vec3i, 12>& idx_to_xyz_pos) {
    uint8_t cnt = 0;
    for (int i = 1; i >= -1; i--) {
        for (int j = 1; j >= -1; j--) {
            for (int k = 1; k >= -1; k--) {
                if (int(i != 0) + int(j != 0) + int(k != 0) == 2) {
                    xyz_to_idx_pos[{i, j, k}] = cnt;
                    idx_to_xyz_pos[cnt++] = {i, j, k};
                }
            }
        }
    }
}


// corner positions: all grid positions with three nonzero coordinates
inline void InitXYZPos(std::map<Vec3i, uint8_t, std::greater<>>& xyz_to_idx_pos,
                       std::array<Vec3i, 8>& idx_to_xyz_pos) {
    uint8_t cnt = 0;
    for (int i = 1; i >= -1; i--) {
        for (int j = 1; j >= -1; j--) {
            for (int k = 1; k >= -1; k--) {
                if (int(i != 0) + int(j != 0) + int(k != 0) == 3) {
                    xyz_to_idx_pos[{i, j, k}] = cnt;
                    idx_to_xyz_pos[cnt++] = {i, j, k};
                }
            }
        }
    }
}


// the 48 cube symmetries as signed permutation matrices
inline void InitMatSym(std::array<Mat3i, 48>& idx_to_mat_sym, std::map<Mat3i, uint8_t>& mat_to_idx_sym) {
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


inline Vec3i Vec3iAbs(const Vec3i& vec3) {
    return {std::abs(vec3[0]), std::abs(vec3[1]), std::abs(vec3[2])};
}


// lehmer code of an edge position permutation (piece -> slot)
inline uint32_t PosPermToLehmerPos(const std::array<uint8_t, 12>& pos_perm) {
    uint32_t result = 0;
    uint16_t available = (1<<12) - 1; // 12 bits set = {0..11} available

    for (int i = 0; i < 12; i++) {
        // Count how many available elements are less than perm[i]
        uint16_t below = available & ((1U << pos_perm[i]) - 1);
        int idx = std::popcount(below);

        result += idx * kFactorials<12>[11 - i];
        available &= ~(1U << pos_perm[i]);
    }
    return result;
}


inline std::array<uint8_t, 12> LehmerPosToPosPerm(uint32_t lehmer_pos) {
    std::array<uint8_t, 12> pos_perm;
    uint16_t available = (1<<12) - 1; // 12 bits set = {0..11} available

    for (int i = 11; i >= 0; i--) {
        int idx = lehmer_pos / kFactorials<12>[i];
        lehmer_pos %= kFactorials<12>[i];

        // Find idx-th set bit in available
        uint16_t mask = available;
        for (int k = 0; k < idx; k++) {
            mask &= mask - 1; // clear lowest set bit
        }
        int bit = std::countr_zero(mask);

        pos_perm[11 - i] = uint8_t(bit); // NOLINT
        available &= ~(1U << bit);
    }
    return pos_perm;
}


// lehmer code of a corner position permutation (piece -> slot)
inline uint16_t PosPermToLehmerPos(const std::array<uint8_t, 8>& pos_perm) {
    uint16_t result = 0;
    uint8_t available = ~uint8_t(0);

    for (int i = 0; i < 8; i++) {
        int idx = std::popcount(available & ((1U << pos_perm[i]) - 1));
        result += idx * kFactorials<8>[7 - i];
        available &= ~(1U << pos_perm[i]);
    }
    return result;
}


// apply a symmetry to an edge position permutation (conjugation):
// res[sym_map[i]] = sym_map[pos_perm[i]]
inline std::array<uint8_t, 12> SymmetryPositionRotation(const std::array<std::array<uint8_t, 12>, 48>& idx_piece_sym,
                                                        const std::array<uint8_t, 12>& pos_perm, uint8_t sym) {
    std::array<uint8_t, 12> res{};
    for (int i = 0; i < 12; i++) {
        res[idx_piece_sym[sym][i]] = idx_piece_sym[sym][pos_perm[i]];
    }
    return res;
}


// corner orientation: lehmer code over the axis index of the first 7 slots
inline uint16_t OrientPermToLehmerOrient(std::map<Vec3i, uint8_t>& xyz_to_idx_orient,
                                         const std::array<Vec3i, 8>& orient_perm) {
    uint16_t result = 0;
    for (int i = 0; i < 7; i++) {
        result *= 3;
        result += xyz_to_idx_orient[orient_perm[i]];
    }
    return result;
}


// the two orientation vectors of every edge position; j=0 is the standard
// orientation ("the next axis after the zero element")
inline void InitXYZOrient(const std::array<Vec3i, 12>& idx_to_xyz_pos,
                          std::array<std::array<Vec3i, 2>, 12>& idx_to_xyz_orient,
                          std::array<std::map<Vec3i, uint16_t>, 12>& xyz_to_idx_orient) {
    for (int i = 0; i < 12; i++) {
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
}


struct EdgeSymmetryData {
    std::map<Vec3i, uint8_t, std::greater<>> xyz_to_idx_pos;
    std::array<Vec3i, 12> idx_to_xyz_pos;
    std::array<Mat3i, 48> idx_to_mat_sym;
    std::map<Mat3i, uint8_t> mat_to_idx_sym;
    std::array<std::array<uint8_t, 12>, kNumRot> idx_piece_rot;
    std::array<std::array<uint8_t, 12>, 48> idx_piece_sym;
    std::array<std::array<uint8_t, 48>, 48> sym_mul_sym;
    std::array<uint8_t, 48> sym_trans;
    std::array<std::array<Vec3i, 2>, 12> idx_to_xyz_orient;
    std::array<std::map<Vec3i, uint16_t>, 12> xyz_to_idx_orient;
};


inline EdgeSymmetryData BuildEdgeSymmetryData() {
    EdgeSymmetryData data;
    InitXYZPos(data.xyz_to_idx_pos, data.idx_to_xyz_pos);
    InitMatSym(data.idx_to_mat_sym, data.mat_to_idx_sym);
    for (uint8_t rot = 0; rot < kNumRot; rot++) {
        for (int i = 0; i < 12; i++) {
            data.idx_piece_rot[rot][i] = data.xyz_to_idx_pos[RotateXYZPiece(data.idx_to_xyz_pos[i], rot)];
        }
    }
    for (uint8_t sym = 0; sym < 48; sym++) {
        for (int i = 0; i < 12; i++) {
            data.idx_piece_sym[sym][i] = data.xyz_to_idx_pos[MatVecMul(data.idx_to_mat_sym[sym], data.idx_to_xyz_pos[i])];
        }
    }
    for (int i = 0; i < 48; i++) {
        for (int j = 0; j < 48; j++) {
            data.sym_mul_sym[i][j] = data.mat_to_idx_sym[MatMul(data.idx_to_mat_sym[i], data.idx_to_mat_sym[j])];
        }
    }
    for (int i = 0; i < 48; i++) {
        data.sym_trans[i] = data.mat_to_idx_sym[MatTrans(data.idx_to_mat_sym[i])];
    }
    InitXYZOrient(data.idx_to_xyz_pos, data.idx_to_xyz_orient, data.xyz_to_idx_orient);
    return data;
}


struct CornerPositionData {
    std::map<Vec3i, uint8_t, std::greater<>> xyz_to_idx_pos;
    std::array<Vec3i, 8> idx_to_xyz_pos;
    std::array<std::array<uint8_t, 8>, kNumRot> idx_piece_rot;
};


inline CornerPositionData BuildCornerPositionData() {
    CornerPositionData data;
    InitXYZPos(data.xyz_to_idx_pos, data.idx_to_xyz_pos);
    for (uint8_t rot = 0; rot < kNumRot; rot++) {
        for (int i = 0; i < 8; i++) {
            data.idx_piece_rot[rot][i] = data.xyz_to_idx_pos[RotateXYZPiece(data.idx_to_xyz_pos[i], rot)];
        }
    }
    return data;
}


} // namespace piece_geometry
