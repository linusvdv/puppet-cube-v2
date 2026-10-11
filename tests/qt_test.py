"""Headless smoke test of the Qt application (real solver, offscreen platform).

Drives the MainWindow with the real puppetpy solver (2 runs) and checks the
full signal/selection/transport flow: auto-follow playback, run list, late
solution, timeline clicks, steps, jumps, and the boundary pose invariant
(jumping to the end leaves the cube solved in the bookkeeping).

The GL rendering itself is verified by the offscreen mode (see offscreen.py)
- here the QOpenGLWidget may or may not get a context under the offscreen
platform, its state is only reported.

Run:  QT_QPA_PLATFORM=offscreen python tests/qt_test.py
"""

import os
import sys
import time

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "gui"))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build_cont"))

from PySide6.QtWidgets import QApplication  # noqa: E402

import puppetpy  # noqa: E402
from cube_model import derive_moves  # noqa: E402
from main import find_module_dir, parse_args  # noqa: E402
from qt_gui import MainWindow  # noqa: E402
import timeline  # noqa: E402


class Args:
    mode = "qt"
    module_path = find_module_dir()
    width = 900
    height = 700
    scramble_duration = 0.02
    solution_duration = 0.05
    pause = 0.005
    step_duration = 0.05
    lighting = True
    gui_log_level = "info"
    run_pause = 0.1
    edit = False
    # extra command line arguments are passed to the solver (e.g.
    # --use_cuda=false when no gpu memory is free)
    solver_args = ["-r", "4", "-t", "8"] + sys.argv[1:]


def process(app, seconds):
    end = time.time() + seconds
    while time.time() < end:
        app.processEvents()
        time.sleep(0.005)


