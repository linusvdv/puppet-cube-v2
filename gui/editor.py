"""The cube editor: paint the stickers of a standard 3x3 to input an
arbitrary position.

The editor shows a plain 3x3 - every corner rendered as the small simple
corner (no protrusions, all 54 stickers visible) - with gray stickers and
fixed center colors. Clicking a sticker paints it with the selected color,
dragging orbits the camera.

The painted cube is validated against the piece inventory (all pieces
exist, none appears twice, corner colorings are physically possible) and
converted to solver piece placements (see pieces.py / puppetpy
convert_position).

Pure state and geometry helpers (no gl) plus the Qt editor panel.
"""

import numpy as np
from PySide6.QtCore import Qt
from PySide6.QtGui import QColor
from PySide6.QtWidgets import (QButtonGroup, QGridLayout, QHBoxLayout, QLabel,
                               QPushButton, QToolButton, QVBoxLayout, QWidget)

from geometry import COLORS, PIECE_COLOR_TYPES, SOLVED_PIECES, _split_piece_mesh, get_piece_meshes
from pieces import CORNERS, EDGES, sticker_tables

# gray sticker color of unpainted stickers
GRAY = np.array([0.42, 0.42, 0.45])

# flash color of invalid pieces
DARK_RED = np.array([0.55, 0.08, 0.08])

COLOR_NAMES = ["yellow", "orange", "green", "red", "blue", "white"]

# gui piece index of every home grid position
PIECE_OF_GRID = {tuple(int(c) for c in SOLVED_PIECES[p][1]): p for p in range(26)}


def editor_geometry():
    """Piece geometry of the editor: every corner uses the small simple
    corner mesh (type 2, like piece 18) - a standard 3x3 without
    protrusions in which all 54 stickers are visible.

    The color groups per piece are unchanged (a corner still has 3 groups),
    so the sticker structure of the real cube carries over.
    """
    piece_meshes = get_piece_meshes()
    geometry = []
    for piece_idx, (piece_type, colors) in enumerate(PIECE_COLOR_TYPES):
        mesh = piece_meshes[2 if piece_idx >= 18 else piece_type]
        points = np.array(mesh["points"], dtype=np.float64)
        vertices, normals, groups = _split_piece_mesh(points, mesh["triangles"], colors)
        line_indices = np.array(mesh["lines"], dtype=np.int64).reshape(-1, 2)
        lines = points[line_indices]
        geometry.append({"vertices": vertices, "normals": normals,
                         "groups": groups, "lines": lines})
    return geometry


def editor_placements():
    """Visual rotations of the editor scene: corners place the small corner
    mesh with the axis sign matrix (the type 2 mesh models the corner at
    (1,1,1)), everything else keeps its solved placement."""
    rotations = []
    small_corner = np.array(SOLVED_PIECES[18][2], dtype=float)
    for p in range(26):
        if p >= 18:
            grid = SOLVED_PIECES[p][1]
            rotations.append(np.diag([float(grid[0]), float(grid[1]), float(grid[2])]) @ small_corner)
        else:
            rotations.append(np.array(SOLVED_PIECES[p][2], dtype=float))
    return rotations


def editor_group_faces():
    """Face direction of every color group per piece in the editor scene
    (corners: the small corner mesh placed with the axis signs - group 0
    faces the slot's y axis, group 1 the z axis, group 2 the x axis)."""
    sticker_faces, _ = sticker_tables()
    faces = []
    for p in range(26):
        if p >= 18:
            grid = SOLVED_PIECES[p][1]
            faces.append([(0, grid[1], 0), (0, 0, grid[2]), (grid[0], 0, 0)])
        else:
            faces.append(sticker_faces[p])
    return faces


def build_triangles(geometry, rotations):
    """Posed triangles for picking: (piece, group, triangle vertices)."""
    triangles = []
    for piece in range(len(geometry)):
        verts = geometry[piece]["vertices"] @ rotations[piece].T
        for group, (faces, _) in enumerate(geometry[piece]["groups"]):
            for tri in verts[faces]:
                triangles.append((piece, group, tri))
    return triangles


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


def pick(x, y, view, triangles):
    """The (piece, group) sticker at widget position (top left origin) or
    None - a ray through the pixel against the posed triangles (the meshes
    have inconsistent winding, both orientations are accepted)."""
    renderer = view.renderer
    if renderer is None:
        return None
    dpr = view.devicePixelRatioF()
    width = renderer.viewport_width
    height = renderer.viewport_height
    px, py = x * dpr, y * dpr
    ndc_x = 2.0 * px / width - 1.0
    ndc_y = 1.0 - 2.0 * py / height
    tan = np.tan(np.pi / 8.0)  # PerspectiveCamera(yfov=pi/4)
    cam_dir = np.array([ndc_x * tan * width / height, ndc_y * tan, -1.0])
    pose = view.trackball.pose
    origin = pose[:3, 3]
    ray_dir = pose[:3, :3] @ cam_dir
    first = None
    for piece, group, tri in triangles:
        t = _ray_triangle(origin, ray_dir, tri)
        if t is not None and (first is None or t < first[0]):
            first = (t, piece, group)
    if first is None:
        return None
    return first[1], first[2]


