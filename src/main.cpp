#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <ios>
#include <numeric>
#include <ostream>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <parallel_hashmap/phmap.h>

#include "actions.h"
#include "cube.h"
#include "error_handler.h"
#include "window_manager.h"
#include "settings.h"
#include "rotation.h"
#include "search.h"


std::string PrecisionDouble (double number) {
    std::stringstream stream;
    stream << std::fixed << std::setprecision(2) << number;
    return stream.str();
}


void ShowSearchStatistic (ErrorHandler error_handler, int depth, size_t num_runs, std::vector<double>& time_durations, std::vector<int>& search_depths, std::vector<uint64_t>& all_num_positions) {
    if (num_runs <= 0) {
        return;
    }

    if (num_runs != time_durations.size() || num_runs != search_depths.size() || num_runs != all_num_positions.size()) {
        error_handler.Handle(ErrorHandler::Level::kWarning, "main.cpp", "Show search statistic recieved vectors of wrong size");
        std::stringstream infos;
        infos << num_runs << " " << time_durations.size() << " " << search_depths.size() << " " << all_num_positions.size() << std::endl;
        error_handler.Handle(ErrorHandler::Level::kAll, "main.cpp", infos.str());
        return;
    }

    std::stringstream statistic;
    const int indent = 8;
    const int table_space = 15;

    int q_1 = num_runs / 4;
    int q_2 = num_runs / 2;
    int q_3 = num_runs * 3 / 4;

    statistic << std::endl;
    statistic << std::setw(indent) << "" << "Statistic of depth: " << depth << std::endl;
    statistic << std::setw(indent) << "" << "number of runs: " << num_runs << std::endl;
    statistic << std::setw(indent) << "" << std::setw(indent) << "" << std::setw(table_space) << "time (s)" << std::setw(table_space) << "positions" << std::setw(table_space) << "depths" << std::endl;

    statistic << std::setw(indent) << "" << std::left << std::setw(indent) << "total" << std::right
        << std::setw(table_space) << PrecisionDouble(std::reduce(time_durations.begin(), time_durations.end()))
        << std::setw(table_space) << std::reduce(all_num_positions.begin(), all_num_positions.end())
        << std::setw(table_space) << "-" << std::endl;

    statistic << std::setw(indent) << "" << std::left << std::setw(indent) << "max" << std::right
        << std::setw(table_space) << PrecisionDouble(*std::max_element(time_durations.begin(), time_durations.end()))
        << std::setw(table_space) << *std::max_element(all_num_positions.begin(), all_num_positions.end())
        << std::setw(table_space) << *std::max_element(search_depths.begin(), search_depths.end()) << std::endl;

    std::nth_element(time_durations.begin(), time_durations.begin() + q_3, time_durations.end());
    std::nth_element(all_num_positions.begin(), all_num_positions.begin() + q_3, all_num_positions.end());
    std::nth_element(search_depths.begin(), search_depths.begin() + q_3, search_depths.end());
    statistic << std::setw(indent) << "" << std::left << std::setw(indent) << "Q3" << std::right
        << std::setw(table_space) << PrecisionDouble(time_durations[q_3])
        << std::setw(table_space) << all_num_positions[q_3]
        << std::setw(table_space) << search_depths[q_3] << std::endl;

    std::nth_element(time_durations.begin(), time_durations.begin() + q_2, time_durations.end());
    std::nth_element(all_num_positions.begin(), all_num_positions.begin() + q_2, all_num_positions.end());
    std::nth_element(search_depths.begin(), search_depths.begin() + q_2, search_depths.end());
    statistic << std::setw(indent) << "" << std::left << std::setw(indent) << "median" << std::right
        << std::setw(table_space) << PrecisionDouble(time_durations[q_2])
        << std::setw(table_space) << all_num_positions[q_2]
        << std::setw(table_space) << search_depths[q_2] << std::endl;

    std::nth_element(time_durations.begin(), time_durations.begin() + q_1, time_durations.end());
    std::nth_element(all_num_positions.begin(), all_num_positions.begin() + q_1, all_num_positions.end());
    std::nth_element(search_depths.begin(), search_depths.begin() + q_1, search_depths.end());
    statistic << std::setw(indent) << "" << std::left << std::setw(indent) << "Q1" << std::right
        << std::setw(table_space) << PrecisionDouble(time_durations[q_1])
        << std::setw(table_space) << all_num_positions[q_1]
        << std::setw(table_space) << search_depths[q_1] << std::endl;

    statistic << std::setw(indent) << "" << std::left << std::setw(indent) << "min" << std::right
        << std::setw(table_space) << PrecisionDouble(*std::min_element(time_durations.begin(), time_durations.end()))
        << std::setw(table_space) << *std::min_element(all_num_positions.begin(), all_num_positions.end())
        << std::setw(table_space) << *std::min_element(search_depths.begin(), search_depths.end());

    error_handler.Handle(ErrorHandler::Level::kInfo, "main.cpp", statistic.str());
}


int main (int argc, char *argv[]) {
    // create an error handler
    ErrorHandler error_handler(ErrorHandler::Level::kInfo);

    // settings
    // it is not thread safe only a copy is sent to the window manager
    Setting settings(error_handler, argc, argv);

    // load legal moves from file
    InitializePositionData(error_handler, settings);
    // calculate all start positions
    TablebaseInitialisation(error_handler, 5);

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

    for (int depth = 0; depth <= 11; depth++) {
        if (actions.stop) {
            break;
        }

        // get some informations
        std::vector<double> time_durations;
        std::vector<int> search_depths;
        std::vector<uint64_t> all_num_positions;

        const int k_num_runs = 100;
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
            CubeHashMap visited;
            for (int search_depth = 0; search_depth <= depth; search_depth++) {
                if (actions.stop) {
                    break;
                }

                error_handler.Handle(ErrorHandler::Level::kAll, "main.cpp",  "depth " + std::to_string(search_depth));
                uint64_t num_positions = 0;
                if (Search(error_handler, actions, cube, search_depth, num_positions, visited)) {
                    error_handler.Handle(ErrorHandler::Level::kAll, "main.cpp",  "Found solution of depth " + std::to_string(search_depth) + " visiting " + std::to_string(num_positions) + " positions");

                    // statistic
                    search_depths.push_back(search_depth);
                    all_num_positions.push_back(num_positions);

                    // show solution
                    while (!actions.solve.empty()) {
                        actions.Push(Action(Instructions::kRotation, actions.solve.top()));
                        actions.solve.pop();
                    }
                    break;
                }
            }

            auto end_time = std::chrono::system_clock::now();
            std::chrono::duration<double> time_duration = end_time - start_time;
            if (!actions.stop) {
                time_durations.push_back(time_duration.count());
            }

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
