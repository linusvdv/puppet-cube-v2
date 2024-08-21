#include <random>
#include <string>
#include <thread>

#include "actions.h"
#include "cube.h"
#include "error_handler.h"
#include "window_manager.h"
#include "settings.h"
#include "rotation.h"
#include "search.h"


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
        for (int run = 0; run < 20; run++) {
            if (actions.stop) {
                break;
            }

            // scramble
            actions.Push(Action(Instructions::kIsScrambling, Rotations()));
            RandomRotations(error_handler, cube, actions, depth, rng);
            actions.Push(Action(Instructions::kIsSolving, Rotations()));

            error_handler.Handle(ErrorHandler::Level::kAll, "main.cpp",  "Starting search with scrambled position of depth " + std::to_string(depth));
            for (int search_depth = 0; search_depth < 20; search_depth++) {
                if (actions.stop) {
                    break;
                }

                error_handler.Handle(ErrorHandler::Level::kAll, "main.cpp",  "depth " + std::to_string(search_depth));
                if (Search(error_handler, actions, cube, search_depth)) {
                    error_handler.Handle(ErrorHandler::Level::kAll, "main.cpp",  "Found solution of depth " + std::to_string(search_depth));

                    // show solution
                    while (!actions.sove.empty()) {
                        actions.Push(Action(Instructions::kRotation, actions.sove.top()));
                        actions.sove.pop();
                    }
                    break;
                }
            }

            actions.Push(Action(Instructions::kReset, Rotations()));
            cube = Cube();
        }
    }


    // wait until the window manager has finished
    if (settings.gui) {
        window_manager.join();
    }
    return 0;
}
