#pragma once

#include "aurex/base/result.hpp"
#include "aurex/ir/ir.hpp"
#include "aurex/ir/pass_pipeline.hpp"

#include <string>

namespace aurex::backend {

struct AuroraEmitOutput {
    std::string text;
};

struct AuroraEmitRequest {
    const ir::Module* module = nullptr;
    std::string module_name;
    std::string output_path;
    ir::OptimizationLevel opt_level = ir::OptimizationLevel::none;
};

[[nodiscard]] base::Result<AuroraEmitOutput> emit_aurora_asm(const AuroraEmitRequest& request);
[[nodiscard]] base::Result<void> emit_aurora_obj(const AuroraEmitRequest& request);

} // namespace aurex::backend
