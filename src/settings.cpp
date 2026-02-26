#include <cstddef>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>
#include <getopt.h>

#include "settings.hpp"
#include "logger.hpp"

#ifdef USE_CUDA
#include "info_bridge.hpp"
#endif  // USE_CUDA


// default values for the settings
// general informations
std::string Settings::root_path;                // automatic detection
#ifdef USE_CUDA
bool Settings::use_cuda = true;
#else
bool Settings::use_cuda = false;
#endif
int Settings::device_count;                     // automatic detection
bool Settings::hardware_info = false;

// search starting position
size_t Settings::num_runs = 10;                 // NOLINT
int Settings::scrambling_depth = 100;           // NOLINT
int Settings::min_corner_heuristic = 0;

// search
int Settings::num_threads;                      // automatic detection
int Settings::num_gpu_upload_threads;           // automatic detection
int Settings::num_gputhreads;                   // automatic detection
int Settings::num_positions_per_batch = 1000;   // NOLINT
int Settings::num_parallel_batches;             // automatic detection

// tablebase
int Settings::tb_depth = 6;                     // NOLINT


static struct option long_options[] = {
    {"help", no_argument, NULL, 'h'},

    // general informations
    {"root_path", required_argument, NULL, 0},
    {"use_cuda", required_argument, NULL, 0},
    {"device_count", required_argument, NULL, 'd'},
    {"info", no_argument, NULL, 'i'},
    {"log_level", required_argument, NULL, 'l'},

    // search starting postion
    {"num_runs", required_argument, NULL, 'r'},
    {"scrambling_depth", required_argument, NULL, 's'},
    {"min_corner_heuristic", required_argument, NULL, 'm'},

    // search
    {"threads", required_argument, NULL, 't'},

    // tablebase
    {"tb_depth", required_argument, NULL, 0},

    {NULL, 0, NULL, 0}
};


std::string help_msg = R"(
usage: ./build/bin/PuppetCubeV2 [options]
    --option=value
    --option value
    -ovalue
    -o value

list of options
    -h --help                  show this message

    --root_path                path to root folder puppet-cube-v2                         [./PathToPuppetCubeV2/../../]
    --use_cuda                 run cuda                                                   [USE_CUDA]     (true|1|false|0)
    -d --device_count          number of gpu                                              [NUM_GPUS]     (1, NUM_GPUS)
    -i --info                  show additional hardware info
    -l --log_level             logger/error level                                         [memory]       (critical|error|warning|info|all|extra|memory)

    -r --num_runs              number of runs                                             [10]           (0, 1e18)
    -s --scrambling_depth      how many moves to scramble                                 [100]          (0, 1000000)
    -m --min_corner_heuristic  all starting position have at least this corner heuristic  [0]            (0, 27)

    -t --threads               number of threads used in the program                      [MAX_THREADS]  (1, MAX_THREADS)

    --tb_depth                 depth of the tablebase (9 uses 40 GB RAM)                  [6]            (0, 9)
)";


bool GetBoolFromOptarg (bool& num, const std::string& option) {
    try {
        if (optarg == NULL) {
            LOG_WARNING(option, "No argument passed to the option");
            return false;
        }
        if (std::string(optarg) == "true" || std::string(optarg) == "1") {
            num = true;
            return true;
        }
        if (std::string(optarg) == "false" || std::string(optarg) == "0") {
            num = false;
            return true;
        }
        LOG_WARNING(option, "invalid_argument", optarg);
        return false;
    }
    catch (const std::invalid_argument& e) {
        LOG_ERROR(option, "invalid argument", e.what());
        return false;
    }
}


