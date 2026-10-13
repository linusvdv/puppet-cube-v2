"""Generates the web test position: replays the run-0 scramble (from
web/tests/testdata/scrambles.json, written by tests/node_test.js) on the VisualCube and
extracts the piece placements (the same path the qt editor uses).

Also writes the native reference state (state_after_moves of the native
module) so node_test.js can cross-check the wasm rotations.

Run:  source .venv_cont/bin/activate && python3 web/tools/make_placements.py
"""

import json
import os
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
sys.path.insert(0, os.path.join(REPO, "gui"))
sys.path.insert(0, os.path.join(REPO, "build_cont_mem"))

from cube_model import VisualCube, derive_moves  # noqa: E402
from pieces import placements_from_visual_cube  # noqa: E402


def placement_wire(placement):
    """One placement as the flat int array the wasm binding expects."""
    slot, piece, home_dirs, cur_dirs = placement
    flat = [int(c) for c in slot] + [int(c) for c in piece]
    for home, cur in zip(home_dirs, cur_dirs):
        flat += [int(c) for c in home] + [int(c) for c in cur]
    return flat


def placement_tuple(placement):
    """One placement as the nested lists the puppetpy binding expects."""
    slot, piece, home_dirs, cur_dirs = placement
    return ([[int(c) for c in slot], [int(c) for c in piece],
             [[int(c) for c in d] for d in home_dirs],
             [[int(c) for c in d] for d in cur_dirs]])


def main():
    with open(os.path.join(REPO, "web", "tests", "testdata", "scrambles.json")) as f:
        scrambles = json.load(f)

    import puppetpy  # noqa: E402
    moves = derive_moves(puppetpy.rotation_reps())  # index compatible with the solver

    # the run-0 scramble replayed on the visual cube
    cube = VisualCube()
    for rot in scrambles[0]:
        cube.apply_move(moves[rot])
    corners, edges = placements_from_visual_cube(cube)
    out = os.path.join(REPO, "web", "tests", "testdata", "run0_placements.json")
    with open(out, "w") as f:
        json.dump({"corners": [placement_wire(p) for p in corners],
                   "edges": [placement_wire(p) for p in edges]}, f)
    print(f"wrote {out} ({len(corners)} corners, {len(edges)} edges)")
    corners = [placement_tuple(p) for p in corners]
    edges = [placement_tuple(p) for p in edges]

    # the native reference state of the same scramble (uses the native
    # module; reads the existing precomputation files, no search)
    puppetpy.initialize(["--root_path", REPO + os.sep, "-l", "critical", "--use_cuda=false"])
    state = puppetpy.state_after_moves(scrambles[0])
    out = os.path.join(REPO, "web", "tests", "testdata", "run0_state.json")
    with open(out, "w") as f:
        json.dump(list(state), f)
    print(f"wrote {out} (state {list(state)})")

    # the native reference solve of the same position (depth + positions -
    # the cross-check for the wasm search)
    print("solving the position natively (cpu mode) ...")
    events = []
    puppetpy.solve_position(corners, edges, lambda ev: events.append(ev))
    solution = [ev for ev in events if ev.kind == puppetpy.SearchEventKind.SOLUTION][0]
    print(f"native solve: depth {solution.depth} ({len(solution.moves)} moves)")


if __name__ == "__main__":
    main()
