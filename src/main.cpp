#include "cube.h"
#include "logger.h"
#include "settings.h"


int main (int argc, char *argv[]) {
    LOG_INFO("Puppet Cube V2 by Linus VandeVondele");
    LOG_MEMORY();

    // settings initialization
    Settings(argc, argv);

    Cube::Initialize();
    LOG_INFO("Cube Initialized");

    Cube::TablebaseInitialization();
    LOG_INFO("Tablebase Initialized");

    return 0;
}
