#pragma once

#include <string>
#include <utility>


#include "error_handler.h"


class Setting {
public:
    Setting(ErrorHandler& error_handler, int argc, char *argv[]);

    // path to files
    // this path should be equivelant to path/to/puppet-cube-v2/
    std::string rootPath;

    // graphical user interface
    bool gui = false;

    // mouse rotation
    std::pair<float, float> rotation = {-40, 30};
    std::pair<double, double> last_position = {0, 0};

    // graphical
    double soom = 1;
    float pieceOffset = 0.001;

    bool should_rotate = true;
    float min_elapsed_time_since_last_rotation = 0.2; // seconds
    float rotation_speed = 2;
    float scrambling_multiplier = 400;

    // search
    int num_threads = 0;
    int tablebase_depth = 5;
    uint64_t max_num_positions = 10000000;
    int min_depth = 0;

    // scramble
    int num_runs = 1000;
    int scramble_depth = 1000;
    int start_offset = 0;
    int min_coner_heuristic = 0;

    static constexpr int kIndent = 8;
};
