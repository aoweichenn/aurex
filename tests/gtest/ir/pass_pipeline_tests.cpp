#include <aurex/ir/analysis_manager.hpp>
#include <aurex/ir/pass_pipeline.hpp>

#include <utility>

#include <gtest/support/ir_test_helpers.hpp>

namespace aurex::test {
namespace {

using namespace irtest;

constexpr int PASS_MANAGER_UNKNOWN_PASS_ID = 99;

[[nodiscard]] base::Result<ir::PassResult> preserve_type_table_test_pass(
    Module& module, ir::ModuleAnalysisManager& analyses)
{
    static_cast<void>(module);
    static_cast<void>(analyses);
    ir::PreservedAnalyses preserved = ir::PreservedAnalyses::none();
    preserved.preserve(ir::AnalysisId::type_table);
    return base::Result<ir::PassResult>::ok(ir::PassResult::changed_result(preserved));
}

[[nodiscard]] base::Result<ir::PassResult> unchanged_test_pass(Module& module, ir::ModuleAnalysisManager& analyses)
{
    static_cast<void>(module);
    static_cast<void>(analyses);
    return base::Result<ir::PassResult>::ok(ir::PassResult::unchanged());
}

[[nodiscard]] base::Result<ir::PassResult> invalidate_return_value_test_pass(
    Module& module, ir::ModuleAnalysisManager& analyses)
{
    static_cast<void>(analyses);
    module.functions[0].blocks[0].terminator.value = INVALID_VALUE_ID;
    return base::Result<ir::PassResult>::ok(ir::PassResult::changed_result(ir::PreservedAnalyses::none()));
}

} // namespace

TEST(CoreUnit, PassManagerTracksAnalysesAndVerifierGate)
{
    {
        ir::PreservedAnalyses all = ir::PreservedAnalyses::all();
        EXPECT_TRUE(all.preserves_all());
        EXPECT_FALSE(all.preserves_none());
        EXPECT_TRUE(all.preserves(ir::AnalysisId::control_flow_graph));

        ir::PreservedAnalyses type_only = ir::PreservedAnalyses::none();
        EXPECT_TRUE(type_only.preserves_none());
        type_only.preserve(ir::AnalysisId::type_table);
        EXPECT_TRUE(type_only.preserves(ir::AnalysisId::type_table));
        EXPECT_FALSE(type_only.preserves(ir::AnalysisId::control_flow_graph));

        all.intersect(type_only);
        EXPECT_FALSE(all.preserves_all());
        EXPECT_TRUE(all.preserves(ir::AnalysisId::type_table));
        EXPECT_FALSE(all.preserves(ir::AnalysisId::control_flow_graph));
        all.intersect(ir::PreservedAnalyses::none());
        EXPECT_TRUE(all.preserves_none());

        EXPECT_EQ(ir::pass_id_name(ir::PassId::local_mem2reg), "local_mem2reg");
        EXPECT_EQ(ir::pass_id_name(ir::PassId::cfg_cleanup), "cfg_cleanup");
        EXPECT_EQ(ir::pass_id_name(ir::PassId::custom), "custom");
        EXPECT_EQ(ir::pass_id_name(static_cast<ir::PassId>(PASS_MANAGER_UNKNOWN_PASS_ID)), "unknown");
    }
    {
        Module module = make_simple_module();
        ir::ModulePassManager manager;
        manager.add(ir::ModulePass{ir::PassId::custom, "test.preserve_type_table", preserve_type_table_test_pass});
        manager.add(ir::ModulePass{ir::PassId::custom, "test.unchanged", unchanged_test_pass});
        ASSERT_EQ(manager.passes().size(), 2U);

        ir::VerifierGate verifier(ir::VerifierGateOptions{
            true,
            true,
            true,
        });
        auto result = manager.run(module, verifier);
        ASSERT_TRUE(result) << result.error().message;
        EXPECT_EQ(result.value().scheduled_pass_count, 2U);
        EXPECT_EQ(result.value().executed_pass_count, 2U);
        EXPECT_TRUE(result.value().changed);
        EXPECT_TRUE(result.value().preserved_analyses.preserves(ir::AnalysisId::type_table));
        EXPECT_FALSE(result.value().preserved_analyses.preserves(ir::AnalysisId::control_flow_graph));
    }
    {
        Module module = make_simple_module();
        ir::ModulePassManager manager;
        manager.add(ir::ModulePass{ir::PassId::custom, "test.invalidate_return", invalidate_return_value_test_pass});
        ir::VerifierGate verifier(ir::VerifierGateOptions{
            true,
            true,
            true,
        });
        const auto result = manager.run(module, verifier);
        ASSERT_FALSE(result);
        expect_contains_all(result.error().message,
            {
                "test.invalidate_return",
                "return value value id is invalid",
            });
    }
    {
        Module module = make_simple_module();
        ir::ModulePassManager manager;
        manager.add(ir::ModulePass{ir::PassId::custom, "test.invalid", nullptr});
        const auto result = manager.run(module, ir::VerifierGate(ir::VerifierGateOptions{}));
        ASSERT_FALSE(result);
        expect_contains(result.error().message, "invalid pass");
    }
}

TEST(CoreUnit, PassPipelineSummaryExposesScheduledPassesAndPreservedAnalyses)
{
    {
        Module module = make_simple_module();
        PassPipelineOptions options;
        options.optimization_level = ir::OptimizationLevel::none;
        auto result = ir::run_pass_pipeline_with_summary(module, options);
        ASSERT_TRUE(result) << result.error().message;
        EXPECT_EQ(result.value().scheduled_pass_count, 0U);
        EXPECT_EQ(result.value().executed_pass_count, 0U);
        EXPECT_FALSE(result.value().changed);
        EXPECT_TRUE(result.value().preserved_analyses.preserves_all());
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const TypeHandle ptr_i32 = ptr(module, PointerMutability::mut, i32);
        Function function = make_function(module, "summary_slot", i32);
        FunctionBuilder builder{module, function};
        Value slot = module.make_value();
        slot.kind = ValueKind::alloca;
        slot.type = ptr_i32;
        const ValueId slot_id = builder.add(slot);
        const ValueId one = builder.add(integer_value(module, i32, "1"));
        Value store = module.make_value();
        store.kind = ValueKind::store;
        store.type = builtin(module, BuiltinType::void_);
        store.object = slot_id;
        store.lhs = one;
        const ValueId store_id = builder.add(store);
        Value load = module.make_value();
        load.kind = ValueKind::load;
        load.type = i32;
        load.object = slot_id;
        const ValueId load_id = builder.add(load);
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {slot_id, one, store_id, load_id};
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = load_id;
        append_function(module, function);

        PassPipelineOptions options;
        options.optimization_level = ir::OptimizationLevel::basic;
        auto result = ir::run_pass_pipeline_with_summary(module, options);
        ASSERT_TRUE(result) << result.error().message;
        EXPECT_EQ(result.value().scheduled_pass_count, 2U);
        EXPECT_EQ(result.value().executed_pass_count, 2U);
        EXPECT_TRUE(result.value().changed);
        EXPECT_TRUE(result.value().preserved_analyses.preserves(ir::AnalysisId::control_flow_graph));
        EXPECT_TRUE(result.value().preserved_analyses.preserves(ir::AnalysisId::type_table));
        EXPECT_FALSE(result.value().preserved_analyses.preserves_all());
    }
    {
        Module module = make_simple_module();
        PassPipelineOptions options;
        options.optimization_level = ir::OptimizationLevel::basic;
        options.enable_mem2reg = false;
        options.enable_cfg_cleanup = true;
        auto result = ir::run_pass_pipeline_with_summary(module, options);
        ASSERT_TRUE(result) << result.error().message;
        EXPECT_EQ(result.value().scheduled_pass_count, 1U);
        EXPECT_EQ(result.value().executed_pass_count, 1U);
    }
}

TEST(CoreUnit, PassPipelineOptimizesAndReportsVerificationFailures)
{
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const TypeHandle ptr_i32 = ptr(module, PointerMutability::mut, i32);
        Function function = make_function(module, "local_slot", i32);
        FunctionBuilder builder{module, function};
        Value slot = module.make_value();
        slot.kind = ValueKind::alloca;
        slot.type = ptr_i32;
        const ValueId slot_id = builder.add(slot);
        const ValueId one = builder.add(integer_value(module, i32, "1"));
        Value store = module.make_value();
        store.kind = ValueKind::store;
        store.type = builtin(module, BuiltinType::void_);
        store.object = slot_id;
        store.lhs = one;
        const ValueId store_id = builder.add(store);
        Value load = module.make_value();
        load.kind = ValueKind::load;
        load.type = i32;
        load.object = slot_id;
        const ValueId load_id = builder.add(load);
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {slot_id, one, store_id, load_id};
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = load_id;
        append_function(module, function);

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
        FunctionBuilder builder{module, function};
        const ValueId condition = builder.add(bool_value(module, true));
        const ValueId result = builder.add(integer_value(module, i32, "3"));
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
        append_function(module, function);

        PassPipelineOptions options;
        options.optimization_level = ir::OptimizationLevel::basic;
        ASSERT_TRUE(ir::run_pass_pipeline(module, options));
        EXPECT_EQ(module.functions[0].blocks[0].terminator.kind, TerminatorKind::branch);
        EXPECT_EQ(module.functions[0].blocks[0].terminator.target.value, exit.value);
    }
    {
        Module module = make_simple_module();
        module.functions[0].blocks[0].terminator.value = INVALID_VALUE_ID;
        PassPipelineOptions options;
        expect_error_contains(ir::run_pass_pipeline(module, options), "return value value id is invalid");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const TypeHandle record_type = module.types.named_struct("unit.Record", "unit_Record", false);
        const TypeHandle ptr_record = ptr(module, PointerMutability::mut, record_type);
        RecordLayout record = record_layout(module, record_type, "unit.Record", "unit_Record", false);
        record.fields.push_back(record_field(module, "x", i32));
        append_record(module, record);
        Function function = make_function(module, "escape_uses", i32);
        FunctionBuilder builder{module, function};
        Value slot = module.make_value();
        slot.kind = ValueKind::alloca;
        slot.type = ptr_record;
        const ValueId slot_id = builder.add(slot);
        Value field = module.make_value();
        field.kind = ValueKind::field_addr;
        field.type = ptr(module, PointerMutability::mut, i32);
        field.object = slot_id;
        set_name(module, field, "x");
        const ValueId field_id = builder.add(field);
        Value aggregate = module.make_value();
        aggregate.kind = ValueKind::aggregate;
        aggregate.type = record_type;
        aggregate.fields.push_back(field_value(module, "x", builder.add(integer_value(module, i32, "1"))));
        const ValueId aggregate_id = builder.add(aggregate);
        Value cast = module.make_value();
        cast.kind = ValueKind::cast;
        cast.type = ptr_record;
        cast.target_type = ptr_record;
        cast.cast_kind = CastKind::pointer;
        cast.lhs = slot_id;
        const ValueId cast_id = builder.add(cast);
        const ValueId result = builder.add(integer_value(module, i32, "0"));
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
        append_function(module, function);

        PassPipelineOptions options;
        options.optimization_level = ir::OptimizationLevel::basic;
        ASSERT_TRUE(ir::run_pass_pipeline(module, options));
        EXPECT_EQ(module.functions[0].blocks[0].values.size(), 6U);
    }
    {
        Module module;
        Function function =
            make_function(module, "empty", builtin(module, BuiltinType::i32), Linkage::extern_c, AbiCallConv::c);
        append_function(module, function);
        PassPipelineOptions options;
        options.optimization_level = ir::OptimizationLevel::basic;
        ASSERT_TRUE(ir::run_pass_pipeline(module, options));
    }
}

