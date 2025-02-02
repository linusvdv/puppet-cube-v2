# puppet-cube-v2

[PuppetCubeV2.webm](https://github.com/user-attachments/assets/a0779cad-28bf-48d4-9e59-f1440ea8e9c0)

<!--toc:start-->
- [puppet-cube-v2](#puppet-cube-v2)
  - [Abstract](#abstract)
  - [Pre compilation of position_data](#pre-compilation-of-positiondata)
  - [Compilation](#compilation)
  - [Run](#run)
  - [Help](#help)
<!--toc:end-->
## Abstract

In this thesis, the Puppet Cube V2, a shapeshifting variant of the classic Rubik’s Cube, is investigated in two parts, namely its 3D rendering and its solution finding with the help of a search. The interactive visualization of this cube incorporates features such as lighting and transparency. The primary focus of this study was the search. The Puppet Cube V2, represented as a graph, is used to investigate five different graph algorithms. The resulting program is able to find short solutions to randomly scrambled cubes quickly and improves the found solution with additional search time. A comprehensive description of the final implementation is provided, which is able to prove an optimal solution, although there exist $5 \cdot 10^{18}$ positions of the Puppet Cube V2. The algorithm runs in parallel to enhance computational efficiency. Additionally, the thesis presents key properties of the Puppet Cube V2 and the employed algorithm. Notably, a lower bound for God’s Number is established, which shows that there exist positions where 30 moves are required to solve the cube. Furthermore, the research highlights improvements in the average depth when searching for longer. Finally, a comparison to a state-of-the-art Rubik’s Cube solver further proves the effectiveness of the proposed approach.

## Pre compilation of position_data

Time: ca. 3 min.

```bash
cd position_data/
g++ -Wall -Wextra -g3 -std=c++20 -O3 corner-data.cpp -o corner-data
./corner-data
g++ -Wall -Wextra -g3 -std=c++20 -O3 edge-data.cpp -o edge-data
./edge-data
cd ../
```

## Compilation

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Compilation without GUI. You do not need OpenGL:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DGUI=OFF
cmake --build build -j
```

## Run

```bash
./build/bin/PuppetCubeV2
```

## Help
```
--help                  shows this message
--gui                   graphical user interface [true/false]
--rootPath              path to puppet-cube-v2/
--errorLevel            amount of output [criticalError/error/info/all/extra/memory]
--threads               number of threads [int >= 1]
--runs                  number of runs/start positions/scrambles [int >= 0]
--positions             number of positions searched [int64_t >= 0]
--tablebase_depth       depth of tablebase [int >= 0] be aware 9 is already ca. 40GB RAM
--scramble_depth        scramble depth [int >= 0]
--start_offset          start offset to start from a different position [int >= 0]
--min_depth             stops if it found a solution less or equal to min_depth [int >= 0]
--min_coner_heuristic   scrambles until it finds a cube with this corner heuristic or higher [27 >= int >= 0]

Example: ./build/bin/PuppetCubeV2 --gui=false --rootPath=./ --errorLevel=extra --threads=1 --runs=10 --positions=1000000 --tablebase_depth=7 --scramble_depth=10
```
