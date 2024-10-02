# puppet-cube-v2

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
cmake --build build
```

## Run

```bash
./build/bin/PuppetCubeV2
```
