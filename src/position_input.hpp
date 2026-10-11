#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "cube.hpp"


namespace input {


// placement of a single piece of a user input cube: the grid coordinates of
// the slot and of the piece's home position; for every sticker of the piece
// its home face direction and the face direction it occupies in the input
// cube (signed unit axes like (0, 1, 0))
struct PiecePlacement {
    std::array<int, 3> slot{};
    std::array<int, 3> piece{};
    std::vector<std::array<int, 3>> sticker_home_dirs{};
    std::vector<std::array<int, 3>> sticker_cur_dirs{};
};


enum class PositionError {
    kNone,
    kInvalidInput,       // wrong number of placements
    kInvalidSlot,        // slot or piece is not a valid position
    kDuplicatePiece,     // a piece appears more than once
    kInvalidAssignment,  // sticker directions do not match the slot/piece
    kReflectedCorner,    // corner sticker assignment is a reflection
    kCornerOrientParity, // the last corner orientation is wrong
    kEdgeOrientParity,   // the last edge orientation is wrong
    kCornerUnsolvable,   // corner state is not reachable (bandaged)
    kParityMismatch,     // edge and corner permutation parity differ
    kInternal,           // conversion data inconsistent (should not happen)
};


struct PositionResult {
    bool ok = false;
    State state{};
    PositionError error = PositionError::kNone;
    std::string message;
};


// converts a user input cube (piece placements) into the internal state
// representation and checks that the position is solvable
PositionResult ConvertPosition(const std::vector<PiecePlacement>& corners,
                                const std::vector<PiecePlacement>& edges);


}
