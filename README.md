# puppet-cube-v2

## Pre compilation of legal-move-generation
time: ca. 1 min.
```bash
cd legal-move-generation/
g++ -Wall -Wextra -fdiagnostics-color=always -Wno-sign-compare -std=c++20 -O3 -static main.cpp -o legal-move-generation
./legal-move-generation
cd ../
```

## Compilation:
```bash
cmake -B build
cmake --build build --config Release
```

## Run:
```bash
./build/bin/PuppetCubeV2
```
