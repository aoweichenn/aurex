#include <aurex/application/driver/invocation.hpp>
#include <aurex/application/driver/profile.hpp>
#include <aurex/infrastructure/pipeline/stage.hpp>
#include <aurex/midend/ir/ir_dump.hpp>
#include <aurex/midend/ir/lower_ast.hpp>
#include <aurex/midend/ir/pass_pipeline.hpp>

#include <array>
#include <chrono>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#include <application/driver/pipeline/private/lowering_pipeline.hpp>

namespace aurex::driver {
namespace {

constexpr std::string_view LOWERING_PASS_SUMMARY_SCHEDULED = "scheduled=";
constexpr std::string_view LOWERING_PASS_SUMMARY_EXECUTED = ",executed=";
constexpr std::string_view LOWERING_PASS_SUMMARY_CHANGED = ",changed=";
constexpr std::string_view LOWERING_PASS_SUMMARY_PRESERVED = ",preserved=";
constexpr std::string_view LOWERING_PASS_SUMMARY_INVALIDATED = ",invalidated=";
constexpr std::string_view LOWERING_PASS_SUMMARY_ALL = "all";
constexpr std::string_view LOWERING_PASS_SUMMARY_NONE = "none";
constexpr std::string_view LOWERING_PASS_SUMMARY_SEPARATOR = "|";
constexpr std::string_view LOWERING_PASS_SUMMARY_FAILED = "failed";
constexpr std::array<ir::AnalysisId, ir::IR_ANALYSIS_COUNT> LOWERING_PASS_SUMMARY_ANALYSES{{
    ir::AnalysisId::control_flow_graph,
    ir::AnalysisId::dominance,
    ir::AnalysisId::value_uses,
    ir::AnalysisId::type_table,
    ir::AnalysisId::symbol_table,
    ir::AnalysisId::record_layouts,
}};

[[nodiscard]] ir::PassPipelineOptions make_pass_pipeline_options(
    const ir::OptimizationLevel optimization_level, const PipelineStageRecord& stage) noexcept
{
    return ir::PassPipelineOptions{
        optimization_level,
        true,
        true,
        true,
        true,
        false,
        stage.name,
        stage.profile_name,
    };
}

void append_analysis_list(std::string& detail, const std::span<const ir::AnalysisId> analyses)
{
    if (analyses.empty()) {
        detail += LOWERING_PASS_SUMMARY_NONE;
        return;
    }
    for (base::usize index = 0; index < analyses.size(); ++index) {
        if (index != 0) {
            detail += LOWERING_PASS_SUMMARY_SEPARATOR;
        }
        detail += ir::analysis_id_name(analyses[index]);
    }
}

void append_preserved_analysis_list(std::string& detail, const ir::PreservedAnalyses& preserved_analyses)
{
    if (preserved_analyses.preserves_all()) {
        detail += LOWERING_PASS_SUMMARY_ALL;
        return;
    }
    std::vector<ir::AnalysisId> preserved;
    preserved.reserve(LOWERING_PASS_SUMMARY_ANALYSES.size());
    for (const ir::AnalysisId analysis : LOWERING_PASS_SUMMARY_ANALYSES) {
        if (preserved_analyses.preserves(analysis)) {
            preserved.push_back(analysis);
        }
    }
    append_analysis_list(detail, preserved);
}

[[nodiscard]] std::string pass_pipeline_summary_detail(const ir::PassPipelineRunSummary& summary)
{
    std::string detail;
    detail += LOWERING_PASS_SUMMARY_SCHEDULED;
    detail += std::to_string(summary.scheduled_pass_count);
    detail += LOWERING_PASS_SUMMARY_EXECUTED;
    detail += std::to_string(summary.executed_pass_count);
    detail += LOWERING_PASS_SUMMARY_CHANGED;
    detail += summary.changed ? "1" : "0";
    detail += LOWERING_PASS_SUMMARY_PRESERVED;
    append_preserved_analysis_list(detail, summary.preserved_analyses);
    detail += LOWERING_PASS_SUMMARY_INVALIDATED;
    append_analysis_list(detail, summary.invalidated_analyses);
    return detail;
}

} // namespace

LoweringPipeline::LoweringPipeline(CompilationSession& session) noexcept : session_(session)
{
}

base::Result<void> LoweringPipeline::dump_checked_output(const sema::CheckedModule& checked)
{
    ScopedCompilationPhase phase(this->session_.profiler(), PipelineStageId::checked_dump);
    std::cout << sema::dump_checked_module(checked);
    return base::Result<void>::ok();
}

base::Result<ir::Module> LoweringPipeline::lower_and_optimize(
    const syntax::AstModule& ast, const sema::CheckedModule& checked)
{
    auto ir_result = [&] {
        ScopedCompilationPhase phase(this->session_.profiler(), PipelineStageId::ir_lower);
        return ir::lower_ast(ast, checked);
    }();
    if (!ir_result) {
        return base::Result<ir::Module>::fail(ir_result.error());
    }

    auto pipeline_result = [&] {
        const PipelineStageRecord& stage = pipeline_stage_record(PipelineStageId::ir_pass_pipeline);
        const auto started = std::chrono::steady_clock::now();
        auto result = ir::run_pass_pipeline_with_summary(
            ir_result.value(), make_pass_pipeline_options(this->session_.invocation().optimization_level, stage));
        const std::chrono::steady_clock::duration elapsed = std::chrono::steady_clock::now() - started;
        if (result) {
            this->session_.profiler()->record(
                PipelineStageId::ir_pass_pipeline, pass_pipeline_summary_detail(result.value()), elapsed);
        } else {
            this->session_.profiler()->record(PipelineStageId::ir_pass_pipeline, LOWERING_PASS_SUMMARY_FAILED, elapsed);
        }
        if (!result) {
            return base::Result<void>::fail(result.error());
        }
        return base::Result<void>::ok();
    }();
    if (!pipeline_result) {
        return base::Result<ir::Module>::fail(pipeline_result.error());
    }

    return base::Result<ir::Module>::ok(ir_result.take_value());
}

base::Result<void> LoweringPipeline::dump_ir_output(const ir::Module& module)
{
    ScopedCompilationPhase phase(this->session_.profiler(), PipelineStageId::ir_dump);
    std::cout << ir::dump_module(module);
    return base::Result<void>::ok();
}

} // namespace aurex::driver
