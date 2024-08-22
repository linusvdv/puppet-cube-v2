#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "actions.h"
#include "cube.h"
#include "error_handler.h"
#include "window_manager.h"
#include "settings.h"
#include "rotation.h"
#include "search.h"


std::string PrecisionStringDouble (double number) {
    std::stringstream stream;
    stream << std::fixed << std::setprecision(2) << number;
    return stream.str();
}


void ShowSearchStatistic (ErrorHandler error_handler, int depth, int num_runs, std::vector<double>& time_durations, std::vector<int>& search_depths, std::vector<uint64_t>& all_num_positions) {
    if (num_runs <= 0) {
        return;
    }
    if (time_durations.empty() || search_depths.empty()) {
        return;
    }
    std::string statistic;
    statistic += "\n\t";
    statistic += "Statistic of depth: " + std::to_string(depth);
    statistic += "\n\t";
    statistic += "number of position: " + std::to_string(num_runs);
    statistic += "\n\t";
    statistic += "total time: " + PrecisionStringDouble(std::reduce(time_durations.begin(), time_durations.end())) + "s";
    statistic += "\n\t";
    statistic += "max time: " + PrecisionStringDouble(*std::max_element(time_durations.begin(), time_durations.end())) + "s";
    statistic += "\n\t";
    statistic += "avarage time: " + PrecisionStringDouble(std::reduce(time_durations.begin(), time_durations.end()) / num_runs) + "s";
    statistic += "\n\t";
    statistic += "min time: " + PrecisionStringDouble(*std::min_element(time_durations.begin(), time_durations.end())) + "s";
    statistic += "\n\t";
    statistic += "max depth: " + std::to_string(*std::max_element(search_depths.begin(), search_depths.end()));
    statistic += "\n\t";
    statistic += "avarage depth: " + PrecisionStringDouble(float(std::reduce(search_depths.begin(), search_depths.end())) / num_runs);
    statistic += "\n\t";
    statistic += "min depth: " + std::to_string(*std::min_element(search_depths.begin(), search_depths.end()));
    statistic += "\n\t";
    statistic += "max positions: " + std::to_string(*std::max_element(all_num_positions.begin(), all_num_positions.end()));
    statistic += "\n\t";
    statistic += "avarage positions: " + PrecisionStringDouble(float(std::reduce(all_num_positions.begin(), all_num_positions.end())) / num_runs);
    statistic += "\n\t";
    statistic += "min positions: " + std::to_string(*std::min_element(all_num_positions.begin(), all_num_positions.end()));
    statistic += "\n\t";
    statistic += "average nodes per second: " + PrecisionStringDouble(double(std::reduce(all_num_positions.begin(), all_num_positions.end()) / std::reduce(time_durations.begin(), time_durations.end())));
    error_handler.Handle(ErrorHandler::Level::kInfo, "main.cpp", statistic);
}


int main (int argc, char *argv[]) {
    // create an error handler
    ErrorHandler error_handler(ErrorHandler::Level::kInfo);

    // settings
    // it is not thread safe only a copy is sent to the window manager
    Setting settings(error_handler, argc, argv);

    // load legal moves from file
    InitializePositionData(error_handler, settings);

    // actions for communication with window manager
    Actions actions;
    std::thread window_manager;

    // check if user wants a graphical interface
    if (settings.gui) {
        // create a graphical interface running on another thread
        window_manager = std::thread(WindowManager, error_handler, settings, std::ref(actions));
    }


    Cube cube;

    // random number initialisation
    std::random_device device;
    std::mt19937 rng(device());
    // get reproducible random numbers
    rng.seed(0);

    for (int depth = 0; depth < 20; depth++) {
        if (actions.stop) {
            break;
        }

        // get some informations
        std::vector<double> time_durations;
        std::vector<int> search_depths;
        std::vector<uint64_t> all_num_positions;

        const int k_num_runs = 20;
        for (int run = 0; run < k_num_runs; run++) {
            if (actions.stop) {
                break;
            }
            // get duration time
            auto start_time = std::chrono::system_clock::now();

            // scramble
            actions.Push(Action(Instructions::kIsScrambling, Rotations()));
            RandomRotations(cube, actions, depth, rng);
            actions.Push(Action(Instructions::kIsSolving, Rotations()));

            // solve
            error_handler.Handle(ErrorHandler::Level::kAll, "main.cpp",  "Starting search with scrambled position of depth " + std::to_string(depth));
            for (int search_depth = 0; search_depth <= depth; search_depth++) {
                if (actions.stop) {
                    break;
                }

                error_handler.Handle(ErrorHandler::Level::kAll, "main.cpp",  "depth " + std::to_string(search_depth));
                uint64_t num_positions = 0;
                if (Search(error_handler, actions, cube, search_depth, num_positions)) {
                    error_handler.Handle(ErrorHandler::Level::kAll, "main.cpp",  "Found solution of depth " + std::to_string(search_depth) + " visiting " + std::to_string(num_positions) + " positions");

                    // statistic
                    search_depths.push_back(search_depth);
                    all_num_positions.push_back(num_positions);

                    // show solution
                    while (!actions.sove.empty()) {
                        actions.Push(Action(Instructions::kRotation, actions.sove.top()));
                        actions.sove.pop();
                    }
                    break;
                }
            }

            auto end_time = std::chrono::system_clock::now();
            std::chrono::duration<double> time_duration = end_time - start_time;
            time_durations.push_back(time_duration.count());

            actions.Push(Action(Instructions::kReset, Rotations()));
            cube = Cube();
        }

        // calculate some statistic
        ShowSearchStatistic(error_handler, depth, search_depths.size(), time_durations, search_depths, all_num_positions);
    }


    // wait until the window manager has finished
    if (settings.gui) {
        window_manager.join();
    }
    return 0;
}
