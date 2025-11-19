#include <atomic>


template <typename T>
inline void AtomicMin(std::atomic<T>& lhs, T rhs) {
    static_assert(std::is_integral_v<T>, "AtomicMin requires integral type");

    T old = lhs.load(std::memory_order_relaxed);

    while (old > rhs &&
           !lhs.compare_exchange_weak(
               old, rhs,
               std::memory_order_relaxed,
               std::memory_order_relaxed))
    {}
}

