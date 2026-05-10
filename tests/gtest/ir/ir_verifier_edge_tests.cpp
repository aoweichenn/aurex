#include <aurex/ir/verify.hpp>
#include <gtest/support/ir_test_helpers.hpp>

namespace aurex::test {
namespace {

using namespace irtest;

} // namespace

TEST(CoreUnit, IrVerifierReportsAdditionalEdgeCaseErrors) {
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        [[maybe_unused]] const GlobalConstantId constant =
            add_global_constant(module, GlobalConstant {"missing", "unit_missing", i32, INVALID_VALUE_ID});
        expect_error_contains(ir::verify_module(module), "constant initializer value id is invalid");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        Value load;
        load.kind = ValueKind::load;
        load.type = i32;
        [[maybe_unused]] const GlobalConstantId constant =
            add_global_constant(module, GlobalConstant {"runtime", "unit_runtime", i32, add_value(module, load)});
        expect_error_contains(ir::verify_module(module), "constant initializer is not compile-time constant");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        Value aggregate;
        aggregate.kind = ValueKind::aggregate;
        aggregate.type = i32;
        [[maybe_unused]] const GlobalConstantId constant =
            add_global_constant(module, GlobalConstant {"not_record", "unit_not_record", i32, add_value(module, aggregate)});
        expect_error_contains(ir::verify_module(module), "aggregate result is not a record");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        Function function = make_function(module, "bad_entry_signature", i32);
        function.is_entry = true;
        function.call_conv = AbiCallConv::c;
        FunctionBuilder builder {module, function};
        Value param;
        param.kind = ValueKind::param;
        param.type = i32;
        const ValueId param_id = builder.add(param);
        const ValueId result = builder.add(integer_value(i32, "0"));
        function.signature_params.push_back(FunctionParam {"value", i32});
        function.param_values.push_back(param_id);
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {param_id, result};
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = result;
        module.functions.push_back(function);
        const auto verify = ir::verify_module(module);
        ASSERT_FALSE(verify);
        expect_contains_all(verify.error().message, {
            "must use Aurex ABI",
            "must use no parameters or argc/argv parameters",
        });
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        Function function = make_function(module, "param_count", i32);
        function.signature_params.push_back(FunctionParam {"value", i32});
        FunctionBuilder builder {module, function};
        const ValueId result = builder.add(integer_value(i32, "0"));
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {result};
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = result;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "parameter signature/value count mismatch");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        Function function = make_function(module, "invalid_value", i32);
        FunctionBuilder builder {module, function};
        const ValueId result = builder.add(integer_value(i32, "0"));
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {ValueId {42}, result};
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = result;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "invalid value in block");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        Function function = make_function(module, "unresolved_call", i32);
        FunctionBuilder builder {module, function};
        Value call;
        call.kind = ValueKind::call;
        call.type = i32;
        const ValueId call_id = builder.add(call);
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {call_id};
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = call_id;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "call has no target symbol");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        Function function = make_function(module, "call_out_of_range", i32);
        FunctionBuilder builder {module, function};
        Value call;
        call.kind = ValueKind::call;
        call.type = i32;
        call.call_target = FunctionId {9};
        const ValueId call_id = builder.add(call);
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {call_id};
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = call_id;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "call target out of range");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        Function first = make_function(module, "extern_same_a", i32, Linkage::extern_c, AbiCallConv::c);
        first.signature_params.push_back(FunctionParam {"value", i32});
        Function second = make_function(module, "extern_same_b", i32, Linkage::extern_c, AbiCallConv::c);
        second.symbol = first.symbol;
        second.signature_params.push_back(FunctionParam {"value", i32});
        module.functions.push_back(first);
        module.functions.push_back(second);
        EXPECT_TRUE(ir::verify_module(module));
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        Function function = make_function(module, "bad_phi_edge", i32);
        FunctionBuilder builder {module, function};
        const ValueId one = builder.add(integer_value(i32, "1"));
        const ValueId two = builder.add(integer_value(i32, "2"));
        Value phi;
        phi.kind = ValueKind::phi;
        phi.type = i32;
        const ValueId phi_id = builder.add(phi);

        const BlockId entry = builder.block("entry");
        const BlockId dead = builder.block("dead");
        const BlockId join = builder.block("join");
        function.blocks[entry.value].values = {one};
        function.blocks[entry.value].terminator.kind = TerminatorKind::branch;
        function.blocks[entry.value].terminator.target = join;
        function.blocks[dead.value].values = {two};
        function.blocks[dead.value].terminator.kind = TerminatorKind::return_;
        function.blocks[dead.value].terminator.value = two;
        module.values[phi_id.value].incoming = {
            PhiInput {entry, one},
            PhiInput {dead, two},
        };
        function.blocks[join.value].values = {phi_id};
        function.blocks[join.value].terminator.kind = TerminatorKind::return_;
        function.blocks[join.value].terminator.value = phi_id;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "phi predecessor has no edge to block");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        Function function = make_function(module, "missing_phi_incoming", i32);
        FunctionBuilder builder {module, function};
        const ValueId one = builder.add(integer_value(i32, "1"));
        const ValueId two = builder.add(integer_value(i32, "2"));
        Value phi;
        phi.kind = ValueKind::phi;
        phi.type = i32;
        const ValueId phi_id = builder.add(phi);

        const BlockId entry = builder.block("entry");
        const BlockId other = builder.block("other");
        const BlockId join = builder.block("join");
        function.blocks[entry.value].values = {one};
        function.blocks[entry.value].terminator.kind = TerminatorKind::branch;
        function.blocks[entry.value].terminator.target = join;
        function.blocks[other.value].values = {two};
        function.blocks[other.value].terminator.kind = TerminatorKind::branch;
        function.blocks[other.value].terminator.target = join;
        module.values[phi_id.value].incoming = {PhiInput {entry, one}};
        function.blocks[join.value].values = {phi_id};
        function.blocks[join.value].terminator.kind = TerminatorKind::return_;
        function.blocks[join.value].terminator.value = phi_id;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "phi is missing incoming predecessor");
    }
}