TEST(CoreUnit, PassPipelineRemovesUnreachableBlocksAndRewritesPhiInputs)
{
    Module module;
    const TypeHandle i32 = builtin(module, BuiltinType::i32);
    Function function = make_function(module, "unreachable_phi", i32);
    FunctionBuilder builder{module, function};
    const ValueId one = builder.add(integer_value(module, i32, "1"));
    const ValueId two = builder.add(integer_value(module, i32, "2"));
    const BlockId entry = builder.block("entry");
    const BlockId dead = builder.block("dead");
    const BlockId join = builder.block("join");
    function.blocks[entry.value].values = {one};
    function.blocks[entry.value].terminator.kind = TerminatorKind::branch;
    function.blocks[entry.value].terminator.target = join;
    function.blocks[dead.value].values = {two};
    function.blocks[dead.value].terminator.kind = TerminatorKind::branch;
    function.blocks[dead.value].terminator.target = join;
    Value phi = module.make_value();
    phi.kind = ValueKind::phi;
    phi.type = i32;
    phi.incoming = {PhiInput{entry, one}, PhiInput{dead, two}};
    const ValueId phi_id = builder.add(phi);
    function.blocks[join.value].values = {phi_id};
    function.blocks[join.value].terminator.kind = TerminatorKind::return_;
    function.blocks[join.value].terminator.value = phi_id;
    append_function(module, function);

    PassPipelineOptions options;
    options.optimization_level = ir::OptimizationLevel::basic;
    ASSERT_TRUE(ir::run_pass_pipeline(module, options));
    ASSERT_EQ(module.functions[0].blocks.size(), 2U);
    const Value& rewritten_phi = module.values[phi_id.value];
    ASSERT_EQ(rewritten_phi.incoming.size(), 1U);
    EXPECT_EQ(rewritten_phi.incoming[0].predecessor.value, 0U);
}

