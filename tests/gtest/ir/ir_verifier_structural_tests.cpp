#include <aurex/ir/verify.hpp>
#include <gtest/support/ir_test_helpers.hpp>

#include <initializer_list>
#include <string>
#include <utility>

namespace aurex::test {
namespace {

using namespace irtest;

constexpr const char* IR_VERIFIER_LITERAL_ZERO = "0";
constexpr const char* IR_VERIFIER_LITERAL_ONE = "1";
constexpr const char* IR_VERIFIER_LITERAL_TWO = "2";
constexpr const char* IR_VERIFIER_STRING_LITERAL_BYTES = "\"bytes\"";
constexpr const char* IR_VERIFIER_C_STRING_LITERAL_BYTES = "c\"bytes\"";
constexpr base::u64 IR_VERIFIER_NESTED_ARRAY_INNER_COUNT = 3;
constexpr base::u64 IR_VERIFIER_NESTED_ARRAY_OUTER_COUNT = 2;

[[nodiscard]] Value typed_value(const ValueKind kind, const TypeHandle type, std::string text = {}) {
    Value value;
    value.kind = kind;
    value.type = type;
    value.text = std::move(text);
    return value;
}

[[nodiscard]] Value alloca_value(const TypeHandle type) {
    Value value;
    value.kind = ValueKind::alloca;
    value.type = type;
    return value;
}

void append_return_block(
    FunctionBuilder& builder,
    Function& function,
    const std::initializer_list<ValueId> values,
    const ValueId return_value
) {
    const BlockId entry = builder.block("entry");
    function.blocks[entry.value].values = values;
    function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
    function.blocks[entry.value].terminator.value = return_value;
}

[[nodiscard]] Value slice_value(const TypeHandle type, const ValueId data, const ValueId length) {
    Value value;
    value.kind = ValueKind::slice;
    value.type = type;
    value.lhs = data;
    value.rhs = length;
    return value;
}

[[nodiscard]] Value slice_data_value(const TypeHandle type, const ValueId object) {
    Value value;
    value.kind = ValueKind::slice_data;
    value.type = type;
    value.object = object;
    return value;
}

[[nodiscard]] Value slice_len_value(const TypeHandle type, const ValueId object) {
    Value value;
    value.kind = ValueKind::slice_len;
    value.type = type;
    value.object = object;
    return value;
}

} // namespace

TEST(CoreUnit, IrVerifierReportsRepresentativeStructuralErrors) {
    {
        Module module = make_simple_module();
        EXPECT_TRUE(ir::verify_module(module));
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const ValueId unary_operand = add_value(module, integer_value(i32, IR_VERIFIER_LITERAL_ONE));
        Value unary;
        unary.kind = ValueKind::unary;
        unary.type = i32;
        unary.unary_op = UnaryOp::numeric_negate;
        unary.lhs = unary_operand;
        const ValueId unary_id = add_value(module, unary);
        const ValueId binary_rhs = add_value(module, integer_value(i32, IR_VERIFIER_LITERAL_TWO));
        Value binary;
        binary.kind = ValueKind::binary;
        binary.type = i32;
        binary.binary_op = BinaryOp::add;
        binary.lhs = unary_id;
        binary.rhs = binary_rhs;
        const ValueId binary_id = add_value(module, binary);
        Value cast;
        cast.kind = ValueKind::cast;
        cast.type = i32;
        cast.target_type = i32;
        cast.lhs = binary_id;
        cast.cast_kind = CastKind::numeric;
        const ValueId cast_id = add_value(module, cast);
        [[maybe_unused]] const GlobalConstantId unary_constant =
            add_global_constant(module, GlobalConstant {"unary", "unit_unary", i32, unary_id});
        [[maybe_unused]] const GlobalConstantId binary_constant =
            add_global_constant(module, GlobalConstant {"binary", "unit_binary", i32, binary_id});
        [[maybe_unused]] const GlobalConstantId cast_constant =
            add_global_constant(module, GlobalConstant {"cast", "unit_cast", i32, cast_id});
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
        module.functions[0].blocks[0].terminator.value = INVALID_VALUE_ID;
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
        const TypeHandle bool_type = builtin(module, BuiltinType::bool_);
        const TypeHandle ptr_i32 = ptr(module, PointerMutability::mut, i32);
        Function function = make_function(module, "bad_load_result", i32);
        FunctionBuilder builder {module, function};
        Value object;
        object.kind = ValueKind::alloca;
        object.type = ptr_i32;
        const ValueId object_id = builder.add(object);
        Value load;
        load.kind = ValueKind::load;
        load.type = bool_type;
        load.object = object_id;
        const ValueId load_id = builder.add(load);
        const ValueId result = builder.add(integer_value(i32, "0"));
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {object_id, load_id, result};
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = result;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "load result type mismatch");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        Function function = make_function(module, "bad_alloca", i32);
        FunctionBuilder builder {module, function};
        Value object;
        object.kind = ValueKind::alloca;
        object.type = i32;
        const ValueId object_id = builder.add(object);
        const ValueId result = builder.add(integer_value(i32, "0"));
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {object_id, result};
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = result;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "alloca result must be a pointer");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const TypeHandle void_type = builtin(module, BuiltinType::void_);
        const TypeHandle const_ptr_i32 = ptr(module, PointerMutability::const_, i32);
        Function function = make_function(module, "bad_store_const_target", i32);
        FunctionBuilder builder {module, function};
        Value object;
        object.kind = ValueKind::null_literal;
        object.type = const_ptr_i32;
        const ValueId object_id = builder.add(object);
        const ValueId source = builder.add(integer_value(i32, "1"));
        Value store;
        store.kind = ValueKind::store;
        store.type = void_type;
        store.object = object_id;
        store.lhs = source;
        const ValueId store_id = builder.add(store);
        const ValueId result = builder.add(integer_value(i32, "0"));
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {object_id, source, store_id, result};
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = result;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "store target must be mutable");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const TypeHandle usize = builtin(module, BuiltinType::usize);
        const TypeHandle void_type = builtin(module, BuiltinType::void_);
        Function function = make_function(module, "bad_scalar_shapes", i32);
        FunctionBuilder builder {module, function};
        Value null_value;
        null_value.kind = ValueKind::null_literal;
        null_value.type = i32;
        const ValueId bad_null = builder.add(null_value);
        Value bool_value;
        bool_value.kind = ValueKind::bool_literal;
        bool_value.type = i32;
        bool_value.text = "true";
        const ValueId bad_bool = builder.add(bool_value);
        Value sizeof_value;
        sizeof_value.kind = ValueKind::size_of;
        sizeof_value.type = usize;
        sizeof_value.target_type = void_type;
        const ValueId bad_sizeof = builder.add(sizeof_value);
        const ValueId result = builder.add(integer_value(i32, "0"));
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {bad_null, bad_bool, bad_sizeof, result};
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = result;
        module.functions.push_back(function);
        const auto verify = ir::verify_module(module);
        ASSERT_FALSE(verify);
        expect_contains_all(verify.error().message, {
            "null literal type must be pointer",
            "bool literal type must be bool",
            "sizeof target type is not valid storage",
        });
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const TypeHandle usize = builtin(module, BuiltinType::usize);
        const TypeHandle void_type = builtin(module, BuiltinType::void_);
        const TypeHandle inner_array = module.types.array(IR_VERIFIER_NESTED_ARRAY_INNER_COUNT, void_type);
        const TypeHandle nested_array = module.types.array(IR_VERIFIER_NESTED_ARRAY_OUTER_COUNT, inner_array);
        Function function = make_function(module, "nested_storage", i32);
        FunctionBuilder builder {module, function};
        Value sizeof_value;
        sizeof_value.kind = ValueKind::size_of;
        sizeof_value.type = usize;
        sizeof_value.target_type = nested_array;
        const ValueId sizeof_id = builder.add(sizeof_value);
        const ValueId result = builder.add(integer_value(i32, IR_VERIFIER_LITERAL_ZERO));
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {sizeof_id, result};
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = result;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "sizeof target element element type is not valid storage");
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
        const TypeHandle bool_type = builtin(module, BuiltinType::bool_);
        const TypeHandle record_type = module.types.named_struct("unit.Record", "unit_Record", false);
        module.records.push_back(RecordLayout {
            record_type,
            "unit.Record",
            "unit_Record",
            false,
            {RecordField {"value", i32}},
        });
        Function function = make_function(module, "bad_field_type", i32);
        FunctionBuilder builder {module, function};
        Value object;
        object.kind = ValueKind::alloca;
        object.type = ptr(module, PointerMutability::mut, record_type);
        const ValueId object_id = builder.add(object);
        Value field;
        field.kind = ValueKind::field_addr;
        field.type = ptr(module, PointerMutability::mut, bool_type);
        field.object = object_id;
        field.name = "value";
        const ValueId field_id = builder.add(field);
        const ValueId result = builder.add(integer_value(i32, "0"));
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {object_id, field_id, result};
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = result;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "field address result type mismatch");
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
            {RecordField {"value", i32}},
        });
        Function function = make_function(module, "bad_field_mutability", i32);
        FunctionBuilder builder {module, function};
        Value object;
        object.kind = ValueKind::param;
        object.type = ptr(module, PointerMutability::const_, record_type);
        const ValueId object_id = builder.add(object);
        function.signature_params.push_back(FunctionParam {"object", object.type});
        function.param_values.push_back(object_id);
        Value field;
        field.kind = ValueKind::field_addr;
        field.type = ptr(module, PointerMutability::mut, i32);
        field.object = object_id;
        field.name = "value";
        const ValueId field_id = builder.add(field);
        const ValueId result = builder.add(integer_value(i32, "0"));
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {object_id, field_id, result};
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = result;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "field address cannot be mutable through const object");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const TypeHandle bool_type = builtin(module, BuiltinType::bool_);
        const TypeHandle array_i32 = module.types.array(4, i32);
        Function function = make_function(module, "bad_index_type", i32);
        FunctionBuilder builder {module, function};
        Value object;
        object.kind = ValueKind::alloca;
        object.type = ptr(module, PointerMutability::mut, array_i32);
        const ValueId object_id = builder.add(object);
        const ValueId index_id = builder.add(integer_value(i32, "0"));
        Value index;
        index.kind = ValueKind::index_addr;
        index.type = ptr(module, PointerMutability::mut, bool_type);
        index.object = object_id;
        index.index = index_id;
        const ValueId index_addr_id = builder.add(index);
        const ValueId result = builder.add(integer_value(i32, "0"));
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {object_id, index_id, index_addr_id, result};
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = result;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "index address result type mismatch");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const ValueId first = add_value(module, Value {ValueKind::constant_ref, i32, "", "", INVALID_FUNCTION_ID, INVALID_VALUE_ID, INVALID_VALUE_ID, INVALID_VALUE_ID, INVALID_VALUE_ID, {}, {}, {}, GlobalConstantId {1}});
        const ValueId second = add_value(module, Value {ValueKind::constant_ref, i32, "", "", INVALID_FUNCTION_ID, INVALID_VALUE_ID, INVALID_VALUE_ID, INVALID_VALUE_ID, INVALID_VALUE_ID, {}, {}, {}, GlobalConstantId {0}});
        [[maybe_unused]] const GlobalConstantId first_constant =
            add_global_constant(module, GlobalConstant {"a", "unit_a", i32, first});
        [[maybe_unused]] const GlobalConstantId second_constant =
            add_global_constant(module, GlobalConstant {"b", "unit_b", i32, second});
        expect_error_contains(ir::verify_module(module), "cyclic constant reference");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const ValueId invalid_ref = add_value(module, Value {ValueKind::constant_ref, i32, "", "", INVALID_FUNCTION_ID, INVALID_VALUE_ID, INVALID_VALUE_ID, INVALID_VALUE_ID, INVALID_VALUE_ID, {}, {}, {}, GlobalConstantId {42}});
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
        const TypeHandle array_type = module.types.array(2, i32);
        Value aggregate;
        aggregate.kind = ValueKind::aggregate;
        aggregate.type = array_type;
        aggregate.elements = {
            add_value(module, integer_value(i32, "1")),
            add_value(module, integer_value(i32, "2")),
        };
        const ValueId aggregate_id = add_value(module, aggregate);
        [[maybe_unused]] const GlobalConstantId constant =
            add_global_constant(module, GlobalConstant {"array", "unit_array", array_type, aggregate_id});
        EXPECT_TRUE(ir::verify_module(module));
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const TypeHandle array_type = module.types.array(2, i32);
        Value aggregate;
        aggregate.kind = ValueKind::aggregate;
        aggregate.type = array_type;
        aggregate.elements = {add_value(module, integer_value(i32, "1"))};
        const ValueId aggregate_id = add_value(module, aggregate);
        [[maybe_unused]] const GlobalConstantId constant =
            add_global_constant(module, GlobalConstant {"bad_array", "unit_bad_array", array_type, aggregate_id});
        expect_error_contains(ir::verify_module(module), "array aggregate constant element count mismatch");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const TypeHandle array_type = module.types.array(2, i32);
        Value aggregate;
        aggregate.kind = ValueKind::aggregate;
        aggregate.type = array_type;
        aggregate.fields = {{"x", add_value(module, integer_value(i32, "1"))}};
        const ValueId aggregate_id = add_value(module, aggregate);
        [[maybe_unused]] const GlobalConstantId constant = add_global_constant(
            module,
            GlobalConstant {"bad_array_fields", "unit_bad_array_fields", array_type, aggregate_id}
        );
        expect_error_contains(ir::verify_module(module), "array aggregate constant cannot contain named fields");
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
            {RecordField {"x", i32}},
        });
        Value aggregate;
        aggregate.kind = ValueKind::aggregate;
        aggregate.type = record_type;
        aggregate.fields = {{"x", add_value(module, integer_value(i32, "1"))}};
        aggregate.elements = {add_value(module, integer_value(i32, "2"))};
        const ValueId aggregate_id = add_value(module, aggregate);
        [[maybe_unused]] const GlobalConstantId constant = add_global_constant(
            module,
            GlobalConstant {"bad_record_elements", "unit_bad_record_elements", record_type, aggregate_id}
        );
        expect_error_contains(ir::verify_module(module), "record aggregate constant cannot contain array elements");
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
        const TypeHandle array_type = module.types.array(2, i32);
        Function function = make_function(module, "bad_array_aggregate_fields", i32);
        FunctionBuilder builder {module, function};
        Value aggregate;
        aggregate.kind = ValueKind::aggregate;
        aggregate.type = array_type;
        aggregate.fields = {{"x", builder.add(integer_value(i32, "1"))}};
        aggregate.elements = {
            builder.add(integer_value(i32, "1")),
            builder.add(integer_value(i32, "2")),
        };
        const ValueId aggregate_id = builder.add(aggregate);
        const ValueId result = builder.add(integer_value(i32, "0"));
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {
            aggregate.fields[0].value,
            aggregate.elements[0],
            aggregate.elements[1],
            aggregate_id,
            result,
        };
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = result;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "array aggregate cannot contain named fields");
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const TypeHandle array_type = module.types.array(2, i32);
        Function function = make_function(module, "bad_array_aggregate_count", i32);
        FunctionBuilder builder {module, function};
        Value aggregate;
        aggregate.kind = ValueKind::aggregate;
        aggregate.type = array_type;
        aggregate.elements = {
            builder.add(integer_value(i32, "1")),
        };
        const ValueId aggregate_id = builder.add(aggregate);
        const ValueId result = builder.add(integer_value(i32, "0"));
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {
            aggregate.elements[0],
            aggregate_id,
            result,
        };
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = result;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "array aggregate element count mismatch");
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
            {RecordField {"x", i32}},
        });
        Function function = make_function(module, "bad_record_aggregate_elements", i32);
        FunctionBuilder builder {module, function};
        Value aggregate;
        aggregate.kind = ValueKind::aggregate;
        aggregate.type = record_type;
        aggregate.fields = {{"x", builder.add(integer_value(i32, "1"))}};
        aggregate.elements = {builder.add(integer_value(i32, "2"))};
        const ValueId aggregate_id = builder.add(aggregate);
        const ValueId result = builder.add(integer_value(i32, "0"));
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = {
            aggregate.fields[0].value,
            aggregate.elements[0],
            aggregate_id,
            result,
        };
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = result;
        module.functions.push_back(function);
        expect_error_contains(ir::verify_module(module), "record aggregate cannot contain array elements");
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

