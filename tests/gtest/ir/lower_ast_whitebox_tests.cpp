#include <aurex/ir/lower_ast.hpp>

#include <string>
#include <utility>
#include <vector>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
#define private public
#include <ir/lower_ast_internal.hpp>
#undef private
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

using ir::AbiCallConv;
using ir::BinaryOp;
using ir::BlockId;
using ir::Function;
using ir::FunctionId;
using ir::Linkage;
using ir::add_function;
using ir::Terminator;
using ir::TerminatorKind;
using ir::Value;
using ir::ValueId;
using ir::ValueKind;
using ir::add_block;
using ir::detail::Lowerer;
using ir::INVALID_BLOCK_ID;
using ir::INVALID_FUNCTION_ID;
using ir::INVALID_VALUE_ID;
using ir::is_valid;
using sema::BuiltinType;
using sema::CheckedModule;
using sema::EnumCaseInfo;
using sema::PointerMutability;
using sema::TypeHandle;
using sema::INVALID_TYPE_HANDLE;
using syntax::ExprId;
using syntax::ExprKind;
using syntax::StmtId;
using syntax::TypeId;

[[nodiscard]] ExprId push_name(syntax::AstModule& ast, const std::string_view text) {
    syntax::NameExprPayload payload;
    payload.text = text;
    payload.text_id = ast.intern_identifier(text);
    return ast.push_name_expr({}, std::move(payload));
}

[[nodiscard]] ExprId push_integer(syntax::AstModule& ast, const std::string_view text = "1") {
    return ast.push_literal_expr(ExprKind::integer_literal, {}, text);
}

[[nodiscard]] ExprId push_invalid_expr(syntax::AstModule& ast) {
    return ast.push_invalid_expr({});
}

void set_expr_type(CheckedModule& checked, const ExprId expr, const TypeHandle type) {
    if (checked.expr_types.size() <= expr.value) {
        checked.expr_types.resize(expr.value + 1, INVALID_TYPE_HANDLE);
    }
    if (checked.expr_c_name_ids.size() <= expr.value) {
        checked.expr_c_name_ids.resize(expr.value + 1, sema::INVALID_IDENT_ID);
    }
    checked.expr_types[expr.value] = type;
}

[[nodiscard]] Value typed_value(
    ir::Module& module,
    const ValueKind kind,
    const TypeHandle type,
    const std::string_view text = {}
) {
    Value value = module.make_value();
    value.kind = kind;
    value.type = type;
    if (!text.empty()) {
        value.text = module.intern(text);
    }
    return value;
}

} // namespace

