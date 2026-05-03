#include "nex/executor.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

namespace nex {

class ThreadPool {
public:
    explicit ThreadPool(int count) {
        for (int i = 0; i < count; ++i) {
            workers_.emplace_back(&ThreadPool::worker_loop, this);
        }
    }

    ~ThreadPool() {
        stop();
        for (auto& w : workers_) if (w.joinable()) w.join();
    }

    void stop() {
        {
            std::lock_guard lock(mu_);
            stopped_ = true;
        }
        cv_.notify_all();
    }

    void enqueue(std::function<void()> task) {
        {
            std::lock_guard lock(mu_);
            if (stopped_) return;
            tasks_.push_back(std::move(task));
        }
        cv_.notify_one();
    }

    void wait_all() {
        std::unique_lock lock(mu_);
        cv_done_.wait(lock, [this] { return tasks_.empty() && active_ == 0; });
    }

private:
    void worker_loop() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock lock(mu_);
                cv_.wait(lock, [this] { return stopped_ || !tasks_.empty(); });
                if (stopped_ && tasks_.empty()) return;
                task = std::move(tasks_.front());
                tasks_.pop_front();
                ++active_;
            }
            task();
            {
                std::lock_guard lock(mu_);
                --active_;
            }
            cv_done_.notify_one();
        }
    }

    std::vector<std::thread> workers_;
    std::deque<std::function<void()>> tasks_;
    std::mutex mu_;
    std::condition_variable cv_;
    std::condition_variable cv_done_;
    int active_ = 0;
    bool stopped_ = false;
};

// ── Executor ─────────────────────────────────────────────────────────────

Executor::Executor(int concurrency) {
    if (concurrency <= 0) {
        concurrency = static_cast<int>(std::thread::hardware_concurrency());
        if (concurrency <= 0) concurrency = 4;
    }
    concurrency_ = concurrency;
}

Executor::~Executor() = default;

Result<void> Executor::run(const std::vector<BuildJob>& jobs) {
    if (jobs.empty()) return Result<void>::ok();

    // Single job: just run synchronously
    if (jobs.size() == 1) return run_one(jobs[0]);

    ThreadPool pool(concurrency_);
    std::atomic<int> remaining(static_cast<int>(jobs.size()));
    std::atomic<int> failures(0);

    for (size_t i = 0; i < jobs.size(); ++i) {
        auto job = jobs[i]; // copy
        pool.enqueue([job = std::move(job), &remaining, &failures]() {
            std::cout << "[nex] " << job.description << '\n';
            auto ok = run_one(job);
            if (!ok) {
                std::cerr << "[nex] ERROR: " << ok.error().message << '\n';
                ++failures;
            }
            --remaining;
        });
    }

    pool.wait_all();

    if (failures > 0) {
        return Result<void>::err(ErrorCode::exec_error,
            std::to_string(failures.load()) + " build job(s) failed");
    }
    return Result<void>::ok();
}

Result<void> Executor::run_one(const BuildJob& job) {
    std::cout << "  " << job.command << '\n';
    int rc = std::system(job.command.c_str());
    if (rc != 0) {
        return Result<void>::err(ErrorCode::exec_error,
            "command failed (exit " + std::to_string(rc) + "): " + job.description);
    }
    return Result<void>::ok();
}

int Executor::concurrency() const noexcept {
    return concurrency_;
}

} // namespace nex
