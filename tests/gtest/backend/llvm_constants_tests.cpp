#include <aurex/backend/llvm_backend.hpp>
#include <gtest/support/ir_test_helpers.hpp>

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

TEST(CoreUnit, LlvmBackendCoversConstantsCastsStringsAndNullModule) {
    {
        auto result = backend::emit_llvm_ir({nullptr, "null"});
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code, ErrorCode::internal_error);
        expect_contains(result.error().message, "null IR module");
    }
    {
        Module module;
        const TypeHandle bool_type = builtin(module, BuiltinType::bool_);
        const TypeHandle u8 = builtin(module, BuiltinType::u8);
        const TypeHandle i8 = builtin(module, BuiltinType::i8);
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const TypeHandle u32 = builtin(module, BuiltinType::u32);
        const TypeHandle i64 = builtin(module, BuiltinType::i64);
        const TypeHandle u64 = builtin(module, BuiltinType::u64);
        const TypeHandle f32 = builtin(module, BuiltinType::f32);
        const TypeHandle f64 = builtin(module, BuiltinType::f64);
        const TypeHandle usize = builtin(module, BuiltinType::usize);
        const TypeHandle str_type = builtin(module, BuiltinType::str);
        const TypeHandle char_type = builtin(module, BuiltinType::char_);
        const TypeHandle ptr_i32 = ptr(module, PointerMutability::mut, i32);
        const TypeHandle array_i32 = module.types.array(2, i32);
        const TypeHandle nested_array_i32 = module.types.array(3, array_i32);
        const TypeHandle tag_enum_type = module.types.named_enum("unit.Tag", "unit_Tag");
        module.types.set_enum_underlying(tag_enum_type, u8);
        const TypeHandle pair_type = module.types.named_struct("unit.Pair", "unit_Pair", false);
        module.records.push_back(RecordLayout {
            pair_type,
            "unit.Pair",
            "unit_Pair",
            false,
            {RecordField {"left", i32}, RecordField {"right", i32}},
        });

        const ValueId true_id = add_value(module, bool_value(module, true));
        [[maybe_unused]] const GlobalConstantId flag_constant =
            add_global_constant(module, GlobalConstant {"flag", "unit_flag", bool_type, true_id});
        Value byte;
        byte.kind = ValueKind::byte_literal;
        byte.type = u8;
        byte.text = "b'\\n'";
        const ValueId byte_id = add_value(module, byte);
        [[maybe_unused]] const GlobalConstantId byte_constant =
            add_global_constant(module, GlobalConstant {"byte", "unit_byte", u8, byte_id});
        Value null_value;
        null_value.kind = ValueKind::null_literal;
        null_value.type = ptr_i32;
        const ValueId null_id = add_value(module, null_value);
        [[maybe_unused]] const GlobalConstantId ptr_constant =
            add_global_constant(module, GlobalConstant {"ptr", "unit_ptr", ptr_i32, null_id});
        Value cstr;
        cstr.kind = ValueKind::c_string_literal;
        cstr.type = ptr(module, PointerMutability::const_, u8);
        cstr.text = "c\"abc\"";
        const ValueId cstr_id = add_value(module, cstr);
        [[maybe_unused]] const GlobalConstantId cstr_constant =
            add_global_constant(module, GlobalConstant {"cstr", "unit_cstr", cstr.type, cstr_id});
        Value raw_str;
        raw_str.kind = ValueKind::raw_string_literal;
        raw_str.type = str_type;
        raw_str.text = "r\"C:\\tmp\\a\"";
        const ValueId raw_str_id = add_value(module, raw_str);
        [[maybe_unused]] const GlobalConstantId raw_str_constant =
            add_global_constant(module, GlobalConstant {"raw_str", "unit_raw_str", str_type, raw_str_id});
        Value char_value;
        char_value.kind = ValueKind::char_literal;
        char_value.type = char_type;
        char_value.text = "'\\u{03BB}'";
        const ValueId char_id = add_value(module, char_value);
        [[maybe_unused]] const GlobalConstantId char_constant =
            add_global_constant(module, GlobalConstant {"char_value", "unit_char_value", char_type, char_id});
        Value cast;
        cast.kind = ValueKind::cast;
        cast.type = i64;
        cast.target_type = i64;
        cast.cast_kind = CastKind::numeric;
        cast.lhs = add_value(module, integer_value(i32, "9"));
        const ValueId cast_id = add_value(module, cast);
        [[maybe_unused]] const GlobalConstantId wide_constant =
            add_global_constant(module, GlobalConstant {"wide", "unit_wide", i64, cast_id});

        auto add_cast_constant = [&](
                                     const std::string& name,
                                     const std::string& symbol,
                                     const TypeHandle target_type,
                                     const ValueId operand,
                                     const CastKind cast_kind = CastKind::numeric
                                 ) {
            Value value;
            value.kind = ValueKind::cast;
            value.type = target_type;
            value.target_type = target_type;
            value.cast_kind = cast_kind;
            value.lhs = operand;
            return add_global_constant(module, GlobalConstant {name, symbol, target_type, add_value(module, value)});
        };
        [[maybe_unused]] const GlobalConstantId same_constant =
            add_cast_constant("same_i32", "unit_same_i32", i32, add_value(module, integer_value(i32, "5")));
        [[maybe_unused]] const GlobalConstantId trunc_constant =
            add_cast_constant("trunc_i8", "unit_trunc_i8", i8, add_value(module, integer_value(i64, "257")));
        [[maybe_unused]] const GlobalConstantId zext_constant =
            add_cast_constant("zext_u64", "unit_zext_u64", u64, add_value(module, integer_value(u32, "9")));
        [[maybe_unused]] const GlobalConstantId unsigned_float_constant =
            add_cast_constant("u32_to_f64", "unit_u32_to_f64", f64, add_value(module, integer_value(u32, "11")));
        const GlobalConstantId f32_constant =
            add_cast_constant("i32_to_f32", "unit_i32_to_f32", f32, add_value(module, integer_value(i32, "13")));
        Value f32_ref;
        f32_ref.kind = ValueKind::constant_ref;
        f32_ref.type = f32;
        f32_ref.constant = f32_constant;
        [[maybe_unused]] const GlobalConstantId f32_to_f64_constant =
            add_cast_constant("f32_to_f64", "unit_f32_to_f64", f64, add_value(module, f32_ref));
        const GlobalConstantId f64_constant =
            add_cast_constant("i32_to_f64", "unit_i32_to_f64", f64, add_value(module, integer_value(i32, "17")));
        Value f64_ref;
        f64_ref.kind = ValueKind::constant_ref;
        f64_ref.type = f64;
        f64_ref.constant = f64_constant;
        [[maybe_unused]] const GlobalConstantId f64_to_u32_constant =
            add_cast_constant("f64_to_u32", "unit_f64_to_u32", u32, add_value(module, f64_ref));
        [[maybe_unused]] const GlobalConstantId f64_to_f32_constant =
            add_cast_constant("f64_to_f32", "unit_f64_to_f32", f32, add_value(module, f64_ref));
        [[maybe_unused]] const GlobalConstantId bcast_constant =
            add_cast_constant("u32_to_f32_bits", "unit_u32_to_f32_bits", f32, add_value(module, integer_value(u32, "0")), CastKind::bcast);
        [[maybe_unused]] const GlobalConstantId ptrat_constant =
            add_cast_constant("ptrat", "unit_ptrat", ptr_i32, add_value(module, integer_value(usize, "4096")), CastKind::paddr);
        [[maybe_unused]] const GlobalConstantId tag_constant =
            add_global_constant(module, GlobalConstant {"tag", "unit_tag", tag_enum_type, add_value(module, integer_value(tag_enum_type, "1"))});

        Value aggregate;
        aggregate.kind = ValueKind::aggregate;
        aggregate.type = pair_type;
        aggregate.fields = {
            {"left", add_value(module, integer_value(i32, "1"))},
            {"right", add_value(module, integer_value(i32, "2"))},
        };
        const ValueId aggregate_id = add_value(module, aggregate);
        [[maybe_unused]] const GlobalConstantId pair_constant =
            add_global_constant(module, GlobalConstant {"pair", "unit_pair", pair_type, aggregate_id});
        Value sizeof_value;
        sizeof_value.kind = ValueKind::size_of;
        sizeof_value.type = builtin(module, BuiltinType::usize);
        sizeof_value.target_type = nested_array_i32;
        const ValueId size_id = add_value(module, sizeof_value);
        [[maybe_unused]] const GlobalConstantId size_constant =
            add_global_constant(module, GlobalConstant {"size", "unit_size", sizeof_value.type, size_id});
        Value align_value = sizeof_value;
        align_value.kind = ValueKind::align_of;
        align_value.target_type = pair_type;
        const ValueId align_id = add_value(module, align_value);
        [[maybe_unused]] const GlobalConstantId align_constant =
            add_global_constant(module, GlobalConstant {"align", "unit_align", align_value.type, align_id});

        Function external = make_function(module, "external", i32, Linkage::extern_c, AbiCallConv::c);
        external.symbol = "unit_external";
        module.functions.push_back(external);

        Function function = make_function(module, "exercise", i32);
        FunctionBuilder builder {module, function};
        Value lhs_param;
        lhs_param.kind = ValueKind::param;
        lhs_param.type = i32;
        lhs_param.name = "lhs";
        const ValueId lhs = builder.add(lhs_param);
        Value rhs_param = lhs_param;
        rhs_param.name = "rhs";
        const ValueId rhs = builder.add(rhs_param);
        Value u_lhs_param;
        u_lhs_param.kind = ValueKind::param;
        u_lhs_param.type = u32;
        u_lhs_param.name = "u_lhs";
        const ValueId u_lhs = builder.add(u_lhs_param);
        Value u_rhs_param = u_lhs_param;
        u_rhs_param.name = "u_rhs";
        const ValueId u_rhs = builder.add(u_rhs_param);
        function.signature_params = {
            FunctionParam {"lhs", i32},
            FunctionParam {"rhs", i32},
            FunctionParam {"u_lhs", u32},
            FunctionParam {"u_rhs", u32},
        };
        function.param_values = {lhs, rhs, u_lhs, u_rhs};

        std::vector<ValueId> values;
        for (const BinaryOp op : {
                 BinaryOp::div,
                 BinaryOp::mod,
                 BinaryOp::shl,
                 BinaryOp::shr,
                 BinaryOp::bit_and,
                 BinaryOp::bit_xor,
                 BinaryOp::bit_or,
             }) {
            Value binary;
            binary.kind = ValueKind::binary;
            binary.type = i32;
            binary.binary_op = op;
            binary.lhs = lhs;
            binary.rhs = rhs;
            values.push_back(builder.add(binary));
        }
        for (const BinaryOp op : {BinaryOp::div, BinaryOp::mod, BinaryOp::shr}) {
            Value binary;
            binary.kind = ValueKind::binary;
            binary.type = u32;
            binary.binary_op = op;
            binary.lhs = u_lhs;
            binary.rhs = u_rhs;
            values.push_back(builder.add(binary));
        }
        Value unsigned_less;
        unsigned_less.kind = ValueKind::binary;
        unsigned_less.type = bool_type;
        unsigned_less.binary_op = BinaryOp::less;
        unsigned_less.lhs = u_lhs;
        unsigned_less.rhs = u_rhs;
        values.push_back(builder.add(unsigned_less));
        Value float_cast;
        float_cast.kind = ValueKind::cast;
        float_cast.type = f64;
        float_cast.target_type = f64;
        float_cast.cast_kind = CastKind::numeric;
        float_cast.lhs = lhs;
        values.push_back(builder.add(float_cast));
        Value runtime_string;
        runtime_string.kind = ValueKind::string_literal;
        runtime_string.type = str_type;
        runtime_string.text = "\"hi\"";
        values.push_back(builder.add(runtime_string));
        Value runtime_raw_string = runtime_string;
        runtime_raw_string.kind = ValueKind::raw_string_literal;
        runtime_raw_string.text = "r\"C:\\tmp\\a\"";
        values.push_back(builder.add(runtime_raw_string));
        Value runtime_char;
        runtime_char.kind = ValueKind::char_literal;
        runtime_char.type = char_type;
        runtime_char.text = "'λ'";
        values.push_back(builder.add(runtime_char));
        Value local_array;
        local_array.kind = ValueKind::alloca;
        local_array.type = ptr(module, PointerMutability::mut, array_i32);
        const ValueId local_array_id = builder.add(local_array);
        values.push_back(local_array_id);
        Value index;
        index.kind = ValueKind::index_addr;
        index.type = ptr_i32;
        index.object = local_array_id;
        index.index = rhs;
        values.push_back(builder.add(index));
        Value call_name;
        call_name.kind = ValueKind::call;
        call_name.type = i32;
        call_name.name = "named_external";
        call_name.call_target = FunctionId {0};
        values.push_back(builder.add(call_name));
        const ValueId result = builder.add(integer_value(i32, "0"));
        values.push_back(result);
        const BlockId entry = builder.block("entry");
        function.blocks[entry.value].values = values;
        function.blocks[entry.value].terminator.kind = TerminatorKind::return_;
        function.blocks[entry.value].terminator.value = result;
        module.functions.push_back(function);

        auto llvm_ir = backend::emit_llvm_ir({&module, "unit_backend"});
        ASSERT_TRUE(llvm_ir) << llvm_ir.error().message;
        expect_contains_all(llvm_ir.value().text, {
            "@unit_flag",
            "@unit_byte",
            "@unit_ptr",
            "@unit_raw_str",
            "@unit_char_value",
            "@unit_pair",
            "@unit_size",
            "@unit_align",
            "@unit_tag",
            "@unit_same_i32",
            "@unit_trunc_i8",
            "@unit_zext_u64",
            "@unit_u32_to_f64",
            "@unit_f32_to_f64",
            "@unit_f64_to_u32",
            "@unit_f64_to_f32",
            "@unit_u32_to_f32_bits",
            "@unit_ptrat",
            "%unit_Pair = type",
            "and",
            "xor",
            "shl",
            "lshr",
            "srem",
            "udiv",
            "urem",
            "getelementptr",
            "str.data",
        });
    }
}

