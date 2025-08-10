#include <cstdint>
#include <map>
#include <queue>
#include <set>
#include <utility>
#include <vector>

#include "BCHTSet.hpp"
#include "cube.hpp"
#include "logger.hpp"


std::pair<uint64_t, u_int64_t> PackState(const Cube::State& state) {
    uint64_t low = 0;
    uint64_t high = 0;
    low |= uint64_t(state.corner_orientation);
    low <<= 16;  // NOLINT
    low |= uint64_t(state.corner_position);
    low <<= 16;  // NOLINT
    low |= uint64_t(state.edge_orientation);
    high |= uint64_t(state.edge_position_1);
    high <<= 32;  // NOLINT
    high |= uint64_t(state.edge_position_2);

    return {low, high};
}


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

    std::pair<uint64_t, uint64_t> MixInput(uint64_t low, uint64_t high) const {
        return {Mix64(high ^ state_high), (low ^ state_low)};
    }
};

uint32_t ComputeBucket(std::pair<uint64_t, uint64_t> pack_state, uint32_t num_buckets) {
    uint64_t combined = pack_state.first ^ pack_state.second;
    return uint32_t(combined % num_buckets);
}

std::array<uint32_t, 3> GetStartBuckets(const Cube::State& key, uint32_t num_buckets) {
    // 3 independent hashers with different seeds
    constexpr SplitMix128 hasher1(0x123456789abcdef0ULL, 0xfedcba9876543210ULL);  // NOLINT
    constexpr SplitMix128 hasher2(0x0f1e2d3c4b5a6978ULL, 0x87654321abcdef09ULL);  // NOLINT
    constexpr SplitMix128 hasher3(0xabcdef0123456789ULL, 0x0123456789abcdefULL);  // NOLINT

    std::pair<uint64_t, uint64_t> pack_state = PackState(key);

    return {
        ComputeBucket(hasher1.MixInput(pack_state.first, pack_state.second), num_buckets),
        ComputeBucket(hasher2.MixInput(pack_state.first, pack_state.second), num_buckets),
        ComputeBucket(hasher3.MixInput(pack_state.first, pack_state.second), num_buckets)
    };
}


int GetBucketIndex(Cube::State& cube, uint32_t hash, uint32_t num_buckets) {
    std::array<uint32_t, 3> start_buckets = GetStartBuckets(cube, num_buckets);
    for (int i = 0; i < 3; i++) {
        if (start_buckets[i] == hash) {
            return i;
        }
    }
    LOG_ERROR("hash and bucket do not fit");
    return -1;
}


bool BfsInsert(std::vector<Cube::State>& table, uint32_t num_buckets, Cube::State key) {
    std::array<uint32_t, 3> start_buckets = GetStartBuckets(key, num_buckets);

    // Layer 0: try direct insert
    for (uint32_t bucket : start_buckets) {
        for (int i = 0; i < kBucketSize; i++) {
            if (table[(bucket*kBucketSize) + i] == Cube::State()) {
                table[(bucket*kBucketSize) + i] = key;
                return true;
            }
        }
    }

    // Parent map: child node -> parent node
    std::map<Cube::State, std::pair<Cube::State, uint32_t>> parents;
    std::set<uint32_t> used_keys;
    std::priority_queue<std::pair<int, Cube::State>, std::vector<std::pair<int, Cube::State>>, std::greater<>> p_q;

    // insert the starting nodes
    p_q.push({0, key});
    parents.insert({key, {Cube::State(), -1}});

    // bfs / dijkstra
    while (p_q.empty()) {
        std::pair<int, Cube::State> current = p_q.top();
        p_q.pop();

        std::array<uint32_t, 3> current_buckets = GetStartBuckets(current.second, num_buckets);
        for (int j = 0; j < 3; j++) {
            for (int i = 0; i < kBucketSize; i++) {
                uint32_t hash_idx = (current_buckets[j]*kBucketSize) + i;
                // found empty spot
                if (table[hash_idx] == Cube::State()) {
                    uint32_t final_hash = hash_idx;
                    std::pair<Cube::State, uint32_t> final_parent = parents[current.second];
                    while (final_parent.first != Cube::State()) {
                        table[final_hash] = final_parent.first;
                        final_hash = final_parent.second;
                        final_parent = parents[final_parent.first];
                    }
                    return true;
                }

                if (used_keys.contains(hash_idx)) {
                    continue;
                }
                used_keys.insert(hash_idx);

                int prev_bucket_index = GetBucketIndex(table[hash_idx], hash_idx, num_buckets);
                int diff = j - prev_bucket_index;
                p_q.push({current.first + diff, table[hash_idx]});
                parents[table[hash_idx]] = {current.second, hash_idx};
            }
        }
    }

    return false;
}


std::vector<Cube::State> BuildBCHTSet(const Cube::Tablebase& tablebase) {
    uint32_t num_buckets = tablebase.size() / kBucketSize * kLoadFacor;
    std::vector<Cube::State> table(num_buckets);

    uint32_t cnt = 0;
    bool failed = false;
    for (const Cube::State& key : tablebase) {
        cnt++;
        if (cnt % (num_buckets * kBucketSize / 100) == 0) {
            LOG_ALL("Building of the BCHT set:", cnt / (num_buckets * kBucketSize / 100), "% full");
        }
        if (!BfsInsert(table, num_buckets, key)) {
            failed = true;
            break;
        }
    }

    if (failed) {
        LOG_CRITICAL("Build of the BCHT set fail! Try decrease the load factor!");
    }

    LOG_INFO("Build of the BCHT set");

    return table;
}
