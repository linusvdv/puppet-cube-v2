"""Headless render regression of the cube editor (EGL, no display, no
solver).

Renders the editor scene (the standard 3x3 with gray stickers) in both
lighting modes and checks that painting a sticker changes exactly the
pixels of that sticker's screen region - the whole recolor path including
the matura lighting recompute (base color mutation + signature
invalidation) and the in place vertex buffer upload.

Same process pixel comparisons only (the EGL config selection varies
across processes - do not compare saved PNGs).

Run:  python tests/editor_render_test.py
"""

import os
import sys

os.environ.setdefault("PYOPENGL_PLATFORM", "egl")

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "gui"))

import numpy as np
import pyrender

import editor
from cube_model import VisualCube
from scene import CAMERA_EYE, CAMERA_TARGET, PuppetScene, look_at

WIDTH, HEIGHT = 640, 480


def render(scene, renderer):
    pose = look_at(CAMERA_EYE, CAMERA_TARGET)
    scene.scene.set_pose(scene.camera, pose)
    scene.update_poses(0.0)
    if scene.lighting:
        scene.update_lighting(pose)
    scene.flush_lighting()
    color, _depth = renderer.render(scene.scene)
    return color


def changed_mask(before, after):
    return np.any(before != after, axis=2)


def main():
    geometry = editor.editor_geometry()
    placements = editor.editor_placements()

    for lighting in (True, False):
        cube = VisualCube()
        for p, rotation in enumerate(placements):
            cube.rotations[p] = rotation
        scene = PuppetScene(cube, lighting=lighting, piece_geometry=geometry)
        triangles = editor.build_triangles(geometry, placements)
        renderer = pyrender.OffscreenRenderer(WIDTH, HEIGHT)

        # gray out all edge and corner stickers (like the editor does)
        scene.recolor_groups({(p, g): editor.GRAY for p in range(6, 26)
                              for g in range(len(scene.groups[p]))})

        before = render(scene, renderer)

        # a gray editor: only the 6 center stickers are colored (with the
        # matura lighting the base colors are the source of truth, without
        # it color_0)
        def group_color(p, g):
            verts = scene.groups[p][g][0].reshape(-1)
            if scene.lighting:
                return scene.base_colors[p][verts[0]]
            return scene.primitives[p].color_0[verts[0], :3]

        colored = sum(1 for p in range(26) for g in range(len(scene.groups[p]))
                      if not np.allclose(group_color(p, g), editor.GRAY))
        assert colored == 6, f"lighting {lighting}: expected 6 colored center stickers, got {colored}"

        # paint one corner sticker red: exactly its pixels may change
        piece, group = 18, 0  # the top sticker of the (1,1,1) corner
        red = np.array([1.0, 0.0, 0.0])
        scene.recolor_groups({(piece, group): red})
        after = render(scene, renderer)
        mask = changed_mask(before, after)
        assert mask.sum() > 200, f"lighting {lighting}: repaint not visible"
        assert mask.sum() < 20000, \
            f"lighting {lighting}: repaint changed too many pixels ({mask.sum()})"

        # the changed pixels lie within the projected sticker: the picking
        # ray through the mask centroid hits the painted sticker
        ys, xs = np.nonzero(mask)
        center_px = (float(xs.mean()), float(ys.mean()))

        class StubRenderer:
            viewport_width = WIDTH
            viewport_height = HEIGHT

        class StubView:
            renderer = StubRenderer()
            devicePixelRatioF = lambda self: 1.0

        stub = StubView()
        stub.trackball = type("T", (), {"pose": look_at(CAMERA_EYE, CAMERA_TARGET)})()
        hit = editor.pick(*center_px, stub, triangles)
        assert hit == (piece, group), \
            f"lighting {lighting}: changed pixels centered on {hit}, " \
            f"expected {(piece, group)}"

        renderer.delete()
        print(f"lighting {lighting}: sticker repaint visible "
              f"({int(mask.sum())} pixels, on the painted sticker)")

    print("editor_render_test PASSED")


if __name__ == "__main__":
    main()
