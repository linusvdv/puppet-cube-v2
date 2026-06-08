#include <cstddef>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "BCHTSet.hpp"
#include "cube.hpp"
#include "logger.hpp"
#include "settings.hpp"
#include "utils.hpp"
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
        for (uint8_t rotation = 0; rotation < kNumRot; rotation++) {
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


bool ExistsPrecomputation(std::vector<std::vector<State>>& tablebase, int depth) {
    std::string file_path = GetFilePath("tablebase_" + std::to_string(depth) + ".bin");
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
    std::string file_path = GetFilePath("tablebase_" + std::to_string(depth) + ".bin");
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
}