TEST(CoreUnit, IrVerifierAcceptsStringAndLayoutValues) {
    Module module;
    const TypeHandle i32 = builtin(module, BuiltinType::i32);
    const TypeHandle usize = builtin(module, BuiltinType::usize);
    const TypeHandle str = builtin(module, BuiltinType::str);
    const TypeHandle u8 = builtin(module, BuiltinType::u8);
    const TypeHandle const_u8_ptr = ptr(module, PointerMutability::const_, u8);

    Function function = make_function(module, "string_and_layout", str);
    FunctionBuilder builder {module, function};

    const ValueId string_value =
        builder.add(typed_value(ValueKind::string_literal, str, IR_VERIFIER_STRING_LITERAL_BYTES));
    const ValueId c_string_value = builder.add(
        typed_value(ValueKind::c_string_literal, const_u8_ptr, IR_VERIFIER_C_STRING_LITERAL_BYTES)
    );

    Value size_of_value = typed_value(ValueKind::size_of, usize);
    size_of_value.target_type = i32;
    const ValueId size_of_id = builder.add(size_of_value);

    Value align_of_value = typed_value(ValueKind::align_of, usize);
    align_of_value.target_type = i32;
    const ValueId align_of_id = builder.add(align_of_value);

    Value str_data_value = typed_value(ValueKind::str_data, const_u8_ptr);
    str_data_value.object = string_value;
    const ValueId str_data_id = builder.add(str_data_value);

    Value str_len_value = typed_value(ValueKind::str_byte_len, usize);
    str_len_value.object = string_value;
    const ValueId str_len_id = builder.add(str_len_value);

    Value from_bytes_value = typed_value(ValueKind::str_from_bytes_unchecked, str);
    from_bytes_value.args = {c_string_value, str_len_id};
    const ValueId from_bytes_id = builder.add(from_bytes_value);

    append_return_block(
        builder,
        function,
        {string_value, c_string_value, size_of_id, align_of_id, str_data_id, str_len_id, from_bytes_id},
        from_bytes_id
    );
    module.functions.push_back(function);

    EXPECT_TRUE(ir::verify_module(module));
}

