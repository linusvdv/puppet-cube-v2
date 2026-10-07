"""Qt application of the Puppet Cube V2 gui.

Interactive solve inspection window: the 3d view (pyrender rendering the
matura visual model, see scene.py / geometry.py) with a run list on the
left and a move timeline with transport controls at the bottom. The
solver runs in a background thread through the puppetpy module and
reports every scramble/solution via Qt signals (queued to the main
thread).

Transport: play/pause, single step forwards/backwards (quick animation,
the backward step visibly turns the layer back), jump to the scrambled
position / the solved position, and clicking the timeline jumps to the
clicked boundary. New runs are followed automatically (replayed as they
arrive, like the old pyglet viewer) until the user interacts manually -
the follow checkbox re-enables it.

Keyboard: space play/pause, left/right single step, Home/End jump to the
scrambled/solved position.

Closing the window exits the program hard (the search can not be aborted
mid run). Run with: python3 gui/main.py
"""

import os
import sys
import threading
import time
import traceback

import numpy as np
from OpenGL.GL import (GL_COLOR_BUFFER_BIT, GL_DRAW_FRAMEBUFFER,
                       GL_DRAW_FRAMEBUFFER_BINDING, GL_FRAMEBUFFER,
                       GL_NEAREST, GL_NO_ERROR, GL_READ_FRAMEBUFFER,
                       glBlitFramebuffer, glBindFramebuffer, glGetError,
                       glGetIntegerv)

from PySide6.QtCore import QObject, Qt, QTimer, Signal
from PySide6.QtGui import QKeySequence, QShortcut, QSurfaceFormat
from PySide6.QtOpenGLWidgets import QOpenGLWidget
from PySide6.QtWidgets import (QApplication, QCheckBox, QHBoxLayout,
                               QListWidget, QListWidgetItem, QMainWindow,
                               QPushButton, QScrollArea, QSplitter, QStyle,
                               QVBoxLayout, QWidget)

import pyrender
from pyrender.constants import RenderFlags
from pyrender.trackball import Trackball

import gl_patches
from cube_model import VisualCube
from main import run_solver
from playback import PlaybackController
from scene import CAMERA_TARGET, CAMERA_EYE, PuppetScene, look_at
from timeline import ROW_SOLUTION, MoveTimeline

# gui log levels (like the c++ logger: higher setting = more output);
# --gui_log_level extra adds the deep gl diagnostics
LOG_LEVELS = {"error": 1, "warning": 2, "info": 3, "extra": 4}
GUI_LOG_LEVEL = LOG_LEVELS["info"]


def log(level, message):
    if LOG_LEVELS[level] <= GUI_LOG_LEVEL:
        print(f"[{time.strftime('%a %b %d %H:%M:%S %Y')}] GUI-{level}: {message}",
              flush=True)


def clear_gl_errors():
    """Discards stale gl errors.

    Qt's own gl usage (context setup, backing store compositing) can leave
    errors in the queue; PyOpenGL checks the error state on every call and
    would blame the first of OUR calls for them.
    """
    for _ in range(8):
        if glGetError() == GL_NO_ERROR:
            return


