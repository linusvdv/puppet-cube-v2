"""Verification of the visual cube model against the solver.

1. Move derivation check: the visual move data derived from the solver
   rotation representations (puppetpy.rotation_reps()) is compared against
   the matura renderer reference table (kPieceRotationMatrices from
   additional/matura_with_rendering/src/renderer.cpp) and the solver
   matrices of the old matura version - proving the V1/V2 move semantics
   parity.

   All moves match except S/S': the matura renderer had the S/S' animation
   axes flipped. The frame invariant (visual = rotation_offset * bookkeeping)
   requires the middle slice to animate with the INVERSE of the solver slice
   matrix (like M and E do) - the old S rows animated with the matrix itself,
   which desyncs the visual and bookkeeping frames. The derived axes follow
   the uniform rule and the replay test below proves them correct.

2. Replay check: scrambles + solutions from the solver are replayed through
   the VisualCube. Asserted per run:
   - all bookkeeping grid positions return home (the replay is solver-correct)
   - every visual rotation equals rotation_offset @ solved_rotation
     (the cube ends visually solved up to the tracked whole cube rotation)
   - the accumulated rotation_offset is an exact 90 degree multiple rotation

Usage: python3 verify_model.py [--module_path PATH] [--runs N] [--threads N]
"""

import argparse
import os
import sys

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "gui"))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build_cont"))

import puppetpy

from cube_model import VisualCube, derive_moves, rotation_about
from geometry import SOLVED_PIECES


# matura renderer reference table: (name, relevant_index, relevant_value, matrix, visual axis, old axis flipped)
# from additional/matura_with_rendering/src/renderer.cpp kPieceRotationMatrices
# the matrices are simultaneously the old solver matrices (kCornerRotation/kEdgeRotation)
REFERENCE_MOVES = [
    ("R",  0,  1, [[1, 0, 0], [0, 0, 1], [0, -1, 0]], (-1, 0, 0), False),
    ("R'", 0,  1, [[1, 0, 0], [0, 0, -1], [0, 1, 0]], (1, 0, 0), False),
    ("L",  0, -1, [[1, 0, 0], [0, 0, -1], [0, 1, 0]], (1, 0, 0), False),
    ("L'", 0, -1, [[1, 0, 0], [0, 0, 1], [0, -1, 0]], (-1, 0, 0), False),
    ("U",  1,  1, [[0, 0, -1], [0, 1, 0], [1, 0, 0]], (0, -1, 0), False),
    ("U'", 1,  1, [[0, 0, 1], [0, 1, 0], [-1, 0, 0]], (0, 1, 0), False),
    ("D",  1, -1, [[0, 0, 1], [0, 1, 0], [-1, 0, 0]], (0, 1, 0), False),
    ("D'", 1, -1, [[0, 0, -1], [0, 1, 0], [1, 0, 0]], (0, -1, 0), False),
    ("F",  2,  1, [[0, 1, 0], [-1, 0, 0], [0, 0, 1]], (0, 0, -1), False),
    ("F'", 2,  1, [[0, -1, 0], [1, 0, 0], [0, 0, 1]], (0, 0, 1), False),
    ("B",  2, -1, [[0, -1, 0], [1, 0, 0], [0, 0, 1]], (0, 0, 1), False),
    ("B'", 2, -1, [[0, 1, 0], [-1, 0, 0], [0, 0, 1]], (0, 0, -1), False),
    ("M",  0,  0, [[1, 0, 0], [0, 0, 1], [0, -1, 0]], (1, 0, 0), False),
    ("M'", 0,  0, [[1, 0, 0], [0, 0, -1], [0, 1, 0]], (-1, 0, 0), False),
    ("E",  1,  0, [[0, 0, -1], [0, 1, 0], [1, 0, 0]], (0, 1, 0), False),
    ("E'", 1,  0, [[0, 0, 1], [0, 1, 0], [-1, 0, 0]], (0, -1, 0), False),
    ("S",  2,  0, [[0, -1, 0], [1, 0, 0], [0, 0, 1]], (0, 0, 1), True),
    ("S'", 2,  0, [[0, 1, 0], [-1, 0, 0], [0, 0, 1]], (0, 0, -1), True),
]


