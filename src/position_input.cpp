#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <queue>
#include <string>
#include <vector>

#include "corner.hpp"
#include "edge.hpp"
#include "logger.hpp"
#include "piece_geometry.hpp"
#include "position_input.hpp"
#include "rotation.hpp"
#include "utils.hpp"


namespace input {
namespace {


// the lazily built conversion data (pure functions of the rotation definitions)
struct InputData {
    piece_geometry::EdgeSymmetryData edge_data;
    piece_geometry::CornerPositionData corner_data;
    std::map<Vec3i, uint8_t> corner_axis_to_idx; // e_x -> 0, e_y -> 1, e_z -> 2
    std::array<uint8_t, corner::kNumOrient> implied_digit7; // the implied axis of the last slot
};


InputData BuildInputData() {
    InputData data;
    data.edge_data = piece_geometry::BuildEdgeSymmetryData();
    data.corner_data = piece_geometry::BuildCornerPositionData();
    for (int i = 0; i < 3; i++) {
        Vec3i axis{};
        axis[i] = 1;
        data.corner_axis_to_idx[axis] = uint8_t(i); // NOLINT
    }

    // corner orientation validity: breadth first search over the 8
    // orientation axis vectors (like the corner orientation change
    // precomputation) recording the implied axis of the last slot for every
    // orientation of the first 7 slots (the reachable orientations form
    // exactly 3^7 of the 3^8 axis assignments)
    data.implied_digit7.fill(uint8_t(-1));
    std::array<Vec3i, corner::kNumCorners> orient_perm;
    orient_perm.fill({1, 0, 0});
    std::queue<std::array<Vec3i, corner::kNumCorners>> next;
    data.implied_digit7[piece_geometry::OrientPermToLehmerOrient(data.corner_axis_to_idx, orient_perm)] = 0;
    next.push(orient_perm);
    uint32_t cnt = 1;
    while (!next.empty()) {
        orient_perm = next.front();
        next.pop();
        for (uint8_t rot = 0; rot < kNumRot; rot++) {
            std::array<Vec3i, corner::kNumCorners> next_orient_perm;
            for (int i = 0; i < corner::kNumCorners; i++) {
                if (data.corner_data.idx_piece_rot[rot][i] == i) {
                    next_orient_perm[i] = orient_perm[i];
                    continue;
                }
                next_orient_perm[data.corner_data.idx_piece_rot[rot][i]] =
                    piece_geometry::Vec3iAbs(MatVecMul(idx_to_rot_rep[rot].matrix, orient_perm[i]));
            }
            uint16_t next_lehmer_orient = piece_geometry::OrientPermToLehmerOrient(data.corner_axis_to_idx, next_orient_perm);
            if (data.implied_digit7[next_lehmer_orient] == uint8_t(-1)) {
                data.implied_digit7[next_lehmer_orient] = data.corner_axis_to_idx[next_orient_perm[corner::kNumCorners-1]];
                next.push(next_orient_perm);
                cnt++;
            }
        }
    }
    if (cnt != corner::kNumOrient) {
        LOG_CRITICAL("Corner orientation precomputation wrong size", cnt, "/", uint32_t(corner::kNumOrient));
    }
    return data;
}


const InputData& GetInputData() {
    static InputData data = BuildInputData();
    return data;
}


// is the direction a signed unit axis
bool IsSignedAxis(const std::array<int, 3>& dir) {
    int nonzero = 0;
    for (int i = 0; i < 3; i++) {
        if (dir[i] != 0) {
            if (std::abs(dir[i]) != 1) {
                return false;
            }
            nonzero++;
        }
    }
    return nonzero == 1;
}


// are the directions exactly the face directions of a grid position (one
// signed unit axis per nonzero coordinate, matching its sign)
bool AreFaceDirs(const std::array<int, 3>& pos, const std::vector<std::array<int, 3>>& dirs) {
    int pos_cnt = 0;
    for (int c : pos) {
        pos_cnt += int(c != 0);
    }
    if (int(dirs.size()) != pos_cnt) {
        return false;
    }
    std::array<int, 3> used{};
    for (const std::array<int, 3>& dir : dirs) {
        if (!IsSignedAxis(dir)) {
            return false;
        }
        int axis = -1;
        for (int i = 0; i < 3; i++) {
            if (dir[i] != 0) {
                axis = i;
                break;
            }
        }
        if (used[axis] != 0 || dir[axis] != pos[axis]) {
            return false;
        }
        used[axis] = 1;
    }
    return true;
}


// the positive axis of a direction
Vec3i AxisVec(const std::array<int, 3>& dir) {
    Vec3i axis{};
    for (int i = 0; i < 3; i++) {
        if (dir[i] != 0) {
            axis[i] = 1;
        }
    }
    return axis;
}


// matrix with the directions as columns
Mat3i DirsToMat(const std::vector<std::array<int, 3>>& dirs) {
    Mat3i mat{};
    for (int c = 0; c < 3; c++) {
        for (int r = 0; r < 3; r++) {
            mat[r][c] = dirs[c][r];
        }
    }
    return mat;
}


int Mat3Det(const Mat3i& mat) {
    return mat[0][0]*(mat[1][1]*mat[2][2]-mat[1][2]*mat[2][1])
         - mat[0][1]*(mat[1][0]*mat[2][2]-mat[1][2]*mat[2][0])
         + mat[0][2]*(mat[1][0]*mat[2][1]-mat[1][1]*mat[2][0]);
}


// parity of a permutation (0 = even, 1 = odd)
template <std::size_t N>
int PermutationParity(const std::array<uint8_t, N>& perm) {
    int cnt = 0;
    for (std::size_t i = 0; i < N; i++) {
        for (std::size_t j = i+1; j < N; j++) {
            cnt += int(perm[i] > perm[j]);
        }
    }
    return cnt % 2;
}


}


PositionResult ConvertPosition(const std::vector<PiecePlacement>& corners,
                                const std::vector<PiecePlacement>& edges) {
    const InputData& data = GetInputData();
    PositionResult result;

    if (corners.size() != size_t(corner::kNumCorners) || edges.size() != size_t(kNumEdges)) {
        result.error = PositionError::kInvalidInput;
        result.message = "expected 8 corner and 12 edge placements";
        return result;
    }

    // ---------------- corners ----------------
    std::array<uint8_t, corner::kNumCorners> corner_perm{};
    std::array<uint8_t, corner::kNumCorners> corner_digits{};
    uint16_t corner_pieces_used = 0;
    for (const PiecePlacement& placement : corners) {
        auto slot_it = data.corner_data.xyz_to_idx_pos.find(placement.slot);
        auto piece_it = data.corner_data.xyz_to_idx_pos.find(placement.piece);
        if (slot_it == data.corner_data.xyz_to_idx_pos.end() || piece_it == data.corner_data.xyz_to_idx_pos.end()) {
            result.error = PositionError::kInvalidSlot;
            result.message = "slot or piece is not a valid corner position";
            return result;
        }
        uint8_t slot = slot_it->second;
        uint8_t piece = piece_it->second;
        if (((corner_pieces_used >> piece) & 1) != 0) {
            result.error = PositionError::kDuplicatePiece;
            result.message = "a corner piece appears more than once";
            return result;
        }
        corner_pieces_used |= uint16_t(1) << piece;
        if (!AreFaceDirs(placement.piece, placement.sticker_home_dirs) ||
            !AreFaceDirs(placement.slot, placement.sticker_cur_dirs)) {
            result.error = PositionError::kInvalidAssignment;
            result.message = "sticker directions do not match the corner piece or slot";
            return result;
        }
        // a physical placement is a proper rotation, not a reflection
        if (Mat3Det(DirsToMat(placement.sticker_cur_dirs))*Mat3Det(DirsToMat(placement.sticker_home_dirs)) != 1) {
            result.error = PositionError::kReflectedCorner;
            result.message = "a corner is colored in an impossible (mirrored) way";
            return result;
        }
        corner_perm[piece] = slot;
        // the corner orientation is the axis the sticker with home direction
        // along the x axis points to (the orientation vector of the piece is
        // the placement rotation applied to the x axis)
        size_t home_x = placement.sticker_home_dirs.size();
        for (size_t i = 0; i < placement.sticker_home_dirs.size(); i++) {
            if (placement.sticker_home_dirs[i][0] != 0) {
                home_x = i;
                break;
            }
        }
        if (home_x == placement.sticker_home_dirs.size()) {
            result.error = PositionError::kInvalidAssignment;
            result.message = "corner sticker directions do not span all axes";
            return result;
        }
        corner_digits[slot] = data.corner_axis_to_idx.at(AxisVec(placement.sticker_cur_dirs[home_x]));
    }

    // corner position (8! lehmer code) and orientation (3^7 over the first 7 slots)
    uint16_t corner_pos = piece_geometry::PosPermToLehmerPos(corner_perm);
    uint16_t corner_orient = 0;
    for (int i = 0; i < corner::kNumCorners-1; i++) {
        corner_orient = corner_orient*3 + corner_digits[i];
    }
    // the orientation of the last corner is implied by the other 7
    if (data.implied_digit7[corner_orient] != corner_digits[corner::kNumCorners-1]) {
        result.error = PositionError::kCornerOrientParity;
        result.message = "corner orientation parity is wrong (the last corner is twisted)";
        return result;
    }

    // ---------------- edges ----------------
    std::array<uint8_t, kNumEdges> edge_perm{};
    uint16_t edge_pieces_used = 0;
    std::array<Vec3i, kNumEdges> primary_dirs{}; // world direction of the primary sticker per slot
    for (const PiecePlacement& placement : edges) {
        auto slot_it = data.edge_data.xyz_to_idx_pos.find(placement.slot);
        auto piece_it = data.edge_data.xyz_to_idx_pos.find(placement.piece);
        if (slot_it == data.edge_data.xyz_to_idx_pos.end() || piece_it == data.edge_data.xyz_to_idx_pos.end()) {
            result.error = PositionError::kInvalidSlot;
            result.message = "slot or piece is not a valid edge position";
            return result;
        }
        uint8_t slot = slot_it->second;
        uint8_t piece = piece_it->second;
        if (((edge_pieces_used >> piece) & 1) != 0) {
            result.error = PositionError::kDuplicatePiece;
            result.message = "an edge piece appears more than once";
            return result;
        }
        edge_pieces_used |= uint16_t(1) << piece;
        if (!AreFaceDirs(placement.piece, placement.sticker_home_dirs) ||
            !AreFaceDirs(placement.slot, placement.sticker_cur_dirs)) {
            result.error = PositionError::kInvalidAssignment;
            result.message = "sticker directions do not match the edge piece or slot";
            return result;
        }
        // the primary sticker (its home direction is the standard
        // orientation vector of the piece's home position) carries the
        // piece's orientation marker
        const Vec3i& primary = data.edge_data.idx_to_xyz_orient[piece][0];
        size_t primary_sticker = placement.sticker_home_dirs.size();
        for (size_t i = 0; i < placement.sticker_home_dirs.size(); i++) {
            if (placement.sticker_home_dirs[i] == primary) {
                primary_sticker = i;
                break;
            }
        }
        if (primary_sticker == placement.sticker_home_dirs.size()) {
            result.error = PositionError::kInvalidAssignment;
            result.message = "edge sticker directions do not include the standard orientation";
            return result;
        }
        edge_perm[piece] = slot;
        primary_dirs[slot] = placement.sticker_cur_dirs[primary_sticker];
    }

    // ---------------- edge position class and symmetry ----------------
    // the class is the index of the orbit representative (the
    // lexicographically smallest conjugate) in the sorted table
    uint32_t rep_lehmer = piece_geometry::PosPermToLehmerPos(edge_perm);
    std::array<uint8_t, kNumEdges> rep_perm = edge_perm;
    for (uint8_t sym = 0; sym < edge::kNumSym; sym++) {
        std::array<uint8_t, kNumEdges> img =
            piece_geometry::SymmetryPositionRotation(data.edge_data.idx_piece_sym, edge_perm, data.edge_data.sym_trans[sym]);
        uint32_t img_lehmer = piece_geometry::PosPermToLehmerPos(img);
        if (img_lehmer < rep_lehmer) {
            rep_lehmer = img_lehmer;
            rep_perm = img;
        }
    }
    auto rep_it = std::lower_bound(edge::rep_lehmer_pos.begin(), edge::rep_lehmer_pos.end(), rep_lehmer);
    if (rep_it == edge::rep_lehmer_pos.end() || *rep_it != rep_lehmer) {
        result.error = PositionError::kInternal;
        result.message = "representative position not found in the precomputation";
        return result;
    }
    uint32_t edge_pos = uint32_t(rep_it - edge::rep_lehmer_pos.begin());
    // the symmetry is the first one mapping the representative to the input
    uint8_t edge_sym = 0;
    bool sym_found = false;
    for (uint8_t sym = 0; sym < edge::kNumSym; sym++) {
        if (piece_geometry::SymmetryPositionRotation(data.edge_data.idx_piece_sym, rep_perm, data.edge_data.sym_trans[sym]) == edge_perm) {
            edge_sym = sym;
            sym_found = true;
            break;
        }
    }
    if (!sym_found) {
        result.error = PositionError::kInternal;
        result.message = "symmetry of the input position not found";
        return result;
    }

    // ---------------- edge orientation ----------------
    // the orientation is stored in the symmetry frame of the state: the
    // marker directions are transformed with the state's symmetry (slots
    // relabeled, directions rotated) and are given relative to the frame's
    // baseline (the transformed orientation of the solved state)
    uint16_t baseline = 0;
    for (uint8_t slot = 0; slot < kNumEdges; slot++) {
        uint8_t frame_slot = data.edge_data.idx_piece_sym[edge_sym][slot];
        Vec3i frame_dir = MatVecMul(data.edge_data.idx_to_mat_sym[edge_sym], data.edge_data.idx_to_xyz_orient[slot][0]);
        baseline |= data.edge_data.xyz_to_idx_orient[frame_slot].at(frame_dir) << frame_slot;
    }
    uint16_t edge_bits = 0;
    for (uint8_t slot = 0; slot < kNumEdges; slot++) {
        uint8_t frame_slot = data.edge_data.idx_piece_sym[edge_sym][slot];
        Vec3i frame_dir = MatVecMul(data.edge_data.idx_to_mat_sym[edge_sym], primary_dirs[slot]);
        auto orient_it = data.edge_data.xyz_to_idx_orient[frame_slot].find(frame_dir);
        if (orient_it == data.edge_data.xyz_to_idx_orient[frame_slot].end()) {
            result.error = PositionError::kInvalidAssignment;
            result.message = "edge sticker does not point along the slot";
            return result;
        }
        edge_bits |= orient_it->second << frame_slot;
    }
    edge_bits ^= baseline;
    // the last orientation bit is implied by the parity
    if (std::popcount(edge_bits) % 2 == 1) {
        result.error = PositionError::kEdgeOrientParity;
        result.message = "edge orientation parity is wrong (the last edge is flipped)";
        return result;
    }
    uint16_t edge_orient = edge_bits & (edge::kNumOrient-1);

    // ---------------- solvability ----------------
    // the corner sub state has to be reachable from the solved state
    // (unvisited and illegal states keep the zero word in the heuristic)
    if (corner::GetHeuristic(corner_pos, corner_orient) == 0) {
        result.error = PositionError::kCornerUnsolvable;
        result.message = "the corner position is not solvable (bandaged)";
        return result;
    }
    // every move permutes edges and corners with the same parity, so the
    // permutations of edges and corners have to match
    if (PermutationParity(edge_perm) != PermutationParity(corner_perm)) {
        result.error = PositionError::kParityMismatch;
        result.message = "edge and corner permutation parities differ - the position is not solvable";
        return result;
    }

    result.ok = true;
    result.state = State{edge_pos, corner_pos, corner_orient, edge_orient, edge_sym};
    LOG_EXTRA("converted input position: edge_pos", edge_pos, "corner_pos", corner_pos,
              "corner_orient", corner_orient, "edge_orient", edge_orient, "edge_sym", edge_sym);
    return result;
}


}
