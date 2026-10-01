#pragma once
#include <barrier>
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

    std::vector<std::jthread>    workers_;
    std::barrier<>               barrier_start_;
    std::barrier<>               barrier_end_;
    std::function<void(size_t)>  task_;
    std::atomic<bool>            shutdown_{false};
};
