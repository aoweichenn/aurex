#pragma once

#include <aurex/base/result.hpp>

#include <chrono>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace aurex::driver {

struct CompilationPhaseProfile {
    std::string name;
    std::string detail;
    double elapsed_ms = 0.0;
    double rss_mib_after = 0.0;
    double rss_delta_mib = 0.0;
};

class CompilationProfiler final {
public:
    explicit CompilationProfiler(bool enabled = false);

    [[nodiscard]] bool enabled() const noexcept;
    void record(std::string_view name, std::string_view detail, std::chrono::steady_clock::duration elapsed);
    void record(std::string_view name, std::chrono::steady_clock::duration elapsed);
    [[nodiscard]] std::span<const CompilationPhaseProfile> phases() const noexcept;
    [[nodiscard]] base::Result<void> write_json(const std::filesystem::path& path) const;

private:
    bool enabled_ = false;
    double last_rss_mib_ = 0.0;
    std::vector<CompilationPhaseProfile> phases_;
};

class ScopedCompilationPhase final {
public:
    ScopedCompilationPhase(CompilationProfiler* profiler, std::string_view name, std::string_view detail = {}) noexcept;
    ScopedCompilationPhase(const ScopedCompilationPhase&) = delete;
    ScopedCompilationPhase& operator=(const ScopedCompilationPhase&) = delete;
    ScopedCompilationPhase(ScopedCompilationPhase&&) = delete;
    ScopedCompilationPhase& operator=(ScopedCompilationPhase&&) = delete;
    ~ScopedCompilationPhase();

private:
    CompilationProfiler* profiler_ = nullptr;
    std::string_view name_;
    std::string_view detail_;
    std::chrono::steady_clock::time_point started_;
};

} // namespace aurex::driver
