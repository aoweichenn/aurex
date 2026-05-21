#pragma once

#include <aurex/base/result.hpp>
#include <aurex/ir/ir.hpp>
#include <aurex/ir/optimization.hpp>

#include <string_view>

namespace aurex::ir {

struct PassPipelineOptions {
    OptimizationLevel optimization_level = OptimizationLevel::none;
    bool verify_input = true;
    bool verify_output = true;
    bool enable_mem2reg = true;
    bool enable_cfg_cleanup = true;
};

[[nodiscard]] std::string_view optimization_level_name(OptimizationLevel level) noexcept;
[[nodiscard]] base::Result<void> run_pass_pipeline(Module& module, const PassPipelineOptions& options);

} // namespace aurex::ir
