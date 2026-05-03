#pragma once

#include "aurex/ir/pass_pipeline.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace aurex::driver {

enum class EmitKind {
    tokens,
    ast,
    modules,
    checked,
    ir,
    llvm_ir,
    check,
    assembly,
    object,
    executable,
};

enum class StandardLibraryBackend {
    host_c,
    none,
};

struct CompilerInvocation {
    std::filesystem::path input_path;
    std::filesystem::path tool_path;
    std::filesystem::path output_path;
    std::filesystem::path standard_library_path;
    EmitKind emit_kind = EmitKind::executable;
    std::vector<std::filesystem::path> import_paths;
    std::string clang_path = "clang";
    std::vector<std::string> clang_args;
    ir::OptimizationLevel optimization_level = ir::OptimizationLevel::none;
    StandardLibraryBackend standard_library_backend = StandardLibraryBackend::host_c;
    bool use_standard_library = true;
};

} // namespace aurex::driver
