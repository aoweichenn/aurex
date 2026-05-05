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
        const TypeHandle i32 = builtin(module, BuiltinType::i32);
        const TypeHandle u32 = builtin(module, BuiltinType::u32);
        const TypeHandle i64 = builtin(module, BuiltinType::i64);
        const TypeHandle f64 = builtin(module, BuiltinType::f64);
        const TypeHandle str_type = builtin(module, BuiltinType::str);
        const TypeHandle ptr_i32 = ptr(module, PointerMutability::mut, i32);
        const TypeHandle array_i32 = module.types.array(2, i32);
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
        Value cast;
        cast.kind = ValueKind::cast;
        cast.type = i64;
        cast.target_type = i64;
        cast.cast_kind = CastKind::numeric;
        cast.lhs = add_value(module, integer_value(i32, "9"));
        const ValueId cast_id = add_value(module, cast);
        [[maybe_unused]] const GlobalConstantId wide_constant =
            add_global_constant(module, GlobalConstant {"wide", "unit_wide", i64, cast_id});
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
        sizeof_value.target_type = array_i32;
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
                 BinaryOp::shl,
                 BinaryOp::shr,
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
        for (const BinaryOp op : {BinaryOp::mod, BinaryOp::shr}) {
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
            "@unit_pair",
            "@unit_size",
            "@unit_align",
            "%unit_Pair = type",
            "xor",
            "shl",
            "lshr",
            "urem",
            "getelementptr",
            "str.data",
        });
    }
}

} // namespace aurex::test
