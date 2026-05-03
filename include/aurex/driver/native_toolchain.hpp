#pragma once

#include "aurex/base/result.hpp"
#include "aurex/driver/invocation.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace aurex::driver {

struct NativeCompileRequest {
    std::string clang_path;
    std::vector<std::string> clang_args;
    std::filesystem::path input_path;
    std::filesystem::path output_path;
    std::vector<std::filesystem::path> support_source_paths;
    EmitKind emit_kind = EmitKind::executable;
    bool input_is_llvm_ir = false;
};

[[nodiscard]] base::Result<void> invoke_clang(const NativeCompileRequest& request);

} // namespace aurex::driver
