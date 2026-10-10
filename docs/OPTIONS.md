# Command line options

Options can be given in any of these forms:

```
--option=value
--option value
-ovalue
-o value
```

## Full help output

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
    --run_offset               start at a specific run number                             [0]           (0, 1e18)
    -s --scrambling_depth      how many moves to scramble                                 [100]          (0, 1000000)
    -m --min_corner_heuristic  all starting position have at least this corner heuristic  [0]           (0, 27)

    -t --threads               number of threads used in the program                      [MAX_THREADS]  (1, MAX_THREADS)
    --tt_size                  size of the transposition table in MB                      [1000]         (128, 1000000)

    --tb_depth                 depth of the tablebase (9 uses 40 GB RAM)                  [6]           (0, 9)
```

## Notes

- **`--root_path`** defaults to two folders above the binary, so run `./build/bin/PuppetCubeV2` from the repository root, or pass the absolute path of the repository.
- **`--tb_depth`**: the help text says 6, but the actual default is 7.
- **`--tt_size`**: two transposition tables are allocated, so the memory used is twice this value (2 GB at the default).
- **`--run_offset`**: scrambles are seeded by the run number, so the same offset gives the same scrambles.
- **`-m`**: use 27 to get the hardest start positions.
- **`-l`**: the levels are ordered from least to most output (`critical`, `error`, `warning`, `info`, `all`, `extra`, `memory`). The default `memory` is the most verbose. `extra` prints everything except the memory lines.
- **`--use_cuda=false`** runs the CPU search, which is about 13x slower than the GPU search on the author's hardware (RTX 4070, 24 cores).
