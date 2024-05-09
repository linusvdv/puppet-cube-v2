#include <cstddef>
#include <string>

#include "settings.h"


Setting::Setting(int argc, char *argv[]) {
    // get root path
    std::string temp_root_path = argv[0];
    std::size_t executable_place = temp_root_path.find_last_of("/\\");
    temp_root_path = temp_root_path.substr(0, executable_place);
    temp_root_path.append("/../../");
    rootPath = temp_root_path;

    for (int i = 1; i < argc; i++) {
        if (argv[i] == "nogui") {
            gui = false;
        }
    }
}
