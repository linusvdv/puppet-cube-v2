#include <algorithm>
#include <atomic>
#include <bit>
#include <cstdint>
#include <map>
#include <mutex>
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

#ifndef REDUCE_MEMORY
constexpr uint64_t kNumHeuristic = uint64_t(kNumPos)*kNumOrient;
constexpr uint32_t kNumHeuristicBuckets = 81609107;

using Heuristic = std::vector<std::array<uint64_t, kNumOrient/kNumStoredPerBucket>>;
#endif

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
#ifdef REDUCE_MEMORY
// the maximum of both is used as the heuristic
// (admissible like the full one, but weaker)
std::vector<uint8_t> position_heuristic; // [pos] exact distance of the relaxed problem where the orientation is ignored
std::vector<uint8_t> orientation_heuristic; // [orient] exact distance of the relaxed problem where the position is ignored (all symmetry frame changes allowed)
#else
std::vector<std::array<uint32_t, kNumOrient/kNumStoredPerBucket>> heuristic_bucket;
std::vector<uint64_t> heuristic_value;
#endif


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
#ifdef REDUCE_MEMORY
    return std::max(position_heuristic[pos], orientation_heuristic[orient]);
#else
    return heuristic_value[heuristic_bucket[pos][orient/kNumStoredPerBucket]] >> ((orient%kNumStoredPerBucket) * 4) & kSingleHeuristicValue;
