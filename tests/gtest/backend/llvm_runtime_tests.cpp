#include <aurex/backend/llvm_backend.hpp>
#include <gtest/support/ir_test_helpers.hpp>

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

namespace aurex::backend {
[[nodiscard]] bool parse_u64(std::string_view text, std::uint64_t& out) noexcept;
[[nodiscard]] std::string decode_string_literal(std::string_view literal, bool has_c_prefix);
[[nodiscard]] std::uint64_t parse_byte_literal(std::string_view literal);
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
    RecordLayout pair_record = record_layout(module, pair_type, "unit.Pair", "unit_Pair", false);
    pair_record.fields.push_back(record_field(module, "left", i32));
    pair_record.fields.push_back(record_field(module, "right", i32));
    append_record(module, pair_record);
    const TypeHandle ptr_pair = ptr(module, PointerMutability::mut, pair_type);
    const TypeHandle ptr_ptr_pair = ptr(module, PointerMutability::mut, ptr_pair);

    Value undef = module.make_value();
    undef.kind = ValueKind::undef;
    undef.type = i32;
    [[maybe_unused]] const GlobalConstantId undef_constant =
        add_global_constant(module, GlobalConstant {"undef", "unit_undef", i32, add_value(module, undef)});

    Value string_constant = module.make_value();
    string_constant.kind = ValueKind::string_literal;
    string_constant.type = str_type;
    set_text(module, string_constant, "\"constant\"");
    [[maybe_unused]] const GlobalConstantId text_constant =
        add_global_constant(module, GlobalConstant {"text", "unit_text", str_type, add_value(module, string_constant)});

    const ValueId base_value = add_value(module, integer_value(module, i32, "7"));
    const GlobalConstantId base_constant =
        add_global_constant(module, GlobalConstant {"base", "unit_base", i32, base_value});
    Value ref_value = module.make_value();
    ref_value.kind = ValueKind::constant_ref;
    ref_value.type = i32;
    ref_value.constant = base_constant;
    [[maybe_unused]] const GlobalConstantId ref_constant =
        add_global_constant(module, GlobalConstant {"ref", "unit_ref", i32, add_value(module, ref_value)});

    Value null_pointer = module.make_value();
    null_pointer.kind = ValueKind::null_literal;
    null_pointer.type = ptr_i32;
    const ValueId null_pointer_id = add_value(module, null_pointer);
    Value null_address = module.make_value();
    null_address.kind = ValueKind::cast;
    null_address.type = usize;
    null_address.target_type = usize;
    null_address.cast_kind = CastKind::ptr_addr;
    null_address.lhs = null_pointer_id;
    [[maybe_unused]] const GlobalConstantId null_address_constant =
        add_global_constant(module, GlobalConstant {"null_address", "unit_null_address", usize, add_value(module, null_address)});

    const ValueId literal_for_float = add_value(module, integer_value(module, i32, "9"));
    Value int_to_float_constant = module.make_value();
    int_to_float_constant.kind = ValueKind::cast;
    int_to_float_constant.type = f64;
    int_to_float_constant.target_type = f64;
    int_to_float_constant.cast_kind = CastKind::numeric;
    int_to_float_constant.lhs = literal_for_float;
    const ValueId float_constant_value = add_value(module, int_to_float_constant);
    [[maybe_unused]] const GlobalConstantId float_constant =
        add_global_constant(module, GlobalConstant {"float_value", "unit_float_value", f64, float_constant_value});
    Value float_to_int_constant = module.make_value();
    float_to_int_constant.kind = ValueKind::cast;
    float_to_int_constant.type = i32;
    float_to_int_constant.target_type = i32;
    float_to_int_constant.cast_kind = CastKind::numeric;
    float_to_int_constant.lhs = float_constant_value;
    [[maybe_unused]] const GlobalConstantId int_constant =
        add_global_constant(module, GlobalConstant {"int_value", "unit_int_value", i32, add_value(module, float_to_int_constant)});

    Function sink = make_function(module, "sink", void_type, Linkage::extern_c, AbiCallConv::c);
    set_symbol(module, sink, "unit_sink");
    sink.signature_params.push_back(function_param(module, "value", i32));
    append_function(module, sink);
    const FunctionId sink_id {0};

