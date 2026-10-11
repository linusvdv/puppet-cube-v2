"""Piece level view of the cube: sticker tables and the extraction of piece
placements for the solver conversion (puppetpy.convert_position).

The sticker tables are derived from the geometry: every color group of a
piece (in PIECE_COLOR_TYPES group order) has a well defined home face
direction. In the base mesh the group order per piece type corresponds to a
fixed face order (verified by ray casting, see verify_sticker_faces); the
solved placement rotation of the piece maps it to the world face direction.

placements_from_visual_cube(cube) extracts the placements of a VisualCube
state in the solver frame: the slot grid positions (bookkeeping grids) and
the sticker directions with the visual rotations corrected by the inverse
frame offset (visual = offset @ physical).
"""

import numpy as np

from geometry import PIECE_COLOR_TYPES, SOLVED_PIECES, get_piece_geometry

NUM_PIECES = 26
CENTERS = range(0, 6)
EDGES = range(6, 18)
CORNERS = range(18, 26)

# face direction of every color group in the base mesh (before the solved
# placement rotation), per piece type; the group order is the one of
# PIECE_COLOR_TYPES
BASE_GROUP_FACES = {
    0: [( 0, 1, 0)],
    1: [( 0, 1, 0), ( 0, 0, 1)],
    2: [( 0, 1, 0), ( 0, 0, 1), ( 1, 0, 0)],
    3: [( 0, 1, 0), (-1, 0, 0), ( 0, 0, 1)],
    4: [( 0, 1, 0), ( 0, 0,-1), (-1, 0, 0)],
    5: [( 0, 1, 0), ( 0, 0, 1), ( 1, 0, 0)],
}

_tables = None


def sticker_tables():
    """(sticker_faces, sticker_colors): per piece the home face direction and
    the color index of every color group, in group order"""
    global _tables
    if _tables is None:
        sticker_faces = []
        for piece, (piece_type, _) in enumerate(PIECE_COLOR_TYPES):
            rotation = np.array(SOLVED_PIECES[piece][2], dtype=int)
            faces = [tuple(int(x) for x in rotation @ np.array(base))
                     for base in BASE_GROUP_FACES[piece_type]]
            assert len(faces) == len(PIECE_COLOR_TYPES[piece][1])
            sticker_faces.append(faces)
        sticker_colors = [list(colors) for _, colors in PIECE_COLOR_TYPES]
        _tables = (sticker_faces, sticker_colors)
    return _tables


def placements_from_visual_cube(cube):
    """piece placements of a VisualCube state in the solver frame, as expected
    by puppetpy.convert_position(corners, edges): per corner slot and edge
    slot (slot, piece home grid, sticker home face dirs, sticker current face
    dirs)"""
    sticker_faces, _ = sticker_tables()
    offset_inv = np.round(np.array(cube.rotation_offset, dtype=float).T).astype(int)
    piece_at = {tuple(int(c) for c in cube.grids[p]): p for p in range(NUM_PIECES)}
    corners = []
    edges = []
    for p in list(EDGES) + list(CORNERS):
        slot = tuple(int(c) for c in SOLVED_PIECES[p][1])
        piece = piece_at[slot]
        home_placement = np.array(SOLVED_PIECES[piece][2], dtype=int)
        rotation = offset_inv @ np.round(np.array(cube.rotations[piece], dtype=float)).astype(int)
        home_dirs = sticker_faces[piece]
        cur_dirs = []
        for home_dir in home_dirs:
            base_dir = home_placement.T @ np.array(home_dir)
            cur_dirs.append(tuple(int(x) for x in rotation @ base_dir))
        placement = (list(slot), list(SOLVED_PIECES[piece][1]),
                     [list(h) for h in home_dirs], [list(c) for c in cur_dirs])
        (corners if p >= 18 else edges).append(placement)
    return corners, edges


def relativize_dirs(dirs, to_slot):
    """map face directions to the same axes with the signs of another slot"""
    out = []
    for d in dirs:
        axis = next(i for i, x in enumerate(d) if x != 0)
        nd = [0, 0, 0]
        nd[axis] = to_slot[axis]
        out.append(nd)
    return out


def swap_placements(placements, i, j):
    """swap the pieces of two placements between their slots, keeping the
    sticker assignments relative to the slots (slot relativized swap)"""
    a, b = placements[i], placements[j]
    placements[i] = (a[0], b[1], b[2], relativize_dirs(b[3], a[0]))
    placements[j] = (b[0], a[1], a[2], relativize_dirs(a[3], b[0]))


def _ray_triangle(origin, ray_dir, tri):
    e1 = tri[1] - tri[0]
    e2 = tri[2] - tri[0]
    pvec = np.cross(ray_dir, e2)
    det = float(e1 @ pvec)
    if abs(det) < 1e-12:
        return None
    inv = 1.0 / det
    tvec = origin - tri[0]
    u = float(tvec @ pvec) * inv
    if u < -1e-9 or u > 1.0 + 1e-9:
        return None
    qvec = np.cross(tvec, e1)
    v = float(ray_dir @ qvec) * inv
    if v < -1e-9 or u + v > 1.0 + 1e-9:
        return None
    t = float(e2 @ qvec) * inv
    if t <= 1e-9:
        return None
    return t


def verify_sticker_faces():
    """validates the sticker face table by ray casting: a ray from far
    outside onto every 3x3 sticker position of a face must hit a sticker
    whose derived home face direction is the ray direction"""
    sticker_faces, _ = sticker_tables()
    geometry = get_piece_geometry()
    triangles = [] # (piece, group, tri)
    for piece in range(NUM_PIECES):
        rotation = np.array(SOLVED_PIECES[piece][2], dtype=float)
        verts = geometry[piece]["vertices"] @ rotation.T
        for g, (faces, _) in enumerate(geometry[piece]["groups"]):
            for tri in verts[faces]:
                triangles.append((piece, g, tri))
    pitch = 0.4
    for axis in range(3):
        for sign in (1, -1):
            face_dir = np.array([0.0, 0.0, 0.0])
            face_dir[axis] = float(sign)
            others = [i for i in range(3) if i != axis]
            for a in (-pitch, 0.0, pitch):
                for b in (-pitch, 0.0, pitch):
                    origin = np.array([0.0, 0.0, 0.0])
                    origin[axis] = sign * 10.0
                    origin[others[0]] = a
                    origin[others[1]] = b
                    first = None
                    for piece, g, tri in triangles:
                        t = _ray_triangle(origin, -face_dir, tri)
                        if t is not None and (first is None or t < first[0]):
                            first = (t, piece, g)
                    assert first is not None, f"no hit from direction {face_dir} at {origin}"
                    _, piece, g = first
                    assert sticker_faces[piece][g] == tuple(int(x) for x in face_dir), \
                        f"sticker face mismatch: piece {piece} group {g} is " \
                        f"{sticker_faces[piece][g]} but was hit from {tuple(int(x) for x in face_dir)}"
