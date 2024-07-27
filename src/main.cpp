#include <thread>

#include "rotation.h"
#include "error_handler.h"
#include "window_manager.h"
#include "settings.h"


int main (int argc, char *argv[]) {
    // create an error handler
    ErrorHandler error_handler(ErrorHandler::Level::kAll);

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

    // test rotations
    actions.Push(Action(Instructions::kRotation, Rotations::kR));
    actions.Push(Action(Instructions::kRotation, Rotations::kL));
    actions.Push(Action(Instructions::kRotation, Rotations::kF));
    actions.Push(Action(Instructions::kReset, Rotations()));

    actions.Push(Action(Instructions::kIsScrambling, Rotations()));
    actions.Push(Action(Instructions::kRotation, Rotations::kR));
    actions.Push(Action(Instructions::kRotation, Rotations::kL));
    actions.Push(Action(Instructions::kRotation, Rotations::kF));

    actions.Push(Action(Instructions::kIsSolving, Rotations()));
    actions.Push(Action(Instructions::kRotation, Rotations::kFc));
    actions.Push(Action(Instructions::kRotation, Rotations::kLc));
    actions.Push(Action(Instructions::kRotation, Rotations::kRc));


    // wait until the window manager has finished
    window_manager.join();
    return 0;
}
