#include "aurex/ir/lower_ast.hpp"

#include <string>
#include <vector>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
#define private public
#include "../../../src/ir/lower_ast_internal.hpp"
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
using ir::Terminator;
using ir::TerminatorKind;
using ir::Value;
using ir::ValueId;
using ir::ValueKind;
using ir::add_block;
using ir::detail::Lowerer;
using ir::invalid_block_id;
using ir::invalid_function_id;
using ir::invalid_value_id;
using ir::is_valid;
using sema::BuiltinType;
using sema::CheckedModule;
using sema::EnumCaseInfo;
using sema::PointerMutability;
using sema::TypeHandle;
using sema::invalid_type_handle;
using syntax::ExprId;
using syntax::ExprKind;
using syntax::ItemId;
using syntax::StmtId;
using syntax::TypeId;

[[nodiscard]] ExprId push_name(syntax::AstModule& ast, const std::string_view text) {
    syntax::ExprNode expr;
    expr.kind = ExprKind::name;
    expr.text = text;
    return ast.push_expr(expr);
}

[[nodiscard]] ExprId push_integer(syntax::AstModule& ast, const std::string_view text = "1") {
    syntax::ExprNode expr;
    expr.kind = ExprKind::integer_literal;
    expr.text = text;
    return ast.push_expr(expr);
}

[[nodiscard]] ExprId push_invalid_expr(syntax::AstModule& ast) {
    syntax::ExprNode expr;
    expr.kind = ExprKind::invalid;
    return ast.push_expr(expr);
}

void set_expr_type(CheckedModule& checked, const ExprId expr, const TypeHandle type) {
    if (checked.expr_types.size() <= expr.value) {
        checked.expr_types.resize(expr.value + 1, invalid_type_handle);
    }
    if (checked.expr_c_names.size() <= expr.value) {
        checked.expr_c_names.resize(expr.value + 1);
    }
    checked.expr_types[expr.value] = type;
}

