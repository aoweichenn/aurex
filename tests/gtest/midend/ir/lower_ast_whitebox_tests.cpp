#include <aurex/backend/llvm_backend.hpp>
#include <aurex/frontend/lex/lexer.hpp>
#include <aurex/frontend/parse/parser.hpp>
#include <aurex/frontend/sema/sema.hpp>
#include <aurex/infrastructure/query/query_key.hpp>
#include <aurex/midend/ir/ir_dump.hpp>
#include <aurex/midend/ir/lower_ast.hpp>
#include <aurex/midend/ir/verify.hpp>

#include <array>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <midend/ir/lowering/private/lower_ast_internal.hpp>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

using ir::AbiCallConv;
using ir::add_block;
using ir::add_function;
using ir::BinaryOp;
using ir::BlockId;
using ir::CleanupAbiPolicy;
using ir::Function;
using ir::FunctionId;
using ir::INVALID_BLOCK_ID;
using ir::INVALID_FUNCTION_ID;
using ir::INVALID_IR_TEXT_ID;
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
using sema::FunctionLookupKey;
using sema::FunctionSignature;
using sema::INVALID_TYPE_HANDLE;
using sema::PointerMutability;
using sema::TypeHandle;
using syntax::ExprId;
using syntax::ExprKind;
using syntax::StmtId;
using syntax::TypeId;

constexpr std::string_view LOWER_AST_GENERIC_TEST_MODULE = "lower_ast_generic";
constexpr std::string_view LOWER_AST_GENERIC_TEST_FUNCTION = "id";
constexpr std::string_view LOWER_AST_ROLLBACK_FLAG_ALLOCA = "drop.flag.aggregate.rollback";
constexpr std::string_view LOWER_AST_ROLLBACK_ARRAY_FIELD_PREFIX = "aggregate.rollback.";
constexpr base::SourceId LOWER_AST_RAII_TEST_SOURCE_ID{808};
constexpr base::u8 LOWER_AST_INVALID_CLEANUP_POLICY_VALUE = 240U;

[[nodiscard]] ExprId push_name(syntax::AstModule& ast, const std::string_view text)
{
    syntax::NameExprPayload payload;
    payload.text = text;
    payload.text_id = ast.intern_identifier(text);
    return ast.push_name_expr({}, std::move(payload));
}

