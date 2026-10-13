#pragma once
#ifdef __EMSCRIPTEN__
#include "emscripten_barrier.hpp"
using ThreadPoolBarrier = ems::Barrier;
#else
#include <barrier>
using ThreadPoolBarrier = std::barrier<>;
#endif
#include <functional>
#include <thread>
#include <vector>


class ThreadPool {
public:
    explicit ThreadPool(size_t n);
    ~ThreadPool();

    void AsyncRun(std::function<void(size_t)> task);
    void Wait();
    void Run(std::function<void(size_t)> task);


private:
    void WorkerLoop(size_t thread_id);

    ThreadPoolBarrier            barrier_start_;
    ThreadPoolBarrier            barrier_end_;
    std::function<void(size_t)>  task_;
    std::atomic<bool>            shutdown_{false};
    std::vector<std::jthread>    workers_;
};
