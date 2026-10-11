"""Tests of the cube editor logic (no Qt window, no gl, no solver).

1. Editor geometry: the group face directions are the face directions of
   the pieces' home slots.
2. Extraction: painting the editor cube with the sticker colors of a
   replayed cube yields the same piece placements as
   pieces.placements_from_visual_cube (order independent pair comparison).
3. Error cases: gray stickers, unknown color sets, duplicated pieces and
   mirrored corner colorings are rejected.
4. Initial placement data: applying it to a fresh visual cube reproduces
   the replayed bookkeeping grids and the physical rotations.
5. Picking: the sticker under a projected point is recovered (stub view,
   no gl).

Usage: python3 editor_test.py
"""

import math
import os
import random
import sys

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "gui"))

import editor
from cube_model import VisualCube, derive_moves
from geometry import COLORS, SOLVED_PIECES
from pieces import placements_from_visual_cube, sticker_tables
from scene import look_at, CAMERA_EYE, CAMERA_TARGET

NUM_MOVES = 18


def paint_from_cube(state, cube):
    """Paints the editor like the given visual cube state: the color of
    every editor sticker (piece at home, group) is the color of the sticker
    of the piece that sits at its slot in the given cube."""
    corners, edges = placements_from_visual_cube(cube)
    face_color = {}  # (slot, face dir) -> color of the sticker there
    for slot, _piece, home_dirs, cur_dirs in list(corners) + list(edges):
        piece = editor.PIECE_OF_GRID[tuple(_piece)]
        _, sticker_colors = sticker_tables()
        for home_dir, cur_dir, color in zip(home_dirs, cur_dirs, sticker_colors[piece]):
            face_color[(tuple(slot), tuple(cur_dir))] = color
    for piece, faces in enumerate(editor.editor_group_faces()):
        slot = tuple(int(c) for c in SOLVED_PIECES[piece][1])
        for group, face in enumerate(faces):
            state.set_color(piece, group, face_color.get((slot, tuple(face))))


def as_pair_set(placements):
    """The placements as a set of (slot, piece, home->cur pairs) - the
    sticker order within a placement is irrelevant."""
    out = set()
    for slot, piece, home_dirs, cur_dirs in placements:
        out.add((tuple(slot), tuple(piece),
                 frozenset((tuple(h), tuple(c)) for h, c in zip(home_dirs, cur_dirs))))
    return out


class StubRenderer:
    def __init__(self, width, height):
        self.viewport_width = width
        self.viewport_height = height


class StubView:
    def __init__(self, width, height, pose):
        self.renderer = StubRenderer(width, height)
        self.trackball = type("Trackball", (), {"pose": pose})()
        self.devicePixelRatioF = lambda: 1.0


def check_editor_geometry():
    print("== editor geometry ==")
    faces = editor.editor_group_faces()
    for p in range(26):
        grid = SOLVED_PIECES[p][1]
        assert len(faces[p]) == sum(1 for c in grid if c != 0), \
            f"piece {p}: wrong sticker count"
        used = set()
        for face in faces[p]:
            axis = next(i for i, x in enumerate(face) if x != 0)
            assert abs(face[axis]) == 1 and face[axis] == grid[axis], \
                f"piece {p}: face {face} is not a face of slot {grid}"
            assert axis not in used, f"piece {p}: axis used twice"
            used.add(axis)
    # the placements of the editor scene are valid rotations
    for p, rotation in enumerate(editor.editor_placements()):
        assert np.allclose(rotation @ rotation.T, np.eye(3), atol=1e-9), \
            f"piece {p}: editor placement not orthogonal"
    print("group faces and placements ok")


