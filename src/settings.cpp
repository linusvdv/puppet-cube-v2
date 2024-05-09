#include <cstddef>
#include <string>
#include <vector>

#include "settings.h"


Setting::Setting(int argc, char *argv[]) {
    std::vector<std::string> arguments(argv, argv+argc);

    // get root path
    std::string temp_root_path = arguments[0];
    std::size_t executable_place = temp_root_path.find_last_of("/\\");
    temp_root_path = temp_root_path.substr(0, executable_place);
    temp_root_path.append("/../../");
    rootPath = temp_root_path;

    for (std::string argument : arguments) {
        // gui
        if (argument == "nogui") {
            gui = false;
        }
        // root path
        else if (argument.find("rootPath=") == 0) {
            rootPath = argument.erase(0, std::string("rootPath=").size());
        }
    }
}
