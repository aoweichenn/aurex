#include "aurex/ir/verify.hpp"
#include "gtest/support/ir_test_helpers.hpp"

namespace aurex::test {
namespace {

using namespace irtest;

} // namespace

TEST(CoreUnit, IrVerifierReportsRepresentativeStructuralErrors) {
    {
        Module module = make_simple_module();
        EXPECT_TRUE(ir::verify_module(module));
    }
    {
        Module module = make_simple_module();
        module.functions[0].symbol.clear();
        expect_error_contains(ir::verify_module(module), "empty ABI symbol");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        module.functions.push_back(make_function(module, "dup_a", i32));
        module.functions.push_back(make_function(module, "dup_b", i32));
        module.functions[1].symbol = module.functions[0].symbol;
        expect_error_contains(ir::verify_module(module), "duplicate non-extern function ABI symbol");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        module.functions.push_back(make_function(module, "extern_a", i32, Linkage::extern_c, AbiCallConv::c));
        module.functions.push_back(make_function(module, "extern_b", i32, Linkage::extern_c, AbiCallConv::c));
        module.functions[1].symbol = module.functions[0].symbol;
        module.functions[1].signature_params.push_back(FunctionParam {"value", i32});
        expect_error_contains(ir::verify_module(module), "inconsistent declarations");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        Function function = make_function(module, "bad_extern", i32, Linkage::extern_c, AbiCallConv::aurex);
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "must use C ABI");
    }
    {
        Module module;
        Function function = make_function(module, "bad_entry", builtin(module, BuiltinType::i64));
        function.is_entry = true;
        function.linkage = Linkage::export_c;
        module.functions.push_back(function);
        const auto result = ir::verify_module(module);
        ASSERT_FALSE(result);
        expect_contains_all(result.error().message, {
            "must use internal linkage",
            "must return i32 or void",
        });
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        Function function = make_function(module, "bad_params", i32);
        const ValueId param = add_value(module, integer_value(i32, "1"));
        function.signature_params.push_back(FunctionParam {"x", i32});
        function.param_values.push_back(param);
        const BlockId entry = add_block(function, "entry");
        function.blocks[entry.value].values.push_back(param);
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = param;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "non-param value");
    }
    {
        Module module = make_simple_module();
        module.functions[0].blocks[0].terminator.kind = TerminatorKind::none;
        expect_error_contains(ir::verify_module(module), "has no terminator");
    }
    {
        Module module = make_simple_module();
        module.functions[0].blocks[0].terminator.value = invalid_value_id;
        expect_error_contains(ir::verify_module(module), "return value value id is invalid");
    }
    {
        Module module;
        Function function = make_function(module, "void_value", builtin(module, BuiltinType::void_));
        FunctionBuilder builder {module, function};
        const ValueId value = builder.add(integer_value(builtin(module, BuiltinType::i32), "1"));
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values.push_back(value);
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = value;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "returns a value");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        Function function = make_function(module, "bad_branch", i32);
        FunctionBuilder builder {module, function};
        const ValueId value = builder.add(integer_value(i32, "1"));
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values.push_back(value);
        function.blocks[entry.value].terminator.kind = TerminatorKind::branch;
        function.blocks[entry.value].terminator.target = BlockId {42};
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "branch target block id is invalid");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        Function function = make_function(module, "bad_cond", i32);
        FunctionBuilder builder {module, function};
        const ValueId value = builder.add(integer_value(i32, "1"));
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values.push_back(value);
        function.blocks[entry.value].terminator.kind = TerminatorKind::cond_branch;
        function.blocks[entry.value].terminator.condition = value;
        function.blocks[entry.value].terminator.then_target = entry;
        function.blocks[entry.value].terminator.else_target = entry;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "branch condition type mismatch");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        Function function = make_function(module, "empty_phi", i32);
        FunctionBuilder builder {module, function};
        Value phi;
        phi.kind = ValueKind::phi;
        phi.type = i32;
        const ValueId phi_id = builder.add(phi);
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values.push_back(phi_id);
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = phi_id;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "phi has no incoming values");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        Function target = make_function(module, "target", i32);
        module.functions.push_back(target);
        Function caller = make_function(module, "caller", i32);
        FunctionBuilder builder {module, caller};
        Value call;
        call.kind = ValueKind::call;
        call.type = i32;
        call.call_target = FunctionId {0};
        call.args.push_back(builder.add(integer_value(i32, "1")));
        const ValueId call_id = builder.add(call);
        const BlockId entry = builder.block("entry");
        caller.blocks[entry.value].values.push_back(call.args[0]);
        caller.blocks[entry.value].values.push_back(call_id);
        caller.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        caller.blocks[entry.value].terminator.value = call_id;
        module.functions.push_back(caller);
        expect_error_contains(ir::verify_module(module), "wrong argument count");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        Function function = make_function(module, "bad_load", i32);
        FunctionBuilder builder {module, function};
        const ValueId not_pointer = builder.add(integer_value(i32, "1"));
        Value load;
        load.kind = ValueKind::load;
        load.type = i32;
        load.object = not_pointer;
        const ValueId load_id = builder.add(load);
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {not_pointer, load_id};
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = load_id;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "load object is not a pointer");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const TypeHandle ptr_i32 = ptr(module, PointerMutability::mut, i32);
        Function function = make_function(module, "bad_field", i32);
        FunctionBuilder builder {module, function};
        Value object;
        object.kind = ValueKind::alloca;
        object.type = ptr_i32;
        const ValueId object_id = builder.add(object);
        Value field;
        field.kind = ValueKind::field_addr;
        field.type = ptr_i32;
        field.object = object_id;
        field.name = "missing";
        const ValueId field_id = builder.add(field);
        const ValueId result = builder.add(integer_value(i32, "0"));
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {object_id, field_id, result};
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = result;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "unknown field");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const ValueId first = add_value(module, Value {ValueKind::constant_ref, i32, "", "", invalid_function_id, invalid_value_id, invalid_value_id, invalid_value_id, invalid_value_id, {}, {}, {}, GlobalConstantId {1}});
        const ValueId second = add_value(module, Value {ValueKind::constant_ref, i32, "", "", invalid_function_id, invalid_value_id, invalid_value_id, invalid_value_id, invalid_value_id, {}, {}, {}, GlobalConstantId {0}});
        [[maybe_unused]] const GlobalConstantId first_constant =
            add_global_constant(module, GlobalConstant {"a", "unit_a", i32, first});
        [[maybe_unused]] const GlobalConstantId second_constant =
            add_global_constant(module, GlobalConstant {"b", "unit_b", i32, second});
        expect_error_contains(ir::verify_module(module), "cyclic constant reference");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const ValueId invalid_ref = add_value(module, Value {ValueKind::constant_ref, i32, "", "", invalid_function_id, invalid_value_id, invalid_value_id, invalid_value_id, invalid_value_id, {}, {}, {}, GlobalConstantId {42}});
        [[maybe_unused]] const GlobalConstantId constant =
            add_global_constant(module, GlobalConstant {"bad", "unit_bad", i32, invalid_ref});
        expect_error_contains(ir::verify_module(module), "constant reference id is invalid");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const TypeHandle bool_type = builtin(module, BuiltinType::bool_);
        const ValueId value = add_value(module, integer_value(i32, "1"));
        [[maybe_unused]] const GlobalConstantId constant =
            add_global_constant(module, GlobalConstant {"mismatch", "unit_mismatch", bool_type, value});
        expect_error_contains(ir::verify_module(module), "constant initializer type mismatch");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const TypeHandle record_type = module.types.named_struct("unit.Record", "unit_Record", false);
        module.records.push_back(RecordLayout {
            record_type,
            "unit.Record",
            "unit_Record",
            false,
            {RecordField {"x", i32}, RecordField {"y", i32}},
        });
        Value aggregate;
        aggregate.kind = ValueKind::aggregate;
        aggregate.type = record_type;
        aggregate.fields = {
            {"x", add_value(module, integer_value(i32, "1"))},
            {"x", add_value(module, integer_value(i32, "2"))},
            {"missing", add_value(module, integer_value(i32, "3"))},
        };
        const ValueId aggregate_id = add_value(module, aggregate);
        [[maybe_unused]] const GlobalConstantId constant =
            add_global_constant(module, GlobalConstant {"bad_record", "unit_bad_record", record_type, aggregate_id});
        const auto result = ir::verify_module(module);
        ASSERT_FALSE(result);
        expect_contains_all(result.error().message, {
            "duplicate aggregate field x",
            "unknown aggregate field missing",
        });
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const TypeHandle record_type = module.types.named_struct("unit.Record", "unit_Record", false);
        module.records.push_back(RecordLayout {
            record_type,
            "unit.Record",
            "unit_Record",
            false,
            {RecordField {"x", i32}, RecordField {"y", i32}},
        });
        Value aggregate;
        aggregate.kind = ValueKind::aggregate;
        aggregate.type = record_type;
        aggregate.fields = {{"x", add_value(module, integer_value(i32, "1"))}};
        const ValueId aggregate_id = add_value(module, aggregate);
        [[maybe_unused]] const GlobalConstantId constant =
            add_global_constant(module, GlobalConstant {"incomplete", "unit_incomplete", record_type, aggregate_id});
        expect_error_contains(ir::verify_module(module), "aggregate constant does not initialize every field");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        Function function = make_function(module, "extern_with_body", i32, Linkage::extern_c, AbiCallConv::c);
        FunctionBuilder builder {module, function};
        const ValueId result = builder.add(integer_value(i32, "0"));
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values.push_back(result);
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = result;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "must not have blocks");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        Function function = make_function(module, "param_mismatch", i32);
        FunctionBuilder builder {module, function};
        Value param;
        param.kind = ValueKind::param;
        param.type = builtin(module, BuiltinType::bool_);
        const ValueId param_id = builder.add(param);
        function.signature_params.push_back(FunctionParam {"x", i32});
        function.param_values.push_back(param_id);
        const ValueId result = builder.add(integer_value(i32, "0"));
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {param_id, result};
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = result;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "parameter type mismatch");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        Function target = make_function(module, "target", i32);
        target.signature_params.push_back(FunctionParam {"x", i32});
        module.functions.push_back(target);
        Function caller = make_function(module, "caller_bad_arg", i32);
        FunctionBuilder builder {module, caller};
        const ValueId arg = builder.add(bool_value(module, true));
        Value call;
        call.kind = ValueKind::call;
        call.type = builtin(module, BuiltinType::bool_);
        call.call_target = FunctionId {0};
        call.args.push_back(arg);
        const ValueId call_id = builder.add(call);
        const ValueId result = builder.add(integer_value(i32, "0"));
        const BlockId entry = builder.block("entry");
        caller.blocks[entry.value].values = {arg, call_id, result};
        caller.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        caller.blocks[entry.value].terminator.value = result;
        module.functions.push_back(caller);
        const auto verify = ir::verify_module(module);
        ASSERT_FALSE(verify);
        expect_contains_all(verify.error().message, {
            "call argument type mismatch",
            "call to @test_target result type mismatch",
        });
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const TypeHandle record_type = module.types.named_struct("unit.Record", "unit_Record", false);
        module.records.push_back(RecordLayout {
            record_type,
            "unit.Record",
            "unit_Record",
            false,
            {RecordField {"x", i32}, RecordField {"y", i32}},
        });
        Function function = make_function(module, "bad_aggregate", i32);
        FunctionBuilder builder {module, function};
        Value aggregate;
        aggregate.kind = ValueKind::aggregate;
        aggregate.type = record_type;
        aggregate.fields = {
            {"x", builder.add(integer_value(i32, "1"))},
            {"x", builder.add(integer_value(i32, "2"))},
        };
        const ValueId aggregate_id = builder.add(aggregate);
        const ValueId result = builder.add(integer_value(i32, "0"));
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {
            aggregate.fields[0].value,
            aggregate.fields[1].value,
            aggregate_id,
            result,
        };
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = result;
        module.functions.push_back(function);
        const auto verify = ir::verify_module(module);
        ASSERT_FALSE(verify);
        expect_contains_all(verify.error().message, {
            "duplicate aggregate field x",
            "aggregate does not initialize every field",
        });
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const TypeHandle void_type = builtin(module, BuiltinType::void_);
        const TypeHandle ptr_i32 = ptr(module, PointerMutability::mut, i32);
        Function function = make_function(module, "bad_store_source", i32);
        FunctionBuilder builder {module, function};
        Value slot;
        slot.kind = ValueKind::alloca;
        slot.type = ptr_i32;
        const ValueId slot_id = builder.add(slot);
        const ValueId source = builder.add(bool_value(module, true));
        Value store;
        store.kind = ValueKind::store;
        store.type = void_type;
        store.object = slot_id;
        store.lhs = source;
        const ValueId store_id = builder.add(store);
        const ValueId result = builder.add(integer_value(i32, "0"));
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {slot_id, source, store_id, result};
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = result;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "store source type mismatch");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        Function function = make_function(module, "bad_binary_operands", i32);
        FunctionBuilder builder {module, function};
        const ValueId lhs = builder.add(integer_value(i32, "1"));
        const ValueId rhs = builder.add(bool_value(module, false));
        Value binary;
        binary.kind = ValueKind::binary;
        binary.type = i32;
        binary.binary_op = BinaryOp::add;
        binary.lhs = lhs;
        binary.rhs = rhs;
        const ValueId binary_id = builder.add(binary);
        const ValueId result = builder.add(integer_value(i32, "0"));
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {lhs, rhs, binary_id, result};
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = result;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "binary operand type mismatch");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const TypeHandle bool_type = builtin(module, BuiltinType::bool_);
        Function function = make_function(module, "bad_binary_result", i32);
        FunctionBuilder builder {module, function};
        const ValueId lhs = builder.add(integer_value(i32, "1"));
        const ValueId rhs = builder.add(integer_value(i32, "2"));
        Value binary;
        binary.kind = ValueKind::binary;
        binary.type = bool_type;
        binary.binary_op = BinaryOp::add;
        binary.lhs = lhs;
        binary.rhs = rhs;
        const ValueId binary_id = builder.add(binary);
        const ValueId result = builder.add(integer_value(i32, "0"));
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {lhs, rhs, binary_id, result};
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = result;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "numeric binary result must match operand type");
    }
}

} // namespace aurex::test
