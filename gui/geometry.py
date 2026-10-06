"""Geometry of the Puppet Cube V2 visual model.

Ported verbatim from the matura renderer:
    additional/matura_with_rendering/src/mesh.cpp     (piece type meshes)
    additional/matura_with_rendering/src/renderer.cpp (solved piece placement)

The visual model consists of 26 pieces (6 centers, 12 edges, 8 corners) in
6 piece types. The corner types 3/4 carry the protrusions of the bandaged
cube (generated as curved surfaces). Slice moves of the search rotate the
outer layers (M = R + L'), all 6 centers stay fixed - which is why the
search state does not need to track them.
"""

import math

import numpy as np

NUM_PIECES = 26
NUM_PIECE_TYPES = 6

# yellow, orange, green, red, blue, white, black
COLORS = np.array([
    [1.0, 1.0, 0.0],
    [1.0, 0.5, 0.0],
    [0.0, 1.0, 0.0],
    [1.0, 0.0, 0.0],
    [0.0, 0.0, 1.0],
    [1.0, 1.0, 1.0],
    [0.0, 0.0, 0.0],
])

Y, O, G, R, B, W, K = range(7)

# (piece type, sticker colors of the triangle groups) for each of the 26 pieces
PIECE_COLOR_TYPES = [
    (0, [Y]),
    (0, [O]),
    (0, [G]),
    (0, [R]),
    (0, [B]),
    (0, [W]),
    (1, [Y, O]),
    (1, [Y, G]),
    (1, [Y, R]),
    (1, [Y, B]),
    (1, [B, O]),
    (1, [G, O]),
    (1, [G, R]),
    (1, [B, R]),
    (1, [W, O]),
    (1, [W, G]),
    (1, [W, R]),
    (1, [W, B]),
    (2, [Y, O, B]),
    (3, [Y, G, O]),
    (3, [O, W, B]),
    (3, [B, R, Y]),
    (4, [Y, R, G]),
    (4, [O, G, W]),
    (4, [B, W, R]),
    (5, [W, G, R]),
]


