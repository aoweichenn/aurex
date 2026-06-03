#pragma once

#include <aurex/midend/ir/ir.hpp>
#include <aurex/midend/ir/pass_manager.hpp>

#include <optional>
#include <span>
#include <vector>

namespace aurex::ir {

struct BlockControlFlow final {
    BlockId block = INVALID_BLOCK_ID;
    std::vector<BlockId> successors;
    std::vector<BlockId> predecessors;
};

struct FunctionControlFlowGraph final {
    FunctionId function = INVALID_FUNCTION_ID;
    std::vector<BlockControlFlow> blocks;
};

struct ControlFlowGraphAnalysis final {
    std::vector<FunctionControlFlowGraph> functions;

    [[nodiscard]] const FunctionControlFlowGraph* find_function(FunctionId function) const noexcept;
};

enum class ValueUseSiteKind {
    value_operand,
    phi_input,
    terminator_operand,
    global_initializer,
};

struct ValueUseSite final {
    ValueUseSiteKind kind = ValueUseSiteKind::value_operand;
    FunctionId function = INVALID_FUNCTION_ID;
    BlockId block = INVALID_BLOCK_ID;
    ValueId user = INVALID_VALUE_ID;
    GlobalConstantId constant = INVALID_GLOBAL_CONSTANT_ID;
};

struct ValueUseList final {
    ValueId value = INVALID_VALUE_ID;
    std::vector<ValueUseSite> uses;
};

struct ValueUseAnalysis final {
    std::vector<ValueUseList> values;

    [[nodiscard]] std::span<const ValueUseSite> uses(ValueId value) const noexcept;
};

struct FunctionDominanceAnalysis final {
    FunctionId function = INVALID_FUNCTION_ID;
    std::vector<std::vector<BlockId>> dominators;
};

struct DominanceAnalysis final {
    std::vector<FunctionDominanceAnalysis> functions;

    [[nodiscard]] const FunctionDominanceAnalysis* find_function(FunctionId function) const noexcept;
    [[nodiscard]] bool dominates(FunctionId function, BlockId dominator, BlockId block) const noexcept;
};

class ModuleAnalysisManager final {
public:
    void clear() noexcept;
    void invalidate(const PreservedAnalyses& preserved_analyses) noexcept;

    [[nodiscard]] bool is_cached(AnalysisId analysis) const noexcept;
    [[nodiscard]] PreservedAnalyses cached_analyses() const noexcept;

    [[nodiscard]] const ControlFlowGraphAnalysis& control_flow_graph(const Module& module);
    [[nodiscard]] const ValueUseAnalysis& value_uses(const Module& module);
    [[nodiscard]] const DominanceAnalysis& dominance(const Module& module);

private:
    const Module* module_ = nullptr;
    std::optional<ControlFlowGraphAnalysis> control_flow_graph_;
    std::optional<ValueUseAnalysis> value_uses_;
    std::optional<DominanceAnalysis> dominance_;

    void ensure_module(const Module& module);
};

} // namespace aurex::ir
