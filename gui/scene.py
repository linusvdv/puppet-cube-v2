"""pyrender scene of the visual cube, shared by all gui modes.

PuppetScene builds one node per piece of the 26 piece visual model
(geometry.py) and provides the matura thesis lighting: the vertex colors
are recomputed per frame from the view direction (update_lighting) and
uploaded in place (flush_lighting) - see the PuppetScene docstrings.
"""

import numpy as np
import pyrender
from OpenGL.GL import GL_ARRAY_BUFFER, GL_STATIC_DRAW, glBindBuffer, glBufferData
from pyrender.constants import GLTF

from cube_model import VisualCube
from geometry import get_piece_geometry

# numpy 2.0 compatibility for pyrender
if not hasattr(np, "infty"):
    np.infty = np.inf

CAMERA_EYE = np.array([3.0, 2.4, 3.6])
CAMERA_TARGET = np.array([0.0, 0.0, 0.0])


def look_at(eye, target, up=(0.0, 1.0, 0.0)):
    """Camera to world pose looking from eye to target."""
    eye = np.asarray(eye, dtype=float)
    target = np.asarray(target, dtype=float)
    forward = target - eye
    forward /= np.linalg.norm(forward)
    right = np.cross(forward, np.asarray(up, dtype=float))
    right /= np.linalg.norm(right)
    true_up = np.cross(right, forward)
    pose = np.eye(4)
    pose[:3, 0], pose[:3, 1], pose[:3, 2], pose[:3, 3] = right, true_up, -forward, eye
    return pose


