#include "aurex/driver/standard_library.hpp"

#include <cstdlib>
#include <string_view>

namespace aurex::driver {

namespace {

#ifndef AUREX_BUILTIN_STDLIB_ROOT
#define AUREX_BUILTIN_STDLIB_ROOT ""
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
    return std::filesystem::exists(path / "text.ax", error) && !error &&
           std::filesystem::exists(path / "ffi" / "c" / "libc.ax", error) && !error &&
           std::filesystem::exists(path / "ffi" / "c" / "support" / "host_c.c", error) && !error;
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

} // namespace

std::string_view standard_library_backend_name(const StandardLibraryBackend backend) noexcept {
    switch (backend) {
    case StandardLibraryBackend::host_c: return "host-c";
    case StandardLibraryBackend::none: return "none";
    }
    return "host-c";
}

std::optional<StandardLibraryLayout> find_standard_library(const CompilerInvocation& invocation) {
    for (const std::filesystem::path& candidate : standard_library_candidates(invocation)) {
        const std::filesystem::path root = canonical_or_absolute(candidate);
        if (is_standard_library_root(root)) {
            return StandardLibraryLayout {
                root,
                root / "ffi" / "c" / "support" / "host_c.c",
            };
        }
    }
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
        return {layout.host_c_support_source};
    case StandardLibraryBackend::none:
        return {};
    }
    return {};
}

} // namespace aurex::driver
