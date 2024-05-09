#include "error_handler.h"
#include "renderer.h"
#include "settings.h"


int main (int argc, char *argv[]) {
    ErrorHandler error_handler(ErrorHandler::Level::kAll);

    Setting settings(error_handler, argc, argv);

    if (settings.gui) {
        Renderer(error_handler, settings);
    }

    return 0;
}
