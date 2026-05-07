#include "aurex/driver/standard_library.hpp"

#include <cstdlib>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace aurex::driver {

namespace {

#ifndef AUREX_BUILTIN_STDLIB_ROOT
#define AUREX_BUILTIN_STDLIB_ROOT ""
#endif

#ifndef AUREX_BUILTIN_HOST_C_SUPPORT_LIBRARY
#define AUREX_BUILTIN_HOST_C_SUPPORT_LIBRARY ""
#endif

[[nodiscard]] std::filesystem::path canonical_or_absolute(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(path, error);
    if (!error) {
        return canonical;
    }
    return std::filesystem::absolute(path);
}

[[nodiscard]] bool is_standard_library_root(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::exists(path / "core" / "text.ax", error) && !error &&
           std::filesystem::exists(path / "ffi" / "c" / "libc.ax", error) && !error &&
           std::filesystem::exists(path / "ffi" / "c" / "support" / "host_c.c", error) && !error;
}

[[nodiscard]] std::filesystem::path built_in_host_c_support_library(const std::filesystem::path& root) {
    std::filesystem::path library = AUREX_BUILTIN_HOST_C_SUPPORT_LIBRARY;
    if (library.empty()) {
        return {};
    }

    std::error_code error;
    const std::filesystem::path built_in_root = canonical_or_absolute(AUREX_BUILTIN_STDLIB_ROOT);
    if (root != built_in_root) {
        return {};
    }

    library = canonical_or_absolute(library);
    return std::filesystem::exists(library, error) && !error ? library : std::filesystem::path {};
}

void append_candidate(std::vector<std::filesystem::path>& candidates, const std::filesystem::path& path) {
    if (!path.empty()) {
        candidates.push_back(path);
    }
}

[[nodiscard]] std::vector<std::filesystem::path> standard_library_candidates(const CompilerInvocation& invocation) {
    std::vector<std::filesystem::path> candidates;
    append_candidate(candidates, invocation.standard_library_path);
    if (const char* env = std::getenv("AUREX_STDLIB"); env != nullptr && env[0] != '\0') {
        append_candidate(candidates, env);
    }
    append_candidate(candidates, AUREX_BUILTIN_STDLIB_ROOT);

    if (!invocation.tool_path.empty()) {
        const std::filesystem::path tool = canonical_or_absolute(invocation.tool_path);
        const std::filesystem::path tool_dir = tool.parent_path();
        append_candidate(candidates, tool_dir / "std");
        append_candidate(candidates, tool_dir / ".." / "std");
        append_candidate(candidates, tool_dir / ".." / ".." / "std");
        append_candidate(candidates, tool_dir / ".." / "share" / "aurex" / "std");
        append_candidate(candidates, tool_dir / ".." / "lib" / "aurex" / "std");
        append_candidate(candidates, tool_dir / ".." / ".." / "share" / "aurex" / "std");
    }

    append_candidate(candidates, std::filesystem::current_path() / "std");
    return candidates;
}

[[nodiscard]] std::string standard_library_cache_key(const CompilerInvocation& invocation) {
    std::string key;
    key += invocation.standard_library_path.string();
    key.push_back('\n');
    if (const char* env = std::getenv("AUREX_STDLIB"); env != nullptr) {
        key += env;
    }
    key.push_back('\n');
    key += invocation.tool_path.string();
    key.push_back('\n');
    std::error_code error;
    key += std::filesystem::current_path(error).string();
    return key;
}

} // namespace

std::string_view standard_library_backend_name(const StandardLibraryBackend backend) noexcept {
    switch (backend) {
    case StandardLibraryBackend::host_c: return "host-c";
    case StandardLibraryBackend::none: return "none";
    }
    return "host-c";
}

std::optional<StandardLibraryLayout> find_standard_library(const CompilerInvocation& invocation) {
    static std::unordered_map<std::string, std::optional<StandardLibraryLayout>> cache;
    static std::mutex cache_mutex;
    const std::string cache_key = standard_library_cache_key(invocation);
    std::lock_guard lock(cache_mutex);
    if (const auto found = cache.find(cache_key); found != cache.end()) {
        return found->second;
    }

    for (const std::filesystem::path& candidate : standard_library_candidates(invocation)) {
        const std::filesystem::path root = canonical_or_absolute(candidate);
        if (is_standard_library_root(root)) {
            StandardLibraryLayout layout {
                root,
                root / "ffi" / "c" / "support" / "host_c.c",
                built_in_host_c_support_library(root),
            };
            cache.emplace(cache_key, layout);
            return layout;
        }
    }
    cache.emplace(cache_key, std::nullopt);
    return std::nullopt;
}

std::vector<std::filesystem::path> standard_library_import_paths(const CompilerInvocation& invocation) {
    std::vector<std::filesystem::path> paths = invocation.import_paths;
    if (!invocation.use_standard_library) {
        return paths;
    }
    const std::optional<StandardLibraryLayout> layout = find_standard_library(invocation);
    if (layout) {
        paths.push_back(layout->root.parent_path());
    }
    return paths;
}

std::vector<std::filesystem::path> standard_library_support_sources(
    const StandardLibraryLayout& layout,
    const StandardLibraryBackend backend
) {
    switch (backend) {
    case StandardLibraryBackend::host_c:
        if (!layout.host_c_support_library.empty()) {
            return {layout.host_c_support_library};
        }
        return {layout.host_c_support_source};
    case StandardLibraryBackend::none:
        return {};
    }
    return {};
}

} // namespace aurex::driver
