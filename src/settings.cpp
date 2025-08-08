#include <string>
#include <thread>
#include <vector>

#include "settings.hpp"


std::string Settings::root_path;
int Settings::num_threads = 1;


Settings::Settings (int argc, char *argv[]) {
    num_threads = std::thread::hardware_concurrency();

    std::vector<std::string> arguments(argv, argv+argc);

    // get root path
    std::string temp_root_path = arguments[0];
    std::size_t executable_place = temp_root_path.find_last_of("/\\");
    if (executable_place != std::string::npos) {
        temp_root_path = temp_root_path.substr(0, executable_place);
    }
    else {
        temp_root_path = ".";
    }
    temp_root_path.append("/../../");
    root_path.append(temp_root_path);
}