[[nodiscard]] ExprId push_field(syntax::AstModule& ast, const ExprId object, const std::string_view field)
{
    return ast.push_field_expr({}, object, field, ast.intern_identifier(field));
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

[[nodiscard]] syntax::AstModule parse_lower_ast_source(const std::string_view source)
{
    base::DiagnosticSink diagnostics;
    lex::Lexer lexer(LOWER_AST_RAII_TEST_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    if (!tokens) {
        ADD_FAILURE() << tokens.error().message;
        return {};
    }

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    if (!parsed) {
        ADD_FAILURE() << parsed.error().message;
        return {};
    }
    if (diagnostics.has_error()) {
        for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
            ADD_FAILURE() << diagnostic.message;
        }
        return {};
    }
    return parsed.take_value();
}

struct LowerAstAnalysis {
    syntax::AstModule ast;
    CheckedModule checked;
};

[[nodiscard]] LowerAstAnalysis analyze_lower_ast_source(const std::string_view source)
{
    LowerAstAnalysis analysis;
    analysis.ast = parse_lower_ast_source(source);
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(analysis.ast, diagnostics);
    auto checked = analyzer.analyze();
    if (!checked) {
        for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
            ADD_FAILURE() << diagnostic.message;
        }
        ADD_FAILURE() << checked.error().message;
        return analysis;
    }
    analysis.checked = checked.take_value();
    return analysis;
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

void add_struct_info(CheckedModule& checked, syntax::AstModule& ast, const std::string_view name, const TypeHandle type,
    const std::initializer_list<std::pair<std::string_view, TypeHandle>> fields)
{
    sema::StructInfo info;
    info.name = checked.intern_text(name);
    info.name_id = ast.intern_identifier(name);
    info.c_name = checked.intern_text(std::string(name));
    info.module = syntax::ModuleId{0U};
    info.type = type;
    info.fields = checked.make_struct_field_list();
    for (const auto& [field_name, field_type] : fields) {
        info.fields.push_back(sema::StructFieldInfo{
            .name = checked.intern_text(field_name),
            .name_id = ast.intern_identifier(field_name),
            .c_name = checked.intern_text(field_name),
            .module = syntax::ModuleId{0U},
            .type = field_type,
            .range = {},
            .visibility = syntax::Visibility::public_,
            .stable_key = {},
        });
    }
    checked.structs.emplace(sema::ModuleLookupKey{0U, info.name_id}, std::move(info));
}

[[nodiscard]] FunctionLookupKey add_destructor_signature(
    CheckedModule& checked, syntax::AstModule& ast, const TypeHandle self_type, const std::string_view symbol)
{
    const sema::IdentId drop_name = ast.intern_identifier("drop");
    FunctionLookupKey key{0U, self_type.value, drop_name};
    FunctionSignature signature = checked.make_function_signature();
    signature.name = checked.intern_text("drop");
    signature.name_id = drop_name;
    signature.semantic_key = key;
    signature.c_name = checked.intern_text(symbol);
    signature.module = syntax::ModuleId{0U};
    signature.method_owner_type = self_type;
    signature.return_type = checked.types.builtin(BuiltinType::void_);
    signature.param_types = checked.make_type_handle_list();
    signature.param_types.push_back(self_type);
    signature.is_method = true;
    signature.has_self_param = true;
    signature.is_trait_impl_method = true;
    signature.is_destructor = true;
    checked.functions.emplace(key, std::move(signature));

    sema::DestructorInfo destructor;
    destructor.module = syntax::ModuleId{0U};
    destructor.self_type = self_type;
    destructor.function_key = key;
    destructor.fingerprint = sema::destructor_info_fingerprint(destructor);
    checked.destructors.emplace(self_type.value, destructor);
    return key;
}

void add_enum_case_info(CheckedModule& checked, syntax::AstModule& ast, const std::string_view enum_name,
    const std::string_view case_name, const TypeHandle enum_type, const TypeHandle payload_type,
    const std::string_view c_name, const std::string_view value_text)
{
    EnumCaseInfo info = checked.make_enum_case_info();
    info.name = checked.intern_text(case_name);
    info.name_id = ast.intern_identifier(case_name);
    info.c_name = checked.intern_text(c_name);
    info.module = syntax::ModuleId{0U};
    info.type = enum_type;
    info.payload_type = payload_type;
    if (sema::is_valid(payload_type)) {
        info.payload_types.push_back(payload_type);
    }
    info.value_text = checked.intern_text(value_text);
    info.enum_name = checked.intern_text(enum_name);
    info.case_name = checked.intern_text(case_name);
    info.case_name_id = info.name_id;
    checked.enum_cases.emplace(sema::ModuleLookupKey{0U, info.name_id}, std::move(info));
}

[[nodiscard]] FunctionId add_ir_destructor_function(
    Lowerer& lowerer, const TypeHandle self_type, const std::string_view symbol)
{
    Function destructor = lowerer.module_.make_function();
    destructor.name = lowerer.module_.intern("drop");
    destructor.symbol = lowerer.module_.intern(symbol);
    destructor.return_type = lowerer.module_.types.builtin(BuiltinType::void_);
    destructor.signature_params.push_back(ir::FunctionParam{lowerer.module_.intern("self"), self_type});
    Value self_param = lowerer.module_.make_value();
    self_param.kind = ValueKind::param;
    self_param.name = lowerer.module_.intern("self");
    self_param.type = self_type;
    const ValueId self_param_id = add_value(lowerer.module_, self_param);
    destructor.param_values.push_back(self_param_id);
    const BlockId entry = add_block(lowerer.module_, destructor, "entry");
    destructor.blocks[entry.value].values.push_back(self_param_id);
    destructor.blocks[entry.value].terminator.kind = TerminatorKind::return_;
    const FunctionId id = add_function(lowerer.module_, destructor);
    lowerer.function_symbols_[lowerer.module_.functions[id.value].symbol] = id;
    return id;
}

[[nodiscard]] FunctionId prepare_current_function(
    Lowerer& lowerer, const std::string_view name, const TypeHandle return_type)
{
    Function function = lowerer.module_.make_function();
    function.name = lowerer.module_.intern(name);
    function.symbol = lowerer.module_.intern(name);
    function.return_type = return_type;
    const BlockId entry = add_block(lowerer.module_, function, "entry");
    const FunctionId function_id = add_function(lowerer.module_, function);
    lowerer.current_function_ = &lowerer.module_.functions[function_id.value];
    lowerer.current_block_ = entry;
    return function_id;
}

[[nodiscard]] ValueId append_alloca(Lowerer& lowerer, const std::string_view name, const TypeHandle value_type)
{
    Value slot = lowerer.module_.make_value();
    slot.kind = ValueKind::alloca;
    slot.name = lowerer.module_.intern(name);
    slot.type = lowerer.module_.types.pointer(PointerMutability::mut, value_type);
    return lowerer.append_value(slot);
}

[[nodiscard]] base::usize count_named_values(
    const Lowerer& lowerer, const ValueKind kind, const std::string_view name, const base::usize first_value)
{
    base::usize count = 0;
    for (base::usize index = first_value; index < lowerer.module_.values.size(); ++index) {
        const Value& value = lowerer.module_.values[index];
        if (value.kind == kind && lowerer.module_.text(value.name) == name) {
            ++count;
        }
    }
    return count;
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
    const ExprId untyped_name = push_name(ast, "untyped");
    const ExprId integer = push_integer(ast);
    const ExprId invalid_value_expr = push_invalid_expr(ast);
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
    set_expr_type(checked, invalid_value_expr, i32);
    set_expr_type(checked, condition_lhs, bool_type);
    set_expr_type(checked, condition_rhs, bool_type);
    set_expr_type(checked, logical, bool_type);

    Lowerer lowerer(ast, checked);

    EXPECT_EQ(lowerer.lower_place_address(syntax::INVALID_EXPR_ID).address.value, INVALID_VALUE_ID.value);
    EXPECT_EQ(lowerer.lower_place_address(ExprId{999}).address.value, INVALID_VALUE_ID.value);
    EXPECT_EQ(lowerer.lower_place_address(missing_place).address.value, INVALID_VALUE_ID.value);
    EXPECT_EQ(lowerer.lower_place_address(integer).address.value, INVALID_VALUE_ID.value);
    EXPECT_EQ(lowerer.lower_object_place_or_value(untyped_name).address.value, INVALID_VALUE_ID.value);
    EXPECT_EQ(lowerer.lower_object_place_or_value(invalid_value_expr).address.value, INVALID_VALUE_ID.value);
    EXPECT_FALSE(lowerer.is_local_slot_type(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(lowerer.pointee_is_mutable(integer));

    Value slot_value = lowerer.module_.make_value();
    slot_value.kind = ValueKind::alloca;
    slot_value.type = ptr_i32;
    const ValueId slot_id = lowerer.append_value(slot_value);
    lowerer.locals_.emplace(ast.find_identifier("slot"),
        ir::detail::LocalBinding{
            .slot = slot_id,
            .cleanup_flag = INVALID_VALUE_ID,
            .type = i32,
            .is_mutable = true,
            .field_cleanups = {},
        });
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
    EXPECT_NE(dump.find("abi(generic_marker_only)"), std::string::npos);
    bool saw_generic_cleanup_policy = false;
    for (const Value& value : lowerer.module_.values) {
        if (value.kind == ValueKind::drop_if && lowerer.module_.types.same(value.target_type, resource_type)) {
            saw_generic_cleanup_policy = true;
            EXPECT_EQ(value.cleanup_policy, CleanupAbiPolicy::generic_marker_only);
        }
    }
    EXPECT_TRUE(saw_generic_cleanup_policy);
}

TEST(CoreUnit, LowerAstWhiteBoxCustomDestructorLowersToRuntimeCallAndLlvm)
{
    const std::string_view source = "module lower_raii_runtime;\n"
                                    "struct File { fd: i32; }\n"
                                    "impl Drop for File {\n"
                                    "  fn drop(self: deinit File) -> void {}\n"
                                    "}\n"
                                    "fn run(value: File) -> void {}\n"
                                    "fn main() -> void {}\n";

    LowerAstAnalysis analysis = analyze_lower_ast_source(source);
    auto lowered = ir::lower_ast(analysis.ast, analysis.checked);
    ASSERT_TRUE(lowered) << lowered.error().message;
    const base::Result<void> verified = ir::verify_module(lowered.value());
    ASSERT_TRUE(verified) << verified.error().message;

    base::usize destructor_call_count = 0;
    base::usize destructor_self_cleanup_count = 0;
    for (const Value& value : lowered.value().values) {
        if (value.kind == ValueKind::call && lowered.value().text(value.name).find("drop") != std::string_view::npos) {
            ++destructor_call_count;
            ASSERT_EQ(value.args.size(), 1U);
            ASSERT_LT(value.args.front().value, lowered.value().values.size());
            EXPECT_EQ(lowered.value().values[value.args.front().value].kind, ValueKind::load);
        }
        if (value.kind == ValueKind::drop_if && is_valid(value.object)
            && value.object.value < lowered.value().values.size()
            && lowered.value().text(lowered.value().values[value.object.value].name) == "self") {
            ++destructor_self_cleanup_count;
        }
    }
    EXPECT_EQ(destructor_call_count, 1U);
    EXPECT_EQ(destructor_self_cleanup_count, 0U);

    auto llvm_ir = backend::emit_llvm_ir({&lowered.value(), "lower_raii_runtime"});
    ASSERT_TRUE(llvm_ir) << llvm_ir.error().message;
    EXPECT_NE(llvm_ir.value().text.find("call void @"), std::string::npos) << llvm_ir.value().text;
    EXPECT_NE(llvm_ir.value().text.find("drop"), std::string::npos) << llvm_ir.value().text;
}

TEST(CoreUnit, LowerAstWhiteBoxCustomDestructorRootRunsBeforeFieldCleanup)
{
    syntax::AstModule ast;
    CheckedModule checked;

    const TypeHandle field_type = checked.types.generic_param("Field");
    const TypeHandle box_type = checked.types.named_struct("Box", "Box", false);
    const TypeHandle void_type = checked.types.builtin(BuiltinType::void_);
    add_struct_info(checked, ast, "Box", box_type, {{"inner", field_type}});
    static_cast<void>(add_destructor_signature(checked, ast, box_type, "drop_box"));

    const sema::IdentId value_name = ast.intern_identifier("value");
    const StmtId body = push_block(ast, {});
    syntax::ItemNode function_item;
    function_item.kind = syntax::ItemKind::fn_decl;
    function_item.name = "cleanup_custom_box";
    function_item.name_id = ast.intern_identifier("cleanup_custom_box");
    function_item.params = {syntax::ParamDecl{"value", syntax::INVALID_TYPE_ID, {}, value_name}};
    function_item.return_type = syntax::INVALID_TYPE_ID;
    function_item.body = body;

    Lowerer lowerer(ast, checked);
    lowerer.lower_record_layouts();
    static_cast<void>(add_ir_destructor_function(lowerer, box_type, "drop_box"));
    Function function = lowerer.module_.make_function();
    function.name = lowerer.module_.intern("cleanup_custom_box");
    function.symbol = lowerer.module_.intern("cleanup_custom_box");
    function.return_type = void_type;
    function.signature_params.push_back(ir::FunctionParam{lowerer.module_.intern("value"), box_type});
    const FunctionId function_id = add_function(lowerer.module_, function);

    lowerer.lower_function_body(function_id, Lowerer::FunctionBodyView{function_item.params, body});

    const base::Result<void> verified = ir::verify_module(lowerer.module_);
    ASSERT_TRUE(verified) << verified.error().message;

    base::usize root_drop_if_index = lowerer.module_.values.size();
    base::usize field_drop_if_index = lowerer.module_.values.size();
    base::usize root_call_index = lowerer.module_.values.size();
    base::usize field_drop_count = 0;
    for (base::usize index = 0; index < lowerer.module_.values.size(); ++index) {
        const Value& value = lowerer.module_.values[index];
        if (value.kind == ValueKind::drop_if && is_valid(value.object)
            && value.object.value < lowerer.module_.values.size()) {
            const std::string_view object_name = lowerer.module_.text(lowerer.module_.values[value.object.value].name);
            if (object_name == "value") {
                root_drop_if_index = index;
                EXPECT_EQ(value.target_type.value, box_type.value);
                EXPECT_EQ(value.cleanup_policy, CleanupAbiPolicy::static_custom_destructor);
            } else if (lowerer.module_.text(value.name) == "inner") {
                field_drop_if_index = index;
                EXPECT_EQ(value.cleanup_policy, CleanupAbiPolicy::generic_marker_only);
                ++field_drop_count;
            }
        }
        if (value.kind == ValueKind::call && lowerer.module_.text(value.name) == "drop_box") {
            root_call_index = index;
        }
    }

    ASSERT_LT(root_drop_if_index, lowerer.module_.values.size());
    ASSERT_LT(field_drop_if_index, lowerer.module_.values.size());
    ASSERT_LT(root_call_index, lowerer.module_.values.size());
    EXPECT_LT(root_drop_if_index, field_drop_if_index);
    EXPECT_LT(root_drop_if_index, root_call_index);
    EXPECT_EQ(field_drop_count, 1U);

    const std::string dump = ir::dump_module(lowerer.module_);
    EXPECT_NE(dump.find("alloca drop.flag.value"), std::string::npos) << dump;
    EXPECT_NE(dump.find("alloca drop.flag.value.inner"), std::string::npos) << dump;
    EXPECT_EQ(count_occurrences(dump, "call drop_box"), 1U) << dump;
}

TEST(CoreUnit, LowerAstWhiteBoxRuntimeDropTypeProbeCoversStructuralShapes)
{
    syntax::AstModule ast;
    CheckedModule checked;

    const TypeHandle resource_type = checked.types.named_struct("Resource", "Resource", false);
    const TypeHandle wrapper_type = checked.types.named_struct("Wrapper", "Wrapper", false);
    const TypeHandle i32 = checked.types.builtin(BuiltinType::i32);
    const TypeHandle tuple_type = checked.types.tuple({i32, resource_type});
    const TypeHandle trivial_tuple_type = checked.types.tuple({i32, checked.types.builtin(BuiltinType::bool_)});
    const TypeHandle array_type = checked.types.array(2U, resource_type);
    const TypeHandle empty_array_type = checked.types.array(0U, resource_type);
    const TypeHandle scalar_array_type = checked.types.array(2U, i32);
    const TypeHandle enum_type = checked.types.named_enum("Choice", "Choice");
    checked.types.set_enum_underlying(enum_type, i32);
    checked.types.set_enum_payload_layout(enum_type, resource_type, 8U, 4U);
    add_struct_info(checked, ast, "Resource", resource_type, {});
    add_struct_info(checked, ast, "Wrapper", wrapper_type, {{"trivial", i32}, {"resource", resource_type}});
    static_cast<void>(add_destructor_signature(checked, ast, resource_type, "drop_resource"));
    add_enum_case_info(checked, ast, "Choice", "some", enum_type, resource_type, "Choice_some", "1");
    add_enum_case_info(checked, ast, "Choice", "none", enum_type, INVALID_TYPE_HANDLE, "Choice_none", "0");

    Lowerer lowerer(ast, checked);
    lowerer.lower_record_layouts();

    EXPECT_FALSE(lowerer.type_may_emit_runtime_drop(INVALID_TYPE_HANDLE, ir::detail::CleanupDropMode::full));
    EXPECT_FALSE(lowerer.type_may_emit_runtime_drop(
        TypeHandle{999U}, ir::detail::CleanupDropMode::full));
    EXPECT_FALSE(lowerer.type_may_emit_runtime_drop(trivial_tuple_type, ir::detail::CleanupDropMode::full));
    EXPECT_FALSE(lowerer.type_may_emit_runtime_drop(empty_array_type, ir::detail::CleanupDropMode::full));
    EXPECT_FALSE(lowerer.type_may_emit_runtime_drop(scalar_array_type, ir::detail::CleanupDropMode::full));
    EXPECT_TRUE(lowerer.type_may_emit_runtime_drop(resource_type, ir::detail::CleanupDropMode::custom_destructor_only));
    EXPECT_FALSE(lowerer.type_may_emit_runtime_drop(wrapper_type, ir::detail::CleanupDropMode::custom_destructor_only));
    EXPECT_TRUE(lowerer.type_may_emit_runtime_drop(wrapper_type, ir::detail::CleanupDropMode::full));
    EXPECT_TRUE(lowerer.type_may_emit_runtime_drop(tuple_type, ir::detail::CleanupDropMode::full));
    EXPECT_TRUE(lowerer.type_may_emit_runtime_drop(array_type, ir::detail::CleanupDropMode::full));
    EXPECT_TRUE(lowerer.type_may_emit_runtime_drop(enum_type, ir::detail::CleanupDropMode::full));
    EXPECT_FALSE(
        lowerer.type_may_emit_runtime_drop(checked.types.builtin(BuiltinType::bool_), ir::detail::CleanupDropMode::full));

    const sema::IdentId missing_drop_name = ast.intern_identifier("missing_drop");
    sema::DestructorInfo missing_signature_destructor;
    missing_signature_destructor.self_type = wrapper_type;
    missing_signature_destructor.function_key = sema::FunctionLookupKey{0U, wrapper_type.value, missing_drop_name};
    EXPECT_EQ(lowerer.destructor_call_target(missing_signature_destructor).function.value, INVALID_FUNCTION_ID.value);

    const FunctionLookupKey drop_key = add_destructor_signature(checked, ast, wrapper_type, "drop_wrapper");
    const sema::DestructorInfo& wrapper_destructor = checked.destructors.at(wrapper_type.value);
    const ir::detail::CallTarget unresolved_drop = lowerer.destructor_call_target(wrapper_destructor);
    EXPECT_EQ(unresolved_drop.function.value, INVALID_FUNCTION_ID.value);
    EXPECT_EQ(lowerer.module_.text(unresolved_drop.symbol), "drop_wrapper");

    checked.functions.at(drop_key).c_name = sema::InternedText{};
    EXPECT_EQ(lowerer.destructor_call_target(wrapper_destructor).function.value, INVALID_FUNCTION_ID.value);
}

TEST(CoreUnit, LowerAstWhiteBoxCleanupAbiPolicyClassifiesMarkerKinds)
{
    syntax::AstModule ast;
    CheckedModule checked;

    const TypeHandle generic =
        checked.types.generic_param(sema::generic_param_identity_from_text("cleanup_policy.T"), "T");
    const TypeHandle associated = checked.types.associated_projection(generic, query::MemberKey{}, "Item");
    const TypeHandle opaque = checked.types.opaque_struct("cleanup_policy.Opaque", "cleanup_policy_Opaque");
    const TypeHandle record = checked.types.named_struct("cleanup_policy.Record", "cleanup_policy_Record", false);
    const TypeHandle i32 = checked.types.builtin(BuiltinType::i32);
    const TypeHandle bool_type = checked.types.builtin(BuiltinType::bool_);
    const TypeHandle pointer = checked.types.pointer(PointerMutability::mut, i32);
    const TypeHandle reference = checked.types.reference(PointerMutability::const_, i32);
    const TypeHandle slice = checked.types.slice(PointerMutability::const_, i32);
    const TypeHandle function_type =
        checked.types.function(sema::FunctionCallConv::aurex, false, false, std::span<const TypeHandle>{}, i32);
    const TypeHandle enum_type = checked.types.named_enum("cleanup_policy.Enum", "cleanup_policy_Enum");
    const TypeHandle array = checked.types.array(2U, generic);
    const TypeHandle tuple = checked.types.tuple({i32, generic});
    checked.types.set_enum_underlying(enum_type, i32);
    add_struct_info(checked, ast, "Record", record, {{"value", generic}});
    static_cast<void>(add_destructor_signature(checked, ast, record, "drop_record"));

    Lowerer lowerer(ast, checked);
    lowerer.lower_record_layouts();

    EXPECT_EQ(lowerer.cleanup_abi_policy(generic, ir::detail::CleanupDropMode::full),
        CleanupAbiPolicy::generic_marker_only);
    EXPECT_EQ(lowerer.cleanup_abi_policy(associated, ir::detail::CleanupDropMode::full),
        CleanupAbiPolicy::associated_projection_marker_only);
    EXPECT_EQ(lowerer.cleanup_abi_policy(opaque, ir::detail::CleanupDropMode::full),
        CleanupAbiPolicy::opaque_marker_only);
    EXPECT_EQ(lowerer.cleanup_abi_policy(tuple, ir::detail::CleanupDropMode::full),
        CleanupAbiPolicy::structural_static);
    EXPECT_EQ(lowerer.cleanup_abi_policy(enum_type, ir::detail::CleanupDropMode::full),
        CleanupAbiPolicy::structural_static);
    EXPECT_EQ(lowerer.cleanup_abi_policy(array, ir::detail::CleanupDropMode::full),
        CleanupAbiPolicy::structural_static);
    EXPECT_EQ(lowerer.cleanup_abi_policy(record, ir::detail::CleanupDropMode::full),
        CleanupAbiPolicy::static_custom_destructor);
    EXPECT_EQ(lowerer.cleanup_abi_policy(bool_type, ir::detail::CleanupDropMode::full),
        CleanupAbiPolicy::unknown_marker_only);
    EXPECT_EQ(lowerer.cleanup_abi_policy(pointer, ir::detail::CleanupDropMode::full),
        CleanupAbiPolicy::unknown_marker_only);
    EXPECT_EQ(lowerer.cleanup_abi_policy(reference, ir::detail::CleanupDropMode::full),
        CleanupAbiPolicy::unknown_marker_only);
    EXPECT_EQ(lowerer.cleanup_abi_policy(slice, ir::detail::CleanupDropMode::full),
        CleanupAbiPolicy::unknown_marker_only);
    EXPECT_EQ(lowerer.cleanup_abi_policy(function_type, ir::detail::CleanupDropMode::full),
        CleanupAbiPolicy::unknown_marker_only);
    EXPECT_EQ(lowerer.cleanup_abi_policy(tuple, ir::detail::CleanupDropMode::custom_destructor_only),
        CleanupAbiPolicy::unknown_marker_only);
    EXPECT_EQ(lowerer.cleanup_abi_policy(INVALID_TYPE_HANDLE, ir::detail::CleanupDropMode::full),
        CleanupAbiPolicy::unknown_marker_only);
    EXPECT_EQ(ir::cleanup_abi_policy_name(CleanupAbiPolicy::none), "none");
    EXPECT_EQ(ir::cleanup_abi_policy_name(CleanupAbiPolicy::structural_static), "structural_static");
    EXPECT_EQ(ir::cleanup_abi_policy_name(CleanupAbiPolicy::generic_marker_only), "generic_marker_only");
    EXPECT_EQ(ir::cleanup_abi_policy_name(CleanupAbiPolicy::associated_projection_marker_only),
        "associated_projection_marker_only");
    EXPECT_EQ(ir::cleanup_abi_policy_name(CleanupAbiPolicy::opaque_marker_only), "opaque_marker_only");
    EXPECT_EQ(ir::cleanup_abi_policy_name(CleanupAbiPolicy::unknown_marker_only), "unknown_marker_only");
    EXPECT_EQ(ir::cleanup_abi_policy_name(CleanupAbiPolicy::static_custom_destructor), "static_custom_destructor");
    EXPECT_EQ(ir::cleanup_abi_policy_name(static_cast<CleanupAbiPolicy>(LOWER_AST_INVALID_CLEANUP_POLICY_VALUE)),
        "invalid");
}

TEST(CoreUnit, LowerAstWhiteBoxRuntimeDropGlueExpandsStructTupleArrayAndEnumPayloads)
{
    syntax::AstModule ast;
    CheckedModule checked;

    const TypeHandle resource_type = checked.types.named_struct("RuntimeResource", "RuntimeResource", false);
    const TypeHandle struct_type = checked.types.named_struct("RuntimeBox", "RuntimeBox", false);
    const TypeHandle i32 = checked.types.builtin(BuiltinType::i32);
    const TypeHandle tuple_type = checked.types.tuple({resource_type, resource_type});
    const TypeHandle trivial_tuple_type = checked.types.tuple({i32, checked.types.builtin(BuiltinType::bool_)});
    const TypeHandle array_type = checked.types.array(2U, resource_type);
    const TypeHandle empty_array_type = checked.types.array(0U, resource_type);
    const TypeHandle enum_type = checked.types.named_enum("RuntimeChoice", "RuntimeChoice");
    const TypeHandle void_type = checked.types.builtin(BuiltinType::void_);

    checked.types.set_enum_underlying(enum_type, i32);
    checked.types.set_enum_payload_layout(enum_type, resource_type, 8U, 4U);
    add_struct_info(checked, ast, "RuntimeResource", resource_type, {});
    add_struct_info(checked, ast, "RuntimeBox", struct_type, {{"head", resource_type}, {"tail", i32}});
    static_cast<void>(add_destructor_signature(checked, ast, resource_type, "drop_runtime_resource"));
    add_enum_case_info(checked, ast, "RuntimeChoice", "some", enum_type, resource_type, "RuntimeChoice_some", "1");
    add_enum_case_info(checked, ast, "RuntimeChoice", "none", enum_type, INVALID_TYPE_HANDLE, "RuntimeChoice_none", "0");

    Lowerer lowerer(ast, checked);
    lowerer.lower_record_layouts();
    lowerer.index_enum_cases();
    static_cast<void>(add_ir_destructor_function(lowerer, resource_type, "drop_runtime_resource"));
    static_cast<void>(prepare_current_function(lowerer, "runtime_drop_shapes", void_type));

    const ValueId struct_slot = append_alloca(lowerer, "box", struct_type);
    const ValueId tuple_slot = append_alloca(lowerer, "tuple", tuple_type);
    const ValueId array_slot = append_alloca(lowerer, "array", array_type);
    const ValueId enum_slot = append_alloca(lowerer, "choice", enum_type);
    const ValueId trivial_tuple_slot = append_alloca(lowerer, "trivial_tuple", trivial_tuple_type);
    const ValueId empty_array_slot = append_alloca(lowerer, "empty_array", empty_array_type);
    const ir::IrTextId box_name = lowerer.module_.intern("box");
    const ir::IrTextId tuple_name = lowerer.module_.intern("tuple");
    const ir::IrTextId array_name = lowerer.module_.intern("array");
    const ir::IrTextId enum_name = lowerer.module_.intern("choice");
    const ir::IrTextId trivial_tuple_name = lowerer.module_.intern("trivial_tuple");
    const ir::IrTextId empty_array_name = lowerer.module_.intern("empty_array");

    EXPECT_FALSE(lowerer.append_runtime_drop_glue(
        INVALID_VALUE_ID, struct_type, box_name, ir::detail::CleanupDropMode::full));
    EXPECT_FALSE(lowerer.append_runtime_drop_glue(
        struct_slot, TypeHandle{999U}, box_name, ir::detail::CleanupDropMode::full));
    EXPECT_FALSE(lowerer.append_runtime_drop_glue(
        trivial_tuple_slot, trivial_tuple_type, trivial_tuple_name, ir::detail::CleanupDropMode::full));
    EXPECT_FALSE(lowerer.append_runtime_drop_glue(
        empty_array_slot, empty_array_type, empty_array_name, ir::detail::CleanupDropMode::full));
    EXPECT_TRUE(lowerer.append_runtime_drop_glue(
        struct_slot, struct_type, box_name, ir::detail::CleanupDropMode::full));
    EXPECT_TRUE(lowerer.append_runtime_drop_glue(
        tuple_slot, tuple_type, tuple_name, ir::detail::CleanupDropMode::full));
    EXPECT_TRUE(lowerer.append_runtime_drop_glue(
        array_slot, array_type, array_name, ir::detail::CleanupDropMode::full));
    EXPECT_TRUE(lowerer.append_runtime_drop_glue(
        enum_slot, enum_type, enum_name, ir::detail::CleanupDropMode::full));
    Terminator return_term;
    return_term.kind = TerminatorKind::return_;
    lowerer.set_terminator(lowerer.current_block_, return_term);

    const base::Result<void> verified = ir::verify_module(lowerer.module_);
    ASSERT_TRUE(verified) << verified.error().message;

    base::usize destructor_call_count = 0;
    base::usize index_addr_count = 0;
    base::usize enum_tag_load_count = 0;
    base::usize enum_payload_cast_count = 0;
    base::usize conditional_drop_branch_count = 0;
    for (const Value& value : lowerer.module_.values) {
        if (value.kind == ValueKind::call && lowerer.module_.text(value.name) == "drop_runtime_resource") {
            ++destructor_call_count;
        } else if (value.kind == ValueKind::index_addr && lowerer.module_.text(value.name) == "drop.index") {
            ++index_addr_count;
        } else if (value.kind == ValueKind::load && lowerer.module_.text(value.name) == "drop.tag") {
            ++enum_tag_load_count;
        } else if (value.kind == ValueKind::cast && lowerer.module_.types.same(value.target_type,
                   lowerer.module_.types.pointer(PointerMutability::mut, resource_type))) {
            ++enum_payload_cast_count;
        }
    }
    for (const ir::BasicBlock& block : lowerer.current_function_->blocks) {
        if (block.terminator.kind == TerminatorKind::cond_branch) {
            ++conditional_drop_branch_count;
        }
    }

    EXPECT_EQ(destructor_call_count, 6U);
    EXPECT_EQ(index_addr_count, 2U);
    EXPECT_EQ(enum_tag_load_count, 1U);
    EXPECT_GE(enum_payload_cast_count, 1U);
    EXPECT_GE(conditional_drop_branch_count, 1U);
}

TEST(CoreUnit, LowerAstWhiteBoxRootCleanupFlagRecomputesFromFieldFlags)
{
    syntax::AstModule ast;
    CheckedModule checked;

    const TypeHandle resource_type = checked.types.generic_param("Field");
    const TypeHandle box_type = checked.types.named_struct("FlagBox", "FlagBox", false);
    const TypeHandle void_type = checked.types.builtin(BuiltinType::void_);
    const sema::IdentId value_name = ast.intern_identifier("value");
    const ExprId value = push_name(ast, "value");
    const ExprId value_left = push_field(ast, value, "left");
    set_expr_type(checked, value, box_type);
    set_expr_type(checked, value_left, resource_type);

    add_struct_info(checked, ast, "FlagBox", box_type, {{"left", resource_type}, {"right", resource_type}});
    static_cast<void>(add_destructor_signature(checked, ast, box_type, "drop_flag_box"));

    Lowerer lowerer(ast, checked);
    lowerer.lower_record_layouts();
    lowerer.cleanup_scopes_.push_back({});
    static_cast<void>(prepare_current_function(lowerer, "root_flag_recompute", void_type));

    const ValueId slot = append_alloca(lowerer, "value", box_type);
    ir::detail::LocalBinding binding{
        .slot = slot,
        .cleanup_flag = INVALID_VALUE_ID,
        .type = box_type,
        .is_mutable = true,
        .field_cleanups = {},
    };
    lowerer.register_local_cleanup(binding, "value");
    lowerer.locals_.emplace(value_name, binding);

    lowerer.mark_place_initialized(value_left);

    base::usize root_flag_store_count = 0;
    base::usize root_flag_and_count = 0;
    base::usize field_flag_load_count = 0;
    for (const Value& value_node : lowerer.module_.values) {
        if (value_node.kind == ValueKind::binary && value_node.binary_op == BinaryOp::logical_and) {
            ++root_flag_and_count;
            continue;
        }
        if (value_node.kind == ValueKind::load && is_valid(value_node.object)
            && value_node.object.value < lowerer.module_.values.size()) {
            const Value& target = lowerer.module_.values[value_node.object.value];
            if (target.kind == ValueKind::alloca
                && lowerer.module_.text(target.name).find("drop.flag.value.") == 0U) {
                ++field_flag_load_count;
            }
            continue;
        }
        if (value_node.kind != ValueKind::store || !is_valid(value_node.object)
            || value_node.object.value >= lowerer.module_.values.size()) {
            continue;
        }
        const Value& target = lowerer.module_.values[value_node.object.value];
        if (target.kind == ValueKind::alloca && lowerer.module_.text(target.name) == "drop.flag.value") {
            ++root_flag_store_count;
        }
    }

    EXPECT_GE(root_flag_store_count, 2U);
    EXPECT_GE(field_flag_load_count, 2U);
    EXPECT_GE(root_flag_and_count, 1U);
    const std::string dump = ir::dump_module(lowerer.module_);
    EXPECT_NE(dump.find("and"), std::string::npos) << dump;
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

TEST(CoreUnit, LowerAstWhiteBoxCleanupStackTracksStructFieldMoveAndReinit)
{
    syntax::AstModule ast;
    CheckedModule checked;

    const TypeHandle resource_type = checked.types.generic_param("T");
    const TypeHandle box_type = checked.types.named_struct("Box", "Box", false);
    const TypeHandle void_type = checked.types.builtin(BuiltinType::void_);
    add_struct_info(checked, ast, "Box", box_type, {{"left", resource_type}, {"right", resource_type}});

    const sema::IdentId initial_name = ast.intern_identifier("initial");
    const sema::IdentId replacement_name = ast.intern_identifier("replacement");

    const ExprId initial = push_name(ast, "initial");
    const ExprId replacement = push_name(ast, "replacement");
    const ExprId slot_for_move = push_name(ast, "slot");
    const ExprId slot_left_move = push_field(ast, slot_for_move, "left");
    const ExprId slot_for_assign = push_name(ast, "slot");
    const ExprId slot_left_assign = push_field(ast, slot_for_assign, "left");
    set_expr_type(checked, initial, box_type);
    set_expr_type(checked, replacement, resource_type);
    set_expr_type(checked, slot_for_move, box_type);
    set_expr_type(checked, slot_left_move, resource_type);
    set_expr_type(checked, slot_for_assign, box_type);
    set_expr_type(checked, slot_left_assign, resource_type);
    set_expr_owned_use_mode(checked, initial, sema::OwnedUseMode::owned_consume);
    set_expr_owned_use_mode(checked, replacement, sema::OwnedUseMode::owned_consume);
    set_expr_owned_use_mode(checked, slot_left_move, sema::OwnedUseMode::owned_consume);

    const StmtId slot_decl = push_var_stmt(ast, "slot", initial, true);
    set_stmt_local_type(checked, slot_decl, box_type);
    const StmtId moved_decl = push_var_stmt(ast, "moved", slot_left_move, false);
    set_stmt_local_type(checked, moved_decl, resource_type);
    const StmtId reinit = push_assign_stmt(ast, slot_left_assign, replacement);
    const StmtId body = push_block(ast, {slot_decl, moved_decl, reinit});

    syntax::ItemNode function_item;
    function_item.kind = syntax::ItemKind::fn_decl;
    function_item.name = "cleanup_field_move";
    function_item.name_id = ast.intern_identifier("cleanup_field_move");
    function_item.params = {
        syntax::ParamDecl{"initial", syntax::INVALID_TYPE_ID, {}, initial_name},
        syntax::ParamDecl{"replacement", syntax::INVALID_TYPE_ID, {}, replacement_name},
    };
    function_item.return_type = syntax::INVALID_TYPE_ID;
    function_item.body = body;

    Lowerer lowerer(ast, checked);
    lowerer.lower_record_layouts();
    Function function = lowerer.module_.make_function();
    function.name = lowerer.module_.intern("cleanup_field_move");
    function.symbol = lowerer.module_.intern("cleanup_field_move");
    function.return_type = void_type;
    function.signature_params.push_back(ir::FunctionParam{lowerer.module_.intern("initial"), box_type});
    function.signature_params.push_back(ir::FunctionParam{lowerer.module_.intern("replacement"), resource_type});
    const FunctionId function_id = add_function(lowerer.module_, function);

    lowerer.lower_function_body(function_id, Lowerer::FunctionBodyView{function_item.params, body});

    const base::Result<void> verified = ir::verify_module(lowerer.module_);
    ASSERT_TRUE(verified) << verified.error().message;

    std::vector<std::string_view> dropped_names;
    base::usize left_flag_false_count = 0;
    base::usize left_flag_true_count = 0;
    for (const Value& value : lowerer.module_.values) {
        if (value.kind == ValueKind::drop_if) {
            ASSERT_TRUE(is_valid(value.object));
            ASSERT_LT(value.object.value, lowerer.module_.values.size());
            dropped_names.push_back(lowerer.module_.text(value.name));
        }
        if (value.kind != ValueKind::store || !is_valid(value.object) || !is_valid(value.lhs)
            || value.object.value >= lowerer.module_.values.size()
            || value.lhs.value >= lowerer.module_.values.size()) {
            continue;
        }
        const Value& target = lowerer.module_.values[value.object.value];
        const Value& source = lowerer.module_.values[value.lhs.value];
        if (target.kind != ValueKind::alloca || source.kind != ValueKind::bool_literal
            || lowerer.module_.text(target.name) != "drop.flag.slot.left") {
            continue;
        }
        if (lowerer.module_.text(source.text) == "false") {
            ++left_flag_false_count;
        } else if (lowerer.module_.text(source.text) == "true") {
            ++left_flag_true_count;
        }
    }

    EXPECT_NE(std::ranges::find(dropped_names, std::string_view{"left"}), dropped_names.end());
    EXPECT_NE(std::ranges::find(dropped_names, std::string_view{"right"}), dropped_names.end());
    EXPECT_GE(left_flag_false_count, 2U);
    EXPECT_GE(left_flag_true_count, 2U);

    const std::string dump = ir::dump_module(lowerer.module_);
    EXPECT_NE(dump.find("alloca drop.flag.slot.left"), std::string::npos) << dump;
    EXPECT_NE(dump.find("alloca drop.flag.slot.right"), std::string::npos) << dump;
    EXPECT_NE(dump.find("field_addr"), std::string::npos) << dump;
}

TEST(CoreUnit, LowerAstWhiteBoxCleanupPlaceHelpersCoverDropFlagEdges)
{
    syntax::AstModule ast;
    CheckedModule checked;

    const TypeHandle resource_type = checked.types.generic_param("T");
    const TypeHandle void_type = checked.types.builtin(BuiltinType::void_);
    const sema::IdentId slot_name = ast.intern_identifier("slot");
    const sema::IdentId missing_name = ast.intern_identifier("missing");
    const sema::IdentId left_name = ast.intern_identifier("left");
    const sema::IdentId right_name = ast.intern_identifier("right");

    const ExprId slot = push_name(ast, "slot");
    const ExprId scoped_slot = ast.push_name_expr({}, "pkg", {}, "slot");
    const ExprId missing = push_name(ast, "missing");
    const ExprId missing_left = push_field(ast, missing, "left");
    const ExprId slot_left = push_field(ast, slot, "left");
    const ExprId integer = push_integer(ast);
    set_expr_type(checked, slot, resource_type);
    set_expr_type(checked, scoped_slot, resource_type);
    set_expr_type(checked, missing, resource_type);
    set_expr_type(checked, missing_left, resource_type);
    set_expr_type(checked, slot_left, resource_type);
    set_expr_type(checked, integer, resource_type);
    set_expr_owned_use_mode(checked, slot, sema::OwnedUseMode::owned_consume);
    set_expr_owned_use_mode(checked, missing_left, sema::OwnedUseMode::owned_consume);
    set_expr_owned_use_mode(checked, slot_left, sema::OwnedUseMode::owned_consume);
    set_expr_owned_use_mode(checked, integer, sema::OwnedUseMode::owned_consume);

    Lowerer lowerer(ast, checked);
    static_cast<void>(prepare_current_function(lowerer, "cleanup_place_edges", void_type));
    lowerer.cleanup_scopes_.push_back({});

    const ValueId slot_address = append_alloca(lowerer, "slot", resource_type);
    const ir::IrTextId left_ir_name = lowerer.module_.intern("left");
    const ir::IrTextId right_ir_name = lowerer.module_.intern("right");
    const ValueId left_address =
        lowerer.append_field_address(slot_address, left_ir_name, resource_type, PointerMutability::mut);
    const ValueId right_address =
        lowerer.append_field_address(slot_address, right_ir_name, resource_type, PointerMutability::mut);
    const ValueId left_flag = lowerer.append_cleanup_flag("slot.left");
    const ValueId right_flag = lowerer.append_cleanup_flag("slot.right");

    ir::detail::CleanupBinding left_cleanup{
        .address = left_address,
        .flag = left_flag,
        .type = resource_type,
        .name = left_ir_name,
        .projections = {ir::detail::CleanupProjection{
            .kind = ir::detail::LocalPlaceProjectionKind::field,
            .field_name_id = left_name,
            .element_index = sema::SEMA_BODY_FLOW_INVALID_INDEX,
            .field_name = left_ir_name,
        }},
    };
    ir::detail::CleanupBinding right_cleanup{
        .address = right_address,
        .flag = right_flag,
        .type = resource_type,
        .name = right_ir_name,
        .projections = {ir::detail::CleanupProjection{
            .kind = ir::detail::LocalPlaceProjectionKind::field,
            .field_name_id = right_name,
            .element_index = sema::SEMA_BODY_FLOW_INVALID_INDEX,
            .field_name = right_ir_name,
        }},
    };
    lowerer.locals_.emplace(slot_name,
        ir::detail::LocalBinding{
            .slot = slot_address,
            .cleanup_flag = INVALID_VALUE_ID,
            .type = resource_type,
            .is_mutable = true,
            .field_cleanups = {left_cleanup, right_cleanup},
        });

    ASSERT_NE(lowerer.local_binding_for_name_expr(slot), nullptr);
    EXPECT_EQ(lowerer.local_binding_for_name_expr(syntax::INVALID_EXPR_ID), nullptr);
    EXPECT_EQ(lowerer.local_binding_for_name_expr(integer), nullptr);
    EXPECT_EQ(lowerer.local_binding_for_name_expr(scoped_slot), nullptr);
    EXPECT_EQ(lowerer.local_binding_for_name_expr(missing), nullptr);
    EXPECT_FALSE(lowerer.local_place_path(syntax::INVALID_EXPR_ID).has_value());
    EXPECT_FALSE(lowerer.local_place_path(ExprId{999}).has_value());
    EXPECT_FALSE(lowerer.local_place_path(scoped_slot).has_value());
    EXPECT_EQ(lowerer.local_binding_for_place_path(ir::detail::LocalPlacePath{}), nullptr);
    EXPECT_TRUE(sema::resource_needs_drop(lowerer.resource_summary(INVALID_TYPE_HANDLE)));
    EXPECT_EQ(lowerer.struct_info_for_type(INVALID_TYPE_HANDLE), nullptr);

    const std::optional<ir::detail::LocalPlacePath> field_path = lowerer.local_place_path(slot_left);
    ASSERT_TRUE(field_path.has_value());
    ASSERT_EQ(field_path->projections.size(), 1U);
    EXPECT_TRUE(lowerer.cleanup_binding_has_prefix(left_cleanup, {}));
    const std::array<ir::detail::LocalPlaceProjection, 2> longer_path{
        ir::detail::LocalPlaceProjection{
            .kind = ir::detail::LocalPlaceProjectionKind::field,
            .field_name_id = left_name,
            .element_index = sema::SEMA_BODY_FLOW_INVALID_INDEX,
            .field_name = "left",
        },
        ir::detail::LocalPlaceProjection{
            .kind = ir::detail::LocalPlaceProjectionKind::field,
            .field_name_id = right_name,
            .element_index = sema::SEMA_BODY_FLOW_INVALID_INDEX,
            .field_name = "right",
        },
    };
    EXPECT_FALSE(lowerer.cleanup_binding_has_prefix(left_cleanup, longer_path));
    EXPECT_TRUE(lowerer.cleanup_projection_matches(
        ir::detail::CleanupProjection{
            .kind = ir::detail::LocalPlaceProjectionKind::field,
            .field_name_id = sema::INVALID_IDENT_ID,
            .element_index = sema::SEMA_BODY_FLOW_INVALID_INDEX,
            .field_name = left_ir_name,
        },
        ir::detail::LocalPlaceProjection{
            .kind = ir::detail::LocalPlaceProjectionKind::field,
            .field_name_id = sema::INVALID_IDENT_ID,
            .element_index = sema::SEMA_BODY_FLOW_INVALID_INDEX,
            .field_name = "left",
        }));
    EXPECT_FALSE(lowerer.cleanup_projection_matches(
        ir::detail::CleanupProjection{
            .kind = ir::detail::LocalPlaceProjectionKind::field,
            .field_name_id = sema::INVALID_IDENT_ID,
            .element_index = sema::SEMA_BODY_FLOW_INVALID_INDEX,
            .field_name = left_ir_name,
        },
        ir::detail::LocalPlaceProjection{
            .kind = ir::detail::LocalPlaceProjectionKind::tuple_element,
            .field_name_id = sema::INVALID_IDENT_ID,
            .element_index = 0U,
            .field_name = "0",
        }));

    const base::usize values_before_invalid_drop = lowerer.module_.values.size();
    lowerer.append_cleanup_drop(INVALID_VALUE_ID, resource_type, left_ir_name);
    lowerer.append_cleanup_drop_if(INVALID_VALUE_ID, left_flag, resource_type, left_ir_name);
    EXPECT_EQ(lowerer.module_.values.size(), values_before_invalid_drop);
    lowerer.append_cleanup_drop(slot_address, resource_type, lowerer.module_.intern("slot"));

    lowerer.append_local_cleanup_drop_if(lowerer.locals_.at(slot_name));
    lowerer.append_place_cleanup_drop_if(missing);
    lowerer.append_place_cleanup_drop_if(missing_left);
    lowerer.append_place_cleanup_drop_if(slot);
    lowerer.append_place_cleanup_drop_if(slot_left);
    lowerer.mark_place_initialized(slot);
    lowerer.mark_place_initialized(missing_left);
    lowerer.mark_place_initialized(slot_left);
    lowerer.mark_local_moved(missing_name);
    lowerer.mark_expr_place_moved(integer);
    lowerer.mark_expr_place_moved(missing_left);
    lowerer.mark_expr_place_moved(slot);
    lowerer.mark_expr_place_moved(slot_left);

    base::usize drop_count = 0;
    base::usize drop_if_count = 0;
    base::usize left_false_store_count = 0;
    base::usize left_true_store_count = 0;
    for (const Value& value : lowerer.module_.values) {
        if (value.kind == ValueKind::drop) {
            ++drop_count;
            continue;
        }
        if (value.kind == ValueKind::drop_if) {
            ++drop_if_count;
            continue;
        }
        if (value.kind != ValueKind::store || !is_valid(value.object) || !is_valid(value.lhs)
            || value.object.value >= lowerer.module_.values.size()
            || value.lhs.value >= lowerer.module_.values.size()) {
            continue;
        }
        const Value& target = lowerer.module_.values[value.object.value];
        const Value& source = lowerer.module_.values[value.lhs.value];
        if (target.kind != ValueKind::alloca || source.kind != ValueKind::bool_literal
            || lowerer.module_.text(target.name) != "drop.flag.slot.left") {
            continue;
        }
        if (lowerer.module_.text(source.text) == "false") {
            ++left_false_store_count;
        } else if (lowerer.module_.text(source.text) == "true") {
            ++left_true_store_count;
        }
    }

    EXPECT_EQ(drop_count, 1U);
    EXPECT_GE(drop_if_count, 5U);
    EXPECT_GE(left_false_store_count, 3U);
    EXPECT_GE(left_true_store_count, 1U);
}

TEST(CoreUnit, LowerAstWhiteBoxTupleElementCleanupPlaceEdges)
{
    syntax::AstModule ast;
    CheckedModule checked;

    const TypeHandle resource_type = checked.types.generic_param("T");
    const TypeHandle tuple_type = checked.types.tuple({resource_type, resource_type});
    const TypeHandle void_type = checked.types.builtin(BuiltinType::void_);
    const sema::IdentId slot_name = ast.intern_identifier("slot");

    const ExprId slot = push_name(ast, "slot");
    const ExprId slot_first = push_field(ast, slot, "0");
    const ExprId slot_second = push_field(ast, slot, "1");
    const ExprId slot_named = push_field(ast, slot, "first");
    const ExprId slot_huge_index = push_field(ast, slot, "4294967296");
    set_expr_type(checked, slot, tuple_type);
    set_expr_type(checked, slot_first, resource_type);
    set_expr_type(checked, slot_second, resource_type);
    set_expr_type(checked, slot_named, resource_type);
    set_expr_type(checked, slot_huge_index, resource_type);
    set_expr_owned_use_mode(checked, slot_first, sema::OwnedUseMode::owned_consume);

    Lowerer lowerer(ast, checked);
    static_cast<void>(prepare_current_function(lowerer, "tuple_cleanup_place_edges", void_type));
    lowerer.cleanup_scopes_.push_back({});

    const ValueId tuple_slot = append_alloca(lowerer, "slot", tuple_type);
    ir::detail::LocalBinding tuple_binding{
        .slot = tuple_slot,
        .cleanup_flag = INVALID_VALUE_ID,
        .type = tuple_type,
        .is_mutable = true,
        .field_cleanups = {},
    };
    ASSERT_TRUE(lowerer.register_structured_local_cleanup(tuple_binding, "slot"));
    ASSERT_EQ(tuple_binding.field_cleanups.size(), 2U);
    ASSERT_EQ(tuple_binding.field_cleanups[0].projections.size(), 1U);
    ASSERT_EQ(tuple_binding.field_cleanups[1].projections.size(), 1U);
    EXPECT_EQ(tuple_binding.field_cleanups[0].projections[0].kind, ir::detail::LocalPlaceProjectionKind::tuple_element);
    EXPECT_EQ(tuple_binding.field_cleanups[0].projections[0].element_index, 0U);
    EXPECT_EQ(tuple_binding.field_cleanups[1].projections[0].kind, ir::detail::LocalPlaceProjectionKind::tuple_element);
    EXPECT_EQ(tuple_binding.field_cleanups[1].projections[0].element_index, 1U);
    EXPECT_EQ(lowerer.module_.text(tuple_binding.field_cleanups[0].name), "0");
    EXPECT_EQ(lowerer.module_.text(tuple_binding.field_cleanups[1].name), "1");

    lowerer.locals_.emplace(slot_name, tuple_binding);
    const std::optional<ir::detail::LocalPlacePath> first_path = lowerer.local_place_path(slot_first);
    ASSERT_TRUE(first_path.has_value());
    ASSERT_EQ(first_path->projections.size(), 1U);
    EXPECT_EQ(first_path->projections.front().kind, ir::detail::LocalPlaceProjectionKind::tuple_element);
    EXPECT_EQ(first_path->projections.front().element_index, 0U);
    EXPECT_TRUE(lowerer.cleanup_binding_has_prefix(tuple_binding.field_cleanups[0], first_path->projections));
    EXPECT_FALSE(lowerer.cleanup_binding_has_prefix(tuple_binding.field_cleanups[1], first_path->projections));

    const std::optional<ir::detail::LocalPlacePath> named_path = lowerer.local_place_path(slot_named);
    ASSERT_TRUE(named_path.has_value());
    ASSERT_EQ(named_path->projections.size(), 1U);
    EXPECT_EQ(named_path->projections.front().kind, ir::detail::LocalPlaceProjectionKind::field);
    EXPECT_EQ(named_path->projections.front().element_index, sema::SEMA_BODY_FLOW_INVALID_INDEX);
    EXPECT_EQ(named_path->projections.front().field_name, "first");

    const std::optional<ir::detail::LocalPlacePath> huge_index_path = lowerer.local_place_path(slot_huge_index);
    ASSERT_TRUE(huge_index_path.has_value());
    ASSERT_EQ(huge_index_path->projections.size(), 1U);
    EXPECT_EQ(huge_index_path->projections.front().kind, ir::detail::LocalPlaceProjectionKind::field);
    EXPECT_EQ(huge_index_path->projections.front().element_index, sema::SEMA_BODY_FLOW_INVALID_INDEX);
    EXPECT_EQ(huge_index_path->projections.front().field_name, "4294967296");

    EXPECT_FALSE(lowerer.cleanup_projection_matches(
        ir::detail::CleanupProjection{
            .kind = ir::detail::LocalPlaceProjectionKind::tuple_element,
            .field_name_id = sema::INVALID_IDENT_ID,
            .element_index = sema::SEMA_BODY_FLOW_INVALID_INDEX,
            .field_name = lowerer.module_.intern("0"),
        },
        first_path->projections.front()));
    EXPECT_FALSE(lowerer.cleanup_projection_matches(
        ir::detail::CleanupProjection{
            .kind = ir::detail::LocalPlaceProjectionKind::tuple_element,
            .field_name_id = sema::INVALID_IDENT_ID,
            .element_index = 0U,
            .field_name = lowerer.module_.intern("0"),
        },
        ir::detail::LocalPlaceProjection{
            .kind = ir::detail::LocalPlaceProjectionKind::tuple_element,
            .field_name_id = sema::INVALID_IDENT_ID,
            .element_index = sema::SEMA_BODY_FLOW_INVALID_INDEX,
            .field_name = "0",
        }));

    tuple_binding.cleanup_flag = lowerer.append_cleanup_flag("slot.root");
    lowerer.append_local_cleanup_drop_if(tuple_binding);

    lowerer.mark_expr_place_moved(slot_first);
    lowerer.mark_place_initialized(slot_first);
    lowerer.append_place_cleanup_drop_if(slot_second);

    base::usize first_false_store_count = 0;
    base::usize first_true_store_count = 0;
    base::usize second_drop_count = 0;
    for (const Value& value : lowerer.module_.values) {
        if (value.kind == ValueKind::drop_if && lowerer.module_.text(value.name) == "1") {
            ++second_drop_count;
            continue;
        }
        if (value.kind != ValueKind::store || !is_valid(value.object) || !is_valid(value.lhs)
            || value.object.value >= lowerer.module_.values.size()
            || value.lhs.value >= lowerer.module_.values.size()) {
            continue;
        }
        const Value& target = lowerer.module_.values[value.object.value];
        const Value& source = lowerer.module_.values[value.lhs.value];
        if (target.kind != ValueKind::alloca || source.kind != ValueKind::bool_literal
            || lowerer.module_.text(target.name) != "drop.flag.slot.0") {
            continue;
        }
        if (lowerer.module_.text(source.text) == "false") {
            ++first_false_store_count;
        } else if (lowerer.module_.text(source.text) == "true") {
            ++first_true_store_count;
        }
    }
    EXPECT_GT(first_false_store_count, 0U);
    EXPECT_GT(first_true_store_count, 1U);
    EXPECT_GT(second_drop_count, 0U);
}

TEST(CoreUnit, LowerAstWhiteBoxStructuredCleanupRegistrationCoversNestedAndTrivialFields)
{
    syntax::AstModule ast;
    CheckedModule checked;

    const TypeHandle i32 = checked.types.builtin(BuiltinType::i32);
    const TypeHandle resource_type = checked.types.generic_param("T");
    const TypeHandle inner_type = checked.types.named_struct("Inner", "Inner", false);
    const TypeHandle outer_type = checked.types.named_struct("Outer", "Outer", false);
    const TypeHandle plain_type = checked.types.named_struct("Plain", "Plain", false);
    const TypeHandle tuple_type = checked.types.tuple({i32, inner_type});
    const TypeHandle void_type = checked.types.builtin(BuiltinType::void_);
    add_struct_info(checked, ast, "Inner", inner_type, {{"value", resource_type}});
    add_struct_info(checked, ast, "Outer", outer_type, {{"owned", inner_type}, {"trivial", i32}});
    add_struct_info(checked, ast, "Plain", plain_type, {{"number", i32}});

    Lowerer lowerer(ast, checked);
    static_cast<void>(prepare_current_function(lowerer, "cleanup_registration_edges", void_type));
    lowerer.cleanup_scopes_.push_back({});
    const ValueId outer_slot = append_alloca(lowerer, "outer", outer_type);
    ir::detail::LocalBinding outer_binding{
        .slot = outer_slot,
        .cleanup_flag = INVALID_VALUE_ID,
        .type = outer_type,
        .is_mutable = true,
        .field_cleanups = {},
    };

    ASSERT_TRUE(lowerer.register_structured_local_cleanup(outer_binding, "outer"));
    ASSERT_EQ(outer_binding.field_cleanups.size(), 1U);
    ASSERT_EQ(outer_binding.field_cleanups.front().projections.size(), 2U);
    EXPECT_EQ(lowerer.module_.text(outer_binding.field_cleanups.front().projections[0].field_name), "owned");
    EXPECT_EQ(lowerer.module_.text(outer_binding.field_cleanups.front().projections[1].field_name), "value");

    const ValueId tuple_slot = append_alloca(lowerer, "tuple", tuple_type);
    ir::detail::LocalBinding tuple_binding{
        .slot = tuple_slot,
        .cleanup_flag = INVALID_VALUE_ID,
        .type = tuple_type,
        .is_mutable = true,
        .field_cleanups = {},
    };
    ASSERT_TRUE(lowerer.register_structured_local_cleanup(tuple_binding, "tuple"));
    ASSERT_EQ(tuple_binding.field_cleanups.size(), 1U);
    ASSERT_EQ(tuple_binding.field_cleanups.front().projections.size(), 2U);
    EXPECT_EQ(tuple_binding.field_cleanups.front().projections[0].kind,
        ir::detail::LocalPlaceProjectionKind::tuple_element);
    EXPECT_EQ(tuple_binding.field_cleanups.front().projections[0].element_index, 1U);
    EXPECT_EQ(tuple_binding.field_cleanups.front().projections[1].kind, ir::detail::LocalPlaceProjectionKind::field);
    EXPECT_EQ(lowerer.module_.text(tuple_binding.field_cleanups.front().projections[1].field_name), "value");

    const std::vector<ir::detail::CleanupProjection> no_cleanup_projections;
    EXPECT_FALSE(lowerer.append_structured_cleanup_bindings(
        tuple_binding, INVALID_VALUE_ID, tuple_type, no_cleanup_projections, "invalid.address"));
    EXPECT_FALSE(lowerer.append_structured_cleanup_bindings(
        tuple_binding, tuple_slot, INVALID_TYPE_HANDLE, no_cleanup_projections, "invalid.type"));

    std::vector<ir::detail::StructuredCleanupFrame> pending_cleanup_frames;
    EXPECT_FALSE(lowerer.push_structured_cleanup_children(
        ir::detail::StructuredCleanupFrame{
            .address = tuple_slot,
            .type = INVALID_TYPE_HANDLE,
            .flag_name = "invalid.type",
            .name = INVALID_IR_TEXT_ID,
            .projections = {},
        },
        pending_cleanup_frames));
    EXPECT_TRUE(pending_cleanup_frames.empty());
    EXPECT_FALSE(lowerer.push_structured_cleanup_children(
        ir::detail::StructuredCleanupFrame{
            .address = INVALID_VALUE_ID,
            .type = tuple_type,
            .flag_name = "invalid.tuple.address",
            .name = INVALID_IR_TEXT_ID,
            .projections = {},
        },
        pending_cleanup_frames));
    EXPECT_TRUE(pending_cleanup_frames.empty());
    EXPECT_FALSE(lowerer.push_structured_cleanup_children(
        ir::detail::StructuredCleanupFrame{
            .address = INVALID_VALUE_ID,
            .type = inner_type,
            .flag_name = "invalid.struct.address",
            .name = INVALID_IR_TEXT_ID,
            .projections = {},
        },
        pending_cleanup_frames));
    EXPECT_TRUE(pending_cleanup_frames.empty());

    const ValueId plain_slot = append_alloca(lowerer, "plain", plain_type);
    ir::detail::LocalBinding plain_binding{
        .slot = plain_slot,
        .cleanup_flag = INVALID_VALUE_ID,
        .type = plain_type,
        .is_mutable = true,
        .field_cleanups = {},
    };
    EXPECT_FALSE(lowerer.register_structured_local_cleanup(plain_binding, "plain"));
    EXPECT_TRUE(plain_binding.field_cleanups.empty());

    ir::detail::LocalBinding invalid_slot_binding{
        .slot = INVALID_VALUE_ID,
        .cleanup_flag = INVALID_VALUE_ID,
        .type = outer_type,
        .is_mutable = true,
        .field_cleanups = {},
    };
    EXPECT_FALSE(lowerer.register_structured_local_cleanup(invalid_slot_binding, "invalid"));
    EXPECT_EQ(
        lowerer
            .append_field_address(INVALID_VALUE_ID, lowerer.module_.intern("owned"), inner_type, PointerMutability::mut)
            .value,
        INVALID_VALUE_ID.value);
    EXPECT_EQ(lowerer
                  .append_field_address(
                      outer_slot, lowerer.module_.intern("owned"), INVALID_TYPE_HANDLE, PointerMutability::mut)
                  .value,
        INVALID_VALUE_ID.value);

    Lowerer empty_scope_lowerer(ast, checked);
    const ValueId detached_slot = append_alloca(empty_scope_lowerer, "detached", resource_type);
    ir::detail::LocalBinding detached_binding{
        .slot = detached_slot,
        .cleanup_flag = INVALID_VALUE_ID,
        .type = resource_type,
        .is_mutable = true,
        .field_cleanups = {},
    };
    empty_scope_lowerer.register_local_cleanup(detached_binding, "detached");
    EXPECT_FALSE(is_valid(detached_binding.cleanup_flag));
    EXPECT_TRUE(detached_binding.field_cleanups.empty());
    EXPECT_TRUE(empty_scope_lowerer.cleanup_scopes_.empty());
}

TEST(CoreUnit, LowerAstWhiteBoxAggregateRollbackHelperGuardsAndCleanupScope)
{
    syntax::AstModule ast;
    CheckedModule checked;

    const TypeHandle i32 = checked.types.builtin(BuiltinType::i32);
    const TypeHandle resource_type = checked.types.generic_param("RollbackResource");
    const TypeHandle record_type = checked.types.named_struct("RollbackRecord", "RollbackRecord", false);
    const TypeHandle resource_array_type = checked.types.array(2U, resource_type);
    const TypeHandle scalar_array_type = checked.types.array(2U, i32);
    const TypeHandle void_type = checked.types.builtin(BuiltinType::void_);
    add_struct_info(checked, ast, "RollbackRecord", record_type, {{"plain", i32}, {"owned", resource_type}});

    const ExprId plain = push_integer(ast, "11");
    const ExprId other_plain = push_integer(ast, "12");
    const ExprId owned = push_name(ast, "owned_source");
    set_expr_type(checked, plain, i32);
    set_expr_type(checked, other_plain, i32);
    set_expr_type(checked, owned, resource_type);

    Lowerer lowerer(ast, checked);
    lowerer.lower_record_layouts();

    const std::array<ir::detail::AggregateElementInit, 2> record_elements{{
        {lowerer.module_.intern("plain"), plain, i32},
        {lowerer.module_.intern("owned"), owned, resource_type},
    }};
    const std::array<ir::detail::AggregateElementInit, 2> scalar_array_elements{{
        {INVALID_IR_TEXT_ID, plain, i32},
        {INVALID_IR_TEXT_ID, other_plain, i32},
    }};
    const std::array<ir::detail::AggregateElementInit, 2> resource_array_elements{{
        {INVALID_IR_TEXT_ID, owned, resource_type},
        {INVALID_IR_TEXT_ID, owned, resource_type},
    }};

    EXPECT_FALSE(lowerer.aggregate_needs_rollback(record_type, record_elements));
    EXPECT_FALSE(lowerer.aggregate_needs_rollback(record_type, std::span<const ir::detail::AggregateElementInit>{}));

    const ValueId invalid_record =
        lowerer.lower_record_aggregate_with_rollback(INVALID_TYPE_HANDLE, record_elements, "invalid.record");
    ASSERT_TRUE(is_valid(invalid_record));
    ASSERT_LT(invalid_record.value, lowerer.module_.values.size());
    EXPECT_EQ(lowerer.module_.values[invalid_record.value].kind, ValueKind::aggregate);
    EXPECT_FALSE(sema::is_valid(lowerer.module_.values[invalid_record.value].type));

    static_cast<void>(prepare_current_function(lowerer, "aggregate_rollback_edges", void_type));
    const ValueId owned_slot = append_alloca(lowerer, "owned_source", resource_type);
    lowerer.locals_.emplace(ast.find_identifier("owned_source"),
        ir::detail::LocalBinding{
            .slot = owned_slot,
            .cleanup_flag = INVALID_VALUE_ID,
            .type = resource_type,
            .is_mutable = false,
            .field_cleanups = {},
        });

    EXPECT_TRUE(lowerer.aggregate_needs_rollback(record_type, record_elements));
    EXPECT_FALSE(lowerer.aggregate_needs_rollback(INVALID_TYPE_HANDLE, record_elements));
    EXPECT_FALSE(lowerer.aggregate_needs_rollback(scalar_array_type, scalar_array_elements));
    lowerer.lowering_constant_initializer_ = true;
    EXPECT_FALSE(lowerer.aggregate_needs_rollback(record_type, record_elements));
    lowerer.lowering_constant_initializer_ = false;

    const base::usize no_scope_first_value = lowerer.module_.values.size();
    const ValueId no_scope_result =
        lowerer.lower_record_aggregate_with_rollback(record_type, record_elements, "record.no.scope");
    EXPECT_TRUE(is_valid(no_scope_result));
    EXPECT_EQ(count_named_values(lowerer, ValueKind::alloca, LOWER_AST_ROLLBACK_FLAG_ALLOCA, no_scope_first_value), 0U);

    lowerer.cleanup_scopes_.push_back({});
    const base::usize scoped_first_value = lowerer.module_.values.size();
    const ValueId scoped_record =
        lowerer.lower_record_aggregate_with_rollback(record_type, record_elements, "record.with.scope");
    EXPECT_TRUE(is_valid(scoped_record));
    ASSERT_FALSE(lowerer.cleanup_scopes_.empty());
    EXPECT_TRUE(lowerer.cleanup_scopes_.back().empty());
    EXPECT_EQ(count_named_values(lowerer, ValueKind::alloca, LOWER_AST_ROLLBACK_FLAG_ALLOCA, scoped_first_value), 1U);

    const ValueId fallback_array =
        lowerer.lower_array_aggregate_with_rollback(record_type, record_elements, "array.fallback");
    EXPECT_TRUE(is_valid(fallback_array));
    EXPECT_TRUE(lowerer.cleanup_scopes_.back().empty());

    const base::usize scalar_array_first_value = lowerer.module_.values.size();
    const ValueId scalar_array =
        lowerer.lower_array_aggregate_with_rollback(scalar_array_type, scalar_array_elements, "array.scalar");
    EXPECT_TRUE(is_valid(scalar_array));
    EXPECT_EQ(
        count_named_values(lowerer, ValueKind::alloca, LOWER_AST_ROLLBACK_FLAG_ALLOCA, scalar_array_first_value), 0U);

    const base::usize resource_array_first_value = lowerer.module_.values.size();
    const ValueId resource_array =
        lowerer.lower_array_aggregate_with_rollback(resource_array_type, resource_array_elements, "array.resource");
    EXPECT_TRUE(is_valid(resource_array));
    EXPECT_EQ(
        count_named_values(lowerer, ValueKind::index_addr, LOWER_AST_ROLLBACK_ARRAY_FIELD_PREFIX, resource_array_first_value),
        0U);
    EXPECT_EQ(count_named_values(lowerer, ValueKind::index_addr, "aggregate.rollback.0", resource_array_first_value), 1U);
    EXPECT_EQ(count_named_values(lowerer, ValueKind::index_addr, "aggregate.rollback.1", resource_array_first_value), 1U);
    EXPECT_TRUE(lowerer.cleanup_scopes_.back().empty());

    Terminator ret;
    ret.kind = TerminatorKind::return_;
    lowerer.set_terminator(lowerer.current_block_, ret);
    EXPECT_FALSE(lowerer.aggregate_needs_rollback(record_type, record_elements));
    const ValueId terminated_record =
        lowerer.lower_record_aggregate_with_rollback(record_type, record_elements, "record.terminated");
    EXPECT_EQ(terminated_record.value, INVALID_VALUE_ID.value);
    EXPECT_TRUE(lowerer.cleanup_scopes_.back().empty());
}

TEST(CoreUnit, LowerAstWhiteBoxStatementGuardsAndFallbackCleanupActions)
{
    syntax::AstModule ast;
    CheckedModule checked;

    syntax::TypeNode i32_type_node;
    i32_type_node.kind = syntax::TypeKind::primitive;
    i32_type_node.primitive = syntax::PrimitiveTypeKind::i32;
    const TypeId i32_type_id = ast.push_type(i32_type_node);

    const TypeHandle i32 = checked.types.builtin(BuiltinType::i32);
    const TypeHandle resource_type = checked.types.generic_param("T");
    const TypeHandle void_type = checked.types.builtin(BuiltinType::void_);
    checked.syntax_type_handles.resize(i32_type_id.value + 1, INVALID_TYPE_HANDLE);
    checked.syntax_type_handles[i32_type_id.value] = i32;

    const ExprId marker = push_integer(ast, "9");
    set_expr_type(checked, marker, i32);
    syntax::StmtNode expr_stmt;
    expr_stmt.kind = syntax::StmtKind::expr;
    expr_stmt.init = marker;
    const StmtId expr_stmt_id = ast.push_stmt(expr_stmt);
    const StmtId block = push_block(ast, {expr_stmt_id});

    syntax::ItemNode fallback_function_item;
    fallback_function_item.kind = syntax::ItemKind::fn_decl;
    fallback_function_item.name = "fallback_param";
    fallback_function_item.name_id = ast.intern_identifier("fallback_param");
    fallback_function_item.params = {syntax::ParamDecl{"value", i32_type_id, {}, ast.intern_identifier("value")}};
    fallback_function_item.return_type = syntax::INVALID_TYPE_ID;
    fallback_function_item.body = block;

    Lowerer fallback_lowerer(ast, checked);
    Function fallback_function = fallback_lowerer.module_.make_function();
    fallback_function.name = fallback_lowerer.module_.intern("fallback_param");
    fallback_function.symbol = fallback_lowerer.module_.intern("fallback_param");
    fallback_function.return_type = void_type;
    const FunctionId fallback_function_id = add_function(fallback_lowerer.module_, fallback_function);
    fallback_lowerer.lower_function_body(
        fallback_function_id, Lowerer::FunctionBodyView{fallback_function_item.params, block});
    ASSERT_EQ(fallback_lowerer.module_.functions[fallback_function_id.value].param_values.size(), 1U);
    const ValueId fallback_param = fallback_lowerer.module_.functions[fallback_function_id.value].param_values.front();
    ASSERT_LT(fallback_param.value, fallback_lowerer.module_.values.size());
    EXPECT_TRUE(fallback_lowerer.module_.types.same(fallback_lowerer.module_.values[fallback_param.value].type, i32));

    Lowerer lowerer(ast, checked);
    static_cast<void>(prepare_current_function(lowerer, "statement_guard_edges", void_type));
    lowerer.lower_block_contents(syntax::INVALID_STMT_ID);
    lowerer.lower_block_contents(StmtId{999});
    lowerer.lower_block_contents(expr_stmt_id);
    lowerer.lower_stmt(syntax::INVALID_STMT_ID);
    lowerer.lower_stmt(StmtId{999});

    Terminator ret;
    ret.kind = TerminatorKind::return_;
    lowerer.set_terminator(lowerer.current_block_, ret);
    lowerer.lower_block_contents(block);

    syntax::StmtNode break_stmt;
    break_stmt.kind = syntax::StmtKind::break_;
    const StmtId break_stmt_id = ast.push_stmt(break_stmt);
    lowerer.current_block_ = add_block(lowerer.module_, *lowerer.current_function_, "break.guard");
    lowerer.lower_stmt(break_stmt_id);
    ASSERT_EQ(lowerer.current_function_->blocks[lowerer.current_block_.value].terminator.kind, TerminatorKind::branch);
    EXPECT_EQ(lowerer.current_function_->blocks[lowerer.current_block_.value].terminator.target.value,
        INVALID_BLOCK_ID.value);

    syntax::StmtNode continue_stmt;
    continue_stmt.kind = syntax::StmtKind::continue_;
    const StmtId continue_stmt_id = ast.push_stmt(continue_stmt);
    lowerer.current_block_ = add_block(lowerer.module_, *lowerer.current_function_, "continue.guard");
    lowerer.lower_stmt(continue_stmt_id);
    ASSERT_EQ(lowerer.current_function_->blocks[lowerer.current_block_.value].terminator.kind, TerminatorKind::branch);
    EXPECT_EQ(lowerer.current_function_->blocks[lowerer.current_block_.value].terminator.target.value,
        INVALID_BLOCK_ID.value);

    const StmtId defer_stmt = push_defer_stmt(ast, marker);
    lowerer.cleanup_scopes_.clear();
    lowerer.current_block_ = add_block(lowerer.module_, *lowerer.current_function_, "defer.guard");
    lowerer.lower_stmt(defer_stmt);
    EXPECT_TRUE(lowerer.cleanup_scopes_.empty());

    const ValueId resource_slot = append_alloca(lowerer, "resource", resource_type);
    lowerer.cleanup_scopes_.push_back({ir::detail::CleanupAction{
        .kind = ir::detail::CleanupActionKind::drop_local,
        .slot = resource_slot,
        .flag = INVALID_VALUE_ID,
        .type = resource_type,
        .defer_expr = syntax::INVALID_EXPR_ID,
        .name = lowerer.module_.intern("resource"),
    }});
    lowerer.emit_cleanup_scopes(0);

    EXPECT_NE(std::ranges::find_if(lowerer.module_.values,
                  [](const Value& value) noexcept {
                      return value.kind == ValueKind::drop;
                  }),
        lowerer.module_.values.end());
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
