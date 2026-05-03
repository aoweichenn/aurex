#pragma once

#include "nex/error.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nex {

namespace fs = std::filesystem;

// Single source file's module info
struct ModuleInfo {
    std::string              name;       // "a.b.c"
    std::vector<std::string> imports;    // ["x.y", "z"]
};

// Scan an .ax source text for module/import declarations.
ModuleInfo scan_source(std::string_view text);

// Resolve a module path ("a.b.c") to a file path, searching in import_dirs.
// Returns the found path or nullopt.
std::optional<fs::path> resolve_module(
    std::string_view module_path,
    const std::vector<fs::path>& import_dirs);

// Find all .ax files recursively under root, scanning each for module info.
// Returns a map of module_name -> file_path.
std::vector<std::pair<std::string, fs::path>> discover_modules(
    const std::vector<fs::path>& search_dirs);

} // namespace nex
