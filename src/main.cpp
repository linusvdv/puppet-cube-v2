#include "renderer.h"
#include "settings.h"


int main (int argc, char *argv[]) {
    Setting settings(argc, argv);

    if (settings.gui) {
        Renderer(settings);
    }

    return 0;
}
