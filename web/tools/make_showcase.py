"""Generates web/hardest.json: the hardest proven position (31 moves).

Runs the native solver (build_cont: CUDA, full tables) on the showcase
settings -r 1 -m 27 --run_offset 6023 (the -m 27 scramble hunt takes
~150k moves - far too many to display, so the scrambled position is
stored as piece PLACEMENTS instead) and the 31-move optimal solution as
move indices. The web app displays the position via the custom-run
machinery (initial placement + solution) - no solver tables needed.

Usage: source .venv_cont/bin/activate && python3 web/tools/make_showcase.py
(the proof takes ~2 minutes on a 4070)
"""

import json
import os
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
sys.path.insert(0, os.path.join(REPO, "gui"))
sys.path.insert(0, os.path.join(REPO, "build_cont"))

from cube_model import VisualCube, derive_moves  # noqa: E402
from pieces import placements_from_visual_cube  # noqa: E402

RUN_OFFSET = 6023
MIN_CORNER_HEURISTIC = 27
EXPECTED_DEPTH = 31


def placement_wire(placement):
    slot, piece, home_dirs, cur_dirs = placement
    flat = [int(c) for c in slot] + [int(c) for c in piece]
    for home, cur in zip(home_dirs, cur_dirs):
        flat += [int(c) for c in home] + [int(c) for c in cur]
    return flat


def placement_tuple(placement):
    slot, piece, home_dirs, cur_dirs = placement
    return ([[int(c) for c in slot], [int(c) for c in piece],
             [[int(c) for c in d] for d in home_dirs],
             [[int(c) for c in d] for d in cur_dirs]])


def main():
    import puppetpy
    puppetpy.initialize(["--root_path", REPO + os.sep, "-l", "warning",
                         "-r", "1", "-m", str(MIN_CORNER_HEURISTIC),
                         "--run_offset", str(RUN_OFFSET)])

    events = []
    puppetpy.run(events.append)

    scramble = None
    solution = None
    depth = None
    for ev in events:
        if ev.kind == puppetpy.SearchEventKind.SCRAMBLE and ev.run_idx == 0:
            scramble = list(ev.moves)
        elif ev.kind == puppetpy.SearchEventKind.SOLUTION and ev.run_idx == 0:
            solution = list(ev.moves)
            depth = ev.depth
    if scramble is None or solution is None:
        raise SystemExit("did not receive the run-0 scramble/solution events")
    if depth != EXPECTED_DEPTH:
        raise SystemExit(f"unexpected depth {depth} (expected {EXPECTED_DEPTH})")

    # verify: replaying scramble + solution from solved must end solved
    state = list(puppetpy.state_after_moves(scramble + solution))
    if state != [0, 0, 0, 0, 0]:
        raise SystemExit(f"scramble+solution does not end solved: {state}")

    # the scrambled position as piece placements (the displayable form)
    print(f"replaying the {len(scramble)} scramble moves on the visual cube ...")
    moves = derive_moves(puppetpy.rotation_reps())
    cube = VisualCube()
    for rot in scramble:
        cube.apply_move(moves[rot])
    corners, edges = placements_from_visual_cube(cube)

    # the placements must convert back to exactly the scrambled state
    ok, converted, error, message = puppetpy.convert_position(
        [placement_tuple(p) for p in corners], [placement_tuple(p) for p in edges])
    if not ok or list(converted) != list(puppetpy.state_after_moves(scramble)):
        raise SystemExit(f"placement round trip failed: ok={ok} {message}")

    out = {
        "depth": depth,
        "placements": {"corners": [placement_wire(p) for p in corners],
                       "edges": [placement_wire(p) for p in edges]},
        "solution": solution,
    }
    path = os.path.join(REPO, "web", "hardest.json")
    with open(path, "w") as f:
        json.dump(out, f, separators=(",", ":"))
    print(f"wrote {path}: depth {depth}, {len(corners)} corners, "
          f"{len(edges)} edges, {len(solution)} solution moves")


if __name__ == "__main__":
    main()
