"""Headless benchmark and regression of the render paths (EGL, no display,
no solver).

Compares the previous Qt render path (RenderFlags.OFFSCREEN + a blit:
pyrender allocates its own double framebuffer set - single + 4x msaa
color and depth renderbuffers, ~123 MiB at 1440p - and reads the whole
frame back to the cpu on every render) against the direct render into a
"widget" framebuffer (gl_patches.install_direct_render, what the Qt view
uses).

Checks that both paths produce the same image (bit exact when rendering
into an equally multisampled framebuffer) and reports frame times and
video memory.

Run:  python tests/gl_bench.py [widthxheight ...]   (default 3 sizes)
"""

import os
import sys
import time

os.environ.setdefault("PYOPENGL_PLATFORM", "egl")  # this benchmark only -
# the qt process must NOT set it (qt's context is glx based)

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "gui"))

import numpy as np
import pyrender
from OpenGL.GL import (GL_COLOR_ATTACHMENT0, GL_COLOR_BUFFER_BIT,
                       GL_DEPTH_ATTACHMENT, GL_DEPTH_COMPONENT24,
                       GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER, GL_NEAREST,
                       GL_READ_FRAMEBUFFER, GL_RGBA, GL_RENDERBUFFER,
                       GL_UNSIGNED_BYTE, glBindFramebuffer, glBindRenderbuffer,
                       glBlitFramebuffer, glFinish, glFramebufferRenderbuffer,
                       glGenFramebuffers, glGenRenderbuffers, glGetError,
                       glReadPixels, glRenderbufferStorage,
                       glRenderbufferStorageMultisample)
from pyrender.constants import RenderFlags
from pyrender.platforms.egl import EGLPlatform

import gl_patches
from cube_model import VisualCube
from scene import CAMERA_TARGET, PuppetScene, look_at

# GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX (nvidia extension)
GL_AVAILABLE_VIDMEM = 0x9049
FRAMES = 100


def vram_free():
    """Available video memory in MiB via the nvidia extension, if present."""
    try:
        from OpenGL.GL import GL_EXTENSIONS, glGetIntegerv, glGetString
        if "GL_NVX_gpu_memory_info" not in glGetString(GL_EXTENSIONS).decode():
            return None
        value = int(glGetIntegerv(GL_AVAILABLE_VIDMEM))
        if not 0 < value < 100000:  # sanity: the gpu has 12282 MiB
            return None
        return value
    except Exception:  # noqa: BLE001 - extension optional
        return None


class WidgetFramebuffer:
    """Color + depth renderbuffers, like the QOpenGLWidget content fbo."""

    def __init__(self, width, height, samples=0):
        self.width, self.height, self.samples = width, height, samples
        self.color, self.depth = glGenRenderbuffers(2)
        for buffer, internal in ((self.color, GL_RGBA),
                                 (self.depth, GL_DEPTH_COMPONENT24)):
            glBindRenderbuffer(GL_RENDERBUFFER, buffer)
            if samples:
                glRenderbufferStorageMultisample(
                    GL_RENDERBUFFER, samples, internal, width, height)
            else:
                glRenderbufferStorage(GL_RENDERBUFFER, internal, width, height)
        self.fbo = glGenFramebuffers(1)
        glBindFramebuffer(GL_FRAMEBUFFER, self.fbo)
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                  GL_RENDERBUFFER, self.color)
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_RENDERBUFFER, self.depth)

    def read(self):
        """Resolved image as (h, w, 4) uint8 (bottom left origin)."""
        if not self.samples:
            glBindFramebuffer(GL_READ_FRAMEBUFFER, self.fbo)
            data = glReadPixels(0, 0, self.width, self.height,
                                GL_RGBA, GL_UNSIGNED_BYTE)
            return np.frombuffer(data, dtype=np.uint8).reshape(
                self.height, self.width, 4).copy()
        resolved = WidgetFramebuffer(self.width, self.height, samples=0)
        glBindFramebuffer(GL_READ_FRAMEBUFFER, self.fbo)
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolved.fbo)
        glBlitFramebuffer(0, 0, self.width, self.height,
                          0, 0, self.width, self.height,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST)
        image = resolved.read()
        resolved.delete()
        return image

    def delete(self):
        from OpenGL.GL import glDeleteFramebuffers, glDeleteRenderbuffers
        glDeleteFramebuffers(1, [self.fbo])
        glDeleteRenderbuffers(2, [self.color, self.depth])


def orbit_pose(angle):
    eye = np.array([3.0 * np.cos(angle), 2.4, 3.6 * np.sin(angle)])
    return look_at(eye, CAMERA_TARGET)


