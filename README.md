# puppet-cube-v2

## Pre compilation of legal-move-generation
time: ca. 20 sec.
```bash
cd legal-move-generation/
g++ -Wall -Wextra -g3 -std=c++20 -O3 main.cpp -o legal-move-generation
./legal-move-generation
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
