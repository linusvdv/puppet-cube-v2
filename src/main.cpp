#include "cube.h"
#include "logger.h"
#include "settings.h"


int main (int argc, char *argv[]) {
    LOG_INFO("Puppet Cube V2 by Linus VandeVondele");
    LOG_MEMORY();

    // settings initialization
    Settings(argc, argv);

    Cube::Initialize();

    return 0;
}