TEST(CoreUnit, PassPipelineCoversScalarPromotionKindsAndRedirectedBranchMerging)
{
    Module module;
    const TypeHandle bool_type = builtin(module, BuiltinType::bool_);
    const TypeHandle u8 = builtin(module, BuiltinType::u8);
    const TypeHandle u32 = builtin(module, BuiltinType::u32);
    const TypeHandle i32 = builtin(module, BuiltinType::i32);
    const TypeHandle f64 = builtin(module, BuiltinType::f64);
    const TypeHandle usize = builtin(module, BuiltinType::usize);
    const TypeHandle str_type = builtin(module, BuiltinType::str);
    const TypeHandle ptr_bool = ptr(module, PointerMutability::mut, bool_type);
    const TypeHandle ptr_f64 = ptr(module, PointerMutability::mut, f64);
    const TypeHandle ptr_i32 = ptr(module, PointerMutability::mut, i32);
    const TypeHandle ptr_ptr_i32 = ptr(module, PointerMutability::mut, ptr_i32);
    const TypeHandle enum_type = module.types.named_enum("unit.Tag", "unit_Tag");
    module.types.set_enum_underlying(enum_type, u32);
    const TypeHandle ptr_enum = ptr(module, PointerMutability::mut, enum_type);
    const TypeHandle const_u8_ptr = ptr(module, PointerMutability::const_, u8);

    Function function = make_function(module, "scalar_kinds", i32);
    FunctionBuilder builder{module, function};
    Value text_value = module.make_value();
    text_value.kind = ValueKind::string_literal;
    text_value.type = str_type;
    set_text(module, text_value, "\"scalar kinds\"");
    const ValueId string_value = builder.add(text_value);

    Value bool_slot = module.make_value();
    bool_slot.kind = ValueKind::alloca;
    bool_slot.type = ptr_bool;
    const ValueId bool_slot_id = builder.add(bool_slot);
    Value float_slot = module.make_value();
    float_slot.kind = ValueKind::alloca;
    float_slot.type = ptr_f64;
    const ValueId float_slot_id = builder.add(float_slot);
    Value pointer_slot = module.make_value();
    pointer_slot.kind = ValueKind::alloca;
    pointer_slot.type = ptr_ptr_i32;
    const ValueId pointer_slot_id = builder.add(pointer_slot);
    Value enum_slot = module.make_value();
    enum_slot.kind = ValueKind::alloca;
    enum_slot.type = ptr_enum;
    const ValueId enum_slot_id = builder.add(enum_slot);
    Value str_data = module.make_value();
    str_data.kind = ValueKind::str_data;
    str_data.type = const_u8_ptr;
    str_data.object = string_value;
    const ValueId string_data = builder.add(str_data);
    Value str_byte_len = module.make_value();
    str_byte_len.kind = ValueKind::str_byte_len;
    str_byte_len.type = usize;
    str_byte_len.object = string_value;
    const ValueId string_byte_len = builder.add(str_byte_len);
    Value from_bytes = module.make_value();
    from_bytes.kind = ValueKind::str_from_bytes_unchecked;
    from_bytes.type = str_type;
    from_bytes.args = {string_data, string_byte_len};
    const ValueId rebuilt_string = builder.add(from_bytes);
    Value str_slice = module.make_value();
    str_slice.kind = ValueKind::str_slice_checked;
    str_slice.type = str_type;
    str_slice.object = string_value;
    str_slice.lhs = string_byte_len;
    str_slice.rhs = string_byte_len;
    const ValueId sliced_string = builder.add(str_slice);
    const ValueId result = builder.add(integer_value(module, i32, "0"));
    const BlockId entry = builder.block("entry");
    function.blocks[entry.value].values = {
        bool_slot_id,
        float_slot_id,
        pointer_slot_id,
        enum_slot_id,
        string_value,
        string_data,
        string_byte_len,
        rebuilt_string,
        sliced_string,
        result,
    };
    function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
    function.blocks[entry.value].terminator.value = result;
    append_function(module, function);

    PassPipelineOptions options;
    options.optimization_level = ir::OptimizationLevel::basic;
    ASSERT_TRUE(ir::run_pass_pipeline(module, options));
    ASSERT_EQ(module.functions[0].blocks.size(), 1U);
}

