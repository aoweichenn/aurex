#include <aurex/driver/file_cache.hpp>

#include <aurex/driver/driver_messages.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>

namespace aurex::driver {

namespace {

struct FileCacheKey {
    std::filesystem::path path;

    [[nodiscard]] bool operator==(const FileCacheKey& other) const noexcept {
        return path == other.path;
    }
};

struct FileCacheKeyHash {
    [[nodiscard]] std::size_t operator()(const FileCacheKey& key) const noexcept {
        return std::filesystem::hash_value(key.path);
    }
};

struct FileCacheEntry {
    std::filesystem::file_time_type modified {};
    std::uintmax_t size = 0;
    std::string text;
};

[[nodiscard]] std::filesystem::path cache_path_key(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(path, error);
    if (!error) {
        return canonical;
    }
    return std::filesystem::absolute(path);
}

[[nodiscard]] base::Result<std::string> read_text_file_uncached(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return base::Result<std::string>::fail({base::ErrorCode::io_error, std::string(DRIVER_INPUT_OPEN_FAILED)});
    }

    std::string text;
    std::error_code error;
    const std::uintmax_t size = std::filesystem::file_size(path, error);
    if (!error) {
        text.resize(static_cast<std::size_t>(size));
        if (!text.empty()) {
            input.read(text.data(), static_cast<std::streamsize>(text.size()));
            if (!input) {
                return base::Result<std::string>::fail({base::ErrorCode::io_error, std::string(DRIVER_INPUT_READ_FAILED)});
            }
        }
        return base::Result<std::string>::ok(std::move(text));
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return base::Result<std::string>::ok(buffer.str());
}

std::mutex file_cache_mutex;
std::unordered_map<FileCacheKey, FileCacheEntry, FileCacheKeyHash> file_cache;

} // namespace

base::Result<std::string> read_text_file(const std::filesystem::path& path) {
    std::error_code size_error;
    std::error_code modified_error;
    const std::filesystem::path key_path = cache_path_key(path);
    const std::uintmax_t size = std::filesystem::file_size(path, size_error);
    const std::filesystem::file_time_type modified = std::filesystem::last_write_time(path, modified_error);
    if (size_error || modified_error) {
        return read_text_file_uncached(path);
    }

    const FileCacheKey key {key_path};
    {
        std::lock_guard lock(file_cache_mutex);
        if (const auto found = file_cache.find(key); found != file_cache.end() &&
            found->second.size == size &&
            found->second.modified == modified) {
            return base::Result<std::string>::ok(found->second.text);
        }
    }

    auto read_result = read_text_file_uncached(path);
    if (!read_result) {
        return read_result;
    }

    std::string text = std::move(read_result.value());
    {
        std::lock_guard lock(file_cache_mutex);
        file_cache[key] = FileCacheEntry {
            modified,
            size,
            text,
        };
    }
    return base::Result<std::string>::ok(std::move(text));
}

void clear_file_cache() {
    std::lock_guard lock(file_cache_mutex);
    file_cache.clear();
}

} // namespace aurex::driver
