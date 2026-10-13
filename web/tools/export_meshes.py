"""Exports the piece geometry of the viewer and the editor to JSON for the
web frontend (web/meshes.json, web/editor_meshes.json, web/rotation_reps.json).

Run:  source .venv_cont/bin/activate && python3 web/tools/export_meshes.py
"""

import json
import os
import sys

import numpy as np

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
sys.path.insert(0, os.path.join(REPO, "gui"))

from geometry import COLORS, PIECE_COLOR_TYPES, SOLVED_PIECES, get_piece_geometry  # noqa: E402
from pieces import BASE_GROUP_FACES, sticker_tables  # noqa: E402
from editor import editor_geometry, editor_group_faces, editor_placements  # noqa: E402


def piece_json(piece):
    return {
        "vertices": np.asarray(piece["vertices"], dtype=float).tolist(),
        "normals": np.asarray(piece["normals"], dtype=float).tolist(),
        "groups": [{"faces": np.asarray(faces, dtype=int).tolist(),
                    "color": [float(c) for c in color]}
                   for faces, color in piece["groups"]],
        "lines": np.asarray(piece["lines"], dtype=float).tolist(),
    }


def main():
    out_dir = os.path.join(REPO, "web")

    # the rotation representations for the frontend move derivation
    # (derive_moves in cube_model.js) - static data from the native module
    sys.path.insert(0, os.path.join(REPO, "build_cont_mem"))
    import puppetpy  # noqa: E402
    reps = puppetpy.rotation_reps()
    out = os.path.join(out_dir, "rotation_reps.json")
    with open(out, "w") as f:
        json.dump([{"name": r.name, "index": r.index,
                    "matrix": np.asarray(r.matrix, dtype=int).reshape(-1).tolist(),
                    "activate": r.activate} for r in reps], f, separators=(",", ":"))
    print(f"wrote {out} ({os.path.getsize(out)} bytes)")

    geometry = get_piece_geometry()
    sticker_faces, sticker_colors = sticker_tables()

    data = {
        # face colors in the order used by PIECE_COLOR_TYPES (Y O G R B W)
        "colors": [[float(c) for c in col] for col in COLORS],
        # per piece: (type, solved grid, solved rotation)
        "solved": [{"type": int(t), "grid": [int(c) for c in grid],
                    "rotation": np.asarray(rot, dtype=float).tolist()}
                   for t, grid, rot in SOLVED_PIECES],
        # per piece: home face direction of every color group (group order)
        "stickerFaces": [[[int(x) for x in d] for d in faces] for faces in sticker_faces],
        # per piece: color index of every color group
        "stickerColors": [list(colors) for colors in sticker_colors],
        # the renderable geometry of the viewer (canonical solved pose)
        "pieces": [piece_json(p) for p in geometry],
        # face directions of the color groups in the BASE mesh (per type)
        "baseGroupFaces": {str(t): [[int(x) for x in f] for f in faces]
                           for t, faces in BASE_GROUP_FACES.items()},
    }
    out = os.path.join(out_dir, "meshes.json")
    with open(out, "w") as f:
        json.dump(data, f, separators=(",", ":"))
    print(f"wrote {out} ({os.path.getsize(out) // 1024} KB)")

    editor = {
        "pieces": [piece_json(p) for p in editor_geometry()],
        "rotations": [np.asarray(r, dtype=float).tolist() for r in editor_placements()],
        # face direction of every group per piece IN THE POSED EDITOR SCENE
        # (corners: the axis-mapped order, differs from the puppet faces)
        "groupFaces": [[[int(x) for x in f] for f in faces]
                       for faces in editor_group_faces()],
    }
    out = os.path.join(out_dir, "editor_meshes.json")
    with open(out, "w") as f:
        json.dump(editor, f, separators=(",", ":"))
    print(f"wrote {out} ({os.path.getsize(out) // 1024} KB)")


if __name__ == "__main__":
    main()
