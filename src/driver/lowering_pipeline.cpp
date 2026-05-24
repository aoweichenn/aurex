#include "lowering_pipeline.hpp"

#include <aurex/driver/invocation.hpp>
#include <aurex/driver/profile.hpp>
#include <aurex/ir/ir_dump.hpp>
#include <aurex/ir/lower_ast.hpp>
#include <aurex/ir/pass_pipeline.hpp>

#include <iostream>

#include "pipeline_stage.hpp"

namespace aurex::driver {
namespace {

[[nodiscard]] ir::PassPipelineOptions make_pass_pipeline_options(
    const ir::OptimizationLevel optimization_level) noexcept
{
    return ir::PassPipelineOptions{
        optimization_level,
        true,
        true,
        true,
        true,
    };
}

} // namespace

LoweringPipeline::LoweringPipeline(CompilationSession& session) noexcept : session_(session)
{
}

base::Result<void> LoweringPipeline::dump_checked_output(const sema::CheckedModule& checked)
{
    ScopedCompilationPhase phase(this->session_.profiler(), pipeline_stage_profile_name(PipelineStageId::checked_dump));
    std::cout << sema::dump_checked_module(checked);
    return base::Result<void>::ok();
}

base::Result<ir::Module> LoweringPipeline::lower_and_optimize(
    const syntax::AstModule& ast, const sema::CheckedModule& checked)
{
    auto ir_result = [&] {
        ScopedCompilationPhase phase(this->session_.profiler(), pipeline_stage_profile_name(PipelineStageId::ir_lower));
        return ir::lower_ast(ast, checked);
    }();
    if (!ir_result) {
        return base::Result<ir::Module>::fail(ir_result.error());
    }

    auto pipeline_result = [&] {
        ScopedCompilationPhase phase(
            this->session_.profiler(), pipeline_stage_profile_name(PipelineStageId::ir_pass_pipeline));
        return ir::run_pass_pipeline(
            ir_result.value(), make_pass_pipeline_options(this->session_.invocation().optimization_level));
    }();
    if (!pipeline_result) {
        return base::Result<ir::Module>::fail(pipeline_result.error());
    }

    return base::Result<ir::Module>::ok(ir_result.take_value());
}

base::Result<void> LoweringPipeline::dump_ir_output(const ir::Module& module)
{
    ScopedCompilationPhase phase(this->session_.profiler(), pipeline_stage_profile_name(PipelineStageId::ir_dump));
    std::cout << ir::dump_module(module);
    return base::Result<void>::ok();
}

} // namespace aurex::driver
