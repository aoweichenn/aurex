#include "aurex/ir/verify.hpp"
#include "gtest/support/ir_test_helpers.hpp"

namespace aurex::test {
namespace {

using namespace irtest;

} // namespace

TEST(CoreUnit, IrVerifierReportsAdditionalEdgeCaseErrors) {
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        [[maybe_unused]] const GlobalConstantId constant =
            add_global_constant(module, GlobalConstant {"missing", "unit_missing", i32, invalid_value_id});
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
}

} // namespace aurex::test
