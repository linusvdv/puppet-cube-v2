"""Round trip test of the user input position conversion.

1. Sticker table check: the derived sticker face directions are validated by
   ray casting against the posed meshes.

2. Round trip check: scrambles from the solver are replayed through the
   VisualCube; the piece placements extracted from the model must convert
   (puppetpy.convert_position) back to exactly the internal state the solver
   computed for the same moves (puppetpy.state_after_moves).

3. Error cases: a flipped edge, a twisted corner, a mirrored corner, a
   duplicate piece and a parity mismatch must be rejected with the
   corresponding error.

Usage: python3 convert_test.py [--module_path PATH] [--runs N] [--threads N]
"""

import argparse
import copy
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "gui"))

from cube_model import VisualCube, derive_moves
from pieces import placements_from_visual_cube, swap_placements, verify_sticker_faces


def find_puppetpy(module_path):
    if module_path is None:
        base = os.path.dirname(os.path.abspath(__file__))
        for candidate in (os.path.join(base, "..", "build_cont"),
                          os.path.join(base, "..", "build")):
            if os.path.isdir(candidate) and any(f.startswith("puppetpy") for f in os.listdir(candidate)):
                sys.path.insert(0, candidate)
                break
    else:
        sys.path.insert(0, module_path)
    import puppetpy
    return puppetpy


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--module_path", default=None)
    parser.add_argument("--runs", type=int, default=10)
    parser.add_argument("--threads", type=int, default=8)
    args = parser.parse_args()

    puppetpy = find_puppetpy(args.module_path)
    moves = derive_moves(puppetpy.rotation_reps())

    print("== sticker table check (ray casting) ==")
    verify_sticker_faces()
    print("sticker face table matches all 54 ray cast hits")

    root_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..") + os.sep
    puppetpy.initialize(["--root_path", root_path, "-l", "warning",
                         "-r", str(args.runs), "-t", str(args.threads)])

    print("== round trip check ==")
    events = []
    puppetpy.run(lambda event: events.append(event))
    runs = {}
    for event in events:
        runs.setdefault(event.run_idx, []).append(event)
    assert runs, "no runs"

    last = None
    replays = {}
    for run_idx in sorted(runs):
        scramble = next(e for e in runs[run_idx] if str(e.kind) == "SearchEventKind.SCRAMBLE")
        expected = puppetpy.state_after_moves(scramble.moves)

        cube = VisualCube()
        for move_idx in scramble.moves:
            cube.apply_move(moves[move_idx])
        corners, edges = placements_from_visual_cube(cube)
        replays[run_idx] = (corners, edges)
        last = (corners, edges, scramble.moves)
        ok, state, error, message = puppetpy.convert_position(corners, edges)
        assert ok, f"run {run_idx}: conversion failed: {error} {message}"
        assert tuple(state) == tuple(expected), \
            f"run {run_idx}: converted state {tuple(state)} != solver state {tuple(expected)}"
        print(f"run {run_idx}: {len(scramble.moves)} moves -> state matches "
              f"(edge_pos={state[0]}, corner_pos={state[1]}, sym={state[4]})")

    print("== error cases ==")
    corners, edges, _ = last

    # solved cube must convert to the solved state
    cube = VisualCube()
    corners0, edges0 = placements_from_visual_cube(cube)
    ok, state, error, message = puppetpy.convert_position(corners0, edges0)
    assert ok and tuple(state) == (0, 0, 0, 0, 0), f"solved cube: {ok} {state} {error} {message}"
    print("solved cube -> solved state")

    # one flipped edge (its two stickers swap faces) breaks the orientation parity
    bad = copy.deepcopy(edges)
    bad[0] = (bad[0][0], bad[0][1], bad[0][2], [bad[0][3][1], bad[0][3][0]])
    ok, state, error, message = puppetpy.convert_position(corners, bad)
    assert not ok and str(error) == "PositionError.EDGE_ORIENT_PARITY", f"flipped edge: {error} {message}"
    print("flipped edge -> EDGE_ORIENT_PARITY")

    # one twisted corner (cyclic sticker rotation) breaks the corner parity
    bad = copy.deepcopy(corners)
    bad[0] = (bad[0][0], bad[0][1], bad[0][2], [bad[0][3][1], bad[0][3][2], bad[0][3][0]])
    ok, state, error, message = puppetpy.convert_position(bad, edges)
    assert not ok and str(error) == "PositionError.CORNER_ORIENT_PARITY", f"twisted corner: {error} {message}"
    print("twisted corner -> CORNER_ORIENT_PARITY")

    # one mirrored corner (two stickers swap faces) is a reflection
    bad = copy.deepcopy(corners)
    bad[0] = (bad[0][0], bad[0][1], bad[0][2], [bad[0][3][1], bad[0][3][0], bad[0][3][2]])
    ok, state, error, message = puppetpy.convert_position(bad, edges)
    assert not ok and str(error) == "PositionError.REFLECTED_CORNER", f"mirrored corner: {error} {message}"
    print("mirrored corner -> REFLECTED_CORNER")

    # a duplicated piece is rejected
    bad = copy.deepcopy(edges)
    bad[1] = (bad[1][0], bad[0][1], bad[0][2], bad[1][3])
    ok, state, error, message = puppetpy.convert_position(corners, bad)
    assert not ok and str(error) == "PositionError.DUPLICATE_PIECE", f"duplicate piece: {error} {message}"
    print("duplicate piece -> DUPLICATE_PIECE")

    # a single edge swap breaks the permutation parity (repair the edge
    # orientation parity if the swap also flipped an edge); the swap has to
    # be between slots with the same axis pair so the sticker directions
    # stay valid
    same_axis = None
    for i in range(len(edges)):
        for j in range(i+1, len(edges)):
            if {k for k, x in enumerate(edges[i][0]) if x != 0} == {k for k, x in enumerate(edges[j][0]) if x != 0}:
                same_axis = (i, j)
                break
        if same_axis:
            break
    bad = copy.deepcopy(edges)
    swap_placements(bad, same_axis[0], same_axis[1])
    ok, state, error, message = puppetpy.convert_position(corners, bad)
    if ok or str(error) != "PositionError.PARITY_MISMATCH":
        bad[2] = (bad[2][0], bad[2][1], bad[2][2], [bad[2][3][1], bad[2][3][0]])
        ok, state, error, message = puppetpy.convert_position(corners, bad)
    assert not ok and str(error) == "PositionError.PARITY_MISMATCH", f"swapped edges: {error} {message}"
    print("swapped edges -> PARITY_MISMATCH")

    print("== sufficiency check (recombined positions must solve) ==")
    # combine the edges of one scramble with the corners of another: such a
    # position is not derived from a move sequence; when it passes all
    # checks (the permutation parities have to match by chance) it has to be
    # solvable
    solved_cnt = 0
    for i in sorted(replays):
        for j in sorted(replays):
            if i == j:
                continue
            corners_mix, edges_mix = replays[j][0], replays[i][1]
            ok, state, error, message = puppetpy.convert_position(corners_mix, edges_mix)
            if not ok:
                assert str(error) == "PositionError.PARITY_MISMATCH", \
                    f"runs {i}/{j}: unexpected rejection {error} {message}"
                continue
            events = []
            puppetpy.solve_position(corners_mix, edges_mix, lambda e: events.append(e))
            solution = next(e for e in events if str(e.kind) == "SearchEventKind.SOLUTION")
            final = puppetpy.state_after_moves(solution.moves, list(state))
            assert tuple(final) == (0, 0, 0, 0, 0), \
                f"runs {i}/{j}: solution of depth {solution.depth} does not solve the position"
            solved_cnt += 1
            print(f"edges of run {i} + corners of run {j}: solved with depth {solution.depth}")
            if solved_cnt >= 3:
                break
        if solved_cnt >= 3:
            break
    assert solved_cnt >= 1, "no parity matching recombination found (unlikely)"
    print(f"{solved_cnt} recombined positions solved")

    print("convert_test PASSED")


if __name__ == "__main__":
    main()