def timed_frames(scene, draw, frames=FRAMES):
    """Runs draw() per frame (camera orbiting, lighting recomputed like the
    real app) and returns (ms per frame, part timings)."""
    start = time.perf_counter()
    lighting_time = flush_time = draw_time = 0.0
    for i in range(frames):
        camera = orbit_pose(i * 0.01)
        scene.scene.set_pose(scene.camera, camera)
        t = time.perf_counter()
        scene.update_lighting(camera)
        lighting_time += time.perf_counter() - t
        t = time.perf_counter()
        scene.flush_lighting()
        flush_time += time.perf_counter() - t
        t = time.perf_counter()
        draw()
        draw_time += time.perf_counter() - t
    glFinish()
    elapsed = (time.perf_counter() - start) / frames * 1000.0
    parts = (f"[lighting {lighting_time / frames * 1000.0:5.2f} "
             f"flush {flush_time / frames * 1000.0:5.2f} "
             f"draw {draw_time / frames * 1000.0:5.2f}]")
    return elapsed, parts


def run_resolution(width, height):
    print(f"\n=== {width}x{height} ===")
    platform = EGLPlatform(width, height)
    platform.init_context()
    platform.make_current()

    scene = PuppetScene(VisualCube(), lighting=True)
    renderer = pyrender.Renderer(width, height)
    widget = WidgetFramebuffer(width, height)

    # --- path A: the previous qt path (offscreen render + blit) ----------
    def draw_offscreen_blit():
        renderer.render(scene.scene, RenderFlags.OFFSCREEN)
        glBindFramebuffer(GL_READ_FRAMEBUFFER, renderer._main_fb)  # noqa: SLF001 - pyrender
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, widget.fbo)
        glBlitFramebuffer(0, 0, width, height, 0, 0, width, height,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST)
        glBindFramebuffer(GL_FRAMEBUFFER, widget.fbo)

    ms_a, parts_a = timed_frames(scene, draw_offscreen_blit)

    # video memory of pyrender's internal framebuffers (allocated by the
    # offscreen path): free them and watch the available memory recover
    free_with_fbos = vram_free()
    renderer._delete_main_framebuffer()  # noqa: SLF001 - pyrender
    free_without = vram_free()
    if free_with_fbos is not None and free_without is not None:
        vram_info = f", internal framebuffers {free_without - free_with_fbos} MiB vram"
    else:
        # analytic: single + 4x msaa, rgba + depth24 renderbuffers
        vram_info = f", internal framebuffers ~{35 * width * height >> 20} MiB vram"
    print(f"path A (offscreen+blit): {ms_a:6.2f} ms/frame {parts_a}{vram_info}")

    # --- path B: direct render into the widget framebuffer ---------------
    assert gl_patches.install_direct_render(renderer, lambda: widget.fbo), \
        f"pyrender {pyrender.__version__} != {gl_patches.PATCH_VERSION}"

    def draw_direct():
        renderer.render(scene.scene, RenderFlags.NONE)

    ms_b, parts_b = timed_frames(scene, draw_direct)
    free_b = vram_free()
    if free_b is not None and free_without is not None:
        print(f"path B (direct):        {ms_b:6.2f} ms/frame {parts_b}, "
              f"extra vram {free_without - free_b} MiB")
    else:
        print(f"path B (direct):        {ms_b:6.2f} ms/frame {parts_b}")

    # --- regression: both paths must produce the same image ---------------
    # path A result: render offscreen and take pyrender's returned image
    camera = orbit_pose(0.7)
    scene.scene.set_pose(scene.camera, camera)
    scene.update_lighting(camera)
    scene.flush_lighting()
    image_a, _ = renderer.render(scene.scene, RenderFlags.OFFSCREEN)
    renderer._delete_main_framebuffer()  # noqa: SLF001 - pyrender

    # path B into an equally multisampled framebuffer + resolve, then read
    widget_ms = WidgetFramebuffer(width, height, samples=4)
    gl_patches.install_direct_render(renderer, lambda: widget_ms.fbo)
    renderer.render(scene.scene, RenderFlags.NONE)
    image_b = widget_ms.read()
    widget_ms.delete()

    # pyrender's returned image is vertically flipped (image convention)
    image_a = np.flipud(image_a)
    image_b = image_b[:, :, :3]
    assert image_a.shape == image_b.shape, (image_a.shape, image_b.shape)
    differing = int(np.count_nonzero((image_a != image_b).any(axis=-1)))
    assert differing == 0, \
        f"direct render differs from offscreen render ({differing} pixels)"
    print(f"regression: direct and offscreen renders are identical "
          f"({image_a.shape[1]}x{image_a.shape[0]} pixels)")

    # lighting cost per frame (runs in both paths)
    start = time.perf_counter()
    for i in range(FRAMES):
        scene.update_lighting(orbit_pose(i * 0.01))
    print(f"update_lighting alone:  "
          f"{(time.perf_counter() - start) / FRAMES * 1000.0:6.2f} ms/frame")

    while glGetError():
        pass
    renderer.delete()
    widget.delete()


def main():
    sizes = [(int(a), int(b)) for a, b in
             (arg.split("x") for arg in sys.argv[1:])] or \
            [(1280, 720), (1920, 1080), (2560, 1440)]
    print(f"pyrender {pyrender.__version__}, frames per measurement: {FRAMES}")
    for width, height in sizes:
        run_resolution(width, height)
    print("\nBENCHMARK DONE")


if __name__ == "__main__":
    main()