def initial_placement_data(corners, edges):
    """Visual model setup of a custom position: (piece, grid, rotation) per
    piece. The rotation is the piece's total rotation (net rotation composed
    with its solved placement): it maps the mesh's base sticker directions
    to the current ones, so the meshes show the right colors on the right
    faces automatically (the third axis of an edge completes the frame)."""
    out = []
    for slot, piece_home, home_dirs, cur_dirs in list(corners) + list(edges):
        piece = PIECE_OF_GRID[tuple(piece_home)]
        placement = np.array(SOLVED_PIECES[piece][2], dtype=float)
        # mesh frame directions of the stickers: home = placement @ base
        base = [placement.T @ np.array(h, dtype=float) for h in home_dirs]
        cur = [np.array(c, dtype=float) for c in cur_dirs]
        if len(base) == 3:
            rotation = np.column_stack(cur) @ np.column_stack(base).T
        else:
            base3 = np.cross(base[0], base[1])
            cur3 = np.cross(cur[0], cur[1])
            rotation = np.column_stack(cur + [cur3]) @ np.column_stack(base + [base3]).T
        out.append((piece, np.array(slot, dtype=float), rotation))
    return out


class InvalidPosition(ValueError):
    """An invalid editor position. `slots` are the grid positions of the
    pieces that caused it (flashed dark red in the editor)."""

    def __init__(self, message, slots):
        super().__init__(message)
        self.slots = list(slots)


def invalid_pieces_for_error(error):
    """The pieces to flash for a solver side conversion error. The parity
    and reachability errors are properties of a whole piece class - no
    single piece is identifiable, the class flashes."""
    name = str(error)
    if name in ("PositionError.CORNER_ORIENT_PARITY", "PositionError.CORNER_UNSOLVABLE"):
        return list(CORNERS)
    if name == "PositionError.EDGE_ORIENT_PARITY":
        return list(EDGES)
    if name == "PositionError.PARITY_MISMATCH":
        return list(EDGES) + list(CORNERS)
    return []


