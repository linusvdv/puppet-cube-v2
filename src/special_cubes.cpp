#include "cube.hpp"


State HardestCubeMatura() {
    State state = kSolvedState;
    state = Cube::Rotate(state, int(Rotations::kDc)).second;
    state = Cube::Rotate(state, int(Rotations::kLc)).second;
    state = Cube::Rotate(state, int(Rotations::kUc)).second;
    state = Cube::Rotate(state, int(Rotations::kL)).second;
    state = Cube::Rotate(state, int(Rotations::kF)).second;
    state = Cube::Rotate(state, int(Rotations::kL)).second;
    state = Cube::Rotate(state, int(Rotations::kF)).second;
    state = Cube::Rotate(state, int(Rotations::kDc)).second;
    state = Cube::Rotate(state, int(Rotations::kMc)).second;
    state = Cube::Rotate(state, int(Rotations::kD)).second;
    state = Cube::Rotate(state, int(Rotations::kL)).second;
    state = Cube::Rotate(state, int(Rotations::kEc)).second;
    state = Cube::Rotate(state, int(Rotations::kF)).second;
    state = Cube::Rotate(state, int(Rotations::kM)).second;
    state = Cube::Rotate(state, int(Rotations::kBc)).second;
    state = Cube::Rotate(state, int(Rotations::kL)).second;
    state = Cube::Rotate(state, int(Rotations::kBc)).second;
    state = Cube::Rotate(state, int(Rotations::kUc)).second;
    state = Cube::Rotate(state, int(Rotations::kR)).second;
    state = Cube::Rotate(state, int(Rotations::kL)).second;
    state = Cube::Rotate(state, int(Rotations::kB)).second;
    state = Cube::Rotate(state, int(Rotations::kR)).second;
    state = Cube::Rotate(state, int(Rotations::kFc)).second;
    state = Cube::Rotate(state, int(Rotations::kFc)).second;
    state = Cube::Rotate(state, int(Rotations::kL)).second;
    state = Cube::Rotate(state, int(Rotations::kD)).second;
    state = Cube::Rotate(state, int(Rotations::kD)).second;
    state = Cube::Rotate(state, int(Rotations::kFc)).second;
    state = Cube::Rotate(state, int(Rotations::kUc)).second;
    state = Cube::Rotate(state, int(Rotations::kUc)).second;
    Cube cur = Cube();
    LOG_EXTRA("HARDEST:", int(cur.GetMaxHeuristic(state)));
    return state;
}