TEST(CoreUnit, IrVerifierReportsStringBuiltinShapeErrors) {
    Module module;
    const TypeHandle i32 = builtin(module, BuiltinType::i32);
    const TypeHandle usize = builtin(module, BuiltinType::usize);
    const TypeHandle str = builtin(module, BuiltinType::str);
    const TypeHandle u8 = builtin(module, BuiltinType::u8);
    const TypeHandle mut_u8_ptr = ptr(module, PointerMutability::mut, u8);
    const TypeHandle const_u8_ptr = ptr(module, PointerMutability::const_, u8);

    Function function = make_function(module, "bad_string_builtins", i32);
    FunctionBuilder builder {module, function};

    const ValueId bad_string_literal =
        builder.add(typed_value(ValueKind::string_literal, i32, IR_VERIFIER_STRING_LITERAL_BYTES));
    const ValueId bad_c_string_literal =
        builder.add(typed_value(ValueKind::c_string_literal, i32, IR_VERIFIER_C_STRING_LITERAL_BYTES));
    const ValueId bad_byte_literal = builder.add(typed_value(ValueKind::byte_literal, i32, IR_VERIFIER_LITERAL_ONE));
    const ValueId bad_undef = builder.add(typed_value(ValueKind::undef, builtin(module, BuiltinType::void_)));
    const ValueId good_string_value =
        builder.add(typed_value(ValueKind::string_literal, str, IR_VERIFIER_STRING_LITERAL_BYTES));
    const ValueId good_c_string_value = builder.add(
        typed_value(ValueKind::c_string_literal, const_u8_ptr, IR_VERIFIER_C_STRING_LITERAL_BYTES)
    );
    const ValueId bad_mut_ptr_value = builder.add(typed_value(ValueKind::undef, mut_u8_ptr));
    const ValueId usize_one = builder.add(integer_value(usize, IR_VERIFIER_LITERAL_ONE));
    const ValueId bool_value_id = builder.add(bool_value(module, true));

    Value bad_str_data_result = typed_value(ValueKind::str_data, i32);
    bad_str_data_result.object = good_string_value;
    const ValueId bad_str_data_result_id = builder.add(bad_str_data_result);

    Value bad_str_data_operand = typed_value(ValueKind::str_data, const_u8_ptr);
    bad_str_data_operand.object = bad_byte_literal;
    const ValueId bad_str_data_operand_id = builder.add(bad_str_data_operand);

    Value bad_str_len_result = typed_value(ValueKind::str_byte_len, i32);
    bad_str_len_result.object = good_string_value;
    const ValueId bad_str_len_result_id = builder.add(bad_str_len_result);

    Value bad_str_len_operand = typed_value(ValueKind::str_byte_len, usize);
    bad_str_len_operand.object = bad_byte_literal;
    const ValueId bad_str_len_operand_id = builder.add(bad_str_len_operand);

    Value bad_from_count = typed_value(ValueKind::str_from_bytes_unchecked, str);
    bad_from_count.args = {good_c_string_value};
    const ValueId bad_from_count_id = builder.add(bad_from_count);

    Value bad_from_result = typed_value(ValueKind::str_from_bytes_unchecked, i32);
    bad_from_result.args = {good_c_string_value, usize_one};
    const ValueId bad_from_result_id = builder.add(bad_from_result);

    Value bad_from_data = typed_value(ValueKind::str_from_bytes_unchecked, str);
    bad_from_data.args = {bad_mut_ptr_value, usize_one};
    const ValueId bad_from_data_id = builder.add(bad_from_data);

    Value bad_from_length = typed_value(ValueKind::str_from_bytes_unchecked, str);
    bad_from_length.args = {good_c_string_value, bool_value_id};
    const ValueId bad_from_length_id = builder.add(bad_from_length);

    const ValueId result = builder.add(integer_value(i32, IR_VERIFIER_LITERAL_ZERO));
    append_return_block(
        builder,
        function,
        {
            bad_string_literal,
            bad_c_string_literal,
            bad_byte_literal,
            bad_undef,
            good_string_value,
            good_c_string_value,
            bad_mut_ptr_value,
            usize_one,
            bool_value_id,
            bad_str_data_result_id,
            bad_str_data_operand_id,
            bad_str_len_result_id,
            bad_str_len_operand_id,
            bad_from_count_id,
            bad_from_result_id,
            bad_from_data_id,
            bad_from_length_id,
            result,
        },
        result
    );
    module.functions.push_back(function);

    const auto verify = ir::verify_module(module);
    ASSERT_FALSE(verify);
    expect_contains_all(verify.error().message, {
        "string literal type must be str",
        "c string literal type must be *const u8",
        "byte literal type must be u8",
        "undef value cannot have void type",
        "strptr result must be *const u8",
        "strptr operand type mismatch",
        "strblen result must be usize",
        "strblen operand type mismatch",
        "strraw requires data and length arguments",
        "strraw result must be str",
        "strraw data must be *const u8",
        "strraw length type mismatch",
    });
}

