#pragma once

#include <aurex/base/result.hpp>
#include <aurex/ir/ir.hpp>
#include <aurex/ir/optimization.hpp>
#include <aurex/ir/pass_manager.hpp>

#include <string_view>

namespace aurex::ir {

struct PassPipelineOptions {
    OptimizationLevel optimization_level = OptimizationLevel::none;
    bool verify_input = true;
    bool verify_output = true;
    bool enable_mem2reg = true;
    bool enable_cfg_cleanup = true;
    bool verify_after_each_pass = false;
    std::string_view stage_name = IR_PASS_PIPELINE_DEFAULT_STAGE_NAME;
    std::string_view stage_profile_name = IR_PASS_PIPELINE_DEFAULT_STAGE_PROFILE_NAME;
};

[[nodiscard]] std::string_view optimization_level_name(OptimizationLevel level) noexcept;
[[nodiscard]] base::Result<PassPipelineRunSummary> run_pass_pipeline_with_summary(
    Module& module, const PassPipelineOptions& options);
[[nodiscard]] base::Result<void> run_pass_pipeline(Module& module, const PassPipelineOptions& options);

} // namespace aurex::ir
