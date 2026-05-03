#pragma once

#include "nex/error.hpp"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include <filesystem>

namespace nex {

namespace fs = std::filesystem;

// A single build job (compile command).
struct BuildJob {
    std::string description;  // human-readable description
    std::string command;      // shell command to execute
    std::string work_dir;     // optional working directory
};

// Executes build jobs in parallel using a thread pool.
class Executor {
public:
    explicit Executor(int concurrency = 0); // 0 = auto (nproc)
    ~Executor();

    Executor(const Executor&) = delete;
    Executor& operator=(const Executor&) = delete;

    // Run all jobs in parallel. Returns ok if all succeed.
    // Prints each job's output to stdout/stderr as they complete.
    Result<void> run(const std::vector<BuildJob>& jobs);

    // Execute a single command synchronously.
    static Result<void> run_one(const BuildJob& job);

    int concurrency() const noexcept;

private:
    int concurrency_;
};

} // namespace nex
