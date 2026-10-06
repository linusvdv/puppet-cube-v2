"""Graphical user interface for the Puppet Cube V2 solver.

Replays scrambles and solutions of the search on the 26 piece visual model
(see geometry.py / cube_model.py). The solver runs in a background thread
through the puppetpy module and reports every scramble/solution via a
callback.

Usage:
    python3 gui/main.py [gui options] [solver options]

All unknown options are passed through to the solver (-r, --run_offset,
-t, -s, -m, -l, --tb_depth, ...).

Modes:
    qt        interactive Qt window (3d view, run list on the left, move
              timeline and transport controls at the bottom) - default
              when a display is available
    offscreen renders key frames to PNG files without a window (EGL) and
              verifies that the post solution frame matches the solved
              cube up to the tracked whole cube rotation

This module stays importable without side effects (no heavy imports at
module level) - the gl platform for the offscreen mode has to be selected
before pyrender is imported anywhere.
"""

import argparse
import glob
import os
import sys

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
    parser.add_argument("--mode", choices=["qt", "offscreen"], default=None,
                        help="qt (default with display) or offscreen (PNG output)")
    parser.add_argument("--module_path", default=None,
                        help="directory of the puppetpy module (default: search build, build_cont)")
    parser.add_argument("--out", default=os.path.expanduser("~/tmp/puppet-gui"),
                        help="output directory of the offscreen mode")
    parser.add_argument("--width", type=int, default=900)
    parser.add_argument("--height", type=int, default=700)
    parser.add_argument("--scramble_duration", type=float, default=0.05,
                        help="seconds per scramble move")
    parser.add_argument("--solution_duration", type=float, default=0.175,
                        help="seconds per solution move")
    parser.add_argument("--pause", type=float, default=0.04,
                        help="seconds between moves")
    parser.add_argument("--run_pause", type=float, default=1.0,
                        help="seconds showing the solved cube between runs (follow mode)")
    parser.add_argument("--step_duration", type=float, default=0.15,
                        help="seconds per single move step (arrow keys / step buttons)")
    parser.add_argument("--lighting", action=argparse.BooleanOptionalAction, default=True,
                        help="matura thesis lighting and black outline (default)")
    parser.add_argument("--gui_log_level", choices=["error", "warning", "info", "extra"],
                        default="info",
                        help="log level of the gui (extra adds gl diagnostics)")
    args, solver_args = parser.parse_known_args()

    if args.mode is None:
        args.mode = "qt" if os.environ.get("DISPLAY") else "offscreen"
    if args.module_path is None:
        args.module_path = find_module_dir()
    args.solver_args = solver_args
    return args


def run_solver(puppetpy, event_callback, solver_args):
    """Runs the search in the background (tables are loaded once)."""
    root_path = os.path.abspath(REPO_ROOT) + os.sep
    # -l all: the output of the c++ program is shown while the gui runs
    # (pass -l <level> to the gui to change it)
    arguments = ["--root_path", root_path, "-l", "all"] + list(solver_args)
    puppetpy.initialize(arguments)
    puppetpy.run(event_callback)


def num_runs_of(solver_args):
    """Extracts -r/--num_runs from the solver arguments (offscreen mode)."""
    num_runs = 10
    for idx, argument in enumerate(solver_args):
        if argument in ("-r", "--num_runs") and idx + 1 < len(solver_args):
            num_runs = int(solver_args[idx + 1])
        elif argument.startswith("--num_runs="):
            num_runs = int(argument.split("=", 1)[1])
    return num_runs


def main():
    args = parse_args()

    # the gl platform has to be selected before pyrender is imported;
    # in the qt process it must NOT be egl (the qt context is glx based and
    # PyOpenGL would not see it)
    if args.mode == "offscreen":
        os.environ.setdefault("PYOPENGL_PLATFORM", "egl")

    sys.path.insert(0, GUI_DIR)
    sys.path.insert(0, args.module_path)

    import puppetpy
    from cube_model import derive_moves
    moves = derive_moves(puppetpy.rotation_reps())

    if args.mode == "qt":
        from qt_gui import run_qt_gui
        run_qt_gui(args, moves, puppetpy)
    else:
        from offscreen import OffscreenRenderer
        OffscreenRenderer(args, moves, num_runs_of(args.solver_args), puppetpy).run()


if __name__ == "__main__":
    main()
