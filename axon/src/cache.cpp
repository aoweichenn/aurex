#include "nex/cache.hpp"
#include "nex/digest.hpp"

#include <fstream>
#include <sstream>

namespace nex {

BuildCache::BuildCache(const fs::path& cache_file) : file_(cache_file) {}

Result<void> BuildCache::load() {
    hashes_.clear();
    std::ifstream in(file_);
    if (!in) return Result<void>::ok(); // no cache yet = ok
    std::string line;
    while (std::getline(in, line)) {
        auto sep = line.find('|');
        if (sep == std::string::npos) continue;
        auto key = line.substr(0, sep);
        auto val = line.substr(sep + 1);
        hashes_.emplace(std::move(key), std::move(val));
    }
    return Result<void>::ok();
}

Result<void> BuildCache::save() const {
    std::ofstream out(file_, std::ios::binary);
    if (!out) return Result<void>::err(ErrorCode::cache_error, "cannot write cache");
    for (const auto& [k, v] : hashes_) {
        out << k << '|' << v << '\n';
    }
    if (!out) return Result<void>::err(ErrorCode::cache_error, "cache write failed");
    return Result<void>::ok();
}

std::string BuildCache::get(const fs::path& path) const {
    auto it = hashes_.find(path.string());
    return it != hashes_.end() ? it->second : std::string{};
}

void BuildCache::set(const fs::path& path, std::string hash) {
    hashes_[path.string()] = std::move(hash);
}

bool BuildCache::all_match(const std::vector<fs::path>& paths) const {
    for (const auto& p : paths) {
        auto stored = get(p);
        if (stored.empty()) return false;
        auto current = file_digest(p);
        if (current != stored) return false;
    }
    return !paths.empty();
}

std::size_t BuildCache::size() const noexcept {
    return hashes_.size();
}

} // namespace nex