def check_extraction():
    print("== extraction ==")
    moves = None
    random.seed(1234)
    for trial in range(5):
        cube = VisualCube()
        if moves is None:
            # move objects from the matura reference table (no solver
            # module needed), all 18 rotations
            from playback_test import make_move
            moves = [make_move(i) for i in range(NUM_MOVES)]
        seq = [random.choice(moves) for _ in range(30 * (trial + 1))]
        for move in seq:
            cube.apply_move(move)

        state = editor.EditorState()
        paint_from_cube(state, cube)
        corners, edges = state.placements()
        expected_c, expected_e = placements_from_visual_cube(cube)
        assert as_pair_set(corners) == as_pair_set(expected_c), f"trial {trial}: corners differ"
        assert as_pair_set(edges) == as_pair_set(expected_e), f"trial {trial}: edges differ"
        print(f"trial {trial}: {len(seq)} moves - placements match")

        # the initial placement reproduces the bookkeeping state
        data = editor.initial_placement_data(corners, edges)
        fresh = VisualCube()
        for piece, grid, rotation in data:
            fresh.grids[piece] = [int(c) for c in grid]
            fresh.rotations[piece] = rotation
        for p in range(6, 26):
            assert [int(c) for c in fresh.grids[p]] == [int(c) for c in cube.grids[p]], \
                f"trial {trial}: piece {p} grid mismatch"
            expected = np.round(cube.rotation_offset.T @ cube.rotations[p], 6)
            assert np.allclose(fresh.rotations[p], expected, atol=1e-6), \
                f"trial {trial}: piece {p} rotation mismatch"
        # the centers are not tracked by the search (they can spin around
        # their own axis) - only their grids have to match
        for p in range(6):
            assert [int(c) for c in fresh.grids[p]] == [int(c) for c in cube.grids[p]], \
                f"trial {trial}: center {p} grid mismatch"
        print(f"trial {trial}: initial placement matches the replay")


def check_errors():
    print("== error cases ==")
    state = editor.EditorState()
    try:
        state.placements()
        raise AssertionError("empty editor accepted")
    except editor.InvalidPosition as error:
        assert "gray" in str(error), error
        assert len(error.slots) == 20, f"all slots have gray stickers: {error.slots}"
    print("gray stickers rejected (all slots flagged)")

    # paint everything solved, then corrupt one slot
    solved = editor.EditorState()
    paint_from_cube(solved, VisualCube())
    corners, edges = solved.placements()

    # unknown color set: replace a sticker color on an edge slot
    bad = editor.EditorState()
    paint_from_cube(bad, VisualCube())
    bad.set_color(6, 0, (bad.sticker_color(6, 0) + 1) % 6)
    try:
        bad.placements()
        raise AssertionError("wrong colors accepted")
    except editor.InvalidPosition as error:
        assert "do not form a piece" in str(error), error
        slot6 = tuple(int(c) for c in editor.SOLVED_PIECES[6][1])
        assert error.slots == [slot6], error.slots
    print("unknown color set rejected (slot flagged)")

    # duplicated piece: paint two slots with the same edge piece
    dup = editor.EditorState()
    paint_from_cube(dup, VisualCube())
    # slot of piece 7 gets the colors of the piece of slot 6
    for group in range(2):
        dup.set_color(7, group, dup.sticker_color(6, group))
    try:
        dup.placements()
        raise AssertionError("duplicate accepted")
    except editor.InvalidPosition as error:
        assert "appears twice" in str(error), error
        slot7 = tuple(int(c) for c in editor.SOLVED_PIECES[7][1])
        assert sorted(map(tuple, error.slots)) == sorted([slot6, slot7]), error.slots
    print("duplicated piece rejected (both slots flagged)")

    # mirrored corner: swap two sticker colors of a corner slot
    mirror = editor.EditorState()
    paint_from_cube(mirror, VisualCube())
    c0 = mirror.sticker_color(18, 0)
    c1 = mirror.sticker_color(18, 1)
    mirror.set_color(18, 0, c1)
    mirror.set_color(18, 1, c0)
    try:
        mirror.placements()
        raise AssertionError("mirrored corner accepted")
    except editor.InvalidPosition as error:
        assert "mirrored" in str(error), error
        slot18 = tuple(int(c) for c in editor.SOLVED_PIECES[18][1])
        assert error.slots == [slot18], error.slots
    print("mirrored corner rejected (slot flagged)")


