#!/usr/bin/env bash

heuristic_factors=(1.6 1.65)
for cur in "${heuristic_factors[@]}"; do
    OUTFILE="out/out_heuristic_${cur}_r30"
    ./../build/bin/PuppetCubeV2 -p100000000000 -t4 -r30 --heuristic_factor="$cur" > "$OUTFILE"
done
