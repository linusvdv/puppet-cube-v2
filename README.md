# puppet-cube-v2

## Pre compilation of legal-move-generation
time: ca. 20 sec.
```bash
cd position_data/
g++ -Wall -Wextra -g3 -std=c++20 -O3 corner-data.cpp -o corner-data
./corner-data
cd ../
```

## Compilation:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Run:
```bash
./build/bin/PuppetCubeV2
```
