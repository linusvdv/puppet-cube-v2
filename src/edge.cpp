#include <algorithm>
#include <atomic>
#include <bit>
#include <cstdint>
#include <map>
#include <numeric>
#include <thread>
#include <unordered_map>
#include <vector>

#include "edge.hpp"
#include "logger.hpp"
#include "rotation.hpp"
#include "utils.hpp"
#include "parallel_hashmap/phmap.h"


namespace edge {
constexpr uint32_t kNumLehmerPos = Factorial(12);
constexpr uint32_t kNumPos = 9985968;
constexpr uint16_t kNumSymChange = 921; // this is unfortunatly more than 256 (which would fit in uint8_t and could therefore be packed in a uint32_t with the position)

constexpr uint64_t kNumHeuristic = uint64_t(kNumPos)*kNumOrient;
constexpr int kMyAtomicBitsetSizePerEl = 64;
constexpr uint32_t kNumHeuristicBuckets = 81609107;

using Heuristic = std::vector<std::array<uint64_t, kNumOrient/kNumStoredPerBucket>>;
using NextVistited = std::vector<std::array<std::atomic<uint64_t>, kNumOrient/kMyAtomicBitsetSizePerEl>>;

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

// heuristic
std::vector<std::array<uint32_t, kNumOrient/kNumStoredPerBucket>> heuristic_bucket;
std::vector<uint64_t> heuristic_value;


// most important one!
// ===================
void Rotate(uint32_t& pos, uint8_t& sym, uint16_t& orient, uint8_t rot) {
    // change rotation relative to symmetry
    rot = rotation_change[sym][rot];
    // position lookup (pos + symmetry change)
    uint64_t packed = position_change[pos][rot];
    uint32_t sym_change = packed >> kPosShift;
    pos = packed & kPosMask;
    // change symmetry
    uint16_t packed_sym = symmetry_change[sym_change][sym];
    uint8_t rel_sym = uint8_t(packed_sym>>8); // NOLINT
    sym = uint8_t(packed_sym);
    // orientation lookup
    orient = orientation_change[orient][rel_sym][rot]; // this rotation is not correct
}


uint8_t GetHeuristic(uint32_t pos, uint16_t orient) {
    return heuristic_value[heuristic_bucket[pos][orient/kNumStoredPerBucket]] >> ((orient%kNumStoredPerBucket) * 4) & kSingleHeuristicValue;
}


void InitXYZPos(std::map<Vec3i, uint8_t, std::greater<>>& xyz_to_idx_pos, std::array<Vec3i, kNumEdges>& idx_to_xyz_pos) {
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
    std::fill(pos_to_lehmer_pos[0].data(), pos_to_lehmer_pos[0].data() + (kNumPos * kNumSym), uint32_t(-1));
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
            uint32_t lehmer_pos = PosPermToLehmerPos(SymmetryPositionRotation(idx_piece_sym, pos_perm, sym_trans[sym]));
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
    symmetry_change.assign(kNumSymChange, {});
    position_change.assign(kNumPos, {});
    LOG_MEMORY();

    for (uint32_t pos = 0; pos < kNumPos; pos++) {
        for (uint8_t rot = 0; rot < kNumRot; rot++) {
            uint32_t lehmer_pos = pos_to_lehmer_pos[pos][0];
            uint32_t next_lehmer_pos = RotatePieces(idx_piece_rot, lehmer_pos, rot);

            uint32_t packed = lehmer_pos_to_pos[next_lehmer_pos];
            uint32_t next_sym_position = packed & kPosMask;
            uint32_t next_sym_change = packed >> kPosShift;
            uint8_t next_pos_active_sym = sym_pos_active_sym[next_sym_position];

            std::array<uint16_t, kNumSym> cur_sym_change;
            cur_sym_change.fill(uint16_t(-1));
            for (int sym = 0; sym < kNumSym; sym++) {
                cur_sym_change[sym] = sym_to_active_sym[next_pos_active_sym][sym_mul_sym[next_sym_change][sym]];
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
            // C = B * A^-1 = B * A^T
            uint16_t relative_sym = sym_mul_sym[cur_sym_change[sym]][sym_trans[sym]];
            cur_sym_change[sym] |= relative_sym << 8;
        }
    }

    if (sym_change_cnt != kNumSymChange) {
        LOG_CRITICAL("Wrong precomputation with symmetry change");
    }
}


// this is computed multiple times but as it so fast this is fine
void InitDefaultToSymOrient(std::array<std::array<uint16_t, kNumOrient>, kNumSym>& default_to_sym_orient,
                            const std::array<Vec3i, kNumEdges>& idx_to_xyz_pos,
                            const std::array<Mat3i, kNumSym>& idx_to_mat_sym,
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
    std::array<uint16_t, kNumSym> default_orient_change;
    for (uint16_t i = 0; i < kNumOrient; i++) {
        uint16_t orientation = i | (uint16_t(std::popcount(i)%2 == 1) << (kNumEdges-1)); // this is flipping the last bit to the correct place
        for (int sym = 0; sym < kNumSym; sym++) {
            uint16_t next_orientation = 0;
            for (int j = 0; j < kNumEdges; j++) {
                Vec3i edge_orient = idx_to_xyz_orient[j][(orientation>>j)&1];
                uint16_t next_pos = idx_piece_sym[sym][j];
                Vec3i next_edge_orient = MatVecMul(idx_to_mat_sym[sym], edge_orient);;
                next_orientation |= xyz_to_idx_orient[next_pos][next_edge_orient] << next_pos;
            }
            if (i == 0) {
                default_orient_change[sym] = next_orientation;
            }
            next_orientation ^= default_orient_change[sym];
            next_orientation &= kNumOrient-1;
            default_to_sym_orient[sym][i] = next_orientation;
        }
    }
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
        for (uint8_t rot = 0; rot < kNumRot; rot++) {
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

    std::array<std::array<uint16_t, kNumOrient>, kNumSym> default_to_sym_orient;
    InitDefaultToSymOrient(default_to_sym_orient, idx_to_xyz_pos, idx_to_mat_sym, idx_piece_sym);

    orientation_change.assign(kNumOrient, {});
    for (uint16_t orientation = 0; orientation < kNumOrient; orientation++) {
        for (uint8_t rel_sym = 0; rel_sym < kNumSym; rel_sym++) {
            for (uint8_t rot = 0; rot < kNumRot; rot++) {
                uint16_t next_orient = orient_rot[orientation][rot];
                uint16_t next_orient_sym = default_to_sym_orient[rel_sym][next_orient];
                orientation_change[orientation][rel_sym][rot] = next_orient_sym;
            }
        }
    }
}


inline uint8_t GetPreHeuristic(const Heuristic& heuristic, uint32_t pos, uint16_t orient) {
    return heuristic[pos][orient/kNumStoredPerBucket] >> ((orient%kNumStoredPerBucket) * 4) & kSingleHeuristicValue;
}


inline void SetAtomicNextVisited(NextVistited& next_visited, uint32_t pos, uint16_t orient) {
    next_visited[pos][orient/kMyAtomicBitsetSizePerEl].fetch_or(uint64_t(1)<<(orient%kMyAtomicBitsetSizePerEl), std::memory_order_relaxed);
}


void HeuristicMultithread(const Heuristic& heuristic, NextVistited& next_visited,
                          const std::vector<uint64_t>& sym_pos_same_sym,
                          const std::array<std::array<uint16_t, kNumOrient>, kNumSym>& default_to_sym_orient,
                          uint32_t pos_left, uint32_t pos_right, uint8_t next_depth) {
    for (uint32_t pos = pos_left; pos < pos_right && pos < kNumPos; pos++) {
        for (uint16_t orient = 0; orient < kNumOrient; orient++) {
            if (GetPreHeuristic(heuristic, pos, orient) != next_depth-1) {
                continue;
            }
            for (uint8_t rot = 0; rot < kNumRot; rot++) {
                uint32_t next_pos = pos;
                uint8_t next_sym = 0;
                uint16_t next_orient = orient;
                Rotate(next_pos, next_sym, next_orient, rot);
                if (GetPreHeuristic(heuristic, next_pos, next_orient) != kSingleHeuristicValue) {
                    continue;
                }
                SetAtomicNextVisited(next_visited, next_pos, next_orient);
                uint64_t cur_sym_pos_same_sym = sym_pos_same_sym[next_pos];
                if (std::popcount(cur_sym_pos_same_sym) <= 1) {
                    continue;
                }
                for (uint8_t sym = 0; sym < kNumSym; sym++) {
                    if (((cur_sym_pos_same_sym >> sym) & 1) == 0) {
                        continue;
                    }
                    SetAtomicNextVisited(next_visited, next_pos, default_to_sym_orient[sym][next_orient]);
                }
            }
        }
    }
}


void UpdateHeuristicMultithread(Heuristic& heuristic, NextVistited& next_visited,
                                uint32_t pos_left, uint32_t pos_right, uint64_t& local_cnt, uint8_t depth) {
    uint64_t local_num_pos = 0;
    for (uint32_t pos = pos_left; pos < pos_right && pos < kNumPos; pos++) {
        for (uint16_t orient_idx = 0; orient_idx < kNumOrient/kMyAtomicBitsetSizePerEl; orient_idx++) {
            uint64_t value = next_visited[pos][orient_idx].load(std::memory_order_relaxed);
            if (value == 0) {
                continue;
            }
            next_visited[pos][orient_idx].store(uint64_t(0), std::memory_order_relaxed);

            local_num_pos += std::popcount(value);
            while (value != 0U) {
                uint16_t orient = (orient_idx*kMyAtomicBitsetSizePerEl) + std::countr_zero(value);
                heuristic[pos][orient/kNumStoredPerBucket] ^= uint64_t(kSingleHeuristicValue - depth) << ((orient % kNumStoredPerBucket) * 4);
                value &= value - 1;
            }
        }
    }
    local_cnt = local_num_pos;
}


void InitHeuristic(const std::array<Vec3i, kNumEdges>& idx_to_xyz_pos,
                   const std::array<Mat3i, kNumSym>& idx_to_mat_sym,
                   const std::array<std::array<uint8_t, kNumEdges>, kNumSym>& idx_piece_sym,
                   const std::array<std::array<uint8_t, kNumSym>, kNumSym>& sym_mul_sym,
                   const std::array<uint8_t, kNumSym>& sym_trans) {
    LOG_EXTRA("sym_pos_same_sym start");
    std::vector<uint64_t> sym_pos_same_sym(kNumPos, 0);
    for (uint32_t pos = 0; pos < kNumPos; pos++) {
        for (uint8_t rot = 0; rot < kNumRot; rot++) {
            uint32_t next_pos = position_change[pos][rot] & kPosMask;
            uint32_t next_sym_change = position_change[pos][rot] >> kPosShift;
            if (sym_pos_same_sym[next_pos] != 0) {
                continue;
            }
            uint64_t cur_sym_pos_same_sym = 0;
            uint8_t first_same_sym = uint8_t(-1);
            for (uint8_t sym = 0; sym < kNumSym; sym++) {
                if (uint8_t(symmetry_change[next_sym_change][sym]) == 0) {
                    if (first_same_sym == uint8_t(-1)) {
                        first_same_sym = uint8_t(symmetry_change[next_sym_change][sym]>>8);
                    }
                    uint8_t same_sym = sym_mul_sym[symmetry_change[next_sym_change][sym]>>8][sym_trans[first_same_sym]];
                    cur_sym_pos_same_sym |= uint64_t(1) << same_sym;
                }
            }
            sym_pos_same_sym[next_pos] = cur_sym_pos_same_sym;
        }
    }
    LOG_MEMORY();

    std::array<std::array<uint16_t, kNumOrient>, kNumSym> default_to_sym_orient;
    InitDefaultToSymOrient(default_to_sym_orient, idx_to_xyz_pos, idx_to_mat_sym, idx_piece_sym);

    Heuristic heuristic(kNumPos);
    std::fill(heuristic[0].data(), heuristic[0].data() + (kNumHeuristic / kNumStoredPerBucket), ~uint64_t(0));
    LOG_MEMORY();
    NextVistited next_visited(kNumPos);
    LOG_MEMORY();

    heuristic[0][0] ^= kSingleHeuristicValue;
    uint64_t num_pos = 1;
    uint8_t heuristic_level = 1; // 0 to 14
    while (num_pos < kNumHeuristic) {
        {
            std::vector<std::jthread> heuristic_threads;
            heuristic_threads.reserve(Settings::GetNumThreads());
            for (int thread = 0; thread < Settings::GetNumThreads(); thread++) {
                heuristic_threads.emplace_back(HeuristicMultithread,
                                               std::ref(heuristic), std::ref(next_visited),
                                               std::ref(sym_pos_same_sym),
                                               std::ref(default_to_sym_orient),
                                               uint32_t(((kNumPos/Settings::GetNumThreads())+1)*thread),
                                               uint32_t(((kNumPos/Settings::GetNumThreads())+1)*(thread+1)),
                                               heuristic_level);
            }
        }
        uint64_t num_pos_level = 0;
        std::vector<uint64_t> per_thread_cnt(Settings::GetNumThreads(), 0);
        {
            std::vector<std::jthread> update_heuristic_threads;
            update_heuristic_threads.reserve(Settings::GetNumThreads());
            for (int thread = 0; thread < Settings::GetNumThreads(); thread++) {
                update_heuristic_threads.emplace_back(UpdateHeuristicMultithread,
                                               std::ref(heuristic), std::ref(next_visited),
                                               uint32_t(((kNumPos/Settings::GetNumThreads())+1)*thread),
                                               uint32_t(((kNumPos/Settings::GetNumThreads())+1)*(thread+1)),
                                               std::ref(per_thread_cnt[thread]),
                                               heuristic_level);
            }
        }
        for (uint64_t local_cnt : per_thread_cnt) {
            num_pos_level += local_cnt;
        }
        num_pos += num_pos_level;
        LOG_EXTRA("Level", heuristic_level, ":", num_pos_level);
        heuristic_level++;
    }
    LOG_ALL("Finished Generation");
    NextVistited().swap(next_visited);
    LOG_MEMORY();

    heuristic_bucket.assign(kNumPos, {});
    heuristic_value.assign(kNumHeuristicBuckets, 0);
    LOG_MEMORY();
    phmap::flat_hash_map<uint64_t, uint32_t> heuristic_value_bucket;
    uint32_t bucket_cnt = 0;
    for (uint32_t pos = 0; pos < kNumPos; pos++) {
        for (uint16_t orient_bucket = 0; orient_bucket < kNumOrient/kNumStoredPerBucket; orient_bucket++) {
            uint64_t cur_heuristic = heuristic[pos][orient_bucket];
            auto [it, inserted] = heuristic_value_bucket.try_emplace(cur_heuristic, bucket_cnt);
            if (inserted) {
                heuristic_value[bucket_cnt] = cur_heuristic;
                bucket_cnt++;
            }
            heuristic_bucket[pos][orient_bucket] = it->second;
        }
        if (pos % 100000 == 0) {
            LOG_EXTRA(pos, "/", kNumPos);
        }
    }
    if (bucket_cnt != kNumHeuristicBuckets) {
        LOG_CRITICAL("Heuristic bucket cnt incorrect", bucket_cnt, kNumHeuristicBuckets);
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

    LoadOrGenerate("[1/7] Edge Rotation Change", [&](){InitRotationChange(idx_to_mat_sym);},
                   "edge_rotation_change.bin", rotation_change, kNumSym);
    LoadMultipleOrGenerate("[2/7] Edge Position Change, Symmetry Change", [&](){InitPositionChangeSymmetryChange(idx_piece_rot, idx_piece_sym, sym_mul_sym, sym_trans);},
                           "edge_position_change.bin", position_change, kNumPos,
                           "edge_symmetry_change.bin", symmetry_change, kNumSymChange);
    LoadOrGenerate("[3/7] Edge Orientation Change", [&](){InitOrientationChange(idx_to_xyz_pos, idx_to_mat_sym, idx_piece_rot, idx_piece_sym);},
                   "edge_orientation_change.bin", orientation_change, kNumOrient);
    LoadMultipleOrGenerate("[4/7] Edge Heuristic", [&](){InitHeuristic(idx_to_xyz_pos, idx_to_mat_sym, idx_piece_sym, sym_mul_sym, sym_trans);},
                           "edge_heuristic_bucket.bin", heuristic_bucket, kNumPos,
                           "edge_heuristic_value.bin", heuristic_value, kNumHeuristicBuckets);
}
}
