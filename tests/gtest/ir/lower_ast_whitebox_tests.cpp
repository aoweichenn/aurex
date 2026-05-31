#include <aurex/ir/ir_dump.hpp>
#include <aurex/ir/lower_ast.hpp>
#include <aurex/ir/verify.hpp>
#include <aurex/query/query_key.hpp>

#include <array>
#include <span>
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
using ir::add_block;
using ir::add_function;
using ir::BinaryOp;
using ir::BlockId;
using ir::Function;
using ir::FunctionId;
using ir::INVALID_BLOCK_ID;
using ir::INVALID_FUNCTION_ID;
using ir::INVALID_VALUE_ID;
using ir::is_valid;
using ir::Linkage;
using ir::Terminator;
using ir::TerminatorKind;
using ir::Value;
using ir::ValueId;
using ir::ValueKind;
using ir::detail::Lowerer;
using sema::BuiltinType;
using sema::CheckedModule;
using sema::EnumCaseInfo;
using sema::INVALID_TYPE_HANDLE;
using sema::PointerMutability;
using sema::TypeHandle;
using syntax::ExprId;
using syntax::ExprKind;
using syntax::StmtId;
using syntax::TypeId;

[[nodiscard]] ExprId push_name(syntax::AstModule& ast, const std::string_view text)
{
    syntax::NameExprPayload payload;
    payload.text = text;
    payload.text_id = ast.intern_identifier(text);
    return ast.push_name_expr({}, std::move(payload));
}

[[nodiscard]] ExprId push_integer(syntax::AstModule& ast, const std::string_view text = "1")
{
    return ast.push_literal_expr(ExprKind::integer_literal, {}, text);
}

[[nodiscard]] ExprId push_invalid_expr(syntax::AstModule& ast)
{
    return ast.push_invalid_expr({});
}

[[nodiscard]] query::GenericInstanceKey test_generic_instance_key()
{
    constexpr std::string_view LOWER_AST_GENERIC_TEST_MODULE = "lower_ast_generic";
    constexpr std::string_view LOWER_AST_GENERIC_TEST_FUNCTION = "id";
    const std::array<std::string_view, 1> module_path{LOWER_AST_GENERIC_TEST_MODULE};
    const std::array<std::string_view, 1> def_path{LOWER_AST_GENERIC_TEST_FUNCTION};
    const query::PackageKey package = query::package_key(std::span<const std::string_view>{});
    const query::ModuleKey module = query::module_key(package, module_path);
    const query::DefKey def =
        query::def_key(module, query::DefNamespace::value, query::DefKind::generic_template, def_path);
    const std::array<query::CanonicalTypeKey, 1> args{query::canonical_builtin(query::BuiltinTypeKey::i32)};
    return query::generic_instance_key(def, args, std::span<const query::StableFingerprint128>{},
        query::param_env_key(std::span<const std::string_view>{}));
}

void set_expr_type(CheckedModule& checked, const ExprId expr, const TypeHandle type)
{
    if (checked.expr_types.size() <= expr.value) {
        checked.expr_types.resize(expr.value + 1, INVALID_TYPE_HANDLE);
    }
    if (checked.expr_c_name_ids.size() <= expr.value) {
        checked.expr_c_name_ids.resize(expr.value + 1, sema::INVALID_IDENT_ID);
    }
    checked.expr_types[expr.value] = type;
}

void set_expr_owned_use_mode(CheckedModule& checked, const ExprId expr, const sema::OwnedUseMode mode)
{
    if (checked.expr_owned_use_modes.size() <= expr.value) {
        checked.expr_owned_use_modes.resize(expr.value + 1, sema::OwnedUseMode::none);
    }
    checked.expr_owned_use_modes[expr.value] = mode;
}

void set_stmt_local_type(CheckedModule& checked, const StmtId stmt, const TypeHandle type)
{
    if (checked.stmt_local_types.size() <= stmt.value) {
        checked.stmt_local_types.resize(stmt.value + 1, INVALID_TYPE_HANDLE);
    }
    checked.stmt_local_types[stmt.value] = type;
}