def check_picking():
    print("== picking ==")
    geometry = editor.editor_geometry()
    placements = editor.editor_placements()
    triangles = editor.build_triangles(geometry, placements)
    width, height = 640, 480
    pose = look_at(CAMERA_EYE, CAMERA_TARGET)
    view = StubView(width, height, pose)
    tan = math.tan(np.pi / 8.0)
    aspect = width / height
    eye = pose[:3, 3]
    verified = 0
    for piece in range(6, 26):  # edges and corners have one group per sticker
        verts = geometry[piece]["vertices"] @ placements[piece].T
        for group, face in enumerate(editor.editor_group_faces()[piece]):
            group_verts = verts[geometry[piece]["groups"][group][0].reshape(-1)]
            face = np.array(face, dtype=float)
            # the sticker center: the group's vertex mean pushed onto the
            # outer face plane along the face direction (the groups of the
            # matura meshes include side triangles)
            center = group_verts.mean(axis=0)
            center = center + face * ((group_verts @ face).max() - center @ face)
            cam = pose[:3, :3].T @ (center - eye)
            if cam[2] >= 0:
                continue  # behind the camera
            ndc_x = cam[0] / (tan * aspect * -cam[2])
            ndc_y = cam[1] / (tan * -cam[2])
            px = (ndc_x + 1.0) / 2.0 * width
            py = (1.0 - ndc_y) / 2.0 * height
            hit = editor.pick(px, py, view, triangles)
            # the pick has to hit something, and the front most stickers
            # (unoccluded, face towards the camera) have to be picked
            # exactly at their face center projection - this verifies that
            # the projection and the pick ray are inverse to each other
            if hit is None:
                raise AssertionError(f"piece {piece} group {group}: no hit at ({px:.0f}, {py:.0f})")
            if hit == (piece, group):
                verified += 1
    # roughly half of the 48 edge/corner stickers are visible from the
    # camera and unoccluded - the front most ones have to be picked exactly
    assert verified >= 20, f"only {verified} stickers picked exactly"
    print(f"{verified} front stickers picked exactly at their projected face centers")


def check_undo_and_counts():
    print("== undo and sticker counts ==")
    import random
    state = editor.EditorState()
    assert state.remaining_per_color() == [8] * 6
    assert state.undo() is None, "empty editor has nothing to undo"

    from playback_test import make_move
    moves = [make_move(i) for i in range(NUM_MOVES)]
    random.seed(99)
    cube = VisualCube()
    for move in [random.choice(moves) for _ in range(25)]:
        cube.apply_move(move)
    paint_from_cube(state, cube)
    # a complete cube paints exactly 8 stickers of every color
    assert state.remaining_per_color() == [0] * 6, state.remaining_per_color()
    corners, edges = state.placements()

    # clear is one undo step and restores everything
    state.clear()
    assert state.remaining_per_color() == [8] * 6
    keys = state.undo()
    assert keys is not None and len(keys) == 48
    assert state.remaining_per_color() == [0] * 6
    corners2, edges2 = state.placements()
    assert as_pair_set(corners2) == as_pair_set(corners)
    assert as_pair_set(edges2) == as_pair_set(edges)

    # a single sticker paint is one undo step (including erasing)
    old = state.sticker_color(6, 0)
    state.set_color(6, 0, (old + 1) % 6)
    assert state.remaining_per_color()[old] == 1
    assert state.remaining_per_color()[(old + 1) % 6] == -1, "overuse shows negative"
    assert state.undo() == [(6, 0)]
    assert state.sticker_color(6, 0) == old
    assert state.remaining_per_color() == [0] * 6

    # redo re-applies the undone paint
    assert state.redo() == [(6, 0)]
    assert state.sticker_color(6, 0) == (old + 1) % 6
    assert state.remaining_per_color()[old] == 1
    assert state.redo() is None, "one redo step only"
    # undo / redo round trip
    assert state.undo() == [(6, 0)]
    assert state.redo() == [(6, 0)]
    assert state.undo() == [(6, 0)]

    # a manual paint after an undo clears the redo path (single history)
    prev1 = state.sticker_color(6, 1)
    state.set_color(6, 1, old)
    assert state.redo() is None, "a new paint deletes the redo path"
    assert state.sticker_color(6, 0) == old, "the manual paint does not jump"
    state.undo()  # revert the manual paint
    assert state.sticker_color(6, 1) == prev1

    state.set_color(6, 0, None)  # erase
    assert state.remaining_per_color()[old] == 1
    state.undo()
    assert state.sticker_color(6, 0) == old

    # painting the same color again is a no-op (no undo entry)
    stack_len = len(state._undo_stack)
    state.set_color(6, 0, old)
    assert len(state._undo_stack) == stack_len
    print("undo, redo and sticker counts ok")


def main():
    check_editor_geometry()
    check_extraction()
    check_errors()
    check_undo_and_counts()
    check_picking()
    print("editor_test PASSED")


if __name__ == "__main__":
    main()
