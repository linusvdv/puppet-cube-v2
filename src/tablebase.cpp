#include <chrono>
#include <cstddef>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "BCHTSet.hpp"
#include "cube.hpp"
#include "logger.hpp"
#include "settings.hpp"
#include "tablebase.hpp"

std::vector<std::vector<State>> Tablebase::tablebase = {};


void TablebaseSearch (const std::vector<State>& previous, const std::vector<State>& current, TablebasePrecomputation& next, int thread_idx, int num_threads) {
    int count = 0;
    for (const State& position : current) {
        count++;
        if (count%num_threads != thread_idx) {
            continue;
        }
        if (position == State()) {
            continue;
        }
        for (uint8_t rotation = 0; rotation < kNumRotations; rotation++) {
            std::pair<bool, State> next_position = Cube::Rotate(position, rotation);
            if (!next_position.first) {
                continue;
            }

            if (BCHTSetContains(previous, next_position.second) || BCHTSetContains(current, next_position.second)) {
                continue;
            }
            next.insert(next_position.second);
        }
    }
}


void PhmapTiming(const TablebasePrecomputation& tablebase_precomputation, const std::vector<State>& random_positions, size_t thread_idx, size_t num_threads) {
    size_t hit = 0;
    size_t miss = 0;
    for (size_t i = thread_idx; i < random_positions.size(); i+=num_threads) {
        if (tablebase_precomputation.contains(random_positions[i])) {
            hit++;
        }
        else {
            miss++;
        }
    }
    LOG_EXTRA("thread", thread_idx, "hits:", hit, "miss:", miss);
}


void BCHTTiming(const std::vector<State>& tablebase_layer, const std::vector<State>& random_positions, size_t thread_idx, size_t num_threads) {
    size_t hit = 0;
    size_t miss = 0;
    size_t rp_size = random_positions.size();
    for (size_t i = thread_idx; i < rp_size; i+=num_threads) {
        if (BCHTSetContains(tablebase_layer, random_positions[i])) {
            hit++;
        }
        else {
            miss++;
        }
    }
    LOG_EXTRA("thread", thread_idx, "hits:", hit, "miss:", miss);
}


std::vector<State> TimeTablebaseCPU(std::vector<std::vector<State>>& tablebase, TablebasePrecomputation& tablebase_precomputation) {
    // only time largest tb_depth

    LOG_EXTRA("Create test data for timing tablebase CPU");
    std::vector<State> random_positions;
    for (const State& state : tablebase[Settings::GetTBDepth()]) {
        if (state == State()) {
            continue;
        }
        random_positions.push_back(state);
    }
    for (const State& state : tablebase[Settings::GetTBDepth()-1]) {
        if (state == State()) {
            continue;
        }
        random_positions.push_back(state);
    }

    for (size_t i = 0; i < random_positions.size(); i++) {
        std::swap(random_positions[i], random_positions[rand()%random_positions.size()]);
    }

    // Time phmap
    LOG_EXTRA("Start timing of phmap");
    std::chrono::time_point phmap_time = std::chrono::high_resolution_clock::now(); // get the current time

    PhmapTiming(tablebase_precomputation, random_positions, 0, 1);

    std::chrono::time_point phmap_since_epoch = std::chrono::high_resolution_clock::now(); // get the duration since epoch
    std::chrono::milliseconds phmap_millis = std::chrono::duration_cast<std::chrono::milliseconds>(phmap_since_epoch - phmap_time);
    LOG_ALL("Time duration for phmap:", phmap_millis.count());

    // Time phmap multithreads
    LOG_EXTRA("Start timing of phmap multithreads");
    std::chrono::time_point phmap_time_multi = std::chrono::high_resolution_clock::now(); // get the current time

    {
        std::vector<std::jthread> threads;
        for (int j = 0; j < Settings::GetNumThreads(); j++) {
            threads.push_back(std::jthread(PhmapTiming, std::ref(tablebase_precomputation), std::ref(random_positions), j, Settings::GetNumThreads()));
        }
    }

    std::chrono::time_point phmap_since_epoch_multi = std::chrono::high_resolution_clock::now(); // get the duration since epoch
    std::chrono::milliseconds phmap_millis_multi = std::chrono::duration_cast<std::chrono::milliseconds>(phmap_since_epoch_multi - phmap_time_multi);
    LOG_ALL("Time duration for phmap multithreads:", phmap_millis_multi.count());

    // Time BCHT
    LOG_EXTRA("Start timing of BCHT");
    std::chrono::time_point bcht_time = std::chrono::high_resolution_clock::now(); // get the current time

    BCHTTiming(tablebase[Settings::GetTBDepth()], random_positions, 0, 1);

    std::chrono::time_point bcht_since_epoch = std::chrono::high_resolution_clock::now(); // get the current time
    std::chrono::milliseconds bcht_millis = std::chrono::duration_cast<std::chrono::milliseconds>(bcht_since_epoch - bcht_time);
    LOG_ALL("Time duration for BCHT:", bcht_millis.count());

    // Time BCHT multithreads
    LOG_EXTRA("Start timing of BCHT multithreads");
    std::chrono::time_point bcht_time_multi = std::chrono::high_resolution_clock::now(); // get the current time

    {
        std::vector<std::jthread> threads;
        for (int j = 0; j < Settings::GetNumThreads(); j++) {
            threads.push_back(std::jthread(BCHTTiming, std::ref(tablebase[Settings::GetTBDepth()]), std::ref(random_positions), j, Settings::GetNumThreads()));
        }
    }

    std::chrono::time_point bcht_since_epoch_multi = std::chrono::high_resolution_clock::now(); // get the current time
    std::chrono::milliseconds bcht_millis_multi = std::chrono::duration_cast<std::chrono::milliseconds>(bcht_since_epoch_multi - bcht_time_multi);
    LOG_ALL("Time duration for BCHT multithreads:", bcht_millis_multi.count());

    LOG_MEMORY();
    return random_positions;
}