TEST(CoreUnit, PassPipelineRecordsSliceAndArrayAggregateUses)
{
    Module module;
    const TypeHandle i32 = builtin(module, BuiltinType::i32);
    const TypeHandle usize = builtin(module, BuiltinType::usize);
    const TypeHandle ptr_i32 = ptr(module, PointerMutability::mut, i32);
    const TypeHandle array_i32 = module.types.array(2, i32);
    const TypeHandle slice_i32 = module.types.slice(PointerMutability::mut, i32);

    Function function = make_function(module, "slice_uses", i32);
    FunctionBuilder builder{module, function};
    Value pointer_slot = module.make_value();
    pointer_slot.kind = ValueKind::alloca;
    pointer_slot.type = ptr(module, PointerMutability::mut, ptr_i32);
    const ValueId pointer_slot_id = builder.add(pointer_slot);
    Value length_slot = module.make_value();
    length_slot.kind = ValueKind::alloca;
    length_slot.type = ptr(module, PointerMutability::mut, usize);
    const ValueId length_slot_id = builder.add(length_slot);
    Value loaded_pointer = module.make_value();
    loaded_pointer.kind = ValueKind::load;
    loaded_pointer.type = ptr_i32;
    loaded_pointer.object = pointer_slot_id;
    const ValueId loaded_pointer_id = builder.add(loaded_pointer);
    Value loaded_length = module.make_value();
    loaded_length.kind = ValueKind::load;
    loaded_length.type = usize;
    loaded_length.object = length_slot_id;
    const ValueId loaded_length_id = builder.add(loaded_length);
    Value slice_value = module.make_value();
    slice_value.kind = ValueKind::slice;
    slice_value.type = slice_i32;
    slice_value.lhs = loaded_pointer_id;
    slice_value.rhs = loaded_length_id;
    const ValueId slice_id = builder.add(slice_value);
    Value slice_data = module.make_value();
    slice_data.kind = ValueKind::slice_data;
    slice_data.type = ptr_i32;
    slice_data.object = slice_id;
    const ValueId slice_data_id = builder.add(slice_data);
    Value slice_len = module.make_value();
    slice_len.kind = ValueKind::slice_len;
    slice_len.type = usize;
    slice_len.object = slice_id;
    const ValueId slice_len_id = builder.add(slice_len);
    Value array_value = module.make_value();
    array_value.kind = ValueKind::aggregate;
    array_value.type = array_i32;
    array_value.elements = {
        builder.add(integer_value(module, i32, "1")),
        builder.add(integer_value(module, i32, "2")),
    };
    const ValueId array_id = builder.add(array_value);
    const ValueId result = builder.add(integer_value(module, i32, "0"));
    const BlockId entry = builder.block("entry");
    function.blocks[entry.value].values = {
        pointer_slot_id,
        length_slot_id,
        loaded_pointer_id,
        loaded_length_id,
        slice_id,
        slice_data_id,
        slice_len_id,
        array_value.elements[0],
        array_value.elements[1],
        array_id,
        result,
    };
    function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
    function.blocks[entry.value].terminator.value = result;
    append_function(module, function);

    PassPipelineOptions options;
    options.optimization_level = ir::OptimizationLevel::basic;
    ASSERT_TRUE(ir::run_pass_pipeline(module, options));
    EXPECT_EQ(module.functions[0].blocks[0].values.size(), 9U);
}

