#include <map>

#include "corner.hpp"
#include "utils.hpp"


namespace corner {
constexpr uint16_t kNumPos = Factorial(kNumCorners);
constexpr std::array<uint32_t, kNumCorners+1> kFactorials = []{
    std::array<uint32_t, kNumCorners+1> arr{};
    for (int i = 0; i <= kNumCorners; i++) {
        arr[i] = Factorial(i);
    }
    return arr;
}();


std::vector<uint16_t> position_change;


uint16_t PosPermToLehmerPos(const std::array<uint8_t, kNumPos>& pos_perm) {
    uint16_t result = 0;
    uint8_t available = ~uint8_t(0);

    for (int i = 0; i < kNumCorners; i++) {
        int idx = std::popcount(available & ((1U << pos_perm[i]) - 1));
        result += idx * kFactorials[kNumCorners-1 - i];
        available &= ~(1U << pos_perm[i]);
    }
    return result;
}


std::array<uint8_t, kNumCorners> LehmerPosToPosPerm(uint16_t lehmer_pos) {
    std::array<uint8_t, kNumCorners> pos_perm;
    uint8_t available = ~uint8_t(0);

    for (int i = kNumCorners-1; i >= 0; i--) {
        int idx = lehmer_pos / kFactorials[i];
        lehmer_pos %= kFactorials[i];

        uint8_t mask = available;
        for (int k = 0; k < idx; k++) {
            mask &= mask - 1;
        }
        int bit = std::countr_zero(mask);

        pos_perm[kNumCorners-1 - i] = bit;
        available &= ~(1U << bit);
    }
    return pos_perm;
}


void InitXYZPos(std::map<Vec3i, uint8_t, std::greater<>>& xyz_to_idx_pos,
                std::array<Vec3i, kNumCorners>& idx_to_xyz_pos) {
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


void InitPositionChange() {
}


void Init() {
    // piece position
    std::map<Vec3i, uint8_t, std::greater<>> xyz_to_idx_pos;
    std::array<Vec3i, kNumCorners> idx_to_xyz_pos;
    InitXYZPos(xyz_to_idx_pos, idx_to_xyz_pos);

    // idx piece rotation
    std::array<std::array<uint8_t, kNumCorners>, kNumRot> idx_piece_rot;
    for (uint8_t rot = 0; rot < kNumRot; rot++) {
        for (int i = 0; i < kNumCorners; i++) {
            idx_piece_rot[rot][i] = xyz_to_idx_pos[RotateXYZPiece(idx_to_xyz_pos[i], rot)];
        }
    }

    // // initial starting protrution
    // std::array<Vec3i, kNumCorners> protrution;
    // int cnt = 0;
    // for (int i = 0; i >= -1; i--) {
    //     for (int j = 0; j >= -1; j--) {
    //         for (int k = 0; k >= -1; k--) {
    //             protrution[cnt++] = {i, j, k};
    //         }
    //     }
    // }

    LoadOrGenerate("[5/7] Corner Position Change", [](){InitPositionChange();}, "corner_position_change.bin", position_change, kNumPos);
}
}
