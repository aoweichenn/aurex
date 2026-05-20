#pragma once

#include <aurex/ir/pass_pipeline.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace aurex::driver {

enum class EmitKind {
    tokens,
    ast,
    modules,
    checked,
    typed,
    ir,
    llvm_ir,
    check,
    assembly,
    object,
    executable,
};

enum class DiagnosticOutputFormat {
    text,
    json,
};

struct CompilerInvocation {
    std::filesystem::path input_path;
    std::filesystem::path tool_path;
    std::filesystem::path output_path;
    std::filesystem::path incremental_cache_path;
    std::filesystem::path profile_output_path;
    EmitKind emit_kind = EmitKind::executable;
    std::vector<std::filesystem::path> import_paths;
    std::string clang_path = "clang";
    std::vector<std::string> clang_args;
    ir::OptimizationLevel optimization_level = ir::OptimizationLevel::none;
    DiagnosticOutputFormat diagnostic_format = DiagnosticOutputFormat::text;
    bool query_pruning_enabled = true;
};

} // namespace aurex::driver