TEST(CoreUnit, PassPipelineMergesEmptyRedirectedBranchesIntoASingleBranch)
{
    Module module;
    const TypeHandle i32 = builtin(module, BuiltinType::i32);
    Function function = make_function(module, "merge_redirects", i32);
    FunctionBuilder builder{module, function};
    const ValueId result = builder.add(integer_value(module, i32, "7"));
    const ValueId condition = builder.add(bool_value(module, true));
    const BlockId entry = builder.block("entry");
    const BlockId then_block = builder.block("then");
    const BlockId else_block = builder.block("else");
    const BlockId join = builder.block("join");
    function.blocks[entry.value].terminator.kind = TerminatorKind::cond_branch;
    function.blocks[entry.value].terminator.condition = condition;
    function.blocks[entry.value].terminator.then_target = then_block;
    function.blocks[entry.value].terminator.else_target = else_block;
    function.blocks[then_block.value].terminator.kind = TerminatorKind::branch;
    function.blocks[then_block.value].terminator.target = join;
    function.blocks[else_block.value].terminator.kind = TerminatorKind::branch;
    function.blocks[else_block.value].terminator.target = join;
    function.blocks[join.value].values = {result};
    function.blocks[join.value].terminator.kind = TerminatorKind::return_;
    function.blocks[join.value].terminator.value = result;
    append_function(module, function);

    PassPipelineOptions options;
    options.optimization_level = ir::OptimizationLevel::basic;
    ASSERT_TRUE(ir::run_pass_pipeline(module, options));
    ASSERT_EQ(module.functions[0].blocks.size(), 2U);
    EXPECT_EQ(module.functions[0].blocks[0].terminator.kind, TerminatorKind::branch);
    EXPECT_EQ(module.functions[0].blocks[0].terminator.target.value, 1U);
}

