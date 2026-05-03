#pragma once

#include "aurex/driver/invocation.hpp"

#include <filesystem>
#include <optional>
#include <vector>

namespace aurex::driver {

struct StandardLibraryLayout {
    std::filesystem::path root;
    std::filesystem::path native_support_source;
};

[[nodiscard]] std::optional<StandardLibraryLayout> find_standard_library(const CompilerInvocation& invocation);
[[nodiscard]] std::vector<std::filesystem::path> standard_library_import_paths(const CompilerInvocation& invocation);

} // namespace aurex::driver