TEST(CoreUnit, IrVerifierReportsOperatorAndStorageShapeErrors) {
    Module module;
    const TypeHandle i32 = builtin(module, BuiltinType::i32);
    const TypeHandle bool_type = builtin(module, BuiltinType::bool_);
    const TypeHandle usize = builtin(module, BuiltinType::usize);
    const TypeHandle void_type = builtin(module, BuiltinType::void_);
    const TypeHandle opaque_type = module.types.opaque_struct("unit.Opaque", "unit_Opaque");
    const TypeHandle record_type = module.types.named_struct("unit.Record", "unit_Record", false);
    const TypeHandle array_i32 = module.types.array(IR_VERIFIER_NESTED_ARRAY_INNER_COUNT, i32);
    const TypeHandle const_ptr_i32 = ptr(module, PointerMutability::const_, i32);
    const TypeHandle mut_ptr_invalid = ptr(module, PointerMutability::mut, sema::INVALID_TYPE_HANDLE);
    const TypeHandle mut_ptr_opaque = ptr(module, PointerMutability::mut, opaque_type);
    const TypeHandle const_array_ptr = ptr(module, PointerMutability::const_, array_i32);
    const TypeHandle mut_bool_ptr = ptr(module, PointerMutability::mut, bool_type);

    module.records.push_back(RecordLayout {
        record_type,
        "unit.Record",
        "unit_Record",
        false,
        {RecordField {"x", i32}},
    });

    Function function = make_function(module, "operators_and_storage", i32);
    FunctionBuilder builder {module, function};

    const ValueId bool_true = builder.add(bool_value(module, true));
    const ValueId bool_false = builder.add(bool_value(module, false));
    const ValueId i32_one = builder.add(integer_value(i32, IR_VERIFIER_LITERAL_ONE));
    const ValueId i32_two = builder.add(integer_value(i32, IR_VERIFIER_LITERAL_TWO));
    const ValueId usize_one = builder.add(integer_value(usize, IR_VERIFIER_LITERAL_ONE));
    const ValueId invalid_typed_literal =
        builder.add(typed_value(ValueKind::integer_literal, sema::INVALID_TYPE_HANDLE, IR_VERIFIER_LITERAL_ONE));
    const ValueId record_value = builder.add(typed_value(ValueKind::undef, record_type));

    const ValueId alloca_not_pointer = builder.add(alloca_value(i32));
    const ValueId alloca_mutable = builder.add(alloca_value(const_ptr_i32));
    const ValueId alloca_invalid_storage = builder.add(alloca_value(mut_ptr_invalid));
    const ValueId alloca_opaque_storage = builder.add(alloca_value(mut_ptr_opaque));
    const ValueId load_void_object = alloca_mutable;

    Value load_void = typed_value(ValueKind::load, void_type);
    load_void.object = load_void_object;
    const ValueId load_void_id = builder.add(load_void);

    Value store_nonvoid = typed_value(ValueKind::store, i32);
    store_nonvoid.object = alloca_mutable;
    store_nonvoid.lhs = i32_one;
    const ValueId store_nonvoid_id = builder.add(store_nonvoid);

    Value cast_mismatch = typed_value(ValueKind::cast, bool_type);
    cast_mismatch.target_type = i32;
    cast_mismatch.lhs = i32_one;
    cast_mismatch.cast_kind = CastKind::numeric;
    const ValueId cast_mismatch_id = builder.add(cast_mismatch);

    Value field_not_pointer = typed_value(ValueKind::field_addr, i32);
    field_not_pointer.object = i32_one;
    field_not_pointer.name = "x";
    const ValueId field_not_pointer_id = builder.add(field_not_pointer);

    Value field_mismatch = typed_value(ValueKind::field_addr, ptr(module, PointerMutability::mut, bool_type));
    field_mismatch.object = builder.add(
        typed_value(ValueKind::undef, ptr(module, PointerMutability::const_, record_type))
    );
    field_mismatch.name = "x";
    const ValueId field_mismatch_id = builder.add(field_mismatch);

    Value index_not_pointer = typed_value(ValueKind::index_addr, i32);
    index_not_pointer.object = i32_one;
    index_not_pointer.index = i32_one;
    const ValueId index_not_pointer_id = builder.add(index_not_pointer);

    const ValueId const_array_object =
        builder.add(typed_value(ValueKind::undef, const_array_ptr));

    Value index_bad = typed_value(ValueKind::index_addr, mut_bool_ptr);
    index_bad.object = const_array_object;
    index_bad.index = bool_false;
    const ValueId index_bad_id = builder.add(index_bad);

    Value unary_missing_operand = typed_value(ValueKind::unary, i32);
    unary_missing_operand.unary_op = UnaryOp::logical_not;
    unary_missing_operand.lhs = INVALID_VALUE_ID;
    const ValueId unary_missing_operand_id = builder.add(unary_missing_operand);

    Value unary_invalid_operand = typed_value(ValueKind::unary, i32);
    unary_invalid_operand.unary_op = UnaryOp::numeric_negate;
    unary_invalid_operand.lhs = invalid_typed_literal;
    const ValueId unary_invalid_operand_id = builder.add(unary_invalid_operand);

    Value unary_logical_bad = typed_value(ValueKind::unary, i32);
    unary_logical_bad.unary_op = UnaryOp::logical_not;
    unary_logical_bad.lhs = bool_true;
    const ValueId unary_logical_bad_id = builder.add(unary_logical_bad);

    Value unary_numeric_bad = typed_value(ValueKind::unary, i32);
    unary_numeric_bad.unary_op = UnaryOp::numeric_negate;
    unary_numeric_bad.lhs = bool_false;
    const ValueId unary_numeric_bad_id = builder.add(unary_numeric_bad);

    Value unary_bitwise_bad = typed_value(ValueKind::unary, bool_type);
    unary_bitwise_bad.unary_op = UnaryOp::bitwise_not;
    unary_bitwise_bad.lhs = bool_true;
    const ValueId unary_bitwise_bad_id = builder.add(unary_bitwise_bad);

    Value unary_address_bad = typed_value(ValueKind::unary, bool_type);
    unary_address_bad.unary_op = UnaryOp::address_of;
    unary_address_bad.lhs = i32_one;
    const ValueId unary_address_bad_id = builder.add(unary_address_bad);

    Value binary_compare_bad_result = typed_value(ValueKind::binary, i32);
    binary_compare_bad_result.binary_op = BinaryOp::less;
    binary_compare_bad_result.lhs = i32_one;
    binary_compare_bad_result.rhs = i32_two;
    const ValueId binary_compare_bad_result_id = builder.add(binary_compare_bad_result);

    Value binary_compare_bad_operand = typed_value(ValueKind::binary, bool_type);
    binary_compare_bad_operand.binary_op = BinaryOp::less;
    binary_compare_bad_operand.lhs = bool_true;
    binary_compare_bad_operand.rhs = bool_false;
    const ValueId binary_compare_bad_operand_id = builder.add(binary_compare_bad_operand);

    Value binary_equality_bad_result = typed_value(ValueKind::binary, i32);
    binary_equality_bad_result.binary_op = BinaryOp::equal;
    binary_equality_bad_result.lhs = bool_true;
    binary_equality_bad_result.rhs = bool_false;
    const ValueId binary_equality_bad_result_id = builder.add(binary_equality_bad_result);

    Value binary_equality_non_scalar = typed_value(ValueKind::binary, bool_type);
    binary_equality_non_scalar.binary_op = BinaryOp::equal;
    binary_equality_non_scalar.lhs = record_value;
    binary_equality_non_scalar.rhs = record_value;
    const ValueId binary_equality_non_scalar_id = builder.add(binary_equality_non_scalar);

    Value binary_logical_bad = typed_value(ValueKind::binary, bool_type);
    binary_logical_bad.binary_op = BinaryOp::logical_and;
    binary_logical_bad.lhs = i32_one;
    binary_logical_bad.rhs = i32_two;
    const ValueId binary_logical_bad_id = builder.add(binary_logical_bad);

    Value binary_integer_bad_result = typed_value(ValueKind::binary, bool_type);
    binary_integer_bad_result.binary_op = BinaryOp::bit_and;
    binary_integer_bad_result.lhs = i32_one;
    binary_integer_bad_result.rhs = i32_two;
    const ValueId binary_integer_bad_result_id = builder.add(binary_integer_bad_result);

    Value binary_integer_bad_operand = typed_value(ValueKind::binary, bool_type);
    binary_integer_bad_operand.binary_op = BinaryOp::bit_and;
    binary_integer_bad_operand.lhs = bool_true;
    binary_integer_bad_operand.rhs = bool_false;
    const ValueId binary_integer_bad_operand_id = builder.add(binary_integer_bad_operand);

    Value binary_numeric_bad_result = typed_value(ValueKind::binary, bool_type);
    binary_numeric_bad_result.binary_op = BinaryOp::add;
    binary_numeric_bad_result.lhs = i32_one;
    binary_numeric_bad_result.rhs = i32_two;
    const ValueId binary_numeric_bad_result_id = builder.add(binary_numeric_bad_result);

    Value binary_numeric_bad_operand = typed_value(ValueKind::binary, bool_type);
    binary_numeric_bad_operand.binary_op = BinaryOp::add;
    binary_numeric_bad_operand.lhs = bool_true;
    binary_numeric_bad_operand.rhs = bool_false;
    const ValueId binary_numeric_bad_operand_id = builder.add(binary_numeric_bad_operand);

    const ValueId result = builder.add(integer_value(i32, IR_VERIFIER_LITERAL_ZERO));
    append_return_block(
        builder,
        function,
        {
            bool_true,
            bool_false,
            i32_one,
            i32_two,
            usize_one,
            invalid_typed_literal,
            record_value,
            alloca_not_pointer,
            alloca_mutable,
            alloca_invalid_storage,
            alloca_opaque_storage,
            load_void_id,
            store_nonvoid_id,
            cast_mismatch_id,
            field_not_pointer_id,
            field_mismatch_id,
            index_not_pointer_id,
            const_array_object,
            index_bad_id,
            unary_missing_operand_id,
            unary_invalid_operand_id,
            unary_logical_bad_id,
            unary_numeric_bad_id,
            unary_bitwise_bad_id,
            unary_address_bad_id,
            binary_compare_bad_result_id,
            binary_compare_bad_operand_id,
            binary_equality_bad_result_id,
            binary_equality_non_scalar_id,
            binary_logical_bad_id,
            binary_integer_bad_result_id,
            binary_integer_bad_operand_id,
            binary_numeric_bad_result_id,
            binary_numeric_bad_operand_id,
            result,
        },
        result
    );
    module.functions.push_back(function);

    const auto verify = ir::verify_module(module);
    ASSERT_FALSE(verify);
    expect_contains_all(verify.error().message, {
        "alloca result must be a pointer",
        "alloca result must be a mutable pointer",
        "alloca pointee type is invalid",
        "alloca pointee type is not valid storage",
        "load result must not be void",
        "store result must be void",
        "cast result type must match cast target type",
        "field address result is not a pointer",
        "field address result type mismatch",
        "field address cannot be mutable through const object",
        "index address result is not a pointer",
        "index must be an integer",
        "index address result type mismatch",
        "index address cannot be mutable through const object",
        "unary operand value id is invalid",
        "unary operand type is invalid",
        "logical unary operator requires bool operand and result",
        "numeric unary operator requires matching numeric operand and result",
        "bitwise unary operator requires matching integer operand and result",
        "address/dereference unary passthrough type mismatch",
        "comparison binary result must be bool",
        "comparison binary operands must be numeric",
        "equality binary result must be bool",
        "equality binary operands must be scalar",
        "logical binary operator requires bool operands and result",
        "integer binary result must match operand type",
        "integer binary operator requires integer operands",
        "numeric binary result must match operand type",
        "numeric binary operator requires numeric operands",
    });
}

