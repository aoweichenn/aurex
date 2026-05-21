#pragma once

#include <aurex/driver/diagnostic_format.hpp>
#include <aurex/driver/emit_kind.hpp>
#include <aurex/ir/optimization.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace aurex::driver {

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
