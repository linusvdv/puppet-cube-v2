# puppet-cube-v2

<!--toc:start-->
- [puppet-cube-v2](#puppet-cube-v2)
  - [Pre compilation of position_data](#pre-compilation-of-positiondata)
  - [Compilation](#compilation)
  - [Run](#run)
  - [Help](#help)
<!--toc:end-->

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
--help              shows this message
--gui               graphical user interface [true/false]
--rootPath          path to puppet-cube-v2/
--errorLevel        amount of output [criticalError/error/info/all/extra/memory]
--threads           number of threads [int >= 1]
--runs              number of runs/start positions/scrambles [int >= 0]
--positions         number of positions searched [int64_t >= 0]
--tablebase_depth   depth of tablebase [int >= 0] be aware 9 is already ca. 40GB RAM
--scramble_depth    scramble depth [int >= 0]

Example: ./build/bin/PuppetCubeV2 --gui=false --rootPath=./ --errorLevel=extra --threads=1 --runs=10 --positions=1000000 --tablebase_depth=7 --scramble_depth=1
```