def main():
    # the Args class below must stay in sync with the real argument parser
    # (a missing attribute would only crash on the user's machine, the test
    # namespace always has them all)
    saved_argv = sys.argv
    sys.argv = ["qt_test"]
    try:
        parsed = parse_args()
    finally:
        sys.argv = saved_argv
    args_keys = {k for k in vars(Args) if not k.startswith("_")}
    assert args_keys <= set(vars(parsed)), \
        f"Args uses attributes missing from main.parse_args: {args_keys - set(vars(parsed))}"

    app = QApplication([])
    moves = derive_moves(puppetpy.rotation_reps())
    window = MainWindow(Args(), moves, puppetpy)
    window.show()
    app.processEvents()
    print(f"gl context in offscreen platform: {'yes' if window.view.renderer is not None else 'no'}")

    # wait for all runs to appear and finish
    num_runs = 4
    deadline = time.time() + 240
    while time.time() < deadline:
        process(app, 0.2)
        if window.runs_list.count() >= num_runs:
            if all(window.runs[i].solution is not None for i in range(num_runs)):
                break
    assert window.runs_list.count() == num_runs, f"run list: {window.runs_list.count()}"
    for i in range(num_runs):
        assert window.runs[i].solution is not None, f"run {i} has no solution"
    assert window.follow_box.isChecked(), "follow must still be enabled"
    # a normal run shows BOTH lines (scramble + solution) - the scroll
    # area has a fixed height, so it has to track the two-row timeline
    assert not window.timeline.single_row
    assert window.timeline.minimumSizeHint().height() >= \
        timeline.TOP + 2 * timeline.CHIP_H + timeline.ROW_SPACING
    assert window.timeline_scroll.height() >= \
        window.timeline.minimumSizeHint().height(), \
        "the timeline must not clip the solution line"
    print(f"runs listed, follow active: depths "
          f"{[window.runs[i].depth for i in range(num_runs)]}")

    # the follow must not cut off the run 0 replay: while it is still
    # playing, no other run is loaded
    if window.controller.run_id == 0:
        assert window.controller.is_playing or window.controller.is_animating, \
            "run 0 replay must still be running"
        print("run switching deferred while the previous replay runs")
    # the follow advances to the NEXT run - runs 1..3 are all computed by
    # now, but the first switch must be to run 1, not to the latest
    deadline = time.time() + 120
    while window.controller.run_id == 0 and time.time() < deadline:
        process(app, 0.1)
    assert window.controller.run_id == 1, \
        "follow must switch to the NEXT run, not the latest"
    print("follow switched to the next run (not the latest)")

    # auto follow plays the remaining runs one after another and parks at
    # the end of the last one
    deadline = time.time() + 240
    while (window.controller.run_id != num_runs - 1
           or window.controller.state != "idle"
           or window.controller.is_animating) and time.time() < deadline:
        process(app, 0.1)
    assert window.controller.run_id == num_runs - 1
    assert window.controller.boundary == window.controller.num_moves, \
        "follow playback must reach the end of the run"
    assert window.timeline.boundary == window.controller.num_moves, "playhead synced"
    assert window.cube.is_solved_bookkeeping(), "end of replay must be solved"
    print("auto-follow playback completed, cube solved")

    # manual selection of run 0 disables follow and shows the scrambled state
    window.runs_list.setCurrentRow(0)
    assert not window.follow_box.isChecked(), "manual selection must disable follow"
    assert window.controller.run_id == 0
    assert window.controller.boundary == window.controller.scramble_len, \
        "manual selection loads at the scrambled position, paused"
    assert not window.controller.is_playing
    assert not window.cube.is_solved_bookkeeping(), "scrambled state is not solved"
    print("manual selection: follow off, scrambled state shown")

    # timeline click jumps to a boundary (row 1 = solution line)
    target = window.controller.scramble_len + 3
    window.on_timeline_click(target, 1)
    assert window.controller.boundary == target
    assert window.timeline.boundary == target
    print("timeline click: boundary jump")

    # single steps (animated)
    window.on_step_forward()
    deadline = time.time() + 10
    while window.controller.is_animating and time.time() < deadline:
        process(app, 0.02)
    assert window.controller.boundary == target + 1
    window.on_step_backward()
    deadline = time.time() + 10
    while window.controller.is_animating and time.time() < deadline:
        process(app, 0.02)
    assert window.controller.boundary == target
    print("single steps forwards and backwards")

    # play/pause
    window.on_toggle_play()
    assert window.controller.is_playing
    process(app, 0.3)
    assert window.controller.boundary > target, "playback must advance"
    window.on_toggle_play()
    assert not window.controller.is_playing
    boundary = window.controller.boundary
    process(app, 0.2)
    assert window.controller.boundary == boundary, "paused playback must freeze"
    print("play/pause")

    # jumps and the pose invariant
    window.on_jump_solved()
    assert window.controller.boundary == window.controller.num_moves
    assert window.cube.is_solved_bookkeeping(), "jump to solved must solve the cube"
    window.on_jump_scrambled()
    assert window.controller.boundary == window.controller.scramble_len
    assert not window.cube.is_solved_bookkeeping()
    # the ambiguous scrambled boundary shows its playhead at the start of
    # the solution line
    assert window.timeline.boundary_row == 1, \
        "jump to scrambled must put the playhead on the solution line"
    print("jumps with pose invariant")

    # boundary equivalence against sequential replay for every boundary
    controller = window.controller
    reference = window.cube
    for b in range(controller.num_moves + 1):
        controller.set_boundary(b)
        app.processEvents()
        replay = __import__("cube_model").VisualCube()
        for move in controller.moves[:b]:
            replay.apply_move(move)
        assert all(__import__("numpy").array_equal(a, r)
                   for a, r in zip(reference.rotations, replay.rotations)), f"boundary {b}"
    print("boundary pose equivalence for all boundaries")

    # --- editor: paint run 0's scrambled position and solve it -------------
    import numpy as np
    import editor
    from cube_model import VisualCube
    from editor_test import paint_from_cube
    from geometry import COLORS

    def paint_sticker(piece, group, color):
        """Programmatic paint keeping the scene in sync (like a user click
        through on_editor_pick)."""
        window._stop_flash()
        window.editor_state.set_color(piece, group, color)
        rgb = editor.GRAY if color is None else COLORS[color]
        window.editor_scene.recolor_groups({(piece, group): rgb})

    def scene_matches_state():
        scene = window.editor_scene
        for p in range(6, 26):
            for g in range(len(scene.groups[p])):
                verts = scene.groups[p][g][0].reshape(-1)
                shown = (scene.base_colors[p][verts[0]] if scene.lighting
                         else scene.primitives[p].color_0[verts[0], :3])
                color = window.editor_state.sticker_color(p, g)
                expected = editor.GRAY if color is None else COLORS[color]
                if not np.allclose(shown, expected):
                    return False
        return True

    def sync_scene_from_state():
        """Recolors the whole editor scene from the state (programmatic
        paints bypass on_editor_pick)."""
        recolors = {}
        for p in range(6, 26):
            for g in range(len(window.editor_scene.groups[p])):
                color = window.editor_state.sticker_color(p, g)
                recolors[(p, g)] = editor.GRAY if color is None else COLORS[color]
        window.editor_scene.recolor_groups(recolors)

    replay = VisualCube()
    for move in window.runs[0].scramble:
        replay.apply_move(move)

    window.set_edit_mode(True)
    assert window.editing and window.view.gui_scene is window.editor_scene
    assert window.view.pick_handler is not None
    assert window.editor_panel.isVisible() and not window.follow_box.isChecked()
    print("edit mode on")

    # an incomplete cube is rejected without contacting the solver
    window.on_editor_solve()
    assert window._solver_tasks.empty(), "incomplete position must not be submitted"
    assert "gray" in window.editor_panel.status.text()
    assert window._flash_groups is not None, "invalid pieces must flash"
    window._stop_flash()
    print("incomplete position rejected")

    paint_from_cube(window.editor_state, replay)
    sync_scene_from_state()
    window._update_editor_status()
    assert "ready to solve" in window.editor_panel.status.text(), \
        window.editor_panel.status.text()
    # a complete cube uses all 8 manually paintable stickers per color
    assert window.editor_state.remaining_per_color() == [0] * 6
    assert all(button.text() == "0" for button in window.editor_panel.color_buttons)
    print("painted position validates")

    # undo: the last paint action is reverted (state, scene and counts)
    old = window.editor_state.sticker_color(9, 0)
    paint_sticker(9, 0, (old + 1) % 6)
    assert window.editor_state.remaining_per_color()[old] == 1
    assert window.editor_state.remaining_per_color()[(old + 1) % 6] == -1
    window.on_editor_undo()
    assert window.editor_state.sticker_color(9, 0) == old
    assert window.editor_state.remaining_per_color() == [0] * 6
    assert scene_matches_state(), "undo must keep the scene in sync"
    assert all(button.text() == "0" for button in window.editor_panel.color_buttons)
    print("undo reverts the last paint action")

    # a parity broken position (one flipped edge) is rejected immediately
    # by the pre-check - the pieces flash dark red, the solver queue stays
    # untouched (regression: the exception used to kill the solver thread
    # and every later solve hung in "solving the custom position...")
    c0 = window.editor_state.sticker_color(6, 0)
    c1 = window.editor_state.sticker_color(6, 1)
    paint_sticker(6, 0, c1)
    paint_sticker(6, 1, c0)
    window.on_editor_solve()
    assert window._solver_tasks.empty(), "parity broken position must not be submitted"
    assert window.editing, "a rejected position stays in the editor"
    assert window._flash_groups is not None, "parity error must flash the edges"
    assert (6, 0) in window._flash_groups, "the flipped edge class flashes"
    deadline = time.time() + 10
    while window._flash_groups is not None and time.time() < deadline:
        process(app, 0.1)
    assert window._flash_groups is None, "the flash must end by itself"
    print("parity broken position rejected with flashing pieces")

    # puppet cube preview: the real piece shapes of the painted position
    paint_sticker(6, 0, c0)
    paint_sticker(6, 1, c1)
    window.editor_panel.puppet_view.setChecked(True)
    assert window.view.gui_scene is window.editor_preview_scene
    assert window.view.pick_handler is None, "no painting in the preview"
    assert window.editor_panel.puppet_view.text() == "standard cube", \
        "the toggle label names the view it switches to"
    window.editor_panel.puppet_view.setChecked(False)
    assert window.view.gui_scene is window.editor_scene
    assert window.view.pick_handler is not None
    assert window.editor_panel.puppet_view.text() == "puppet cube"
    paint_sticker(6, 0, c1)  # invalid again
    window.editor_panel.puppet_view.setChecked(True)
    assert not window.editor_panel.puppet_view.isChecked(), \
        "the preview must be refused for an invalid position"
    assert window.view.gui_scene is window.editor_scene
    window._stop_flash()
    paint_sticker(6, 0, c0)
    print("puppet cube preview toggle")

    # regression: a second flash (different piece class) must not restore
    # the color capture of the first flash over later paints - the scene
    # colors have to match the editor state after every flash
    # (scene_matches_state is defined at the top of the editor section)

    # twist one corner (cyclic color rotation) -> corner orientation parity
    cc = [window.editor_state.sticker_color(18, g) for g in range(3)]
    paint_sticker(18, 0, cc[1])
    paint_sticker(18, 1, cc[2])
    paint_sticker(18, 2, cc[0])
    window.on_editor_solve()
    assert window._flash_groups is not None and (18, 0) in window._flash_groups, \
        "a twisted corner must flash the corners"
    deadline = time.time() + 10
    while window._flash_groups is not None and time.time() < deadline:
        process(app, 0.1)
    assert scene_matches_state(), "the corner flash must restore the colors"
    # paint the fix (like the user would)
    for g in range(3):
        paint_sticker(18, g, cc[g])
    assert scene_matches_state()
    # break an edge: the edge flash must not reset the corner colors
    paint_sticker(6, 0, c1)
    paint_sticker(6, 1, c0)
    window.on_editor_solve()
    assert window._flash_groups is not None and (6, 0) in window._flash_groups
    deadline = time.time() + 10
    while window._flash_groups is not None and time.time() < deadline:
        process(app, 0.1)
    assert window._flash_groups is None, "the flash must end by itself"
    assert scene_matches_state(), \
        "the second flash must not reset other pieces' colors"
    paint_sticker(6, 0, c0)
    paint_sticker(6, 1, c1)
    print("consecutive flashes keep the painted colors")

    # a failing solver task does not kill the task queue
    def boom(_puppetpy):
        raise RuntimeError("task queue failure test")
    window._solver_tasks.put(boom)

    window.on_editor_solve()
    assert not window.editing, "solving must leave the edit mode"
    assert window.view.gui_scene is window.gui_scene
    custom_id = 1000000
    # the custom run shows automatically when its search starts (the solve
    # button jumps to it - the follow mode is off after editing)
    deadline = time.time() + 240
    while time.time() < deadline:
        process(app, 0.2)
        if custom_id in window.runs:
            break
    assert custom_id in window.runs, "custom run did not start"
    assert window.controller.run_id == custom_id, \
        "the custom run must be shown automatically"
    assert window.timeline.single_row
    assert window.timeline_scroll.height() < timeline.TOP + 2 * timeline.CHIP_H, \
        "custom runs collapse the scramble line (one row)"
    # boundary 0 is the painted position (the autoplay may already have
    # advanced when the solution arrived quickly - jump back to check)
    window.controller.pause()
    window.controller.jump_scrambled()
    for p in range(26):
        assert [int(c) for c in window.cube.grids[p]] == [int(c) for c in replay.grids[p]], \
            f"boundary 0 must show the painted position (piece {p})"
    assert not window.cube.is_solved_bookkeeping()
    print("custom run loaded automatically at the painted position")

    deadline = time.time() + 240
    while time.time() < deadline:
        process(app, 0.2)
        if window.runs[custom_id].solution is not None:
            break
    assert window.runs[custom_id].solution is not None, "custom run did not solve"
    assert window.runs_list.count() == num_runs + 1
    assert window.runs[custom_id].depth == window.runs[0].depth, \
        "the painted position of run 0 must solve to the same depth"
    print(f"custom position solved (depth {window.runs[custom_id].depth}, "
          f"same as the depth of run 0)")

    # play the solution to the end
    window.on_toggle_play()
    assert window.controller.is_playing
    deadline = time.time() + 120
    while (window.controller.state != "idle" or window.controller.is_animating) \
            and time.time() < deadline:
        process(app, 0.05)
    assert window.controller.boundary == window.controller.num_moves
    assert window.cube.is_solved_bookkeeping(), "custom solution replay must solve"
    print("custom solution replay solved the cube")

    # leaving the editor via the button restores the run view
    window.set_edit_mode(True)
    window.set_edit_mode(False)
    assert window.view.gui_scene is window.gui_scene
    print("edit mode toggling restores the run view")

    # leaving the editor with no run selected selects run 0 (the "done
    # during the precomputation" flow - done must land somewhere)
    window.set_edit_mode(True)
    window.controller.pause()
    window.controller.set_run(None, [], None)  # nothing selected
    window.set_edit_mode(False)
    assert window.controller.run_id == 0, \
        "done must select run 0 when no run is selected"
    assert window.controller.boundary == window.controller.scramble_len
    assert not window.timeline.single_row, "run 0 shows both lines"
    assert window._auto_show_first_run is False
    print("leaving the editor selects run 0 when nothing is loaded")

    print("ALL QT TESTS PASSED")
    os._exit(0)


if __name__ == "__main__":
    main()
