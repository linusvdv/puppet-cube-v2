#include <cstdint>
#include <map>
#include <queue>
#include <set>
#include <utility>
#include <vector>

#include "BCHTSet.hpp"
#include "cube.hpp"
#include "logger.hpp"


struct SplitMix128 {
    uint64_t state_low;
    uint64_t state_high;

    static constexpr uint64_t kMulA = 0x2545f4914f6cdd1dULL;
    static constexpr uint64_t kMulB = 0x9e3779b97f4a7c15ULL;

    constexpr SplitMix128(uint64_t seed_low, uint64_t seed_high) : state_low(seed_low), state_high(seed_high) {}

    static uint64_t Mix64(uint64_t num) {
        num ^= num >> 31;  // NOLINT
        num *= kMulA;
        num ^= num >> 33;  // NOLINT
        num *= kMulB;
        num ^= num >> 28;  // NOLINT
        return num;
    }

    uint32_t MixInput(const Cube::State& state, const uint32_t& num_buckets) const {
        uint64_t combined = Mix64(state.hash_1 ^ state_high) ^ (state.hash_2 ^ state_low);
        return uint32_t(combined % num_buckets);
    }
};


// 2 independent hashers with different seeds
constexpr SplitMix128 hasher1(0x123456789abcdef0ULL, 0xfedcba9876543210ULL);  // NOLINT
constexpr SplitMix128 hasher2(0x0f1e2d3c4b5a6978ULL, 0x87654321abcdef09ULL);  // NOLINT


bool BCHTSetContains(const std::vector<Cube::State>& table, const Cube::State& key) {
    uint32_t num_buckets = table.size() / kBucketSize;
    uint64_t h_1 = hasher1.MixInput(key, num_buckets);
    for (int j = 0; j < kBucketSize; j++) {
        Cube::State tb_data = table[(h_1*kBucketSize) + j];
        if (tb_data == key) {
            return true;
        }
        if (tb_data.hash_1 == uint64_t(-1) && tb_data.hash_2 == uint16_t(-1)) {
            return false;
        }
    }
    uint64_t h_2 = hasher2.MixInput(key, num_buckets);
    for (int j = 0; j < kBucketSize; j++) {
        Cube::State tb_data = table[(h_2*kBucketSize) + j];
        if (tb_data == key) {
            return true;
        }
        if (tb_data.hash_1 == uint64_t(-1) && tb_data.hash_2 == uint16_t(-1)) {
            return false;
        }
    }
    return false;
}


int GetBucketIndex(const Cube::State& cube, uint32_t hash, uint32_t num_buckets) {
    if (hasher1.MixInput(cube, num_buckets) == hash) {
        return 0;
    }
    if (hasher2.MixInput(cube, num_buckets) == hash) {
        return 1;
    }
    LOG_CRITICAL("hash and bucket do not fit");
    return -1;
}


bool BfsInsert(std::vector<Cube::State>& table, uint32_t num_buckets, const Cube::State& key) {
    std::array<uint32_t, 2> start_buckets = {
        hasher1.MixInput(key, num_buckets),
        hasher2.MixInput(key, num_buckets)
    };

    // Layer 0: try direct insert
    for (uint32_t bucket : start_buckets) {
        for (int i = 0; i < kBucketSize; i++) {
            if (table[(bucket*kBucketSize) + i] == Cube::State()) {
                table[(bucket*kBucketSize) + i] = key;
                GetBucketIndex(table[(bucket*kBucketSize) + i], bucket, num_buckets);
                return true;
            }
        }
    }

    // Parent map: child node -> parent node
    std::map<uint32_t, uint32_t> parents;
    std::set<uint32_t> used_hash_idxs;
    std::priority_queue<std::pair<int, std::pair<Cube::State, uint32_t>>, std::vector<std::pair<int, std::pair<Cube::State, uint32_t>>>, std::greater<>> p_q;

    // insert the starting nodes
    p_q.push({0, {key, -1}});

    // bfs / dijkstra
    while (!p_q.empty()) {
        std::pair<int, std::pair<Cube::State, uint32_t>> current = p_q.top();
        p_q.pop();


        std::array<uint32_t, 2> current_buckets = {
        hasher1.MixInput(current.second.first, num_buckets),
        hasher2.MixInput(current.second.first, num_buckets)
        };
        for (int j = 0; j < 2; j++) {
            for (int i = 0; i < kBucketSize; i++) {
                uint32_t hash_idx = (current_buckets[j]*kBucketSize) + i;
                if (used_hash_idxs.contains(hash_idx)) {
                    continue;
                }
                used_hash_idxs.insert(hash_idx);

                // found empty spot
                if (table[hash_idx] == Cube::State()) {
                    uint32_t final_hash_idx = hash_idx;
                    uint32_t final_parent = current.second.second;
                    while (final_parent != uint32_t(-1)) {
                        table[final_hash_idx] = table[final_parent];
                        GetBucketIndex(table[final_hash_idx], final_hash_idx/kBucketSize, num_buckets);
                        final_hash_idx = final_parent;
                        final_parent = parents[final_parent];
                    }
                    table[final_hash_idx] = key;
                    return true;
                }

                int prev_bucket_index = GetBucketIndex(table[hash_idx], current_buckets[j], num_buckets);
                int diff = j - prev_bucket_index;
                p_q.push({current.first + diff, {table[hash_idx], hash_idx}});
                parents[hash_idx] = current.second.second;
            }
        }
    }

    return false;
}


constexpr size_t kMinBuckets = 100;
std::vector<Cube::State> BuildBCHTSet(const TablebasePrecomputation& tablebase) {
    size_t num_buckets = std::max(size_t(std::ceil(double(tablebase.size()) / kBucketSize / kLoadFacor)), kMinBuckets);
    std::vector<Cube::State> table(num_buckets*kBucketSize, Cube::State());
    LOG_EXTRA("Creating BCHT with load factor:", tablebase.size() / double(table.size()) * 100, "%");

    size_t cnt = 0;
    for (const Cube::State& key : tablebase) {
        cnt++;
        if (num_buckets > 1e7 && cnt % (num_buckets * kBucketSize / 100) == 0) {  // NOLINT
            LOG_EXTRA("Building of the BCHT set:", int(cnt / (double(num_buckets) * kBucketSize / 100)), "% full");
        }
        if (!BfsInsert(table, num_buckets, key)) {
            LOG_CRITICAL("Build of the BCHT set fail! Try decrease the load factor!");
        }
    }
    return table;
}
