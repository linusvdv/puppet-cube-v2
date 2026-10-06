#!/usr/bin/env bash
# creates the python virtual environment for the gui, installs the packages
# and activates it
#
# the activation only has an effect when the script is sourced:
#     source gui/setup_venv.sh
# the name of the virtual environment can be changed (e.g. when the same
# workspace is used from different machines):
#     PUPPET_VENV=.venv_cont source gui/setup_venv.sh
#
# headless offscreen rendering additionally needs the apt packages
# libegl1 and libgles2; the qt gui additionally needs the qt system
# libraries (present on any desktop): libxkbcommon0 libxkbcommon-x11-0
# libxcb-cursor0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1
# libxcb-render-util0 libxcb-render0 libxcb-shape0 libxcb-util1
# libxcb-xkb1 libxkbfile1 libxcomposite1 libxdamage1 libxfixes3
# libxrandr2 libxtst6 libwayland-cursor0 libwayland-egl1
# libwayland-server0 libpcsclite1 libpulse0

GUI_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV="$GUI_DIR/../${PUPPET_VENV:-.venv}"

# create the virtual environment and install the packages
if ! "$VENV/bin/python" -c "import pyrender, trimesh, OpenGL, imageio, pybind11, PySide6" > /dev/null 2>&1; then
    python3 -m venv "$VENV"
    "$VENV/bin/pip" install --upgrade pip
    # numpy < 2: the tested configuration (pyrender 0.1.45 predates numpy 2)
    "$VENV/bin/pip" install pyrender trimesh PyOpenGL imageio pybind11 PySide6 "numpy<2"
fi

# shellcheck disable=SC1091
source "$VENV/bin/activate"
echo "python venv for the gui ready: $VIRTUAL_ENV"