TEST(CoreUnit, PassPipelineRewritesAggregatePhiAndConstantsAfterMem2Reg)
{
    Module module;
    const TypeHandle i32 = builtin(module, BuiltinType::i32);
    const TypeHandle ptr_i32 = ptr(module, PointerMutability::mut, i32);
    const TypeHandle record_type = module.types.named_struct("unit.Box", "unit_Box", false);
    RecordLayout record = record_layout(module, record_type, "unit.Box", "unit_Box", false);
    record.fields.push_back(record_field(module, "value", i32));
    append_record(module, record);

    Function function = make_function(module, "rewrite_uses", i32);
    FunctionBuilder builder{module, function};
    Value slot = module.make_value();
    slot.kind = ValueKind::alloca;
    slot.type = ptr_i32;
    const ValueId slot_id = builder.add(slot);
    const ValueId one = builder.add(integer_value(module, i32, "1"));
    Value store = module.make_value();
    store.kind = ValueKind::store;
    store.type = builtin(module, BuiltinType::void_);
    store.object = slot_id;
    store.lhs = one;
    const ValueId store_id = builder.add(store);
    Value load = module.make_value();
    load.kind = ValueKind::load;
    load.type = i32;
    load.object = slot_id;
    const ValueId load_id = builder.add(load);
    Value aggregate = module.make_value();
    aggregate.kind = ValueKind::aggregate;
    aggregate.type = record_type;
    aggregate.fields.push_back(field_value(module, "value", load_id));
    const ValueId aggregate_id = builder.add(aggregate);
    Value phi = module.make_value();
    phi.kind = ValueKind::phi;
    phi.type = i32;
    const BlockId entry = builder.block("entry");
    phi.incoming = {PhiInput{entry, load_id}, PhiInput{entry, one}};
    const ValueId phi_id = builder.add(phi);
    function.blocks[entry.value].values = {slot_id, one, store_id, load_id, aggregate_id, phi_id};
    function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
    function.blocks[entry.value].terminator.value = load_id;
    append_function(module, function);
    const GlobalConstantId constant =
        add_global_constant(module, GlobalConstant{"folded", "unit_folded", i32, load_id});

    PassPipelineOptions options;
    options.verify_input = false;
    options.verify_output = false;
    options.optimization_level = ir::OptimizationLevel::basic;
    ASSERT_TRUE(ir::run_pass_pipeline(module, options));
    EXPECT_EQ(module.constants[constant.value].initializer.value, one.value);
    EXPECT_EQ(module.values[aggregate_id.value].fields[0].value.value, one.value);
    EXPECT_EQ(module.values[phi_id.value].incoming[0].value.value, one.value);
    EXPECT_EQ(module.functions[0].blocks[0].terminator.value.value, one.value);
}