TEST(CoreUnit, LlvmBackendCoversConstantBinaryCastAndFloatLiteralEdges) {
    Module module;
    const TypeHandle bool_type = builtin(module, BuiltinType::bool_);
    const TypeHandle i32 = builtin(module, BuiltinType::i32);
    const TypeHandle u32 = builtin(module, BuiltinType::u32);
    const TypeHandle i64 = builtin(module, BuiltinType::i64);
    const TypeHandle f64 = builtin(module, BuiltinType::f64);
    const TypeHandle ptr_i32 = ptr(module, PointerMutability::mut, i32);
    const TypeHandle ptr_i64 = ptr(module, PointerMutability::mut, i64);

    auto add_float_literal = [&](const std::string& text) {
        Value value;
        value.kind = ValueKind::float_literal;
        value.type = f64;
        value.text = text;
        return add_value(module, value);
    };
    auto add_binary_constant = [&](
                                   const std::string& name,
                                   const std::string& symbol,
                                   const TypeHandle result_type,
                                   const BinaryOp op,
                                   const ValueId lhs,
                                   const ValueId rhs
                               ) {
        Value value;
        value.kind = ValueKind::binary;
        value.type = result_type;
        value.binary_op = op;
        value.lhs = lhs;
        value.rhs = rhs;
        return add_global_constant(module, GlobalConstant {name, symbol, result_type, add_value(module, value)});
    };
    auto add_cast_constant = [&](
                                 const std::string& name,
                                 const std::string& symbol,
                                 const TypeHandle result_type,
                                 const ValueId operand,
                                 const CastKind cast_kind = CastKind::numeric
                             ) {
        Value value;
        value.kind = ValueKind::cast;
        value.type = result_type;
        value.target_type = result_type;
        value.cast_kind = cast_kind;
        value.lhs = operand;
        return add_global_constant(module, GlobalConstant {name, symbol, result_type, add_value(module, value)});
    };
    auto add_unary_constant = [&](
                                 const std::string& name,
                                 const std::string& symbol,
                                 const TypeHandle result_type,
                                 const UnaryOp unary_op,
                                 const ValueId operand
                             ) {
        Value value;
        value.kind = ValueKind::unary;
        value.type = result_type;
        value.unary_op = unary_op;
        value.lhs = operand;
        return add_global_constant(module, GlobalConstant {name, symbol, result_type, add_value(module, value)});
    };

    const ValueId int_one = add_value(module, integer_value(i32, "1"));
    const ValueId int_two = add_value(module, integer_value(i32, "2"));
    const ValueId int_three = add_value(module, integer_value(i32, "3"));
    const ValueId float_one = add_float_literal("1.5");
    const ValueId float_two = add_float_literal("2.5");
    const ValueId float_same = add_float_literal("3.5");
    Value null_pointer;
    null_pointer.kind = ValueKind::null_literal;
    null_pointer.type = ptr_i32;
    const ValueId null_pointer_id = add_value(module, null_pointer);

    [[maybe_unused]] const GlobalConstantId float_less_constant =
        add_binary_constant("float_less", "unit_float_less", bool_type, BinaryOp::less, float_one, float_two);
    [[maybe_unused]] const GlobalConstantId int_less_constant =
        add_binary_constant("int_less", "unit_int_less", bool_type, BinaryOp::less, int_one, int_two);
    [[maybe_unused]] const GlobalConstantId float_less_equal_constant =
        add_binary_constant("float_less_equal", "unit_float_less_equal", bool_type, BinaryOp::less_equal, float_one, float_two);
    [[maybe_unused]] const GlobalConstantId int_less_equal_constant =
        add_binary_constant("int_less_equal", "unit_int_less_equal", bool_type, BinaryOp::less_equal, int_one, int_two);
    [[maybe_unused]] const GlobalConstantId float_greater_equal_constant =
        add_binary_constant("float_greater_equal", "unit_float_greater_equal", bool_type, BinaryOp::greater_equal, float_two, float_one);
    [[maybe_unused]] const GlobalConstantId int_greater_equal_constant =
        add_binary_constant("int_greater_equal", "unit_int_greater_equal", bool_type, BinaryOp::greater_equal, int_two, int_one);
    [[maybe_unused]] const GlobalConstantId float_greater_constant =
        add_binary_constant("float_greater", "unit_float_greater", bool_type, BinaryOp::greater, float_two, float_one);
    [[maybe_unused]] const GlobalConstantId int_greater_constant =
        add_binary_constant("int_greater", "unit_int_greater", bool_type, BinaryOp::greater, int_two, int_one);
    [[maybe_unused]] const GlobalConstantId float_equal_constant =
        add_binary_constant("float_equal", "unit_float_equal", bool_type, BinaryOp::equal, float_same, float_same);
    [[maybe_unused]] const GlobalConstantId int_equal_constant =
        add_binary_constant("int_equal", "unit_int_equal", bool_type, BinaryOp::equal, int_three, int_three);
    [[maybe_unused]] const GlobalConstantId float_not_equal_constant =
        add_binary_constant("float_not_equal", "unit_float_not_equal", bool_type, BinaryOp::not_equal, float_one, float_two);
    [[maybe_unused]] const GlobalConstantId int_not_equal_constant =
        add_binary_constant("int_not_equal", "unit_int_not_equal", bool_type, BinaryOp::not_equal, int_one, int_two);
    [[maybe_unused]] const GlobalConstantId u32_xor_constant = add_binary_constant(
        "u32_xor",
        "unit_u32_xor",
        u32,
        BinaryOp::bit_xor,
        add_value(module, integer_value(u32, "10")),
        add_value(module, integer_value(u32, "12"))
    );
    [[maybe_unused]] const GlobalConstantId float_add_constant =
        add_binary_constant("float_add", "unit_float_add", f64, BinaryOp::add, add_float_literal("1.25"), add_float_literal("2.5"));
    [[maybe_unused]] const GlobalConstantId float_sub_constant =
        add_binary_constant("float_sub", "unit_float_sub", f64, BinaryOp::sub, add_float_literal("7.5"), add_float_literal("2.25"));
    [[maybe_unused]] const GlobalConstantId float_mul_constant =
        add_binary_constant("float_mul", "unit_float_mul", f64, BinaryOp::mul, add_float_literal("1.5"), add_float_literal("4.0"));
    [[maybe_unused]] const GlobalConstantId float_div_constant =
        add_binary_constant("float_div", "unit_float_div", f64, BinaryOp::div, add_float_literal("9.0"), add_float_literal("3.0"));
    [[maybe_unused]] const GlobalConstantId u32_div_constant =
        add_binary_constant("u32_div", "unit_u32_div", u32, BinaryOp::div, add_value(module, integer_value(u32, "21")), add_value(module, integer_value(u32, "3")));
    [[maybe_unused]] const GlobalConstantId u32_mod_constant =
        add_binary_constant("u32_mod", "unit_u32_mod", u32, BinaryOp::mod, add_value(module, integer_value(u32, "22")), add_value(module, integer_value(u32, "5")));
    [[maybe_unused]] const GlobalConstantId u32_shr_constant =
        add_binary_constant("u32_shr", "unit_u32_shr", u32, BinaryOp::shr, add_value(module, integer_value(u32, "32")), add_value(module, integer_value(u32, "2")));
    [[maybe_unused]] const GlobalConstantId u32_less_equal_constant =
        add_binary_constant("u32_less_equal", "unit_u32_less_equal", bool_type, BinaryOp::less_equal, add_value(module, integer_value(u32, "5")), add_value(module, integer_value(u32, "5")));
    [[maybe_unused]] const GlobalConstantId u32_greater_constant =
        add_binary_constant("u32_greater", "unit_u32_greater", bool_type, BinaryOp::greater, add_value(module, integer_value(u32, "9")), add_value(module, integer_value(u32, "4")));
    [[maybe_unused]] const GlobalConstantId u32_greater_equal_constant =
        add_binary_constant("u32_greater_equal", "unit_u32_greater_equal", bool_type, BinaryOp::greater_equal, add_value(module, integer_value(u32, "9")), add_value(module, integer_value(u32, "9")));
    [[maybe_unused]] const GlobalConstantId u32_bit_or_constant =
        add_binary_constant("u32_bit_or", "unit_u32_bit_or", u32, BinaryOp::bit_or, add_value(module, integer_value(u32, "10")), add_value(module, integer_value(u32, "12")));
    [[maybe_unused]] const GlobalConstantId float_neg_constant =
        add_unary_constant("float_neg", "unit_float_neg", f64, UnaryOp::numeric_negate, add_float_literal("8.0"));
    [[maybe_unused]] const GlobalConstantId float_to_bool_constant =
        add_cast_constant("float_to_bool", "unit_float_to_bool", bool_type, float_one);
    [[maybe_unused]] const GlobalConstantId pointer_cast_constant =
        add_cast_constant("pointer_cast", "unit_pointer_cast", ptr_i64, null_pointer_id, CastKind::pointer);

    auto llvm_ir = backend::emit_llvm_ir({&module, "unit_backend_edges"});
    ASSERT_TRUE(llvm_ir) << llvm_ir.error().message;
    expect_contains_all(llvm_ir.value().text, {
        "@unit_float_less",
        "@unit_int_less",
        "@unit_float_less_equal",
        "@unit_int_less_equal",
        "@unit_float_greater_equal",
        "@unit_int_greater_equal",
        "@unit_float_greater",
        "@unit_int_greater",
        "@unit_float_equal",
        "@unit_int_equal",
        "@unit_float_not_equal",
        "@unit_int_not_equal",
        "@unit_u32_xor",
        "@unit_float_add",
        "@unit_float_sub",
        "@unit_float_mul",
        "@unit_float_div",
        "@unit_u32_div",
        "@unit_u32_mod",
        "@unit_u32_shr",
        "@unit_u32_less_equal",
        "@unit_u32_greater",
        "@unit_u32_greater_equal",
        "@unit_u32_bit_or",
        "@unit_float_neg",
        "@unit_float_to_bool",
        "@unit_pointer_cast",
    });
}

} // namespace aurex::test
