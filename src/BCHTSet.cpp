#include <cstdint>
#include <map>
#include <queue>
#include <set>
#include <utility>
#include <vector>

#include "BCHTSet.hpp"
#include "cube.hpp"
#include "logger.hpp"


bool BCHTSetContains(const std::vector<PackedState>& table, const State& key) {
    uint32_t num_buckets = table.size() / kBucketSize;
    uint64_t h_1 = key.SplitMix64<State::kXORlow1, State::kXORhigh1>()%num_buckets;
    for (int j = 0; j < kBucketSize; j++) {
        PackedState tb_data = table[(h_1*kBucketSize) + j];
        if (IsSameState(key, tb_data)) {
            return true;
        }
        if (tb_data.hash_1 == uint16_t(-1) && tb_data.hash_2 == uint32_t(-1) && tb_data.hash_3 == uint32_t(-1)) {
            return false;
        }
    }
    uint64_t h_2 = key.SplitMix64<State::kXORlow2, State::kXORhigh2>()%num_buckets;
    for (int j = 0; j < kBucketSize; j++) {
        PackedState tb_data = table[(h_2*kBucketSize) + j];
        if (IsSameState(key, tb_data)) {
            return true;
        }
        if (tb_data.hash_1 == uint16_t(-1) && tb_data.hash_2 == uint32_t(-1) && tb_data.hash_3 == uint32_t(-1)) {
            return false;
        }
    }
    return false;
}


int GetBucketIndex(const PackedState& cube, uint32_t hash, uint32_t num_buckets) {
    if (PackedStateToState(cube).SplitMix64<State::kXORlow1, State::kXORhigh1>()%num_buckets == hash) {
        return 0;
    }
    if (PackedStateToState(cube).SplitMix64<State::kXORlow2, State::kXORhigh2>()%num_buckets == hash) {
        return 1;
    }
    LOG_CRITICAL("hash and bucket do not fit");
    return -1;
}


bool BfsInsert(std::vector<PackedState>& table, uint32_t num_buckets, const State& key) {
    std::array<uint32_t, 2> start_buckets = {
        uint32_t(key.SplitMix64<State::kXORlow1, State::kXORhigh1>()%num_buckets),
        uint32_t(key.SplitMix64<State::kXORlow2, State::kXORhigh2>()%num_buckets)
    };

    // Layer 0: try direct insert
    for (uint32_t bucket : start_buckets) {
        for (int i = 0; i < kBucketSize; i++) {
            if (table[(bucket*kBucketSize) + i] == PackedState()) {
                table[(bucket*kBucketSize) + i] = PackedState(key);
                GetBucketIndex(table[(bucket*kBucketSize) + i], bucket, num_buckets);
                return true;
            }
        }
    }

    // Parent map: child node -> parent node
    std::map<uint32_t, uint32_t> parents;
    std::set<uint32_t> used_hash_idxs;
    std::priority_queue<std::pair<int, std::pair<State, uint32_t>>, std::vector<std::pair<int, std::pair<State, uint32_t>>>, std::greater<>> p_q;

    // insert the starting nodes
    p_q.push({0, {key, -1}});

    // bfs / dijkstra
    while (!p_q.empty()) {
        std::pair<int, std::pair<State, uint32_t>> current = p_q.top();
        p_q.pop();


        std::array<uint32_t, 2> current_buckets = {
            uint32_t(current.second.first.SplitMix64<State::kXORlow1, State::kXORhigh1>()%num_buckets),
            uint32_t(current.second.first.SplitMix64<State::kXORlow2, State::kXORhigh2>()%num_buckets)
        };
        for (int j = 0; j < 2; j++) {
            for (int i = 0; i < kBucketSize; i++) {
                uint32_t hash_idx = (current_buckets[j]*kBucketSize) + i;
                if (used_hash_idxs.contains(hash_idx)) {
                    continue;
                }
                used_hash_idxs.insert(hash_idx);

                // found empty spot
                if (table[hash_idx] == State()) {
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
                p_q.push({current.first + diff, {PackedStateToState(table[hash_idx]), hash_idx}});
                parents[hash_idx] = current.second.second;
            }
        }
    }

    return false;
}


constexpr size_t kMinBuckets = 100;
std::vector<PackedState> BuildBCHTSet(const TablebasePrecomputation& tablebase) {
    size_t num_buckets = std::max(size_t(std::ceil(double(tablebase.size()) / kBucketSize / kLoadFacor)), kMinBuckets);
    std::vector<PackedState> table(num_buckets*kBucketSize, State());
    LOG_EXTRA("Creating BCHT with load factor:", SkipSpace(tablebase.size() / double(table.size()) * 100), "%");

    size_t cnt = 0;
    for (const State& key : tablebase) {
        cnt++;
        if (num_buckets > 1e7 && cnt % (num_buckets * kBucketSize / 100) == 0) {  // NOLINT
            LOG_EXTRA("Building of the BCHT set:", SkipSpace(int(cnt / (double(num_buckets) * kBucketSize / 100))), "% full");
        }
        if (!BfsInsert(table, num_buckets, key)) {
            LOG_EXTRA(cnt, "/", tablebase.size());
            LOG_CRITICAL("Build of the BCHT set fail! Try decrease the load factor!");
        }
    }
    return table;
}