template<typename T>
bool GetTFromOptarg (T& num, T low, T upper, const std::string& option) {
    try {
        if (optarg == NULL) {
            LOG_WARNING(option, "No argument passed to the option");
            return false;
        }
        T new_num;
        if constexpr (std::is_same_v<T, int>) {
            new_num = std::stoi(optarg);
        }
        else if constexpr (std::is_same_v<T, size_t>) {
            new_num = std::stoull(optarg);
        }
        else if constexpr (std::is_same_v<T, float>) {
            new_num = std::stof(optarg);
        }
        else {
            static_assert(std::false_type::value, "unsupported type");
        }
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

void Settings::SetDefault (std::vector<std::string>& arguments) {
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

    // device_count
    #ifdef USE_CUDA
    device_count = GetCUDADeviceCount();
    #elif
    device_count = 0;
    #endif  // USE_CUDA

    num_threads = std::thread::hardware_concurrency();
    num_gpu_upload_threads = 6 * device_count;
    // TEST: to get better time

    #ifdef USE_CUDA
    // only the first device gets checked
    num_gputhreads = (GetThreadsPerDevice(0) / 4 / kBlockDim) * kBlockDim;
    #elif
    num_gputhreads = 0;
    #endif  // USE_CUDA
    LOG_EXTRA("num_gputhreads:", num_gputhreads);

    num_parallel_batches = num_threads * 2;
}


Settings::Settings (int argc, char *argv[]) {
    std::vector<std::string> arguments(argv, argv+argc);
    SetDefault(arguments);

    const char* short_options = "hd:il:r:s:m:t:";
    opterr = 0; // supress error messages from getopt_long
    int option_index;
    signed char cop;

    while ((cop = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
        switch (cop) {
            case 'h':
                LOG_ALL(help_msg);
                exit(0);

            case 'd':
                GetTFromOptarg(device_count, 1, device_count, "DEVICE COUNT");
                break;
            case 'i':
                hardware_info = true;
                break;
            case 'l': {
                std::string log_level = std::string(optarg);
                if (log_level == "critical") {
                    Logger::SetLoggerLevel(LoggerLevel::kCriticalError);
                }
                else if (log_level == "error") {
                    Logger::SetLoggerLevel(LoggerLevel::kError);
                }
                else if (log_level == "warning") {
                    Logger::SetLoggerLevel(LoggerLevel::kWarning);
                }
                else if (log_level == "info") {
                    Logger::SetLoggerLevel(LoggerLevel::kInfo);
                }
                else if (log_level == "all") {
                    Logger::SetLoggerLevel(LoggerLevel::kAll);
                }
                else if (log_level == "extra") {
                    Logger::SetLoggerLevel(LoggerLevel::kExtra);
                }
                else if (log_level == "memory") {
                    Logger::SetLoggerLevel(LoggerLevel::kMemory);
                }
                else {
                    LOG_WARNING("Not recognized log level:", log_level);
                }
                break;
                }
            case 'r':
                GetTFromOptarg(num_runs, size_t(0), size_t(1e18), "NUM RUNS");  // NOLINT
                break;
            case 's':
                GetTFromOptarg(scrambling_depth, 0, 1000000, "SCRAMBLING DEPTH"); // NOLINT
                break;
            case 'm':
                GetTFromOptarg(min_corner_heuristic, 0, 27, "MIN CORNER HEURISTIC");
                break;
            case 't':
                GetTFromOptarg(num_threads, 1, num_threads, "THREADS");
                break;
            case 0:
                if (std::string(long_options[option_index].name) == "root_path") {
                    root_path = std::string(optarg);
                }
                if (std::string(long_options[option_index].name) == "use_cuda") {
                    GetBoolFromOptarg(use_cuda, "USE CUDA");
                }

                if (std::string(long_options[option_index].name) == "tb_depth") {
                    GetTFromOptarg(tb_depth, 0, 9, "TB DEPTH"); // NOLINT
                }
                break;
            case '?':
                LOG_WARNING("Unrecognized option:", argv[optind-1]);
                break;
            case ':':
                LOG_WARNING("Missing argument for an option.");
                break;
            default:
                LOG_WARNING("not expected argument");
                break;
        }
    }

    #ifndef USE_CUDA
    if (use_cuda) {
        use_cuda = false;
        LOG_WARNING("Cannot enable USE CUDA - compile with CUDA");
    }
    #endif  // USE_CUDA

    if (use_cuda && device_count <= 0) {
        use_cuda = false;
        LOG_ERROR("No GPU detected!");
        LOG_WARNING("Disabled CUDA search");
    }
}
