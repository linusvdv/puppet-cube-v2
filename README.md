# Puppet Cube V2

[PuppetCubeV2.webm](https://github.com/user-attachments/assets/a0779cad-28bf-48d4-9e59-f1440ea8e9c0)

<!--toc:start-->
- [Puppet Cube V2](#puppet-cube-v2)
  - [About this project](#about-this-project)
  - [Abstract of the matura thesis](#abstract-of-the-matura-thesis)
  - [Compilation](#compilation)
  - [Help](#help)
<!--toc:end-->

## About this project

This project started as a matura thesis, a graded project at the end of my Gymnasium (high school). The Puppet Cube V2 is a shapeshifting variant of the Rubik's Cube. The program consists of two main parts: 3D rendering of the cube and solution finding with the help of a search. It quickly finds short solutions to randomly scrambled cubes and improves them with additional search time. Given enough time and memory, this implementation can even prove a solution optimal, i.e. shortest possible. The state of the project from the matura thesis can be found at release [tag v1.0](https://github.com/linusvdv/puppet-cube-v2/tree/v1.0).

The following year was spent redesigning the project from the ground up, and the result can be found at [tag v2.0](https://github.com/linusvdv/puppet-cube-v2/tree/v2.0). Further breakthroughs in the heuristic algorithms were found, and GPU acceleration was added, which required writing a new search from scratch. This new implementation proves optimality directly, in a few seconds, and in under a second even for the hardest cubes on strong hardware. Up to this point, no AI was used in this project.

From v2.0 onwards, AI is being used for three main purposes: tidying up the search so the code can run on the CPU alone, re-adding a 3D rendering of the cube, and verifying correctness, improving runtime efficiency, and optionally reducing memory usage (selectable at compile time) by using the old edge heuristic.

Future plans include solving arbitrary user-defined positions (after checking that the cube is in a legal, solvable state), adding a neural network for image recognition to detect the user's position, and possibly a web interface.

## Abstract of the matura thesis

In this thesis, the Puppet Cube V2, a shapeshifting variant of the classic Rubik’s Cube, is investigated in two parts, namely its 3D rendering and its solution finding with the help of a search. The interactive visualization of this cube incorporates features such as lighting and transparency. The primary focus of this study was the search. The Puppet Cube V2, represented as a graph, is used to investigate five different graph algorithms. The resulting program is able to find short solutions to randomly scrambled cubes quickly and improves the found solution with additional search time. A comprehensive description of the final implementation is provided, which is able to prove an optimal solution, although there exist $5 \cdot 10^{18}$ positions of the Puppet Cube V2. The algorithm runs in parallel to enhance computational efficiency. Additionally, the thesis presents key properties of the Puppet Cube V2 and the employed algorithm. Notably, a lower bound for God’s Number is established, which shows that there exist positions where 30 moves are required to solve the cube. Furthermore, the research highlights improvements in the average depth when searching for longer. Finally, a comparison to a state-of-the-art Rubik’s Cube solver further proves the effectiveness of the proposed approach.

## Compilation

```bash
cmake -B build
cmake --build build -j
```

Extra arguments:

```bash
cmake -B build -DCMAKE_CUDA_COMPILER=/usr/local/cuda-12.8/bin/nvcc -DCMAKE_BUILD_TYPE=Debug -DCUDA_ARCH=90
cmake --build build -j
```

Without cuda:

```bash
cmake -B build -DUSE_CUDA=OFF
cmake --build build -j
```

## Help

```
usage: ./build/bin/PuppetCubeV2 [options]
    --option=value
    --option value
    -ovalue
    -o value

list of options
    -h --help                  show this message

    --root_path                path to root folder puppet-cube-v2                         [./PathToPuppetCubeV2/../../]
    --use_cuda                 run cuda                                                   [USE_CUDA]     (true|1|false|0)
    -d --device_count          number of gpu                                              [NUM_GPUS]     (1, NUM_GPUS)
    -i --info                  show additional hardware info
    -l --log_level             logger/error level                                         [memory]       (critical|error|warning|info|all|extra|memory)

    -r --num_runs              number of runs                                             [10]           (0, 1e18)
    --run_offset               start at a specific run number                             [0]            (0, 1e18)
    -s --scrambling_depth      how many moves to scramble                                 [100]          (0, 1000000)
    -m --min_corner_heuristic  all starting position have at least this corner heuristic  [0]            (0, 27)

    -t --threads               number of threads used in the program                      [MAX_THREADS]  (1, MAX_THREADS)
    --tt_size                  size of the transposition table in MB                      [1000]         (128, 1000000)

    --tb_depth                 depth of the tablebase (9 uses 40 GB RAM)                  [6]            (0, 9)
```
