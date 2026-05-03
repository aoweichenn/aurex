#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace aurex::driver {

enum class EmitKind {
    tokens,
    ast,
    modules,
    checked,
    check,
    c,
    assembly,
    executable,
};

struct CompilerInvocation {
    std::filesystem::path input_path;
    std::filesystem::path output_path;
    EmitKind emit_kind = EmitKind::c;
    std::vector<std::filesystem::path> import_paths;
    std::string clang_path = "clang";
    std::vector<std::string> clang_args;
    std::vector<std::filesystem::path> runtime_c_paths;
};

} // namespace aurex::driver