TEST(CoreUnit, LowerAstWhiteBoxExpressionFallbacksAndCoercions) {
    syntax::AstModule ast;
    CheckedModule checked;

    const TypeHandle i32 = checked.types.builtin(BuiltinType::i32);
    const TypeHandle i64 = checked.types.builtin(BuiltinType::i64);
    const TypeHandle str = checked.types.builtin(BuiltinType::str);
    const TypeHandle ptr_i32 = checked.types.pointer(PointerMutability::mut, i32);
    const TypeHandle const_ptr_i32 = checked.types.pointer(PointerMutability::const_, i32);
    const TypeHandle enum_type = checked.types.named_enum("OptionI32", "OptionI32");
    checked.types.set_enum_underlying(enum_type, i32);
    checked.types.set_enum_payload_layout(enum_type, i64, 8, 8);

    EnumCaseInfo none_case;
    none_case.name = checked.intern_text("None");
    none_case.name_id = ast.intern_identifier("None");
    none_case.c_name = checked.intern_text("OptionI32_None");
    none_case.type = enum_type;
    none_case.value_text = checked.intern_text("0");
    none_case.enum_name = checked.intern_text("OptionI32");
    none_case.case_name = checked.intern_text("None");
    none_case.case_name_id = none_case.name_id;
    checked.enum_cases.emplace(sema::ModuleLookupKey {0, none_case.name_id}, none_case);

    const ExprId missing_name = push_name(ast, "missing");
    const ExprId none_name = push_name(ast, "None");
    const ExprId integer = push_integer(ast);
    const ExprId invalid_expr = push_invalid_expr(ast);

    set_expr_type(checked, missing_name, INVALID_TYPE_HANDLE);
    set_expr_type(checked, none_name, enum_type);
    checked.expr_c_name_ids[none_name.value] = checked.intern_c_name(none_case.c_name);
    set_expr_type(checked, integer, i32);
    set_expr_type(checked, invalid_expr, INVALID_TYPE_HANDLE);

    Lowerer lowerer(ast, checked);

    EXPECT_FALSE(sema::is_valid(lowerer.expr_type(syntax::INVALID_EXPR_ID)));
    EXPECT_FALSE(sema::is_valid(lowerer.expr_type(ExprId {999})));
    EXPECT_FALSE(sema::is_valid(lowerer.syntax_type(syntax::INVALID_TYPE_ID)));
    EXPECT_FALSE(sema::is_valid(lowerer.syntax_type(TypeId {999})));
    EXPECT_FALSE(sema::is_valid(lowerer.stmt_local_type(syntax::INVALID_STMT_ID)));
    EXPECT_FALSE(sema::is_valid(lowerer.stmt_local_type(StmtId {999})));

    EXPECT_EQ(lowerer.lower_expr(syntax::INVALID_EXPR_ID).value, INVALID_VALUE_ID.value);
    EXPECT_EQ(lowerer.lower_expr(invalid_expr).value, INVALID_VALUE_ID.value);
    EXPECT_EQ(lowerer.lower_if_expr(integer, lowerer.expr_view(integer)).value, INVALID_VALUE_ID.value);

    const ValueId unknown_value = lowerer.lower_name(missing_name, lowerer.expr_view(missing_name));
    ASSERT_TRUE(is_valid(unknown_value));
    ASSERT_LT(unknown_value.value, lowerer.module_.values.size());
    EXPECT_EQ(lowerer.module_.values[unknown_value.value].kind, ValueKind::load);
    EXPECT_EQ(lowerer.module_.text(lowerer.module_.values[unknown_value.value].name), "missing");
    EXPECT_FALSE(sema::is_valid(lowerer.module_.values[unknown_value.value].type));

    const ValueId enum_value = lowerer.lower_name(none_name, lowerer.expr_view(none_name));
    ASSERT_TRUE(is_valid(enum_value));
    ASSERT_LT(enum_value.value, lowerer.module_.values.size());
    EXPECT_EQ(lowerer.module_.values[enum_value.value].kind, ValueKind::aggregate);
    EXPECT_TRUE(lowerer.module_.types.same(lowerer.module_.values[enum_value.value].type, enum_type));

    EXPECT_EQ(lowerer.module_.text(lowerer.call_symbol(missing_name)), "missing");
    EXPECT_EQ(lowerer.module_.text(lowerer.call_symbol(syntax::INVALID_EXPR_ID)), "<invalid>");
    const ir::detail::CallTarget missing_target = lowerer.call_target(missing_name);
    EXPECT_FALSE(is_valid(missing_target.function));
    EXPECT_EQ(lowerer.module_.text(missing_target.symbol), "missing");
    EXPECT_EQ(lowerer.module_.text(lowerer.value_symbol(missing_name, lowerer.expr_view(missing_name))), "missing");

    EXPECT_FALSE(sema::is_valid(lowerer.call_param_type(INVALID_FUNCTION_ID, 0)));
    EXPECT_FALSE(sema::is_valid(lowerer.call_param_type(FunctionId {999}, 0)));
    EXPECT_FALSE(sema::is_valid(lowerer.variadic_argument_type(INVALID_TYPE_HANDLE)));

    EXPECT_FALSE(sema::is_valid(lowerer.local_load_type(INVALID_VALUE_ID)));
    const ValueId str_value = lowerer.append_value(typed_value(lowerer.module_, ValueKind::string_literal, str, "text"));
    ASSERT_TRUE(is_valid(str_value));
    EXPECT_FALSE(sema::is_valid(lowerer.local_load_type(str_value)));

    EXPECT_EQ(lowerer.coerce_value(INVALID_VALUE_ID, i32).value, INVALID_VALUE_ID.value);
    Value null_value = lowerer.module_.make_value();
    null_value.kind = ValueKind::null_literal;
    null_value.type = INVALID_TYPE_HANDLE;
    const ValueId null_id = lowerer.append_value(null_value);
    EXPECT_EQ(lowerer.coerce_value(null_id, ptr_i32).value, null_id.value);
    ASSERT_LT(null_id.value, lowerer.module_.values.size());
    EXPECT_TRUE(lowerer.module_.types.same(lowerer.module_.values[null_id.value].type, ptr_i32));

    const ValueId const_pointer = lowerer.append_value(typed_value(lowerer.module_, ValueKind::undef, const_ptr_i32));
    EXPECT_EQ(lowerer.coerce_value(const_pointer, i32).value, const_pointer.value);
}