[[nodiscard]] StmtId push_block(syntax::AstModule& ast, std::vector<StmtId> statements)
{
    syntax::StmtNode block;
    block.kind = syntax::StmtKind::block;
    block.statements = std::move(statements);
    return ast.push_stmt(block);
}

[[nodiscard]] StmtId push_defer_stmt(syntax::AstModule& ast, const ExprId expr)
{
    syntax::StmtNode stmt;
    stmt.kind = syntax::StmtKind::defer;
    stmt.init = expr;
    return ast.push_stmt(stmt);
}

[[nodiscard]] StmtId push_var_stmt(
    syntax::AstModule& ast, const std::string_view name, const ExprId init, const bool is_mutable)
{
    syntax::StmtNode stmt;
    stmt.kind = is_mutable ? syntax::StmtKind::var : syntax::StmtKind::let;
    stmt.name = name;
    stmt.name_id = ast.intern_identifier(name);
    stmt.init = init;
    return ast.push_stmt(stmt);
}

[[nodiscard]] StmtId push_assign_stmt(syntax::AstModule& ast, const ExprId lhs, const ExprId rhs)
{
    syntax::StmtNode stmt;
    stmt.kind = syntax::StmtKind::assign;
    stmt.lhs = lhs;
    stmt.rhs = rhs;
    return ast.push_stmt(stmt);
}

[[nodiscard]] base::usize count_occurrences(const std::string& text, const std::string_view needle)
{
    if (needle.empty()) {
        return 0;
    }
    base::usize count = 0;
    std::string::size_type position = text.find(needle);
    while (position != std::string::npos) {
        ++count;
        position = text.find(needle, position + needle.size());
    }
    return count;
}

[[nodiscard]] Value typed_value(
    ir::Module& module, const ValueKind kind, const TypeHandle type, const std::string_view text = {})
{
    Value value = module.make_value();
    value.kind = kind;
    value.type = type;
    if (!text.empty()) {
        value.text = module.intern(text);
    }
    return value;
}

} // namespace

