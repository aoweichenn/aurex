#include <aurex/ir/analysis_manager.hpp>
#include <aurex/ir/pass_manager.hpp>
#include <aurex/ir/verify.hpp>

#include <string>
#include <utility>

namespace aurex::ir {
namespace {

constexpr std::string_view PASS_MANAGER_INVALID_PASS = "IR pass manager received an invalid pass";
constexpr std::string_view PASS_MANAGER_VERIFY_AFTER_PASS_FAILED = "IR verifier failed after pass ";
constexpr std::string_view PASS_MANAGER_VERIFY_DETAIL_SEPARATOR = ": ";

[[nodiscard]] std::size_t analysis_index(const AnalysisId analysis) noexcept
{
    return static_cast<std::size_t>(analysis);
}

[[nodiscard]] base::Result<void> prefix_after_pass_verifier_error(
    const std::string_view pass_name, const base::Error& error)
{
    std::string message;
    message.reserve(PASS_MANAGER_VERIFY_AFTER_PASS_FAILED.size() + pass_name.size()
        + PASS_MANAGER_VERIFY_DETAIL_SEPARATOR.size() + error.message.size());
    message += PASS_MANAGER_VERIFY_AFTER_PASS_FAILED;
    message += pass_name;
    message += PASS_MANAGER_VERIFY_DETAIL_SEPARATOR;
    message += error.message;
    return base::Result<void>::fail({error.code, std::move(message)});
}

} // namespace

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
    return verify_module(module);
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
    return prefix_after_pass_verifier_error(pass_name, result.error());
}

base::Result<void> VerifierGate::verify_output(const Module& module) const
{
    if (!this->options_.verify_output) {
        return base::Result<void>::ok();
    }
    return verify_module(module);
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
