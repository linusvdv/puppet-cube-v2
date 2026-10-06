"""Graphical user interface for the Puppet Cube V2 solver.

Replays scrambles and solutions of the search on the 26 piece visual model
(see geometry.py / cube_model.py). The solver runs in a background thread
through the puppetpy module and reports every scramble/solution via a
callback; the moves are animated with pyrender (physical slice move
animation, see cube_model.py).

Between runs the cube is reset to the solved state (like the matura
renderer): with the physical slice animation a replay ends visually solved
up to a whole cube rotation (the tracked frame offset).

Usage:
    python3 gui/main.py [gui options] [solver options]

All unknown options are passed through to the solver (-r, --run_offset,
-t, -s, -m, -l, --tb_depth, ...).

Modes:
    gui       interactive pyrender viewer (orbit camera, space pauses the
              animation, runs until the window is closed) - default when a
              display is available
    offscreen renders key frames to PNG files without a window (EGL) and
              verifies that the post solution frame matches the solved
              cube up to the tracked whole cube rotation
"""

import argparse
import glob
import os
import queue
import sys
import threading
import time

import numpy as np
import pyglet

GUI_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.join(GUI_DIR, "..")


def find_module_dir():
    """Searches the default build directories for the puppetpy module."""
    for name in ("build", "build_cont"):
        directory = os.path.join(REPO_ROOT, name)
        if glob.glob(os.path.join(directory, "puppetpy*.so")):
            return directory
    raise SystemExit("puppetpy module not found - build it with cmake -B build "
                     "(BUILD_PYTHON is on by default) or pass --module_path")


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--mode", choices=["gui", "offscreen"], default=None,
                        help="gui (default with display) or offscreen (PNG output)")
    parser.add_argument("--module_path", default=None,
                        help="directory of the puppetpy module (default: search build, build_cont)")
    parser.add_argument("--out", default=os.path.expanduser("~/tmp/puppet-gui"),
                        help="output directory of the offscreen mode")
    parser.add_argument("--width", type=int, default=900)
    parser.add_argument("--height", type=int, default=700)
    parser.add_argument("--scramble_duration", type=float, default=0.1,
                        help="seconds per scramble move")
    parser.add_argument("--solution_duration", type=float, default=0.35,
                        help="seconds per solution move")
    parser.add_argument("--pause", type=float, default=0.04,
                        help="seconds between moves")
    parser.add_argument("--run_pause", type=float, default=1.0,
                        help="seconds showing the solved cube between runs")
    parser.add_argument("--lighting", action=argparse.BooleanOptionalAction, default=True,
                        help="matura thesis lighting and black outline (default)")
    args, solver_args = parser.parse_known_args()

    if args.mode is None:
        args.mode = "gui" if os.environ.get("DISPLAY") else "offscreen"
    if args.module_path is None:
        args.module_path = find_module_dir()
    args.solver_args = solver_args
    return args


# the GL platform has to be selected before pyrender is imported
ARGS = parse_args()
if ARGS.mode == "offscreen":
    os.environ.setdefault("PYOPENGL_PLATFORM", "egl")

sys.path.insert(0, GUI_DIR)
sys.path.insert(0, ARGS.module_path)

import puppetpy  # noqa: E402
import pyrender  # noqa: E402
from OpenGL.GL import GL_ARRAY_BUFFER, GL_STATIC_DRAW, glBindBuffer, glBufferData  # noqa: E402
from pyrender.constants import GLTF  # noqa: E402