class CubeView(QOpenGLWidget):
    """The 3d view: a pyrender Renderer drawing into the widget framebuffer.

    The Renderer does not own a gl context (same as the old pyglet viewer):
    it is created in initializeGL with the qt context current. Unlike the
    pyglet window, framebuffer 0 is NOT the widget content - QOpenGLWidget
    renders into a dedicated fbo that qt binds at paintGL entry (see
    QOpenGLWidget::defaultFramebufferObject). gl_patches.install_direct_render
    redirects pyrender's forward pass to draw directly into that fbo (no
    intermediate buffers, no readback); the fallback path for unexpected
    pyrender versions renders offscreen and blits (_render_offscreen_blit).
    """

    def __init__(self, gui_scene, parent=None):
        super().__init__(parent)
        self.gui_scene = gui_scene
        self.renderer = None
        self.paint_failures = 0
        self.direct_render = False
        self._widget_fbo = 0
        # orbit camera (mouse), wheel zooms
        self.trackball = Trackball(look_at(CAMERA_EYE, CAMERA_TARGET),
                                   (640, 480), 5.2, CAMERA_TARGET)

    def initializeGL(self):
        dpr = self.devicePixelRatioF()
        self.renderer = pyrender.Renderer(int(self.width() * dpr),
                                          int(self.height() * dpr))
        # reparenting / screen changes destroy the context - free the gl
        # objects while the old context is still current
        self.context().aboutToBeDestroyed.connect(self._context_lost)
        self.direct_render = gl_patches.install_direct_render(
            self.renderer, lambda: self._widget_fbo)
        if not self.direct_render:
            log("warning", f"pyrender {pyrender.__version__} != "
                f"{gl_patches.PATCH_VERSION} - using the slow offscreen+blit "
                "render path")
        else:
            log("info", "rendering directly into the widget framebuffer")
        self._log_gl_state()

    def _log_gl_state(self):
        """Logs what qt actually gave us (context version/profile/type and
        the driver strings) - the gl 3.3 core request can silently fall
        back, e.g. to a gles context that does not know desktop gl enums."""
        fmt = self.context().format()
        profile = {QSurfaceFormat.OpenGLContextProfile.CoreProfile: "core",
                   QSurfaceFormat.OpenGLContextProfile.CompatibilityProfile: "compat",
                   QSurfaceFormat.OpenGLContextProfile.NoProfile: "none"}.get(fmt.profile(), "?")
        rtype = {QSurfaceFormat.RenderableType.OpenGL: "desktop-gl",
                 QSurfaceFormat.RenderableType.OpenGLES: "gles"}.get(
                     fmt.renderableType(), str(fmt.renderableType()))
        log("info", f"gl context {fmt.majorVersion()}.{fmt.minorVersion()} "
            f"{profile} {rtype}, samples {fmt.samples()}")
        try:
            from OpenGL.GL import GL_RENDERER, GL_VENDOR, GL_VERSION, glGetString
            clear_gl_errors()
            log("info", f"gl driver {glGetString(GL_VERSION)} | "
                f"{glGetString(GL_VENDOR)} | {glGetString(GL_RENDERER)}")
        except Exception as error:  # noqa: BLE001 - diagnostics only
            log("warning", f"gl driver query failed: {error}")

    def _context_lost(self):
        log("warning", "gl context lost - recreating the renderer")
        self.makeCurrent()
        if self.renderer is not None:
            self.renderer.delete()
            self.renderer = None
        self.doneCurrent()

    def schedule_repaint(self):
        """update() guarded for platforms where no gl context exists
        (offscreen testing) - without this Qt retries the context creation
        on every repaint."""
        if self.renderer is not None:
            self.update()

    def resizeGL(self, w, h):
        dpr = self.devicePixelRatioF()
        if self.renderer is not None:
            self.renderer.viewport_width = int(w * dpr)
            self.renderer.viewport_height = int(h * dpr)
        self.trackball.resize((w, h))

    def paintGL(self):
        if self.renderer is None:
            return
        try:
            self._render_frame()
        except Exception:  # noqa: BLE001 - logged, the frame is dropped
            # Qt would re-call paintGL every frame - log the first failures
            # in full, then only every 100th, instead of spamming
            self.paint_failures += 1
            if self.paint_failures <= 3 or self.paint_failures % 100 == 0:
                log("error", f"paintGL failed (occurrence {self.paint_failures}):\n"
                    f"{traceback.format_exc()}")

    def _render_frame(self):
        clear_gl_errors()
        # the fbo qt bound for paintGL is the widget content (nonzero for
        # QOpenGLWidget) - captured for the patched viewport setup
        self._widget_fbo = int(glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING))
        pose = self.trackball.pose
        self.gui_scene.scene.set_pose(self.gui_scene.camera, pose)
        # the matura lighting follows the camera - the vertex colors are
        # recomputed and uploaded in the gl thread (this one) before drawing
        if self.gui_scene.lighting:
            self.gui_scene.update_lighting(pose)
            self.gui_scene.flush_lighting()
        if self.direct_render:
            # draws straight into the widget framebuffer
            self.renderer.render(self.gui_scene.scene, RenderFlags.NONE)
        else:
            self._render_offscreen_blit()
        if GUI_LOG_LEVEL >= LOG_LEVELS["extra"]:
            log("extra", f"widget fbo {self._widget_fbo}, direct "
                f"{self.direct_render}, viewport {self.renderer.viewport_width}"
                f"x{self.renderer.viewport_height}")
            errors = []
            for _ in range(8):
                code = glGetError()
                if code == GL_NO_ERROR:
                    break
                errors.append(hex(code))
            if errors:
                log("extra", f"gl errors left after render: {' '.join(errors)}")

    def _render_offscreen_blit(self):
        """Fallback for unexpected pyrender versions: render into pyrender's
        internal framebuffer and blit it over the widget framebuffer."""
        self.renderer.render(self.gui_scene.scene, RenderFlags.OFFSCREEN)
        width = self.renderer.viewport_width
        height = self.renderer.viewport_height
        glBindFramebuffer(GL_READ_FRAMEBUFFER, self.renderer._main_fb)  # noqa: SLF001 - pyrender
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, self._widget_fbo)
        glBlitFramebuffer(0, 0, width, height, 0, 0, width, height,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST)
        glBindFramebuffer(GL_FRAMEBUFFER, self._widget_fbo)

    def mousePressEvent(self, event):
        # the trackball works with a bottom left origin
        pos = np.array([event.position().x(), self.height() - event.position().y()])
        if event.button() == Qt.MouseButton.LeftButton:
            self.trackball.set_state(Trackball.STATE_ROTATE)
        elif event.button() == Qt.MouseButton.MiddleButton:
            self.trackball.set_state(Trackball.STATE_PAN)
        elif event.button() == Qt.MouseButton.RightButton:
            self.trackball.set_state(Trackball.STATE_ZOOM)
        self.trackball.down(pos)
        self.update()

    def mouseMoveEvent(self, event):
        if event.buttons():
            pos = np.array([event.position().x(), self.height() - event.position().y()])
            self.trackball.drag(pos)
            self.update()

    def wheelEvent(self, event):
        self.trackball.scroll(event.angleDelta().y() / 120.0)
        self.update()


