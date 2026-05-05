#include "aurex/backend/llvm_backend.hpp"
#include "gtest/support/ir_test_helpers.hpp"

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

namespace aurex::backend {
[[nodiscard]] bool parse_u64(const std::string& text, std::uint64_t& out) noexcept;
[[nodiscard]] std::string decode_string_literal(const std::string& literal, bool has_c_prefix);
[[nodiscard]] std::uint64_t parse_byte_literal(const std::string& literal);
} // namespace aurex::backend

namespace aurex::test {
namespace {

using base::ErrorCode;
using namespace irtest;

} // namespace

TEST(CoreUnit, LlvmBackendCoversPhiRuntimeCastsUnaryBinaryAndConstantInitializers) {
    Module module;
    const TypeHandle void_type = builtin(module, BuiltinType::void_);
    const TypeHandle bool_type = builtin(module, BuiltinType::bool_);
    const TypeHandle i32 = builtin(module, BuiltinType::i32);
    const TypeHandle u32 = builtin(module, BuiltinType::u32);
    const TypeHandle i64 = builtin(module, BuiltinType::i64);
    const TypeHandle f32 = builtin(module, BuiltinType::f32);
    const TypeHandle f64 = builtin(module, BuiltinType::f64);
    const TypeHandle usize = builtin(module, BuiltinType::usize);
    const TypeHandle str_type = builtin(module, BuiltinType::str);
    const TypeHandle ptr_i32 = ptr(module, PointerMutability::mut, i32);
    const TypeHandle pair_type = module.types.named_struct("unit.Pair", "unit_Pair", false);
    module.records.push_back(RecordLayout {
        pair_type,
        "unit.Pair",
        "unit_Pair",
        false,
        {RecordField {"left", i32}, RecordField {"right", i32}},
    });

    Value undef;
    undef.kind = ValueKind::undef;
    undef.type = i32;
    [[maybe_unused]] const GlobalConstantId undef_constant =
        add_global_constant(module, GlobalConstant {"undef", "unit_undef", i32, add_value(module, undef)});

    Value string_constant;
    string_constant.kind = ValueKind::string_literal;
    string_constant.type = str_type;
    string_constant.text = "\"constant\"";
    [[maybe_unused]] const GlobalConstantId text_constant =
        add_global_constant(module, GlobalConstant {"text", "unit_text", str_type, add_value(module, string_constant)});

    const ValueId base_value = add_value(module, integer_value(i32, "7"));
    const GlobalConstantId base_constant =
        add_global_constant(module, GlobalConstant {"base", "unit_base", i32, base_value});
    Value ref_value;
    ref_value.kind = ValueKind::constant_ref;
    ref_value.type = i32;
    ref_value.constant = base_constant;
    [[maybe_unused]] const GlobalConstantId ref_constant =
        add_global_constant(module, GlobalConstant {"ref", "unit_ref", i32, add_value(module, ref_value)});

    Value null_pointer;
    null_pointer.kind = ValueKind::null_literal;
    null_pointer.type = ptr_i32;
    const ValueId null_pointer_id = add_value(module, null_pointer);
    Value null_address;
    null_address.kind = ValueKind::cast;
    null_address.type = usize;
    null_address.target_type = usize;
    null_address.cast_kind = CastKind::ptr_addr;
    null_address.lhs = null_pointer_id;
    [[maybe_unused]] const GlobalConstantId null_address_constant =
        add_global_constant(module, GlobalConstant {"null_address", "unit_null_address", usize, add_value(module, null_address)});

    const ValueId literal_for_float = add_value(module, integer_value(i32, "9"));
    Value int_to_float_constant;
    int_to_float_constant.kind = ValueKind::cast;
    int_to_float_constant.type = f64;
    int_to_float_constant.target_type = f64;
    int_to_float_constant.cast_kind = CastKind::numeric;
    int_to_float_constant.lhs = literal_for_float;
    const ValueId float_constant_value = add_value(module, int_to_float_constant);
    [[maybe_unused]] const GlobalConstantId float_constant =
        add_global_constant(module, GlobalConstant {"float_value", "unit_float_value", f64, float_constant_value});
    Value float_to_int_constant;
    float_to_int_constant.kind = ValueKind::cast;
    float_to_int_constant.type = i32;
    float_to_int_constant.target_type = i32;
    float_to_int_constant.cast_kind = CastKind::numeric;
    float_to_int_constant.lhs = float_constant_value;
    [[maybe_unused]] const GlobalConstantId int_constant =
        add_global_constant(module, GlobalConstant {"int_value", "unit_int_value", i32, add_value(module, float_to_int_constant)});

    Function sink = make_function(module, "sink", void_type, Linkage::extern_c, AbiCallConv::c);
    sink.symbol = "unit_sink";
    sink.signature_params.push_back(FunctionParam {"value", i32});
    module.functions.push_back(sink);
    const FunctionId sink_id {0};

    Function choose = make_function(module, "choose_phi", i32);
    FunctionBuilder choose_builder {module, choose};
    Value flag_param;
    flag_param.kind = ValueKind::param;
    flag_param.type = bool_type;
    flag_param.name = "flag";
    const ValueId flag = choose_builder.add(flag_param);
    Value lhs_param;
    lhs_param.kind = ValueKind::param;
    lhs_param.type = i32;
    lhs_param.name = "lhs";
    const ValueId lhs = choose_builder.add(lhs_param);
    Value rhs_param = lhs_param;
    rhs_param.name = "rhs";
    const ValueId rhs = choose_builder.add(rhs_param);
    choose.signature_params = {
        FunctionParam {"flag", bool_type},
        FunctionParam {"lhs", i32},
        FunctionParam {"rhs", i32},
    };
    choose.param_values = {flag, lhs, rhs};
    const BlockId choose_entry = choose_builder.block("entry");
    const BlockId choose_then = choose_builder.block("then");
    const BlockId choose_else = choose_builder.block("else");
    const BlockId choose_join = choose_builder.block("join");
    choose.blocks[choose_entry.value].terminator.kind = TerminatorKind::cond_branch;
    choose.blocks[choose_entry.value].terminator.condition = flag;
    choose.blocks[choose_entry.value].terminator.then_target = choose_then;
    choose.blocks[choose_entry.value].terminator.else_target = choose_else;
    choose.blocks[choose_then.value].terminator.kind = TerminatorKind::branch;
    choose.blocks[choose_then.value].terminator.target = choose_join;
    choose.blocks[choose_else.value].terminator.kind = TerminatorKind::branch;
    choose.blocks[choose_else.value].terminator.target = choose_join;
    Value phi;
    phi.kind = ValueKind::phi;
    phi.type = i32;
    phi.incoming = {PhiInput {choose_then, lhs}, PhiInput {choose_else, rhs}};
    const ValueId phi_id = choose_builder.add(phi);
    choose.blocks[choose_join.value].values = {phi_id};
    choose.blocks[choose_join.value].terminator.kind = TerminatorKind::return_;
    choose.blocks[choose_join.value].terminator.value = phi_id;
    module.functions.push_back(choose);

    Function ops = make_function(module, "runtime_ops", i32);
    FunctionBuilder builder {module, ops};
    Value op_flag_param = flag_param;
    op_flag_param.name = "flag";
    const ValueId op_flag = builder.add(op_flag_param);
    Value op_lhs_param = lhs_param;
    op_lhs_param.name = "lhs";
    const ValueId op_lhs = builder.add(op_lhs_param);
    Value op_rhs_param = lhs_param;
    op_rhs_param.name = "rhs";
    const ValueId op_rhs = builder.add(op_rhs_param);
    Value op_unsigned_param;
    op_unsigned_param.kind = ValueKind::param;
    op_unsigned_param.type = u32;
    op_unsigned_param.name = "u_lhs";
    const ValueId op_unsigned = builder.add(op_unsigned_param);
    Value op_float_param;
    op_float_param.kind = ValueKind::param;
    op_float_param.type = f64;
    op_float_param.name = "float_value";
    const ValueId op_float = builder.add(op_float_param);
    Value op_pointer_param;
    op_pointer_param.kind = ValueKind::param;
    op_pointer_param.type = ptr_i32;
    op_pointer_param.name = "ptr";
    const ValueId op_pointer = builder.add(op_pointer_param);
    ops.signature_params = {
        FunctionParam {"flag", bool_type},
        FunctionParam {"lhs", i32},
        FunctionParam {"rhs", i32},
        FunctionParam {"u_lhs", u32},
        FunctionParam {"float_value", f64},
        FunctionParam {"ptr", ptr_i32},
    };
    ops.param_values = {op_flag, op_lhs, op_rhs, op_unsigned, op_float, op_pointer};

    std::vector<ValueId> values;
    auto add_and_keep = [&](Value value) {
        const ValueId id = builder.add(std::move(value));
        values.push_back(id);
        return id;
    };

    Value store;
    store.kind = ValueKind::store;
    store.type = void_type;
    store.object = op_pointer;
    store.lhs = op_lhs;
    add_and_keep(store);

    Value load;
    load.kind = ValueKind::load;
    load.type = i32;
    load.object = op_pointer;
    const ValueId loaded = add_and_keep(load);

    for (const UnaryOp op : {UnaryOp::logical_not, UnaryOp::numeric_negate, UnaryOp::bitwise_not, UnaryOp::address_of, UnaryOp::dereference}) {
        Value unary;
        unary.kind = ValueKind::unary;
        unary.unary_op = op;
        unary.lhs = op == UnaryOp::logical_not ? op_flag : (op == UnaryOp::numeric_negate ? op_float : op_lhs);
        unary.type = op == UnaryOp::logical_not ? bool_type : (op == UnaryOp::numeric_negate ? f64 : i32);
        if (op == UnaryOp::address_of || op == UnaryOp::dereference) {
            unary.lhs = op_pointer;
            unary.type = ptr_i32;
        }
        add_and_keep(unary);
    }

    for (const BinaryOp op : {
             BinaryOp::less_equal,
             BinaryOp::greater,
             BinaryOp::greater_equal,
             BinaryOp::equal,
             BinaryOp::not_equal,
         }) {
        Value cmp;
        cmp.kind = ValueKind::binary;
        cmp.type = bool_type;
        cmp.binary_op = op;
        cmp.lhs = op_float;
        cmp.rhs = op_float;
        add_and_keep(cmp);
    }

    for (const BinaryOp op : {BinaryOp::add, BinaryOp::sub, BinaryOp::mul, BinaryOp::div}) {
        Value binary;
        binary.kind = ValueKind::binary;
        binary.type = f64;
        binary.binary_op = op;
        binary.lhs = op_float;
        binary.rhs = op_float;
        add_and_keep(binary);
    }

    Value pair_a;
    pair_a.kind = ValueKind::aggregate;
    pair_a.type = pair_type;
    pair_a.fields = {
        {"left", add_and_keep(integer_value(i32, "1"))},
        {"right", add_and_keep(integer_value(i32, "2"))},
    };
    const ValueId pair_a_id = add_and_keep(pair_a);
    Value pair_b = pair_a;
    pair_b.fields = {
        {"left", add_and_keep(integer_value(i32, "3"))},
        {"right", add_and_keep(integer_value(i32, "4"))},
    };
    const ValueId pair_b_id = add_and_keep(pair_b);
    Value struct_equal;
    struct_equal.kind = ValueKind::binary;
    struct_equal.type = bool_type;
    struct_equal.binary_op = BinaryOp::equal;
    struct_equal.lhs = pair_a_id;
    struct_equal.rhs = pair_b_id;
    add_and_keep(struct_equal);

    for (const auto [kind, result_type, operand] : {
             std::tuple<CastKind, TypeHandle, ValueId> {CastKind::numeric, i64, op_lhs},
             std::tuple<CastKind, TypeHandle, ValueId> {CastKind::numeric, f64, op_lhs},
             std::tuple<CastKind, TypeHandle, ValueId> {CastKind::numeric, f64, op_unsigned},
             std::tuple<CastKind, TypeHandle, ValueId> {CastKind::numeric, i32, op_float},
             std::tuple<CastKind, TypeHandle, ValueId> {CastKind::numeric, f32, op_float},
             std::tuple<CastKind, TypeHandle, ValueId> {CastKind::ptr_addr, usize, op_pointer},
         }) {
        Value cast;
        cast.kind = ValueKind::cast;
        cast.type = result_type;
        cast.target_type = result_type;
        cast.cast_kind = kind;
        cast.lhs = operand;
        add_and_keep(cast);
    }

    Value address_cast;
    address_cast.kind = ValueKind::cast;
    address_cast.type = usize;
    address_cast.target_type = usize;
    address_cast.cast_kind = CastKind::ptr_addr;
    address_cast.lhs = op_pointer;
    const ValueId address = add_and_keep(address_cast);
    Value pointer_from_address;
    pointer_from_address.kind = ValueKind::cast;
    pointer_from_address.type = ptr_i32;
    pointer_from_address.target_type = ptr_i32;
    pointer_from_address.cast_kind = CastKind::ptr_from_addr;
    pointer_from_address.lhs = address;
    add_and_keep(pointer_from_address);
    Value pointer_bitcast = pointer_from_address;
    pointer_bitcast.cast_kind = CastKind::bitcast;
    pointer_bitcast.lhs = op_pointer;
    add_and_keep(pointer_bitcast);

    Value index;
    index.kind = ValueKind::index_addr;
    index.type = ptr_i32;
    index.object = op_pointer;
    index.index = op_rhs;
    add_and_keep(index);

    Value void_call;
    void_call.kind = ValueKind::call;
    void_call.type = void_type;
    void_call.call_target = sink_id;
    void_call.args = {loaded};
    add_and_keep(void_call);

    Value ref_runtime;
    ref_runtime.kind = ValueKind::constant_ref;
    ref_runtime.type = i32;
    ref_runtime.constant = base_constant;
    add_and_keep(ref_runtime);

    Value return_cast;
    return_cast.kind = ValueKind::cast;
    return_cast.type = i32;
    return_cast.target_type = i32;
    return_cast.cast_kind = CastKind::numeric;
    return_cast.lhs = op_float;
    const ValueId result = add_and_keep(return_cast);

    const BlockId entry = builder.block("entry");
    ops.blocks[entry.value].values = std::move(values);
    ops.blocks[entry.value].terminator.kind = TerminatorKind::return_;
    ops.blocks[entry.value].terminator.value = result;
    module.functions.push_back(ops);

    auto llvm_ir = backend::emit_llvm_ir({&module, "unit_backend_runtime"});
    ASSERT_TRUE(llvm_ir) << llvm_ir.error().message;
    expect_contains_all(llvm_ir.value().text, {
        "@unit_text",
        "@unit_ref",
        "phi i32",
        "sitofp",
        "uitofp",
        "fptosi",
        "fptrunc",
        "ptrtoint",
        "inttoptr",
        "call void @unit_sink",
    });
}

} // namespace aurex::test