TEST(CoreUnit, IrVerifierReportsExternDeclarationMismatches) {
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const TypeHandle f64 = builtin(module, BuiltinType::f64);
        Function first = make_function(module, "extern_return_a", i32, Linkage::extern_c, AbiCallConv::c);
        first.symbol = "unit_extern_return";
        Function second = make_function(module, "extern_return_b", f64, Linkage::extern_c, AbiCallConv::c);
        second.symbol = "unit_extern_return";
        module.functions.push_back(first);
        module.functions.push_back(second);
        expect_error_contains(
            ir::verify_module(module),
            "extern function @unit_extern_return has inconsistent declarations"
        );
    }
    {
        Module module;
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const TypeHandle f64 = builtin(module, BuiltinType::f64);
        Function first = make_function(module, "extern_param_a", i32, Linkage::extern_c, AbiCallConv::c);
        first.symbol = "unit_extern_param";
        first.signature_params.push_back(FunctionParam {"lhs", i32});
        Function second = make_function(module, "extern_param_b", i32, Linkage::extern_c, AbiCallConv::c);
        second.symbol = "unit_extern_param";
        second.signature_params.push_back(FunctionParam {"lhs", f64});
        module.functions.push_back(first);
        module.functions.push_back(second);
        expect_error_contains(
            ir::verify_module(module),
            "extern function @unit_extern_param has inconsistent declarations"
        );
    }
}