TEST(CoreUnit, LowerAstWhiteBoxPlacesCallsAndTerminators) {
    syntax::AstModule ast;
    CheckedModule checked;

    const TypeHandle i32 = checked.types.builtin(BuiltinType::i32);
    const TypeHandle bool_type = checked.types.builtin(BuiltinType::bool_);
    const TypeHandle ptr_i32 = checked.types.pointer(PointerMutability::mut, i32);

    const ExprId missing_place = push_name(ast, "slot");
    const ExprId integer = push_integer(ast);
    const ExprId condition_lhs = push_integer(ast, "0");
    const ExprId condition_rhs = push_integer(ast, "1");
    const ExprId logical = ast.push_binary_expr(
        {},
        syntax::BinaryExprPayload {
            syntax::BinaryOp::logical_and,
            condition_lhs,
            condition_rhs,
        }
    );

    set_expr_type(checked, missing_place, i32);
    set_expr_type(checked, integer, i32);
    set_expr_type(checked, condition_lhs, bool_type);
    set_expr_type(checked, condition_rhs, bool_type);
    set_expr_type(checked, logical, bool_type);

    Lowerer lowerer(ast, checked);

    EXPECT_EQ(lowerer.lower_place_address(syntax::INVALID_EXPR_ID).address.value, INVALID_VALUE_ID.value);
    EXPECT_EQ(lowerer.lower_place_address(ExprId {999}).address.value, INVALID_VALUE_ID.value);
    EXPECT_EQ(lowerer.lower_place_address(missing_place).address.value, INVALID_VALUE_ID.value);
    EXPECT_EQ(lowerer.lower_place_address(integer).address.value, INVALID_VALUE_ID.value);

    Value slot_value = lowerer.module_.make_value();
    slot_value.kind = ValueKind::alloca;
    slot_value.type = ptr_i32;
    const ValueId slot_id = lowerer.append_value(slot_value);
    lowerer.locals_.emplace(ast.find_identifier("slot"), ir::detail::LocalBinding {slot_id, true});
    const ir::detail::PlaceAddress slot_address = lowerer.lower_place_address(missing_place);
    EXPECT_EQ(slot_address.address.value, slot_id.value);
    EXPECT_TRUE(slot_address.is_mutable);

    Function function = lowerer.module_.make_function();
    function.name = lowerer.module_.intern("terminators");
    function.symbol = lowerer.module_.intern("terminators");
    function.return_type = i32;
    const BlockId entry = add_block(lowerer.module_, function, "entry");
    const BlockId exit = add_block(lowerer.module_, function, "exit");
    const FunctionId function_id = add_function(lowerer.module_, function);
    lowerer.current_function_ = &lowerer.module_.functions[function_id.value];
    lowerer.current_block_ = entry;

    Terminator ret;
    ret.kind = TerminatorKind::return_;
    lowerer.set_terminator(BlockId {999}, ret);
    EXPECT_TRUE(lowerer.has_terminator(BlockId {999}));

    lowerer.set_terminator(entry, ret);
    lowerer.append_branch_if_open(exit);
    EXPECT_EQ(lowerer.current_function_->blocks[entry.value].terminator.kind, TerminatorKind::return_);

    lowerer.current_function_->blocks[entry.value].terminator = {};
    lowerer.append_branch_if_open(exit);
    EXPECT_EQ(lowerer.current_function_->blocks[entry.value].terminator.kind, TerminatorKind::branch);
    EXPECT_EQ(lowerer.current_function_->blocks[entry.value].terminator.target.value, exit.value);

    lowerer.current_function_->blocks[entry.value].terminator = {};
    lowerer.current_block_ = entry;
    const ValueId short_circuit = lowerer.lower_short_circuit_expr(logical, lowerer.expr_view(logical));
    ASSERT_TRUE(is_valid(short_circuit));
    ASSERT_LT(short_circuit.value, lowerer.module_.values.size());
    const Value& phi = lowerer.module_.values[short_circuit.value];
    EXPECT_EQ(phi.kind, ValueKind::phi);
    EXPECT_EQ(phi.incoming.size(), 2U);
    EXPECT_EQ(lowerer.current_function_->blocks[entry.value].terminator.kind, TerminatorKind::cond_branch);
}

