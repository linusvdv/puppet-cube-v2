#include <array>
#include <cstdint>
#include <random>
#include <set>

#include "corner.hpp"
#include "cube.hpp"
#include "edge.hpp"
#include "logger.hpp"
#include "rotation.hpp"
#include "settings.hpp"
#include "utils.hpp"

namespace tablebase {
constexpr int kBucketSize = 2;
using Tablebase = std::vector<std::array<uint64_t, kBucketSize>>;

// this will always have 2 hash functions
constexpr double kLoadFactor = 0.8;

constexpr std::array<uint64_t, 10> kTablebaseExpectedPosCnt = {
    1,
    18,
    219,
    2508,
    27455,
    285354,
    2851989,
    27654873,
    261697962,
    2430549703
};


std::vector<Tablebase> tablebase_depths;


inline void InverseSplitMix64(uint64_t& num) {
    num ^= num >> 31; // NOLINT
    num ^= num >> 62; // NOLINT
    num *= 0x319642b2d24d8ec3ULL; // NOLINT
    num ^= num >> 27; // NOLINT
    num ^= num >> 54; // NOLINT
    num *= 0x96de1b173f119089ULL; // NOLINT
    num ^= num >> 30; // NOLINT
    num ^= num >> 60; // NOLINT
}


inline void InverseStateHash1(const uint64_t tb_size, uint64_t idx, uint64_t value, State& out_state) {
    uint64_t base = value & ~63ULL; // NOLINT
    uint64_t offset = (idx + tb_size - (base % tb_size)) % tb_size;
    uint64_t packed_state = base + offset;

    out_state.edge_sym = uint8_t(value ^ packed_state);

    InverseSplitMix64(packed_state);

    packed_state ^= uint64_t(out_state.edge_sym) << 58; // NOLINT
    out_state.edge_pos      = uint32_t(packed_state & 0xFFFFFFULL);         // 24 bits NOLINT
    out_state.corner_pos    = uint16_t((packed_state >> 24) & 0xFFFFULL);   // 16 bits NOLINT
    out_state.corner_orient = uint16_t((packed_state >> 40) & 0xFFFULL);    // 12 bits NOLINT
    out_state.edge_orient   = uint16_t((packed_state >> 52) & 0x7FFULL);    // 11 bits NOLINT
}


inline void InverseStateHash2(const uint64_t tb_size, uint64_t idx, uint64_t value, State& out_state) {
    uint64_t base = value & ~63ULL; // NOLINT
    uint64_t offset = (idx + tb_size - (base % tb_size)) % tb_size;
    uint64_t packed_state = base + offset;

    out_state.edge_sym = uint8_t(value ^ packed_state);

    InverseSplitMix64(packed_state);

    packed_state ^= 0x5555555555555555ULL; // NOLINT

    packed_state ^= uint64_t(out_state.edge_sym) << 58; // NOLINT
    out_state.edge_pos      = uint32_t(packed_state & 0xFFFFFFULL);         // 24 bits NOLINT
    out_state.corner_pos    = uint16_t((packed_state >> 24) & 0xFFFFULL);   // 16 bits NOLINT
    out_state.corner_orient = uint16_t((packed_state >> 40) & 0xFFFULL);    // 12 bits NOLINT
    out_state.edge_orient   = uint16_t((packed_state >> 52) & 0x7FFULL);    // 11 bits NOLINT
}


void InitTablebaseNeutralElement(uint64_t num_elements, Tablebase& tablebase) {
    // compute tablebase size
    uint64_t size = std::max(uint64_t(double(num_elements)/kLoadFactor/kBucketSize/2), uint64_t(64));
    tablebase.assign(size*2, {});
    for (uint64_t i = 0; i < size; i++) {
        for (int j = 0; j < kBucketSize; j++) {
            tablebase[i][j] = i ^ kNeurtralElementXOR;
        }
    }
    for (uint64_t i = 0; i < size; i++) {
        for (int j = 0; j < kBucketSize; j++) {
            tablebase[i+size][j] = i ^ kNeurtralElementXOR;
        }
    }
}


bool TablebaseInsert(Tablebase& tablebase, State state, std::mt19937& gen) {
    std::uniform_int_distribution<> dist_hash(0, 1);
    std::uniform_int_distribution<> dist_bucket_idx(0, kBucketSize-1);
    uint64_t size = tablebase.size()>>1;
    for (int i = 0; i < 1000; i++) { // NOLINT MAX allowed random shuffling
        uint64_t idx1;
        uint64_t value1;
        GetStateHash1(size, idx1, value1, state);
        for (int j = 0; j < kBucketSize; j++) {
            if (tablebase[idx1][j] == value1) { // already in tablebase
                return false;
            }
            if (tablebase[idx1][j] == (idx1 ^ kNeurtralElementXOR)) { // inserted tablebase
                tablebase[idx1][j] = value1;
                return true;
            }
        }
        uint64_t idx2;
        uint64_t value2;
        GetStateHash2(size, idx2, value2, state);
        for (int j = 0; j < kBucketSize; j++) {
            if (tablebase[idx2+size][j] == value2) { // already in tablebase
                return false;
            }
            if (tablebase[idx2+size][j] == (idx2 ^ kNeurtralElementXOR)) { // inserted tablebase
                tablebase[idx2+size][j] = value2;
                return true;
            }
        }
        // No space to insert element move a random one out of the 2*kBucketSize
        // repreat the process with the new randomly selected element O(log(size)) average
        if (dist_hash(gen) == 0) {
            std::swap(value1, tablebase[idx1][dist_bucket_idx(gen)]);
            InverseStateHash1(size, idx1, value1, state);
        }
        else {
            std::swap(value2, tablebase[idx2+size][dist_bucket_idx(gen)]);
            InverseStateHash2(size, idx2, value2, state);
        }
    }
    LOG_CRITICAL("Tablebase: BCHT set could not be constructed because of a too high load factor!");
    return false;
}


bool TablebaseContains(const Tablebase& tablebase, const State& state) {
    uint64_t size = tablebase.size()>>1;
    uint64_t idx1;
    uint64_t value1;
    GetStateHash1(size, idx1, value1, state);
    for (int j = 0; j < kBucketSize; j++) {
        uint64_t save_value = tablebase[idx1][j];
        if (save_value == value1) { // already in tablebase
            return true;
        }
        if (save_value == (idx1 ^ kNeurtralElementXOR)) { // empty
            return false;
        }
    }
    uint64_t idx2;
    uint64_t value2;
    GetStateHash2(size, idx2, value2, state);
    for (int j = 0; j < kBucketSize; j++) {
        uint64_t save_value = tablebase[idx2+size][j];
        if (save_value == value2) { // already in tablebase
            return true;
        }
        if (save_value == (idx2 ^ kNeurtralElementXOR)) { // empty
            return false;
        }
    }
    return false;
}


void GenerateDepth(const Tablebase& tb_prev_depth, const Tablebase& tb_cur_depth, Tablebase& tb_next_depth, uint64_t next_size, std::mt19937& gen) {
    uint64_t next_tb_depth_cnt = 0;
    InitTablebaseNeutralElement(next_size, tb_next_depth);
    uint64_t cur_size = tb_cur_depth.size()>>1;
    for (uint64_t idx = 0; idx < tb_cur_depth.size(); idx++) {
        for (int bucket = 0; bucket < kBucketSize; bucket++) {
            State state;
            if (idx < cur_size) {
                if (tb_cur_depth[idx][bucket] == (idx ^ kNeurtralElementXOR)) {
                    continue;
                }
                InverseStateHash1(cur_size, idx, tb_cur_depth[idx][bucket], state);
            }
            else {
                if (tb_cur_depth[idx][bucket] == ((idx-cur_size) ^ kNeurtralElementXOR)) {
                    continue;
                }
                InverseStateHash2(cur_size, idx-cur_size, tb_cur_depth[idx][bucket], state);
            }
            uint64_t corner_heuristic = corner::GetHeuristic(state.corner_pos, state.corner_orient);
            for (uint8_t rot = 0; rot < kNumRot; rot++) {
                if (((corner_heuristic >> (8+2*rot)) & 3) == 3) { // illegal rotation
                    continue;
                }
                State next_state = state;
                corner::Rotate(next_state.corner_pos, next_state.corner_orient, rot);
                edge::Rotate(next_state.edge_pos, next_state.edge_sym, next_state.edge_orient, rot);
                if (TablebaseContains(tb_prev_depth, next_state) || TablebaseContains(tb_cur_depth, next_state)) {
                    continue;
                }
                if (TablebaseInsert(tb_next_depth, next_state, gen)) {
                    next_tb_depth_cnt++;
                }
            }
        }
    }
    LOG_EXTRA("Count", next_tb_depth_cnt);
    if (next_tb_depth_cnt != next_size) {
        LOG_CRITICAL("not correct tablebase count");
    }
}


void Init() {
    std::mt19937 gen(0);
    tablebase_depths.assign(Settings::GetTBDepth() + 1, {});
    // init tb_depth 0
    InitTablebaseNeutralElement(1, tablebase_depths[0]);
    TablebaseInsert(tablebase_depths[0], kSolvedState, gen);

    Tablebase empty_tablebase;
    InitTablebaseNeutralElement(0, empty_tablebase);

    for (int tb_depth = 1; tb_depth <= Settings::GetTBDepth(); tb_depth++) {
        LoadOrGenerate("[" + std::to_string(tb_depth) + "/" + std::to_string(Settings::GetTBDepth()) + "] Tablebase",
                       [&](){GenerateDepth(
            (tb_depth == 1 ? empty_tablebase : tablebase_depths[tb_depth-2]),
            tablebase_depths[tb_depth-1],
            tablebase_depths[tb_depth],
            kTablebaseExpectedPosCnt[tb_depth], gen);},
                       "tablebase_" + std::to_string(tb_depth) + ".bin",
                       tablebase_depths[tb_depth],
                       std::max(uint64_t(double(kTablebaseExpectedPosCnt[tb_depth])/kLoadFactor/kBucketSize/2), uint64_t(64))*2);
    }
}
}
