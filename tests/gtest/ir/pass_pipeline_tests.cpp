#include "aurex/ir/pass_pipeline.hpp"
#include "gtest/support/ir_test_helpers.hpp"

namespace aurex::test {
namespace {

using namespace irtest;

} // namespace

TEST(CoreUnit, PassPipelineOptimizesAndReportsVerificationFailures) {
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const TypeHandle ptr_i32 = ptr(module, PointerMutability::mut, i32);
        Function function = make_function(module, "local_slot", i32);
        FunctionBuilder builder {module, function};
        Value slot;
        slot.kind = ValueKind::alloca;
        slot.type = ptr_i32;
        const ValueId slot_id = builder.add(slot);
        const ValueId one = builder.add(integer_value(i32, "1"));
        Value store;
        store.kind = ValueKind::store;
        store.type = builtin(module, BuiltinType::void_);
        store.object = slot_id;
        store.lhs = one;
        const ValueId store_id = builder.add(store);
        Value load;
        load.kind = ValueKind::load;
        load.type = i32;
        load.object = slot_id;
        const ValueId load_id = builder.add(load);
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {slot_id, one, store_id, load_id};
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = load_id;
        module.functions.push_back(function);

        PassPipelineOptions options;
        options.optimization_level = ir::OptimizationLevel::basic;
        ASSERT_TRUE(ir::run_pass_pipeline(module, options));
        const BasicBlock& block = module.functions[0].blocks[0];
        EXPECT_EQ(block.values.size(), 1U);
        EXPECT_EQ(block.values[0].value, one.value);
        EXPECT_EQ(block.terminator.value.value, one.value);
        EXPECT_EQ(ir::optimization_level_name(ir::OptimizationLevel::none), "O0");
        EXPECT_EQ(ir::optimization_level_name(ir::OptimizationLevel::basic), "O1");
        EXPECT_EQ(ir::optimization_level_name(ir::OptimizationLevel::standard), "O2");
        EXPECT_EQ(ir::optimization_level_name(ir::OptimizationLevel::aggressive), "O3");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        Function function = make_function(module, "same_target", i32);
        FunctionBuilder builder {module, function};
        const ValueId condition = builder.add(bool_value(module, true));
        const ValueId result = builder.add(integer_value(i32, "3"));
        const BlockId entry = builder.block("entry");
        const BlockId exit = builder.block("exit");
        function.blocks[entry.value].values = {condition};
        function.blocks[entry.value].terminator.kind = TerminatorKind::cond_branch;
        function.blocks[entry.value].terminator.condition = condition;
        function.blocks[entry.value].terminator.then_target = exit;
        function.blocks[entry.value].terminator.else_target = exit;
        function.blocks[exit.value].values = {result};
        function.blocks[exit.value].terminator.kind = TerminatorKind::return_;
        function.blocks[exit.value].terminator.value = result;
        module.functions.push_back(function);

        PassPipelineOptions options;
        options.optimization_level = ir::OptimizationLevel::basic;
        ASSERT_TRUE(ir::run_pass_pipeline(module, options));
        EXPECT_EQ(module.functions[0].blocks[0].terminator.kind, TerminatorKind::branch);
        EXPECT_EQ(module.functions[0].blocks[0].terminator.target.value, exit.value);
    }
    {
        Module module = make_simple_module();
        module.functions[0].blocks[0].terminator.value = invalid_value_id;
        PassPipelineOptions options;
        expect_error_contains(ir::run_pass_pipeline(module, options), "return value value id is invalid");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const TypeHandle record_type = module.types.named_struct("unit.Record", "unit_Record", false);
        const TypeHandle ptr_record = ptr(module, PointerMutability::mut, record_type);
        module.types.set_record_properties(record_type, false, true);
        module.records.push_back(RecordLayout {
            record_type,
            "unit.Record",
            "unit_Record",
            false,
            {RecordField {"x", i32}},
        });
        Function function = make_function(module, "escape_uses", i32);
        FunctionBuilder builder {module, function};
        Value slot;
        slot.kind = ValueKind::alloca;
        slot.type = ptr_record;
        const ValueId slot_id = builder.add(slot);
        Value field;
        field.kind = ValueKind::field_addr;
        field.type = ptr(module, PointerMutability::mut, i32);
        field.object = slot_id;
        field.name = "x";
        const ValueId field_id = builder.add(field);
        Value aggregate;
        aggregate.kind = ValueKind::aggregate;
        aggregate.type = record_type;
        aggregate.fields.push_back({"x", builder.add(integer_value(i32, "1"))});
        const ValueId aggregate_id = builder.add(aggregate);
        Value cast;
        cast.kind = ValueKind::cast;
        cast.type = ptr_record;
        cast.target_type = ptr_record;
        cast.cast_kind = CastKind::pointer;
        cast.lhs = slot_id;
        const ValueId cast_id = builder.add(cast);
        const ValueId result = builder.add(integer_value(i32, "0"));
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {
            slot_id,
            field_id,
            aggregate.fields[0].value,
            aggregate_id,
            cast_id,
            result,
        };
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = result;
        module.functions.push_back(function);

        PassPipelineOptions options;
        options.optimization_level = ir::OptimizationLevel::basic;
        ASSERT_TRUE(ir::run_pass_pipeline(module, options));
        EXPECT_EQ(module.functions[0].blocks[0].values.size(), 6U);
    }
    {
        Module module;
        Function function = make_function(module, "empty", builtin(module, BuiltinType::i32), Linkage::extern_c, AbiCallConv::c);
        module.functions.push_back(function);
        PassPipelineOptions options;
        options.optimization_level = ir::OptimizationLevel::basic;
        ASSERT_TRUE(ir::run_pass_pipeline(module, options));
    }
}

TEST(CoreUnit, PassPipelineRemovesUnreachableBlocksAndRewritesPhiInputs) {
    Module module;
    const TypeHandle i32 = builtin(module, BuiltinType::i32);
    Function function = make_function(module, "unreachable_phi", i32);
    FunctionBuilder builder {module, function};
    const ValueId one = builder.add(integer_value(i32, "1"));
    const ValueId two = builder.add(integer_value(i32, "2"));
    const BlockId entry = builder.block("entry");
    const BlockId dead = builder.block("dead");
    const BlockId join = builder.block("join");
    function.blocks[entry.value].values = {one};
    function.blocks[entry.value].terminator.kind = TerminatorKind::branch;
    function.blocks[entry.value].terminator.target = join;
    function.blocks[dead.value].values = {two};
    function.blocks[dead.value].terminator.kind = TerminatorKind::branch;
    function.blocks[dead.value].terminator.target = join;
    Value phi;
    phi.kind = ValueKind::phi;
    phi.type = i32;
    phi.incoming = {PhiInput {entry, one}, PhiInput {dead, two}};
    const ValueId phi_id = builder.add(phi);
    function.blocks[join.value].values = {phi_id};
    function.blocks[join.value].terminator.kind = TerminatorKind::return_;
    function.blocks[join.value].terminator.value = phi_id;
    module.functions.push_back(function);

    PassPipelineOptions options;
    options.optimization_level = ir::OptimizationLevel::basic;
    ASSERT_TRUE(ir::run_pass_pipeline(module, options));
    ASSERT_EQ(module.functions[0].blocks.size(), 2U);
    const Value& rewritten_phi = module.values[phi_id.value];
    ASSERT_EQ(rewritten_phi.incoming.size(), 1U);
    EXPECT_EQ(rewritten_phi.incoming[0].predecessor.value, 0U);
}

} // namespace aurex::test