class PuppetScene:
    """pyrender scene with one node per piece of the visual cube."""

    # radial offset of the outline lines to avoid z-fighting with the surfaces
    OUTLINE_OFFSET = 0.001

    def __init__(self, cube, lighting=False):
        self.cube = cube
        self.lighting = lighting
        # (primitive, piece_idx, base color, canonical normals) of the lit pieces
        self.lit_primitives = []
        self.pending_reupload = set()
        self.scene = pyrender.Scene(ambient_light=[1.0, 1.0, 1.0],
                                    bg_color=[0.12, 0.12, 0.15, 1.0])
        materials = {}
        white_material = None
        outline_material = None
        self.nodes = []
        for piece_idx, piece in enumerate(get_piece_geometry()):
            primitives = []
            for faces, color in piece["groups"]:
                if lighting:
                    # matura lighting model: color + (abs(dot(normal, view)) - 0.5)
                    # per vertex - rendered as unlit vertex colors (see
                    # update_lighting), so the material is white
                    if white_material is None:
                        white_material = pyrender.MetallicRoughnessMaterial(
                            baseColorFactor=[1.0, 1.0, 1.0, 1.0],
                            roughnessFactor=1.0, metallicFactor=0.0, doubleSided=True)
                    primitive = pyrender.Primitive(
                        positions=piece["vertices"].astype(np.float32),
                        normals=piece["normals"].astype(np.float32),
                        indices=faces.astype(np.uint32), material=white_material)
                    primitive.color_0 = np.tile(
                        np.array([color[0], color[1], color[2], 1.0], dtype=np.float32),
                        (len(piece["vertices"]), 1))
                    self.lit_primitives.append(
                        (primitive, piece_idx, np.array(color, dtype=np.float64),
                         piece["normals"]))
                else:
                    key = tuple(color)
                    if key not in materials:
                        # the piece meshes have an inconsistent triangle winding
                        # (the matura renderer drew without back face culling)
                        materials[key] = pyrender.MetallicRoughnessMaterial(
                            baseColorFactor=[color[0], color[1], color[2], 1.0],
                            roughnessFactor=1.0, metallicFactor=0.0, doubleSided=True)
                    primitive = pyrender.Primitive(
                        positions=piece["vertices"].astype(np.float32),
                        normals=piece["normals"].astype(np.float32),
                        indices=faces.astype(np.uint32), material=materials[key])
                primitives.append(primitive)

            # black outline (the matura renderer drew the lines over the triangles)
            if len(piece["lines"]):
                if outline_material is None:
                    outline_material = pyrender.MetallicRoughnessMaterial(
                        baseColorFactor=[0.0, 0.0, 0.0, 1.0],
                        roughnessFactor=1.0, metallicFactor=0.0, doubleSided=True)
                outline = piece["lines"].reshape(-1, 3) * (1.0 + self.OUTLINE_OFFSET)
                outline_normals = np.tile(np.array([[0.0, 0.0, 1.0]], dtype=np.float32),
                                          (len(outline), 1))
                primitives.append(pyrender.Primitive(
                    positions=outline.astype(np.float32),
                    normals=outline_normals,
                    indices=np.arange(len(outline), dtype=np.uint32).reshape(-1, 2),
                    material=outline_material,
                    mode=GLTF.LINES))
            self.nodes.append(self.scene.add(pyrender.Mesh(primitives=primitives)))

        self.camera = self.scene.add(pyrender.PerspectiveCamera(yfov=np.pi / 4.0),
                                     pose=look_at(CAMERA_EYE, CAMERA_TARGET))
        if lighting:
            self.update_lighting(look_at(CAMERA_EYE, CAMERA_TARGET))
        self.update_poses(0.0)

    def update_lighting(self, camera_pose, extra_rotation=None):
        """Recomputes the matura vertex colors for the current view.

        The matura vertex shader computed Color = aColor + diffusion with
        diffusion = abs(dot(normal, view axis)) - 0.5 (winding independent,
        light glued to the camera, Gouraud interpolated). This is replicated
        with unlit vertex colors - pyrender gamma encodes the fragment
        output, so the colors are linearized (** 2.2) first.

        Deviation from the original: the old pipeline clamped over bright
        vertices per channel at the framebuffer, which makes colors that
        differ only in a saturating channel identical on faces viewed
        straight on (yellow (1,1,0) and orange (1,0.5,0) both end up as
        (1,1,0.5)). Over saturated vertices are scaled down by their max
        channel instead, which preserves the channel ratios - for vertices
        below 1.0 nothing changes.
        """
        if not self.lighting:
            return
        view = -np.asarray(camera_pose, dtype=np.float64)[:3, 2]
        for primitive, piece_idx, base_color, normals in self.lit_primitives:
            rotation = self.cube.rotations[piece_idx]
            if extra_rotation is not None:
                rotation = extra_rotation @ rotation
            posed = normals @ rotation.T
            diffusion = np.abs(posed @ view) - 0.5
            lit = base_color + diffusion[:, None]
            lit_max = lit.max(axis=1, keepdims=True)
            lit /= np.maximum(lit_max, 1.0)
            colors = np.clip(lit, 0.0, 1.0) ** 2.2
            colors = np.concatenate([colors, np.full((len(colors), 1), 1.0)], axis=1)
            colors = colors.astype(np.float32)
            if not np.array_equal(primitive.color_0, colors):
                primitive.color_0 = colors
                self.pending_reupload.add(primitive)

    def flush_lighting(self):
        """Uploads the changed vertex color buffers.

        Has to run in the thread with the current gl context (the gl widget
        paint callback, or the main thread of the offscreen renderer).
        The buffer is updated in place (pyrender only uploads new meshes,
        never changed ones) - this relies on the attribute layout of the
        primitives created above: positions, normals, color_0 interleaved.
        """
        for primitive in self.pending_reupload:
            if not primitive._buffers:  # noqa: SLF001 - not uploaded yet
                continue
            vertex_data = np.ascontiguousarray(np.hstack(
                [primitive.positions, primitive.normals, primitive.color_0]
            ).flatten().astype(np.float32))
            glBindBuffer(GL_ARRAY_BUFFER, primitive._buffers[0])  # noqa: SLF001
            glBufferData(GL_ARRAY_BUFFER, vertex_data.nbytes, vertex_data, GL_STATIC_DRAW)
        self.pending_reupload.clear()

    def update_poses(self, angle):
        """Sets the node poses for the current animation angle."""
        for piece_idx, node in enumerate(self.nodes):
            pose = np.eye(4)
            pose[:3, :3] = self.cube.pose(piece_idx, angle)
            self.scene.set_pose(node, pose)
