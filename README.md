# puppet-cube-v2

[PuppetCubeV2.webm](https://github.com/user-attachments/assets/a0779cad-28bf-48d4-9e59-f1440ea8e9c0)

<!--toc:start-->
- [puppet-cube-v2](#puppet-cube-v2)
  - [Abstract](#abstract)
  - [Compilation](#compilation)
  - [Help](#help)
<!--toc:end-->

## Abstract

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
    -s --scrambling_depth      how many moves to scramble                                 [100]          (0, 1000000)
    -m --min_corner_heuristic  all starting position have at least this corner heuristic  [0]            (0, 27)

    -t --threads               number of threads used in the program                      [MAX_THREADS]  (1, MAX_THREADS)

    --tb_depth                 depth of the tablebase (9 uses 40 GB RAM)                  [6]            (0, 9)
```
