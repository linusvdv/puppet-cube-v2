#include "logger.h"
#include "settings.h"


int main (int argc, char *argv[]) {
    LOG_ALL("Puppet Cube V2 by Linus VandeVondele");
    Settings(argc, argv);
    return 0;
}
