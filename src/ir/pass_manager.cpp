#include <aurex/ir/analysis_manager.hpp>
#include <aurex/ir/pass_manager.hpp>
#include <aurex/ir/verify.hpp>

#include <array>
#include <string>
#include <utility>

namespace aurex::ir {
namespace {

constexpr std::string_view PASS_MANAGER_INVALID_PASS = "IR pass manager received an invalid pass";
constexpr std::string_view PASS_MANAGER_VERIFY_FAILED = "IR verifier failed";
constexpr std::string_view PASS_MANAGER_VERIFY_AFTER_PASS_LABEL = " after pass ";
constexpr std::string_view PASS_MANAGER_VERIFY_DETAIL_SEPARATOR = ": ";
constexpr std::string_view PASS_MANAGER_VERIFY_CONTEXT_BEGIN = " [stage=";
constexpr std::string_view PASS_MANAGER_VERIFY_CONTEXT_PROFILE = " profile=";
constexpr std::string_view PASS_MANAGER_VERIFY_CONTEXT_VERIFIER = " verifier=";
constexpr std::string_view PASS_MANAGER_VERIFY_CONTEXT_PASS = " pass=";
constexpr std::string_view PASS_MANAGER_VERIFY_CONTEXT_END = "]";
constexpr std::string_view PASS_MANAGER_VERIFY_INPUT_NAME = "input";
constexpr std::string_view PASS_MANAGER_VERIFY_AFTER_PASS_NAME = "after_pass";
constexpr std::string_view PASS_MANAGER_VERIFY_OUTPUT_NAME = "output";
constexpr std::string_view PASS_MANAGER_ANALYSIS_CONTROL_FLOW_GRAPH = "control_flow_graph";
constexpr std::string_view PASS_MANAGER_ANALYSIS_DOMINANCE = "dominance";
constexpr std::string_view PASS_MANAGER_ANALYSIS_VALUE_USES = "value_uses";
constexpr std::string_view PASS_MANAGER_ANALYSIS_TYPE_TABLE = "type_table";
constexpr std::string_view PASS_MANAGER_ANALYSIS_SYMBOL_TABLE = "symbol_table";
constexpr std::string_view PASS_MANAGER_ANALYSIS_RECORD_LAYOUTS = "record_layouts";
constexpr std::string_view PASS_MANAGER_ANALYSIS_UNKNOWN = "unknown";
constexpr std::array<AnalysisId, IR_ANALYSIS_COUNT> PASS_MANAGER_ANALYSIS_IDS{{
    AnalysisId::control_flow_graph,
    AnalysisId::dominance,
    AnalysisId::value_uses,
    AnalysisId::type_table,
    AnalysisId::symbol_table,
    AnalysisId::record_layouts,
}};

enum class VerifierInvocationKind {
    input,
    after_pass,
    output,
};

[[nodiscard]] std::size_t analysis_index(const AnalysisId analysis) noexcept
{
    return static_cast<std::size_t>(analysis);
}

[[nodiscard]] std::string_view verifier_invocation_name(const VerifierInvocationKind kind) noexcept
{
    switch (kind) {
        case VerifierInvocationKind::input:
            return PASS_MANAGER_VERIFY_INPUT_NAME;
        case VerifierInvocationKind::after_pass:
            return PASS_MANAGER_VERIFY_AFTER_PASS_NAME;
        case VerifierInvocationKind::output:
            return PASS_MANAGER_VERIFY_OUTPUT_NAME;
    }
    return PASS_MANAGER_VERIFY_OUTPUT_NAME;
}

[[nodiscard]] bool contains_analysis(const std::vector<AnalysisId>& analyses, const AnalysisId analysis) noexcept
{
    for (const AnalysisId existing : analyses) {
        if (existing == analysis) {
            return true;
        }
    }
    return false;
}

void append_invalidated_analyses(std::vector<AnalysisId>& invalidated, const PreservedAnalyses& preserved_analyses)
{
    if (preserved_analyses.preserves_all()) {
        return;
    }
    for (const AnalysisId analysis : PASS_MANAGER_ANALYSIS_IDS) {
        if (preserved_analyses.preserves(analysis) || contains_analysis(invalidated, analysis)) {
            continue;
        }
        invalidated.push_back(analysis);
    }
}

[[nodiscard]] std::string_view verifier_stage_name(const VerifierGateOptions& options) noexcept
{
    return options.stage_name.empty() ? IR_PASS_PIPELINE_DEFAULT_STAGE_NAME : options.stage_name;
}

[[nodiscard]] std::string_view verifier_stage_profile_name(const VerifierGateOptions& options) noexcept
{
    return options.stage_profile_name.empty() ? IR_PASS_PIPELINE_DEFAULT_STAGE_PROFILE_NAME
                                              : options.stage_profile_name;
}

void append_verifier_context(std::string& message, const VerifierGateOptions& options,
    const VerifierInvocationKind kind, const std::string_view pass_name)
{
    message += PASS_MANAGER_VERIFY_CONTEXT_BEGIN;
    message += verifier_stage_name(options);
    message += PASS_MANAGER_VERIFY_CONTEXT_PROFILE;
    message += verifier_stage_profile_name(options);
    message += PASS_MANAGER_VERIFY_CONTEXT_VERIFIER;
    message += verifier_invocation_name(kind);
    if (kind == VerifierInvocationKind::after_pass) {
        message += PASS_MANAGER_VERIFY_CONTEXT_PASS;
        message += pass_name;
    }
    message += PASS_MANAGER_VERIFY_CONTEXT_END;
}

[[nodiscard]] base::Result<void> prefix_verifier_error(const VerifierGateOptions& options,
    const VerifierInvocationKind kind, const std::string_view pass_name, const base::Error& error)
{
    std::string message;
    message.reserve(PASS_MANAGER_VERIFY_FAILED.size() + PASS_MANAGER_VERIFY_AFTER_PASS_LABEL.size() + pass_name.size()
        + PASS_MANAGER_VERIFY_CONTEXT_BEGIN.size() + verifier_stage_name(options).size()
        + PASS_MANAGER_VERIFY_CONTEXT_PROFILE.size() + verifier_stage_profile_name(options).size()
        + PASS_MANAGER_VERIFY_CONTEXT_VERIFIER.size() + verifier_invocation_name(kind).size()
        + PASS_MANAGER_VERIFY_CONTEXT_PASS.size() + pass_name.size() + PASS_MANAGER_VERIFY_CONTEXT_END.size()
        + PASS_MANAGER_VERIFY_DETAIL_SEPARATOR.size() + error.message.size());
    message += PASS_MANAGER_VERIFY_FAILED;
    if (kind == VerifierInvocationKind::after_pass) {
        message += PASS_MANAGER_VERIFY_AFTER_PASS_LABEL;
        message += pass_name;
    }
    append_verifier_context(message, options, kind, pass_name);
    message += PASS_MANAGER_VERIFY_DETAIL_SEPARATOR;
    message += error.message;
    return base::Result<void>::fail({error.code, std::move(message)});
}

} // namespace

std::string_view analysis_id_name(const AnalysisId id) noexcept
{
    switch (id) {
        case AnalysisId::control_flow_graph:
            return PASS_MANAGER_ANALYSIS_CONTROL_FLOW_GRAPH;
        case AnalysisId::dominance:
            return PASS_MANAGER_ANALYSIS_DOMINANCE;
        case AnalysisId::value_uses:
            return PASS_MANAGER_ANALYSIS_VALUE_USES;
        case AnalysisId::type_table:
            return PASS_MANAGER_ANALYSIS_TYPE_TABLE;
        case AnalysisId::symbol_table:
            return PASS_MANAGER_ANALYSIS_SYMBOL_TABLE;
        case AnalysisId::record_layouts:
            return PASS_MANAGER_ANALYSIS_RECORD_LAYOUTS;
    }
    return PASS_MANAGER_ANALYSIS_UNKNOWN;
}

PreservedAnalyses PreservedAnalyses::all() noexcept
{
    PreservedAnalyses preserved;
    preserved.preserves_all_ = true;
    return preserved;
}

PreservedAnalyses PreservedAnalyses::none() noexcept
{
    return PreservedAnalyses{};
}

void PreservedAnalyses::preserve(const AnalysisId analysis) noexcept
{
    if (this->preserves_all_) {
        return;
    }
    this->analyses_.set(analysis_index(analysis));
}

void PreservedAnalyses::intersect(const PreservedAnalyses& other) noexcept
{
    if (other.preserves_all_) {
        return;
    }
    if (this->preserves_all_) {
        this->preserves_all_ = false;
        this->analyses_ = other.analyses_;
        return;
    }
    this->analyses_ &= other.analyses_;
}

bool PreservedAnalyses::preserves(const AnalysisId analysis) const noexcept
{
    return this->preserves_all_ || this->analyses_.test(analysis_index(analysis));
}

bool PreservedAnalyses::preserves_all() const noexcept
{
    return this->preserves_all_;
}

bool PreservedAnalyses::preserves_none() const noexcept
{
    return !this->preserves_all_ && this->analyses_.none();
}

std::string_view pass_id_name(const PassId id) noexcept
{
    switch (id) {
        case PassId::local_mem2reg:
            return "local_mem2reg";
        case PassId::cfg_cleanup:
            return "cfg_cleanup";
        case PassId::custom:
            return "custom";
    }
    return "unknown";
}

PassResult PassResult::unchanged() noexcept
{
    return PassResult{};
}

PassResult PassResult::changed_result(PreservedAnalyses preserved_analyses) noexcept
{
    PassResult result;
    result.changed = true;
    result.preserved_analyses = preserved_analyses;
    return result;
}

VerifierGate::VerifierGate(VerifierGateOptions options) noexcept : options_(options)
{
}

base::Result<void> VerifierGate::verify_input(const Module& module) const
{
    if (!this->options_.verify_input) {
        return base::Result<void>::ok();
    }
    const auto result = verify_module(module);
    if (result) {
        return base::Result<void>::ok();
    }
    return prefix_verifier_error(this->options_, VerifierInvocationKind::input, {}, result.error());
}

base::Result<void> VerifierGate::verify_after_pass(const Module& module, const std::string_view pass_name) const
{
    if (!this->options_.verify_after_each_pass) {
        return base::Result<void>::ok();
    }
    const auto result = verify_module(module);
    if (result) {
        return base::Result<void>::ok();
    }
    return prefix_verifier_error(this->options_, VerifierInvocationKind::after_pass, pass_name, result.error());
}

base::Result<void> VerifierGate::verify_output(const Module& module) const
{
    if (!this->options_.verify_output) {
        return base::Result<void>::ok();
    }
    const auto result = verify_module(module);
    if (result) {
        return base::Result<void>::ok();
    }
    return prefix_verifier_error(this->options_, VerifierInvocationKind::output, {}, result.error());
}

void ModulePassManager::add(ModulePass pass)
{
    this->passes_.push_back(pass);
}

std::span<const ModulePass> ModulePassManager::passes() const noexcept
{
    return std::span<const ModulePass>(this->passes_.data(), this->passes_.size());
}

base::Result<PassPipelineRunSummary> ModulePassManager::run(Module& module, const VerifierGate& verifier) const
{
    ModuleAnalysisManager analyses;
    return this->run(module, verifier, analyses);
}

base::Result<PassPipelineRunSummary> ModulePassManager::run(
    Module& module, const VerifierGate& verifier, ModuleAnalysisManager& analyses) const
{
    const auto input_verification = verifier.verify_input(module);
    if (!input_verification) {
        return base::Result<PassPipelineRunSummary>::fail(input_verification.error());
    }

    PassPipelineRunSummary summary;
    summary.scheduled_pass_count = this->passes_.size();
    for (const ModulePass& pass : this->passes_) {
        if (pass.run == nullptr || pass.name.empty()) {
            return base::Result<PassPipelineRunSummary>::fail(
                {base::ErrorCode::internal_error, std::string(PASS_MANAGER_INVALID_PASS)});
        }
        auto pass_result = pass.run(module, analyses);
        if (!pass_result) {
            return base::Result<PassPipelineRunSummary>::fail(pass_result.error());
        }
        ++summary.executed_pass_count;
        summary.changed = summary.changed || pass_result.value().changed;
        summary.preserved_analyses.intersect(pass_result.value().preserved_analyses);
        if (pass_result.value().changed) {
            append_invalidated_analyses(summary.invalidated_analyses, pass_result.value().preserved_analyses);
            analyses.invalidate(pass_result.value().preserved_analyses);
        }

        const auto pass_verification = verifier.verify_after_pass(module, pass.name);
        if (!pass_verification) {
            return base::Result<PassPipelineRunSummary>::fail(pass_verification.error());
        }
    }

    const auto output_verification = verifier.verify_output(module);
    if (!output_verification) {
        return base::Result<PassPipelineRunSummary>::fail(output_verification.error());
    }
    return base::Result<PassPipelineRunSummary>::ok(summary);
}

} // namespace aurex::ir