TEST(CoreUnit, IrVerifierChecksSliceStructuralRules) {
    Module module;
    const TypeHandle i32 = builtin(module, BuiltinType::i32);
    const TypeHandle bool_type = builtin(module, BuiltinType::bool_);
    const TypeHandle usize = builtin(module, BuiltinType::usize);
    const TypeHandle mut_ptr_i32 = ptr(module, PointerMutability::mut, i32);
    const TypeHandle const_ptr_i32 = ptr(module, PointerMutability::const_, i32);
    const TypeHandle mut_ptr_bool = ptr(module, PointerMutability::mut, bool_type);
    const TypeHandle const_slice_i32 = module.types.slice(PointerMutability::const_, i32);
    const TypeHandle mut_slice_i32 = module.types.slice(PointerMutability::mut, i32);

    Function function = make_function(module, "bad_slices", i32);
    FunctionBuilder builder {module, function};
    const ValueId zero = builder.add(integer_value(i32, IR_VERIFIER_LITERAL_ZERO));
    const ValueId length = builder.add(integer_value(usize, IR_VERIFIER_LITERAL_ONE));
    const ValueId bool_length = builder.add(bool_value(module, true));
    const ValueId mut_data = builder.add(alloca_value(mut_ptr_i32));
    const ValueId const_data = builder.add(typed_value(ValueKind::undef, const_ptr_i32));
    const ValueId bool_data = builder.add(alloca_value(mut_ptr_bool));
    const ValueId const_slice = builder.add(typed_value(ValueKind::undef, const_slice_i32));
    const ValueId mut_slice = builder.add(typed_value(ValueKind::undef, mut_slice_i32));

    const ValueId bad_slice_result = builder.add(slice_value(i32, mut_data, length));
    const ValueId bad_slice_data = builder.add(slice_value(const_slice_i32, bool_data, length));
    const ValueId bad_slice_len = builder.add(slice_value(const_slice_i32, mut_data, bool_length));
    const ValueId bad_mut_slice_const_data = builder.add(slice_value(mut_slice_i32, const_data, length));
    const ValueId bad_slice_non_pointer_data = builder.add(slice_value(const_slice_i32, zero, length));
    const ValueId bad_slice_missing_data = builder.add(slice_value(const_slice_i32, INVALID_VALUE_ID, length));
    const ValueId bad_slice_missing_length = builder.add(slice_value(const_slice_i32, mut_data, INVALID_VALUE_ID));
    const ValueId bad_data_object = builder.add(slice_data_value(const_ptr_i32, zero));
    const ValueId bad_data_result = builder.add(slice_data_value(mut_ptr_i32, const_slice));
    const ValueId bad_data_element = builder.add(slice_data_value(mut_ptr_bool, mut_slice));
    const ValueId bad_data_result_non_pointer = builder.add(slice_data_value(i32, const_slice));
    const ValueId bad_len_result = builder.add(slice_len_value(i32, const_slice));
    const ValueId bad_len_object = builder.add(slice_len_value(usize, zero));
    const ValueId missing_len_object = builder.add(slice_len_value(usize, INVALID_VALUE_ID));
    const ValueId missing_data_object = builder.add(slice_data_value(const_ptr_i32, INVALID_VALUE_ID));

    append_return_block(
        builder,
        function,
        {
            zero,
            length,
            bool_length,
            mut_data,
            const_data,
            bool_data,
            const_slice,
            mut_slice,
            bad_slice_result,
            bad_slice_data,
            bad_slice_len,
            bad_mut_slice_const_data,
            bad_slice_non_pointer_data,
            bad_slice_missing_data,
            bad_slice_missing_length,
            bad_data_object,
            bad_data_result,
            bad_data_element,
            bad_data_result_non_pointer,
            bad_len_result,
            bad_len_object,
            missing_len_object,
            missing_data_object,
        },
        zero
    );
    module.functions.push_back(function);

    const auto result = ir::verify_module(module);
    ASSERT_FALSE(result);
    expect_contains_all(result.error().message, {
        "slice value result must be a slice",
        "slice data must be pointer to slice element",
        "slice length must be usize",
        "slice data value id is invalid",
        "slice data result must be pointer to slice element",
        "slice length result must be usize",
        "slice length value id is invalid",
        "slice_data object value id is invalid",
        "slice_len object value id is invalid",
    });
}

} // namespace aurex::test
