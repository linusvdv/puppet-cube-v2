#include <random>
#include <thread>

#include "actions.h"
#include "error_handler.h"
#include "window_manager.h"
#include "settings.h"
#include "rotation.h"


int main (int argc, char *argv[]) {
    // create an error handler
    ErrorHandler error_handler(ErrorHandler::Level::kInfo);

    // settings
    // it is not thread safe only a copy is sent to the window manager
    Setting settings(error_handler, argc, argv);

    // load legal moves from file
    InitializeLegalMoves(error_handler, settings);

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

    // random scramble
    actions.Push(Action(Instructions::kIsScrambling, Rotations()));
    RandomRotations(error_handler, cube, actions, 100, rng);
    actions.Push(Action(Instructions::kIsSolving, Rotations()));


    // wait until the window manager has finished
    if (settings.gui) {
        window_manager.join();
    }
    return 0;
}
