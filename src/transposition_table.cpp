#include <atomic>
#include <vector>
#include "cube.hpp"
#include "logger.hpp"
#include "settings.hpp"
#include "transposition_table.hpp"


namespace transposition_table {
std::vector<std::atomic<uint64_t>> tt; // NOLINT


void Clear() {
    std::fill(
        reinterpret_cast<uint64_t*>(tt.data()),
        reinterpret_cast<uint64_t*>(tt.data()+tt.size()),
        kDefaultTTEntry
    );
}
void Clear(size_t thread_idx, size_t num_threads) {
    std::fill(
        reinterpret_cast<uint64_t*>(tt.data() + (tt.size()*(thread_idx)/num_threads)),
        reinterpret_cast<uint64_t*>(tt.data() + (tt.size()*(thread_idx+1)/num_threads)),
        kDefaultTTEntry
    );
}


void Init() {
    uint64_t num_elements = (uint64_t(Settings::GetTTSize())*1024*1024) / sizeof(std::atomic<uint64_t>); // NOLINT
    constexpr uint64_t kMinTTSize = (1<<6) * (1<<8);
    if (num_elements < kMinTTSize) {
        LOG_ERROR("Trabsposition Table too small");
        num_elements = kMinTTSize;
        LOG_WARNING("Set Transposition Table size to ", num_elements*sizeof(std::atomic<uint64_t>) / 1024 / 1024, "MB");
    }
    // the number of elements has to have 8 zeros at the end
    num_elements &= ~uint64_t((1ULL<<8) - 1);
    std::vector<std::atomic<uint64_t>>(num_elements).swap(tt);
    Clear();
}


InTT Contains(const State& state, const uint8_t& depth) {
    uint64_t idx;
    uint64_t value;
    GetTTHash(state, depth, idx, value);
    uint64_t tt_value = tt[idx].load(std::memory_order_relaxed);

    if (tt_value == kDefaultTTEntry) { // no entry in TT
        return InTT::kFalse;
    }
    if ((tt_value>>8) != (value>>8)) { // other entry in TT
        return InTT::kCollision;
    }
    if (uint8_t(tt_value) > depth) { // position with higher depth
        return InTT::kHighDepth;
    }
    return InTT::kTrue;
}
}