TEST(CoreUnit, LowerAstWhiteBoxStringBuiltins) {
    syntax::AstModule ast;
    CheckedModule checked;

    const TypeHandle u8 = checked.types.builtin(BuiltinType::u8);
    const TypeHandle usize = checked.types.builtin(BuiltinType::usize);
    const TypeHandle str = checked.types.builtin(BuiltinType::str);
    const TypeHandle char_type = checked.types.builtin(BuiltinType::char_);
    const TypeHandle byte_array = checked.types.array(3, u8);
    const TypeHandle const_u8_ptr = checked.types.pointer(PointerMutability::const_, u8);

    const ExprId str_value_id = ast.push_literal_expr(ExprKind::string_literal, {}, "\"bytes\"");
    const ExprId length_value_id = ast.push_literal_expr(ExprKind::integer_literal, {}, "5");
    const ExprId str_data_id = ast.push_cast_like_expr(
        ExprKind::str_data,
        {},
        syntax::CastExprPayload {syntax::INVALID_TYPE_ID, str_value_id}
    );
    const ExprId str_byte_len_id = ast.push_cast_like_expr(
        ExprKind::str_byte_len,
        {},
        syntax::CastExprPayload {syntax::INVALID_TYPE_ID, str_value_id}
    );
    const ExprId str_from_bytes_id = ast.push_call_expr(
        ExprKind::str_from_bytes_unchecked,
        {},
        syntax::CallExprPayload {syntax::INVALID_EXPR_ID, {str_data_id, length_value_id}}
    );
    const ExprId str_slice_id = ast.push_slice_expr(
        {},
        syntax::SliceExprPayload {str_value_id, length_value_id, length_value_id}
    );
    const ExprId str_suffix_id = ast.push_slice_expr(
        {},
        syntax::SliceExprPayload {str_value_id, length_value_id, syntax::INVALID_EXPR_ID}
    );
    const ExprId malformed_str_from_bytes_id = ast.push_call_expr(
        ExprKind::str_from_bytes_unchecked,
        {},
        syntax::CallExprPayload {syntax::INVALID_EXPR_ID, {str_data_id}}
    );
    const ExprId raw_literal_id = ast.push_literal_expr(ExprKind::raw_string_literal, {}, "r\"C:\\tmp\\a\"");
    const ExprId byte_string_literal_id =
        ast.push_literal_expr(ExprKind::byte_string_literal, {}, "b\"a\\n\\0\"");
    const ExprId char_literal_id = ast.push_literal_expr(ExprKind::char_literal, {}, "'\\u{03BB}'");

    set_expr_type(checked, str_value_id, str);
    set_expr_type(checked, length_value_id, usize);
    set_expr_type(checked, str_data_id, const_u8_ptr);
    set_expr_type(checked, str_byte_len_id, usize);
    set_expr_type(checked, str_from_bytes_id, str);
    set_expr_type(checked, str_slice_id, str);
    set_expr_type(checked, str_suffix_id, str);
    set_expr_type(checked, malformed_str_from_bytes_id, str);
    set_expr_type(checked, raw_literal_id, str);
    set_expr_type(checked, byte_string_literal_id, byte_array);
    set_expr_type(checked, char_literal_id, char_type);

    Lowerer lowerer(ast, checked);
    const ValueId data = lowerer.lower_expr(str_data_id);
    const ValueId byte_len = lowerer.lower_expr(str_byte_len_id);
    const ValueId from_bytes = lowerer.lower_expr(str_from_bytes_id);
    const ValueId slice = lowerer.lower_expr(str_slice_id);
    const ValueId suffix = lowerer.lower_expr(str_suffix_id);
    const ValueId malformed = lowerer.lower_expr(malformed_str_from_bytes_id);
    const ValueId raw = lowerer.lower_expr(raw_literal_id);
    const ValueId byte_string = lowerer.lower_expr(byte_string_literal_id);
    const ValueId char_value = lowerer.lower_expr(char_literal_id);

    ASSERT_TRUE(is_valid(data));
    ASSERT_TRUE(is_valid(byte_len));
    ASSERT_TRUE(is_valid(from_bytes));
    ASSERT_TRUE(is_valid(slice));
    ASSERT_TRUE(is_valid(suffix));
    ASSERT_TRUE(is_valid(malformed));
    ASSERT_TRUE(is_valid(raw));
    ASSERT_TRUE(is_valid(byte_string));
    ASSERT_TRUE(is_valid(char_value));
    EXPECT_EQ(lowerer.module_.values[data.value].kind, ValueKind::str_data);
    EXPECT_EQ(lowerer.module_.values[byte_len.value].kind, ValueKind::str_byte_len);
    EXPECT_EQ(lowerer.module_.values[from_bytes.value].kind, ValueKind::str_from_bytes_unchecked);
    EXPECT_EQ(lowerer.module_.values[from_bytes.value].args.size(), 2U);
    EXPECT_EQ(lowerer.module_.values[slice.value].kind, ValueKind::str_slice_checked);
    EXPECT_EQ(lowerer.module_.values[suffix.value].kind, ValueKind::str_slice_checked);
    ASSERT_TRUE(is_valid(lowerer.module_.values[suffix.value].rhs));
    EXPECT_EQ(lowerer.module_.values[lowerer.module_.values[suffix.value].rhs.value].kind, ValueKind::str_byte_len);
    EXPECT_EQ(lowerer.module_.values[malformed.value].kind, ValueKind::str_from_bytes_unchecked);
    EXPECT_TRUE(lowerer.module_.values[malformed.value].args.empty());
    EXPECT_EQ(lowerer.module_.values[raw.value].kind, ValueKind::raw_string_literal);
    EXPECT_EQ(lowerer.module_.values[byte_string.value].kind, ValueKind::aggregate);
    EXPECT_EQ(lowerer.module_.values[byte_string.value].elements.size(), 3U);
    EXPECT_EQ(lowerer.module_.values[char_value.value].kind, ValueKind::char_literal);
}