# base meshes of the 6 piece types
# points: flat list of (x, y, z); triangles: list of index lists, one per color group
# lines: flat list of point index pairs for the black outline
_BASE_MESHES = [
    # type 0: centers
    {
        "points": [
            (0.2, 0.6, 0.2), (0.2, 0.6, -0.2), (-0.2, 0.6, -0.2), (-0.2, 0.6, 0.2),
            (0.2, 0.2, 0.2), (0.2, 0.2, -0.2), (-0.2, 0.2, -0.2), (-0.2, 0.2, 0.2)],
        "triangles": [[
            0, 1, 2, 0, 2, 3, 0, 4, 5, 0, 1, 5, 1, 5, 6, 1, 2, 6,
            2, 6, 7, 2, 3, 7, 3, 7, 4, 3, 0, 4, 4, 5, 6, 4, 6, 7]],
        "lines": [
            0, 1, 1, 2, 2, 3, 3, 0, 0, 4, 1, 5, 2, 6, 3, 7,
            4, 5, 5, 6, 6, 7, 7, 4],
    },
    # type 1: edges
    {
        "points": [
            (0.2, 0.6, 0.6), (0.2, 0.6, 0.2), (-0.2, 0.6, 0.2), (-0.2, 0.6, 0.6),
            (0.2, 0.2, 0.6), (0.2, 0.2, 0.2), (-0.2, 0.2, 0.2), (-0.2, 0.2, 0.6)],
        "triangles": [
            [0, 1, 2, 0, 2, 3, 0, 5, 1, 1, 5, 6, 1, 2, 6, 2, 6, 3],
            [0, 3, 7, 0, 7, 4, 0, 4, 5, 4, 5, 6, 4, 6, 7, 3, 6, 7]],
        "lines": [
            0, 1, 1, 2, 2, 3, 3, 0, 0, 4, 1, 5, 2, 6, 3, 7,
            4, 5, 5, 6, 6, 7, 7, 4],
    },
    # type 2: small corners
    {
        "points": [
            (0.6, 0.6, 0.6), (0.6, 0.6, 0.2), (0.2, 0.6, 0.2), (0.2, 0.6, 0.6),
            (0.6, 0.2, 0.6), (0.6, 0.2, 0.2), (0.2, 0.2, 0.2), (0.2, 0.2, 0.6)],
        "triangles": [
            [0, 1, 2, 0, 2, 3, 1, 2, 6, 2, 6, 3],
            [0, 3, 7, 0, 7, 4, 3, 6, 7, 4, 6, 7],
            [0, 4, 5, 0, 5, 1, 4, 6, 5, 1, 5, 6]],
        "lines": [
            0, 1, 1, 2, 2, 3, 3, 0, 0, 4, 1, 5, 2, 6, 3, 7,
            4, 5, 5, 6, 6, 7, 7, 4],
    },
    # type 3: corner one direction facing out
    # some of the initialisation is done in _init_one_corner
    {
        "points": [
            (-0.2, 0.6, 0.2), (-0.2, 0.6, 0.6), (-1.0, 0.6, 0.6), (-1.0, 0.6, -0.2),
            (-0.2, 0.2, 0.2), (-0.2, 0.2, 0.6), (-1.0, -0.2, 0.6), (-1.0, -0.2, -0.2),
            (-0.6, 0.2, 0.2), (-0.6, 0.6, -0.2), (-0.6, 0.6, 0.2), (-0.6, -0.2, 0.6),
            (-0.6, 0.2, 0.6)],
        "triangles": [
            [0, 1, 2, 2, 3, 9, 9, 10, 2, 10, 2, 0, 0, 1, 4, 0, 10, 8, 0, 8, 4],
            [2, 3, 6, 6, 7, 3],
            [4, 5, 1, 5, 1, 2, 5, 12, 2, 2, 12, 11, 2, 11, 6, 4, 5, 12, 4, 12, 8]],
        "lines": [
            0, 1, 1, 2, 2, 3, 3, 9, 9, 10, 10, 0, 0, 4, 4, 5, 5, 1,
            5, 12, 12, 11, 11, 6, 6, 7, 2, 6, 3, 7],
    },
    # type 4: corner two directions facing out
    # some of the initialisation is done in _init_two_corner
    {
        "points": [
            (-0.2, 0.6, -0.2), (-0.2, 0.6, -1.0), (-1.0, 0.6, -1.0), (-1.0, 0.6, -0.2),
            (-0.2, -0.2, -1.0), (-1.0, -0.2, -1.0), (-1.0, -0.2, -0.2), (-0.2, 0.2, -0.2),
            (-0.2, 0.2, -0.6), (-0.2, 0.2, -1.0), (-0.6, 0.2, -0.2), (-1.0, 0.2, -0.2)],
        "triangles": [
            [0, 1, 2, 0, 2, 3, 0, 7, 8, 0, 8, 1, 0, 7, 10, 0, 10, 3],
            [1, 4, 5, 1, 5, 2, 8, 9, 1, 4, 9, 14, 9, 14, 15, 4, 5, 14],
            [2, 5, 6, 2, 6, 3, 10, 11, 3, 6, 11, 12, 11, 12, 13, 5, 6, 12]],
        "lines": [
            0, 1, 1, 2, 2, 3, 3, 0, 1, 4, 2, 5, 3, 6, 4, 5, 5, 6,
            0, 7, 7, 13, 7, 15, 12, 13, 14, 15, 12, 6, 14, 4],
    },
    # type 5: big corners
    {
        "points": [
            (1.0, 1.0, 1.0), (1.0, 1.0, 0.2), (0.2, 1.0, 0.2), (0.2, 1.0, 1.0),
            (1.0, 0.2, 1.0), (1.0, 0.2, 0.2), (0.2, 0.2, 0.2), (0.2, 0.2, 1.0)],
        "triangles": [
            [0, 1, 2, 0, 2, 3, 1, 2, 6, 2, 6, 3],
            [0, 3, 7, 0, 7, 4, 3, 6, 7, 4, 6, 7],
            [0, 4, 5, 0, 5, 1, 4, 6, 5, 1, 5, 6]],
        "lines": [
            0, 1, 1, 2, 2, 3, 3, 0, 0, 4, 1, 5, 2, 6, 3, 7,
            4, 5, 5, 6, 6, 7, 7, 4],
    },
]


