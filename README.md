# Puppet Cube V2

[PuppetCubeV2.webm](https://github.com/user-attachments/assets/a0779cad-28bf-48d4-9e59-f1440ea8e9c0)

An optimal solver for the Puppet Cube V2, a shapeshifting variant of the Rubik's Cube with about $5 \cdot 10^{18}$ positions. It finds a shortest possible solution for any scrambled cube and proves that it is optimal. With GPU acceleration this takes a fraction of a second for a random position on high-end consumer hardware, and only slightly longer for the hardest positions. A 3D viewer replays every scramble and solution.

<!--toc:start-->
- [Puppet Cube V2](#puppet-cube-v2)
  - [Features](#features)
  - [Requirements](#requirements)
  - [Build](#build)
  - [Run](#run)
  - [Graphical user interface](#graphical-user-interface)
  - [Project history](#project-history)
  - [Abstract of the matura thesis](#abstract-of-the-matura-thesis)
  - [Future plans](#future-plans)
<!--toc:end-->

## Features

- **Optimal solving.** Search with precomputed heuristics and a tablebase, with proof of optimality.
- **GPU or CPU.** The CUDA search is the fast path. A CPU-only search is available when no GPU is present.
- **Low-memory mode.** A compile-time option cuts memory use to roughly a third, at the cost of speed.
- **3D viewer.** A Qt/pyrender GUI replays scrambles and solutions while the solver keeps running.

## Requirements

- A C++20 compiler and CMake
- Optional: the CUDA toolkit and an NVIDIA GPU for the fast search (build with `-DUSE_CUDA=OFF` otherwise)
- Optional: Python 3 for the GUI (the setup script installs the Python packages)
- Network access during the first configure, which downloads [parallel-hashmap](https://github.com/greg7mdp/parallel-hashmap)

The solver needs large precomputed tables. The memory use depends on how you build it:

|                        | Default build | `REDUCE_MEMORY=ON` |
|------------------------|---------------|--------------------|
| Disk (`precomputation/`) | ~8 GB       | ~2.6 GB            |
| RAM                    | ~10 GB        | ~4.5 GB            |
| GPU memory             | ~8 GB         | ~2.5 GB            |

These are the values for the default tablebase depth. Deeper tablebases need more (`--tb_depth 9` uses about 40 GB of RAM). GPU memory is only needed for the CUDA search.

The tables are generated automatically on the first start if they are missing. This takes about 5 minutes for the default build (with a peak of roughly 20 GB of RAM) and under a minute with `REDUCE_MEMORY=ON`.

## Build

The GUI's Python module (`puppetpy`) is built by default and needs pybind11, so set up the Python environment first. The script has to be sourced so that the activation persists:

```bash
source gui/setup_venv.sh
cmake -B build
cmake --build build -j
```

The binary is written to `build/bin/PuppetCubeV2`.

### Build options

| Option | Effect |
|--------|--------|
| `-DUSE_CUDA=OFF` | Build without CUDA (CPU search only) |
| `-DREDUCE_MEMORY=ON` | Use two small edge heuristics instead of the large one (less memory, slower) |
| `-DBUILD_PYTHON=OFF` | Skip the Python module and the GUI dependency |
| `-DCMAKE_CUDA_COMPILER=<path>` | Select a specific `nvcc` |
| `-DCUDA_ARCH=<n>` | Select the GPU architecture, e.g. `90` |
| `-DCMAKE_BUILD_TYPE=Debug` | Debug build (the default is Release with LTO) |

Options can be combined, for example:

```bash
cmake -B build -DUSE_CUDA=OFF -DBUILD_PYTHON=OFF -DREDUCE_MEMORY=ON
cmake --build build -j
```

## Run

Run the solver from the repository root:

```bash
./build/bin/PuppetCubeV2
```

If you start it from elsewhere, pass `--root_path=/path/to/puppet-cube-v2/` so it can find the `precomputation/` folder.

The most useful options:

| Option | Description | Default |
|--------|-------------|---------|
| `--use_cuda` | Use the GPU search (`false` for CPU) | true if built with CUDA |
| `-t`, `--threads` | Number of threads | all |
| `-m`, `--min_corner_heuristic` | Only use start positions with at least this corner heuristic (use 27 for the hardest cubes) | 0 |
| `-r`, `--num_runs` | Number of scrambled cubes to solve | 10 |
| `--run_offset` | Start at a given run number (scrambles are reproducible) | 0 |
| `-l`, `--log_level` | Amount of output (`critical` ... `memory`) | `memory` (the most verbose) |

For example, to solve one cube and print less output:

```bash
./build/bin/PuppetCubeV2 -r 1 -l extra
```

The full list of options is in [docs/OPTIONS.md](docs/OPTIONS.md).

## Graphical user interface
<img width="1856" height="1532" alt="PuppetCubeV2Renderer" src="https://github.com/user-attachments/assets/299fcf6e-0d14-4841-86cc-61e6220e4e6d" />

The GUI replays the scrambles and solutions on a 3D model of the cube, rendered with [pyrender](https://github.com/mmatl/pyrender) inside a [Qt](https://www.qt.io) window. The solver runs in the background through the `puppetpy` binding and reports every scramble and solution to the GUI. With a CUDA build it uses the GPU search; pass `--use_cuda=false` to use the CPU search.

```bash
python3 gui/main.py                 # interactive viewer with the matura thesis lighting
python3 gui/main.py --no-lighting   # flat colors without the black outline
python3 gui/main.py -r 5 -t 8       # solver options are passed through
```

Without a display:

```bash
python3 gui/main.py --mode offscreen -r 2
```

`gui/setup_venv.sh` creates a `.venv/` with pyrender, trimesh, PyOpenGL, imageio, pybind11, PySide6 and numpy<2. The Qt GUI also needs the usual desktop libraries (libxkbcommon, libxcb-\*, libpulse, ...). Any desktop system has them, and the full list is in the script. Headless rendering additionally needs `libegl1` and `libgles2`.

## Project history

The project started as a matura thesis and was later rebuilt from the ground up, with AI assistance used from v2.0 onwards. The full story, including what changed in each release, is in [docs/HISTORY.md](docs/HISTORY.md).

## Abstract of the matura thesis

In this thesis, the Puppet Cube V2, a shapeshifting variant of the classic Rubik’s Cube, is investigated in two parts, namely its 3D rendering and its solution finding with the help of a search. The interactive visualization of this cube incorporates features such as lighting and transparency. The primary focus of this study was the search. The Puppet Cube V2, represented as a graph, is used to investigate five different graph algorithms. The resulting program is able to find short solutions to randomly scrambled cubes quickly and improves the found solution with additional search time. A comprehensive description of the final implementation is provided, which is able to prove an optimal solution, although there exist $5 \cdot 10^{18}$ positions of the Puppet Cube V2. The algorithm runs in parallel to enhance computational efficiency. Additionally, the thesis presents key properties of the Puppet Cube V2 and the employed algorithm. Notably, a lower bound for God’s Number is established, which shows that there exist positions where 30 moves are required to solve the cube. Furthermore, the research highlights improvements in the average depth when searching for longer. Finally, a comparison to a state-of-the-art Rubik’s Cube solver further proves the effectiveness of the proposed approach.

## Future plans

- Solve arbitrary user-defined positions, after checking that the cube is in a legal, solvable state
- A neural network for image recognition to detect the user's position
- Possibly a web interface
