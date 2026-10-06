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
# libegl1 and libgles2

GUI_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV="$GUI_DIR/../${PUPPET_VENV:-.venv}"

# create the virtual environment and install the packages
if ! "$VENV/bin/python" -c "import pyrender, trimesh, OpenGL, imageio, pybind11" > /dev/null 2>&1; then
    python3 -m venv "$VENV"
    "$VENV/bin/pip" install --upgrade pip
    # numpy < 2: the tested configuration (pyrender 0.1.45 predates numpy 2)
    "$VENV/bin/pip" install pyrender trimesh PyOpenGL imageio pybind11 "numpy<2"
fi

# shellcheck disable=SC1091
source "$VENV/bin/activate"
echo "python venv for the gui ready: $VIRTUAL_ENV"
