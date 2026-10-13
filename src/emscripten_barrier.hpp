// std::barrier replacement for the emscripten build: the blocking wait is
// built on emscripten's futex (Atomics.wait on the shared heap) instead of
// libc++'s thread primitives - under emscripten those spin-poll their locks
// (emscripten_lock_wait_acquire is a CAS loop), which starves the search
// workers when many threads wait on start_work/done_work.
//
// ticket design: the arrival counter is MONOTONIC (never reset - a reset
// races against early arrivals of the next phase), every thread's ticket
// identifies its phase (ticket / count) and the last ticket of a phase
// (ticket % count == count-1) flips the phase word and wakes all waiters.
#pragma once

#ifdef __EMSCRIPTEN__

#include <atomic>
#include <cstdint>

#include <emscripten/atomic.h>

namespace ems {

class Barrier {
public:
    explicit Barrier(int participants)
        : count_(participants), tickets_(0), phase_(0) {}

    void arrive_and_wait() {
        const uint64_t ticket = tickets_.fetch_add(1, std::memory_order_acq_rel);
        const uint64_t my_phase = ticket / uint64_t(count_);
        if (ticket % uint64_t(count_) == uint64_t(count_) - 1) {
            // the last arrival of my_phase completes it
            phase_.store(my_phase + 1, std::memory_order_release);
            emscripten_atomic_notify((void*)&phase_, INT32_MAX);
            return;
        }
        // block until the phase word passes my phase (the futex wait is a
        // true block - it releases the worker thread to the os; the value
        // check makes lost wakeups impossible: a wait entered after the
        // flip returns immediately)
        while (phase_.load(std::memory_order_acquire) <= my_phase) {
            emscripten_atomic_wait_u32((void*)&phase_, uint32_t(my_phase), -1);
        }
    }

private:
    const int count_;
    std::atomic<uint64_t> tickets_;  // monotonic arrival counter
    std::atomic<uint64_t> phase_;    // completed phases (low 32: futex word)
};

} // namespace ems

#endif