class EditorState:
    """The painted stickers of the editor cube and their validation."""

    def __init__(self):
        self.colors = {}  # (piece, group) -> color index (missing = gray)
        self.revision = 0  # bumped on every change (preview scene staleness)
        # paint actions for the undo/redo: lists of (piece, group, color);
        # a new paint action clears the redo path (single history)
        self._undo_stack = []
        self._redo_stack = []
        sticker_faces, sticker_colors = sticker_tables()
        self.sticker_colors = sticker_colors
        # the real home face directions of every piece's stickers (the
        # editor mesh group order differs for corners - the placements have
        # to use the real piece sticker order)
        self.real_faces = sticker_faces
        # piece lookup by the color set
        self.pieces_by_colors = {}
        for p in list(EDGES) + list(CORNERS):
            self.pieces_by_colors.setdefault(frozenset(sticker_colors[p]), []).append(p)
        self.group_faces = editor_group_faces()
        # the stickers of every slot: (piece, group, face dir) of the editor
        # piece that sits at the slot (the editor cube is in the solved
        # arrangement, so the home piece provides the slot's stickers)
        self.slot_stickers = {}
        for p in list(EDGES) + list(CORNERS):
            slot = tuple(int(c) for c in SOLVED_PIECES[p][1])
            self.slot_stickers[slot] = [(p, g, face)
                                        for g, face in enumerate(self.group_faces[p])]

    def set_color(self, piece, group, color):
        """Paints a sticker (color None = erase to gray). Centers are fixed."""
        if piece < 6:
            return
        old = self.colors.get((piece, group))
        if old == color:
            return  # no change, no undo entry
        self._undo_stack.append([(piece, group, old)])
        self._redo_stack.clear()
        self.revision += 1
        if color is None:
            self.colors.pop((piece, group), None)
        else:
            self.colors[(piece, group)] = color

    def clear(self):
        if not self.colors:
            return
        self._undo_stack.append([(p, g, c) for (p, g), c in self.colors.items()])
        self._redo_stack.clear()
        self.revision += 1
        self.colors.clear()

    def undo(self):
        """Reverts the last paint action (a single sticker or a clear).

        Returns the changed (piece, group) keys - to recolor the scene -
        or None when there is nothing to undo.
        """
        if not self._undo_stack:
            return None
        changes = self._undo_stack.pop()
        self._redo_stack.append(self._apply(changes))
        return [(piece, group) for piece, group, _color in changes]

    def redo(self):
        """Re-applies the last undone action (a new paint clears the redo
        path). Returns the changed (piece, group) keys or None."""
        if not self._redo_stack:
            return None
        changes = self._redo_stack.pop()
        self._undo_stack.append(self._apply(changes))
        return [(piece, group) for piece, group, _color in changes]

    def _apply(self, changes):
        """Applies (piece, group, color) entries, returns their inverse."""
        inverse = []
        for piece, group, color in changes:
            inverse.append((piece, group, self.colors.get((piece, group))))
            if color is None:
                self.colors.pop((piece, group), None)
            else:
                self.colors[(piece, group)] = color
        self.revision += 1
        return inverse

    def remaining_per_color(self):
        """How many stickers of every color can still be placed manually:
        every color appears 9 times on the cube, the fixed center sticker
        is the 9th, so 8 are paintable (negative = too many placed)."""
        used = [0] * 6
        for (_piece, _group), color in self.colors.items():
            if 0 <= color < 6:
                used[color] += 1
        return [8 - used[color] for color in range(6)]

    def sticker_color(self, piece, group):
        if piece < 6:
            return self.sticker_colors[piece][group]
        return self.colors.get((piece, group))

    def placements(self):
        """Extracts the piece placements of the painted cube for
        puppetpy.convert_position.

        Raises InvalidPosition (a ValueError) with a user readable message
        when the cube is incomplete or violates the piece inventory (unknown
        color sets, duplicated pieces, mirrored corner colorings); the
        `slots` attribute lists the grid positions of all offending pieces.
        """
        corners = []
        edges = []
        used = {}  # target piece -> the slot it is placed at
        problems = []  # (message, [offending slots])
        missing_total = 0
        missing_slots = set()
        for slot, stickers in sorted(self.slot_stickers.items()):
            face_colors = {}
            missing = 0
            for piece, group, face in stickers:
                color = self.sticker_color(piece, group)
                if color is None:
                    missing += 1
                else:
                    face_colors[face] = color
            if missing:
                missing_total += missing
                missing_slots.add(slot)
                continue
            candidates = self.pieces_by_colors.get(frozenset(face_colors.values()))
            # the slot is an edge/corner slot: the piece has to be of the same kind
            target = None
            if candidates:
                for candidate in candidates:
                    if (candidate >= 18) == (slot in self.corner_slots):
                        target = candidate
                        break
            if target is None:
                problems.append((f"the colors at slot {slot} do not form a piece of this cube",
                                 [slot]))
                continue
            if target in used:
                problems.append((f"the piece at slot {slot} appears twice on the cube",
                                 [slot, used[target]]))
                continue
            used[target] = slot
            # the current direction of every sticker of the target piece: the
            # sticker with color c sits on the face where c was painted (the
            # home directions follow the real piece sticker order)
            home_dirs = []
            cur_dirs = []
            for group, color in enumerate(self.sticker_colors[target]):
                home_dirs.append(self.real_faces[target][group])
                cur_dir = next(face for face, painted in face_colors.items() if painted == color)
                cur_dirs.append(cur_dir)
            if target >= 18:
                # mirrored corner colorings are impossible
                det = np.linalg.det(np.array(cur_dirs, dtype=float).T) * \
                    np.linalg.det(np.array(home_dirs, dtype=float).T)
                if det < 0:
                    problems.append((f"the corner at slot {slot} is colored in an impossible "
                                     f"(mirrored) way", [slot]))
                    continue
                corners.append((list(slot), list(SOLVED_PIECES[target][1]),
                                [list(h) for h in home_dirs], [list(c) for c in cur_dirs]))
            else:
                edges.append((list(slot), list(SOLVED_PIECES[target][1]),
                              [list(h) for h in home_dirs], [list(c) for c in cur_dirs]))
        if missing_total:
            raise InvalidPosition(
                f"{missing_total} sticker{'s' if missing_total > 1 else ''} still gray",
                missing_slots)
        if problems:
            raise InvalidPosition(problems[0][0],
                                  [slot for _message, slots in problems for slot in slots])
        if len(used) != 20:
            raise InvalidPosition("not all pieces are placed", [])
        return corners, edges

    @property
    def corner_slots(self):
        if not hasattr(self, "_corner_slots"):
            self._corner_slots = {tuple(int(c) for c in SOLVED_PIECES[p][1]) for p in CORNERS}
        return self._corner_slots

    def summary(self):
        """Validation state for the status line."""
        try:
            self.placements()
        except ValueError as error:
            return str(error)
        return "the position is complete - ready to solve"


