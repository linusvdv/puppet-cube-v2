#include "thread_pool.hpp"


ThreadPool::ThreadPool(size_t n) : barrier_start_(n + 1), barrier_end_(n + 1) {
    for (size_t i = 0; i < n; ++i) {
        workers_.emplace_back([this, i] { WorkerLoop(i); });
    }
}


ThreadPool::~ThreadPool() {
    shutdown_ = true;
    barrier_start_.arrive_and_wait();
    barrier_end_.arrive_and_wait();
}


void ThreadPool::AsyncRun(std::function<void(size_t)> task) {
    task_ = std::move(task);
    barrier_start_.arrive_and_wait();
}


void ThreadPool::Wait() {
    barrier_end_.arrive_and_wait();
}


void ThreadPool::Run(std::function<void(size_t)> task) {
    AsyncRun(std::move(task));
    Wait();
}


void ThreadPool::WorkerLoop(size_t thread_id) {
    while (true) {
        barrier_start_.arrive_and_wait();
        if (shutdown_) {
            barrier_end_.arrive_and_wait();
            break;
        }
        task_(thread_id);
        barrier_end_.arrive_and_wait();
    }
}