    Function choose = make_function(module, "choose_phi", i32);
    FunctionBuilder choose_builder {module, choose};
    Value flag_param = module.make_value();
    flag_param.kind = ValueKind::param;
    flag_param.type = bool_type;
    set_name(module, flag_param, "flag");
    const ValueId flag = choose_builder.add(flag_param);
    Value lhs_param = module.make_value();
    lhs_param.kind = ValueKind::param;
    lhs_param.type = i32;
    set_name(module, lhs_param, "lhs");
    const ValueId lhs = choose_builder.add(lhs_param);
    Value rhs_param = lhs_param;
    set_name(module, rhs_param, "rhs");
    const ValueId rhs = choose_builder.add(rhs_param);
    choose.signature_params = {
        function_param(module, "flag", bool_type),
        function_param(module, "lhs", i32),
        function_param(module, "rhs", i32),
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
    Value phi = module.make_value();
    phi.kind = ValueKind::phi;
    phi.type = i32;
    phi.incoming = {PhiInput {choose_then, lhs}, PhiInput {choose_else, rhs}};
    const ValueId phi_id = choose_builder.add(phi);
    choose.blocks[choose_join.value].values = {phi_id};
    choose.blocks[choose_join.value].terminator.kind = TerminatorKind::return_;
    choose.blocks[choose_join.value].terminator.value = phi_id;
    append_function(module, choose);

    Function ops = make_function(module, "runtime_ops", i32);
    FunctionBuilder builder {module, ops};
    Value op_flag_param = flag_param;
    set_name(module, op_flag_param, "flag");
    const ValueId op_flag = builder.add(op_flag_param);
    Value op_lhs_param = lhs_param;
    set_name(module, op_lhs_param, "lhs");
    const ValueId op_lhs = builder.add(op_lhs_param);
    Value op_rhs_param = lhs_param;
    set_name(module, op_rhs_param, "rhs");
    const ValueId op_rhs = builder.add(op_rhs_param);
    Value op_unsigned_param = module.make_value();
    op_unsigned_param.kind = ValueKind::param;
    op_unsigned_param.type = u32;
    set_name(module, op_unsigned_param, "u_lhs");
    const ValueId op_unsigned = builder.add(op_unsigned_param);
    Value op_unsigned_rhs_param = op_unsigned_param;
    set_name(module, op_unsigned_rhs_param, "u_rhs");
    const ValueId op_unsigned_rhs = builder.add(op_unsigned_rhs_param);
    Value op_float_param = module.make_value();
    op_float_param.kind = ValueKind::param;
    op_float_param.type = f64;
    set_name(module, op_float_param, "float_value");
    const ValueId op_float = builder.add(op_float_param);
    Value op_pointer_param = module.make_value();
    op_pointer_param.kind = ValueKind::param;
    op_pointer_param.type = ptr_i32;
    set_name(module, op_pointer_param, "ptr");
    const ValueId op_pointer = builder.add(op_pointer_param);
    ops.signature_params = {
        function_param(module, "flag", bool_type),
        function_param(module, "lhs", i32),
        function_param(module, "rhs", i32),
        function_param(module, "u_lhs", u32),
        function_param(module, "u_rhs", u32),
        function_param(module, "float_value", f64),
        function_param(module, "ptr", ptr_i32),
    };
    ops.param_values = {op_flag, op_lhs, op_rhs, op_unsigned, op_unsigned_rhs, op_float, op_pointer};

    std::vector<ValueId> values;
    auto add_and_keep = [&](const Value& value) {
        const ValueId id = builder.add(value);
        values.push_back(id);
        return id;
    };

    Value store = module.make_value();
    store.kind = ValueKind::store;
    store.type = void_type;
    store.object = op_pointer;
    store.lhs = op_lhs;
    add_and_keep(store);

    Value load = module.make_value();
    load.kind = ValueKind::load;
    load.type = i32;
    load.object = op_pointer;
    const ValueId loaded = add_and_keep(load);

    for (const UnaryOp op : {UnaryOp::logical_not, UnaryOp::numeric_negate, UnaryOp::bitwise_not, UnaryOp::address_of, UnaryOp::dereference}) {
        Value unary = module.make_value();
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
        Value cmp = module.make_value();
        cmp.kind = ValueKind::binary;
        cmp.type = bool_type;
        cmp.binary_op = op;
        cmp.lhs = op_float;
        cmp.rhs = op_float;
        add_and_keep(cmp);
    }
    for (const BinaryOp op : {BinaryOp::less_equal, BinaryOp::greater, BinaryOp::greater_equal}) {
        Value unsigned_cmp = module.make_value();
        unsigned_cmp.kind = ValueKind::binary;
        unsigned_cmp.type = bool_type;
        unsigned_cmp.binary_op = op;
        unsigned_cmp.lhs = op_unsigned;
        unsigned_cmp.rhs = op_unsigned_rhs;
        add_and_keep(unsigned_cmp);
    }

    for (const BinaryOp op : {BinaryOp::add, BinaryOp::sub, BinaryOp::mul, BinaryOp::div}) {
        Value binary = module.make_value();
        binary.kind = ValueKind::binary;
        binary.type = f64;
        binary.binary_op = op;
        binary.lhs = op_float;
        binary.rhs = op_float;
        add_and_keep(binary);
    }

    Value pair_a = module.make_value();
    pair_a.kind = ValueKind::aggregate;
    pair_a.type = pair_type;
    pair_a.fields = {
        field_value(module, "left", add_and_keep(integer_value(module, i32, "1"))),
        field_value(module, "right", add_and_keep(integer_value(module, i32, "2"))),
    };
    add_and_keep(pair_a);
    Value pair_b = pair_a;
    pair_b.fields = {
        field_value(module, "left", add_and_keep(integer_value(module, i32, "3"))),
        field_value(module, "right", add_and_keep(integer_value(module, i32, "4"))),
    };
    add_and_keep(pair_b);
    Value pair_a_address = module.make_value();
    pair_a_address.kind = ValueKind::alloca;
    pair_a_address.type = ptr(module, PointerMutability::mut, pair_type);
    const ValueId pair_a_address_id = add_and_keep(pair_a_address);
    Value pair_b_address = module.make_value();
    pair_b_address.kind = ValueKind::alloca;
    pair_b_address.type = ptr(module, PointerMutability::mut, pair_type);
    const ValueId pair_b_address_id = add_and_keep(pair_b_address);
    Value pointer_equal = module.make_value();
    pointer_equal.kind = ValueKind::binary;
    pointer_equal.type = bool_type;
    pointer_equal.binary_op = BinaryOp::equal;
    pointer_equal.lhs = pair_a_address_id;
    pointer_equal.rhs = pair_b_address_id;
    add_and_keep(pointer_equal);
    Value pointer_not_equal = pointer_equal;
    pointer_not_equal.binary_op = BinaryOp::not_equal;
    add_and_keep(pointer_not_equal);

    for (const BinaryOp op : {BinaryOp::logical_and, BinaryOp::logical_or}) {
        Value logical = module.make_value();
        logical.kind = ValueKind::binary;
        logical.type = bool_type;
        logical.binary_op = op;
        logical.lhs = op_flag;
        logical.rhs = op_flag;
        add_and_keep(logical);
    }

    for (const auto [kind, result_type, operand] : {
            std::tuple<CastKind, TypeHandle, ValueId> {CastKind::numeric, i64, op_lhs},
            std::tuple<CastKind, TypeHandle, ValueId> {CastKind::numeric, f64, op_lhs},
            std::tuple<CastKind, TypeHandle, ValueId> {CastKind::numeric, f64, op_unsigned},
            std::tuple<CastKind, TypeHandle, ValueId> {CastKind::numeric, i32, op_float},
            std::tuple<CastKind, TypeHandle, ValueId> {CastKind::numeric, u32, op_float},
            std::tuple<CastKind, TypeHandle, ValueId> {CastKind::numeric, f32, op_float},
            std::tuple<CastKind, TypeHandle, ValueId> {CastKind::ptr_addr, usize, op_pointer},
        }) {
        Value cast = module.make_value();
        cast.kind = ValueKind::cast;
        cast.type = result_type;
        cast.target_type = result_type;
        cast.cast_kind = kind;
        cast.lhs = operand;
        add_and_keep(cast);
    }

    Value address_cast = module.make_value();
    address_cast.kind = ValueKind::cast;
    address_cast.type = usize;
    address_cast.target_type = usize;
    address_cast.cast_kind = CastKind::ptr_addr;
    address_cast.lhs = op_pointer;
    const ValueId address = add_and_keep(address_cast);
    Value pointer_from_address = module.make_value();
    pointer_from_address.kind = ValueKind::cast;
    pointer_from_address.type = ptr_i32;
    pointer_from_address.target_type = ptr_i32;
    pointer_from_address.cast_kind = CastKind::paddr;
    pointer_from_address.lhs = address;
    add_and_keep(pointer_from_address);
    Value pointer_bcast = pointer_from_address;
    pointer_bcast.cast_kind = CastKind::bcast;
    pointer_bcast.lhs = op_pointer;
    add_and_keep(pointer_bcast);

    Value pointer_slot = module.make_value();
    pointer_slot.kind = ValueKind::alloca;
    pointer_slot.type = ptr_ptr_pair;
    const ValueId pointer_slot_id = add_and_keep(pointer_slot);
    Value pointer_slot_field = module.make_value();
    pointer_slot_field.kind = ValueKind::field_addr;
    pointer_slot_field.type = ptr(module, PointerMutability::mut, i32);
    pointer_slot_field.object = pointer_slot_id;
    set_name(module, pointer_slot_field, "left");
    add_and_keep(pointer_slot_field);

    Value index = module.make_value();
    index.kind = ValueKind::index_addr;
    index.type = ptr_i32;
    index.object = op_pointer;
    index.index = op_rhs;
    add_and_keep(index);

    Value void_call = module.make_value();
    void_call.kind = ValueKind::call;
    void_call.type = void_type;
    void_call.call_target = sink_id;
    void_call.args = {loaded};
    add_and_keep(void_call);

    Value unnamed_call = module.make_value();
    unnamed_call.kind = ValueKind::call;
    unnamed_call.type = i32;
    unnamed_call.call_target = FunctionId {1};
    unnamed_call.args = {op_flag, op_lhs, op_rhs};
    add_and_keep(unnamed_call);

    Value ref_runtime = module.make_value();
    ref_runtime.kind = ValueKind::constant_ref;
    ref_runtime.type = i32;
    ref_runtime.constant = base_constant;
    add_and_keep(ref_runtime);

    Value return_cast = module.make_value();
    return_cast.kind = ValueKind::cast;
    return_cast.type = i32;
    return_cast.target_type = i32;
    return_cast.cast_kind = CastKind::numeric;
    return_cast.lhs = op_float;
    const ValueId result = add_and_keep(return_cast);

    const BlockId entry = builder.block("entry");
    assign_ir_vector(ops.blocks[entry.value].values, values);
    ops.blocks[entry.value].terminator.kind = TerminatorKind::return_;
    ops.blocks[entry.value].terminator.value = result;
    append_function(module, ops);

    auto llvm_ir = backend::emit_llvm_ir({&module, "unit_backend_runtime"});
    ASSERT_TRUE(llvm_ir) << llvm_ir.error().message;
    expect_contains_all(llvm_ir.value().text, {
        "@unit_text",
        "@unit_ref",
        "phi i32",
        "sitofp",
        "uitofp",
        "fptoui",
        "fptosi",
        "fptrunc",
        "ptrtoint",
        "inttoptr",
        "call void @unit_sink",
        "icmp ule",
        "icmp ugt",
        "icmp uge",
    });
}

TEST(CoreUnit, LlvmBackendCoversRuntimeStringProjectionAndBinaryEdges) {
    Module module;
    const TypeHandle bool_type = builtin(module, BuiltinType::bool_);
    const TypeHandle i32 = builtin(module, BuiltinType::i32);
    const TypeHandle f64 = builtin(module, BuiltinType::f64);
    const TypeHandle u8 = builtin(module, BuiltinType::u8);
    const TypeHandle usize = builtin(module, BuiltinType::usize);
    const TypeHandle str_type = builtin(module, BuiltinType::str);
    const TypeHandle const_u8_ptr = ptr(module, PointerMutability::const_, u8);

    Function function = make_function(module, "runtime_edges", i32);
    FunctionBuilder builder {module, function};
    Value int_lhs_param = module.make_value();
    int_lhs_param.kind = ValueKind::param;
    int_lhs_param.type = i32;
    set_name(module, int_lhs_param, "lhs");
    const ValueId int_lhs = builder.add(int_lhs_param);
    Value int_rhs_param = int_lhs_param;
    set_name(module, int_rhs_param, "rhs");
    const ValueId int_rhs = builder.add(int_rhs_param);
    Value float_lhs_param = module.make_value();
    float_lhs_param.kind = ValueKind::param;
    float_lhs_param.type = f64;
    set_name(module, float_lhs_param, "float_lhs");
    const ValueId float_lhs = builder.add(float_lhs_param);
    Value float_rhs_param = float_lhs_param;
    set_name(module, float_rhs_param, "float_rhs");
    const ValueId float_rhs = builder.add(float_rhs_param);
    Value text_param = module.make_value();
    text_param.kind = ValueKind::param;
    text_param.type = str_type;
    set_name(module, text_param, "text");
    const ValueId text = builder.add(text_param);
    function.signature_params = {
        function_param(module, "lhs", i32),
        function_param(module, "rhs", i32),
        function_param(module, "float_lhs", f64),
        function_param(module, "float_rhs", f64),
        function_param(module, "text", str_type),
    };
    function.param_values = {int_lhs, int_rhs, float_lhs, float_rhs, text};

    std::vector<ValueId> values;
    auto add_and_keep = [&](const Value& value) {
        const ValueId id = builder.add(value);
        values.push_back(id);
        return id;
    };

    const ValueId runtime_string_id = text;

    Value str_data = module.make_value();
    str_data.kind = ValueKind::str_data;
    str_data.type = const_u8_ptr;
    str_data.object = runtime_string_id;
    const ValueId str_data_id = add_and_keep(str_data);

    Value str_byte_len = module.make_value();
    str_byte_len.kind = ValueKind::str_byte_len;
    str_byte_len.type = usize;
    str_byte_len.object = runtime_string_id;
    const ValueId str_byte_len_id = add_and_keep(str_byte_len);

    Value from_bytes = module.make_value();
    from_bytes.kind = ValueKind::str_from_bytes_unchecked;
    from_bytes.type = str_type;
    from_bytes.args = {str_data_id, str_byte_len_id};
    add_and_keep(from_bytes);

    Value str_slice = module.make_value();
    str_slice.kind = ValueKind::str_slice_checked;
    str_slice.type = str_type;
    str_slice.object = runtime_string_id;
    str_slice.lhs = str_byte_len_id;
    str_slice.rhs = str_byte_len_id;
    add_and_keep(str_slice);

    Value float_less = module.make_value();
    float_less.kind = ValueKind::binary;
    float_less.type = bool_type;
    float_less.binary_op = BinaryOp::less;
    float_less.lhs = float_lhs;
    float_less.rhs = float_rhs;
    add_and_keep(float_less);

    Value int_xor = module.make_value();
    int_xor.kind = ValueKind::binary;
    int_xor.type = i32;
    int_xor.binary_op = BinaryOp::bit_xor;
    int_xor.lhs = int_lhs;
    int_xor.rhs = int_rhs;
    add_and_keep(int_xor);

    const ValueId result = add_and_keep(integer_value(module, i32, "0"));
    const BlockId entry = builder.block("entry");
    assign_ir_vector(function.blocks[entry.value].values, values);
    function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
    function.blocks[entry.value].terminator.value = result;
    append_function(module, function);

    auto llvm_ir = backend::emit_llvm_ir({&module, "unit_backend_runtime_edges"});
    ASSERT_TRUE(llvm_ir) << llvm_ir.error().message;
    expect_contains_all(llvm_ir.value().text, {
        "extractvalue",
        "insertvalue",
        "str.data",
        "str.len",
        "str.slice.ok",
        "__aurex_utf8_boundary",
        "fcmp olt",
        "xor",
    });
}

} // namespace aurex::test