TEST(CoreUnit, IrVerifierReportsRuntimeShapeErrors) {
    Module module;
    const TypeHandle i32 = builtin(module, BuiltinType::i32);
    const TypeHandle bool_type = builtin(module, BuiltinType::bool_);
    const TypeHandle ptr_i32 = ptr(module, PointerMutability::mut, i32);
    const TypeHandle pair_type = module.types.named_struct("unit.Pair", "unit_Pair", false);
    module.records.push_back(RecordLayout {
        pair_type,
        "unit.Pair",
        "unit_Pair",
        false,
        {RecordField {"left", i32}, RecordField {"right", i32}},
    });

    Value constant_value = integer_value(i32, "1");
    const GlobalConstantId constant =
        add_global_constant(module, GlobalConstant {"answer", "unit_answer", i32, add_value(module, constant_value)});
    Value bool_ref;
    bool_ref.kind = ValueKind::constant_ref;
    bool_ref.type = bool_type;
    bool_ref.constant = constant;
    [[maybe_unused]] const ValueId bool_ref_id = add_value(module, bool_ref);

    Function variadic = make_function(module, "variadic", i32, Linkage::extern_c, AbiCallConv::c);
    variadic.symbol = "unit_variadic";
    variadic.is_variadic = true;
    variadic.signature_params.push_back(FunctionParam {"fixed", i32});
    module.functions.push_back(variadic);
    const FunctionId variadic_id {0};

    Function function = make_function(module, "bad_shapes", i32);
    FunctionBuilder builder {module, function};
    const ValueId one = builder.add(integer_value(i32, "1"));
    Value call;
    call.kind = ValueKind::call;
    call.type = i32;
    call.call_target = variadic_id;
    call.args = {one, ValueId {999}};
    const ValueId call_id = builder.add(call);

    Value constant_ref = bool_ref;
    const ValueId bad_constant_ref = builder.add(constant_ref);

    Value aggregate_non_record;
    aggregate_non_record.kind = ValueKind::aggregate;
    aggregate_non_record.type = i32;
    aggregate_non_record.fields = {{"value", one}};
    const ValueId aggregate_non_record_id = builder.add(aggregate_non_record);

    Value aggregate_unknown;
    aggregate_unknown.kind = ValueKind::aggregate;
    aggregate_unknown.type = pair_type;
    aggregate_unknown.fields = {{"missing", one}};
    const ValueId aggregate_unknown_id = builder.add(aggregate_unknown);

    Value load;
    load.kind = ValueKind::load;
    load.type = i32;
    load.object = ValueId {1000};
    const ValueId bad_load = builder.add(load);

    Value store;
    store.kind = ValueKind::store;
    store.type = builtin(module, BuiltinType::void_);
    store.object = one;
    store.lhs = ValueId {1001};
    const ValueId bad_store = builder.add(store);

    Value field;
    field.kind = ValueKind::field_addr;
    field.type = ptr_i32;
    field.object = one;
    field.name = "missing";
    const ValueId bad_field = builder.add(field);

    const BlockId entry = builder.block("entry");
    function.blocks[entry.value].values = {
        call_id,
        bad_constant_ref,
        aggregate_non_record_id,
        aggregate_unknown_id,
        bad_load,
        bad_store,
        bad_field,
        one,
    };
    function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
    function.blocks[entry.value].terminator.value = one;
    module.functions.push_back(function);

    const auto verify = ir::verify_module(module);
    ASSERT_FALSE(verify);
    expect_contains_all(verify.error().message, {
        "call argument out of range",
        "constant reference type mismatch",
        "aggregate result is not a record",
        "unknown aggregate field missing",
        "load object value id is invalid",
        "store target is not a pointer",
        "store source value id is invalid",
        "unknown field 'missing'",
    });
}

} // namespace aurex::test
