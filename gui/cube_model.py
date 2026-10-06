"""Physical move and animation model of the visual cube.

Port of the matura renderer rotation logic (renderer.cpp). The move data
(name, matrix, activate) is taken from the solver rotation representations
(src/rotation.cpp) exposed by the puppetpy module - the visual animation
axes are derived from the solver matrices:

- face moves (R, L, U, D, F, B) animate the face layer rotating exactly
  like the solver matrix,
- slice moves (M, E, S) rotate the outer layers in the search (M = R + L',
  all 6 centers stay fixed). Visually the middle slice is animated instead
  (the natural look of the move, turning the opposite direction) while the
  bookkeeping (the integer grid positions used for the layer selection of
  the following moves) and the rotation_offset frame compensation track the
  difference - exactly like the matura renderer.

As a consequence a scramble+solution replay ends visually solved up to a
whole cube rotation (the accumulated rotation_offset).
"""

import math

import numpy as np

from geometry import SOLVED_PIECES, NUM_PIECES

NUM_MOVES = 18
FIRST_SLICE_MOVE = 12  # M, M', E, E', S, S'


def _rodrigues_axis(matrix):
    """Rotation axis (right hand rule, unit vector) of a 90 degree rotation matrix."""
    return np.array([matrix[2][1] - matrix[1][2],
                     matrix[0][2] - matrix[2][0],
                     matrix[1][0] - matrix[0][1]], dtype=float) / 2.0


class Move:
    """Visual move information derived from a solver rotation representation."""

    def __init__(self, name, index, matrix, activate):
        self.name = name
        self.index = index
        self.matrix = np.array(matrix, dtype=int)  # solver int matrix
        self.activate = activate
        self.axis = next(i for i in range(3) if matrix[i][i] != 0)  # rotation axis index
        self.is_slice = (activate == 0)

        # the visual animation of a face move rotates the face layer following the matrix
        # a slice move animates the middle slice turning the opposite direction
        axis_vec = _rodrigues_axis(matrix)
        self.visual_axis = -axis_vec if self.is_slice else axis_vec

        # grid coordinate of the animated layer
        self.relevant_value = 0 if self.is_slice else activate


def derive_moves(rotation_reps):
    """Derives the visual move data from the solver rotation representations."""
    moves = [None] * NUM_MOVES
    for rep in rotation_reps:
        moves[rep.index] = Move(rep.name, rep.index, rep.matrix, rep.activate)
    assert all(move is not None for move in moves)
    return moves


def rotation_about(axis, angle):
    """Rotation matrix (right hand rule) around a unit axis."""
    axis = np.asarray(axis, dtype=float)
    axis = axis / np.linalg.norm(axis)
    cross = np.array([[0.0, -axis[2], axis[1]],
                      [axis[2], 0.0, -axis[0]],
                      [-axis[1], axis[0], 0.0]])
    return np.eye(3) + math.sin(angle) * cross + (1.0 - math.cos(angle)) * (cross @ cross)


class VisualCube:
    """The 26 piece visual cube with physical slice move animation."""

    def __init__(self):
        self.reset()

    def reset(self):
        """Resets to the solved state."""
        self.grids = [np.array(grid, dtype=int) for _, grid, _ in SOLVED_PIECES]
        self.rotations = [rotation.copy() for _, _, rotation in SOLVED_PIECES]
        self.current = [False] * NUM_PIECES
        # frame compensation between the visual and the bookkeeping frame
        self.rotation_offset = np.eye(3)
        # active move state
        self.active_move = None
        self.active_axis = None

    def begin_move(self, move):
        """Starts a move: marks the animated pieces.

        Returns the indices of the animated pieces and the visual rotation
        axis (a unit vector, already corrected by the frame offset).
        """
        self.active_move = move
        self.active_axis = self.rotation_offset @ move.visual_axis
        self.current = [self.grids[i][move.axis] == move.relevant_value for i in range(NUM_PIECES)]
        return [i for i in range(NUM_PIECES) if self.current[i]], self.active_axis

    def pose(self, piece_idx, angle):
        """Visual rotation matrix of a piece at the current animation angle (radians)."""
        if self.current[piece_idx]:
            return rotation_about(self.active_axis, angle) @ self.rotations[piece_idx]
        return self.rotations[piece_idx]

    def end_move(self):
        """Completes the active move (the visual 90 degree turn is done)."""
        move = self.active_move
        rotation = rotation_about(self.active_axis, math.pi / 2)
        for i in range(NUM_PIECES):
            if self.current[i]:
                # the animated pieces visually rotated
                self.rotations[i] = rotation @ self.rotations[i]
                # the bookkeeping of face moves follows the animated layer
                if not move.is_slice:
                    self.grids[i] = move.matrix @ self.grids[i]
            elif move.is_slice:
                # slice moves rotate the outer pieces in the bookkeeping
                self.grids[i] = move.matrix @ self.grids[i]
        self.current = [False] * NUM_PIECES
        if move.is_slice:
            self.rotation_offset = rotation @ self.rotation_offset
        self.active_move = None
        self.active_axis = None

    def apply_move(self, move):
        """Applies a complete move without animation."""
        self.begin_move(move)
        self.end_move()

    def is_solved_bookkeeping(self):
        """All pieces are home in the bookkeeping (solver) frame."""
        return all(np.array_equal(self.grids[i], np.array(SOLVED_PIECES[i][1]))
                   for i in range(NUM_PIECES))

    def is_solved_visual(self):
        """All pieces are visually home (bookkeeping home and no frame offset)."""
        return self.is_solved_bookkeeping() and np.allclose(self.rotation_offset, np.eye(3))