// place where the precomputation is stored
std::string GetFilePath (std::string file_name, int depth) {
    // path/to/puppet-cube-v2/precomputation/file_name
    return Settings::GetRootPath() + "precomputation/" + file_name + "_" + std::to_string(depth) + ".bin";
}

bool ExistsPrecomputation(std::vector<std::vector<State>>& tablebase, int depth) {
    std::string file_path = GetFilePath("tablebase", depth);
    if (std::FILE* file = std::fopen(file_path.c_str(), "rb")) {
        size_t size = 0;
        if (std::fread(&size, sizeof(size), 1, file) != 1) {
            LOG_CRITICAL(SkipSpace("Tablebase depth ["), SkipSpace(depth), SkipSpace("/"), SkipSpace(int(Settings::GetTBDepth())), "] was not able to read file", file_path);
        }
        tablebase.push_back({});
        tablebase.back().resize(size);

        if (std::fread(tablebase.back().data(), sizeof(State), size, file) == size) {
            LOG_ALL(SkipSpace("Tablebase depth ["), SkipSpace(depth), SkipSpace("/"), SkipSpace(int(Settings::GetTBDepth())), "] read from file");
            LOG_MEMORY();
        }
        else {
            LOG_CRITICAL(SkipSpace("Tablebase depth ["), SkipSpace(depth), SkipSpace("/"), SkipSpace(int(Settings::GetTBDepth())), "] was not able to read file", file_path);
        }
        std::fclose(file);
        return true;
    }
    LOG_ALL("Precompute tablebase depth", depth, "...");
    return false;
}


void SavePrecomputation(std::vector<State>& tablebase_layer, int depth) {
    std::string file_path = GetFilePath("tablebase", depth);
    if (std::FILE* file = std::fopen(file_path.c_str(), "wb")) {
        size_t size = tablebase_layer.size();
        if (std::fwrite(&size, sizeof(size), 1, file) != 1) {
            LOG_ERROR(SkipSpace("Tablebase depth ["), SkipSpace(depth), SkipSpace("/"), SkipSpace(int(Settings::GetTBDepth())), "] failed to write to file");
            return;
        }
        if (std::fwrite(tablebase_layer.data(), sizeof(State), size, file) != size) {
            LOG_ERROR(SkipSpace("Tablebase depth ["), SkipSpace(depth), SkipSpace("/"), SkipSpace(int(Settings::GetTBDepth())), "] failed to write full file");
            return;
        }
        std::fclose(file);
    }
    else {
        LOG_ERROR(SkipSpace("Tablebase depth ["), SkipSpace(depth), SkipSpace("/"), SkipSpace(int(Settings::GetTBDepth())), "] not able to save precomputation to file");
    }
}


void Tablebase::Initialize() {
    std::vector<State> empty_tb_pre = BuildBCHTSet(TablebasePrecomputation({}));
    std::vector<State> starting_position_tb = BuildBCHTSet(TablebasePrecomputation({kSolvedState}));

    tablebase.reserve(Settings::GetTBDepth()+1);
    tablebase.push_back(starting_position_tb);
    std::pair<std::reference_wrapper<std::vector<State>>, std::reference_wrapper<std::vector<State>>> previous_tables = {empty_tb_pre, starting_position_tb};

    TablebasePrecomputation tablebase_layer;
    for (int i = 1; i <= Settings::GetTBDepth(); i++) {
        tablebase_layer.clear();

        if (ExistsPrecomputation(tablebase, i)) {
            std::swap(previous_tables.first, previous_tables.second);
            previous_tables.second = tablebase.back();
            continue;
        }

        // start multiple threads
        {
            std::vector<std::jthread> threads;
            for (int j = 0; j < Settings::GetNumThreads(); j++) {
                threads.push_back(std::jthread(TablebaseSearch, previous_tables.first, previous_tables.second, std::ref(tablebase_layer), j, Settings::GetNumThreads()));
            }
        }
        LOG_ALL(SkipSpace("Tablebase depth ["), SkipSpace(i), SkipSpace("/"), SkipSpace(int(Settings::GetTBDepth())), "] precomputed:", tablebase_layer.size(), "positions");

        tablebase.push_back(BuildBCHTSet(tablebase_layer));
        std::swap(previous_tables.first, previous_tables.second);
        previous_tables.second = tablebase.back();
        SavePrecomputation(tablebase.back(), i);
        LOG_MEMORY();
    }

    if (Settings::GetTestBCHT()) {
        LOG_EXTRA("Needs to generate phmap");
        if (tablebase_layer.empty()) {
            for (const State& state : tablebase.back()) {
                if (state != State()) {
                    tablebase_layer.insert(state);
                }
            }
        }
        std::vector<State> random_positions = TimeTablebaseCPU(tablebase, tablebase_layer);
        #ifdef USE_CUDA
        LOG_EXTRA("Start Tablebase Timing Uploading Precomputation to Device");
        UploadRandomPositionsToDevice(random_positions);
        LOG_INFO("Tablebase Timing Uploaded to Device");
        #endif // USE_CUDA
    }
}