TEST(CoreUnit, LowerAstWhiteBoxDeclarationFallbacks) {
    syntax::AstModule ast;
    CheckedModule checked;

    syntax::TypeNode return_type_node;
    return_type_node.kind = syntax::TypeKind::primitive;
    return_type_node.primitive = syntax::PrimitiveTypeKind::i32;
    const TypeId return_type = ast.push_type(return_type_node);

    syntax::ItemNode exported;
    exported.kind = syntax::ItemKind::fn_decl;
    exported.name = "exported";
    exported.return_type = return_type;
    exported.is_export_c = true;
    static_cast<void>(ast.push_item(exported));
    ast.item_modules[0] = syntax::ModuleId {0};

    checked.syntax_type_handles.resize(return_type.value + 1, INVALID_TYPE_HANDLE);
    checked.syntax_type_handles[return_type.value] = checked.types.builtin(BuiltinType::i32);

    Lowerer lowerer(ast, checked);
    lowerer.lower_function_declarations();

    ASSERT_EQ(lowerer.module_.functions.size(), 1U);
    EXPECT_EQ(lowerer.module_.text(lowerer.module_.functions[0].name), "exported");
    EXPECT_EQ(lowerer.module_.text(lowerer.module_.functions[0].symbol), "exported");
    EXPECT_EQ(lowerer.module_.functions[0].linkage, Linkage::export_c);
    EXPECT_EQ(lowerer.module_.functions[0].call_conv, AbiCallConv::c);
    EXPECT_TRUE(lowerer.module_.types.same(lowerer.module_.functions[0].return_type, checked.syntax_type_handles[return_type.value]));

    syntax::ItemNode abi_named;
    abi_named.name = "plain";
    abi_named.abi_name = "plain_abi";
    EXPECT_EQ(lowerer.module_.text(lowerer.item_symbol(99, abi_named)), "plain_abi");

    EXPECT_FALSE(sema::is_valid(lowerer.enum_case_type(lowerer.module_.intern("missing"))));
}

} // namespace aurex::test
