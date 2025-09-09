#include <string>
#include <thread>
#include <vector>
#include <getopt.h>

#include "settings.hpp"
#include "logger.hpp"


std::string Settings::root_path;
bool Settings::test_bcht = false;
bool Settings::test_bfs = false;
int Settings::bfs_depth = 4;
int Settings::scrambling_depth = 1000;  // NOLINT
int Settings::num_threads = 1;
int Settings::tb_depth = 6;  // NOLINT
int Settings::tb_depth_gpu = 8;  // NOLINT


static struct option long_options[] = {
    {"help", no_argument, NULL, 'h'},
    {"BCHT", no_argument, NULL, 'B'},
    {"threads", required_argument, NULL, 't'},
    {"tb_depth", required_argument, NULL, 0},
    {"tb_depth_gpu", required_argument, NULL, 0},
    {"BFS", no_argument, NULL, 'b'},
    {"BFS_depth", required_argument, NULL, 0},
    {"root_path", required_argument, NULL, 0},
    {"scrambling_depth", required_argument, NULL, 0},
    {NULL, 0, NULL, 0}
};


bool GetIntFromOptarg (int& num, int low, int upper, const std::string& option) {
    try {
        if (optarg == NULL) {
            LOG_WARNING(option, "No argument passed to the option");
            return false;
        }
        int new_num = std::stoi(optarg);
        if (new_num > upper) {
            LOG_WARNING(option, "Value out of expected range got", new_num, "max", upper);
            return false;
        }
        if (new_num < low) {
            LOG_WARNING(option, "Value out of expected range got", new_num, "min", low);
            return false;
        }
        num = new_num;
        return true;
    }
    catch (const std::invalid_argument& e) {
        LOG_ERROR(option, "invalid argument", e.what());
        return false;
    }
    catch (const std::out_of_range& e) {
        LOG_ERROR(option, "out of range", e.what());
        return false;
    }
}


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

    const char* short_options = "hBt:";
    opterr = 0; // supress error messages from getopt_long
    int option_index;
    char cop;

    while ((cop = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
        switch (cop) {
            case 'h':
                // --help
                exit(0);
            case 'B':
                test_bcht = true;
                break;
            case 't':
                GetIntFromOptarg(num_threads, 1, num_threads, "THREADS");
                break;
            case 'b':
                test_bfs = true;
                break;
            case 0:
                if (std::string(long_options[option_index].name) == "tb_depth") {
                    GetIntFromOptarg(tb_depth, 1, 9, "TB DEPTH");
                }
                if (std::string(long_options[option_index].name) == "tb_depth_gpu") {
                    GetIntFromOptarg(tb_depth_gpu, 1, 9, "TB DEPTH GPU");
                }
                if (std::string(long_options[option_index].name) == "BFS_depth") {
                    GetIntFromOptarg(bfs_depth, 1, 6, "BFS DEPTH");
                }
                if (std::string(long_options[option_index].name) == "root_path") {
                    root_path = std::string(optarg);
                }
                if (std::string(long_options[option_index].name) == "scrambling_depth") {
                    GetIntFromOptarg(scrambling_depth, 1, 1000000, "SCRAMBLING DEPTH");
                }
                break;
            case '?':
                LOG_WARNING("Unrecognized option");
                break;
            case ':':
                LOG_WARNING("Missing argument for an option.");
                break;
            default:
                LOG_WARNING("not expected argument");
                break;
        }
    }

    // tb_depth
    tb_depth_gpu = std::min(tb_depth, tb_depth_gpu);
}