def _init_one_corner(piece_mesh):
    # curved protrusion of the one direction facing out corner
    current_triangle_size = len(piece_mesh["points"])

    num_edges_radius_lower = 8  # from area to 45 degree yellow
    num_edges_radius_middle = 8  # from area to 45 degree green
    begin_angle = math.atan(1 / 1)
    middle_angle = math.acos((1 + 2 * math.sqrt(2)) / (math.sqrt(2) * 3))  # 45 degree from corner
    end_angle = math.asin(1 / (math.sqrt(2) * 3))  # edge
    length = 0.2 * math.sqrt(2) * 3

    for i in range(num_edges_radius_lower):
        angle = begin_angle + (middle_angle - begin_angle) / num_edges_radius_lower * (i + 1)

        piece_mesh["points"].extend([
            (-length * math.cos(angle), length * math.sin(angle), -0.2),  # outer yellow
            (-length * math.cos(angle), length * math.sin(angle), 0.2),  # inner
            (-length * math.cos(angle), -0.2, length * math.sin(angle)),  # outer green
            (-length * math.cos(angle), 0.2, length * math.sin(angle))])  # inner

        last_batch = current_triangle_size + i * 4
        piece_mesh["triangles"][0].extend([  # yellow
            last_batch + 0, last_batch - 4, 3,  # outer
            last_batch + 0, last_batch + 1, last_batch - 4,  # smooth
            last_batch + 1, last_batch - 4, last_batch - 3,  # smooth
            last_batch + 1, last_batch - 3, 8])  # inner

        piece_mesh["triangles"][2].extend([  # orange
            last_batch + 2, last_batch - 2, 6,  # outer
            last_batch + 2, last_batch + 3, last_batch - 2,  # smooth
            last_batch + 3, last_batch - 2, last_batch - 1,  # smooth
            last_batch + 3, last_batch - 1, 8])  # inner

        piece_mesh["lines"].extend([
            last_batch + 0, last_batch - 4,
            last_batch + 2, last_batch - 2])

    current_triangle_size = len(piece_mesh["points"])
    for i in range(num_edges_radius_middle):
        angle = middle_angle + (end_angle - middle_angle) / num_edges_radius_middle * (i + 1)

        piece_mesh["points"].extend([
            (-length * math.cos(angle), length * math.sin(angle), -0.2),  # outer yellow
            (-length * math.cos(angle), length * math.sin(angle), 0.2),  # inner
            (-length * math.cos(angle), -0.2, length * math.sin(angle)),  # outer green
            (-length * math.cos(angle), 0.2, length * math.sin(angle))])  # inner

        last_batch = current_triangle_size + i * 4
        piece_mesh["triangles"][1].extend([  # yellow side
            last_batch + 0, last_batch - 4, 3,  # outer
            last_batch + 0, last_batch + 1, last_batch - 4,  # smooth
            last_batch + 1, last_batch - 4, last_batch - 3,  # smooth
            last_batch + 1, last_batch - 3, 8,  # inner
            # orange side
            last_batch + 2, last_batch - 2, 6,  # outer
            last_batch + 2, last_batch + 3, last_batch - 2,  # smooth
            last_batch + 3, last_batch - 2, last_batch - 1,  # smooth
            last_batch + 3, last_batch - 1, 8])  # inner

        piece_mesh["lines"].extend([
            last_batch + 0, last_batch - 4,
            last_batch + 2, last_batch - 2])

    current_triangle_size = len(piece_mesh["points"])
    piece_mesh["triangles"][1].extend([
        current_triangle_size - 4, 3, 7,
        current_triangle_size - 2, 6, 7])

    current_triangle_size = len(piece_mesh["points"])
    num_square = 8
    for i in range(num_square + 1):  # both sides inclusive
        angle_i = end_angle - (2 * end_angle) / num_square * i
        for j in range(num_square + 1):
            angle_j = end_angle - (2 * end_angle) / num_square * j
            piece_mesh["points"].append((
                -length * max(math.cos(angle_i), math.cos(angle_j)),
                length * math.sin(angle_j),
                length * math.sin(angle_i)))

            # area
            last_batch = len(piece_mesh["points"]) - 1
            if j != 0 and i != 0:
                piece_mesh["triangles"][1].extend([
                    last_batch, last_batch - 1, last_batch - num_square - 1,
                    last_batch - 1, last_batch - num_square - 1, last_batch - num_square - 2])

            # outer
            if j == num_square and i != 0:
                piece_mesh["triangles"][1].extend([
                    last_batch, last_batch - num_square - 1, 7])
                piece_mesh["lines"].extend([
                    last_batch, last_batch - num_square - 1])
            if i == num_square and j != 0:
                piece_mesh["triangles"][1].extend([
                    last_batch, last_batch - 1, 7])
                piece_mesh["lines"].extend([
                    last_batch, last_batch - 1])

            # inner
            if j == 0 and i > 1:
                piece_mesh["triangles"][1].extend([
                    last_batch, last_batch - num_square - 1, current_triangle_size])
            if i == 0 and j > 1:
                piece_mesh["triangles"][1].extend([
                    last_batch, last_batch - 1, current_triangle_size])

    piece_mesh["lines"].extend([
        len(piece_mesh["points"]) - 1, 7])


