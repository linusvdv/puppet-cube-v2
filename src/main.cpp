#include <random>
#include <thread>
#include <parallel_hashmap/phmap.h>

#include "actions.h"
#include "cube.h"
#include "error_handler.h"
#include "window_manager.h"
#include "settings.h"
#include "rotation.h"
#include "search.h"
#include "search_manager.h"


int main (int argc, char *argv[]) {
    // create an error handler
    ErrorHandler error_handler(ErrorHandler::Level::kInfo);

    // settings
    // it is not thread safe only a copy is sent to the window manager
    Setting settings(error_handler, argc, argv);

    // actions for communication with window manager
    Actions actions;
    std::thread window_manager;

    // check if user wants a graphical interface
    if (settings.gui) {
        // create a graphical interface running on another thread
        window_manager = std::thread(WindowManager, error_handler, settings, std::ref(actions));
    }

    // random number initialisation
    std::random_device device;
    std::mt19937 rng(device());
    // get reproducible random numbers
    rng.seed(0);

    // load legal moves from file
    InitializePositionData(error_handler, settings);
    InitializeEdgeData(error_handler, settings);

    // start the search manager
    SearchManager(error_handler, actions, rng);

    // wait until the window manager has finished
    if (settings.gui) {
        window_manager.join();
    }
    return 0;
}