TEST(CoreUnit, PassPipelineCoversNonPromotableEscapeAndInvalidValueTolerance)
{
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const TypeHandle ptr_i32 = ptr(module, PointerMutability::mut, i32);
        Function function = make_function(module, "escape_shapes", i32);
        FunctionBuilder builder{module, function};
        Value slot = module.make_value();
        slot.kind = ValueKind::alloca;
        slot.type = ptr_i32;
        const ValueId slot_id = builder.add(slot);
        const ValueId one = builder.add(integer_value(module, i32, "1"));
        Value unary = module.make_value();
        unary.kind = ValueKind::unary;
        unary.type = ptr_i32;
        unary.unary_op = UnaryOp::address_of;
        unary.lhs = slot_id;
        const ValueId unary_id = builder.add(unary);
        Value index = module.make_value();
        index.kind = ValueKind::index_addr;
        index.type = ptr_i32;
        index.object = slot_id;
        index.index = one;
        const ValueId index_id = builder.add(index);
        const ValueId result = builder.add(integer_value(module, i32, "0"));
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {slot_id, one, unary_id, index_id, result};
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = result;
        append_function(module, function);

        PassPipelineOptions options;
        options.optimization_level = ir::OptimizationLevel::basic;
        ASSERT_TRUE(ir::run_pass_pipeline(module, options));
        EXPECT_EQ(module.functions[0].blocks[0].values.size(), 5U);
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const TypeHandle ptr_i32 = ptr(module, PointerMutability::mut, i32);
        Function function = make_function(module, "invalid_kept", i32);
        FunctionBuilder builder{module, function};
        Value slot = module.make_value();
        slot.kind = ValueKind::alloca;
        slot.type = ptr_i32;
        const ValueId slot_id = builder.add(slot);
        const ValueId one = builder.add(integer_value(module, i32, "1"));
        Value store = module.make_value();
        store.kind = ValueKind::store;
        store.type = builtin(module, BuiltinType::void_);
        store.object = slot_id;
        store.lhs = one;
        const ValueId store_id = builder.add(store);
        Value load = module.make_value();
        load.kind = ValueKind::load;
        load.type = i32;
        load.object = slot_id;
        const ValueId load_id = builder.add(load);
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {slot_id, ValueId{999}, one, store_id, load_id};
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = load_id;
        append_function(module, function);

        PassPipelineOptions options;
        options.verify_input = false;
        options.verify_output = false;
        options.optimization_level = ir::OptimizationLevel::basic;
        ASSERT_TRUE(ir::run_pass_pipeline(module, options));
        ASSERT_FALSE(module.functions[0].blocks[0].values.empty());
        EXPECT_EQ(module.functions[0].blocks[0].values.front().value, 999U);
    }
}

TEST(CoreUnit, PassPipelineSkipsEmptyBranchMergeWhenTargetHasPhi)
{
    Module module;
    const TypeHandle i32 = builtin(module, BuiltinType::i32);
    Function function = make_function(module, "phi_target", i32);
    FunctionBuilder builder{module, function};
    const ValueId one = builder.add(integer_value(module, i32, "1"));
    const BlockId entry = builder.block("entry");
    const BlockId empty = builder.block("empty");
    const BlockId join = builder.block("join");
    function.blocks[entry.value].values = {one};
    function.blocks[entry.value].terminator.kind = TerminatorKind::branch;
    function.blocks[entry.value].terminator.target = empty;
    function.blocks[empty.value].terminator.kind = TerminatorKind::branch;
    function.blocks[empty.value].terminator.target = join;
    Value phi = module.make_value();
    phi.kind = ValueKind::phi;
    phi.type = i32;
    phi.incoming = {PhiInput{empty, one}};
    const ValueId phi_id = builder.add(phi);
    function.blocks[join.value].values = {phi_id};
    function.blocks[join.value].terminator.kind = TerminatorKind::return_;
    function.blocks[join.value].terminator.value = phi_id;
    append_function(module, function);

    PassPipelineOptions options;
    options.optimization_level = ir::OptimizationLevel::basic;
    ASSERT_TRUE(ir::run_pass_pipeline(module, options));
    ASSERT_EQ(module.functions[0].blocks.size(), 3U);
    EXPECT_EQ(module.functions[0].blocks[1].terminator.target.value, join.value);
}

} // namespace aurex::test