[[nodiscard]] Value typed_value(const ValueKind kind, const TypeHandle type, std::string text = {}) {
    Value value;
    value.kind = kind;
    value.type = type;
    value.text = std::move(text);
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
    none_case.name = "None";
    none_case.c_name = "OptionI32_None";
    none_case.type = enum_type;
    none_case.value_text = "0";
    none_case.enum_name = "OptionI32";
    none_case.case_name = "None";
    checked.enum_cases.emplace(none_case.c_name, none_case);

    const ExprId missing_name = push_name(ast, "missing");
    const ExprId none_name = push_name(ast, "None");
    const ExprId integer = push_integer(ast);
    const ExprId invalid_expr = push_invalid_expr(ast);

    set_expr_type(checked, missing_name, invalid_type_handle);
    set_expr_type(checked, none_name, enum_type);
    set_expr_type(checked, integer, i32);
    set_expr_type(checked, invalid_expr, invalid_type_handle);

    Lowerer lowerer(ast, checked);

    EXPECT_FALSE(sema::is_valid(lowerer.expr_type(syntax::invalid_expr_id)));
    EXPECT_FALSE(sema::is_valid(lowerer.expr_type(ExprId {999})));
    EXPECT_FALSE(sema::is_valid(lowerer.syntax_type(syntax::invalid_type_id)));
    EXPECT_FALSE(sema::is_valid(lowerer.syntax_type(TypeId {999})));
    EXPECT_FALSE(sema::is_valid(lowerer.stmt_local_type(syntax::invalid_stmt_id)));
    EXPECT_FALSE(sema::is_valid(lowerer.stmt_local_type(StmtId {999})));

    EXPECT_EQ(lowerer.lower_expr(syntax::invalid_expr_id).value, invalid_value_id.value);
    EXPECT_EQ(lowerer.lower_expr(invalid_expr).value, invalid_value_id.value);
    EXPECT_EQ(lowerer.lower_if_expr(integer, ast.exprs[integer.value]).value, invalid_value_id.value);

    const ValueId unknown_value = lowerer.lower_name(missing_name, ast.exprs[missing_name.value]);
    ASSERT_TRUE(is_valid(unknown_value));
    ASSERT_LT(unknown_value.value, lowerer.module_.values.size());
    EXPECT_EQ(lowerer.module_.values[unknown_value.value].kind, ValueKind::load);
    EXPECT_EQ(lowerer.module_.values[unknown_value.value].name, "missing");
    EXPECT_FALSE(sema::is_valid(lowerer.module_.values[unknown_value.value].type));

    const ValueId enum_value = lowerer.lower_name(none_name, ast.exprs[none_name.value]);
    ASSERT_TRUE(is_valid(enum_value));
    ASSERT_LT(enum_value.value, lowerer.module_.values.size());
    EXPECT_EQ(lowerer.module_.values[enum_value.value].kind, ValueKind::aggregate);
    EXPECT_TRUE(lowerer.module_.types.same(lowerer.module_.values[enum_value.value].type, enum_type));

    EXPECT_EQ(lowerer.call_symbol(missing_name), "missing");
    EXPECT_EQ(lowerer.call_symbol(syntax::invalid_expr_id), "<invalid>");
    const ir::detail::CallTarget missing_target = lowerer.call_target(missing_name);
    EXPECT_FALSE(is_valid(missing_target.function));
    EXPECT_EQ(missing_target.symbol, "missing");
    EXPECT_EQ(lowerer.value_symbol(missing_name, ast.exprs[missing_name.value]), "missing");

    EXPECT_FALSE(sema::is_valid(lowerer.call_param_type(invalid_function_id, 0)));
    EXPECT_FALSE(sema::is_valid(lowerer.call_param_type(FunctionId {999}, 0)));
    EXPECT_FALSE(sema::is_valid(lowerer.variadic_argument_type(invalid_type_handle)));

    EXPECT_FALSE(sema::is_valid(lowerer.local_load_type(invalid_value_id)));
    const ValueId str_value = lowerer.append_value(typed_value(ValueKind::string_literal, str, "text"));
    ASSERT_TRUE(is_valid(str_value));
    EXPECT_FALSE(sema::is_valid(lowerer.local_load_type(str_value)));

    EXPECT_EQ(lowerer.coerce_value(invalid_value_id, i32).value, invalid_value_id.value);
    Value null_value;
    null_value.kind = ValueKind::null_literal;
    null_value.type = invalid_type_handle;
    const ValueId null_id = lowerer.append_value(null_value);
    EXPECT_EQ(lowerer.coerce_value(null_id, ptr_i32).value, null_id.value);
    ASSERT_LT(null_id.value, lowerer.module_.values.size());
    EXPECT_TRUE(lowerer.module_.types.same(lowerer.module_.values[null_id.value].type, ptr_i32));

    const ValueId const_pointer = lowerer.append_value(typed_value(ValueKind::undef, const_ptr_i32));
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
    syntax::ExprNode logical_expr;
    logical_expr.kind = ExprKind::binary;
    logical_expr.binary_op = syntax::BinaryOp::logical_and;
    logical_expr.binary_lhs = condition_lhs;
    logical_expr.binary_rhs = condition_rhs;
    const ExprId logical = ast.push_expr(logical_expr);

    set_expr_type(checked, missing_place, i32);
    set_expr_type(checked, integer, i32);
    set_expr_type(checked, condition_lhs, bool_type);
    set_expr_type(checked, condition_rhs, bool_type);
    set_expr_type(checked, logical, bool_type);

    Lowerer lowerer(ast, checked);

    EXPECT_EQ(lowerer.lower_place_address(syntax::invalid_expr_id).address.value, invalid_value_id.value);
    EXPECT_EQ(lowerer.lower_place_address(ExprId {999}).address.value, invalid_value_id.value);
    EXPECT_EQ(lowerer.lower_place_address(missing_place).address.value, invalid_value_id.value);
    EXPECT_EQ(lowerer.lower_place_address(integer).address.value, invalid_value_id.value);

    Value slot_value;
    slot_value.kind = ValueKind::alloca;
    slot_value.type = ptr_i32;
    const ValueId slot_id = lowerer.append_value(slot_value);
    lowerer.locals_.emplace("slot", ir::detail::LocalBinding {slot_id, true});
    const ir::detail::PlaceAddress slot_address = lowerer.lower_place_address(missing_place);
    EXPECT_EQ(slot_address.address.value, slot_id.value);
    EXPECT_TRUE(slot_address.is_mutable);

    Function function;
    function.name = "terminators";
    function.symbol = "terminators";
    function.return_type = i32;
    const BlockId entry = add_block(function, "entry");
    const BlockId exit = add_block(function, "exit");
    lowerer.module_.functions.push_back(std::move(function));
    lowerer.current_function_ = &lowerer.module_.functions.back();
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
    const ValueId short_circuit = lowerer.lower_short_circuit_expr(logical, ast.exprs[logical.value]);
    ASSERT_TRUE(is_valid(short_circuit));
    ASSERT_LT(short_circuit.value, lowerer.module_.values.size());
    const Value& phi = lowerer.module_.values[short_circuit.value];
    EXPECT_EQ(phi.kind, ValueKind::phi);
    EXPECT_EQ(phi.incoming.size(), 2U);
    EXPECT_EQ(lowerer.current_function_->blocks[entry.value].terminator.kind, TerminatorKind::cond_branch);
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

    checked.syntax_type_handles.resize(return_type.value + 1, invalid_type_handle);
    checked.syntax_type_handles[return_type.value] = checked.types.builtin(BuiltinType::i32);

    sema::GenericFunctionInstanceInfo invalid_instance;
    invalid_instance.item = ItemId {999};
    checked.generic_function_instances.push_back(invalid_instance);

    Lowerer lowerer(ast, checked);
    lowerer.lower_function_declarations();
    lowerer.lower_generic_function_declarations();

    ASSERT_EQ(lowerer.module_.functions.size(), 1U);
    EXPECT_EQ(lowerer.module_.functions[0].name, "exported");
    EXPECT_EQ(lowerer.module_.functions[0].symbol, "exported");
    EXPECT_EQ(lowerer.module_.functions[0].linkage, Linkage::export_c);
    EXPECT_EQ(lowerer.module_.functions[0].call_conv, AbiCallConv::c);
    EXPECT_TRUE(lowerer.module_.types.same(lowerer.module_.functions[0].return_type, checked.syntax_type_handles[return_type.value]));

    syntax::ItemNode abi_named;
    abi_named.name = "plain";
    abi_named.abi_name = "plain_abi";
    EXPECT_EQ(lowerer.item_symbol(99, abi_named), "plain_abi");

    EXPECT_FALSE(sema::is_valid(lowerer.enum_case_type("missing")));
}

} // namespace aurex::test