class SolverBridge(QObject):
    """Marshals the solver callbacks (solver thread) into the main thread."""

    scramble_signal = Signal(int, list)
    solution_signal = Signal(int, list, int)
    failed_signal = Signal(str)


class RunData:
    """One solver run: scramble and (once found) solution move lists."""

    def __init__(self, run_idx, scramble):
        self.run_idx = run_idx
        self.scramble = scramble
        self.solution = None
        self.depth = 0


class MainWindow(QMainWindow):
    """Run list + 3d view + move timeline with transport controls."""

    def __init__(self, args, moves, puppetpy):
        super().__init__()
        self.args = args
        self.moves = moves
        self.cube = VisualCube()
        self.gui_scene = PuppetScene(self.cube, lighting=args.lighting)
        self.controller = PlaybackController(self.cube, args.scramble_duration,
                                             args.solution_duration, args.pause,
                                             args.step_duration)
        self.runs = {}
        self._loading_run = False  # programmatic selection guard
        self._shown_boundary = None
        self._shown_state = None
        # when the current replay finished (follow mode) - run_pause countdown
        self._pending_since = None
        self._last_tick = time.perf_counter()

        self.setWindowTitle("Puppet Cube V2")
        self.resize(args.width, args.height)
        self._build_ui()
        self.statusBar().showMessage("loading precomputation tables...")

        self.bridge = SolverBridge()
        self.bridge.scramble_signal.connect(self.on_scramble)
        self.bridge.solution_signal.connect(self.on_solution)
        self.bridge.failed_signal.connect(self.on_solver_failed)

        self.timer = QTimer(self)
        self.timer.timeout.connect(self._tick)
        self.timer.start(15)

        self.solver = threading.Thread(target=self._solver_thread,
                                       args=(puppetpy,), daemon=True)
        self.solver.start()

    # --- ui construction --------------------------------------------------

    def _build_ui(self):
        self.runs_list = QListWidget()
        self.runs_list.setMinimumWidth(180)
        self.runs_list.currentRowChanged.connect(self.on_run_selected)

        self.view = CubeView(self.gui_scene)

        self.timeline = MoveTimeline()
        self.timeline.boundaryRequested.connect(self.on_timeline_click)
        self.timeline_scroll = QScrollArea()
        self.timeline_scroll.setWidget(self.timeline)
        self.timeline_scroll.setWidgetResizable(False)
        self.timeline_scroll.setFixedHeight(self.timeline.minimumSizeHint().height()
                                            + self.timeline_scroll.horizontalScrollBar().sizeHint().height())

        # transport bar
        self.follow_box = QCheckBox("follow search")
        self.follow_box.setChecked(True)
        self.follow_box.toggled.connect(self.on_follow_toggled)

        def transport_button(icon, tooltip, handler):
            button = QPushButton()
            button.setIcon(self.style().standardIcon(icon))
            button.setToolTip(tooltip)
            # keep the keyboard shortcuts (space/arrows) away from the buttons
            button.setFocusPolicy(Qt.FocusPolicy.NoFocus)
            button.clicked.connect(handler)
            return button

        self.skip_back_button = transport_button(
            QStyle.StandardPixmap.SP_MediaSkipBackward, "scrambled position",
            self.on_jump_scrambled)
        self.step_back_button = transport_button(
            QStyle.StandardPixmap.SP_MediaSeekBackward, "previous move (left)",
            self.on_step_backward)
        self.play_button = transport_button(
            QStyle.StandardPixmap.SP_MediaPlay, "play/pause (space)", self.on_toggle_play)
        self.step_forward_button = transport_button(
            QStyle.StandardPixmap.SP_MediaSeekForward, "next move (right)",
            self.on_step_forward)
        self.skip_forward_button = transport_button(
            QStyle.StandardPixmap.SP_MediaSkipForward, "solved position",
            self.on_jump_solved)

        transport = QHBoxLayout()
        transport.addWidget(self.follow_box)
        transport.addStretch(1)
        for button in (self.skip_back_button, self.step_back_button, self.play_button,
                       self.step_forward_button, self.skip_forward_button):
            transport.addWidget(button)
        transport.addStretch(1)

        bottom = QWidget()
        bottom_layout = QVBoxLayout(bottom)
        bottom_layout.addLayout(transport)
        bottom_layout.addWidget(self.timeline_scroll)

        right_split = QSplitter(Qt.Orientation.Vertical)
        right_split.addWidget(self.view)
        right_split.addWidget(bottom)
        right_split.setStretchFactor(0, 1)
        right_split.setStretchFactor(1, 0)

        split = QSplitter(Qt.Orientation.Horizontal)
        split.addWidget(self.runs_list)
        split.addWidget(right_split)
        split.setStretchFactor(0, 0)
        split.setStretchFactor(1, 1)

        self.setCentralWidget(split)

        for key, handler in ((Qt.Key.Key_Space, self.on_toggle_play),
                             (Qt.Key.Key_Left, self.on_step_backward),
                             (Qt.Key.Key_Right, self.on_step_forward),
                             (Qt.Key.Key_Home, self.on_jump_scrambled),
                             (Qt.Key.Key_End, self.on_jump_solved)):
            shortcut = QShortcut(QKeySequence(key), self)
            shortcut.activated.connect(handler)

    # --- solver events (main thread via queued signals) --------------------

    def _solver_thread(self, puppetpy):
        try:
            run_solver(puppetpy, self._solver_callback, self.args.solver_args)
        except Exception as error:  # noqa: BLE001 - reported via the status bar
            self.bridge.failed_signal.emit(f"{type(error).__name__}: {error}")

    def _solver_callback(self, event):
        kind = str(event.kind)
        if kind == "SearchEventKind.SCRAMBLE":
            self.bridge.scramble_signal.emit(
                event.run_idx, [self.moves[m] for m in event.moves])
        elif kind == "SearchEventKind.SOLUTION":
            self.bridge.solution_signal.emit(
                event.run_idx, [self.moves[m] for m in event.moves], len(event.moves))

    def on_scramble(self, run_idx, scramble):
        run = RunData(run_idx, scramble)
        self.runs[run_idx] = run
        item = QListWidgetItem(f"run {run_idx} - searching...")
        item.setData(Qt.ItemDataRole.UserRole, run_idx)
        self.runs_list.insertItem(run_idx, item)
        log("info", f"run {run_idx}: {len(scramble)} scramble moves")
        if self.follow_box.isChecked():
            busy = (self.controller.is_playing or self.controller.is_animating
                    or self.controller.state == "stepping")
            if not busy:
                self._load_run(run_idx, autoplay=True)
            # while a replay is running the run stays selectable in the
            # list - the timer switches to the successor of the current
            # run when the replay finished

    def on_solution(self, run_idx, solution, depth):
        run = self.runs[run_idx]
        run.solution = solution
        run.depth = depth
        self.runs_list.item(run_idx).setText(f"run {run_idx} - depth {depth}")
        log("info", f"run {run_idx}: solution depth {depth}")
        if self.controller.run_id == run_idx:
            self.controller.set_solution(solution)
            self.timeline.set_moves([m.name for m in run.scramble],
                                    [m.name for m in solution])

    def on_solver_failed(self, message):
        self.statusBar().showMessage(f"search failed: {message}")
        log("error", f"search failed: {message}")

    # --- run selection and navigation ----------------------------------------

    def _load_run(self, run_idx, autoplay):
        run = self.runs[run_idx]
        self._pending_since = None
        self._loading_run = True
        self.runs_list.setCurrentRow(run_idx)
        self._loading_run = False
        self.controller.set_run(run_idx, run.scramble, run.solution)
        self.timeline.set_moves([m.name for m in run.scramble],
                                [m.name for m in run.solution]
                                if run.solution is not None else None)
        self._shown_boundary = None
        self._shown_state = None
        if autoplay:
            self.controller.play()
        else:
            self.controller.jump_scrambled()
            # like the jump button: playhead at the start of the solution
            self.timeline.set_boundary(self.controller.boundary, ROW_SOLUTION)
            self._autoscroll_timeline()
            self._update_status()
            self.gui_scene.update_poses(0.0)
            self.view.schedule_repaint()

    def _next_follow_run(self):
        """The run the follow mode advances to: the successor of the
        currently loaded run (NOT the latest - computed runs in between are
        replayed one after another), or the first run if none is loaded."""
        if self.controller.run_id is None:
            return min(self.runs) if self.runs else None
        successor = self.controller.run_id + 1
        return successor if successor in self.runs else None

    def on_run_selected(self, row):
        if self._loading_run or row < 0:
            return
        run_idx = self.runs_list.item(row).data(Qt.ItemDataRole.UserRole)
        if run_idx == self.controller.run_id:
            return
        # manual selection stops the automatic follow
        self.follow_box.setChecked(False)
        self._load_run(run_idx, autoplay=False)

    def on_follow_toggled(self, checked):
        if not checked:
            self._pending_since = None
        # when (re-)enabled, the timer picks the successor of the current
        # run up as soon as the replay finished (run_pause)

    def on_timeline_click(self, boundary, row):
        self.follow_box.setChecked(False)
        self.controller.pause()
        self.controller.set_boundary(boundary)
        # the clicked line decides where the ambiguous scrambled boundary
        # shows its playhead (end of the scramble / start of the solution);
        # the keep rule in set_boundary preserves it for the tick sync
        self.timeline.set_boundary(boundary, row)
        self._autoscroll_timeline()
        self._update_status()
        self.gui_scene.update_poses(self.controller.angle)
        self.view.schedule_repaint()

    def on_toggle_play(self):
        self.follow_box.setChecked(False)
        if self.controller.is_playing or self.controller.state == "stepping":
            self.controller.pause()
        else:
            self.controller.play()

    def on_step_forward(self):
        self.follow_box.setChecked(False)
        self.controller.step_forward()

    def on_step_backward(self):
        self.follow_box.setChecked(False)
        self.controller.step_backward()

    def on_jump_scrambled(self):
        self.follow_box.setChecked(False)
        self.controller.pause()
        self.controller.jump_scrambled()
        # the scrambled position lies on both lines - show the playhead at
        # the start of the solution (where the solving begins); the keep
        # rule in set_boundary preserves it for the tick sync
        self.timeline.set_boundary(self.controller.boundary, ROW_SOLUTION)
        self._autoscroll_timeline()
        self._update_status()
        self.gui_scene.update_poses(0.0)
        self.view.schedule_repaint()

    def on_jump_solved(self):
        self.follow_box.setChecked(False)
        self.controller.pause()
        self.controller.jump_solved()
        self.timeline.set_boundary(self.controller.boundary)
        self._autoscroll_timeline()
        self._update_status()
        self.gui_scene.update_poses(0.0)
        self.view.schedule_repaint()

    # --- animation -------------------------------------------------------------

    def _tick(self):
        now = time.perf_counter()
        dt = now - self._last_tick
        self._last_tick = now
        if self.controller.tick(dt):
            self.gui_scene.update_poses(self.controller.angle)
            self.view.schedule_repaint()

        # follow: switch to the successor of the current run once its replay
        # finished - a short pause shows the solved cube first, like the old
        # pyglet viewer (computed runs in between are NOT skipped)
        if (self.follow_box.isChecked() and self.controller.state == "idle"
                and not self.controller.is_animating):
            successor = self._next_follow_run()
            if successor is not None:
                if self._pending_since is None:
                    self._pending_since = now
                elif now - self._pending_since >= self.args.run_pause:
                    log("info", f"following run {successor}")
                    self._load_run(successor, autoplay=True)
            else:
                self._pending_since = None
        else:
            self._pending_since = None

        boundary = self.controller.boundary
        if boundary != self._shown_boundary:
            self._shown_boundary = boundary
            self.timeline.set_boundary(boundary)
            self._autoscroll_timeline()
            self._update_status()
        state = self.controller.state
        if state != self._shown_state:
            self._shown_state = state
            self._update_play_button()

    def _autoscroll_timeline(self):
        x = int(self.timeline.playhead_x())
        bar = self.timeline_scroll.horizontalScrollBar()
        if x < bar.value() or x > bar.value() + self.timeline_scroll.viewport().width() - 40:
            bar.setValue(x - self.timeline_scroll.viewport().width() // 2)

    def _update_play_button(self):
        playing = self.controller.is_playing or self.controller.state == "stepping"
        icon = self.style().standardIcon(
            QStyle.StandardPixmap.SP_MediaPause if playing else QStyle.StandardPixmap.SP_MediaPlay)
        self.play_button.setIcon(icon)

    def _update_status(self):
        run = self.runs.get(self.controller.run_id)
        if run is None:
            self.statusBar().showMessage("loading precomputation tables...")
            return
        total = self.controller.num_moves
        depth = f", depth {run.depth}" if run.solution is not None else ", searching..."
        self.statusBar().showMessage(
            f"run {run.run_idx}: move {self.controller.boundary}/{total}{depth}")

    def closeEvent(self, event):
        # the search can not be aborted mid run - exit without unwinding the
        # C++ threads (the solver thread is a daemon)
        self.hide()
        sys.stdout.flush()
        os._exit(0)


def run_qt_gui(args, moves, puppetpy):
    global GUI_LOG_LEVEL
    GUI_LOG_LEVEL = LOG_LEVELS[args.gui_log_level]

    # PyOpenGL talks GLX on linux - make qt use the glx integration on x11
    # as well (qt can use egl there, which can fall back to a gles context
    # that does not know desktop gl enums like GL_DRAW_FRAMEBUFFER)
    os.environ.setdefault("QT_XCB_GL_INTEGRATION", "xcb_glx")

    # gl 3.3 core with a depth buffer, msaa for the widget framebuffer -
    # pyrender renders straight into it and applies the output gamma in the
    # shader, so no srgb framebuffer is needed
    fmt = QSurfaceFormat()
    fmt.setVersion(3, 3)
    fmt.setProfile(QSurfaceFormat.CoreProfile)
    fmt.setDepthBufferSize(24)
    fmt.setSamples(4)
    QSurfaceFormat.setDefaultFormat(fmt)
    QApplication.setAttribute(Qt.ApplicationAttribute.AA_ShareOpenGLContexts, True)

    import OpenGL
    import PySide6
    app = QApplication(sys.argv)
    log("info", f"qt {PySide6.__version__} (platform {app.platformName()}), "
        f"PyOpenGL {OpenGL.__version__}, pyrender {pyrender.__version__}, "
        f"pybind11 module {puppetpy.__file__}")
    window = MainWindow(args, moves, puppetpy)
    window.show()
    app.exec()
