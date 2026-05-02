#pragma once

#include <filesystem>
#include <vector>

namespace aurex::driver {

enum class EmitKind {
    tokens,
    ast,
    modules,
    checked,
    check,
    c,
};

struct CompilerInvocation {
    std::filesystem::path input_path;
    std::filesystem::path output_path;
    EmitKind emit_kind = EmitKind::c;
    std::vector<std::filesystem::path> import_paths;
};

} // namespace aurex::driver
