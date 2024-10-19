#include <random>
#include <string>
#include <thread>
#include <parallel_hashmap/phmap.h>
#include <nadeau.h>

#include "actions.h"
#include "cube.h"
#include "error_handler.h"
#include "settings.h"
#include "rotation.h"
#include "search_manager.h"

#ifdef GUI
#include "window_manager.h"
#endif


int main (int argc, char *argv[]) {
    // create an error handler
    ErrorHandler error_handler(ErrorHandler::Level::kMemory);

    // settings
    // it is not thread safe only a copy is sent to the window manager
    Setting settings(error_handler, argc, argv);

    // actions for communication with window manager
    Actions actions;
    std::thread window_manager;

    // check if user wants a graphical interface
    if (settings.gui) {
        #ifdef GUI
        // create a graphical interface running on another thread
        window_manager = std::thread(WindowManager, error_handler, settings, std::ref(actions));
        #else
        error_handler.Handle(ErrorHandler::Level::kError, "main.cpp", "disable cmake -DGUI=OFF or run with --gui=false");
        #endif
    }

    // random number initialisation
    std::random_device device;
    std::mt19937 rng(device());
    // get reproducible random numbers
    rng.seed(0);

    // load legal moves from file
    InitializePositionData(error_handler, settings);
    InitializeEdgeData(error_handler, settings);

    error_handler.Handle(ErrorHandler::Level::kMemory, "main.cpp", "currently using " + std::to_string(getCurrentRSS()/1000000) + " MB"); // NOLINT
    // start the search manager
    SearchManager(error_handler, settings, actions, rng);

    // wait until the window manager has finished
    if (settings.gui) {
        window_manager.join();
    }
    return 0;
}
