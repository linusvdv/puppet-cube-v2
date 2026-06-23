#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <numeric>
#include <queue>

#include "corner.hpp"
#include "logger.hpp"
#include "rotation.hpp"
#include "utils.hpp"


namespace corner {
constexpr std::array<uint32_t, kNumCorners+1> kFactorials = []{
    std::array<uint32_t, kNumCorners+1> arr{};
    for (int i = 0; i <= kNumCorners; i++) {
        arr[i] = Factorial(i);
    }
    return arr;
}();


std::vector<std::array<uint16_t, kNumRot>> position_change;
std::vector<std::array<uint16_t, kNumRot>> orientation_change;
std::vector<std::array<uint64_t, kNumOrient>> heuristic;


uint64_t GetHeuristic(uint16_t pos, uint16_t orient) {
    return heuristic[pos][orient];
}

void Rotate(uint16_t& pos, uint16_t& orient, uint8_t rot) {
    pos = position_change[pos][rot];
    orient = orientation_change[orient][rot];
}


uint16_t OrientPermToLehmerOrient(std::map<Vec3i, uint8_t>& xyz_to_idx_orient,
                                  const std::array<Vec3i, kNumCorners>& orient_perm) {
    uint16_t result = 0;
    for (int i = 0; i < kNumCorners-1; i++) {
        result *= 3;
        result += xyz_to_idx_orient[orient_perm[i]];
    }
    return result;
}


uint16_t PosPermToLehmerPos(const std::array<uint8_t, kNumCorners>& pos_perm) {
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


void InitXYZPos(std::map<Vec3i, uint8_t>& xyz_to_idx_pos,
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


Vec3i Vec3iAbs(const Vec3i& vec3) {
    return {std::abs(vec3[0]), std::abs(vec3[1]), std::abs(vec3[2])};
}


void InitPositionChange(const std::array<std::array<uint8_t, kNumCorners>, kNumRot>& idx_piece_rot) {
    position_change.assign(kNumPos, {});

    std::array<uint8_t, kNumCorners> pos_perm;
    std::iota(pos_perm.begin(), pos_perm.end(), 0);
    uint16_t lehmer_pos = 0;
    do {
        for (uint8_t rot = 0; rot < kNumRot; rot++) {
            std::array<uint8_t, kNumCorners> next_pos_perm = pos_perm;
            for (uint8_t& corner_piece : next_pos_perm) {
                corner_piece = idx_piece_rot[rot][corner_piece];
            }
            uint16_t next_lehmer_pos = PosPermToLehmerPos(next_pos_perm);
            position_change[lehmer_pos][rot] = next_lehmer_pos;
        }
        lehmer_pos++;
    } while (std::next_permutation(pos_perm.begin(), pos_perm.end()));
}


void InitOrinetationChange(const std::array<std::array<uint8_t, kNumCorners>, kNumRot>& idx_piece_rot) {
    orientation_change.assign(kNumOrient, {});

    // piece orientation
    constexpr int kNumDiffOrient = 3;
    std::map<Vec3i, uint8_t> xyz_to_idx_orient;
    std::array<Vec3i, kNumDiffOrient> idx_to_xyz_orient;
    for (int i = 0; i < kNumDiffOrient; i++) {
        Vec3i orient{};
        orient[i] = 1;
        idx_to_xyz_orient[i] = orient;
        xyz_to_idx_orient[orient] = i;
    }

    std::array<Vec3i, kNumCorners> orient_perm;
    orient_perm.fill({1, 0, 0});

    // bfs
    std::set<uint16_t> visited;
    std::queue<std::array<Vec3i, kNumCorners>> next;
    next.push(orient_perm);
    visited.insert(OrientPermToLehmerOrient(xyz_to_idx_orient, orient_perm));
    while (!next.empty()) {
        orient_perm = next.front();
        next.pop();
        uint16_t lehmer_orient = OrientPermToLehmerOrient(xyz_to_idx_orient, orient_perm);
        for (uint8_t rot = 0; rot < kNumRot; rot++) {
            std::array<Vec3i, kNumCorners> next_orient_perm;
            for (int i = 0; i < kNumCorners; i++) {
                if (idx_piece_rot[rot][i] == i) {
                    next_orient_perm[i] = orient_perm[i];
                    continue;
                }
                next_orient_perm[idx_piece_rot[rot][i]] = Vec3iAbs(MatVecMul(idx_to_rot_rep[rot].matrix, orient_perm[i]));
            }
            uint16_t next_lehmer_orient = OrientPermToLehmerOrient(xyz_to_idx_orient, next_orient_perm);
            if (visited.insert(next_lehmer_orient).second) {
                next.push(next_orient_perm);
            }
            orientation_change[lehmer_orient][rot] = next_lehmer_orient;
        }
    }
    if (visited.size() != kNumOrient) {
        LOG_CRITICAL("Corner Orientation Precomputation wrong size", visited.size(), "/", kNumOrient);
    }
}


bool IsLegal (const std::array<Vec3i, kNumCorners>& idx_to_xyz_pos, const std::array<Vec3i, kNumCorners>& protrution) {
    std::array<std::array<std::array<std::array<int, 3>, 3>, 3>, 2> protrution_cnt{};
    for (int i = 0; i < kNumCorners; i++) {
        Vec3i xyz_pos = idx_to_xyz_pos[i];
        for (int j = 0; j < 3; j++) {
            if (protrution[i][j] == 0) {
                continue;
            }
            std::array<int, 2> dir1 = {xyz_pos[(j+1)%3]+1, 1};
            std::array<int, 2> dir2 = {xyz_pos[(j+2)%3]+1, 1};
            for (int cnt1 = 0; cnt1 <= int(protrution[i][(j+1)%3]==0); cnt1++) {
                for (int cnt2 = 0; cnt2 <= int(protrution[i][(j+2)%3]==0); cnt2++) {
                    protrution_cnt[int(protrution[i][j]>0)][j][dir1[cnt1]][dir2[cnt2]]++;
                }
            }
        }
    }
    return std::count_if(protrution_cnt.front().front().front().begin(),
                         protrution_cnt.back().back().back().end(),
                         [](int elm){return elm > 1;}) == 0;
}


struct HeuristicData {
    uint16_t pos;
    uint16_t orient;
    std::array<Vec3i, kNumCorners> protrution;
    uint8_t depth;
};


void InitHeuristic(const std::array<Vec3i, kNumCorners>& idx_to_xyz_pos,
                   const std::array<std::array<uint8_t, kNumCorners>, kNumRot>& idx_piece_rot) {
    heuristic.assign(kNumPos, {});

    // initial starting protrution
    std::array<Vec3i, kNumCorners> protrution;
    int cnt = 0;
    for (int i = 0; i >= -1; i--) {
        for (int j = 0; j >= -1; j--) {
            for (int k = 0; k >= -1; k--) {
                protrution[cnt++] = {i, j, k};
            }
        }
    }
    std::vector<uint8_t> visited(uint32_t(kNumPos) * kNumOrient, uint8_t(-2));
    std::queue<HeuristicData> next;
    next.push({0, 0, protrution, 0});
    visited[0] = 0;
    uint32_t legal_cnt = 0;
    uint32_t cur_legal_cnt = 0;
    uint8_t cur_legal_cnt_depth = 0;
    // bfs
    while (!next.empty()) {
        HeuristicData heuristic_data = next.front();
        next.pop();
        legal_cnt++;
        if (heuristic_data.depth > cur_legal_cnt_depth) {
            LOG_EXTRA("Depth", cur_legal_cnt_depth, ":", cur_legal_cnt);
            cur_legal_cnt = 0;
            cur_legal_cnt_depth = heuristic_data.depth;
        }
        cur_legal_cnt++;

        // add current depth
        uint64_t cur_heuristic = heuristic_data.depth;

        for (uint8_t rot = 0; rot < kNumRot; rot++) {
            HeuristicData next_heuristic_data;
            next_heuristic_data.depth = heuristic_data.depth+1;
            next_heuristic_data.pos = position_change[heuristic_data.pos][rot];
            next_heuristic_data.orient = orientation_change[heuristic_data.orient][rot];

            uint32_t idx = (uint32_t(next_heuristic_data.orient)*kNumPos) + next_heuristic_data.pos;
            if (visited[idx] != uint8_t(-2)) {
                if (visited[idx] == uint8_t(-1)) { // illegal 11
                    cur_heuristic |= uint64_t(3) << (rot*2+8);
                }
                else if (visited[idx] > heuristic_data.depth) { // worse 10
                    cur_heuristic |= uint64_t(2) << (rot*2+8);
                }
                else if (visited[idx] == heuristic_data.depth) { // same 01
                    cur_heuristic |= uint64_t(1) << (rot*2+8);
                }
                // better 00
                continue;
            }
            visited[idx] = next_heuristic_data.depth;

            for (int i = 0; i < kNumCorners; i++) {
                if (idx_piece_rot[rot][i] == i) {
                    next_heuristic_data.protrution[i] = heuristic_data.protrution[i];
                    continue;
                }
                next_heuristic_data.protrution[idx_piece_rot[rot][i]] = MatVecMul(idx_to_rot_rep[rot].matrix, heuristic_data.protrution[i]);
            }
            if (!IsLegal(idx_to_xyz_pos, next_heuristic_data.protrution)) {
                visited[idx] = uint8_t(-1);
                cur_heuristic |= uint64_t(3) << (rot*2+8);
                continue;
            }
            if (visited[idx] > heuristic_data.depth) { // worse 10
                cur_heuristic |= uint64_t(2) << (rot*2+8);
            }
            else if (visited[idx] == heuristic_data.depth) { // same 01
                cur_heuristic |= uint64_t(1) << (rot*2+8);
            }
            next.push(next_heuristic_data);
        }
        heuristic[heuristic_data.pos][heuristic_data.orient] = cur_heuristic;
    }
    LOG_EXTRA("Depth", cur_legal_cnt_depth, ":", cur_legal_cnt);
    LOG_EXTRA("Legal Position Count:", legal_cnt);
}


void Init() {
    // piece position
    std::map<Vec3i, uint8_t> xyz_to_idx_pos;
    std::array<Vec3i, kNumCorners> idx_to_xyz_pos;
    InitXYZPos(xyz_to_idx_pos, idx_to_xyz_pos);

    // idx piece rotation
    std::array<std::array<uint8_t, kNumCorners>, kNumRot> idx_piece_rot;
    for (uint8_t rot = 0; rot < kNumRot; rot++) {
        for (int i = 0; i < kNumCorners; i++) {
            idx_piece_rot[rot][i] = xyz_to_idx_pos[RotateXYZPiece(idx_to_xyz_pos[i], rot)];
        }
    }

    LoadOrGenerate("[5/7] Corner Position Change", [&](){InitPositionChange(idx_piece_rot);},
                   "corner_position_change.bin", position_change, kNumPos);
    LoadOrGenerate("[6/7] Corner Orientation Change", [&](){InitOrinetationChange(idx_piece_rot);},
                   "corner_orientation_change.bin", orientation_change, kNumOrient);
    LoadOrGenerate("[7/7] Corner Heuristic", [&](){InitHeuristic(idx_to_xyz_pos, idx_piece_rot);},
                   "corner_heuristic.bin", heuristic, kNumPos);
}
}