TEST(CoreUnit, LowerAstWhiteBoxExpressionFallbacksAndCoercions)
{
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
    checked.enum_cases.emplace(sema::ModuleLookupKey{0, none_case.name_id}, none_case);

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
    EXPECT_FALSE(sema::is_valid(lowerer.expr_type(ExprId{999})));
    EXPECT_FALSE(sema::is_valid(lowerer.syntax_type(syntax::INVALID_TYPE_ID)));
    EXPECT_FALSE(sema::is_valid(lowerer.syntax_type(TypeId{999})));
    EXPECT_FALSE(sema::is_valid(lowerer.stmt_local_type(syntax::INVALID_STMT_ID)));
    EXPECT_FALSE(sema::is_valid(lowerer.stmt_local_type(StmtId{999})));

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
    EXPECT_FALSE(sema::is_valid(lowerer.call_param_type(FunctionId{999}, 0)));
    EXPECT_FALSE(sema::is_valid(lowerer.variadic_argument_type(INVALID_TYPE_HANDLE)));

    EXPECT_FALSE(sema::is_valid(lowerer.local_load_type(INVALID_VALUE_ID)));
    const ValueId str_value =
        lowerer.append_value(typed_value(lowerer.module_, ValueKind::string_literal, str, "text"));
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

TEST(CoreUnit, LowerAstWhiteBoxPlacesCallsAndTerminators)
{
    syntax::AstModule ast;
    CheckedModule checked;

    const TypeHandle i32 = checked.types.builtin(BuiltinType::i32);
    const TypeHandle bool_type = checked.types.builtin(BuiltinType::bool_);
    const TypeHandle ptr_i32 = checked.types.pointer(PointerMutability::mut, i32);

    const ExprId missing_place = push_name(ast, "slot");
    const ExprId integer = push_integer(ast);
    const ExprId condition_lhs = push_integer(ast, "0");
    const ExprId condition_rhs = push_integer(ast, "1");
    const ExprId logical = ast.push_binary_expr({},
        syntax::BinaryExprPayload{
            syntax::BinaryOp::logical_and,
            condition_lhs,
            condition_rhs,
        });

    set_expr_type(checked, missing_place, i32);
    set_expr_type(checked, integer, i32);
    set_expr_type(checked, condition_lhs, bool_type);
    set_expr_type(checked, condition_rhs, bool_type);
    set_expr_type(checked, logical, bool_type);

    Lowerer lowerer(ast, checked);

    EXPECT_EQ(lowerer.lower_place_address(syntax::INVALID_EXPR_ID).address.value, INVALID_VALUE_ID.value);
    EXPECT_EQ(lowerer.lower_place_address(ExprId{999}).address.value, INVALID_VALUE_ID.value);
    EXPECT_EQ(lowerer.lower_place_address(missing_place).address.value, INVALID_VALUE_ID.value);
    EXPECT_EQ(lowerer.lower_place_address(integer).address.value, INVALID_VALUE_ID.value);

    Value slot_value = lowerer.module_.make_value();
    slot_value.kind = ValueKind::alloca;
    slot_value.type = ptr_i32;
    const ValueId slot_id = lowerer.append_value(slot_value);
    lowerer.locals_.emplace(
        ast.find_identifier("slot"), ir::detail::LocalBinding{slot_id, INVALID_VALUE_ID, i32, true});
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
    lowerer.set_terminator(BlockId{999}, ret);
    EXPECT_TRUE(lowerer.has_terminator(BlockId{999}));

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

TEST(CoreUnit, LowerAstWhiteBoxStringBuiltins)
{
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
    const ExprId str_data_id =
        ast.push_cast_like_expr(ExprKind::str_data, {}, syntax::CastExprPayload{syntax::INVALID_TYPE_ID, str_value_id});
    const ExprId str_byte_len_id = ast.push_cast_like_expr(
        ExprKind::str_byte_len, {}, syntax::CastExprPayload{syntax::INVALID_TYPE_ID, str_value_id});
    const ExprId str_from_bytes_id = ast.push_call_expr(ExprKind::str_from_bytes_unchecked, {},
        syntax::CallExprPayload{syntax::INVALID_EXPR_ID, {str_data_id, length_value_id}});
    const ExprId str_slice_id =
        ast.push_slice_expr({}, syntax::SliceExprPayload{str_value_id, length_value_id, length_value_id});
    const ExprId str_suffix_id =
        ast.push_slice_expr({}, syntax::SliceExprPayload{str_value_id, length_value_id, syntax::INVALID_EXPR_ID});
    const ExprId malformed_str_from_bytes_id = ast.push_call_expr(
        ExprKind::str_from_bytes_unchecked, {}, syntax::CallExprPayload{syntax::INVALID_EXPR_ID, {str_data_id}});
    const ExprId raw_literal_id = ast.push_literal_expr(ExprKind::raw_string_literal, {}, "r\"C:\\tmp\\a\"");
    const ExprId byte_string_literal_id = ast.push_literal_expr(ExprKind::byte_string_literal, {}, "b\"a\\n\\0\"");
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

TEST(CoreUnit, LowerAstWhiteBoxDeclarationFallbacks)
{
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
    ast.item_modules[0] = syntax::ModuleId{0};

    checked.syntax_type_handles.resize(return_type.value + 1, INVALID_TYPE_HANDLE);
    checked.syntax_type_handles[return_type.value] = checked.types.builtin(BuiltinType::i32);

    Lowerer lowerer(ast, checked);
    lowerer.lower_function_declarations();

    ASSERT_EQ(lowerer.module_.functions.size(), 1U);
    EXPECT_EQ(lowerer.module_.text(lowerer.module_.functions[0].name), "exported");
    EXPECT_EQ(lowerer.module_.text(lowerer.module_.functions[0].symbol), "exported");
    EXPECT_EQ(lowerer.module_.functions[0].linkage, Linkage::export_c);
    EXPECT_EQ(lowerer.module_.functions[0].call_conv, AbiCallConv::c);
    EXPECT_TRUE(lowerer.module_.types.same(
        lowerer.module_.functions[0].return_type, checked.syntax_type_handles[return_type.value]));

    syntax::ItemNode abi_named;
    abi_named.name = "plain";
    abi_named.abi_name = "plain_abi";
    EXPECT_EQ(lowerer.module_.text(lowerer.item_symbol(99, abi_named)), "plain_abi");

    EXPECT_FALSE(sema::is_valid(lowerer.enum_case_type(lowerer.module_.intern("missing"))));
}

TEST(CoreUnit, LowerAstWhiteBoxGenericBodiesUseRetainedSemaView)
{
    syntax::AstModule ast;
    CheckedModule checked;

    syntax::TypeNode generic_type_node;
    generic_type_node.kind = syntax::TypeKind::named;
    generic_type_node.name = "T";
    generic_type_node.name_id = ast.intern_identifier("T");
    const TypeId generic_type = ast.push_type(generic_type_node);

    const ExprId value = push_name(ast, "value");
    syntax::StmtNode return_stmt;
    return_stmt.kind = syntax::StmtKind::return_;
    return_stmt.return_value = value;
    const StmtId return_stmt_id = ast.push_stmt(return_stmt);
    syntax::StmtNode body_stmt;
    body_stmt.kind = syntax::StmtKind::block;
    body_stmt.statements = {return_stmt_id};
    const StmtId body = ast.push_stmt(body_stmt);

    syntax::ItemNode id_function;
    id_function.kind = syntax::ItemKind::fn_decl;
    id_function.name = "id";
    id_function.name_id = ast.intern_identifier("id");
    id_function.generic_params = {syntax::GenericParamDecl{"T", {}}};
    id_function.params = {syntax::ParamDecl{"value", generic_type, {}, ast.intern_identifier("value")}};
    id_function.return_type = generic_type;
    id_function.body = body;
    const syntax::ItemId id_item = ast.push_item(id_function);

    const TypeHandle i32 = checked.types.builtin(BuiltinType::i32);
    sema::GenericFunctionInstanceInfo instance;
    instance.item = id_item;
    instance.body = body;
    instance.generic_instance_key = test_generic_instance_key();
    instance.signature = checked.make_function_signature();
    instance.signature.name = checked.intern_text("id");
    instance.signature.name_id = id_function.name_id;
    instance.signature.c_name = checked.intern_text("lower_ast_generic_id_i32");
    instance.signature.generic_instance_key = instance.generic_instance_key;
    instance.signature.return_type = i32;
    instance.signature.has_definition = true;
    instance.signature.definition_item = id_item;
    instance.signature.param_types.push_back(i32);
    instance.side_tables.configure_local_dense(
        sema::GenericNodeSpan{value.value, 1U}, {}, {}, sema::GenericNodeSpan{return_stmt_id.value, 1U});
    instance.side_tables.expr_intrinsic_types.front() = i32;
    instance.side_tables.expr_types.front() = i32;
    instance.side_tables.release_analysis_only_storage();
    checked.generic_function_instances.push_back(std::move(instance));

    auto lowered = ir::lower_ast(ast, checked);
    ASSERT_TRUE(lowered) << lowered.error().message;
    ASSERT_EQ(lowered.value().functions.size(), 1U);
    const ir::Function& function = lowered.value().functions.front();
    EXPECT_EQ(lowered.value().text(function.symbol), "lower_ast_generic_id_i32");
    ASSERT_EQ(function.signature_params.size(), 1U);
    EXPECT_TRUE(lowered.value().types.same(function.signature_params.front().type, i32));
    ASSERT_FALSE(function.blocks.empty());
    EXPECT_EQ(function.blocks.front().terminator.kind, TerminatorKind::return_);
    ASSERT_TRUE(is_valid(function.blocks.front().terminator.value));
    ASSERT_LT(function.blocks.front().terminator.value.value, lowered.value().values.size());
    EXPECT_EQ(lowered.value().values[function.blocks.front().terminator.value.value].kind, ValueKind::load);
    EXPECT_TRUE(
        lowered.value().types.same(lowered.value().values[function.blocks.front().terminator.value.value].type, i32));
}

TEST(CoreUnit, LowerAstWhiteBoxCleanupStackDropsNeedsDropParametersOnImplicitReturn)
{
    syntax::AstModule ast;
    CheckedModule checked;

    const TypeHandle resource_type = checked.types.generic_param("T");
    const TypeHandle void_type = checked.types.builtin(BuiltinType::void_);
    const sema::IdentId value_name = ast.intern_identifier("value");

    syntax::StmtNode body_stmt;
    body_stmt.kind = syntax::StmtKind::block;
    const StmtId body = ast.push_stmt(body_stmt);

    syntax::ItemNode function_item;
    function_item.kind = syntax::ItemKind::fn_decl;
    function_item.name = "cleanup";
    function_item.name_id = ast.intern_identifier("cleanup");
    function_item.params = {syntax::ParamDecl{"value", syntax::INVALID_TYPE_ID, {}, value_name}};
    function_item.return_type = syntax::INVALID_TYPE_ID;
    function_item.body = body;

    Lowerer lowerer(ast, checked);
    Function function = lowerer.module_.make_function();
    function.name = lowerer.module_.intern("cleanup");
    function.symbol = lowerer.module_.intern("cleanup");
    function.return_type = void_type;
    function.signature_params.push_back(ir::FunctionParam{lowerer.module_.intern("value"), resource_type});
    const FunctionId function_id = add_function(lowerer.module_, function);

    lowerer.lower_function_body(function_id, Lowerer::FunctionBodyView{function_item.params, body});

    const base::Result<void> verified = ir::verify_module(lowerer.module_);
    ASSERT_TRUE(verified) << verified.error().message;
    const std::string dump = ir::dump_module(lowerer.module_);
    EXPECT_NE(dump.find("alloca drop.flag.value"), std::string::npos);
    EXPECT_NE(dump.find("drop_if"), std::string::npos);
    EXPECT_NE(dump.find("as T"), std::string::npos);
}

TEST(CoreUnit, LowerAstWhiteBoxCleanupStackInterleavesDeferredActionsAndDrops)
{
    syntax::AstModule ast;
    CheckedModule checked;

    const TypeHandle resource_type = checked.types.generic_param("T");
    const TypeHandle void_type = checked.types.builtin(BuiltinType::void_);
    const TypeHandle i32 = checked.types.builtin(BuiltinType::i32);
    const sema::IdentId outer_name = ast.intern_identifier("outer");
    const sema::IdentId inner_name = ast.intern_identifier("inner");

    const ExprId deferred_marker = push_integer(ast, "7");
    set_expr_type(checked, deferred_marker, i32);
    const StmtId defer_stmt = push_defer_stmt(ast, deferred_marker);
    const StmtId body = push_block(ast, {defer_stmt});

    syntax::ItemNode function_item;
    function_item.kind = syntax::ItemKind::fn_decl;
    function_item.name = "cleanup_order";
    function_item.name_id = ast.intern_identifier("cleanup_order");
    function_item.params = {
        syntax::ParamDecl{"outer", syntax::INVALID_TYPE_ID, {}, outer_name},
        syntax::ParamDecl{"inner", syntax::INVALID_TYPE_ID, {}, inner_name},
    };
    function_item.return_type = syntax::INVALID_TYPE_ID;
    function_item.body = body;

    Lowerer lowerer(ast, checked);
    Function function = lowerer.module_.make_function();
    function.name = lowerer.module_.intern("cleanup_order");
    function.symbol = lowerer.module_.intern("cleanup_order");
    function.return_type = void_type;
    function.signature_params.push_back(ir::FunctionParam{lowerer.module_.intern("outer"), resource_type});
    function.signature_params.push_back(ir::FunctionParam{lowerer.module_.intern("inner"), resource_type});
    const FunctionId function_id = add_function(lowerer.module_, function);

    lowerer.lower_function_body(function_id, Lowerer::FunctionBodyView{function_item.params, body});

    const base::Result<void> verified = ir::verify_module(lowerer.module_);
    ASSERT_TRUE(verified) << verified.error().message;

    base::usize deferred_marker_index = lowerer.module_.values.size();
    std::vector<std::string_view> dropped_slots;
    std::vector<base::usize> drop_indexes;
    for (base::usize index = 0; index < lowerer.module_.values.size(); ++index) {
        const Value& value = lowerer.module_.values[index];
        if (value.kind == ValueKind::integer_literal && lowerer.module_.text(value.text) == "7") {
            deferred_marker_index = index;
        }
        if (value.kind == ValueKind::drop_if) {
            ASSERT_TRUE(is_valid(value.object));
            ASSERT_LT(value.object.value, lowerer.module_.values.size());
            dropped_slots.push_back(lowerer.module_.text(lowerer.module_.values[value.object.value].name));
            drop_indexes.push_back(index);
        }
    }

    ASSERT_EQ(dropped_slots.size(), 2U);
    ASSERT_EQ(drop_indexes.size(), 2U);
    ASSERT_LT(deferred_marker_index, drop_indexes.front());
    EXPECT_EQ(dropped_slots[0], "inner");
    EXPECT_EQ(dropped_slots[1], "outer");
}

TEST(CoreUnit, LowerAstWhiteBoxCleanupStackDropsOldWholeLocalBeforeOverwrite)
{
    syntax::AstModule ast;
    CheckedModule checked;

    const TypeHandle resource_type = checked.types.generic_param("T");
    const TypeHandle void_type = checked.types.builtin(BuiltinType::void_);
    const sema::IdentId old_value_name = ast.intern_identifier("old_value");
    const sema::IdentId new_value_name = ast.intern_identifier("new_value");

    const ExprId old_value = push_name(ast, "old_value");
    const ExprId new_value = push_name(ast, "new_value");
    const ExprId slot_ref = push_name(ast, "slot");
    set_expr_type(checked, old_value, resource_type);
    set_expr_type(checked, new_value, resource_type);
    set_expr_type(checked, slot_ref, resource_type);
    set_expr_owned_use_mode(checked, old_value, sema::OwnedUseMode::owned_consume);
    set_expr_owned_use_mode(checked, new_value, sema::OwnedUseMode::owned_consume);

    const StmtId slot_decl = push_var_stmt(ast, "slot", old_value, true);
    set_stmt_local_type(checked, slot_decl, resource_type);
    const StmtId overwrite = push_assign_stmt(ast, slot_ref, new_value);
    const StmtId body = push_block(ast, {slot_decl, overwrite});

    syntax::ItemNode function_item;
    function_item.kind = syntax::ItemKind::fn_decl;
    function_item.name = "cleanup_overwrite";
    function_item.name_id = ast.intern_identifier("cleanup_overwrite");
    function_item.params = {
        syntax::ParamDecl{"old_value", syntax::INVALID_TYPE_ID, {}, old_value_name},
        syntax::ParamDecl{"new_value", syntax::INVALID_TYPE_ID, {}, new_value_name},
    };
    function_item.return_type = syntax::INVALID_TYPE_ID;
    function_item.body = body;

    Lowerer lowerer(ast, checked);
    Function function = lowerer.module_.make_function();
    function.name = lowerer.module_.intern("cleanup_overwrite");
    function.symbol = lowerer.module_.intern("cleanup_overwrite");
    function.return_type = void_type;
    function.signature_params.push_back(ir::FunctionParam{lowerer.module_.intern("old_value"), resource_type});
    function.signature_params.push_back(ir::FunctionParam{lowerer.module_.intern("new_value"), resource_type});
    const FunctionId function_id = add_function(lowerer.module_, function);

    lowerer.lower_function_body(function_id, Lowerer::FunctionBodyView{function_item.params, body});

    const base::Result<void> verified = ir::verify_module(lowerer.module_);
    ASSERT_TRUE(verified) << verified.error().message;

    std::vector<std::string_view> dropped_slots;
    for (const Value& value : lowerer.module_.values) {
        if (value.kind != ValueKind::drop_if) {
            continue;
        }
        ASSERT_TRUE(is_valid(value.object));
        ASSERT_LT(value.object.value, lowerer.module_.values.size());
        dropped_slots.push_back(lowerer.module_.text(lowerer.module_.values[value.object.value].name));
    }

    ASSERT_EQ(dropped_slots.size(), 4U);
    EXPECT_EQ(dropped_slots[0], "slot");
    EXPECT_EQ(dropped_slots[1], "slot");
    EXPECT_EQ(dropped_slots[2], "new_value");
    EXPECT_EQ(dropped_slots[3], "old_value");

    const std::string dump = ir::dump_module(lowerer.module_);
    EXPECT_EQ(count_occurrences(dump, "drop_if"), 4U);
    EXPECT_NE(dump.find("alloca drop.flag.slot"), std::string::npos);
}

TEST(CoreUnit, LowerAstWhiteBoxRejectsMissingRetainedGenericBodyView)
{
    syntax::AstModule ast;
    CheckedModule checked;

    sema::GenericFunctionInstanceInfo instance;
    instance.generic_instance_key = test_generic_instance_key();
    instance.signature = checked.make_function_signature();
    instance.signature.generic_instance_key = instance.generic_instance_key;
    instance.signature.has_definition = true;
    checked.generic_function_instances.push_back(std::move(instance));

    auto lowered = ir::lower_ast(ast, checked);
    ASSERT_FALSE(lowered);
    EXPECT_EQ(lowered.error().code, base::ErrorCode::internal_error);
    EXPECT_NE(lowered.error().message.find("generic instance body missing retained sema view"), std::string::npos);
}

TEST(CoreUnit, LowerAstWhiteBoxRejectsInvalidRetainedGenericBody)
{
    syntax::AstModule ast;
    CheckedModule checked;

    syntax::StmtNode body_stmt;
    body_stmt.kind = syntax::StmtKind::block;
    const StmtId body = ast.push_stmt(body_stmt);

    syntax::ItemNode id_function;
    id_function.kind = syntax::ItemKind::fn_decl;
    id_function.name = "id";
    id_function.name_id = ast.intern_identifier("id");
    id_function.body = body;
    const syntax::ItemId id_item = ast.push_item(id_function);

    sema::GenericFunctionInstanceInfo instance;
    instance.item = id_item;
    instance.generic_instance_key = test_generic_instance_key();
    instance.signature = checked.make_function_signature();
    instance.signature.generic_instance_key = instance.generic_instance_key;
    instance.signature.has_definition = true;
    checked.generic_function_instances.push_back(std::move(instance));

    auto lowered = ir::lower_ast(ast, checked);
    ASSERT_FALSE(lowered);
    EXPECT_EQ(lowered.error().code, base::ErrorCode::internal_error);
    EXPECT_NE(lowered.error().message.find("generic instance body missing retained sema view"), std::string::npos);
}

TEST(CoreUnit, LowerAstWhiteBoxRejectsMissingRetainedTraitDefaultBodyView)
{
    syntax::AstModule ast;
    CheckedModule checked;

    const TypeHandle i32 = checked.types.builtin(BuiltinType::i32);
    const sema::IdentId reader_name = ast.intern_identifier("Reader");

    sema::TraitDefaultMethodInstanceInfo instance;
    instance.impl_key = sema::TraitImplLookupKey{
        0U,
        reader_name,
        i32.value,
        {},
    };
    instance.trait_name_id = reader_name;
    instance.signature = checked.make_function_signature();
    instance.signature.name = checked.intern_text("is_empty");
    instance.signature.name_id = ast.intern_identifier("is_empty");
    instance.signature.c_name = checked.intern_text("lower_ast_trait_default_is_empty");
    instance.signature.has_definition = true;
    instance.signature.is_trait_default_method_instance = true;
    instance.signature.return_type = checked.types.builtin(BuiltinType::bool_);
    checked.trait_default_method_instances.push_back(std::move(instance));

    auto lowered = ir::lower_ast(ast, checked);
    ASSERT_FALSE(lowered);
    EXPECT_EQ(lowered.error().code, base::ErrorCode::internal_error);
    EXPECT_NE(lowered.error().message.find("trait default method instance body missing retained sema view"),
        std::string::npos);
}

TEST(CoreUnit, LowerAstWhiteBoxTraitDefaultBodyAndDeclarationInvalidViewGuards)
{
    syntax::AstModule ast;
    CheckedModule checked;

    Lowerer empty_lowerer(ast, checked);
    empty_lowerer.lower_generic_function_body(INVALID_FUNCTION_ID, {});
    empty_lowerer.lower_trait_default_method_body(INVALID_FUNCTION_ID, {});

    const TypeHandle i32 = checked.types.builtin(BuiltinType::i32);
    const sema::IdentId reader_name = ast.intern_identifier("Reader");
    const sema::IdentId method_name = ast.intern_identifier("is_empty");
    const sema::IdentId value_name = ast.intern_identifier("value");

    syntax::StmtNode body_stmt;
    body_stmt.kind = syntax::StmtKind::block;
    const StmtId body = ast.push_stmt(body_stmt);

    syntax::ItemNode default_method;
    default_method.kind = syntax::ItemKind::fn_decl;
    default_method.name = "is_empty";
    default_method.name_id = method_name;
    default_method.params = {syntax::ParamDecl{"value", syntax::INVALID_TYPE_ID, {}, value_name}};
    default_method.body = body;
    default_method.is_trait_default_method = true;
    const syntax::ItemId method_item = ast.push_item(default_method);

    const sema::TraitImplLookupKey impl_key{
        0U,
        reader_name,
        i32.value,
        {},
    };

    sema::TraitDefaultMethodInstanceInfo invalid_instance;
    invalid_instance.impl_key = impl_key;
    invalid_instance.trait_name_id = reader_name;
    invalid_instance.signature = checked.make_function_signature();
    invalid_instance.signature.name = checked.intern_text("invalid_default");
    invalid_instance.signature.name_id = method_name;
    invalid_instance.signature.c_name = checked.intern_text("invalid_default");
    invalid_instance.signature.has_definition = true;
    invalid_instance.signature.is_trait_default_method_instance = true;
    invalid_instance.signature.return_type = checked.types.builtin(BuiltinType::bool_);
    checked.trait_default_method_instances.push_back(std::move(invalid_instance));

    sema::GenericFunctionInstanceInfo invalid_generic_instance;
    invalid_generic_instance.generic_instance_key = test_generic_instance_key();
    invalid_generic_instance.signature = checked.make_function_signature();
    invalid_generic_instance.signature.generic_instance_key = invalid_generic_instance.generic_instance_key;
    invalid_generic_instance.signature.has_definition = true;
    checked.generic_function_instances.push_back(std::move(invalid_generic_instance));

    sema::TraitDefaultMethodInstanceInfo valid_without_param_types;
    valid_without_param_types.item = method_item;
    valid_without_param_types.body = body;
    valid_without_param_types.impl_key = impl_key;
    valid_without_param_types.trait_name_id = reader_name;
    valid_without_param_types.signature = checked.make_function_signature();
    valid_without_param_types.signature.name = checked.intern_text("is_empty");
    valid_without_param_types.signature.name_id = method_name;
    valid_without_param_types.signature.c_name = checked.intern_text("lower_ast_trait_default_missing_param_type");
    valid_without_param_types.signature.has_definition = true;
    valid_without_param_types.signature.is_trait_default_method_instance = true;
    valid_without_param_types.signature.return_type = checked.types.builtin(BuiltinType::bool_);
    checked.trait_default_method_instances.push_back(std::move(valid_without_param_types));

    Lowerer lowerer(ast, checked);
    lowerer.lower_function_declarations();

    ASSERT_EQ(lowerer.generic_instance_functions_.size(), 1U);
    EXPECT_FALSE(is_valid(lowerer.generic_instance_functions_.front()));
    ASSERT_EQ(lowerer.trait_default_instance_functions_.size(), 2U);
    EXPECT_FALSE(is_valid(lowerer.trait_default_instance_functions_[0]));
    ASSERT_TRUE(is_valid(lowerer.trait_default_instance_functions_[1]));
    const Function& lowered_function = lowerer.module_.functions[lowerer.trait_default_instance_functions_[1].value];
    ASSERT_EQ(lowered_function.signature_params.size(), 1U);
    EXPECT_FALSE(sema::is_valid(lowered_function.signature_params.front().type));
}

} // namespace aurex::test
