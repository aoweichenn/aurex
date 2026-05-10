#pragma once

#include <aurex/syntax/ast.hpp>

#include <filesystem>
#include <string>
#include <string_view>

namespace aurex::syntax {

[[nodiscard]] std::filesystem::path module_path_to_relative_file(const ModulePath& path);
[[nodiscard]] std::string module_path_to_string(const ModulePath& path);
[[nodiscard]] bool module_paths_equal(const ModulePath& lhs, const ModulePath& rhs) noexcept;
[[nodiscard]] std::string mangle_c_symbol(const ModulePath& module, std::string_view name);

} // namespace aurex::syntax
