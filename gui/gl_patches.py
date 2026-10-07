"""pyrender 0.1.45 integration and performance patches.

pyrender is effectively unmaintained (0.1.45 since 2020) and has two
problems for this gui, patched here instead of forking the library:

- its forward pass binds framebuffer 0 for on-screen rendering, which is
  NOT the QOpenGLWidget content (qt binds the widget's own framebuffer at
  paintGL entry, see QOpenGLWidget::defaultFramebufferObject) -
  install_direct_render redirects the binding,
- it re-resolves every uniform location on every set_uniform call
  (~300 lookups per frame for this scene, ~20% of the render time) -
  install_uniform_location_cache memoizes them.

All patches are pinned to pyrender 0.1.45 (the version every venv of this
repo installs); installers report failure for other versions and the
callers fall back to slower code paths.
"""

import pyrender
from OpenGL.GL import (GL_DEPTH_TEST, GL_DRAW_FRAMEBUFFER, GL_LESS,
                       glBindFramebuffer, glDepthFunc, glDepthMask,
                       glDepthRange, glEnable, glViewport)
from pyrender.constants import RenderFlags

PATCH_VERSION = "0.1.45"


def version_matches():
    """Whether the installed pyrender matches the pinned patch version."""
    return pyrender.__version__ == PATCH_VERSION


def install_uniform_location_cache():
    """Memoizes pyrender's glGetUniformLocation lookups (module level).

    Locations are stable for a program's lifetime, so they are cached per
    (program id, name). The cache is cleared on every call: it is installed
    when a gl context is set up, and program ids can be reused by new
    programs after a context loss.
    """
    import pyrender.shader_program as shader_program
    original = shader_program.glGetUniformLocation
    cache = {}

    def cached_lookup(program, name):
        key = (program, name)
        location = cache.get(key)
        if location is None:
            location = original(program, name)
            cache[key] = location
        return location

    shader_program.glGetUniformLocation = cached_lookup


def install_direct_render(renderer, fbo_getter):
    """Patches the renderer to draw its forward pass into a foreign
    framebuffer instead of framebuffer 0.

    fbo_getter() is called before every render and returns the framebuffer
    id to draw into (the QOpenGLWidget content fbo, queried at paintGL
    entry). Returns True when installed; for other pyrender versions the
    caller has to render with RenderFlags.OFFSCREEN and blit pyrender's
    internal _main_fb over the target - that fallback allocates pyrender's
    double framebuffer set (single + 4x msaa color and depth, ~123 MiB at
    1440p) and reads the whole frame back to the cpu on every render (a
    pipeline stall growing with the window size).

    Also installs the uniform location cache (cleared here - program ids
    can repeat after a context loss, so this must run again for every new
    renderer).
    """
    if not version_matches():
        return False
    install_uniform_location_cache()

    def configure_forward_pass_viewport(flags):
        if flags & RenderFlags.OFFSCREEN:
            renderer._configure_main_framebuffer()  # noqa: SLF001 - pyrender 0.1.45
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, renderer._main_fb_ms)  # noqa: SLF001
        else:
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_getter())
        glViewport(0, 0, renderer.viewport_width, renderer.viewport_height)
        glEnable(GL_DEPTH_TEST)
        glDepthMask(True)
        glDepthFunc(GL_LESS)
        glDepthRange(0.0, 1.0)

    renderer._configure_forward_pass_viewport = configure_forward_pass_viewport  # noqa: SLF001
    return True