def check_move_derivation(moves):
    print("== move derivation check ==")
    assert len(moves) == len(REFERENCE_MOVES)
    for move, (name, rel_idx, rel_val, matrix, axis, old_flipped) in zip(moves, REFERENCE_MOVES):
        assert move.name == name, (move.name, name)
        assert move.axis == rel_idx, (name, move.axis, rel_idx)
        assert move.relevant_value == rel_val, (name, move.relevant_value, rel_val)
        assert np.array_equal(move.matrix, np.array(matrix)), name
        # the flipped S/S' axes of the matura renderer are corrected by the derivation
        expected_axis = np.array(axis, dtype=float) * (-1.0 if old_flipped else 1.0)
        assert np.allclose(move.visual_axis, expected_axis), (name, move.visual_axis, expected_axis)
    print(f"all {len(moves)} moves match the reference (S/S' axes corrected)")


def check_replay(moves, num_runs, num_threads):
    print("== replay check ==")
    root_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..") + os.sep
    puppetpy.initialize(["--root_path", root_path, "-l", "warning",
                         "-r", str(num_runs), "-t", str(num_threads)])

    events = []
    puppetpy.run(lambda event: events.append(event))

    runs = {}
    for event in events:
        runs.setdefault(event.run_idx, []).append(event)

    num_slice_moves = 0
    for run_idx in sorted(runs):
        scramble = next(e for e in runs[run_idx] if str(e.kind) == "SearchEventKind.SCRAMBLE")
        solution = next(e for e in runs[run_idx] if str(e.kind) == "SearchEventKind.SOLUTION")
        num_slice_moves += sum(1 for m in scramble.moves + solution.moves if m >= 12)

        cube = VisualCube()
        for move_idx in scramble.moves + solution.moves:
            cube.apply_move(moves[move_idx])

        assert cube.is_solved_bookkeeping(), f"run {run_idx}: bookkeeping not home"
        offset = cube.rotation_offset
        assert np.allclose(offset @ offset.T, np.eye(3), atol=1e-9), f"run {run_idx}: offset not orthogonal"
        assert np.allclose(offset, offset.round(), atol=1e-9), f"run {run_idx}: offset not a 90 degree multiple"
        for i, (_, grid, solved_rotation) in enumerate(SOLVED_PIECES):
            if i >= 6:
                # edges and corners are tracked by the search: exact match required
                assert np.allclose(cube.rotations[i], offset @ solved_rotation, atol=1e-9), \
                    f"run {run_idx}: piece {i} not solved up to the frame offset"
            else:
                # centers are not tracked by the search: they can be spun around
                # their own axis by the replay (invisible on the symmetric piece)
                axis = np.zeros(3)
                axis[[j for j in range(3) if grid[j] != 0][0]] = 1.0
                relative = offset.T @ cube.rotations[i] @ solved_rotation.T
                assert any(np.allclose(relative, rotation_about(axis, k * np.pi / 2), atol=1e-9)
                           for k in range(4)), \
                    f"run {run_idx}: center piece {i} not solved up to spin and frame offset"
        print(f"run {run_idx}: {len(scramble.moves)} scramble moves + {len(solution.moves)} solution moves "
              f"(depth {solution.depth}) - solved up to frame offset "
              f"{np.round(offset).astype(int).tolist()}")

    assert num_slice_moves > 0, "no slice moves in the replay - test would be trivial"
    print(f"replay included {num_slice_moves} slice moves (frame invariant held through all of them)")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--runs", type=int, default=2)
    parser.add_argument("--threads", type=int, default=8)
    args = parser.parse_args()

    moves = derive_moves(puppetpy.rotation_reps())
    check_move_derivation(moves)
    check_replay(moves, args.runs, args.threads)
    print("MODEL VERIFICATION PASSED")


if __name__ == "__main__":
    main()
