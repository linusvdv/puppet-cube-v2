# puppet-cube-v2

<!--toc:start-->
- [puppet-cube-v2](#puppet-cube-v2)
  - [Pre compilation of position_data](#pre-compilation-of-positiondata)
  - [Compilation](#compilation)
  - [Run](#run)
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

Compilation without GUI

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DGUI=OFF
cmake --build build -j
```

## Run

```bash
./build/bin/PuppetCubeV2
```
