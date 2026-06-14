#include <map>

#include "corner.hpp"
#include "utils.hpp"


namespace corner {
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


void Init() {
    // piece position
    std::map<Vec3i, uint8_t, std::greater<>> xyz_to_idx_pos;
    std::array<Vec3i, kNumCorners> idx_to_xyz_pos;
    InitXYZPos(xyz_to_idx_pos, idx_to_xyz_pos);

    // idx piece rotation
    std::array<std::array<uint8_t, kNumCorners>, kNumRot> idx_piece_rot;
    for (uint8_t rot = 0; rot < kNumRot; rot++) {
        for (int i = 0; i < kNumEdges; i++) {
            idx_piece_rot[rot][i] = xyz_to_idx_pos[RotateXYZPiece(idx_to_xyz_pos[i], rot)];
        }
    }
}
}
