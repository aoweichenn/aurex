#pragma once

#include "nex/error.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nex {

namespace fs = std::filesystem;

// Build cache: tracks content hashes of source files to skip recompilation.
class BuildCache {
public:
    explicit BuildCache(const fs::path& cache_file);

    // Load from disk.
    Result<void> load();
    // Save to disk.
    Result<void> save() const;

    // Get cached hash for a file.
    std::string get(const fs::path& path) const;

    // Update hash for a file.
    void set(const fs::path& path, std::string hash);

    // Check if all files in `paths` match cached hashes.
    // Returns true if all match (build can be skipped).
    bool all_match(const std::vector<fs::path>& paths) const;

    // Number of cached entries.
    std::size_t size() const noexcept;

private:
    fs::path file_;
    std::unordered_map<std::string, std::string> hashes_; // path -> hash
};

} // namespace nex