#endif
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
    // find the position class representatives: the lexicographically smallest
    // permutation of every symmetry orbit; the 12! permutations are enumerated
    // in parallel over the 12*11 prefixes of the first two elements (the chunk
    // order over the prefixes is the lexicographic order)
    constexpr uint32_t kNumChunks = kNumEdges*(kNumEdges-1);
    std::vector<std::vector<std::array<uint8_t, kNumEdges>>> rep_perms(kNumChunks);
    {
        auto is_rep = [&](const std::array<uint8_t, kNumEdges>& pos_perm) {
            for (uint8_t sym = 0; sym < kNumSym; sym++) {
                if (SymmetryPositionRotation(idx_piece_sym, pos_perm, sym_trans[sym]) < pos_perm) {
                    return false;
                }
            }
            return true;
        };
        std::atomic<uint32_t> next_chunk(0);
        std::vector<std::jthread> threads;
        threads.reserve(Settings::GetNumThreads());
        for (int thread = 0; thread < Settings::GetNumThreads(); thread++) {
            threads.emplace_back([&](){
                uint32_t chunk;
                while ((chunk = next_chunk.fetch_add(1, std::memory_order_relaxed)) < kNumChunks) {
                    std::array<uint8_t, kNumEdges> pos_perm{};
                    pos_perm[0] = uint8_t(chunk/(kNumEdges-1)); // NOLINT
                    pos_perm[1] = uint8_t(chunk%(kNumEdges-1)); // NOLINT
                    if (pos_perm[1] >= pos_perm[0]) {
                        pos_perm[1]++;
                    }
                    uint8_t cnt = 2;
                    for (uint8_t i = 0; i < kNumEdges; i++) {
                        if (i != pos_perm[0] && i != pos_perm[1]) {
                            pos_perm[cnt++] = i;
                        }
                    }
                    do {
                        if (is_rep(pos_perm)) {
                            rep_perms[chunk].push_back(pos_perm);
                        }
                    } while (std::next_permutation(pos_perm.begin()+2, pos_perm.end()));
                }
            });
        }
    }

    // the position ids are the prefix sums of the representative counts over
    // the chunks (the chunk order is the lexicographic order)
    std::vector<uint32_t> rep_offset(kNumChunks+1, 0);
    for (uint32_t chunk = 0; chunk < kNumChunks; chunk++) {
        rep_offset[chunk+1] = rep_offset[chunk] + uint32_t(rep_perms[chunk].size());
    }
    if (rep_offset[kNumChunks] != kNumPos) {
        LOG_CRITICAL("Lehmer pos count not correct!");
    }

    // fill the lehmer position lookup and the active symmetry of every
    // representative (the orbits are disjoint -> representatives never write
    // the same entry)
    std::vector<uint32_t> lehmer_pos_to_pos(kNumLehmerPos, uint32_t(-1));
    std::vector<uint32_t> rep_lehmer_pos(kNumPos);
    std::vector<std::array<uint8_t, kNumSym>> rep_active_sym(kNumPos);
    {
        std::atomic<uint32_t> next_chunk(0);
        std::vector<std::jthread> threads;
        threads.reserve(Settings::GetNumThreads());
        for (int thread = 0; thread < Settings::GetNumThreads(); thread++) {
            threads.emplace_back([&](){
                uint32_t chunk;
                while ((chunk = next_chunk.fetch_add(1, std::memory_order_relaxed)) < kNumChunks) {
                    uint32_t cnt = rep_offset[chunk];
                    for (const std::array<uint8_t, kNumEdges>& pos_perm : rep_perms[chunk]) {
                        std::array<uint8_t, kNumSym> cur_active_sym;
                        std::array<uint32_t, kNumSym> cur_lehmer_pos{};
                        std::array<uint8_t, kNumSym> cur_lehmer_active{};
                        uint8_t cur_cnt = 0;
                        for (uint32_t sym = 0; sym < kNumSym; sym++) {
                            uint32_t lehmer_pos = PosPermToLehmerPos(SymmetryPositionRotation(idx_piece_sym, pos_perm, sym_trans[sym]));
                            bool found = false;
                            for (uint8_t i = 0; i < cur_cnt; i++) {
                                if (cur_lehmer_pos[i] == lehmer_pos) {
                                    cur_active_sym[sym] = cur_lehmer_active[i];
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) {
                                cur_lehmer_pos[cur_cnt] = lehmer_pos;
                                cur_lehmer_active[cur_cnt] = uint8_t(sym); // NOLINT
                                cur_cnt++;
                                cur_active_sym[sym] = uint8_t(sym); // NOLINT
                            }
                            if (lehmer_pos_to_pos[lehmer_pos] != uint32_t(-1)) {
                                continue;
                            }
                            lehmer_pos_to_pos[lehmer_pos] = cnt | (sym << kPosShift);
                        }
                        rep_lehmer_pos[cnt] = PosPermToLehmerPos(pos_perm);
                        rep_active_sym[cnt] = cur_active_sym;
                        cnt++;
                    }
                }
            });
        }
    }

    // deduplicate the active symmetry vectors in representative order
    std::vector<uint8_t> sym_pos_active_sym(kNumPos);
    std::map<std::array<uint8_t, kNumSym>, uint8_t> active_sym_map;
    std::vector<std::array<uint8_t, kNumSym>> sym_to_active_sym;
    for (uint32_t pos = 0; pos < kNumPos; pos++) {
        if (!active_sym_map.contains(rep_active_sym[pos])) {
            active_sym_map[rep_active_sym[pos]] = uint8_t(sym_to_active_sym.size()); // NOLINT
            sym_to_active_sym.push_back(rep_active_sym[pos]);
        }
        sym_pos_active_sym[pos] = active_sym_map[rep_active_sym[pos]];
    }
    if (sym_to_active_sym.size() > 256) {
        LOG_CRITICAL("Active symmetry count not correct!");
    }
    std::vector<std::array<uint8_t, kNumSym>>().swap(rep_active_sym);
    std::vector<std::vector<std::array<uint8_t, kNumEdges>>>().swap(rep_perms);

    // precompute the next position and the symmetry change pair of every
    // (pos, rot) in parallel over position blocks; the symmetry change pair
    // (symmetry change id, active symmetry) fully determines the symmetry
    // change vector and is deduplicated locally per block
    symmetry_change.assign(kNumSymChange, {});
    position_change.assign(kNumPos, {});
    LOG_MEMORY();
    constexpr uint32_t kChangeBlockSize = 65536;
    const uint32_t num_change_blocks = (kNumPos+kChangeBlockSize-1)/kChangeBlockSize;
    std::vector<uint32_t> packed_next(uint64_t(kNumPos)*kNumRot);
    std::vector<uint32_t> symmetry_pair(uint64_t(kNumPos)*kNumRot);
    std::vector<std::vector<uint32_t>> block_pairs(num_change_blocks);
    {
        std::atomic<uint32_t> next_block(0);
        std::vector<std::jthread> threads;
        threads.reserve(Settings::GetNumThreads());
        for (int thread = 0; thread < Settings::GetNumThreads(); thread++) {
            threads.emplace_back([&](){
                uint32_t block;
                while ((block = next_block.fetch_add(1, std::memory_order_relaxed)) < num_change_blocks) {
                    LOG_EXTRA("block", block, "/", num_change_blocks);
                    std::unordered_map<uint32_t, uint32_t> pair_to_local;
                    for (uint32_t pos = block*kChangeBlockSize; pos < (block+1)*kChangeBlockSize && pos < kNumPos; pos++) {
                        for (uint8_t rot = 0; rot < kNumRot; rot++) {
                            uint32_t next_lehmer_pos = RotatePieces(idx_piece_rot, rep_lehmer_pos[pos], rot);
                            uint32_t packed = lehmer_pos_to_pos[next_lehmer_pos];
                            packed_next[uint64_t(pos)*kNumRot+rot] = packed;
                            uint32_t pair = ((packed >> kPosShift) << 8) | sym_pos_active_sym[packed & kPosMask];
                            auto [it, inserted] = pair_to_local.try_emplace(pair, uint32_t(block_pairs[block].size()));
                            if (inserted) {
                                block_pairs[block].push_back(pair);
                            }
                            symmetry_pair[uint64_t(pos)*kNumRot+rot] = it->second;
                        }
                    }
                }
            });
        }
    }
    std::vector<uint32_t>().swap(lehmer_pos_to_pos);
    std::vector<uint32_t>().swap(rep_lehmer_pos);

    // merge the blocks in order: the first occurrence of every symmetry
    // change vector over the (pos, rot) order defines its id
    int sym_change_cnt = 0;
    std::map<std::array<uint16_t, kNumSym>, int> sym_change_map;
    std::vector<std::vector<uint32_t>> block_sym_change(num_change_blocks);
    for (uint32_t block = 0; block < num_change_blocks; block++) {
        block_sym_change[block].resize(block_pairs[block].size());
        for (size_t local = 0; local < block_pairs[block].size(); local++) {
            uint32_t pair = block_pairs[block][local];
            uint32_t next_sym_change = pair >> 8;
            uint32_t next_pos_active_sym = pair & 0xFF;
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
            block_sym_change[block][local] = uint32_t(sym_change_map[cur_sym_change]);
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

    // write the position change table
    {
        std::atomic<uint32_t> next_block(0);
        std::vector<std::jthread> threads;
        threads.reserve(Settings::GetNumThreads());
        for (int thread = 0; thread < Settings::GetNumThreads(); thread++) {
            threads.emplace_back([&](){
                uint32_t block;
                while ((block = next_block.fetch_add(1, std::memory_order_relaxed)) < num_change_blocks) {
                    for (uint32_t pos = block*kChangeBlockSize; pos < (block+1)*kChangeBlockSize && pos < kNumPos; pos++) {
                        for (uint8_t rot = 0; rot < kNumRot; rot++) {
                            position_change[pos][rot] = (uint64_t(block_sym_change[block][symmetry_pair[uint64_t(pos)*kNumRot+rot]]) << kPosShift)
                                                        | (packed_next[uint64_t(pos)*kNumRot+rot] & kPosMask);
                        }
                    }
                }
            });
        }
    }
}


// this is computed multiple times but as it is so fast this is fine
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


#ifndef REDUCE_MEMORY
// marks the state directly in the heuristic table; the first marker of a
// state counts it (the states are marked exactly once per level)
inline void MarkHeuristic(Heuristic& heuristic, uint32_t pos, uint16_t orient, uint8_t depth, uint64_t& local_cnt) {
    std::atomic_ref<uint64_t> word(heuristic[pos][orient/kNumStoredPerBucket]);
    int shift = (orient%kNumStoredPerBucket) * 4;
    uint64_t cur = word.load(std::memory_order_relaxed);
    while (((cur >> shift) & kSingleHeuristicValue) == kSingleHeuristicValue) {
        uint64_t next = (cur & ~(kSingleHeuristicValue << shift)) | (uint64_t(depth) << shift);
        if (word.compare_exchange_weak(cur, next, std::memory_order_relaxed, std::memory_order_relaxed)) {
            local_cnt++;
            return;
        }
    }
}


void HeuristicMultithread(Heuristic& heuristic,
                          const std::vector<uint64_t>& sym_pos_same_sym,
                          const std::array<std::array<uint16_t, kNumOrient>, kNumSym>& default_to_sym_orient,
                          std::atomic<uint32_t>& next_block, uint8_t next_depth, std::atomic<uint64_t>& num_pos_level) {
    constexpr uint32_t kBlockSize = 4096;
    // the frontier states are buffered: the 18 successor words of every
    // buffered state are prefetched on enqueue and expanded later (the
    // heuristic lookups are random accesses into the ~10 GB table)
    struct Successor {
        uint32_t next_pos;
        uint16_t next_orient;
    };
    constexpr int kBufferSize = 32;
    std::array<std::array<Successor, kNumRot>, kBufferSize> buffer;
    int buffer_cnt = 0;
    uint64_t local_cnt = 0;
    auto expand_buffer = [&]() {
        for (int i = 0; i < buffer_cnt; i++) {
            for (uint8_t rot = 0; rot < kNumRot; rot++) {
                uint32_t next_pos = buffer[i][rot].next_pos;
                uint16_t next_orient = buffer[i][rot].next_orient;
                MarkHeuristic(heuristic, next_pos, next_orient, next_depth, local_cnt);
                uint64_t cur_sym_pos_same_sym = sym_pos_same_sym[next_pos];
                if (std::popcount(cur_sym_pos_same_sym) <= 1) {
                    continue;
                }
                for (uint8_t sym = 0; sym < kNumSym; sym++) {
                    if (((cur_sym_pos_same_sym >> sym) & 1) == 0) {
                        continue;
                    }
                    MarkHeuristic(heuristic, next_pos, default_to_sym_orient[sym][next_orient], next_depth, local_cnt);
                }
            }
        }
        buffer_cnt = 0;
    };
    uint32_t pos_left = next_block.fetch_add(1, std::memory_order_relaxed)*kBlockSize;
    while (pos_left < kNumPos) {
        uint32_t pos_right = std::min(pos_left+kBlockSize, kNumPos);
        for (uint32_t pos = pos_left; pos < pos_right; pos++) {
            for (uint32_t orient_bucket = 0; orient_bucket < kNumOrient/kNumStoredPerBucket; orient_bucket++) {
                uint64_t word = std::atomic_ref<uint64_t>(heuristic[pos][orient_bucket]).load(std::memory_order_relaxed);
                // swar skip: a nibble equal to next_depth-1 corresponds to a
                // zero nibble after the xor with the broadcast value
                const uint64_t frontier_value = next_depth-1;
                uint64_t equal = word ^ (frontier_value*0x1111111111111111ULL);
                if (((equal-0x1111111111111111ULL) & ~equal & 0x8888888888888888ULL) == 0) {
                    continue;
                }
                for (uint32_t nibble = 0; nibble < kNumStoredPerBucket; nibble++) {
                    if (((word >> (nibble*4)) & kSingleHeuristicValue) != frontier_value) {
                        continue;
                    }
                    uint16_t orient = uint16_t(orient_bucket*kNumStoredPerBucket + nibble); // NOLINT
                    if (buffer_cnt == kBufferSize) {
                        expand_buffer();
                    }
                    std::array<Successor, kNumRot>& successors = buffer[buffer_cnt++];
                    for (uint8_t rot = 0; rot < kNumRot; rot++) {
                        // rotate the state (the expansion always starts with
                        // symmetry 0: the rotation index and the new symmetry
                        // are not needed)
                        uint64_t packed = position_change[pos][rot];
                        uint32_t next_pos = uint32_t(packed & kPosMask);
                        uint8_t rel_sym = uint8_t(symmetry_change[packed >> kPosShift][0] >> 8); // NOLINT
                        uint16_t next_orient = orientation_change[orient][rel_sym][rot];
                        successors[rot] = {next_pos, next_orient};
                        __builtin_prefetch(&heuristic[next_pos][next_orient/kNumStoredPerBucket], 1, 2);
                        __builtin_prefetch(&sym_pos_same_sym[next_pos], 0, 1);
                    }
                }
            }
        }
        pos_left = next_block.fetch_add(1, std::memory_order_relaxed)*kBlockSize;
    }
    expand_buffer();
    num_pos_level.fetch_add(local_cnt, std::memory_order_relaxed);
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

    heuristic[0][0] ^= kSingleHeuristicValue;
    uint64_t num_pos = 1;
    uint8_t heuristic_level = 1; // 0 to 14
    while (num_pos < kNumHeuristic) {
        std::atomic<uint32_t> next_block(0);
        std::atomic<uint64_t> num_pos_level(0);
        {
            std::vector<std::jthread> heuristic_threads;
            heuristic_threads.reserve(Settings::GetNumThreads());
            for (int thread = 0; thread < Settings::GetNumThreads(); thread++) {
                heuristic_threads.emplace_back(HeuristicMultithread,
                                               std::ref(heuristic),
                                               std::ref(sym_pos_same_sym),
                                               std::ref(default_to_sym_orient),
                                               std::ref(next_block),
                                               heuristic_level,
                                               std::ref(num_pos_level));
            }
        }
        uint64_t cur_num_pos_level = num_pos_level.load(std::memory_order_relaxed);
        if (cur_num_pos_level == 0) {
            LOG_CRITICAL("Heuristic level did not find any new positions");
        }
        num_pos += cur_num_pos_level;
        LOG_EXTRA("Level", heuristic_level, ":", cur_num_pos_level);
        heuristic_level++;
    }
    LOG_ALL("Finished Generation");
    std::vector<uint64_t>().swap(sym_pos_same_sym);
    LOG_MEMORY();

    heuristic_bucket.assign(kNumPos, {});
    LOG_MEMORY();

    // deduplicate the heuristic words in parallel: the words are sharded over
    // 256 hash maps; every shard assigns local ids and tracks the first
    // occurrence of every word (the ids of the sequential scan order are
    // recovered with a sort by the first occurrence)
    constexpr uint32_t kNumShards = 256;
    constexpr uint32_t kShardShift = 24;
    std::vector<phmap::flat_hash_map<uint64_t, uint32_t>> shard_maps(kNumShards);
    std::vector<std::mutex> shard_mtx(kNumShards);
    std::vector<std::vector<uint64_t>> shard_words(kNumShards);
    std::vector<std::vector<uint64_t>> shard_first(kNumShards);
    {
        std::atomic<uint32_t> next_block(0);
        std::vector<std::jthread> threads;
        threads.reserve(Settings::GetNumThreads());
        for (int thread = 0; thread < Settings::GetNumThreads(); thread++) {
            threads.emplace_back([&](){
                constexpr uint32_t kBlockSize = 4096;
                uint32_t pos_left = next_block.fetch_add(1, std::memory_order_relaxed)*kBlockSize;
                while (pos_left < kNumPos) {
                    if (pos_left % 262144 == 0) {
                        LOG_EXTRA(pos_left, "/", kNumPos);
                    }
                    uint32_t pos_right = std::min(pos_left+kBlockSize, kNumPos);
                    for (uint32_t pos = pos_left; pos < pos_right; pos++) {
                        for (uint32_t orient_bucket = 0; orient_bucket < kNumOrient/kNumStoredPerBucket; orient_bucket++) {
                            uint64_t cur_heuristic = heuristic[pos][orient_bucket];
                            uint32_t shard = uint32_t((cur_heuristic*0x9E3779B97F4A7C15) >> 56); // NOLINT
                            uint64_t first = uint64_t(pos)*(kNumOrient/kNumStoredPerBucket)+orient_bucket;
                            std::lock_guard<std::mutex> lock(shard_mtx[shard]);
                            auto [it, inserted] = shard_maps[shard].try_emplace(cur_heuristic, uint32_t(shard_words[shard].size()));
                            if (inserted) {
                                shard_words[shard].push_back(cur_heuristic);
                                shard_first[shard].push_back(first);
                            }
                            else if (first < shard_first[shard][it->second]) {
                                shard_first[shard][it->second] = first;
                            }
                            heuristic_bucket[pos][orient_bucket] = (shard << kShardShift) | it->second;
                        }
                    }
                    pos_left = next_block.fetch_add(1, std::memory_order_relaxed)*kBlockSize;
                }
            });
        }
    }
    Heuristic().swap(heuristic);
    LOG_MEMORY();

    // sort the distinct words by their first occurrence with a radix sort
    // (first occurrence << 32 | shard << 24 | local id)
    uint64_t bucket_cnt = 0;
    for (uint32_t shard = 0; shard < kNumShards; shard++) {
        bucket_cnt += shard_words[shard].size();
    }
    if (bucket_cnt != kNumHeuristicBuckets) {
        LOG_CRITICAL("Heuristic bucket cnt incorrect", bucket_cnt, kNumHeuristicBuckets);
    }
    std::vector<uint64_t> sort_data(bucket_cnt);
    std::vector<uint64_t> sort_tmp(bucket_cnt);
    {
        uint64_t cnt = 0;
        for (uint32_t shard = 0; shard < kNumShards; shard++) {
            for (size_t local = 0; local < shard_words[shard].size(); local++) {
                sort_data[cnt++] = (shard_first[shard][local] << 32) | (uint64_t(shard) << kShardShift) | local;
            }
        }
    }
    { // bits 32 to 47 of the first occurrence
        std::array<uint32_t, 1<<16> counts{};
        for (uint64_t value : sort_data) {
            counts[uint32_t(value >> 32) & 0xFFFF]++;
        }
        uint32_t sum = 0;
        for (uint32_t& count : counts) {
            uint32_t cur_count = count;
            count = sum;
            sum += cur_count;
        }
        for (uint64_t value : sort_data) {
            sort_tmp[counts[uint32_t(value >> 32) & 0xFFFF]++] = value;
        }
    }
    { // bits 48 to 62 of the first occurrence
        std::array<uint32_t, 1<<15> counts{};
        for (uint64_t value : sort_tmp) {
            counts[uint32_t(value >> 48) & 0x7FFF]++;
        }
        uint32_t sum = 0;
        for (uint32_t& count : counts) {
            uint32_t cur_count = count;
            count = sum;
            sum += cur_count;
        }
        for (uint64_t value : sort_tmp) {
            sort_data[counts[uint32_t(value >> 48) & 0x7FFF]++] = value;
        }
    }

    // the sorted position is the bucket id (the first occurrence order)
    heuristic_value.assign(kNumHeuristicBuckets, 0);
    std::vector<std::vector<uint32_t>> shard_rank(kNumShards);
    for (uint32_t shard = 0; shard < kNumShards; shard++) {
        shard_rank[shard].resize(shard_words[shard].size());
    }
    {
        std::atomic<uint64_t> next_range(0);
        std::vector<std::jthread> threads;
        threads.reserve(Settings::GetNumThreads());
        constexpr uint64_t kRangeSize = 1<<20;
        for (int thread = 0; thread < Settings::GetNumThreads(); thread++) {
            threads.emplace_back([&](){
                uint64_t left = next_range.fetch_add(1, std::memory_order_relaxed)*kRangeSize;
                while (left < bucket_cnt) {
                    uint64_t right = std::min(left+kRangeSize, bucket_cnt);
                    for (uint64_t bucket = left; bucket < right; bucket++) {
                        uint64_t value = sort_data[bucket];
                        uint32_t shard = uint32_t(value >> kShardShift) & 0xFF;
                        uint32_t local = uint32_t(value & 0xFFFFFF);
                        shard_rank[shard][local] = uint32_t(bucket); // NOLINT
                        heuristic_value[bucket] = shard_words[shard][local];
                    }
                    left = next_range.fetch_add(1, std::memory_order_relaxed)*kRangeSize;
                }
            });
        }
    }

    // replace the temporary shard entries with the bucket ids
    {
        std::atomic<uint32_t> next_block(0);
        std::vector<std::jthread> threads;
        threads.reserve(Settings::GetNumThreads());
        for (int thread = 0; thread < Settings::GetNumThreads(); thread++) {
            threads.emplace_back([&](){
                constexpr uint32_t kBlockSize = 4096;
                uint32_t pos_left = next_block.fetch_add(1, std::memory_order_relaxed)*kBlockSize;
                while (pos_left < kNumPos) {
                    uint32_t pos_right = std::min(pos_left+kBlockSize, kNumPos);
                    for (uint32_t pos = pos_left; pos < pos_right; pos++) {
                        for (uint32_t orient_bucket = 0; orient_bucket < kNumOrient/kNumStoredPerBucket; orient_bucket++) {
                            uint32_t entry = heuristic_bucket[pos][orient_bucket];
                            heuristic_bucket[pos][orient_bucket] = shard_rank[entry >> kShardShift][entry & 0xFFFFFF];
                        }
                    }
                    pos_left = next_block.fetch_add(1, std::memory_order_relaxed)*kBlockSize;
                }
            });
        }
    }
}
#else
void InitReducedHeuristic() {
    // the maximum of both heuristics stays admissible (each one is a lower
    // bound of the full edge heuristic) and consistent; the values have to
    // stay <= 14 like the ones of the full heuristic (the leaf searches rely
    // on this)

    // position only: the position class transition is independent of the
    // orientation and the current symmetry
    position_heuristic.assign(kNumPos, kSingleHeuristicValue);
    std::vector<uint32_t> frontier = {0};
    position_heuristic[0] = 0;
    int depth = 1;
    while (!frontier.empty()) {
        std::vector<uint32_t> next_frontier;
        for (uint32_t pos : frontier) {
            for (uint8_t rot = 0; rot < kNumRot; rot++) {
                uint32_t next_pos = position_change[pos][rot] & kPosMask;
                if (position_heuristic[next_pos] != kSingleHeuristicValue) {
                    continue;
                }
                position_heuristic[next_pos] = uint8_t(depth); // NOLINT
                next_frontier.push_back(next_pos);
            }
        }
        frontier = std::move(next_frontier);
        depth++;
    }
    LOG_ALL("Edge position heuristic finished at depth", depth-1);
    if (depth-1 > 14) {
        LOG_CRITICAL("Edge position heuristic exceeds the maximum edge distance 14");
    }

    // orientation only: the relative symmetry frame change of a move depends
    // on the position -> allow all frame changes for a relaxation
    orientation_heuristic.assign(kNumOrient, kSingleHeuristicValue);
    frontier = {0};
    orientation_heuristic[0] = 0;
    depth = 1;
    while (!frontier.empty()) {
        std::vector<uint32_t> next_frontier;
        for (uint32_t orient : frontier) {
            for (uint8_t rel_sym = 0; rel_sym < kNumSym; rel_sym++) {
                for (uint8_t rot = 0; rot < kNumRot; rot++) {
                    uint16_t next_orient = orientation_change[orient][rel_sym][rot];
                    if (orientation_heuristic[next_orient] != kSingleHeuristicValue) {
                        continue;
                    }
                    orientation_heuristic[next_orient] = uint8_t(depth); // NOLINT
                    next_frontier.push_back(next_orient);
                }
            }
        }
        frontier = std::move(next_frontier);
        depth++;
    }
    LOG_ALL("Edge orientation heuristic finished at depth", depth-1);
    if (depth-1 > 14) {
        LOG_CRITICAL("Edge orientation heuristic exceeds the maximum edge distance 14");
    }
}
#endif


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
    // symmetry multiply symmetry
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
#ifdef REDUCE_MEMORY
    LoadMultipleOrGenerate("[4/7] Edge Heuristic (reduced)", [&](){InitReducedHeuristic();},
                           "edge_heuristic_position.bin", position_heuristic, kNumPos,
                           "edge_heuristic_orientation.bin", orientation_heuristic, kNumOrient);
#else
    LoadMultipleOrGenerate("[4/7] Edge Heuristic", [&](){InitHeuristic(idx_to_xyz_pos, idx_to_mat_sym, idx_piece_sym, sym_mul_sym, sym_trans);},
                           "edge_heuristic_bucket.bin", heuristic_bucket, kNumPos,
                           "edge_heuristic_value.bin", heuristic_value, kNumHeuristicBuckets);
#endif
}
}
