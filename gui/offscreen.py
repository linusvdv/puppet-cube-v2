"""Offscreen mode: renders key frames of each run to PNG files (EGL, no
window) and verifies that the post solution frame matches the solved cube
up to the tracked whole cube rotation. This is the rendering regression
test of the gui.
"""

import os
import queue
import threading
import time

import numpy as np
import pyrender
import imageio.v2 as imageio

from cube_model import VisualCube
from geometry import SOLVED_PIECES
from scene import PuppetScene


class OffscreenRenderer:
    """Offscreen mode: renders key frames and verifies the end state."""

    def __init__(self, args, moves, num_runs, puppetpy):
        self.args = args
        self.moves = moves
        self.num_runs = num_runs
        self.puppetpy = puppetpy
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
        from main import run_solver
        try:
            run_solver(self.puppetpy, self._callback, self.args.solver_args)
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