class EditorPanel(QWidget):
    """Right side panel of the editor: color selection (with the remaining
    sticker count of every color on the button), undo, clear, the view
    toggle, validation status and the solve button."""

    # text color on the color buttons (yellow, orange, green, red, blue, white)
    BUTTON_TEXT_COLORS = ["#202020", "#202020", "#202020", "white", "white", "#202020"]

    def __init__(self, parent=None):
        super().__init__(parent)
        self.selected = None  # selected color index (None = eraser)

        layout = QVBoxLayout(self)
        layout.addWidget(QLabel("colors"))

        grid = QGridLayout()
        self.group = QButtonGroup(self)
        self.group.setExclusive(True)
        self.color_buttons = []
        for idx, name in enumerate(COLOR_NAMES):
            button = QToolButton()
            button.setToolTip(name)
            button.setCheckable(True)
            button.setAutoRaise(True)
            button.setFixedSize(34, 34)
            color = QColor(int(COLORS[idx][0] * 255), int(COLORS[idx][1] * 255),
                           int(COLORS[idx][2] * 255))
            button.setStyleSheet(f"QToolButton {{ background-color: {color.name()}; "
                                 f"color: {self.BUTTON_TEXT_COLORS[idx]}; "
                                 f"border: 1px solid #555555; border-radius: 4px; "
                                 f"font-weight: bold; }}"
                                 f"QToolButton:checked {{ background-color: {color.name()}; "
                                 f"color: {self.BUTTON_TEXT_COLORS[idx]}; "
                                 f"border: 2px solid #e8832d; border-radius: 4px; }}")
            self.group.addButton(button, idx)
            grid.addWidget(button, idx // 3, idx % 3)
            self.color_buttons.append(button)
        self.eraser = QToolButton()
        self.eraser.setToolTip("eraser (gray)")
        self.eraser.setCheckable(True)
        self.eraser.setAutoRaise(True)
        self.eraser.setFixedSize(34, 34)
        gray = QColor(int(GRAY[0] * 255), int(GRAY[1] * 255), int(GRAY[2] * 255))
        self.eraser.setStyleSheet(f"QToolButton:checked {{ background-color: {gray.name()}; "
                                  f"border: 2px solid #e8832d; border-radius: 4px; }}"
                                  f"QToolButton {{ background-color: {gray.name()}; "
                                  f"border: 1px solid #555555; border-radius: 4px; }}")
        self.group.addButton(self.eraser, 6)
        grid.addWidget(self.eraser, 2, 0)
        layout.addLayout(grid)

        undo_redo = QHBoxLayout()
        self.undo_button = QPushButton("undo")
        self.undo_button.setToolTip("revert the last paint action")
        self.undo_button.setFocusPolicy(Qt.FocusPolicy.NoFocus)
        undo_redo.addWidget(self.undo_button)
        self.redo_button = QPushButton("redo")
        self.redo_button.setToolTip("re-apply the last undone paint action")
        self.redo_button.setFocusPolicy(Qt.FocusPolicy.NoFocus)
        undo_redo.addWidget(self.redo_button)
        layout.addLayout(undo_redo)

        self.clear_button = QPushButton("clear")
        self.clear_button.setFocusPolicy(Qt.FocusPolicy.NoFocus)
        layout.addWidget(self.clear_button)

        # toggles between the paintable standard 3x3 and the painted position
        # rendered with the real puppet cube piece shapes (the label always
        # names the view a click switches to)
        self.puppet_view = QPushButton("puppet cube")
        self.puppet_view.setToolTip("show the painted position with the real piece shapes")
        self.puppet_view.setCheckable(True)
        self.puppet_view.setFocusPolicy(Qt.FocusPolicy.NoFocus)
        layout.addWidget(self.puppet_view)

        layout.addStretch(1)

        self.status = QLabel("paint the 54 stickers")
        self.status.setWordWrap(True)
        layout.addWidget(self.status)

        self.solve_button = QPushButton("solve")
        self.solve_button.setFocusPolicy(Qt.FocusPolicy.NoFocus)
        self.solve_button.setDefault(True)
        layout.addWidget(self.solve_button)

    def selected_color(self):
        """The selected color index (None = eraser)."""
        return None if (self.group.checkedId() == 6 or self.group.checkedId() == -1) \
            else self.group.checkedId()

    def update_counts(self, counts):
        """Writes the remaining sticker count of every color onto its
        button (8 per color are paintable, the center is the 9th)."""
        for button, count in zip(self.color_buttons, counts):
            button.setText(str(count))

    def set_puppet_view_active(self, active):
        """Label of the view toggle: it names the view it switches to."""
        self.puppet_view.setText("standard cube" if active else "puppet cube")

    def set_status(self, text):
        self.status.setText(text)
