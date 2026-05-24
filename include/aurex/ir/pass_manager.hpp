#pragma once

#include <aurex/base/result.hpp>
#include <aurex/ir/ir.hpp>

#include <bitset>
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace aurex::ir {

enum class AnalysisId {
    control_flow_graph,
    dominance,
    value_uses,
    type_table,
    symbol_table,
    record_layouts,
};

inline constexpr std::size_t IR_ANALYSIS_COUNT = static_cast<std::size_t>(AnalysisId::record_layouts) + 1U;

class PreservedAnalyses final {
public:
    [[nodiscard]] static PreservedAnalyses all() noexcept;
    [[nodiscard]] static PreservedAnalyses none() noexcept;

    void preserve(AnalysisId analysis) noexcept;
    void intersect(const PreservedAnalyses& other) noexcept;

    [[nodiscard]] bool preserves(AnalysisId analysis) const noexcept;
    [[nodiscard]] bool preserves_all() const noexcept;
    [[nodiscard]] bool preserves_none() const noexcept;

private:
    bool preserves_all_ = false;
    std::bitset<IR_ANALYSIS_COUNT> analyses_;
};

enum class PassId {
    local_mem2reg,
    cfg_cleanup,
    custom,
};

[[nodiscard]] std::string_view pass_id_name(PassId id) noexcept;

struct PassResult {
    bool changed = false;
    PreservedAnalyses preserved_analyses = PreservedAnalyses::all();

    [[nodiscard]] static PassResult unchanged() noexcept;
    [[nodiscard]] static PassResult changed_result(PreservedAnalyses preserved_analyses) noexcept;
};

using ModulePassRun = base::Result<PassResult> (*)(Module& module);

struct ModulePass {
    PassId id = PassId::custom;
    std::string_view name;
    ModulePassRun run = nullptr;
};

struct VerifierGateOptions {
    bool verify_input = true;
    bool verify_output = true;
    bool verify_after_each_pass = false;
};

class VerifierGate final {
public:
    explicit VerifierGate(VerifierGateOptions options) noexcept;

    [[nodiscard]] base::Result<void> verify_input(const Module& module) const;
    [[nodiscard]] base::Result<void> verify_after_pass(const Module& module, std::string_view pass_name) const;
    [[nodiscard]] base::Result<void> verify_output(const Module& module) const;

private:
    VerifierGateOptions options_;
};

struct PassPipelineRunSummary {
    std::size_t scheduled_pass_count = 0;
    std::size_t executed_pass_count = 0;
    bool changed = false;
    PreservedAnalyses preserved_analyses = PreservedAnalyses::all();
};

class ModulePassManager final {
public:
    void add(ModulePass pass);

    [[nodiscard]] std::span<const ModulePass> passes() const noexcept;
    [[nodiscard]] base::Result<PassPipelineRunSummary> run(Module& module, const VerifierGate& verifier) const;

private:
    std::vector<ModulePass> passes_;
};

} // namespace aurex::ir
