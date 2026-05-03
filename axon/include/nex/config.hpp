#pragma once

#include "nex/error.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nex {

namespace fs = std::filesystem;

enum class TargetKind { executable, static_library };

struct TargetConfig {
    std::string              name;
    std::string              entry;         // entry .ax file
    std::string              output;        // output binary name
    TargetKind               kind = TargetKind::executable;
    std::vector<std::string> import_paths;  // -I directories
    std::vector<std::string> link_files;    // extra .c/.o to link
    std::string              m0_flags;
    std::string              c_flags;
};

struct ProjectConfig {
    std::string              project_name;
    std::vector<TargetConfig> targets;
};

// Parse a Nexfile. Format:
//   # comments
//   project = name
//   [target:name]
//   entry = path
//   output = name
//   import = dir          (repeatable)
//   link = file           (repeatable)
//   m0_flags = string
//   c_flags = string
//   kind = executable|static_library
Result<ProjectConfig> parse_nexfile(const fs::path& path);

} // namespace nex
