#include <aurex/midend/ir/analysis_manager.hpp>

#include <initializer_list>
#include <span>
#include <vector>

#include <gtest/support/ir_test_helpers.hpp>

namespace aurex::test {
namespace {

using namespace irtest;

void expect_block_ids(const std::vector<BlockId>& actual, std::initializer_list<base::u32> expected)
{
    ASSERT_EQ(actual.size(), expected.size());
    auto current = actual.begin();
    for (const base::u32 expected_id : expected) {
        ASSERT_NE(current, actual.end());
        EXPECT_EQ(current->value, expected_id);
        ++current;
    }
}

[[nodiscard]] base::Result<ir::PassResult> cache_analyses_and_preserve_cfg_test_pass(
    Module& module, ir::ModuleAnalysisManager& analyses)
{
    static_cast<void>(analyses.control_flow_graph(module));
    static_cast<void>(analyses.value_uses(module));
    static_cast<void>(analyses.dominance(module));

    ir::PreservedAnalyses preserved = ir::PreservedAnalyses::none();
    preserved.preserve(ir::AnalysisId::control_flow_graph);
    return base::Result<ir::PassResult>::ok(ir::PassResult::changed_result(preserved));
}

[[nodiscard]] base::Result<ir::PassResult> unchanged_invalidate_none_test_pass(
    Module& module, ir::ModuleAnalysisManager& analyses)
{
    static_cast<void>(module);
    static_cast<void>(analyses);
    ir::PassResult result;
    result.changed = false;
    result.preserved_analyses = ir::PreservedAnalyses::none();
    return base::Result<ir::PassResult>::ok(result);
}

} // namespace

TEST(CoreUnit, AnalysisManagerBuildsCfgDominanceAndValueUses)
{
    Module module;
    const TypeHandle i32 = builtin(module, BuiltinType::i32);
    Function function = make_function(module, "branching", i32);
    FunctionBuilder builder{module, function};

    const BlockId entry = builder.block("entry");
    const BlockId then_block = builder.block("then");
    const BlockId else_block = builder.block("else");
    const BlockId merge = builder.block("merge");
    const ValueId condition = builder.add(bool_value(module, true));
    const ValueId one = builder.add(integer_value(module, i32, "1"));
    const ValueId two = builder.add(integer_value(module, i32, "2"));
    Value call = module.make_value();
    call.kind = ValueKind::call;
    call.type = i32;
    call.args = {one};
    const ValueId call_id = builder.add(call);
    Value aggregate = module.make_value();
    aggregate.kind = ValueKind::aggregate;
    aggregate.type = i32;
    aggregate.elements = {one};
    aggregate.fields = {field_value(module, "two", two)};
    const ValueId aggregate_id = builder.add(aggregate);
    Value phi = module.make_value();
    phi.kind = ValueKind::phi;
    phi.type = i32;
    phi.incoming = {PhiInput{then_block, one}, PhiInput{else_block, two}};
    const ValueId phi_id = builder.add(phi);
    static_cast<void>(add_global_constant(module, global_constant(module, "ONE", "test_ONE", i32, one)));

    function.blocks[entry.value].values = {condition};
    function.blocks[entry.value].terminator.kind = TerminatorKind::cond_branch;
    function.blocks[entry.value].terminator.condition = condition;
    function.blocks[entry.value].terminator.then_target = then_block;
    function.blocks[entry.value].terminator.else_target = else_block;
    function.blocks[then_block.value].values = {one};
    function.blocks[then_block.value].terminator.kind = TerminatorKind::branch;
    function.blocks[then_block.value].terminator.target = merge;
    function.blocks[else_block.value].values = {two};
    function.blocks[else_block.value].terminator.kind = TerminatorKind::branch;
    function.blocks[else_block.value].terminator.target = merge;
    function.blocks[merge.value].values = {call_id, aggregate_id, phi_id};
    function.blocks[merge.value].terminator.kind = TerminatorKind::return_;
    function.blocks[merge.value].terminator.value = phi_id;
    append_function(module, function);

    ir::ModuleAnalysisManager manager;
    const ir::ControlFlowGraphAnalysis& cfg = manager.control_flow_graph(module);
    ASSERT_TRUE(manager.is_cached(ir::AnalysisId::control_flow_graph));
    ASSERT_EQ(cfg.functions.size(), 1U);
    const ir::FunctionControlFlowGraph* graph = cfg.find_function(FunctionId{0});
    ASSERT_NE(graph, nullptr);
    ASSERT_EQ(graph->blocks.size(), 4U);
    expect_block_ids(graph->blocks[entry.value].successors, {then_block.value, else_block.value});
    expect_block_ids(graph->blocks[then_block.value].predecessors, {entry.value});
    expect_block_ids(graph->blocks[merge.value].predecessors, {then_block.value, else_block.value});
    EXPECT_EQ(cfg.find_function(INVALID_FUNCTION_ID), nullptr);

    const ir::DominanceAnalysis& dominance = manager.dominance(module);
    EXPECT_TRUE(manager.is_cached(ir::AnalysisId::dominance));
    EXPECT_TRUE(dominance.dominates(FunctionId{0}, entry, merge));
    EXPECT_TRUE(dominance.dominates(FunctionId{0}, merge, merge));
    EXPECT_FALSE(dominance.dominates(FunctionId{0}, then_block, merge));
    EXPECT_FALSE(dominance.dominates(INVALID_FUNCTION_ID, entry, merge));

    const ir::ValueUseAnalysis& value_uses = manager.value_uses(module);
    ASSERT_TRUE(manager.is_cached(ir::AnalysisId::value_uses));
    const std::span<const ir::ValueUseSite> condition_uses = value_uses.uses(condition);
    ASSERT_EQ(condition_uses.size(), 1U);
    EXPECT_EQ(condition_uses[0].kind, ir::ValueUseSiteKind::terminator_operand);
    EXPECT_EQ(condition_uses[0].block.value, entry.value);

    const std::span<const ir::ValueUseSite> one_uses = value_uses.uses(one);
    ASSERT_EQ(one_uses.size(), 4U);
    EXPECT_EQ(one_uses[0].kind, ir::ValueUseSiteKind::global_initializer);
    EXPECT_EQ(one_uses[1].kind, ir::ValueUseSiteKind::value_operand);
    EXPECT_EQ(one_uses[1].user.value, call_id.value);
    EXPECT_EQ(one_uses[2].kind, ir::ValueUseSiteKind::value_operand);
    EXPECT_EQ(one_uses[2].user.value, aggregate_id.value);
    EXPECT_EQ(one_uses[3].kind, ir::ValueUseSiteKind::phi_input);
    EXPECT_EQ(one_uses[3].user.value, phi_id.value);

    const std::span<const ir::ValueUseSite> phi_uses = value_uses.uses(phi_id);
    ASSERT_EQ(phi_uses.size(), 1U);
    EXPECT_EQ(phi_uses[0].kind, ir::ValueUseSiteKind::terminator_operand);
    EXPECT_EQ(value_uses.uses(INVALID_VALUE_ID).size(), 0U);
    EXPECT_EQ(value_uses.uses(ValueId{ValueId::INVALID_VALUE - 1U}).size(), 0U);
    EXPECT_FALSE(manager.is_cached(ir::AnalysisId::type_table));
    EXPECT_FALSE(manager.is_cached(ir::AnalysisId::symbol_table));
    EXPECT_FALSE(manager.is_cached(ir::AnalysisId::record_layouts));

    const ir::PreservedAnalyses cached = manager.cached_analyses();
    EXPECT_TRUE(cached.preserves(ir::AnalysisId::control_flow_graph));
    EXPECT_TRUE(cached.preserves(ir::AnalysisId::dominance));
    EXPECT_TRUE(cached.preserves(ir::AnalysisId::value_uses));
}

TEST(CoreUnit, AnalysisManagerHandlesInvalidEdgesUnreachableBlocksAndEmptyFunctions)
{
    Module module;
    const TypeHandle i32 = builtin(module, BuiltinType::i32);
    Function function = make_function(module, "edge_shapes", i32);
    FunctionBuilder builder{module, function};

    const BlockId entry = builder.block("entry");
    const BlockId exit = builder.block("exit");
    const BlockId unreachable_to_exit = builder.block("unreachable_to_exit");
    const BlockId invalid_branch = builder.block("invalid_branch");
    const BlockId none_block = builder.block("none");
    const ValueId condition = builder.add(bool_value(module, true));
    const ValueId result = builder.add(integer_value(module, i32, "0"));

    function.blocks[entry.value].values = {condition};
    function.blocks[entry.value].terminator.kind = TerminatorKind::cond_branch;
    function.blocks[entry.value].terminator.condition = condition;
    function.blocks[entry.value].terminator.then_target = exit;
    function.blocks[entry.value].terminator.else_target = exit;
    function.blocks[exit.value].values = {result};
    function.blocks[exit.value].terminator.kind = TerminatorKind::return_;
    function.blocks[exit.value].terminator.value = result;
    function.blocks[unreachable_to_exit.value].terminator.kind = TerminatorKind::branch;
    function.blocks[unreachable_to_exit.value].terminator.target = exit;
    function.blocks[invalid_branch.value].terminator.kind = TerminatorKind::branch;
    function.blocks[invalid_branch.value].terminator.target = INVALID_BLOCK_ID;
    function.blocks[none_block.value].terminator.kind = TerminatorKind::none;
    function.blocks[none_block.value].values = {ValueId{ValueId::INVALID_VALUE - 1U}};
    append_function(module, function);
    append_function(module, make_function(module, "empty", i32));

    ir::ModuleAnalysisManager manager;
    const ir::ControlFlowGraphAnalysis& cfg = manager.control_flow_graph(module);
    ASSERT_EQ(cfg.functions.size(), 2U);
    const ir::FunctionControlFlowGraph* graph = cfg.find_function(FunctionId{0});
    ASSERT_NE(graph, nullptr);
    expect_block_ids(graph->blocks[entry.value].successors, {exit.value});
    expect_block_ids(graph->blocks[exit.value].predecessors, {entry.value, unreachable_to_exit.value});
    EXPECT_TRUE(graph->blocks[invalid_branch.value].successors.empty());
    EXPECT_TRUE(graph->blocks[none_block.value].successors.empty());

    const ir::DominanceAnalysis& dominance = manager.dominance(module);
    EXPECT_TRUE(dominance.dominates(FunctionId{0}, entry, exit));
    EXPECT_FALSE(dominance.dominates(FunctionId{0}, unreachable_to_exit, exit));
    EXPECT_TRUE(dominance.dominates(FunctionId{0}, invalid_branch, invalid_branch));
    const ir::FunctionDominanceAnalysis* empty_dominance = dominance.find_function(FunctionId{1});
    ASSERT_NE(empty_dominance, nullptr);
    EXPECT_TRUE(empty_dominance->dominators.empty());

    const ir::ValueUseAnalysis& value_uses = manager.value_uses(module);
    EXPECT_EQ(value_uses.uses(result).size(), 1U);

    Module empty_module;
    const ir::ControlFlowGraphAnalysis& empty_cfg = manager.control_flow_graph(empty_module);
    EXPECT_TRUE(empty_cfg.functions.empty());
    EXPECT_FALSE(manager.is_cached(static_cast<ir::AnalysisId>(ValueId::INVALID_VALUE)));
}

TEST(CoreUnit, AnalysisManagerInvalidatesByPreservedAnalyses)
{
    Module module = make_simple_module();
    ir::ModuleAnalysisManager manager;
    static_cast<void>(manager.control_flow_graph(module));
    static_cast<void>(manager.value_uses(module));
    static_cast<void>(manager.dominance(module));

    ir::PreservedAnalyses preserve_cfg = ir::PreservedAnalyses::none();
    preserve_cfg.preserve(ir::AnalysisId::control_flow_graph);
    manager.invalidate(preserve_cfg);
    EXPECT_TRUE(manager.is_cached(ir::AnalysisId::control_flow_graph));
    EXPECT_FALSE(manager.is_cached(ir::AnalysisId::dominance));
    EXPECT_FALSE(manager.is_cached(ir::AnalysisId::value_uses));

    manager.invalidate(ir::PreservedAnalyses::all());
    EXPECT_TRUE(manager.is_cached(ir::AnalysisId::control_flow_graph));

    manager.clear();
    EXPECT_FALSE(manager.is_cached(ir::AnalysisId::control_flow_graph));
    EXPECT_FALSE(manager.cached_analyses().preserves(ir::AnalysisId::control_flow_graph));
}

TEST(CoreUnit, PassManagerInvalidatesCachedAnalysesAfterChangedPasses)
{
    {
        Module module = make_simple_module();
        ir::ModuleAnalysisManager analyses;
        ir::ModulePassManager manager;
        manager.add(ir::ModulePass{
            ir::PassId::custom,
            "test.cache_and_preserve_cfg",
            cache_analyses_and_preserve_cfg_test_pass,
        });

        const auto result = manager.run(module, ir::VerifierGate(ir::VerifierGateOptions{}), analyses);
        ASSERT_TRUE(result) << result.error().message;
        EXPECT_TRUE(result.value().changed);
        EXPECT_TRUE(result.value().preserved_analyses.preserves(ir::AnalysisId::control_flow_graph));
        EXPECT_TRUE(analyses.is_cached(ir::AnalysisId::control_flow_graph));
        EXPECT_FALSE(analyses.is_cached(ir::AnalysisId::dominance));
        EXPECT_FALSE(analyses.is_cached(ir::AnalysisId::value_uses));
    }
    {
        Module module = make_simple_module();
        ir::ModuleAnalysisManager analyses;
        static_cast<void>(analyses.value_uses(module));
        ir::ModulePassManager manager;
        manager.add(ir::ModulePass{
            ir::PassId::custom,
            "test.unchanged_none",
            unchanged_invalidate_none_test_pass,
        });

        const auto result = manager.run(module, ir::VerifierGate(ir::VerifierGateOptions{}), analyses);
        ASSERT_TRUE(result) << result.error().message;
        EXPECT_FALSE(result.value().changed);
        EXPECT_TRUE(analyses.is_cached(ir::AnalysisId::value_uses));
    }
}

} // namespace aurex::test