from cube_model import VisualCube, derive_moves  # noqa: E402
from geometry import get_piece_geometry, SOLVED_PIECES  # noqa: E402

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
            colors = np.clip(base_color + diffusion[:, None], 0.0, 1.0) ** 2.2
            colors = np.concatenate([colors, np.full((len(colors), 1), 1.0)], axis=1)
            colors = colors.astype(np.float32)
            if not np.array_equal(primitive.color_0, colors):
                primitive.color_0 = colors
                self.pending_reupload.add(primitive)

    def flush_lighting(self):
        """Uploads the changed vertex color buffers.

        Has to run in the thread with the current gl context (the viewer
        render thread, or the main thread for the offscreen renderer).
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


class AnimationController:
    """Animates the move queue of the visual cube.

    Commands (processed strictly in order): ("move", (moves, is_scramble))
    and ("reset", None). Timing per move: [turn duration] -> [pause].
    Between runs the reset shows the solved cube for run_pause seconds.
    """

    def __init__(self, cube, scramble_duration, solution_duration, pause, run_pause):
        self.cube = cube
        self.scramble_duration = scramble_duration
        self.solution_duration = solution_duration
        self.pause = pause
        self.run_pause = run_pause
        self.commands = queue.Queue()
        self.moves = None  # list of moves of the current phase
        self.move_idx = 0
        self.duration = 0.0
        self.angle = 0.0
        self.wait = 0.0  # remaining pause

    def push_moves(self, moves, is_scramble):
        self.commands.put(("move", (moves, is_scramble)))

    def push_reset(self):
        self.commands.put(("reset", None))

    def _advance(self):
        """Starts the next move or processes the next command."""
        if self.moves is not None and self.move_idx < len(self.moves):
            self.cube.begin_move(self.moves[self.move_idx])
            self.move_idx += 1
            self.angle = 0.0
            return
        # current phase is finished (the pause after the last move already elapsed)
        if self.moves is not None:
            self.moves = None
            return
        try:
            command, payload = self.commands.get_nowait()
        except queue.Empty:
            return
        if command == "reset":
            self.cube.reset()
            self.wait = self.run_pause
        else:
            moves, is_scramble = payload
            self.moves = moves
            self.move_idx = 0
            self.duration = self.scramble_duration if is_scramble else self.solution_duration

    def step(self, dt):
        """Advances the animation by dt seconds. Returns the current angle."""
        # clamp the time step such that dropped frames never let a turn
        # complete within a single frame
        dt = min(dt, 1.0 / 30.0)
        if self.wait > 0.0:
            self.wait -= dt
            if self.wait > 0.0:
                return self.angle
        if self.moves is None:
            self._advance()
            return self.angle
        if self.cube.active_move is None:
            self._advance()
            if self.cube.active_move is None:
                return self.angle
        self.angle += dt / self.duration * (np.pi / 2.0)
        if self.angle >= np.pi / 2.0:
            self.cube.end_move()
            self.angle = 0.0
            self.wait = self.pause
        return self.angle

    def idle(self):
        return (self.moves is None and self.commands.empty() and self.wait <= 0.0
                and self.cube.active_move is None)


def run_solver(event_callback, solver_args):
    """Runs the search in the background (tables are loaded once)."""
    root_path = os.path.abspath(REPO_ROOT) + os.sep
    # -l all: the output of the c++ program is shown while the gui runs
    # (pass -l <level> to the gui to change it)
    arguments = ["--root_path", root_path, "-l", "all"] + list(solver_args)
    puppetpy.initialize(arguments)
    puppetpy.run(event_callback)


class PuppetViewer(pyrender.Viewer):
    """Interactive viewer with a pause key (space)."""

    def __init__(self, gui_scene, *viewer_args, **viewer_kwargs):
        self.gui_scene = gui_scene
        self.paused = threading.Event()
        super().__init__(*viewer_args, **viewer_kwargs)

    def on_key_press(self, symbol, modifiers):
        if symbol == pyglet.window.key.SPACE:
            if self.paused.is_set():
                self.paused.clear()
            else:
                self.paused.set()
        super().on_key_press(symbol, modifiers)

    def on_draw(self):
        # the matura lighting follows the camera - the vertex colors have to
        # be updated in the gl thread (which owns the context) before drawing
        if self.gui_scene.lighting:
            self.switch_to()
            self.gui_scene.update_lighting(self.scene.get_pose(self.scene.main_camera_node))
            self.gui_scene.flush_lighting()
        super().on_draw()


class Gui:
    """Interactive viewer mode."""

    def __init__(self, args, moves):
        self.args = args
        self.moves = moves
        self.cube = VisualCube()
        self.scene = PuppetScene(self.cube, lighting=args.lighting)
        self.controller = AnimationController(self.cube, args.scramble_duration,
                                              args.solution_duration, args.pause,
                                              args.run_pause)
        self.solver_error = None

    def _callback(self, event):
        kind = str(event.kind)
        if kind == "SearchEventKind.SCRAMBLE":
            self.controller.push_moves([self.moves[m] for m in event.moves], is_scramble=True)
        elif kind == "SearchEventKind.SOLUTION":
            self.controller.push_moves([self.moves[m] for m in event.moves], is_scramble=False)
            self.controller.push_reset()

    def _solver_thread(self):
        try:
            run_solver(self._callback, self.args.solver_args)
        except Exception as error:  # noqa: BLE001 - reported in the main loop
            self.solver_error = error

    def run(self):
        # the solver initialization releases the interpreter lock - the window
        # comes up immediately and shows the solved cube while the ~10 GB of
        # tables are loaded in the background
        solver = threading.Thread(target=self._solver_thread, daemon=True)
        solver.start()

        print("controls: mouse - orbit camera | space - pause/resume | esc - quit")
        viewer = PuppetViewer(self.scene, self.scene.scene, run_in_thread=True,
                              viewport_size=(self.args.width, self.args.height))
        last_time = time.time()
        try:
            while viewer.is_active:
                time.sleep(1.0 / 60.0)
                now = time.time()
                if not viewer.paused.is_set():
                    angle = self.controller.step(now - last_time)
                    with viewer.render_lock:
                        self.scene.update_poses(angle)
                last_time = now
        finally:
            viewer.close_external()
        if self.solver_error is not None:
            raise self.solver_error
        # the search can not be aborted - exit without unwinding the C++ threads
        sys.stdout.flush()
        os._exit(0)


class OffscreenRenderer:
    """Offscreen mode: renders key frames and verifies the end state."""

    def __init__(self, args, moves, num_runs):
        self.args = args
        self.moves = moves
        self.num_runs = num_runs
        self.out = args.out
        os.makedirs(self.out, exist_ok=True)
        self.cube = VisualCube()
        self.scene = PuppetScene(self.cube, lighting=args.lighting)
        self.renderer = pyrender.OffscreenRenderer(args.width, args.height)
        self.events = queue.Queue()
        self.solver_error = None
        self.completed = 0

    def _callback(self, event):
        self.events.put(event)

    def _solver_thread(self):
        try:
            run_solver(self._callback, self.args.solver_args)
        except Exception as error:  # noqa: BLE001 - surfaced after the event loop
            self.solver_error = error

    def render_frame(self, name, extra_rotation=None):
        if extra_rotation is None:
            self.scene.update_poses(0.0)
        else:
            # canonical frame: undo the tracked whole cube rotation
            for piece_idx, node in enumerate(self.scene.nodes):
                rotation = extra_rotation @ self.cube.rotations[piece_idx]
                if piece_idx < 6:
                    # centers can be spun around their own axis by the replay
                    # (not tracked by the search, geometrically invisible but
                    # visible in the shading) - verify the spin and align them
                    # to the solved orientation
                    snapped = np.round(rotation)
                    assert np.allclose(rotation, snapped, atol=1e-6), \
                        f"center piece {piece_idx} not solved up to the frame offset"
                    solved_rotation = SOLVED_PIECES[piece_idx][2]
                    relative = snapped @ solved_rotation.T
                    # center meshes are modeled around the +y axis
                    axis = solved_rotation @ np.array([0.0, 1.0, 0.0])
                    assert np.allclose(relative @ axis, axis), \
                        f"center piece {piece_idx} rotated off its own axis"
                    rotation = solved_rotation
                else:
                    # edges and corners are tracked: the composed rotation is
                    # an exact 90 degree multiple - snap away the float
                    # rounding so the comparison becomes pixel exact
                    snapped = np.round(rotation)
                    assert np.allclose(rotation, snapped, atol=1e-6), \
                        f"piece {piece_idx} not solved up to the frame offset"
                    rotation = snapped
                pose = np.eye(4)
                pose[:3, :3] = rotation
                self.scene.scene.set_pose(node, pose)
        if self.args.lighting:
            camera_pose = self.scene.scene.get_pose(self.scene.camera)
            self.scene.update_lighting(camera_pose, extra_rotation)
            self.scene.flush_lighting()
        color, _ = self.renderer.render(self.scene.scene)
        import imageio.v2 as imageio
        imageio.imwrite(os.path.join(self.out, name), color)
        return color

    def replay_run(self, run_idx, scramble, solution):
        self.render_frame(f"run{run_idx:02d}_00_start.png")
        for move_idx, move in enumerate(scramble):
            self.cube.apply_move(move)
        self.render_frame(f"run{run_idx:02d}_01_scrambled.png")

        for move_idx, move in enumerate(solution):
            self.cube.apply_move(move)
            self.render_frame(f"run{run_idx:02d}_solution_{move_idx + 1:02d}.png")

        # the end state is the solved cube rotated by the tracked frame offset
        offset_inv = self.cube.rotation_offset.T
        self.render_frame(f"run{run_idx:02d}_end.png")
        canonical = self.render_frame(f"run{run_idx:02d}_end_canonical.png",
                                      extra_rotation=offset_inv)
        # the composed matrices carry float rounding (~1e-15): allow a handful
        # of borderline pixels to differ from the solved reference frame
        differing = int(np.count_nonzero((canonical != self._solved_frame).any(axis=-1)))
        if differing > self.args.width * self.args.height // 1000:
            raise AssertionError(f"run {run_idx}: canonical end frame differs from the solved frame "
                                 f"({differing} pixels)")
        print(f"run {run_idx}: {len(scramble)} scramble moves, solution depth {len(solution)} - "
              f"canonical end frame matches the solved frame ({differing} borderline pixels)")
        self.cube.reset()
        self.scene.update_poses(0.0)

    def run(self):
        # reference frame of the solved cube
        self._solved_frame = self.render_frame("solved_reference.png")

        solver = threading.Thread(target=self._solver_thread, daemon=True)
        solver.start()

        runs = {}
        deadline = time.time() + 600
        while True:
            try:
                event = self.events.get(timeout=5)
            except queue.Empty:
                if not solver.is_alive() or time.time() > deadline:
                    break
                continue
            if str(event.kind) == "SearchEventKind.SCRAMBLE":
                runs[event.run_idx] = {"scramble": [self.moves[m] for m in event.moves]}
            else:
                runs[event.run_idx]["solution"] = [self.moves[m] for m in event.moves]
                self.replay_run(event.run_idx, runs[event.run_idx]["scramble"],
                                runs[event.run_idx]["solution"])
                self.completed += 1
                if event.run_idx + 1 >= self.num_runs:
                    break
        self.renderer.delete()
        if self.solver_error is not None:
            raise self.solver_error
        if self.completed == 0:
            raise RuntimeError("the solver did not complete any run")
        print(f"OFFSCREEN VERIFICATION PASSED ({self.completed} runs) - frames in {self.out}")


def main():
    args = ARGS
    moves = derive_moves(puppetpy.rotation_reps())

    # extract -r/--num_runs from the solver arguments to know when to stop
    num_runs = 10
    solver_args = list(args.solver_args)
    for idx, argument in enumerate(solver_args):
        if argument in ("-r", "--num_runs") and idx + 1 < len(solver_args):
            num_runs = int(solver_args[idx + 1])
        elif argument.startswith("--num_runs="):
            num_runs = int(argument.split("=", 1)[1])

    if args.mode == "gui":
        Gui(args, moves).run()
    else:
        OffscreenRenderer(args, moves, num_runs).run()


if __name__ == "__main__":
    main()
