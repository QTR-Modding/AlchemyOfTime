#pragma once

#include <thread>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <future>
#include <type_traits> // For std::invoke_result

class ThreadPool {
public:
    explicit ThreadPool(const size_t numThreads) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock lock(queueMutex);
                        condition.wait(lock, [this] { return stop || !tasks.empty(); });
                        if (stop && tasks.empty()) return;
                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                    task();
                }
            });
        }
    }

    ~ThreadPool() {
        {
            std::unique_lock lock(queueMutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread& worker : workers) {
            worker.join();
        }
    }

    template <class F, class... Args>
    std::future<std::invoke_result_t<F, Args...>> enqueue(F&& f, Args&&... args) {
        using return_type = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
            );

        std::future<return_type> res = task->get_future();
        {
            std::unique_lock lock(queueMutex);
            if (stop) throw std::runtime_error("enqueue on stopped ThreadPool");
            tasks.emplace([task]() { (*task)(); });
        }
        condition.notify_one();
        return res;
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop = false;
};

// Create a global thread pool instance
inline size_t numThreads = std::max(1u, std::thread::hardware_concurrency());
//inline ThreadPool pool(numThreads);


class SpeedProfiler {
    std::chrono::time_point<std::chrono::steady_clock> start_time;
    std::chrono::time_point<std::chrono::steady_clock> end_time;
    std::string name;

public:
    explicit SpeedProfiler(const std::string& name) {
        start_time = std::chrono::steady_clock::now();
        this->name = name;
    }

    ~SpeedProfiler() {
        end_time = std::chrono::steady_clock::now();
        const std::chrono::duration<double> elapsed_seconds = end_time - start_time;
        logger::info("Elapsed time: {}", elapsed_seconds.count());
    }
};