def _init_two_corner(piece_mesh):
    # curved protrusions of the two directions facing out corner
    current_triangle_size = len(piece_mesh["points"])

    num_edges_radius = 20  # multiple of two
    begin_angle = math.asin(1 / (math.sqrt(2) * 3))
    end_angle = math.asin(1 / math.sqrt(2))
    length = -0.2 * math.sqrt(2) * 3

    for i in range(num_edges_radius // 2):
        angle = begin_angle + (end_angle - begin_angle) / (num_edges_radius // 2) * i

        # left bottom, left top, right bottom, right top
        piece_mesh["points"].extend([
            (length * math.cos(angle), -0.2, length * math.sin(angle)),
            (length * math.cos(angle), 0.2, length * math.sin(angle)),
            (length * math.sin(angle), -0.2, length * math.cos(angle)),
            (length * math.sin(angle), 0.2, length * math.cos(angle))])

        if i == 0:
            continue

        last_batch = current_triangle_size + i * 4
        piece_mesh["triangles"][1].extend([
            last_batch + 2, last_batch + 3, last_batch - 1,
            last_batch + 2, last_batch - 2, last_batch - 1,
            last_batch + 3, last_batch - 1, 7,
            last_batch + 2, last_batch - 2, 5])

        piece_mesh["triangles"][2].extend([
            last_batch + 0, last_batch + 1, last_batch - 3,
            last_batch + 0, last_batch - 4, last_batch - 3,
            last_batch + 1, last_batch - 3, 7,
            last_batch + 0, last_batch - 4, 5])

        piece_mesh["lines"].extend([
            last_batch - 4, last_batch + 0,
            last_batch - 2, last_batch + 2])

    # middle
    piece_mesh["points"].extend([
        (length * math.cos(end_angle), -0.2, length * math.sin(end_angle)),
        (length * math.cos(end_angle), 0.2, length * math.sin(end_angle))])

    last_batch = current_triangle_size + num_edges_radius // 2 * 4
    piece_mesh["triangles"][1].extend([
        last_batch + 0, last_batch + 1, last_batch - 1,
        last_batch + 0, last_batch - 2, last_batch - 1,
        last_batch + 1, last_batch - 1, 7,
        last_batch + 0, last_batch - 2, 5])

    piece_mesh["triangles"][2].extend([
        last_batch + 0, last_batch + 1, last_batch - 3,
        last_batch + 0, last_batch - 4, last_batch - 3,
        last_batch + 1, last_batch - 3, 7,
        last_batch + 0, last_batch - 4, 5])

    piece_mesh["lines"].extend([
        last_batch - 4, last_batch + 0,
        last_batch - 2, last_batch + 0])


def get_piece_meshes():
    """Returns the meshes of the 6 piece types (with the protrusion initialisation)."""
    piece_meshes = [{"points": list(mesh["points"]),
                     "triangles": [list(group) for group in mesh["triangles"]],
                     "lines": list(mesh["lines"])}
                    for mesh in _BASE_MESHES]
    _init_one_corner(piece_meshes[3])
    _init_two_corner(piece_meshes[4])
    return piece_meshes


def _rotation_matrix(rotation_x, rotation_y, rotation_z):
    """Rotation matrix (radians) applied in the order x, y, z (column vectors)."""
    def rotate(axis, angle):
        cos_a, sin_a = math.cos(angle), math.sin(angle)
        if axis == 0:
            return np.array([[1.0, 0.0, 0.0], [0.0, cos_a, -sin_a], [0.0, sin_a, cos_a]])
        if axis == 1:
            return np.array([[cos_a, 0.0, sin_a], [0.0, 1.0, 0.0], [-sin_a, 0.0, cos_a]])
        return np.array([[cos_a, -sin_a, 0.0], [sin_a, cos_a, 0.0], [0.0, 0.0, 1.0]])

    return rotate(0, rotation_x) @ rotate(1, rotation_y) @ rotate(2, rotation_z)


# solved visual state of the cube: (piece type, grid position, rotation in degrees)
_SOLID_PIECES_DEGREES = [
    (0, (0, 1, 0), (0.0, 0.0, 0.0)),
    (0, (0, 0, 1), (90.0, 0.0, 0.0)),
    (0, (-1, 0, 0), (0.0, 0.0, 90.0)),
    (0, (0, 0, -1), (-90.0, 0.0, 0.0)),
    (0, (1, 0, 0), (0.0, 0.0, -90.0)),
    (0, (0, -1, 0), (180.0, 0.0, 0.0)),
    (1, (0, 1, 1), (0.0, 0.0, 0.0)),
    (1, (-1, 1, 0), (0.0, -90.0, 0.0)),
    (1, (0, 1, -1), (0.0, 180.0, 0.0)),
    (1, (1, 1, 0), (0.0, 90.0, 0.0)),
    (1, (1, 0, 1), (0.0, 0.0, -90.0)),
    (1, (-1, 0, 1), (0.0, 0.0, 90.0)),
    (1, (-1, 0, -1), (0.0, 180.0, -90.0)),
    (1, (1, 0, -1), (0.0, 180.0, 90.0)),
    (1, (0, -1, 1), (0.0, 0.0, 180.0)),
    (1, (-1, -1, 0), (0.0, -90.0, 180.0)),
    (1, (0, -1, -1), (0.0, 180.0, 180.0)),
    (1, (1, -1, 0), (0.0, 90.0, 180.0)),
    (2, (1, 1, 1), (0.0, 0.0, 0.0)),
    (3, (-1, 1, 1), (0.0, 0.0, 0.0)),
    (3, (1, -1, 1), (90.0, 90.0, 0.0)),
    (3, (1, 1, -1), (-90.0, 0.0, -90.0)),
    (4, (-1, 1, -1), (0.0, 0.0, 0.0)),
    (4, (-1, -1, 1), (0.0, 90.0, 90.0)),
    (4, (1, -1, -1), (-90.0, 0.0, -90.0)),
    (5, (-1, -1, -1), (180.0, -90.0, 0.0)),
]

# solved visual state: list of (piece type, grid position (int tuple), rotation matrix)
SOLVED_PIECES = [
    (piece_type, tuple(grid), _rotation_matrix(*np.radians(rotation)))
    for piece_type, grid, rotation in _SOLID_PIECES_DEGREES]


def _get_normal(first, second, third):
    """Area weighted face normal of a triangle (unnormalized cross product)."""
    vec1 = first - second
    vec2 = first - third
    return np.cross(vec1, vec2)


def _smallest_angle(first, second):
    """Angle between two vectors folded at 90 degrees: min(angle, pi - angle)."""
    cos_angle = np.dot(first, second) / (np.linalg.norm(first) * np.linalg.norm(second))
    angle = np.arccos(np.clip(cos_angle, -1.0, 1.0))
    return min(abs(angle), abs(np.pi - angle), abs(angle - np.pi))


def _add_vec_max_magnitude(vec1, vec2):
    """Sum or difference of two vectors, whichever has the larger magnitude."""
    added = vec1 + vec2
    subtracted = vec1 - vec2
    if np.linalg.norm(added) >= np.linalg.norm(subtracted):
        return added
    return subtracted


def _weighted_normal_average(normals):
    """Groups normals with a folded angle below pi/8 and averages each group.

    Port of the matura WeightedNormalAverage (mesh.cpp): smooths the normals
    of curved surfaces while keeping sharp box edges sharp - vertices are
    split into one vertex per normal group.
    """
    different_normals = []
    different_normal_index = []
    for normal in normals:
        for j in range(len(different_normals)):
            if _smallest_angle(different_normals[j], normal) < np.pi / 8:
                different_normals[j] = _add_vec_max_magnitude(different_normals[j], normal)
                different_normal_index.append(j)
                break
        else:
            different_normals.append(np.array(normal, dtype=np.float64))
            different_normal_index.append(len(different_normals) - 1)
    return different_normals, different_normal_index


def _split_piece_mesh(points, triangle_groups, colors):
    """Computes the split vertices with smoothed normals for one piece.

    Port of the matura TransformPieceMeshToVertexPieceData and the CubeMesh
    creation (mesh.cpp): vertices are keyed by (position, color), the face
    normals at each vertex are grouped by the angle criterion and one vertex
    per group is emitted.

    Returns (vertices (n,3), normals (n,3), triangle groups [(faces (m,3),
    color rgb)]).
    """
    # per vertex key: the triangle indices and face normals using it
    vertex_data = {}
    num_triangles = 0
    for group_idx, triangles in enumerate(triangle_groups):
        for k in range(0, len(triangles), 3):
            corner_indices = triangles[k:k + 3]
            normal = _get_normal(points[corner_indices[0]],
                                 points[corner_indices[1]],
                                 points[corner_indices[2]])
            for corner in corner_indices:
                key = (tuple(points[corner]), colors[group_idx])
                entry = vertex_data.setdefault(key, {"tri": [], "nrm": [], "first": normal.copy()})
                entry["tri"].append(num_triangles)
                entry["nrm"].append(normal)
            num_triangles += 1

    # split the vertices and assign the smoothed normals
    vertices = []
    normals = []
    # per triangle corner: the split vertex index
    triangle_corners = [[-1, -1, -1] for _ in range(num_triangles)]
    for key, entry in vertex_data.items():
        different_normals, different_normal_index = _weighted_normal_average(entry["nrm"])
        for i, normal in enumerate(different_normals):
            magnitude = np.linalg.norm(normal)
            if magnitude < 1e-12:
                # exactly opposing normals cancel out - fall back to a raw normal
                normal = entry["first"]
                magnitude = np.linalg.norm(normal)
            vertices.append(np.array(key[0], dtype=np.float64))
            normals.append(normal / magnitude)
        base_index = len(vertices) - len(different_normals)
        for i, triangle_idx in enumerate(entry["tri"]):
            corners = triangle_corners[triangle_idx]
            slot = 0 if corners[0] == -1 else (1 if corners[1] == -1 else 2)
            corners[slot] = base_index + different_normal_index[i]

    triangle_groups_out = []
    offset = 0
    for group_idx, triangles in enumerate(triangle_groups):
        count = len(triangles) // 3
        # the split vertex indices of the triangles (in processing order)
        faces = np.array(triangle_corners[offset:offset + count], dtype=np.int64)
        triangle_groups_out.append((faces, COLORS[colors[group_idx]]))
        offset += count
    return (np.array(vertices, dtype=np.float64), np.array(normals, dtype=np.float64),
            triangle_groups_out)


def get_piece_geometry():
    """Returns per piece the renderable geometry in the canonical solved pose.

    Per piece a dict with:
    - "vertices" (n,3): split vertices (one per smoothed normal group)
    - "normals" (n,3): the smoothed vertex normals
    - "groups": [(faces (m,3) int, color rgb)] triangle groups per color
    - "lines" (k,2,3): outline segments as point pairs

    The rotation matrices of SOLVED_PIECES have to be applied when placing
    the pieces (they rotate vertices and normals).
    """
    piece_meshes = get_piece_meshes()
    piece_geometry = []
    for piece_idx, (piece_type, _) in enumerate(PIECE_COLOR_TYPES):
        mesh = piece_meshes[piece_type]
        points = np.array(mesh["points"], dtype=np.float64)
        colors = PIECE_COLOR_TYPES[piece_idx][1]

        vertices, normals, groups = _split_piece_mesh(points, mesh["triangles"], colors)
        line_indices = np.array(mesh["lines"], dtype=np.int64).reshape(-1, 2)
        lines = points[line_indices]
        piece_geometry.append({"vertices": vertices, "normals": normals,
                               "groups": groups, "lines": lines})
    return piece_geometry
