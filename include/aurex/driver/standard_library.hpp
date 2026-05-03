#pragma once

#include "aurex/driver/invocation.hpp"

#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

namespace aurex::driver {

struct StandardLibraryLayout {
    std::filesystem::path root;
    std::filesystem::path host_c_support_source;
};

[[nodiscard]] std::string_view standard_library_backend_name(StandardLibraryBackend backend) noexcept;
[[nodiscard]] std::optional<StandardLibraryLayout> find_standard_library(const CompilerInvocation& invocation);
[[nodiscard]] std::vector<std::filesystem::path> standard_library_import_paths(const CompilerInvocation& invocation);
[[nodiscard]] std::vector<std::filesystem::path> standard_library_support_sources(
    const StandardLibraryLayout& layout,
    StandardLibraryBackend backend
);

} // namespace aurex::driver
