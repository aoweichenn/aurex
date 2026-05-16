#include <array>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
#define private public
#include <aurex/sema/sema.hpp>
#undef private
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

using base::u32;
using sema::BuiltinType;
using sema::EnumCaseInfo;
using sema::FunctionSignature;
using sema::IdentId;
using sema::IdentifierInterner;
using sema::PointerMutability;
using sema::StructFieldInfo;
using sema::StructInfo;
using sema::Symbol;
using sema::SymbolKind;
using sema::TypeHandle;
using sema::TypeKind;
using sema::INVALID_TYPE_HANDLE;
using sema::is_valid;
using syntax::ExprId;
using syntax::ModuleId;
using syntax::TypeId;

constexpr u32 SEMA_TEST_ROOT_MODULE_INDEX = 0;
constexpr u32 SEMA_TEST_LIB_ONE_MODULE_INDEX = 1;
constexpr u32 SEMA_TEST_MISSING_MODULE_INDEX = 99;
constexpr base::u32 SEMA_TEST_PATTERN_FIRST_INDEX = 0;
constexpr base::u32 SEMA_TEST_PATTERN_SECOND_INDEX = 1;
constexpr base::u32 SEMA_TEST_PATTERN_TRACKED_COUNT = 3;
constexpr base::u64 SEMA_TEST_ABI_INVALID_SIZE = 0;
constexpr base::u64 SEMA_TEST_ABI_MIN_ALIGNMENT = 1;
constexpr base::u64 SEMA_TEST_RECORD_ABI_SIZE = 16;
constexpr base::u64 SEMA_TEST_ZERO_ALIGN_ENUM_PAYLOAD_SIZE = 1;
constexpr base::u64 SEMA_TEST_ZERO_ALIGN_ENUM_PAYLOAD_ALIGN = 0;
constexpr base::u64 SEMA_TEST_NESTED_ARRAY_COUNT = 5;
constexpr base::u64 SEMA_TEST_INVALID_ARRAY_COUNT = 2;
constexpr base::u64 SEMA_TEST_SMALL_ARRAY_COUNT = 3;
constexpr base::u64 SEMA_TEST_LAYOUT_MAX_ARRAY_COUNT = std::numeric_limits<base::u64>::max();
constexpr base::usize SEMA_TEST_LARGE_GENERIC_SPAN_EXPR_COUNT = 70;
constexpr std::string_view SEMA_TEST_INTEGER_LITERAL_ONE = "1";
constexpr std::string_view SEMA_TEST_IMPORT_ALIAS_ONE = "one";
constexpr std::string_view SEMA_TEST_CONST_VALUE_NAME = "VALUE";
constexpr std::string_view SEMA_TEST_LOCAL_VALUE_NAME = "LOCAL_VALUE";
constexpr std::string_view SEMA_TEST_ENUM_VALUE_NAME = "ENUM_VALUE";
constexpr std::string_view SEMA_TEST_MISSING_VALUE_NAME = "MISSING_VALUE";
constexpr std::string_view SEMA_TEST_ENUM_CASE_C_NAME = "Choice_some";
constexpr std::string_view SEMA_TEST_INTEGER_LITERAL_HEX_UPPER = "0X2A";
constexpr std::string_view SEMA_TEST_INTEGER_LITERAL_HEX_LOWER = "0xa";
constexpr std::string_view SEMA_TEST_INTEGER_LITERAL_BIN_LOWER = "0b1010";
constexpr std::string_view SEMA_TEST_INTEGER_LITERAL_BIN_UPPER = "0B1010";
constexpr std::string_view SEMA_TEST_INTEGER_LITERAL_INVALID_BINARY = "0b2";
constexpr std::string_view SEMA_TEST_INTEGER_LITERAL_INVALID_CHAR = "12g";
constexpr std::string_view SEMA_TEST_INTEGER_LITERAL_EMPTY = "";
constexpr std::string_view SEMA_TEST_INTEGER_LITERAL_UNSIGNED_OVERFLOW = "18446744073709551616";
constexpr std::string_view SEMA_TEST_INTEGER_LITERAL_SIGNED_OVERFLOW = "9223372036854775808";
constexpr std::string_view SEMA_TEST_INTEGER_LITERAL_U8_SUFFIX = "255u8";
constexpr std::string_view SEMA_TEST_INTEGER_LITERAL_I8_SUFFIX = "127i8";
constexpr std::string_view SEMA_TEST_INTEGER_LITERAL_INVALID_SUFFIX = "1f32";
constexpr syntax::PrimitiveTypeKind SEMA_TEST_INVALID_PRIMITIVE_KIND = static_cast<syntax::PrimitiveTypeKind>(99);
constexpr std::string_view SEMA_TEST_FLOAT_OVERFLOW_LITERAL = "1e999999999";
constexpr std::string_view SEMA_TEST_FLOAT_WITH_SEPARATOR_LITERAL = "1_2.5";
constexpr std::string_view SEMA_TEST_FLOAT_INVALID_TRAILING_LITERAL = "1.0x";
constexpr std::string_view SEMA_TEST_FLOAT_LITERAL_F32_SUFFIX = ".5f32";
constexpr std::string_view SEMA_TEST_FLOAT_LITERAL_INVALID_SUFFIX = "1.0u8";
constexpr syntax::BinaryOp SEMA_TEST_INVALID_BINARY_OP = static_cast<syntax::BinaryOp>(99);
constexpr sema::BuiltinType SEMA_TEST_INVALID_BUILTIN_TYPE = static_cast<sema::BuiltinType>(99);
constexpr sema::TypeKind SEMA_TEST_INVALID_TYPE_KIND = static_cast<sema::TypeKind>(99);
constexpr std::string_view SEMA_TEST_INVALID_TYPE_DISPLAY = "<invalid>";
constexpr std::string_view SEMA_TEST_UNKNOWN_TYPE_DISPLAY = "<unknown>";
constexpr std::string_view SEMA_TEST_SYMBOL_OUTER_NAME = "outer_value";
constexpr std::string_view SEMA_TEST_SYMBOL_INNER_NAME = "inner_value";
constexpr std::string_view SEMA_TEST_SYMBOL_DUPLICATE_NAME = "duplicate_value";
constexpr std::string_view SEMA_TEST_ROOT_MODULE_NAME = "core";
constexpr std::string_view SEMA_TEST_CHILD_MODULE_NAME = "mem";
constexpr std::string_view SEMA_TEST_LEAF_MODULE_NAME = "io";

[[nodiscard]] ModuleId module_id(const u32 value) noexcept {
    return ModuleId {value};
}

[[nodiscard]] syntax::ModulePath module_path(const std::initializer_list<std::string_view> parts) {
    syntax::ModulePath path;
    path.parts.assign(parts.begin(), parts.end());
    return path;
}

[[nodiscard]] syntax::ModuleInfo module_info(const std::initializer_list<std::string_view> parts) {
    syntax::ModuleInfo info;
    info.path = module_path(parts);
    return info;
}

[[nodiscard]] syntax::ResolvedImport resolved_import(
    const ModuleId module,
    const std::string_view alias,
    const syntax::Visibility visibility = syntax::Visibility::private_
) {
    syntax::ResolvedImport import;
    import.module = module;
    import.alias = alias;
    import.visibility = visibility;
    return import;
}

[[nodiscard]] syntax::TypeNode primitive_node(const syntax::PrimitiveTypeKind kind) {
    syntax::TypeNode node;
    node.kind = syntax::TypeKind::primitive;
    node.primitive = kind;
    return node;
}

[[nodiscard]] syntax::TypeNode named_node(const std::string_view name) {
    syntax::TypeNode node;
    node.kind = syntax::TypeKind::named;
    node.name = name;
    return node;
}

[[nodiscard]] FunctionSignature function_signature(
    const std::string_view name,
    const ModuleId module,
    const TypeHandle return_type,
    const IdentId name_id = syntax::INVALID_IDENT_ID
) {
    FunctionSignature signature;
    signature.name = std::string(name);
    signature.name_id = name_id;
    signature.c_name = std::string(name);
    signature.module = module;
    signature.return_type = return_type;
    return signature;
}

[[nodiscard]] Symbol symbol(
    const SymbolKind kind,
    const std::string_view name,
    const ModuleId module,
    const TypeHandle type,
    const bool is_mutable = false,
    const syntax::Visibility visibility = syntax::Visibility::public_,
    const IdentId name_id = syntax::INVALID_IDENT_ID
) {
    return Symbol {
        kind,
        std::string(name),
        name_id,
        std::string(name),
        module,
        type,
        {},
        is_mutable,
        visibility,
    };
}

[[nodiscard]] IdentId intern_identifier(
    sema::SemanticAnalyzer& analyzer,
    const std::string_view name
) {
    return analyzer.module_.intern_identifier(name);
}

[[nodiscard]] sema::ModuleLookupKey semantic_module_key(
    sema::SemanticAnalyzer& analyzer,
    const ModuleId module,
    const std::string_view name
) {
    return analyzer.module_lookup_key(module, intern_identifier(analyzer, name));
}

[[nodiscard]] sema::FunctionLookupKey semantic_function_key(
    sema::SemanticAnalyzer& analyzer,
    const ModuleId module,
    const std::string_view name
) {
    return analyzer.function_lookup_key(module, intern_identifier(analyzer, name));
}

[[nodiscard]] sema::FunctionLookupKey semantic_method_key(
    sema::SemanticAnalyzer& analyzer,
    const ModuleId module,
    const TypeHandle owner_type,
    const std::string_view name
) {
    return analyzer.method_function_lookup_key(module, owner_type, intern_identifier(analyzer, name));
}

[[nodiscard]] FunctionSignature indexed_function_signature(
    sema::SemanticAnalyzer& analyzer,
    const std::string_view name,
    const ModuleId module,
    const TypeHandle return_type
) {
    return function_signature(name, module, return_type, intern_identifier(analyzer, name));
}

[[nodiscard]] Symbol indexed_symbol(
    sema::SemanticAnalyzer& analyzer,
    const SymbolKind kind,
    const std::string_view name,
    const ModuleId module,
    const TypeHandle type,
    const bool is_mutable = false,
    const syntax::Visibility visibility = syntax::Visibility::public_
) {
    return symbol(kind, name, module, type, is_mutable, visibility, intern_identifier(analyzer, name));
}

[[nodiscard]] sema::ModuleLookupKey add_named_type(
    sema::SemanticAnalyzer& analyzer,
    const ModuleId module,
    const std::string_view name,
    const TypeHandle type,
    const syntax::Visibility visibility = syntax::Visibility::public_
) {
    const IdentId name_id = intern_identifier(analyzer, name);
    const sema::ModuleLookupKey key = analyzer.module_lookup_key(module, name_id);
    analyzer.named_types_.emplace(key, type);
    analyzer.index_named_type(module, name_id, type, visibility);
    return key;
}

[[nodiscard]] sema::FunctionLookupKey add_function(
    sema::SemanticAnalyzer& analyzer,
    FunctionSignature signature
) {
    if (!is_valid(signature.name_id)) {
        signature.name_id = intern_identifier(analyzer, signature.name);
    }
    const sema::FunctionLookupKey key = analyzer.function_lookup_key(signature.module, signature.name_id);
    signature.semantic_key = key;
    const auto inserted = analyzer.checked_.functions.emplace(key, std::move(signature));
    analyzer.index_function_lookup(inserted.first->second);
    return inserted.first->first;
}

[[nodiscard]] sema::FunctionLookupKey add_method(
    sema::SemanticAnalyzer& analyzer,
    FunctionSignature signature,
    const TypeHandle owner_type
) {
    if (!is_valid(signature.name_id)) {
        signature.name_id = intern_identifier(analyzer, signature.name);
    }
    signature.is_method = true;
    signature.method_owner_type = owner_type;
    const sema::FunctionLookupKey key = analyzer.method_function_lookup_key(signature.module, owner_type, signature.name_id);
    signature.semantic_key = key;
    const auto inserted = analyzer.checked_.functions.emplace(key, std::move(signature));
    analyzer.index_function_lookup(inserted.first->second);
    return inserted.first->first;
}

[[nodiscard]] std::pair<sema::FunctionLookupKey, const Symbol*> add_global_value(
    sema::SemanticAnalyzer& analyzer,
    const ModuleId module,
    const std::string_view name,
    const TypeHandle type,
    const SymbolKind kind = SymbolKind::const_,
    const bool is_mutable = false,
    const syntax::Visibility visibility = syntax::Visibility::public_
) {
    const IdentId name_id = intern_identifier(analyzer, name);
    const sema::FunctionLookupKey key = analyzer.function_lookup_key(module, name_id);
    auto inserted = analyzer.global_values_.emplace(
        key,
        symbol(kind, name, module, type, is_mutable, visibility, name_id)
    );
    analyzer.index_global_value(inserted.first->second);
    return {inserted.first->first, &inserted.first->second};
}

[[nodiscard]] sema::SemanticAnalyzer::GenericTemplateInfo generic_template_info(
    sema::SemanticAnalyzer& analyzer,
    const ModuleId module,
    const std::string_view name,
    const syntax::Visibility visibility = syntax::Visibility::public_
) {
    sema::SemanticAnalyzer::GenericTemplateInfo info = analyzer.make_generic_template_info();
    info.module = module;
    info.name = std::string(name);
    info.name_id = intern_identifier(analyzer, name);
    info.key = analyzer.module_lookup_key(module, info.name_id);
    info.function_key = analyzer.function_lookup_key(module, info.name_id);
    info.params = {intern_identifier(analyzer, "T")};
    info.visibility = visibility;
    return info;
}

[[nodiscard]] std::pair<sema::ModuleLookupKey, const EnumCaseInfo*> add_enum_case(
    sema::SemanticAnalyzer& analyzer,
    const ModuleId module,
    const std::string_view name,
    const std::string_view case_name,
    const TypeHandle enum_type,
    const TypeHandle payload_type = INVALID_TYPE_HANDLE,
    std::vector<TypeHandle> payload_types = {}
) {
    EnumCaseInfo info = analyzer.checked_.make_enum_case_info();
    info.name = std::string(name);
    info.name_id = intern_identifier(analyzer, name);
    info.c_name = std::string(name);
    info.module = module;
    info.type = enum_type;
    info.payload_type = payload_type;
    info.payload_types = analyzer.checked_.copy_type_handle_list(payload_types);
    info.case_name = std::string(case_name);
    info.case_name_id = intern_identifier(analyzer, case_name);

    const sema::ModuleLookupKey key = analyzer.module_lookup_key(module, info.name_id);
    auto inserted = analyzer.checked_.enum_cases.emplace(key, std::move(info));
    analyzer.index_enum_case(inserted.first->second);
    return {inserted.first->first, &inserted.first->second};
}

[[nodiscard]] ExprId push_name(
    syntax::AstModule& module,
    const std::string_view text,
    const std::string_view scope = {}
) {
    syntax::NameExprPayload payload;
    payload.text = text;
    payload.scope_name = scope;
    return module.push_name_expr({}, std::move(payload));
}

[[nodiscard]] ExprId push_field(
    syntax::AstModule& module,
    const ExprId object,
    const std::string_view field_name
) {
    syntax::FieldExprPayload payload;
    payload.object = object;
    payload.field_name = field_name;
    return module.push_field_expr({}, payload);
}

[[nodiscard]] ExprId push_generic_apply(
    syntax::AstModule& module,
    const ExprId callee,
    const std::initializer_list<TypeId> type_args
) {
    syntax::GenericApplyExprPayload payload;
    payload.callee = callee;
    payload.type_args.assign(type_args.begin(), type_args.end());
    return module.push_generic_apply_expr({}, std::move(payload));
}

[[nodiscard]] ExprId push_unary(
    syntax::AstModule& module,
    const syntax::UnaryOp op,
    const ExprId operand
) {
    return module.push_unary_expr(
        syntax::ExprKind::unary,
        {},
        syntax::UnaryExprPayload {
            op,
            operand,
        }
    );
}

[[nodiscard]] ExprId push_binary(
    syntax::AstModule& module,
    const syntax::BinaryOp op,
    const ExprId lhs,
    const ExprId rhs
) {
    return module.push_binary_expr(
        {},
        syntax::BinaryExprPayload {
            op,
            lhs,
            rhs,
        }
    );
}

[[nodiscard]] ExprId push_call(
    syntax::AstModule& module,
    const ExprId callee,
    const std::initializer_list<ExprId> args = {}
) {
    syntax::CallExprPayload payload;
    payload.callee = callee;
    payload.args.assign(args.begin(), args.end());
    return module.push_call_expr(syntax::ExprKind::call, {}, std::move(payload));
}

[[nodiscard]] syntax::PostfixBracketArg bracket_expr_arg(const ExprId expr) {
    syntax::PostfixBracketArg arg;
    arg.expr = expr;
    return arg;
}

[[nodiscard]] syntax::PostfixBracketArg bracket_type_arg(const TypeId type) {
    syntax::PostfixBracketArg arg;
    arg.type = type;
    return arg;
}

[[nodiscard]] syntax::PostfixOp select_op(const std::string_view name) {
    syntax::PostfixOp op;
    op.kind = syntax::PostfixOpKind::select;
    op.name = name;
    return op;
}

[[nodiscard]] syntax::PostfixOp call_op(const std::initializer_list<ExprId> args = {}) {
    syntax::PostfixOp op;
    op.kind = syntax::PostfixOpKind::call;
    op.args.assign(args.begin(), args.end());
    return op;
}

[[nodiscard]] syntax::PostfixOp bracket_op(
    const std::initializer_list<syntax::PostfixBracketArg> args
) {
    syntax::PostfixOp op;
    op.kind = syntax::PostfixOpKind::bracket;
    op.bracket_args.assign(args.begin(), args.end());
    return op;
}

[[nodiscard]] syntax::PostfixOp slice_op(const ExprId start, const ExprId end) {
    syntax::PostfixOp op;
    op.kind = syntax::PostfixOpKind::bracket;
    op.bracket_is_slice = true;
    op.slice_start = start;
    op.slice_end = end;
    return op;
}

[[nodiscard]] ExprId push_postfix_chain(
    syntax::AstModule& module,
    const ExprId base,
    const std::initializer_list<syntax::PostfixOp> ops
) {
    syntax::PostfixChainExprPayload payload;
    payload.base = base;
    payload.ops.assign(ops.begin(), ops.end());
    return module.push_postfix_chain_expr({}, std::move(payload));
}

[[nodiscard]] ExprId push_integer(syntax::AstModule& module) {
    return module.push_literal_expr(syntax::ExprKind::integer_literal, {}, SEMA_TEST_INTEGER_LITERAL_ONE);
}

[[nodiscard]] ExprId push_integer_text(syntax::AstModule& module, const std::string_view text) {
    return module.push_literal_expr(syntax::ExprKind::integer_literal, {}, text);
}

[[nodiscard]] ExprId push_bool(syntax::AstModule& module, const std::string_view text) {
    return module.push_literal_expr(syntax::ExprKind::bool_literal, {}, text);
}

[[nodiscard]] syntax::StmtId push_stmt(syntax::AstModule& module, const syntax::StmtKind kind) {
    syntax::StmtNode stmt;
    stmt.kind = kind;
    return module.push_stmt(stmt);
}

[[nodiscard]] syntax::StmtId push_block(
    syntax::AstModule& module,
    const std::initializer_list<syntax::StmtId> statements
) {
    syntax::StmtNode block;
    block.kind = syntax::StmtKind::block;
    block.statements.assign(statements.begin(), statements.end());
    return module.push_stmt(block);
}

} // namespace

TEST(CoreUnit, SemanticWhiteBoxLayoutPlacesAndModules) {
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"lib", "one"}),
        module_info({"lib", "two"}),
        module_info({"lib", "reexported"}),
    };
    module.modules[0].imports = {
        resolved_import(module_id(1), "one"),
        resolved_import(module_id(2), "two"),
        resolved_import(syntax::INVALID_MODULE_ID, "broken"),
    };
    module.modules[1].imports = {
        resolved_import(module_id(3), "pub", syntax::Visibility::public_),
    };

    const ExprId value_expr = push_name(module, "value");
    const ExprId scoped_value_expr = push_name(module, "shared", "one");
    const ExprId missing_scoped_value_expr = push_name(module, "shared", "missing");
    const ExprId ptr_expr = push_name(module, "ptr");
    const ExprId record_expr = push_name(module, "record");
    const ExprId array_expr = push_name(module, "array");
    const ExprId field_id = push_field(module, ptr_expr, "field");
    const ExprId value_field_id = push_field(module, record_expr, "field");
    const ExprId nested_value_field_id = push_field(module, value_field_id, "field");
    const ExprId index_arg = push_integer(module);
    const ExprId index_id = module.push_index_expr({}, syntax::IndexExprPayload {ptr_expr, index_arg});
    const ExprId value_index_id = module.push_index_expr({}, syntax::IndexExprPayload {array_expr, index_arg});
    const ExprId nested_value_index_id = module.push_index_expr(
        {},
        syntax::IndexExprPayload {value_index_id, index_arg}
    );
    const ExprId deref_id = push_unary(module, syntax::UnaryOp::dereference, ptr_expr);
    const ExprId invalid_deref_id = push_unary(module, syntax::UnaryOp::dereference, value_expr);
    const ExprId not_id = push_unary(module, syntax::UnaryOp::logical_not, ptr_expr);
    const ExprId array_ref_expr = push_name(module, "array_ref");
    const ExprId array_ref_index_id = module.push_index_expr(
        {},
        syntax::IndexExprPayload {array_ref_expr, index_arg}
    );

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(module, diagnostics);
    analyzer.checked_.expr_types.assign(module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.current_module_ = module_id(0);

    sema::TypeTable& types = analyzer.checked_.types;
    const TypeHandle void_type = types.builtin(BuiltinType::void_);
    const TypeHandle bool_type = types.builtin(BuiltinType::bool_);
    const TypeHandle i8 = types.builtin(BuiltinType::i8);
    const TypeHandle u8 = types.builtin(BuiltinType::u8);
    const TypeHandle i16 = types.builtin(BuiltinType::i16);
    const TypeHandle u16 = types.builtin(BuiltinType::u16);
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle u32 = types.builtin(BuiltinType::u32);
    const TypeHandle i64 = types.builtin(BuiltinType::i64);
    const TypeHandle u64 = types.builtin(BuiltinType::u64);
    const TypeHandle isize = types.builtin(BuiltinType::isize);
    const TypeHandle usize = types.builtin(BuiltinType::usize);
    const TypeHandle f32 = types.builtin(BuiltinType::f32);
    const TypeHandle f64 = types.builtin(BuiltinType::f64);
    const TypeHandle str = types.builtin(BuiltinType::str);
    const TypeHandle char_type = types.builtin(BuiltinType::char_);
    const TypeHandle ptr_i32 = types.pointer(PointerMutability::mut, i32);
    const TypeHandle const_ptr_i32 = types.pointer(PointerMutability::const_, i32);
    const TypeHandle array_i16 = types.array(SEMA_TEST_SMALL_ARRAY_COUNT, i16);
    const TypeHandle missing_struct = types.named_struct("missing.Struct", "missing_Struct", false);
    const TypeHandle record_type = types.named_struct("lib.one.Record", "lib_one_Record", false);
    const TypeHandle enum_type = types.named_enum("lib.one.Enum", "lib_one_Enum");
    const TypeHandle payload_enum_type = types.named_enum("lib.one.Payload", "lib_one_Payload");
    const TypeHandle opaque_type = types.opaque_struct("lib.one.Opaque", "lib_one_Opaque");
    const TypeHandle nested_array_i16 = types.array(SEMA_TEST_NESTED_ARRAY_COUNT, array_i16);
    const TypeHandle array_void = types.array(SEMA_TEST_INVALID_ARRAY_COUNT, void_type);
    const TypeHandle array_opaque = types.array(SEMA_TEST_INVALID_ARRAY_COUNT, opaque_type);
    const TypeHandle overflowing_array = types.array(SEMA_TEST_LAYOUT_MAX_ARRAY_COUNT, array_i16);
    const TypeHandle place_record_type = types.named_struct("root.PlaceRecord", "root_PlaceRecord", false);
    const TypeHandle ptr_place_record = types.pointer(PointerMutability::mut, place_record_type);
    const TypeHandle ref_nested_array_i16 = types.reference(PointerMutability::mut, nested_array_i16);
    types.set_enum_underlying(enum_type, u16);
    types.set_enum_underlying(payload_enum_type, u8);
    types.set_enum_payload_layout(payload_enum_type, u64, sizeof(std::uint64_t), alignof(std::uint64_t));

    StructInfo record;
    record.name = "Record";
    record.name_id = intern_identifier(analyzer, "Record");
    record.module = module_id(1);
    record.type = record_type;
    record.fields = {
        StructFieldInfo {"tag", intern_identifier(analyzer, "tag"), "tag", module_id(1), u8},
        StructFieldInfo {"value", intern_identifier(analyzer, "value"), "value", module_id(1), u64},
    };
    analyzer.checked_.structs.emplace(semantic_module_key(analyzer, module_id(1), "Record"), record);

    StructInfo place_record;
    place_record.name = "PlaceRecord";
    place_record.name_id = intern_identifier(analyzer, "PlaceRecord");
    place_record.module = module_id(0);
    place_record.type = place_record_type;
    place_record.fields = {
        StructFieldInfo {"field", intern_identifier(analyzer, "field"), "field", module_id(0), place_record_type},
    };
    analyzer.checked_.structs.emplace(semantic_module_key(analyzer, module_id(0), "PlaceRecord"), place_record);

    EXPECT_EQ(analyzer.abi_size(INVALID_TYPE_HANDLE), SEMA_TEST_ABI_INVALID_SIZE);
    EXPECT_EQ(analyzer.abi_align(INVALID_TYPE_HANDLE), SEMA_TEST_ABI_MIN_ALIGNMENT);
    EXPECT_EQ(analyzer.abi_size(void_type), SEMA_TEST_ABI_INVALID_SIZE);
    EXPECT_EQ(analyzer.abi_align(void_type), SEMA_TEST_ABI_MIN_ALIGNMENT);
    EXPECT_EQ(analyzer.abi_size(bool_type), sizeof(bool));
    EXPECT_EQ(analyzer.abi_align(bool_type), alignof(bool));
    EXPECT_EQ(analyzer.abi_size(i8), sizeof(std::uint8_t));
    EXPECT_EQ(analyzer.abi_size(u8), sizeof(std::uint8_t));
    EXPECT_EQ(analyzer.abi_size(i16), sizeof(std::uint16_t));
    EXPECT_EQ(analyzer.abi_size(u16), sizeof(std::uint16_t));
    EXPECT_EQ(analyzer.abi_size(i32), sizeof(std::uint32_t));
    EXPECT_EQ(analyzer.abi_size(u32), sizeof(std::uint32_t));
    EXPECT_EQ(analyzer.abi_size(i64), sizeof(std::uint64_t));
    EXPECT_EQ(analyzer.abi_size(u64), sizeof(std::uint64_t));
    EXPECT_EQ(analyzer.abi_size(isize), sizeof(std::ptrdiff_t));
    EXPECT_EQ(analyzer.abi_size(usize), sizeof(std::size_t));
    EXPECT_EQ(analyzer.abi_size(f32), sizeof(float));
    EXPECT_EQ(analyzer.abi_size(f64), sizeof(double));
    EXPECT_EQ(analyzer.abi_size(str), sizeof(void*) + sizeof(std::size_t));
    EXPECT_EQ(analyzer.abi_size(char_type), sizeof(std::uint32_t));
    EXPECT_EQ(analyzer.abi_align(i8), alignof(std::uint8_t));
    EXPECT_EQ(analyzer.abi_align(i64), alignof(std::uint64_t));
    EXPECT_EQ(analyzer.abi_align(isize), alignof(std::ptrdiff_t));
    EXPECT_EQ(analyzer.abi_align(usize), alignof(std::size_t));
    EXPECT_EQ(analyzer.abi_align(f32), alignof(float));
    EXPECT_EQ(analyzer.abi_align(f64), alignof(double));
    EXPECT_EQ(analyzer.abi_align(str), alignof(void*));
    EXPECT_EQ(analyzer.abi_align(char_type), alignof(std::uint32_t));
    EXPECT_EQ(analyzer.abi_size(ptr_i32), sizeof(void*));
    EXPECT_EQ(analyzer.abi_align(ptr_i32), alignof(void*));
    EXPECT_EQ(analyzer.abi_size(array_i16), SEMA_TEST_SMALL_ARRAY_COUNT * sizeof(std::uint16_t));
    EXPECT_EQ(analyzer.abi_align(array_i16), alignof(std::uint16_t));
    EXPECT_EQ(analyzer.abi_size(missing_struct), SEMA_TEST_ABI_INVALID_SIZE);
    EXPECT_EQ(analyzer.abi_align(missing_struct), SEMA_TEST_ABI_MIN_ALIGNMENT);
    EXPECT_EQ(analyzer.abi_size(record_type), SEMA_TEST_RECORD_ABI_SIZE);
    EXPECT_EQ(analyzer.abi_align(record_type), alignof(std::uint64_t));
    EXPECT_EQ(analyzer.abi_size(enum_type), sizeof(std::uint16_t));
    EXPECT_EQ(analyzer.abi_align(enum_type), alignof(std::uint16_t));
    EXPECT_GT(analyzer.abi_size(payload_enum_type), sizeof(std::uint8_t));
    EXPECT_EQ(analyzer.abi_align(payload_enum_type), alignof(std::uint64_t));
    EXPECT_EQ(analyzer.abi_size(opaque_type), SEMA_TEST_ABI_INVALID_SIZE);
    EXPECT_EQ(analyzer.abi_align(opaque_type), SEMA_TEST_ABI_MIN_ALIGNMENT);

    EXPECT_FALSE(analyzer.is_valid_cast(syntax::ExprKind::cast, INVALID_TYPE_HANDLE, i32));
    EXPECT_TRUE(analyzer.is_valid_cast(syntax::ExprKind::cast, f64, i32));
    EXPECT_TRUE(analyzer.is_valid_cast(syntax::ExprKind::pcast, const_ptr_i32, ptr_i32));
    EXPECT_TRUE(analyzer.is_valid_cast(syntax::ExprKind::bcast, i32, i32));
    EXPECT_TRUE(analyzer.is_valid_cast(syntax::ExprKind::bcast, u32, i32));
    EXPECT_TRUE(analyzer.is_valid_cast(syntax::ExprKind::bcast, char_type, u32));
    EXPECT_FALSE(analyzer.is_valid_cast(syntax::ExprKind::cast, u32, char_type));
    EXPECT_FALSE(analyzer.is_valid_cast(syntax::ExprKind::bcast, bool_type, i8));
    EXPECT_FALSE(analyzer.is_valid_cast(syntax::ExprKind::bcast, str, ptr_i32));
    EXPECT_FALSE(analyzer.is_valid_cast(syntax::ExprKind::ptr_addr, u32, ptr_i32));
    EXPECT_FALSE(analyzer.is_valid_storage_type(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(analyzer.is_valid_storage_type(void_type));
    EXPECT_FALSE(analyzer.is_valid_storage_type(opaque_type));
    EXPECT_TRUE(analyzer.is_valid_storage_type(char_type));
    EXPECT_TRUE(analyzer.is_valid_storage_type(nested_array_i16));
    EXPECT_FALSE(analyzer.is_valid_storage_type(array_void));
    EXPECT_FALSE(analyzer.is_valid_storage_type(array_opaque));
    EXPECT_FALSE(analyzer.is_valid_storage_type(overflowing_array));

    EXPECT_TRUE(analyzer.symbols_.insert(indexed_symbol(analyzer, SymbolKind::local, "value", module_id(0), i32, true), diagnostics));
    EXPECT_TRUE(analyzer.symbols_.insert(indexed_symbol(analyzer, SymbolKind::local, "ptr", module_id(0), ptr_place_record, true), diagnostics));
    EXPECT_TRUE(analyzer.symbols_.insert(indexed_symbol(analyzer, SymbolKind::local, "record", module_id(0), place_record_type, true), diagnostics));
    EXPECT_TRUE(analyzer.symbols_.insert(indexed_symbol(analyzer, SymbolKind::local, "array", module_id(0), nested_array_i16, true), diagnostics));
    EXPECT_TRUE(analyzer.symbols_.insert(indexed_symbol(analyzer, SymbolKind::local, "array_ref", module_id(0), ref_nested_array_i16, true), diagnostics));
    static_cast<void>(add_global_value(analyzer, module_id(1), "shared", i32, SymbolKind::local, true));

    EXPECT_TRUE(analyzer.is_place_expr(value_expr));
    EXPECT_FALSE(analyzer.is_place_expr(scoped_value_expr));
    EXPECT_FALSE(analyzer.is_place_expr(missing_scoped_value_expr));
    EXPECT_TRUE(analyzer.is_place_expr(field_id));
    EXPECT_TRUE(analyzer.is_place_expr(index_id));
    EXPECT_TRUE(analyzer.is_place_expr(nested_value_field_id));
    EXPECT_TRUE(analyzer.is_place_expr(nested_value_index_id));
    EXPECT_TRUE(analyzer.is_place_expr(deref_id));
    EXPECT_FALSE(analyzer.is_place_expr(not_id));
    EXPECT_FALSE(analyzer.is_place_expr(syntax::INVALID_EXPR_ID));
    EXPECT_TRUE(analyzer.is_writable_place(value_expr));
    EXPECT_FALSE(analyzer.is_writable_place(scoped_value_expr));
    EXPECT_FALSE(analyzer.is_writable_place(missing_scoped_value_expr));
    EXPECT_TRUE(analyzer.is_writable_place(field_id));
    EXPECT_TRUE(analyzer.is_writable_place(index_id));
    EXPECT_TRUE(analyzer.is_writable_place(value_field_id));
    EXPECT_TRUE(analyzer.is_writable_place(value_index_id));
    EXPECT_TRUE(analyzer.is_writable_place(nested_value_field_id));
    EXPECT_TRUE(analyzer.is_writable_place(nested_value_index_id));
    EXPECT_TRUE(analyzer.is_writable_place(array_ref_index_id));
    EXPECT_TRUE(analyzer.is_writable_place(deref_id));
    EXPECT_FALSE(analyzer.is_writable_place(not_id));
    EXPECT_FALSE(analyzer.is_writable_place(syntax::INVALID_EXPR_ID));
    const sema::SemanticAnalyzer::PlaceInfo invalid_deref_place =
        analyzer.analyze_place_info(invalid_deref_id, true);
    EXPECT_FALSE(invalid_deref_place.is_place);
    EXPECT_FALSE(diagnostics.diagnostics().empty());

    analyzer.current_module_ = syntax::INVALID_MODULE_ID;
    EXPECT_FALSE(syntax::is_valid(analyzer.resolve_import_alias("missing", {})));
    analyzer.current_module_ = module_id(0);
    analyzer.module_.modules[0].imports.push_back(resolved_import(module_id(2), "one"));
    EXPECT_FALSE(syntax::is_valid(analyzer.resolve_import_alias("one", {})));
    analyzer.module_.modules[0].imports.pop_back();
    EXPECT_FALSE(syntax::is_valid(analyzer.resolve_import_alias("missing", {})));

    EXPECT_TRUE(analyzer.visible_modules(syntax::INVALID_MODULE_ID).empty());
    EXPECT_EQ(analyzer.visible_modules(module_id(SEMA_TEST_MISSING_MODULE_INDEX)).size(), 1U);
    EXPECT_TRUE(analyzer.module_export_modules(syntax::INVALID_MODULE_ID).empty());
    EXPECT_EQ(analyzer.module_export_modules(module_id(SEMA_TEST_MISSING_MODULE_INDEX)).size(), 1U);
    const auto& visible = analyzer.visible_modules(module_id(0));
    ASSERT_GE(visible.size(), 4U);
    EXPECT_EQ(analyzer.module_name(syntax::INVALID_MODULE_ID), "<unknown>");
    EXPECT_EQ(analyzer.qualified_name(syntax::INVALID_MODULE_ID, "Name"), "Name");
    EXPECT_EQ(analyzer.c_symbol_name(syntax::INVALID_MODULE_ID, "Name"), "Name");
}

TEST(CoreUnit, SemanticWhiteBoxLookupsAndMethodReceivers) {
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"lib", "one"}),
        module_info({"lib", "two"}),
    };
    module.modules[0].imports = {
        resolved_import(module_id(1), "one"),
        resolved_import(module_id(2), "two"),
    };
    const ExprId value_expr = push_name(module, "value");
    const ExprId literal_expr = push_integer(module);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(module, diagnostics);
    analyzer.checked_.expr_types.assign(module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.current_module_ = module_id(0);
    sema::TypeTable& types = analyzer.checked_.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle bool_type = types.builtin(BuiltinType::bool_);
    const TypeHandle record_type = types.named_struct("Record", "Record", false);
    const TypeHandle array_record = types.named_struct("ArrayRecord", "ArrayRecord", true);
    types.set_record_contains_array(array_record, true);
    const TypeHandle ptr_record = types.pointer(PointerMutability::mut, record_type);
    const TypeHandle const_ptr_record = types.pointer(PointerMutability::const_, record_type);
    const TypeHandle enum_type = types.named_enum("Choice", "Choice");

    const IdentId shared_id = intern_identifier(analyzer, "Shared");
    const IdentId private_id = intern_identifier(analyzer, "Private");
    const IdentId ambiguous_id = intern_identifier(analyzer, "ambiguous");
    const IdentId hidden_id = intern_identifier(analyzer, "hidden");
    const IdentId missing_id = intern_identifier(analyzer, "missing");
    const IdentId same_id = intern_identifier(analyzer, "same");
    const IdentId private_value_id = intern_identifier(analyzer, "private_value");
    const IdentId run_id = intern_identifier(analyzer, "run");
    const IdentId static_only_id = intern_identifier(analyzer, "static_only");
    const IdentId free_id = intern_identifier(analyzer, "free");

    static_cast<void>(add_global_value(analyzer, module_id(0), "value", record_type, SymbolKind::local, true));

    static_cast<void>(add_named_type(analyzer, module_id(1), "Shared", i32));
    static_cast<void>(add_named_type(analyzer, module_id(2), "Shared", bool_type));
    EXPECT_FALSE(is_valid(analyzer.find_type_in_visible_modules(shared_id, "Shared", {}, false)));

    static_cast<void>(add_named_type(analyzer, module_id(1), "Private", record_type, syntax::Visibility::private_));
    EXPECT_FALSE(is_valid(analyzer.find_type_in_module(module_id(1), private_id, "Private", {}, false)));
    EXPECT_FALSE(is_valid(analyzer.find_type_in_module(syntax::INVALID_MODULE_ID, missing_id, "Missing", {}, false)));
    EXPECT_FALSE(is_valid(analyzer.find_type_in_module(module_id(1), missing_id, "Missing", {}, false)));

    FunctionSignature one = indexed_function_signature(analyzer, "ambiguous", module_id(1), i32);
    FunctionSignature two = indexed_function_signature(analyzer, "ambiguous", module_id(2), i32);
    static_cast<void>(add_function(analyzer, one));
    static_cast<void>(add_function(analyzer, two));
    EXPECT_EQ(analyzer.find_function_in_visible_modules(ambiguous_id, "ambiguous", {}), nullptr);
    FunctionSignature private_function = indexed_function_signature(analyzer, "hidden", module_id(1), i32);
    private_function.visibility = syntax::Visibility::private_;
    static_cast<void>(add_function(analyzer, private_function));
    EXPECT_EQ(analyzer.find_function_in_module(module_id(1), hidden_id, "hidden", {}), nullptr);
    EXPECT_EQ(analyzer.find_function_in_module(module_id(1), missing_id, "missing", {}), nullptr);
    EXPECT_EQ(analyzer.find_function_in_module(syntax::INVALID_MODULE_ID, missing_id, "missing", {}), nullptr);

    EnumCaseInfo one_case;
    one_case.name = "same";
    one_case.name_id = same_id;
    one_case.case_name = "same";
    one_case.case_name_id = same_id;
    one_case.module = module_id(1);
    one_case.type = enum_type;
    EnumCaseInfo two_case = one_case;
    two_case.module = module_id(2);
    auto one_case_inserted = analyzer.checked_.enum_cases.emplace(semantic_module_key(analyzer, module_id(1), "same"), one_case);
    auto two_case_inserted = analyzer.checked_.enum_cases.emplace(semantic_module_key(analyzer, module_id(2), "same"), two_case);
    analyzer.index_enum_case(one_case_inserted.first->second);
    analyzer.index_enum_case(two_case_inserted.first->second);
    EXPECT_EQ(analyzer.find_enum_case_in_visible_modules(same_id, "same", {}), nullptr);
    EXPECT_EQ(analyzer.find_enum_case_in_visible_modules(missing_id, "missing", {}), nullptr);

    static_cast<void>(add_named_type(analyzer, module_id(0), "Record", record_type));
    static_cast<void>(add_named_type(analyzer, module_id(0), "Choice", enum_type));
    EXPECT_EQ(analyzer.find_enum_case_by_scoped_name(
        intern_identifier(analyzer, "Record"),
        "Record",
        missing_id,
        "missing",
        {},
        true
    ), nullptr);
    EXPECT_EQ(analyzer.find_enum_case_by_scoped_name(
        intern_identifier(analyzer, "Choice"),
        "Choice",
        missing_id,
        "missing",
        {},
        true
    ), nullptr);
    EXPECT_EQ(analyzer.find_enum_constructor(syntax::INVALID_EXPR_ID, true), nullptr);

    static_cast<void>(add_global_value(analyzer, module_id(1), "ambiguous", i32));
    static_cast<void>(add_global_value(analyzer, module_id(2), "ambiguous", i32));
    EXPECT_EQ(analyzer.find_symbol(ambiguous_id, "ambiguous", {}), nullptr);
    EXPECT_EQ(analyzer.find_symbol(missing_id, "missing", {}), nullptr);
    static_cast<void>(add_global_value(
        analyzer,
        module_id(1),
        "private_value",
        i32,
        SymbolKind::const_,
        false,
        syntax::Visibility::private_
    ));
    EXPECT_EQ(analyzer.find_symbol_in_module(module_id(1), missing_id, "missing", {}, true), nullptr);
    EXPECT_EQ(analyzer.find_symbol_in_module(module_id(1), private_value_id, "private_value", {}, true), nullptr);

    FunctionSignature no_self;
    EXPECT_FALSE(analyzer.method_receiver_matches(no_self, record_type, value_expr));
    FunctionSignature by_value;
    by_value.has_self_param = true;
    by_value.param_types = {array_record};
    EXPECT_FALSE(analyzer.method_receiver_matches(by_value, array_record, value_expr));
    FunctionSignature plain_self;
    plain_self.has_self_param = true;
    plain_self.param_types = {record_type};
    EXPECT_TRUE(analyzer.method_receiver_matches(plain_self, record_type, value_expr));
    FunctionSignature pointer_self;
    pointer_self.has_self_param = true;
    pointer_self.param_types = {ptr_record};
    EXPECT_TRUE(analyzer.method_receiver_matches(pointer_self, ptr_record, value_expr));
    EXPECT_FALSE(analyzer.method_receiver_matches(pointer_self, const_ptr_record, value_expr));
    EXPECT_FALSE(analyzer.method_receiver_matches(pointer_self, record_type, literal_expr));
    EXPECT_FALSE(analyzer.method_receiver_matches(plain_self, bool_type, value_expr));
    EXPECT_FALSE(analyzer.method_receiver_matches(pointer_self, bool_type, value_expr));

    FunctionSignature method = indexed_function_signature(analyzer, "run", module_id(1), i32);
    method.is_method = true;
    method.has_self_param = true;
    method.method_owner_type = record_type;
    FunctionSignature other_method = method;
    other_method.module = module_id(2);
    static_cast<void>(add_method(analyzer, method, record_type));
    static_cast<void>(add_method(analyzer, other_method, record_type));
    FunctionSignature static_method = indexed_function_signature(analyzer, "static_only", module_id(1), i32);
    static_method.is_method = true;
    static_method.method_owner_type = record_type;
    static_cast<void>(add_method(analyzer, static_method, record_type));
    FunctionSignature not_method = indexed_function_signature(analyzer, "free", module_id(1), i32);
    not_method.method_owner_type = record_type;
    analyzer.checked_.functions.emplace(semantic_method_key(analyzer, module_id(1), record_type, "free"), not_method);
    EXPECT_EQ(analyzer.find_method_in_visible_modules(record_type, run_id, "run", {}, true), nullptr);
    EXPECT_EQ(analyzer.find_method_in_visible_modules(record_type, static_only_id, "static_only", {}, true), nullptr);
    EXPECT_EQ(analyzer.find_method_in_visible_modules(record_type, free_id, "free", {}, false), nullptr);
    EXPECT_EQ(analyzer.find_method_in_visible_modules(record_type, missing_id, "missing", {}, true), nullptr);
}

TEST(CoreUnit, SemanticWhiteBoxDotOnlyModuleSelectorAndShadowingEdges) {
    syntax::AstModule module;
    module.modules = {
        module_info({"app"}),
        module_info({"lib"}),
        module_info({SEMA_TEST_ROOT_MODULE_NAME, SEMA_TEST_CHILD_MODULE_NAME}),
        module_info({SEMA_TEST_ROOT_MODULE_NAME, SEMA_TEST_CHILD_MODULE_NAME, SEMA_TEST_LEAF_MODULE_NAME}),
    };
    module.modules[SEMA_TEST_ROOT_MODULE_INDEX].imports = {
        resolved_import(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "lib"),
        resolved_import(module_id(2), "mem"),
        resolved_import(module_id(3), "io"),
    };

    syntax::ItemNode item;
    item.kind = syntax::ItemKind::fn_decl;
    item.name = "owned";
    const syntax::ItemId item_id = module.push_item(item);
    module.item_modules[item_id.value] = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(module, diagnostics);
    analyzer.current_module_ = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    sema::TypeTable& types = analyzer.checked_.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle bool_type = types.builtin(BuiltinType::bool_);
    const TypeHandle local_type = types.named_struct("LocalType", "LocalType", false);
    const IdentId local_type_id = intern_identifier(analyzer, "LocalType");
    const IdentId global_id = intern_identifier(analyzer, "global");
    const IdentId lib_id = intern_identifier(analyzer, "lib");
    const IdentId root_module_id = intern_identifier(analyzer, SEMA_TEST_ROOT_MODULE_NAME);
    const IdentId fresh_id = intern_identifier(analyzer, "fresh");
    const IdentId t_id = intern_identifier(analyzer, "T");
    static_cast<void>(add_named_type(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "LocalType", local_type));
    static_cast<void>(add_global_value(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "global", i32));
    EXPECT_TRUE(analyzer.symbols_.insert(indexed_symbol(
        analyzer,
        SymbolKind::local,
        "local",
        module_id(SEMA_TEST_ROOT_MODULE_INDEX),
        i32
    ), diagnostics));

    const ExprId alias_expr = push_name(analyzer.module_, "lib");
    const ExprId root_expr = push_name(analyzer.module_, SEMA_TEST_ROOT_MODULE_NAME);
    const ExprId core_mem = push_field(analyzer.module_, root_expr, SEMA_TEST_CHILD_MODULE_NAME);
    const ExprId core_mem_io = push_field(analyzer.module_, core_mem, SEMA_TEST_LEAF_MODULE_NAME);
    const ExprId invalid_expr = push_name(analyzer.module_, "missing");
    const ExprId missing_root_child = push_field(
        analyzer.module_,
        push_name(analyzer.module_, SEMA_TEST_ROOT_MODULE_NAME),
        "missing"
    );
    const ExprId core_mem_file = push_field(analyzer.module_, core_mem, "File");
    const ExprId local_member = push_field(analyzer.module_, push_name(analyzer.module_, "local"), "member");
    const ExprId alias_member = push_field(analyzer.module_, push_name(analyzer.module_, "mem"), "File");
    const ExprId empty_field = push_field(analyzer.module_, push_name(analyzer.module_, "lib"), {});
    const ExprId scoped_name = push_name(analyzer.module_, "Name", "scope");
    const ExprId not_selector = push_integer(analyzer.module_);
    analyzer.checked_.expr_types.assign(analyzer.module_.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.checked_.expr_c_name_ids.assign(analyzer.module_.exprs.size(), sema::INVALID_IDENT_ID);

    const sema::SemanticAnalyzer::ModuleSelectorPath invalid_path =
        analyzer.expr_selector_path(syntax::INVALID_EXPR_ID);
    EXPECT_TRUE(invalid_path.parts.empty());
    EXPECT_TRUE(analyzer.expr_selector_path(empty_field).parts.empty());
    EXPECT_TRUE(analyzer.expr_selector_path(scoped_name).parts.empty());
    EXPECT_TRUE(analyzer.expr_selector_path(not_selector).parts.empty());

    const sema::SemanticAnalyzer::ModuleSelectorPath core_mem_io_path =
        analyzer.expr_selector_path(core_mem_io);
    ASSERT_EQ(core_mem_io_path.parts.size(), 3U);
    EXPECT_EQ(core_mem_io_path.parts[0], SEMA_TEST_ROOT_MODULE_NAME);
    EXPECT_EQ(core_mem_io_path.parts[1], SEMA_TEST_CHILD_MODULE_NAME);
    EXPECT_EQ(core_mem_io_path.parts[2], SEMA_TEST_LEAF_MODULE_NAME);

    const sema::SemanticAnalyzer::ModuleSelector alias_selector =
        analyzer.resolve_module_selector(alias_expr, true);
    EXPECT_EQ(alias_selector.module.value, SEMA_TEST_LIB_ONE_MODULE_INDEX);
    EXPECT_FALSE(alias_selector.failed_as_module_selector);

    const sema::SemanticAnalyzer::ModuleSelector root_selector =
        analyzer.resolve_module_selector(core_mem, true);
    EXPECT_EQ(root_selector.module.value, 2U);
    EXPECT_FALSE(root_selector.failed_as_module_selector);

    const sema::SemanticAnalyzer::ModuleSelector leaf_selector =
        analyzer.resolve_module_selector(core_mem_io, true);
    EXPECT_EQ(leaf_selector.module.value, 3U);
    EXPECT_FALSE(leaf_selector.failed_as_module_selector);

    const sema::SemanticAnalyzer::ModuleSelector unknown_alias_selector =
        analyzer.resolve_module_selector(invalid_expr, true);
    EXPECT_FALSE(syntax::is_valid(unknown_alias_selector.module));
    EXPECT_TRUE(unknown_alias_selector.failed_as_module_selector);

    const sema::SemanticAnalyzer::ModuleSelector missing_child_selector =
        analyzer.resolve_module_selector(missing_root_child, true);
    EXPECT_FALSE(syntax::is_valid(missing_child_selector.module));
    EXPECT_TRUE(missing_child_selector.failed_as_module_selector);

    const sema::SemanticAnalyzer::ModuleSelector prefix_member_selector =
        analyzer.resolve_module_selector(core_mem_file, true);
    EXPECT_FALSE(syntax::is_valid(prefix_member_selector.module));
    EXPECT_FALSE(prefix_member_selector.failed_as_module_selector);

    const sema::SemanticAnalyzer::ModuleSelector local_selector =
        analyzer.resolve_module_selector(local_member, true);
    EXPECT_FALSE(syntax::is_valid(local_selector.module));
    EXPECT_FALSE(local_selector.failed_as_module_selector);

    const sema::SemanticAnalyzer::ModuleSelector alias_member_selector =
        analyzer.resolve_module_selector(alias_member, true);
    EXPECT_FALSE(syntax::is_valid(alias_member_selector.module));
    EXPECT_FALSE(alias_member_selector.failed_as_module_selector);

    EXPECT_TRUE(analyzer.visible_root_module_name_exists(SEMA_TEST_ROOT_MODULE_NAME));
    EXPECT_FALSE(analyzer.visible_root_module_name_exists({}));
    EXPECT_TRUE(analyzer.visible_module_path_prefix_exists({SEMA_TEST_ROOT_MODULE_NAME, SEMA_TEST_CHILD_MODULE_NAME, "File"}));
    EXPECT_FALSE(analyzer.visible_module_path_prefix_exists({SEMA_TEST_ROOT_MODULE_NAME}));
    EXPECT_FALSE(analyzer.visible_module_path_prefix_exists({"missing", "path"}));

    EXPECT_FALSE(analyzer.can_define_local_name(lib_id, "lib", {}));
    EXPECT_FALSE(analyzer.can_define_local_name(root_module_id, SEMA_TEST_ROOT_MODULE_NAME, {}));
    EXPECT_FALSE(analyzer.can_define_local_name(local_type_id, "LocalType", {}));
    EXPECT_TRUE(analyzer.can_define_local_name(fresh_id, "fresh", {}));

    sema::SemanticAnalyzer::GenericContext generic_context;
    generic_context.params.emplace(t_id, bool_type);
    analyzer.current_generic_context_ = &generic_context;
    EXPECT_TRUE(analyzer.current_generic_param_exists(t_id, "T"));
    EXPECT_TRUE(analyzer.visible_type_name_exists(t_id, "T"));
    EXPECT_FALSE(analyzer.can_define_local_name(t_id, "T", {}));
    analyzer.current_generic_context_ = nullptr;

    const IdentId missing_id = intern_identifier(analyzer, "missing");
    EXPECT_TRUE(analyzer.top_level_value_name_exists(module_id(SEMA_TEST_ROOT_MODULE_INDEX), global_id, "global"));
    EXPECT_FALSE(analyzer.top_level_value_name_exists(module_id(SEMA_TEST_ROOT_MODULE_INDEX), missing_id, "missing"));
    EXPECT_TRUE(analyzer.module_type_or_value_name_exists(module_id(SEMA_TEST_ROOT_MODULE_INDEX), local_type_id, "LocalType"));
    EXPECT_TRUE(analyzer.module_type_or_value_name_exists(module_id(SEMA_TEST_ROOT_MODULE_INDEX), global_id, "global"));
    EXPECT_FALSE(analyzer.module_type_or_value_name_exists(module_id(SEMA_TEST_ROOT_MODULE_INDEX), missing_id, "missing"));

    EXPECT_EQ(analyzer.item_module(item_id).value, SEMA_TEST_ROOT_MODULE_INDEX);
    EXPECT_FALSE(syntax::is_valid(analyzer.item_module(syntax::ItemId {99})));
    analyzer.module_.item_modules.clear();
    EXPECT_FALSE(syntax::is_valid(analyzer.item_module(item_id)));
}

TEST(CoreUnit, SemanticWhiteBoxTypedLookupRejectsAmbiguousPublicReexports) {
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"lib", "one"}),
        module_info({"lib", "two"}),
    };
    module.modules[SEMA_TEST_ROOT_MODULE_INDEX].imports = {
        resolved_import(module_id(1), "one", syntax::Visibility::public_),
        resolved_import(module_id(2), "two", syntax::Visibility::public_),
    };

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(module, diagnostics);
    analyzer.current_module_ = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    sema::TypeTable& types = analyzer.checked_.types;
    const TypeHandle one_type = types.named_struct("lib.one.Shared", "lib_one_Shared", false);
    const TypeHandle two_type = types.named_struct("lib.two.Shared", "lib_two_Shared", false);
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const IdentId shared_id = intern_identifier(analyzer, "Shared");
    const IdentId run_id = intern_identifier(analyzer, "run");
    const IdentId value_id = intern_identifier(analyzer, "VALUE");

    static_cast<void>(add_named_type(analyzer, module_id(1), "Shared", one_type));
    static_cast<void>(add_named_type(analyzer, module_id(2), "Shared", two_type));
    EXPECT_FALSE(is_valid(analyzer.find_type_in_module(
        module_id(SEMA_TEST_ROOT_MODULE_INDEX),
        shared_id,
        "Shared",
        {},
        false
    )));

    static_cast<void>(add_function(analyzer, indexed_function_signature(analyzer, "run", module_id(1), i32)));
    static_cast<void>(add_function(analyzer, indexed_function_signature(analyzer, "run", module_id(2), i32)));
    EXPECT_EQ(
        analyzer.find_function_in_module(module_id(SEMA_TEST_ROOT_MODULE_INDEX), run_id, "run", {}),
        nullptr
    );

    static_cast<void>(add_global_value(analyzer, module_id(1), "VALUE", i32));
    static_cast<void>(add_global_value(analyzer, module_id(2), "VALUE", i32));
    EXPECT_EQ(
        analyzer.find_symbol_in_module(module_id(SEMA_TEST_ROOT_MODULE_INDEX), value_id, "VALUE", {}),
        nullptr
    );
}

TEST(CoreUnit, SemanticWhiteBoxFunctionAndEnumLookupFallbackEdges) {
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"owner"}),
    };
    module.modules[SEMA_TEST_ROOT_MODULE_INDEX].imports = {
        resolved_import(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "owner"),
    };

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(module, diagnostics);
    analyzer.current_module_ = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    sema::TypeTable& types = analyzer.checked_.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle bool_type = types.builtin(BuiltinType::bool_);
    const TypeHandle array_i32 = types.array(SEMA_TEST_SMALL_ARRAY_COUNT, i32);
    const TypeHandle fn_type = types.function(
        sema::FunctionCallConv::aurex,
        false,
        false,
        {i32},
        bool_type
    );
    const TypeHandle unsafe_fn_type = types.function(
        sema::FunctionCallConv::aurex,
        true,
        false,
        {},
        bool_type
    );

    FunctionSignature callable =
        indexed_function_signature(analyzer, "callable", module_id(SEMA_TEST_ROOT_MODULE_INDEX), bool_type);
    callable.param_types = {i32};
    static_cast<void>(add_function(analyzer, callable));
    const Symbol callable_symbol = indexed_symbol(
        analyzer,
        SymbolKind::function,
        "callable",
        module_id(SEMA_TEST_ROOT_MODULE_INDEX),
        INVALID_TYPE_HANDLE
    );
    EXPECT_TRUE(types.is_function(analyzer.function_type_from_symbol(callable_symbol, {})));

    const Symbol fallback_symbol = indexed_symbol(
        analyzer,
        SymbolKind::local,
        "local_fn",
        syntax::INVALID_MODULE_ID,
        fn_type
    );
    EXPECT_TRUE(types.same(analyzer.function_type_from_symbol(fallback_symbol, {}), fn_type));

    analyzer.validate_unsafe_function_value_call(i32, {});
    analyzer.validate_unsafe_function_value_call(unsafe_fn_type, {});
    EXPECT_TRUE(diagnostics.has_error());
    analyzer.unsafe_context_depth_ += 1;
    analyzer.validate_unsafe_function_value_call(unsafe_fn_type, {});
    analyzer.unsafe_context_depth_ -= 1;

    EXPECT_FALSE(analyzer.find_enum_cases_by_type(INVALID_TYPE_HANDLE));
    EnumCaseInfo invalid_case;
    invalid_case.name = "invalid";
    invalid_case.case_name = "invalid";
    invalid_case.type = INVALID_TYPE_HANDLE;
    analyzer.index_enum_case(invalid_case);
    EXPECT_TRUE(analyzer.enum_cases_by_type_.empty());

    const TypeHandle record_type = types.named_struct("Record", "Record", false);
    const TypeHandle enum_type = types.named_enum("Choice", "Choice");
    types.set_enum_underlying(enum_type, i32);
    static_cast<void>(add_named_type(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "Record", record_type));
    static_cast<void>(add_named_type(analyzer, module_id(SEMA_TEST_ROOT_MODULE_INDEX), "Choice", enum_type));
    const EnumCaseInfo* const yes_case = add_enum_case(
        analyzer,
        module_id(SEMA_TEST_ROOT_MODULE_INDEX),
        "Choice_yes",
        "yes",
        enum_type,
        INVALID_TYPE_HANDLE,
        {array_i32}
    ).second;
    ASSERT_NE(yes_case, nullptr);
    ASSERT_NE(analyzer.find_enum_cases_by_type(enum_type), nullptr);
    const IdentId yes_id = intern_identifier(analyzer, "yes");
    const IdentId case_id = intern_identifier(analyzer, "case");
    const IdentId record_id = intern_identifier(analyzer, "Record");
    const IdentId missing_id = intern_identifier(analyzer, "missing");
    const IdentId missing_enum_id = intern_identifier(analyzer, "Missing");
    const base::usize interned_cases = analyzer.module_.identifiers.size();
    const EnumCaseInfo* indexed_yes = analyzer.find_enum_case_by_type_and_case(enum_type, yes_id, "yes");
    ASSERT_NE(indexed_yes, nullptr);
    EXPECT_EQ(indexed_yes->case_name, "yes");
    EXPECT_EQ(analyzer.find_enum_case_by_type_and_case(record_type, yes_id, "yes"), nullptr);
    EXPECT_EQ(analyzer.find_enum_case_by_type_and_case(enum_type, missing_id, "missing"), nullptr);
    EXPECT_EQ(analyzer.module_.identifiers.size(), interned_cases);
    EXPECT_EQ(analyzer.enum_cases_by_type_and_case_.size(), 1U);
    EXPECT_TRUE(sema::is_valid(analyzer.module_.find_identifier("yes")));

    EXPECT_EQ(analyzer.find_enum_case_by_scoped_name(
        missing_enum_id,
        "Missing",
        case_id,
        "case",
        {},
        false
    ), nullptr);
    EXPECT_EQ(analyzer.find_enum_case_by_scoped_name(
        record_id,
        "Record",
        case_id,
        "case",
        {},
        true
    ), nullptr);
    EXPECT_EQ(analyzer.find_enum_case_by_scoped_name(
        intern_identifier(analyzer, "Choice"),
        "Choice",
        yes_id,
        "yes",
        {},
        true
    ), yes_case);

    const ExprId record_name = push_name(analyzer.module_, "Record");
    const ExprId record_case = push_field(analyzer.module_, record_name, "case");
    const ExprId choice_name = push_name(analyzer.module_, "Choice");
    const ExprId choice_missing = push_field(analyzer.module_, choice_name, "missing");
    const ExprId choice_yes = push_field(analyzer.module_, choice_name, "yes");
    const ExprId payload_value = push_name(analyzer.module_, "payload");
    const ExprId enum_call_id = push_call(analyzer.module_, choice_yes, {payload_value});

    analyzer.checked_.expr_types.assign(analyzer.module_.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.checked_.expr_c_name_ids.assign(analyzer.module_.exprs.size(), sema::INVALID_IDENT_ID);
    static_cast<void>(add_global_value(
        analyzer,
        module_id(SEMA_TEST_ROOT_MODULE_INDEX),
        "payload",
        array_i32,
        SymbolKind::local
    ));

    EXPECT_EQ(analyzer.find_enum_constructor(record_case, true), nullptr);
    EXPECT_EQ(analyzer.find_enum_constructor(choice_missing, true), nullptr);
    EXPECT_NE(analyzer.find_enum_constructor(choice_yes, true), nullptr);
    EXPECT_TRUE(types.same(
        analyzer.analyze_enum_constructor_call(enum_call_id, analyzer.expr_view(enum_call_id), *yes_case),
        enum_type
    ));

    const ExprId argument_call_id = push_call(analyzer.module_, syntax::INVALID_EXPR_ID, {payload_value});
    const std::vector<TypeHandle> array_param_types {array_i32};
    analyzer.validate_call_arguments(analyzer.expr_view(argument_call_id), "array_arg", array_param_types, 0, false);
    const ExprId variadic_argument_call_id =
        push_call(analyzer.module_, syntax::INVALID_EXPR_ID, {payload_value, payload_value});
    const std::vector<TypeHandle> no_param_types;
    analyzer.validate_call_arguments(analyzer.expr_view(variadic_argument_call_id), "array_vararg", no_param_types, 0, true);
}

TEST(CoreUnit, SemanticWhiteBoxTypedLookupIndexesCoverHotPaths) {
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"lib"}),
    };
    module.modules[SEMA_TEST_ROOT_MODULE_INDEX].imports = {
        resolved_import(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "lib"),
    };

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(module, diagnostics);
    analyzer.current_module_ = module_id(SEMA_TEST_ROOT_MODULE_INDEX);
    sema::TypeTable& types = analyzer.checked_.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle record_type = types.named_struct("lib.Record", "lib_Record", false);
    const TypeHandle enum_type = types.named_enum("lib.Choice", "lib_Choice");
    types.set_enum_underlying(enum_type, i32);

    const IdentId record_id = intern_identifier(analyzer, "Record");
    const IdentId alias_id = intern_identifier(analyzer, "Alias");
    const IdentId box_id = intern_identifier(analyzer, "Box");
    const IdentId make_id = intern_identifier(analyzer, "make");
    const IdentId generic_method_id = intern_identifier(analyzer, "generic_method");
    const IdentId run_id = intern_identifier(analyzer, "run");
    const IdentId method_id = intern_identifier(analyzer, "method");
    const IdentId value_id = intern_identifier(analyzer, "VALUE");
    const IdentId choice_yes_id = intern_identifier(analyzer, "Choice_yes");
    const IdentId yes_id = intern_identifier(analyzer, "yes");
    const IdentId missing_id = intern_identifier(analyzer, "missing");

    const sema::ModuleLookupKey record_key = add_named_type(
        analyzer,
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX),
        "Record",
        record_type
    );
    StructInfo record_info;
    record_info.name = "Record";
    record_info.name_id = record_id;
    record_info.module = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    record_info.type = record_type;
    const auto record_inserted = analyzer.checked_.structs.emplace(record_key, record_info);
    ASSERT_TRUE(record_inserted.second);
    analyzer.struct_infos_by_type_[record_type.value] = &record_inserted.first->second;

    const TypeId i32_type_id = analyzer.module_.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    sema::TypeAliasInfo alias;
    alias.name = "Alias";
    alias.name_id = alias_id;
    alias.module = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    alias.target = i32_type_id;
    alias.visibility = syntax::Visibility::public_;
    const auto alias_inserted = analyzer.checked_.type_aliases.emplace(
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "Alias"),
        alias
    );
    ASSERT_TRUE(alias_inserted.second);
    analyzer.index_type_alias(alias_inserted.first->second);

    sema::SemanticAnalyzer::GenericTemplateInfo generic_type;
    generic_type.module = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    generic_type.name = "Box";
    generic_type.name_id = box_id;
    generic_type.key = semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "Box");
    generic_type.visibility = syntax::Visibility::public_;
    const auto generic_type_inserted = analyzer.generic_struct_templates_.emplace(generic_type.key, generic_type);
    ASSERT_TRUE(generic_type_inserted.second);
    analyzer.index_generic_struct_template(generic_type_inserted.first->second);

    sema::SemanticAnalyzer::GenericTemplateInfo generic_function;
    generic_function.module = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    generic_function.name = "make";
    generic_function.name_id = make_id;
    generic_function.key = semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "make");
    generic_function.visibility = syntax::Visibility::public_;
    const auto generic_function_inserted = analyzer.generic_function_templates_.emplace(
        generic_function.key,
        generic_function
    );
    ASSERT_TRUE(generic_function_inserted.second);
    analyzer.index_generic_function_template(generic_function_inserted.first->second);

    sema::SemanticAnalyzer::GenericTemplateInfo generic_method;
    generic_method.module = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    generic_method.name = "generic_method";
    generic_method.name_id = generic_method_id;
    generic_method.key = semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "generic_method");
    generic_method.function_key = semantic_method_key(
        analyzer,
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX),
        record_type,
        "generic_method"
    );
    generic_method.impl_type_pattern = record_type;
    generic_method.visibility = syntax::Visibility::public_;
    const auto generic_method_inserted = analyzer.generic_method_templates_.emplace(
        generic_method.function_key,
        generic_method
    );
    ASSERT_TRUE(generic_method_inserted.second);
    analyzer.index_generic_method_template(generic_method_inserted.first->second);

    FunctionSignature run = indexed_function_signature(analyzer, "run", module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), i32);
    run.visibility = syntax::Visibility::public_;
    run.semantic_key = semantic_function_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "run");
    const auto run_inserted = analyzer.checked_.functions.emplace(
        run.semantic_key,
        run
    );
    ASSERT_TRUE(run_inserted.second);
    analyzer.index_function_lookup(run_inserted.first->second);

    FunctionSignature method = indexed_function_signature(analyzer, "method", module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), i32);
    method.is_method = true;
    method.has_self_param = true;
    method.method_owner_type = record_type;
    method.visibility = syntax::Visibility::public_;
    method.semantic_key = semantic_method_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), record_type, "method");
    const auto method_inserted = analyzer.checked_.functions.emplace(
        method.semantic_key,
        method
    );
    ASSERT_TRUE(method_inserted.second);
    analyzer.index_method_lookup(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), record_type, method_id, method_inserted.first->second);

    const auto value_inserted = analyzer.global_values_.emplace(
        semantic_function_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "VALUE"),
        indexed_symbol(analyzer, SymbolKind::const_, "VALUE", module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), i32)
    );
    ASSERT_TRUE(value_inserted.second);
    analyzer.index_global_value(value_inserted.first->second);

    EnumCaseInfo case_info;
    case_info.name = "Choice_yes";
    case_info.name_id = choice_yes_id;
    case_info.case_name = "yes";
    case_info.case_name_id = yes_id;
    case_info.module = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    case_info.type = enum_type;
    const auto case_inserted = analyzer.checked_.enum_cases.emplace(
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "Choice_yes"),
        case_info
    );
    ASSERT_TRUE(case_inserted.second);
    analyzer.index_enum_case(case_inserted.first->second);

    EXPECT_TRUE(analyzer.named_type_lookup_complete());
    EXPECT_TRUE(analyzer.type_alias_lookup_complete());
    EXPECT_TRUE(analyzer.generic_struct_lookup_complete());
    EXPECT_TRUE(analyzer.generic_function_lookup_complete());
    EXPECT_TRUE(analyzer.generic_method_lookup_complete());
    EXPECT_TRUE(analyzer.function_lookup_complete());
    EXPECT_TRUE(analyzer.global_value_lookup_complete());
    EXPECT_TRUE(analyzer.enum_case_module_lookup_complete());

    EXPECT_TRUE(analyzer.top_level_value_name_exists(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), value_id, "VALUE"));
    EXPECT_TRUE(analyzer.module_type_or_value_name_exists(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), record_id, "Record"));
    EXPECT_TRUE(analyzer.module_type_or_value_name_exists(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), value_id, "VALUE"));
    EXPECT_TRUE(analyzer.visible_type_name_exists(record_id, "Record"));
    EXPECT_TRUE(types.same(analyzer.find_type_in_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), record_id, "Record", {}, false), record_type));
    EXPECT_TRUE(types.same(analyzer.find_type_in_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), alias_id, "Alias", {}, false), i32));
    EXPECT_EQ(
        analyzer.find_any_generic_type_template_in_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), box_id, "Box"),
        &generic_type_inserted.first->second
    );
    EXPECT_EQ(analyzer.find_generic_function_in_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), make_id, "make", {}), &generic_function_inserted.first->second);
    EXPECT_EQ(analyzer.find_function_in_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), run_id, "run", {}), &run_inserted.first->second);
    EXPECT_EQ(analyzer.find_method_in_visible_modules(record_type, method_id, "method", {}, true), &method_inserted.first->second);
    EXPECT_EQ(analyzer.find_symbol_in_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), value_id, "VALUE", {}), &value_inserted.first->second);

    analyzer.current_module_ = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    EXPECT_EQ(analyzer.find_enum_case_in_visible_modules(choice_yes_id, "Choice_yes", {}), &case_inserted.first->second);
    const base::usize interned_before_miss = analyzer.module_.identifiers.size();
    EXPECT_EQ(analyzer.find_symbol_in_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), missing_id, "missing", {}, false), nullptr);
    EXPECT_EQ(analyzer.find_function_in_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), missing_id, "missing", {}, false), nullptr);
    EXPECT_EQ(analyzer.module_.identifiers.size(), interned_before_miss);
}

TEST(CoreUnit, SemanticWhiteBoxTypedLookupRejectsUnindexedLegacyMaps) {
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"lib"}),
    };
    module.modules[SEMA_TEST_ROOT_MODULE_INDEX].imports = {
        resolved_import(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "lib"),
    };

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(module, diagnostics);
    analyzer.current_module_ = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    sema::TypeTable& types = analyzer.checked_.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle record_type = types.named_struct("lib.LegacyRecord", "lib_LegacyRecord", false);
    const TypeHandle enum_type = types.named_enum("lib.LegacyChoice", "lib_LegacyChoice");
    types.set_enum_underlying(enum_type, i32);

    const IdentId legacy_record_id = intern_identifier(analyzer, "LegacyRecord");
    const IdentId legacy_alias_id = intern_identifier(analyzer, "LegacyAlias");
    const IdentId legacy_box_id = intern_identifier(analyzer, "LegacyBox");
    const IdentId private_box_id = intern_identifier(analyzer, "PrivateBox");
    const IdentId legacy_enum_id = intern_identifier(analyzer, "LegacyEnum");
    const IdentId legacy_alias_box_id = intern_identifier(analyzer, "LegacyAliasBox");
    const IdentId legacy_make_id = intern_identifier(analyzer, "LegacyMake");
    const IdentId legacy_fn_id = intern_identifier(analyzer, "legacy_fn");
    const IdentId owner_method_id = intern_identifier(analyzer, "owner_method");
    const IdentId legacy_value_id = intern_identifier(analyzer, "LEGACY_VALUE");
    const IdentId legacy_choice_yes_id = intern_identifier(analyzer, "LegacyChoice_yes");
    const IdentId yes_id = intern_identifier(analyzer, "yes");
    const IdentId missing_id = intern_identifier(analyzer, "missing");

    const sema::ModuleLookupKey record_key =
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyRecord");
    analyzer.named_types_.emplace(record_key, record_type);

    StructInfo record_info;
    record_info.name = "LegacyRecord";
    record_info.name_id = legacy_record_id;
    record_info.module = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    record_info.type = record_type;
    const auto record_inserted = analyzer.checked_.structs.emplace(record_key, record_info);
    ASSERT_TRUE(record_inserted.second);
    analyzer.struct_infos_by_type_[record_type.value] = &record_inserted.first->second;

    const TypeId i32_type_id = analyzer.module_.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    sema::TypeAliasInfo alias;
    alias.name = "LegacyAlias";
    alias.name_id = legacy_alias_id;
    alias.module = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    alias.target = i32_type_id;
    alias.visibility = syntax::Visibility::public_;
    analyzer.checked_.type_aliases.emplace(
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyAlias"),
        alias
    );

    analyzer.generic_struct_templates_.emplace(
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyBox"),
        generic_template_info(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyBox")
    );
    analyzer.generic_struct_templates_.emplace(
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "PrivateBox"),
        generic_template_info(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "PrivateBox", syntax::Visibility::private_)
    );
    analyzer.generic_enum_templates_.emplace(
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyEnum"),
        generic_template_info(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyEnum")
    );
    analyzer.generic_type_alias_templates_.emplace(
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyAliasBox"),
        generic_template_info(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyAliasBox")
    );
    analyzer.generic_function_templates_.emplace(
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyMake"),
        generic_template_info(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyMake")
    );

    FunctionSignature fallback_function =
        indexed_function_signature(analyzer, "legacy_fn", module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), i32);
    fallback_function.visibility = syntax::Visibility::public_;
    fallback_function.semantic_key =
        semantic_function_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "legacy_fn");
    analyzer.checked_.functions.emplace(
        fallback_function.semantic_key,
        fallback_function
    );

    FunctionSignature owner_method =
        indexed_function_signature(analyzer, "owner_method", module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), i32);
    owner_method.is_method = true;
    owner_method.has_self_param = true;
    owner_method.method_owner_type = record_type;
    owner_method.visibility = syntax::Visibility::public_;
    owner_method.param_types = {record_type};
    owner_method.semantic_key =
        semantic_method_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), record_type, "owner_method");
    const auto owner_method_inserted = analyzer.checked_.functions.emplace(
        owner_method.semantic_key,
        owner_method
    );
    ASSERT_TRUE(owner_method_inserted.second);

    const auto value_inserted = analyzer.global_values_.emplace(
        semantic_function_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LEGACY_VALUE"),
        indexed_symbol(analyzer, SymbolKind::const_, "LEGACY_VALUE", module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), i32)
    );
    ASSERT_TRUE(value_inserted.second);

    EnumCaseInfo enum_case;
    enum_case.name = "LegacyChoice_yes";
    enum_case.name_id = legacy_choice_yes_id;
    enum_case.case_name = "yes";
    enum_case.case_name_id = yes_id;
    enum_case.module = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    enum_case.type = enum_type;
    const auto enum_case_inserted = analyzer.checked_.enum_cases.emplace(
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyChoice_yes"),
        enum_case
    );
    ASSERT_TRUE(enum_case_inserted.second);

    EXPECT_FALSE(analyzer.named_type_lookup_complete());
    EXPECT_FALSE(analyzer.type_alias_lookup_complete());
    EXPECT_FALSE(analyzer.generic_struct_lookup_complete());
    EXPECT_FALSE(analyzer.generic_enum_lookup_complete());
    EXPECT_FALSE(analyzer.generic_type_alias_lookup_complete());
    EXPECT_FALSE(analyzer.generic_function_lookup_complete());
    EXPECT_FALSE(analyzer.function_lookup_complete());
    EXPECT_FALSE(analyzer.global_value_lookup_complete());
    EXPECT_FALSE(analyzer.enum_case_module_lookup_complete());

    EXPECT_FALSE(analyzer.top_level_value_name_exists(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_value_id, "LEGACY_VALUE"));
    EXPECT_FALSE(analyzer.module_type_or_value_name_exists(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_record_id, "LegacyRecord"));
    EXPECT_FALSE(analyzer.visible_type_name_exists(legacy_alias_id, "LegacyAlias"));
    EXPECT_FALSE(is_valid(analyzer.find_type_in_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_alias_id, "LegacyAlias", {}, false)));
    EXPECT_FALSE(is_valid(analyzer.find_type_in_module(
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX),
        legacy_record_id,
        "LegacyRecord",
        {},
        false
    )));

    analyzer.current_module_ = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    EXPECT_EQ(analyzer.find_generic_struct_in_visible_modules(legacy_box_id, "LegacyBox", {}, false), nullptr);
    EXPECT_EQ(analyzer.find_generic_enum_in_visible_modules(legacy_enum_id, "LegacyEnum", {}, false), nullptr);
    EXPECT_EQ(analyzer.find_generic_type_alias_in_visible_modules(legacy_alias_box_id, "LegacyAliasBox", {}, false), nullptr);
    EXPECT_EQ(analyzer.find_generic_function_in_visible_modules(legacy_make_id, "LegacyMake", {}, false), nullptr);
    EXPECT_EQ(analyzer.find_function_in_visible_modules(legacy_fn_id, "legacy_fn", {}, false), nullptr);
    EXPECT_EQ(analyzer.find_method_in_owner_module(record_type, owner_method_id, "owner_method", true), nullptr);
    EXPECT_EQ(analyzer.find_symbol_in_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_value_id, "LEGACY_VALUE", {}, false), nullptr);
    EXPECT_EQ(analyzer.find_enum_case_in_visible_modules(legacy_choice_yes_id, "LegacyChoice_yes", {}, false), nullptr);

    analyzer.index_named_type(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_record_id, record_type, syntax::Visibility::public_);
    analyzer.index_type_alias(analyzer.checked_.type_aliases.find(
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyAlias")
    )->second);
    analyzer.index_generic_struct_template(analyzer.generic_struct_templates_.find(
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyBox")
    )->second);
    analyzer.index_generic_enum_template(analyzer.generic_enum_templates_.find(
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyEnum")
    )->second);
    analyzer.index_generic_type_alias_template(analyzer.generic_type_alias_templates_.find(
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyAliasBox")
    )->second);
    analyzer.index_generic_function_template(analyzer.generic_function_templates_.find(
        semantic_module_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "LegacyMake")
    )->second);
    analyzer.index_function_lookup(analyzer.checked_.functions.find(
        semantic_function_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "legacy_fn")
    )->second);
    analyzer.index_method_lookup(
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX),
        record_type,
        owner_method_id,
        owner_method_inserted.first->second
    );
    analyzer.index_global_value(value_inserted.first->second);
    analyzer.index_enum_case(enum_case_inserted.first->second);

    EXPECT_TRUE(analyzer.top_level_value_name_exists(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_value_id, "LEGACY_VALUE"));
    EXPECT_TRUE(analyzer.module_type_or_value_name_exists(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_record_id, "LegacyRecord"));
    EXPECT_TRUE(analyzer.visible_type_name_exists(legacy_alias_id, "LegacyAlias"));
    EXPECT_TRUE(types.same(
        analyzer.find_type_in_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_alias_id, "LegacyAlias", {}, false),
        i32
    ));
    EXPECT_NE(analyzer.find_generic_struct_in_visible_modules(legacy_box_id, "LegacyBox", {}, false), nullptr);
    EXPECT_NE(analyzer.find_generic_enum_in_visible_modules(legacy_enum_id, "LegacyEnum", {}, false), nullptr);
    EXPECT_NE(analyzer.find_generic_type_alias_in_visible_modules(legacy_alias_box_id, "LegacyAliasBox", {}, false), nullptr);
    EXPECT_NE(analyzer.find_generic_function_in_visible_modules(legacy_make_id, "LegacyMake", {}, false), nullptr);
    EXPECT_TRUE(analyzer.generic_type_template_exists_in_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_box_id, "LegacyBox"));
    EXPECT_TRUE(analyzer.report_generic_type_requires_args_if_visible(legacy_box_id, "LegacyBox", {}));
    analyzer.report_generic_type_template_in_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_box_id, "LegacyBox", {});
    analyzer.report_generic_type_template_in_module(syntax::INVALID_MODULE_ID, missing_id, "MissingGeneric", {});
    EXPECT_EQ(analyzer.find_generic_struct_in_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), private_box_id, "PrivateBox", {}, false), nullptr);
    EXPECT_EQ(analyzer.find_generic_function_in_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), missing_id, "missing", {}, false), nullptr);
    EXPECT_EQ(analyzer.find_function_in_visible_modules(legacy_fn_id, "legacy_fn", {}, false), &analyzer.checked_.functions.find(
        semantic_function_key(analyzer, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "legacy_fn")
    )->second);
    EXPECT_EQ(
        analyzer.find_method_in_owner_module(record_type, owner_method_id, "owner_method", true),
        &owner_method_inserted.first->second
    );
    EXPECT_EQ(analyzer.find_method_in_owner_module(record_type, missing_id, "missing", true), nullptr);
    EXPECT_EQ(analyzer.find_method_in_owner_module(INVALID_TYPE_HANDLE, owner_method_id, "owner_method", true), nullptr);
    EXPECT_EQ(
        analyzer.find_symbol_in_module(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), legacy_value_id, "LEGACY_VALUE", {}),
        &value_inserted.first->second
    );
    EXPECT_EQ(analyzer.find_symbol_in_module(syntax::INVALID_MODULE_ID, missing_id, "missing", {}), nullptr);
    EXPECT_EQ(analyzer.find_enum_case_in_visible_modules(legacy_choice_yes_id, "LegacyChoice_yes", {}), &enum_case_inserted.first->second);
}

TEST(CoreUnit, SemanticWhiteBoxBodyInferenceEdges) {
    syntax::AstModule module;
    module.modules = {module_info({"root"})};
    const TypeId plain_type_id = module.push_type(named_node("Plain"));

    syntax::StmtNode expr_stmt;
    expr_stmt.kind = syntax::StmtKind::expr;
    const syntax::StmtId expr_stmt_id = module.push_stmt(expr_stmt);
    syntax::ItemNode function;
    function.kind = syntax::ItemKind::fn_decl;
    function.name = "infer";
    function.body = expr_stmt_id;
    const syntax::ItemId function_item = module.push_item(function);
    module.item_modules[function_item.value] = module_id(0);

    const ExprId INVALID_EXPR_ID = module.push_invalid_expr({});

    syntax::StmtNode empty_return_stmt;
    empty_return_stmt.kind = syntax::StmtKind::return_;
    const syntax::StmtId empty_return_stmt_id = module.push_stmt(empty_return_stmt);

    syntax::StmtNode invalid_expr_return_stmt;
    invalid_expr_return_stmt.kind = syntax::StmtKind::return_;
    invalid_expr_return_stmt.return_value = INVALID_EXPR_ID;
    const syntax::StmtId invalid_expr_return_stmt_id = module.push_stmt(invalid_expr_return_stmt);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(module, diagnostics);
    analyzer.checked_.syntax_type_handles.assign(module.types.size(), INVALID_TYPE_HANDLE);
    analyzer.checked_.expr_types.assign(module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.checked_.stmt_local_types.assign(module.stmts.size(), INVALID_TYPE_HANDLE);
    analyzer.current_module_ = module_id(0);
    const TypeHandle i32 = analyzer.checked_.types.builtin(BuiltinType::i32);
    const TypeHandle ptr_i32 = analyzer.checked_.types.pointer(PointerMutability::const_, i32);
    const TypeHandle plain_type = analyzer.checked_.types.named_struct("Plain", "Plain", false);
    static_cast<void>(add_named_type(analyzer, module_id(0), "Plain", plain_type));

    const sema::FunctionLookupKey infer_key = semantic_function_key(analyzer, module_id(0), "infer");
    FunctionSignature conflict_signature = indexed_function_signature(analyzer, "infer", module_id(0), INVALID_TYPE_HANDLE);
    conflict_signature.semantic_key = infer_key;
    sema::SemanticAnalyzer::FunctionBodyState state = sema::SemanticAnalyzer::FunctionBodyState::analyzing;
    analyzer.analyze_function_body_with_signature(function, infer_key, conflict_signature, state);
    state = sema::SemanticAnalyzer::FunctionBodyState::analyzed;
    analyzer.analyze_function_body_with_signature(function, infer_key, conflict_signature, state);
    state = sema::SemanticAnalyzer::FunctionBodyState::not_started;
    conflict_signature.has_conflict = true;
    analyzer.analyze_function_body_with_signature(function, infer_key, conflict_signature, state);

    analyzer.analyze_block(syntax::INVALID_STMT_ID, i32, nullptr);
    analyzer.analyze_stmt(syntax::INVALID_STMT_ID, i32, nullptr);
    sema::SemanticAnalyzer::ReturnTypeInference inference;
    analyzer.finalize_inferred_return(function, infer_key, inference);

    sema::SemanticAnalyzer::ReturnTypeInference invalid_pending_null_return;
    invalid_pending_null_return.inferred_type = ptr_i32;
    invalid_pending_null_return.pending_null_returns.push_back(syntax::INVALID_STMT_ID);
    analyzer.resolve_pending_null_returns(invalid_pending_null_return);

    sema::SemanticAnalyzer::ReturnTypeInference empty_pending_null_return;
    empty_pending_null_return.inferred_type = ptr_i32;
    empty_pending_null_return.pending_null_returns.push_back(empty_return_stmt_id);
    analyzer.resolve_pending_null_returns(empty_pending_null_return);

    sema::SemanticAnalyzer::ReturnTypeInference invalid_expr_pending_null_return;
    invalid_expr_pending_null_return.inferred_type = ptr_i32;
    invalid_expr_pending_null_return.pending_null_returns.push_back(invalid_expr_return_stmt_id);
    analyzer.resolve_pending_null_returns(invalid_expr_pending_null_return);
    analyzer.report_return_inference_diagnostic(syntax::INVALID_STMT_ID, "ignored diagnostic");

    conflict_signature.has_conflict = false;
    analyzer.ensure_function_return_known(conflict_signature, {});
    EXPECT_FALSE(is_valid(analyzer.analyze_expr(syntax::INVALID_EXPR_ID)));
    EXPECT_FALSE(is_valid(analyzer.analyze_expr(INVALID_EXPR_ID)));
    EXPECT_FALSE(is_valid(analyzer.resolve_type(syntax::INVALID_TYPE_ID)));

    analyzer.checked_.syntax_type_handles[plain_type_id.value] = plain_type;
    EXPECT_TRUE(analyzer.checked_.types.same(analyzer.resolve_type(plain_type_id), plain_type));
    const TypeHandle opaque = analyzer.checked_.types.opaque_struct("Opaque", "Opaque");
    analyzer.checked_.syntax_type_handles[plain_type_id.value] = opaque;
    EXPECT_TRUE(analyzer.checked_.types.same(analyzer.resolve_type(plain_type_id), opaque));
    analyzer.checked_.syntax_type_handles[plain_type_id.value] = INVALID_TYPE_HANDLE;
}

TEST(CoreUnit, SemanticWhiteBoxPostfixMaterializationEdges) {
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"lib"}),
    };
    module.modules[0].imports = {
        resolved_import(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "lib"),
    };

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(module, diagnostics);
    analyzer.current_module_ = module_id(0);
    auto sync_side_tables = [&analyzer] {
        analyzer.checked_.expr_types.resize(analyzer.module_.exprs.size(), INVALID_TYPE_HANDLE);
        analyzer.checked_.expr_c_name_ids.resize(analyzer.module_.exprs.size(), sema::INVALID_IDENT_ID);
        analyzer.checked_.syntax_type_handles.resize(analyzer.module_.types.size(), INVALID_TYPE_HANDLE);
    };

    sema::TypeTable& types = analyzer.checked_.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle record_type = types.named_struct("lib.Record", "lib_Record", false);
    static_cast<void>(add_named_type(analyzer, module_id(0), "Concrete", record_type));
    static_cast<void>(add_named_type(
        analyzer,
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX),
        "Record",
        record_type
    ));

    const auto generic_struct = generic_template_info(
        analyzer,
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX),
        "GenericStruct"
    );
    const auto generic_struct_inserted =
        analyzer.generic_struct_templates_.emplace(generic_struct.key, generic_struct);
    ASSERT_TRUE(generic_struct_inserted.second);
    analyzer.index_generic_struct_template(generic_struct_inserted.first->second);

    const auto generic_enum = generic_template_info(
        analyzer,
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX),
        "GenericEnum"
    );
    const auto generic_enum_inserted =
        analyzer.generic_enum_templates_.emplace(generic_enum.key, generic_enum);
    ASSERT_TRUE(generic_enum_inserted.second);
    analyzer.index_generic_enum_template(generic_enum_inserted.first->second);

    const auto generic_alias = generic_template_info(
        analyzer,
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX),
        "GenericAlias"
    );
    const auto generic_alias_inserted =
        analyzer.generic_type_alias_templates_.emplace(generic_alias.key, generic_alias);
    ASSERT_TRUE(generic_alias_inserted.second);
    analyzer.index_generic_type_alias_template(generic_alias_inserted.first->second);

    static_cast<void>(add_function(analyzer, indexed_function_signature(analyzer, "plain", module_id(0), i32)));

    const TypeId i32_type_id = analyzer.module_.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const ExprId value_expr = push_name(analyzer.module_, "value");
    const ExprId one_expr = push_integer(analyzer.module_);
    const ExprId two_expr = push_integer_text(analyzer.module_, "2");
    const ExprId t_name = push_name(analyzer.module_, "T");
    EXPECT_TRUE(analyzer.symbols_.insert(indexed_symbol(analyzer, SymbolKind::local, "value", module_id(0), i32), diagnostics));
    sync_side_tables();

    EXPECT_EQ(analyzer.materialize_postfix_chain(syntax::INVALID_EXPR_ID).value, syntax::INVALID_EXPR_ID.value);
    EXPECT_EQ(analyzer.materialize_postfix_chain(value_expr).value, value_expr.value);

    const ExprId empty_chain = push_postfix_chain(analyzer.module_, one_expr, {});
    sync_side_tables();
    EXPECT_TRUE(types.same(analyzer.analyze_postfix_chain_expr(empty_chain, INVALID_TYPE_HANDLE), i32));

    const ExprId invalid_base_chain =
        push_postfix_chain(analyzer.module_, syntax::INVALID_EXPR_ID, {select_op("missing")});
    const ExprId empty_select_chain = push_postfix_chain(analyzer.module_, value_expr, {select_op({})});
    const ExprId inner_chain = push_postfix_chain(analyzer.module_, value_expr, {select_op("field")});
    const ExprId outer_chain = push_postfix_chain(analyzer.module_, inner_chain, {call_op()});
    const ExprId multi_index_chain = push_postfix_chain(
        analyzer.module_,
        value_expr,
        {bracket_op({bracket_expr_arg(one_expr), bracket_expr_arg(two_expr)})}
    );
    sync_side_tables();

    EXPECT_EQ(analyzer.materialize_postfix_chain(invalid_base_chain).value, invalid_base_chain.value);
    EXPECT_EQ(analyzer.module_.exprs.kind(invalid_base_chain.value), syntax::ExprKind::field);
    EXPECT_EQ(analyzer.materialize_postfix_chain(empty_select_chain).value, empty_select_chain.value);
    EXPECT_EQ(analyzer.module_.exprs.kind(empty_select_chain.value), syntax::ExprKind::invalid);
    EXPECT_EQ(analyzer.materialize_postfix_chain(outer_chain).value, outer_chain.value);
    ASSERT_EQ(analyzer.module_.exprs.kind(outer_chain.value), syntax::ExprKind::call);
    const syntax::CallExprPayload* const outer_call = analyzer.module_.exprs.call_payload(outer_chain.value);
    ASSERT_NE(outer_call, nullptr);
    ASSERT_TRUE(syntax::is_valid(outer_call->callee));
    EXPECT_EQ(
        analyzer.module_.exprs.kind(outer_call->callee.value),
        syntax::ExprKind::field
    );
    EXPECT_EQ(analyzer.materialize_postfix_chain(multi_index_chain).value, multi_index_chain.value);
    EXPECT_EQ(analyzer.module_.exprs.kind(multi_index_chain.value), syntax::ExprKind::index);

    const syntax::PostfixOp unresolved_type_bracket = bracket_op({bracket_expr_arg(t_name)});
    const syntax::PostfixOp concrete_type_bracket = bracket_op({bracket_type_arg(i32_type_id)});
    EXPECT_FALSE(analyzer.postfix_bracket_is_generic_apply(value_expr, slice_op(one_expr, two_expr), nullptr));
    EXPECT_FALSE(analyzer.postfix_bracket_is_generic_apply(syntax::INVALID_EXPR_ID, unresolved_type_bracket, nullptr));
    EXPECT_TRUE(analyzer.postfix_bracket_is_generic_apply(value_expr, concrete_type_bracket, nullptr));

    const ExprId concrete_name = push_name(analyzer.module_, "Concrete");
    const ExprId plain_function_name = push_name(analyzer.module_, "plain");
    const ExprId value_member = push_field(analyzer.module_, value_expr, "member");
    const ExprId lib_name_for_struct = push_name(analyzer.module_, "lib");
    const ExprId lib_struct = push_field(analyzer.module_, lib_name_for_struct, "GenericStruct");
    const ExprId lib_name_for_enum = push_name(analyzer.module_, "lib");
    const ExprId lib_enum = push_field(analyzer.module_, lib_name_for_enum, "GenericEnum");
    const ExprId lib_name_for_alias = push_name(analyzer.module_, "lib");
    const ExprId lib_alias = push_field(analyzer.module_, lib_name_for_alias, "GenericAlias");
    sync_side_tables();

    EXPECT_TRUE(analyzer.postfix_bracket_is_generic_apply(concrete_name, unresolved_type_bracket, nullptr));
    EXPECT_TRUE(analyzer.postfix_bracket_is_generic_apply(plain_function_name, unresolved_type_bracket, nullptr));
    EXPECT_FALSE(analyzer.postfix_bracket_is_generic_apply(value_member, unresolved_type_bracket, nullptr));
    EXPECT_TRUE(analyzer.postfix_bracket_is_generic_apply(lib_struct, unresolved_type_bracket, nullptr));
    EXPECT_TRUE(analyzer.postfix_bracket_is_generic_apply(lib_enum, unresolved_type_bracket, nullptr));
    EXPECT_TRUE(analyzer.postfix_bracket_is_generic_apply(lib_alias, unresolved_type_bracket, nullptr));

    const std::vector<TypeId> direct_type_args = analyzer.postfix_bracket_type_args(concrete_type_bracket);
    ASSERT_EQ(direct_type_args.size(), 1U);
    EXPECT_EQ(direct_type_args.front().value, i32_type_id.value);

    const ExprId lib_name_for_type_arg = push_name(analyzer.module_, "lib");
    const ExprId lib_record_field = push_field(analyzer.module_, lib_name_for_type_arg, "Record");
    const ExprId box_name = push_name(analyzer.module_, "Box");
    const ExprId box_apply = push_generic_apply(analyzer.module_, box_name, {i32_type_id});
    const ExprId ref_t = push_unary(analyzer.module_, syntax::UnaryOp::address_of, t_name);
    const ExprId mut_ref_t = push_unary(analyzer.module_, syntax::UnaryOp::address_of_mut, t_name);
    const ExprId selector_chain = push_postfix_chain(
        analyzer.module_,
        push_name(analyzer.module_, "lib"),
        {select_op("Record")}
    );
    const ExprId multi_selector_chain = push_postfix_chain(
        analyzer.module_,
        push_name(analyzer.module_, "core"),
        {select_op("mem"), select_op("File")}
    );
    const ExprId generic_selector_chain = push_postfix_chain(
        analyzer.module_,
        push_name(analyzer.module_, "Box"),
        {bracket_op({bracket_type_arg(i32_type_id)})}
    );
    sync_side_tables();

    const TypeId field_type = analyzer.postfix_arg_expr_to_type(lib_record_field);
    ASSERT_TRUE(syntax::is_valid(field_type));
    EXPECT_EQ(analyzer.module_.types[field_type.value].scope_name, "lib");
    EXPECT_EQ(analyzer.module_.types[field_type.value].name, "Record");

    const TypeId apply_type = analyzer.postfix_arg_expr_to_type(box_apply);
    ASSERT_TRUE(syntax::is_valid(apply_type));
    EXPECT_EQ(analyzer.module_.types[apply_type.value].name, "Box");
    ASSERT_EQ(analyzer.module_.types[apply_type.value].type_args.size(), 1U);
    EXPECT_EQ(analyzer.module_.types[apply_type.value].type_args.front().value, i32_type_id.value);

    const TypeId ref_type = analyzer.postfix_arg_expr_to_type(ref_t);
    ASSERT_TRUE(syntax::is_valid(ref_type));
    EXPECT_EQ(analyzer.module_.types[ref_type.value].kind, syntax::TypeKind::reference);
    EXPECT_EQ(analyzer.module_.types[ref_type.value].pointer_mutability, syntax::PointerMutability::const_);

    const TypeId mut_ref_type = analyzer.postfix_arg_expr_to_type(mut_ref_t);
    ASSERT_TRUE(syntax::is_valid(mut_ref_type));
    EXPECT_EQ(analyzer.module_.types[mut_ref_type.value].kind, syntax::TypeKind::reference);
    EXPECT_EQ(analyzer.module_.types[mut_ref_type.value].pointer_mutability, syntax::PointerMutability::mut);

    const TypeId selector_type = analyzer.postfix_chain_expr_to_type(selector_chain);
    ASSERT_TRUE(syntax::is_valid(selector_type));
    EXPECT_EQ(analyzer.module_.types[selector_type.value].scope_name, "lib");
    EXPECT_EQ(analyzer.module_.types[selector_type.value].name, "Record");

    const TypeId multi_selector_type = analyzer.postfix_chain_expr_to_type(multi_selector_chain);
    ASSERT_TRUE(syntax::is_valid(multi_selector_type));
    const syntax::TypeNode& multi_selector_node = analyzer.module_.types[multi_selector_type.value];
    EXPECT_EQ(multi_selector_node.scope_name, "core");
    ASSERT_EQ(multi_selector_node.scope_parts.size(), 2U);
    EXPECT_EQ(multi_selector_node.scope_parts[0], "core");
    EXPECT_EQ(multi_selector_node.scope_parts[1], "mem");
    EXPECT_EQ(multi_selector_node.name, "File");

    const TypeId generic_selector_type = analyzer.postfix_chain_expr_to_type(generic_selector_chain);
    ASSERT_TRUE(syntax::is_valid(generic_selector_type));
    EXPECT_EQ(analyzer.module_.types[generic_selector_type.value].name, "Box");
    ASSERT_EQ(analyzer.module_.types[generic_selector_type.value].type_args.size(), 1U);
    EXPECT_EQ(analyzer.module_.types[generic_selector_type.value].type_args.front().value, i32_type_id.value);

    EXPECT_FALSE(syntax::is_valid(analyzer.postfix_arg_expr_to_type(syntax::INVALID_EXPR_ID)));
    EXPECT_FALSE(syntax::is_valid(analyzer.postfix_chain_expr_to_type(syntax::INVALID_EXPR_ID)));

    const TypeId plain_name_type = analyzer.postfix_chain_expr_to_type(push_name(analyzer.module_, "Plain"));
    ASSERT_TRUE(syntax::is_valid(plain_name_type));
    EXPECT_EQ(analyzer.module_.types[plain_name_type.value].name, "Plain");

    const ExprId invalid_type_base_chain = push_postfix_chain(analyzer.module_, one_expr, {select_op("Type")});
    const ExprId invalid_type_op_chain = push_postfix_chain(
        analyzer.module_,
        push_name(analyzer.module_, "Plain"),
        {call_op()}
    );
    const ExprId invalid_object_field = push_field(analyzer.module_, syntax::INVALID_EXPR_ID, "bad");
    const ExprId non_name_object_field = push_field(analyzer.module_, one_expr, "bad");
    const ExprId invalid_apply = push_generic_apply(analyzer.module_, syntax::INVALID_EXPR_ID, {i32_type_id});
    const ExprId invalid_ref = push_unary(analyzer.module_, syntax::UnaryOp::address_of, syntax::INVALID_EXPR_ID);
    const ExprId not_ref = push_unary(analyzer.module_, syntax::UnaryOp::logical_not, t_name);
    sync_side_tables();

    EXPECT_FALSE(syntax::is_valid(analyzer.postfix_chain_expr_to_type(invalid_type_base_chain)));
    EXPECT_FALSE(syntax::is_valid(analyzer.postfix_chain_expr_to_type(invalid_type_op_chain)));
    EXPECT_FALSE(syntax::is_valid(analyzer.postfix_arg_expr_to_type(invalid_object_field)));
    EXPECT_FALSE(syntax::is_valid(analyzer.postfix_arg_expr_to_type(non_name_object_field)));
    EXPECT_FALSE(syntax::is_valid(analyzer.postfix_arg_expr_to_type(invalid_apply)));
    EXPECT_FALSE(syntax::is_valid(analyzer.postfix_arg_expr_to_type(invalid_ref)));
    EXPECT_FALSE(syntax::is_valid(analyzer.postfix_arg_expr_to_type(not_ref)));
    EXPECT_FALSE(syntax::is_valid(analyzer.postfix_arg_expr_to_type(one_expr)));
}

TEST(CoreUnit, SemanticWhiteBoxGenericTemplateNodeSpansTrackReachableAstOnly) {
    syntax::AstModule module;
    module.modules = {module_info({"generic_span"})};

    const TypeId generic_type = module.push_type(named_node("T"));
    const TypeId unused_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::f64));
    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));

    syntax::TypeNode pointer_type;
    pointer_type.kind = syntax::TypeKind::pointer;
    pointer_type.pointee = generic_type;
    const TypeId pointer_generic_type = module.push_type(pointer_type);

    syntax::TypeNode array_type;
    array_type.kind = syntax::TypeKind::array;
    array_type.array_count = SEMA_TEST_SMALL_ARRAY_COUNT;
    array_type.array_element = i32_type;
    const TypeId array_i32_type = module.push_type(array_type);

    syntax::TypeNode slice_type;
    slice_type.kind = syntax::TypeKind::slice;
    slice_type.slice_element = generic_type;
    const TypeId slice_generic_type = module.push_type(slice_type);

    syntax::TypeNode tuple_type;
    tuple_type.kind = syntax::TypeKind::tuple;
    tuple_type.tuple_elements = {i32_type, generic_type};
    const TypeId tuple_i32_generic_type = module.push_type(tuple_type);

    syntax::TypeNode function_type;
    function_type.kind = syntax::TypeKind::function;
    function_type.function_params = {pointer_generic_type, slice_generic_type, tuple_i32_generic_type};
    function_type.function_return = array_i32_type;
    const TypeId function_handle_type = module.push_type(function_type);

    syntax::TypeNode box_type_node = named_node("Box");
    box_type_node.type_args = {function_handle_type};
    const TypeId box_function_type = module.push_type(box_type_node);

    syntax::PatternNode binding_pattern;
    binding_pattern.kind = syntax::PatternKind::binding;
    binding_pattern.binding_name = "value";
    const syntax::PatternId binding_pattern_id = module.push_pattern(binding_pattern);

    syntax::PatternNode unused_pattern;
    unused_pattern.kind = syntax::PatternKind::wildcard;
    const syntax::PatternId unused_pattern_id = module.push_pattern(unused_pattern);

    syntax::PatternNode enum_pattern;
    enum_pattern.kind = syntax::PatternKind::enum_case;
    enum_pattern.enum_type = box_function_type;
    enum_pattern.payload_patterns = {binding_pattern_id};
    const syntax::PatternId enum_pattern_id = module.push_pattern(enum_pattern);

    syntax::PatternNode tuple_pattern;
    tuple_pattern.kind = syntax::PatternKind::tuple;
    tuple_pattern.elements = {binding_pattern_id, enum_pattern_id};
    const syntax::PatternId tuple_pattern_id = module.push_pattern(tuple_pattern);

    syntax::PatternNode slice_pattern;
    slice_pattern.kind = syntax::PatternKind::slice;
    slice_pattern.elements = {tuple_pattern_id};
    slice_pattern.has_slice_rest = true;
    const syntax::PatternId slice_pattern_id = module.push_pattern(slice_pattern);

    syntax::PatternNode struct_pattern;
    struct_pattern.kind = syntax::PatternKind::struct_;
    struct_pattern.struct_name = "Box";
    struct_pattern.field_patterns = {syntax::FieldPattern {"field", slice_pattern_id, {}}};
    const syntax::PatternId struct_pattern_id = module.push_pattern(struct_pattern);

    syntax::PatternNode literal_pattern;
    literal_pattern.kind = syntax::PatternKind::literal;
    literal_pattern.case_name = "1";
    const syntax::PatternId literal_pattern_id = module.push_pattern(literal_pattern);

    syntax::PatternNode or_pattern;
    or_pattern.kind = syntax::PatternKind::or_pattern;
    or_pattern.alternatives = {struct_pattern_id, literal_pattern_id};
    const syntax::PatternId or_pattern_id = module.push_pattern(or_pattern);

    const ExprId name_with_type_arg = module.push_name_expr({}, "value", std::vector<TypeId> {generic_type});
    const ExprId unused_expr = push_integer_text(module, "99");
    const ExprId callee = module.push_name_expr({}, "callee", std::vector<TypeId> {function_handle_type});
    const ExprId generic_apply = push_generic_apply(module, callee, {i32_type});
    const ExprId try_expr = module.push_unary_expr(syntax::ExprKind::try_expr, {}, syntax::UnaryOp::logical_not, generic_apply);
    const ExprId bool_expr = push_bool(module, "true");
    const ExprId binary_expr = push_binary(module, syntax::BinaryOp::add, name_with_type_arg, generic_apply);
    const ExprId call_expr = push_call(module, callee, {binary_expr, try_expr});

    syntax::CallExprPayload string_call_payload;
    string_call_payload.callee = callee;
    string_call_payload.args.assign({name_with_type_arg, generic_apply});
    const ExprId string_call_expr = module.push_call_expr(
        syntax::ExprKind::str_from_bytes_unchecked,
        {},
        std::move(string_call_payload)
    );

    const ExprId if_expr = module.push_if_expr({}, bool_expr, or_pattern_id, call_expr, string_call_expr);

    syntax::StmtNode inner_expr_stmt;
    inner_expr_stmt.kind = syntax::StmtKind::expr;
    inner_expr_stmt.init = binary_expr;
    const syntax::StmtId inner_expr_stmt_id = module.push_stmt(inner_expr_stmt);
    const syntax::StmtId inner_block_id = push_block(module, {inner_expr_stmt_id});
    const ExprId block_expr = module.push_block_expr(syntax::ExprKind::block_expr, {}, inner_block_id, if_expr);
    const ExprId unsafe_block_expr = module.push_block_expr(syntax::ExprKind::unsafe_block, {}, inner_block_id, block_expr);

    const ExprId match_guard_expr = push_bool(module, "false");
    const ExprId match_value_expr = push_integer(module);
    const ExprId match_expr = module.push_match_expr(
        {},
        name_with_type_arg,
        std::vector<syntax::MatchArm> {
            syntax::MatchArm {or_pattern_id, match_guard_expr, match_value_expr, {}},
        }
    );

    const ExprId array_expr = module.push_array_expr(
        {},
        std::vector<ExprId> {block_expr, unsafe_block_expr},
        match_value_expr,
        name_with_type_arg
    );
    const ExprId tuple_expr = module.push_tuple_expr({}, std::vector<ExprId> {array_expr, match_expr});

    syntax::PostfixOp postfix_op;
    postfix_op.kind = syntax::PostfixOpKind::struct_literal;
    postfix_op.bracket_args.assign({
        bracket_expr_arg(name_with_type_arg),
        bracket_type_arg(box_function_type),
    });
    postfix_op.slice_start = match_guard_expr;
    postfix_op.slice_end = match_value_expr;
    postfix_op.args.assign({call_expr});
    postfix_op.field_inits.assign({syntax::FieldInit {"field", tuple_expr, {}}});
    const ExprId postfix_expr = push_postfix_chain(module, tuple_expr, {postfix_op});
    const ExprId field_expr = push_field(module, postfix_expr, "field");
    const ExprId index_expr = module.push_index_expr({}, syntax::IndexExprPayload {field_expr, match_value_expr});
    const ExprId slice_expr = module.push_slice_expr({}, syntax::SliceExprPayload {index_expr, match_guard_expr, match_value_expr});
    const ExprId struct_literal_expr = module.push_struct_literal_expr(
        {},
        push_name(module, "Box"),
        {},
        {},
        "Box",
        std::vector<TypeId> {box_function_type},
        std::vector<syntax::FieldInit> {syntax::FieldInit {"field", slice_expr, {}}},
        syntax::INVALID_IDENT_ID,
        syntax::INVALID_IDENT_ID
    );
    const ExprId cast_expr = module.push_cast_like_expr(
        syntax::ExprKind::cast,
        {},
        syntax::CastExprPayload {i32_type, struct_literal_expr}
    );

    syntax::StmtNode unused_stmt;
    unused_stmt.kind = syntax::StmtKind::expr;
    unused_stmt.init = unused_expr;
    const syntax::StmtId unused_stmt_id = module.push_stmt(unused_stmt);

    syntax::StmtNode return_stmt;
    return_stmt.kind = syntax::StmtKind::return_;
    return_stmt.return_value = if_expr;
    const syntax::StmtId return_stmt_id = module.push_stmt(return_stmt);

    const syntax::StmtId then_block_id = push_block(module, {inner_expr_stmt_id});
    const syntax::StmtId else_block_id = push_block(module, {return_stmt_id});

    syntax::StmtNode else_if_stmt;
    else_if_stmt.kind = syntax::StmtKind::if_;
    else_if_stmt.condition = bool_expr;
    else_if_stmt.then_block = then_block_id;
    const syntax::StmtId else_if_stmt_id = module.push_stmt(else_if_stmt);

    syntax::StmtNode if_stmt;
    if_stmt.kind = syntax::StmtKind::if_;
    if_stmt.condition = call_expr;
    if_stmt.pattern = or_pattern_id;
    if_stmt.then_block = then_block_id;
    if_stmt.else_block = else_block_id;
    if_stmt.else_if = else_if_stmt_id;
    const syntax::StmtId if_stmt_id = module.push_stmt(if_stmt);

    syntax::StmtNode let_stmt;
    let_stmt.kind = syntax::StmtKind::let;
    let_stmt.pattern = struct_pattern_id;
    let_stmt.declared_type = box_function_type;
    let_stmt.init = struct_literal_expr;
    let_stmt.else_block = else_block_id;
    const syntax::StmtId let_stmt_id = module.push_stmt(let_stmt);

    syntax::StmtNode assign_stmt;
    assign_stmt.kind = syntax::StmtKind::assign;
    assign_stmt.lhs = field_expr;
    assign_stmt.rhs = cast_expr;
    const syntax::StmtId assign_stmt_id = module.push_stmt(assign_stmt);

    syntax::StmtNode for_init_stmt;
    for_init_stmt.kind = syntax::StmtKind::expr;
    for_init_stmt.init = array_expr;
    const syntax::StmtId for_init_stmt_id = module.push_stmt(for_init_stmt);

    syntax::StmtNode for_update_stmt;
    for_update_stmt.kind = syntax::StmtKind::expr;
    for_update_stmt.init = tuple_expr;
    const syntax::StmtId for_update_stmt_id = module.push_stmt(for_update_stmt);
    const syntax::StmtId loop_body_id = push_block(module, {assign_stmt_id});

    syntax::StmtNode for_stmt;
    for_stmt.kind = syntax::StmtKind::for_;
    for_stmt.for_init = for_init_stmt_id;
    for_stmt.condition = bool_expr;
    for_stmt.for_update = for_update_stmt_id;
    for_stmt.body = loop_body_id;
    const syntax::StmtId for_stmt_id = module.push_stmt(for_stmt);

    syntax::StmtNode for_range_stmt;
    for_range_stmt.kind = syntax::StmtKind::for_range;
    for_range_stmt.range_start = name_with_type_arg;
    for_range_stmt.range_end = match_value_expr;
    for_range_stmt.range_step = match_guard_expr;
    for_range_stmt.body = loop_body_id;
    const syntax::StmtId for_range_stmt_id = module.push_stmt(for_range_stmt);

    syntax::StmtNode while_stmt;
    while_stmt.kind = syntax::StmtKind::while_;
    while_stmt.condition = bool_expr;
    while_stmt.pattern = or_pattern_id;
    while_stmt.body = loop_body_id;
    const syntax::StmtId while_stmt_id = module.push_stmt(while_stmt);

    syntax::StmtNode defer_stmt;
    defer_stmt.kind = syntax::StmtKind::defer;
    defer_stmt.init = match_expr;
    const syntax::StmtId defer_stmt_id = module.push_stmt(defer_stmt);

    const syntax::StmtId body_id = push_block(
        module,
        {
            let_stmt_id,
            assign_stmt_id,
            if_stmt_id,
            for_stmt_id,
            for_range_stmt_id,
            while_stmt_id,
            defer_stmt_id,
            return_stmt_id,
        }
    );

    syntax::ItemNode generic_function;
    generic_function.kind = syntax::ItemKind::fn_decl;
    generic_function.name = "span";
    generic_function.generic_params = {syntax::GenericParamDecl {"T", {}}};
    generic_function.params = {syntax::ParamDecl {"param", box_function_type, {}}};
    generic_function.return_type = function_handle_type;
    generic_function.impl_type = pointer_generic_type;
    generic_function.body = body_id;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(module, diagnostics);
    sema::SemanticAnalyzer::GenericTemplateInfo info = analyzer.make_generic_template_info();
    analyzer.populate_generic_template_node_spans(info, generic_function);

    const auto contains_id = [](const sema::SemaIndexTable& ids, const base::u32 value) {
        return std::ranges::find(ids, value) != ids.end();
    };
    EXPECT_TRUE(info.expr_span.contains(cast_expr.value));
    EXPECT_TRUE(info.pattern_span.contains(or_pattern_id.value));
    EXPECT_TRUE(info.type_span.contains(box_function_type.value));
    EXPECT_TRUE(info.stmt_span.contains(body_id.value));
    ASSERT_FALSE(info.expr_node_ids.empty());
    ASSERT_FALSE(info.pattern_node_ids.empty());
    ASSERT_FALSE(info.type_node_ids.empty());
    ASSERT_FALSE(info.stmt_node_ids.empty());
    EXPECT_FALSE(contains_id(info.expr_node_ids, unused_expr.value));
    EXPECT_FALSE(contains_id(info.pattern_node_ids, unused_pattern_id.value));
    EXPECT_FALSE(contains_id(info.type_node_ids, unused_type.value));
    EXPECT_FALSE(contains_id(info.stmt_node_ids, unused_stmt_id.value));

    sema::GenericSideTables side_tables;
    side_tables.configure_local_dense(
        info.expr_span,
        info.pattern_span,
        info.type_span,
        info.stmt_span,
        info.expr_node_ids,
        info.pattern_node_ids,
        info.type_node_ids,
        info.stmt_node_ids
    );
    EXPECT_EQ(side_tables.local_expr_index(unused_expr), sema::SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX);
    EXPECT_EQ(side_tables.local_pattern_index(unused_pattern_id), sema::SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX);
    EXPECT_EQ(side_tables.local_type_index(unused_type), sema::SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX);
    EXPECT_EQ(side_tables.local_stmt_index(unused_stmt_id), sema::SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX);
    EXPECT_EQ(side_tables.expr_types.size(), info.expr_node_ids.size());
    EXPECT_EQ(side_tables.pattern_c_name_ids.size(), info.pattern_node_ids.size());
    EXPECT_EQ(side_tables.syntax_type_handles.size(), info.type_node_ids.size());
    EXPECT_EQ(side_tables.stmt_local_types.size(), info.stmt_node_ids.size());
}

TEST(CoreUnit, SemanticWhiteBoxGenericTemplateNodeSpansSwitchToHashDedupForLargeBodies) {
    syntax::AstModule module;
    module.modules = {module_info({"generic_large_span"})};

    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    std::vector<ExprId> values;
    values.reserve(SEMA_TEST_LARGE_GENERIC_SPAN_EXPR_COUNT);
    for (base::usize index = 0; index < SEMA_TEST_LARGE_GENERIC_SPAN_EXPR_COUNT; ++index) {
        values.push_back(push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_ONE));
    }
    values.push_back(values.front());

    const ExprId tuple_expr = module.push_tuple_expr({}, values);
    syntax::StmtNode return_stmt;
    return_stmt.kind = syntax::StmtKind::return_;
    return_stmt.return_value = tuple_expr;
    const syntax::StmtId return_stmt_id = module.push_stmt(return_stmt);
    const syntax::StmtId body_id = push_block(module, {return_stmt_id});

    syntax::ItemNode generic_function;
    generic_function.kind = syntax::ItemKind::fn_decl;
    generic_function.name = "large_span";
    generic_function.generic_params = {syntax::GenericParamDecl {"T", {}}};
    generic_function.return_type = i32_type;
    generic_function.body = body_id;

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(module, diagnostics);
    sema::SemanticAnalyzer::GenericTemplateInfo info = analyzer.make_generic_template_info();
    analyzer.populate_generic_template_node_spans(info, generic_function);

    EXPECT_TRUE(info.expr_span.contains(tuple_expr.value));
    EXPECT_GE(info.expr_span.count, SEMA_TEST_LARGE_GENERIC_SPAN_EXPR_COUNT);
    EXPECT_TRUE(info.expr_node_ids.empty());
}

TEST(CoreUnit, SemanticWhiteBoxGenericInstancesUseLocalDenseSideTables) {
    syntax::AstModule module;
    module.modules = {module_info({"generic_sparse"})};

    const TypeId generic_type = module.push_type(named_node("T"));
    syntax::TypeNode i32_type_node = primitive_node(syntax::PrimitiveTypeKind::i32);
    const TypeId i32_type = module.push_type(i32_type_node);

    const ExprId value = push_name(module, "value");

    syntax::StmtNode return_stmt;
    return_stmt.kind = syntax::StmtKind::return_;
    return_stmt.return_value = value;
    const syntax::StmtId return_stmt_id = module.push_stmt(return_stmt);
    const syntax::StmtId body = push_block(module, {return_stmt_id});

    syntax::ItemNode id_function;
    id_function.kind = syntax::ItemKind::fn_decl;
    id_function.name = "id";
    id_function.generic_params = {syntax::GenericParamDecl {"T", {}}};
    id_function.params = {syntax::ParamDecl {"value", generic_type, {}}};
    id_function.return_type = generic_type;
    id_function.body = body;
    const syntax::ItemId id_item = module.push_item(id_function);
    module.item_modules[id_item.value] = module_id(0);

    const ExprId call_callee = push_name(module, "id");
    const ExprId call_arg = push_integer(module);
    const ExprId call = push_call(module, call_callee, {call_arg});

    syntax::StmtNode main_return;
    main_return.kind = syntax::StmtKind::return_;
    main_return.return_value = call;
    const syntax::StmtId main_return_id = module.push_stmt(main_return);
    const syntax::StmtId main_body = push_block(module, {main_return_id});

    syntax::ItemNode main_function;
    main_function.kind = syntax::ItemKind::fn_decl;
    main_function.name = "main";
    main_function.return_type = i32_type;
    main_function.body = main_body;
    const syntax::ItemId main_item = module.push_item(main_function);
    module.item_modules[main_item.value] = module_id(0);

    syntax::AstModule discard_side_tables_module = module;
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(std::move(module), diagnostics);
    auto checked_result = analyzer.analyze();
    ASSERT_TRUE(checked_result) << checked_result.error().message;
    const sema::CheckedModule& checked = checked_result.value();
    ASSERT_EQ(checked.generic_function_instances.size(), 1U);
    const sema::FunctionSignature& signature = checked.generic_function_instances.front().signature;
    EXPECT_EQ(signature.name, "id");
    EXPECT_TRUE(sema::is_valid(signature.semantic_key));
    ASSERT_EQ(signature.generic_args.size(), 1U);
    EXPECT_TRUE(checked.types.same(signature.generic_args.front(), checked.types.builtin(sema::BuiltinType::i32)));
    EXPECT_EQ(sema::function_display_name(checked.types, signature), "id[i32]");
    EXPECT_TRUE(checked.functions.contains(signature.semantic_key));
    EXPECT_NE(sema::dump_checked_module(checked).find("id[i32]"), std::string::npos);

    const sema::GenericSideTables& side_tables = checked.generic_function_instances.front().side_tables;
    EXPECT_TRUE(side_tables.sparse);
    EXPECT_TRUE(side_tables.local_dense);
    EXPECT_TRUE(side_tables.expr_span.contains(value.value));
    EXPECT_TRUE(side_tables.stmt_span.contains(return_stmt_id.value));
    EXPECT_TRUE(checked.generic_side_table_layouts.empty());
    EXPECT_EQ(
        checked.generic_function_instances.front().side_table_layout_index,
        sema::SEMA_GENERIC_SIDE_TABLE_INVALID_LAYOUT_INDEX
    );
    EXPECT_EQ(side_tables.layout, nullptr);
    EXPECT_EQ(side_tables.expr_types.size(), side_tables.expr_span.count);
    EXPECT_TRUE(side_tables.expr_expected_types.empty());
    EXPECT_EQ(side_tables.expr_c_name_ids.size(), side_tables.expr_span.count);
    EXPECT_EQ(side_tables.pattern_c_name_ids.size(), side_tables.pattern_span.count);
    EXPECT_TRUE(side_tables.pattern_case_name_ids.empty());
    EXPECT_EQ(side_tables.syntax_type_handles.size(), side_tables.type_span.count);
    EXPECT_EQ(side_tables.stmt_local_types.size(), side_tables.stmt_span.count);
    const base::usize value_local = side_tables.local_expr_index(value);
    ASSERT_NE(value_local, sema::SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX);
    ASSERT_LT(value_local, side_tables.expr_types.size());
    EXPECT_TRUE(sema::is_valid(side_tables.expr_types[value_local]));
    EXPECT_TRUE(side_tables.sparse_expr_types.empty());
    EXPECT_TRUE(side_tables.sparse_expr_expected_types.empty());
    EXPECT_TRUE(side_tables.sparse_fallbacks.empty());
    EXPECT_TRUE(checked.expr_expected_types.empty());
    EXPECT_TRUE(checked.pattern_case_name_ids.empty());

    sema::SemanticOptions discard_options;
    discard_options.retain_generic_side_tables = false;
    base::DiagnosticSink discard_diagnostics;
    sema::SemanticAnalyzer discard_analyzer(
        std::move(discard_side_tables_module),
        discard_diagnostics,
        discard_options
    );
    auto discard_result = discard_analyzer.analyze();
    ASSERT_TRUE(discard_result) << discard_result.error().message;
    EXPECT_TRUE(discard_result.value().generic_function_instances.empty());
    EXPECT_TRUE(discard_result.value().functions.contains(signature.semantic_key));
}

TEST(CoreUnit, SemanticWhiteBoxGenericTypeDisplaysAreLazy) {
    syntax::AstModule module;
    module.modules = {module_info({"generic_type_display"})};

    const TypeId generic_type = module.push_type(named_node("T"));
    const TypeId i32_type = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));

    syntax::TypeNode box_i32_type_node = named_node("Box");
    box_i32_type_node.type_args = {i32_type};
    const TypeId box_i32_type = module.push_type(box_i32_type_node);

    syntax::TypeNode maybe_i32_type_node = named_node("Maybe");
    maybe_i32_type_node.type_args = {i32_type};
    const TypeId maybe_i32_type = module.push_type(maybe_i32_type_node);

    syntax::ItemNode box_item;
    box_item.kind = syntax::ItemKind::struct_decl;
    box_item.name = "Box";
    box_item.generic_params = {syntax::GenericParamDecl {"T", {}}};
    box_item.fields = {syntax::FieldDecl {"value", generic_type, {}}};
    const syntax::ItemId box_item_id = module.push_item(box_item);
    module.item_modules[box_item_id.value] = module_id(0);

    syntax::EnumCaseDecl some_case;
    some_case.name = "some";
    some_case.payload_types = {generic_type};
    syntax::EnumCaseDecl none_case;
    none_case.name = "none";
    syntax::ItemNode maybe_item;
    maybe_item.kind = syntax::ItemKind::enum_decl;
    maybe_item.name = "Maybe";
    maybe_item.generic_params = {syntax::GenericParamDecl {"T", {}}};
    maybe_item.enum_cases = {some_case, none_case};
    const syntax::ItemId maybe_item_id = module.push_item(maybe_item);
    module.item_modules[maybe_item_id.value] = module_id(0);

    const ExprId zero = push_integer(module);
    syntax::StmtNode return_stmt;
    return_stmt.kind = syntax::StmtKind::return_;
    return_stmt.return_value = zero;
    const syntax::StmtId return_stmt_id = module.push_stmt(return_stmt);
    const syntax::StmtId body = push_block(module, {return_stmt_id});

    syntax::ItemNode use_item;
    use_item.kind = syntax::ItemKind::fn_decl;
    use_item.name = "use";
    use_item.params = {
        syntax::ParamDecl {"box", box_i32_type, {}},
        syntax::ParamDecl {"maybe", maybe_i32_type, {}},
    };
    use_item.return_type = i32_type;
    use_item.body = body;
    const syntax::ItemId use_item_id = module.push_item(use_item);
    module.item_modules[use_item_id.value] = module_id(0);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(std::move(module), diagnostics);
    auto checked_result = analyzer.analyze();
    ASSERT_TRUE(checked_result) << checked_result.error().message;
    const sema::CheckedModule& checked = checked_result.value();

    const sema::StructInfo* generic_box = nullptr;
    for (const auto& entry : checked.structs) {
        const sema::StructInfo& info = entry.second;
        if (info.name == "Box" && !checked.types.get(info.type).generic_args.empty()) {
            generic_box = &info;
            break;
        }
    }
    ASSERT_NE(generic_box, nullptr);
    EXPECT_EQ(generic_box->name, "Box");
    EXPECT_EQ(checked.types.get(generic_box->type).name, "generic_type_display.Box");
    EXPECT_EQ(checked.types.display_name(generic_box->type), "generic_type_display.Box[i32]");

    const sema::EnumCaseInfo* generic_some = nullptr;
    for (const auto& entry : checked.enum_cases) {
        const sema::EnumCaseInfo& info = entry.second;
        if (info.enum_name == "Maybe" && info.case_name == "some") {
            generic_some = &info;
            break;
        }
    }
    ASSERT_NE(generic_some, nullptr);
    EXPECT_EQ(generic_some->enum_name, "Maybe");
    EXPECT_EQ(checked.types.display_name(generic_some->type), "generic_type_display.Maybe[i32]");
    EXPECT_EQ(generic_some->name.find("[i32]"), std::string::npos);

    const std::string checked_dump = sema::dump_checked_module(checked);
    EXPECT_NE(checked_dump.find("struct priv Box[i32]"), std::string::npos);
    EXPECT_NE(checked_dump.find("case Maybe[i32]_some"), std::string::npos);
}

TEST(CoreUnit, SemanticWhiteBoxRecordSideTableDenseAndSparseEdges) {
    syntax::AstModule module;
    module.modules = {module_info({"root"})};
    const ExprId expr_id = push_integer(module);
    const ExprId missing_expr_id {expr_id.value + 100U};

    syntax::PatternNode pattern;
    pattern.kind = syntax::PatternKind::binding;
    pattern.binding_name = "value";
    const syntax::PatternId pattern_id = module.push_pattern(pattern);
    const syntax::PatternId alternative_pattern_id = module.push_pattern(pattern);
    const syntax::PatternId missing_pattern_id {alternative_pattern_id.value + 100U};

    const TypeId type_id = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const TypeId missing_type_id {type_id.value + 100U};

    syntax::StmtNode stmt;
    stmt.kind = syntax::StmtKind::expr;
    stmt.init = expr_id;
    const syntax::StmtId stmt_id = module.push_stmt(stmt);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(module, diagnostics);
    sema::TypeTable& types = analyzer.checked_.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle i64 = types.builtin(BuiltinType::i64);

    sema::GenericSideTables dense_side_tables;
    analyzer.current_side_tables_.side_tables = &dense_side_tables;
    static_cast<void>(analyzer.record_expr_type(expr_id, i32));
    analyzer.record_expr_expected_type(expr_id, i64);
    analyzer.record_expr_c_name(expr_id, "dense_expr");
    analyzer.record_pattern_c_name(pattern_id, "dense_pattern");
    analyzer.record_pattern_case_name(pattern_id, "DenseCase");
    analyzer.record_pattern_case_name(alternative_pattern_id, "DenseAlternative");
    analyzer.merge_pattern_case_names(pattern_id, alternative_pattern_id);
    analyzer.record_syntax_type_handle(type_id, i32);
    analyzer.record_stmt_local_type(stmt_id, i64);

    EXPECT_TRUE(types.same(analyzer.cached_expr_type(expr_id), i32));
    EXPECT_TRUE(types.same(analyzer.cached_expr_expected_type(expr_id), i64));
    EXPECT_TRUE(types.same(analyzer.cached_syntax_type(type_id), i32));
    EXPECT_EQ(analyzer.cached_expr_c_name(expr_id), "dense_expr");
    EXPECT_EQ(analyzer.cached_pattern_c_name(pattern_id), "dense_pattern");
    ASSERT_TRUE(dense_side_tables.pattern_case_name_ids.contains(pattern_id.value));
    EXPECT_TRUE(dense_side_tables.pattern_case_name_ids[pattern_id.value].contains(
        analyzer.checked_.intern_c_name("DenseAlternative")
    ));
    EXPECT_TRUE(types.same(dense_side_tables.stmt_local_types[stmt_id.value], i64));
    EXPECT_FALSE(is_valid(analyzer.cached_expr_type(missing_expr_id)));
    EXPECT_FALSE(is_valid(analyzer.cached_expr_expected_type(missing_expr_id)));
    EXPECT_FALSE(is_valid(analyzer.cached_syntax_type(missing_type_id)));
    EXPECT_TRUE(analyzer.cached_expr_c_name(missing_expr_id).empty());
    EXPECT_TRUE(analyzer.cached_pattern_c_name(missing_pattern_id).empty());
    EXPECT_EQ(&analyzer.active_expr_types(), &dense_side_tables.expr_types);
    EXPECT_EQ(&analyzer.active_expr_expected_types(), &dense_side_tables.expr_expected_types);
    EXPECT_EQ(&analyzer.active_expr_c_name_ids(), &dense_side_tables.expr_c_name_ids);
    EXPECT_EQ(&analyzer.active_pattern_c_name_ids(), &dense_side_tables.pattern_c_name_ids);
    EXPECT_EQ(&analyzer.active_pattern_case_name_ids(), &dense_side_tables.pattern_case_name_ids);
    EXPECT_EQ(&analyzer.active_syntax_type_handles(), &dense_side_tables.syntax_type_handles);
    EXPECT_EQ(&analyzer.active_stmt_local_types(), &dense_side_tables.stmt_local_types);
    EXPECT_GT(dense_side_tables.arena_bytes(), 0U);
    EXPECT_GT(dense_side_tables.arena_blocks(), 0U);
    EXPECT_GT(dense_side_tables.pattern_case_name_ids.arena_bytes(), 0U);

    sema::GenericSideTables sparse_side_tables;
    sparse_side_tables.sparse = true;
    analyzer.current_side_tables_.side_tables = &sparse_side_tables;
    analyzer.current_side_tables_.cache_syntax_types = true;
    static_cast<void>(analyzer.record_expr_type(expr_id, i64));
    static_cast<void>(analyzer.record_expr_type(syntax::INVALID_EXPR_ID, i64));
    analyzer.record_expr_expected_type(expr_id, i32);
    analyzer.record_expr_expected_type(syntax::INVALID_EXPR_ID, i32);
    analyzer.record_expr_c_name(expr_id, "sparse_expr");
    analyzer.record_pattern_c_name(pattern_id, "sparse_pattern");
    analyzer.record_pattern_case_name(alternative_pattern_id, "SparseAlternative");
    analyzer.merge_pattern_case_names(pattern_id, alternative_pattern_id);
    analyzer.record_syntax_type_handle(type_id, i64);
    analyzer.record_syntax_type_handle(syntax::INVALID_TYPE_ID, i32);
    analyzer.record_stmt_local_type(stmt_id, i32);

    EXPECT_TRUE(types.same(analyzer.cached_expr_type(expr_id), i64));
    EXPECT_TRUE(types.same(analyzer.cached_expr_expected_type(expr_id), i32));
    EXPECT_TRUE(types.same(analyzer.cached_syntax_type(type_id), i64));
    EXPECT_EQ(analyzer.cached_expr_c_name(expr_id), "sparse_expr");
    EXPECT_EQ(analyzer.cached_pattern_c_name(pattern_id), "sparse_pattern");
    ASSERT_TRUE(sparse_side_tables.pattern_case_name_ids.contains(pattern_id.value));
    EXPECT_TRUE(sparse_side_tables.pattern_case_name_ids[pattern_id.value].contains(
        analyzer.checked_.intern_c_name("SparseAlternative")
    ));
    EXPECT_TRUE(types.same(sparse_side_tables.sparse_stmt_local_types[stmt_id.value], i32));
    EXPECT_FALSE(is_valid(analyzer.cached_expr_type(syntax::INVALID_EXPR_ID)));
    EXPECT_FALSE(is_valid(analyzer.cached_expr_expected_type(syntax::INVALID_EXPR_ID)));
    EXPECT_FALSE(is_valid(analyzer.cached_expr_type(missing_expr_id)));
    EXPECT_FALSE(is_valid(analyzer.cached_expr_expected_type(missing_expr_id)));
    EXPECT_FALSE(is_valid(analyzer.cached_syntax_type(syntax::INVALID_TYPE_ID)));
    EXPECT_FALSE(is_valid(analyzer.cached_syntax_type(missing_type_id)));
    EXPECT_TRUE(analyzer.cached_expr_c_name(syntax::INVALID_EXPR_ID).empty());
    EXPECT_TRUE(analyzer.cached_expr_c_name(missing_expr_id).empty());
    EXPECT_TRUE(analyzer.cached_pattern_c_name(syntax::INVALID_PATTERN_ID).empty());
    EXPECT_TRUE(analyzer.cached_pattern_c_name(missing_pattern_id).empty());
    EXPECT_GT(sparse_side_tables.arena_bytes(), 0U);
    EXPECT_GT(sparse_side_tables.arena_blocks(), 0U);
    EXPECT_GT(sparse_side_tables.pattern_case_name_ids.arena_bytes(), 0U);

    sema::GenericSideTables local_side_tables;
    local_side_tables.configure_local_dense(
        sema::GenericNodeSpan {expr_id.value, 1U},
        sema::GenericNodeSpan {pattern_id.value, 1U},
        sema::GenericNodeSpan {type_id.value, 1U},
        sema::GenericNodeSpan {stmt_id.value, 1U}
    );
    analyzer.current_side_tables_.side_tables = &local_side_tables;
    analyzer.current_side_tables_.cache_syntax_types = true;
    static_cast<void>(analyzer.record_expr_type(expr_id, i32));
    analyzer.record_expr_expected_type(expr_id, i64);
    analyzer.record_expr_c_name(expr_id, "local_expr");
    analyzer.record_pattern_c_name(pattern_id, "local_pattern");
    analyzer.record_syntax_type_handle(type_id, i32);
    analyzer.record_stmt_local_type(stmt_id, i64);

    const base::usize local_expr = local_side_tables.local_expr_index(expr_id);
    const base::usize local_pattern = local_side_tables.local_pattern_index(pattern_id);
    const base::usize local_type = local_side_tables.local_type_index(type_id);
    const base::usize local_stmt = local_side_tables.local_stmt_index(stmt_id);
    ASSERT_NE(local_expr, sema::SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX);
    ASSERT_NE(local_pattern, sema::SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX);
    ASSERT_NE(local_type, sema::SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX);
    ASSERT_NE(local_stmt, sema::SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX);
    EXPECT_TRUE(types.same(local_side_tables.expr_types[local_expr], i32));
    EXPECT_TRUE(types.same(local_side_tables.expr_expected_types[local_expr], i64));
    EXPECT_EQ(analyzer.checked_.c_name_text(local_side_tables.expr_c_name_ids[local_expr]), "local_expr");
    EXPECT_EQ(analyzer.checked_.c_name_text(local_side_tables.pattern_c_name_ids[local_pattern]), "local_pattern");
    EXPECT_TRUE(types.same(local_side_tables.syntax_type_handles[local_type], i32));
    EXPECT_TRUE(types.same(local_side_tables.stmt_local_types[local_stmt], i64));
    EXPECT_TRUE(local_side_tables.sparse_expr_types.empty());
    EXPECT_TRUE(local_side_tables.sparse_expr_expected_types.empty());
    EXPECT_TRUE(local_side_tables.sparse_expr_c_name_ids.empty());
    EXPECT_TRUE(local_side_tables.sparse_pattern_c_name_ids.empty());
    EXPECT_TRUE(local_side_tables.sparse_syntax_type_handles.empty());
    EXPECT_TRUE(local_side_tables.sparse_stmt_local_types.empty());
    EXPECT_TRUE(local_side_tables.sparse_fallbacks.empty());

    sema::GenericSideTables sparse_local_side_tables;
    constexpr base::u32 SEMA_TEST_SPARSE_LOCAL_SPAN_BEGIN = 10U;
    constexpr base::u32 SEMA_TEST_SPARSE_LOCAL_SPAN_COUNT = 91U;
    const std::array<base::u32, 2> sparse_expr_ids {
        SEMA_TEST_SPARSE_LOCAL_SPAN_BEGIN,
        SEMA_TEST_SPARSE_LOCAL_SPAN_BEGIN + SEMA_TEST_SPARSE_LOCAL_SPAN_COUNT - 1U,
    };
    const std::array<base::u32, 2> sparse_pattern_ids = sparse_expr_ids;
    const std::array<base::u32, 2> sparse_type_ids = sparse_expr_ids;
    const std::array<base::u32, 2> sparse_stmt_ids = sparse_expr_ids;
    sparse_local_side_tables.configure_local_dense(
        sema::GenericNodeSpan {SEMA_TEST_SPARSE_LOCAL_SPAN_BEGIN, SEMA_TEST_SPARSE_LOCAL_SPAN_COUNT},
        sema::GenericNodeSpan {SEMA_TEST_SPARSE_LOCAL_SPAN_BEGIN, SEMA_TEST_SPARSE_LOCAL_SPAN_COUNT},
        sema::GenericNodeSpan {SEMA_TEST_SPARSE_LOCAL_SPAN_BEGIN, SEMA_TEST_SPARSE_LOCAL_SPAN_COUNT},
        sema::GenericNodeSpan {SEMA_TEST_SPARSE_LOCAL_SPAN_BEGIN, SEMA_TEST_SPARSE_LOCAL_SPAN_COUNT},
        sparse_expr_ids,
        sparse_pattern_ids,
        sparse_type_ids,
        sparse_stmt_ids
    );
    EXPECT_EQ(sparse_local_side_tables.expr_types.size(), sparse_expr_ids.size());
    EXPECT_EQ(sparse_local_side_tables.pattern_c_name_ids.size(), sparse_pattern_ids.size());
    EXPECT_EQ(sparse_local_side_tables.syntax_type_handles.size(), sparse_type_ids.size());
    EXPECT_EQ(sparse_local_side_tables.stmt_local_types.size(), sparse_stmt_ids.size());
    EXPECT_EQ(sparse_local_side_tables.local_expr_index(ExprId {sparse_expr_ids.front()}), 0U);
    EXPECT_EQ(sparse_local_side_tables.local_expr_index(ExprId {sparse_expr_ids.back()}), 1U);
    EXPECT_EQ(
        sparse_local_side_tables.local_expr_index(ExprId {sparse_expr_ids.front() + 1U}),
        sema::SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX
    );
    analyzer.current_side_tables_.side_tables = &sparse_local_side_tables;
    analyzer.current_side_tables_.cache_syntax_types = true;
    static_cast<void>(analyzer.record_expr_type(ExprId {sparse_expr_ids.front() + 1U}, i32));
    analyzer.record_expr_expected_type(ExprId {sparse_expr_ids.front() + 1U}, i64);
    analyzer.record_expr_c_name(ExprId {sparse_expr_ids.front() + 1U}, "sparse_local_expr");
    analyzer.record_pattern_c_name(syntax::PatternId {sparse_pattern_ids.front() + 1U}, "sparse_local_pattern");
    analyzer.record_pattern_case_name(syntax::PatternId {sparse_pattern_ids.front()}, "SparseLocalAlternative");
    analyzer.record_pattern_case_name(syntax::PatternId {sparse_pattern_ids.front() + 1U}, "SparseLocalCase");
    analyzer.merge_pattern_case_names(
        syntax::PatternId {sparse_pattern_ids.front() + 1U},
        syntax::PatternId {sparse_pattern_ids.front()}
    );
    analyzer.merge_pattern_case_names(
        syntax::PatternId {sparse_pattern_ids.front()},
        syntax::PatternId {sparse_pattern_ids.front() + 1U}
    );
    analyzer.record_syntax_type_handle(TypeId {sparse_type_ids.front() + 1U}, i32);
    analyzer.record_stmt_local_type(syntax::StmtId {sparse_stmt_ids.front() + 1U}, i64);
    EXPECT_EQ(sparse_local_side_tables.sparse_fallbacks.expr_types, 1U);
    EXPECT_EQ(sparse_local_side_tables.sparse_fallbacks.expr_expected_types, 1U);
    EXPECT_EQ(sparse_local_side_tables.sparse_fallbacks.expr_c_name_ids, 1U);
    EXPECT_EQ(sparse_local_side_tables.sparse_fallbacks.pattern_c_name_ids, 1U);
    EXPECT_EQ(sparse_local_side_tables.sparse_fallbacks.pattern_case_name_ids, 3U);
    EXPECT_EQ(sparse_local_side_tables.sparse_fallbacks.syntax_type_handles, 1U);
    EXPECT_EQ(sparse_local_side_tables.sparse_fallbacks.stmt_local_types, 1U);
    EXPECT_EQ(sparse_local_side_tables.sparse_fallbacks.total(), 9U);
}

TEST(CoreUnit, SemanticWhiteBoxArenaBackedSemaStorageCopiesAndMoves) {
    const IdentId alpha_id {1};
    const IdentId beta_id {2};
    const TypeHandle i32 {static_cast<base::u32>(BuiltinType::i32)};
    const TypeHandle i64 {static_cast<base::u32>(BuiltinType::i64)};

    sema::PatternCaseNameTable pattern_names;
    pattern_names.reserve(2);
    pattern_names.insert(10, alpha_id);
    pattern_names.insert(20, beta_id);
    pattern_names.merge(10, pattern_names[20]);
    ASSERT_TRUE(pattern_names.contains(10));
    EXPECT_NE(pattern_names.find(10), pattern_names.end());
    EXPECT_TRUE(pattern_names[10].contains(alpha_id));
    EXPECT_TRUE(pattern_names[10].contains(beta_id));

    sema::PatternCaseNameTable pattern_copy(pattern_names);
    EXPECT_TRUE(pattern_copy[10].contains(beta_id));
    sema::PatternCaseNameTable pattern_assigned;
    pattern_assigned = pattern_names;
    EXPECT_TRUE(pattern_assigned[20].contains(beta_id));
    sema::PatternCaseNameTable pattern_moved(std::move(pattern_copy));
    EXPECT_TRUE(pattern_moved[10].contains(alpha_id));
    sema::PatternCaseNameTable pattern_move_assigned;
    pattern_move_assigned = std::move(pattern_assigned);
    EXPECT_TRUE(pattern_move_assigned[20].contains(beta_id));
    pattern_move_assigned.clear();
    EXPECT_TRUE(pattern_move_assigned.empty());

    sema::GenericSideTables side_tables;
    side_tables.sparse = true;
    side_tables.expr_types.push_back(i32);
    side_tables.prepare_analysis_only_storage(1U);
    side_tables.expr_expected_types.push_back(i64);
    side_tables.expr_c_name_ids.push_back(alpha_id);
    side_tables.pattern_c_name_ids.push_back(beta_id);
    side_tables.syntax_type_handles.push_back(i32);
    side_tables.stmt_local_types.push_back(i64);
    side_tables.sparse_expr_types.emplace(4U, i32);
    side_tables.sparse_expr_expected_types.emplace(5U, i64);
    side_tables.sparse_expr_c_name_ids.emplace(6U, alpha_id);
    side_tables.sparse_pattern_c_name_ids.emplace(7U, beta_id);
    side_tables.sparse_syntax_type_handles.emplace(8U, i32);
    side_tables.sparse_stmt_local_types.emplace(9U, i64);
    side_tables.pattern_case_name_ids.insert(10, alpha_id);
    side_tables.record_sparse_fallback(sema::GenericSparseFallbackKind::expr_type);
    side_tables.record_sparse_fallback(sema::GenericSparseFallbackKind::stmt_local_type);
    side_tables.release_analysis_only_storage();
    EXPECT_TRUE(side_tables.expr_expected_types.empty());
    EXPECT_TRUE(side_tables.sparse_expr_expected_types.empty());
    EXPECT_TRUE(side_tables.pattern_case_name_ids.empty());
    EXPECT_EQ(side_tables.analysis_arena_, nullptr);
    side_tables.prepare_analysis_only_storage(1U);
    side_tables.expr_expected_types.push_back(i64);
    side_tables.sparse_expr_expected_types.emplace(5U, i64);
    side_tables.pattern_case_name_ids.insert(10, alpha_id);

    sema::GenericSideTables side_copy(side_tables);
    EXPECT_TRUE(side_copy.sparse);
    EXPECT_EQ(side_copy.expr_types.front().value, i32.value);
    EXPECT_EQ(side_copy.sparse_expr_c_name_ids.at(6U).value, alpha_id.value);
    EXPECT_EQ(side_copy.sparse_fallbacks.total(), 2U);
    sema::GenericSideTables side_assigned;
    side_assigned = side_tables;
    EXPECT_EQ(side_assigned.sparse_stmt_local_types.at(9U).value, i64.value);
    EXPECT_EQ(side_assigned.sparse_fallbacks.expr_types, 1U);
    sema::GenericSideTables side_moved(std::move(side_copy));
    EXPECT_TRUE(side_moved.pattern_case_name_ids[10].contains(alpha_id));
    EXPECT_EQ(side_moved.sparse_fallbacks.stmt_local_types, 1U);
    sema::GenericSideTables side_move_assigned;
    side_move_assigned = std::move(side_assigned);
    EXPECT_EQ(side_move_assigned.syntax_type_handles.front().value, i32.value);
    EXPECT_EQ(side_move_assigned.sparse_fallbacks.total(), 2U);

    sema::CheckedModule checked;
    const IdentId checked_c_name = checked.intern_c_name("m0_test");
    checked.expr_types.push_back(i32);
    checked.prepare_analysis_only_storage(1U);
    checked.expr_expected_types.push_back(i64);
    checked.expr_c_name_ids.push_back(checked_c_name);
    checked.pattern_c_name_ids.push_back(checked_c_name);
    checked.pattern_case_name_ids.insert(3, checked_c_name);
    checked.syntax_type_handles.push_back(i32);
    checked.stmt_local_types.push_back(i64);
    checked.item_c_name_ids.push_back(checked_c_name);
    checked.coercions.push_back(sema::CoercionRecord {
        ExprId {0},
        i32,
        i64,
        sema::CoercionKind::contextual_integer_literal,
    });

    FunctionSignature signature = checked.make_function_signature();
    signature.name = "f";
    signature.name_id = alpha_id;
    signature.semantic_key = sema::FunctionLookupKey {
        module_id(0).value,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        alpha_id,
    };
    signature.c_name = "m0_f";
    signature.param_types.push_back(i32);
    signature.generic_args.push_back(i64);
    signature.return_type = i32;
    checked.functions.emplace(signature.semantic_key, signature);
    StructInfo struct_info = checked.make_struct_info();
    struct_info.name = "S";
    struct_info.name_id = alpha_id;
    struct_info.c_name = "m0_S";
    struct_info.module = module_id(0);
    struct_info.type = i32;
    const sema::ModuleLookupKey struct_key {module_id(0).value, alpha_id};
    checked.structs.emplace(struct_key, struct_info);
    EnumCaseInfo enum_case = checked.make_enum_case_info();
    enum_case.name = "E_case";
    enum_case.name_id = beta_id;
    enum_case.case_name = "case";
    enum_case.case_name_id = beta_id;
    enum_case.c_name = "m0_E_case";
    enum_case.type = i64;
    enum_case.payload_types.push_back(i32);
    const sema::ModuleLookupKey enum_case_key {module_id(0).value, beta_id};
    checked.enum_cases.emplace(enum_case_key, enum_case);
    sema::TypeAliasInfo alias_info;
    alias_info.name = "Alias";
    alias_info.name_id = alpha_id;
    alias_info.module = module_id(0);
    alias_info.target = TypeId {0};
    const sema::ModuleLookupKey alias_key {module_id(0).value, alpha_id};
    checked.type_aliases.emplace(alias_key, alias_info);

    sema::GenericFunctionInstanceInfo instance;
    instance.key = sema::FunctionLookupKey {
        module_id(0).value,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        checked.intern_c_name("f[i32]"),
    };
    instance.item = syntax::ItemId {0};
    instance.signature = checked.clone_function_signature(signature);
    instance.side_table_layout_index = checked.append_generic_side_table_layout(
        sema::GenericNodeSpan {4U, 3U},
        {},
        {},
        {},
        {},
        {},
        {},
        {}
    );
    ASSERT_EQ(instance.side_table_layout_index, 0U);
    const sema::GenericSideTableLayout* const instance_layout =
        checked.generic_side_table_layout(instance.side_table_layout_index);
    ASSERT_NE(instance_layout, nullptr);
    instance.side_tables.configure_local_dense(*instance_layout);
    instance.side_tables.expr_types.front() = i32;
    instance.side_tables.expr_expected_types.front() = i64;
    checked.generic_function_instances.push_back(std::move(instance));

    sema::CheckedModule checked_copy(checked);
    ASSERT_EQ(checked_copy.functions.size(), 1U);
    EXPECT_EQ(checked_copy.c_name_text(checked_copy.expr_c_name_ids.front()), "m0_test");
    EXPECT_EQ(checked_copy.generic_function_instances.front().signature.name, "f");
    EXPECT_EQ(checked_copy.generic_side_table_layouts.size(), 1U);
    EXPECT_EQ(
        checked_copy.generic_function_instances.front().side_tables.layout,
        &checked_copy.generic_side_table_layouts.front()
    );
    EXPECT_EQ(checked_copy.generic_function_instances.front().side_tables.local_expr_index(ExprId {4U}), 0U);
    checked.release_analysis_only_storage();
    EXPECT_TRUE(checked.expr_expected_types.empty());
    EXPECT_TRUE(checked.pattern_case_name_ids.empty());
    EXPECT_EQ(checked.analysis_arena_, nullptr);
    ASSERT_FALSE(checked.generic_function_instances.empty());
    EXPECT_TRUE(checked.generic_function_instances.front().side_tables.expr_expected_types.empty());
    EXPECT_TRUE(checked.generic_function_instances.front().side_tables.pattern_case_name_ids.empty());
    EXPECT_EQ(
        checked.generic_function_instances.front().side_tables.layout,
        &checked.generic_side_table_layouts.front()
    );
    sema::CheckedModule checked_assigned;
    checked_assigned = checked;
    EXPECT_EQ(checked_assigned.enum_cases.at(enum_case_key).payload_types.front().value, i32.value);
    sema::CheckedModule checked_moved(std::move(checked_copy));
    EXPECT_EQ(checked_moved.structs.at(struct_key).name, "S");
    sema::CheckedModule checked_move_assigned;
    checked_move_assigned = std::move(checked_assigned);
    EXPECT_EQ(checked_move_assigned.type_aliases.at(alias_key).name, "Alias");
}

TEST(CoreUnit, SemanticWhiteBoxParserOnlyModuleContractIsNormalized) {
    using CheckedExprCNameTable = decltype(sema::CheckedModule {}.expr_c_name_ids);
    using CheckedPatternCNameTable = decltype(sema::CheckedModule {}.pattern_c_name_ids);
    using CheckedItemCNameTable = decltype(sema::CheckedModule {}.item_c_name_ids);
    using CheckedGenericLayoutTable = decltype(sema::CheckedModule {}.generic_side_table_layouts);
    using CheckedGenericInstanceTable = decltype(sema::CheckedModule {}.generic_function_instances);
    using AnalyzerGenericMethodIndex =
        decltype(std::declval<sema::SemanticAnalyzer&>().generic_method_templates_by_name_);
    using AnalyzerEnumCaseTypeIndex =
        decltype(std::declval<sema::SemanticAnalyzer&>().enum_cases_by_type_);
    using AnalyzerVisibleModuleCache =
        decltype(std::declval<sema::SemanticAnalyzer&>().visible_modules_cache_);
    using GenericTemplateParams =
        decltype(std::declval<sema::SemanticAnalyzer::GenericTemplateInfo&>().params);
    using GenericTemplateConstraints =
        decltype(std::declval<sema::SemanticAnalyzer::GenericTemplateInfo&>().constraints);
    using FunctionParamTypes = decltype(std::declval<sema::FunctionSignature&>().param_types);
    using FunctionGenericArgs = decltype(std::declval<sema::FunctionSignature&>().generic_args);
    using StructFields = decltype(std::declval<sema::StructInfo&>().fields);
    using EnumPayloadTypes = decltype(std::declval<sema::EnumCaseInfo&>().payload_types);
    using TypeTupleElements = decltype(std::declval<sema::TypeInfo&>().tuple_elements);
    using TypeFunctionParams = decltype(std::declval<sema::TypeInfo&>().function_params);
    using TypeGenericArgs = decltype(std::declval<sema::TypeInfo&>().generic_args);

    static_assert(std::is_same_v<CheckedExprCNameTable::value_type, sema::IdentId>);
    static_assert(std::is_same_v<CheckedPatternCNameTable::value_type, sema::IdentId>);
    static_assert(std::is_same_v<CheckedItemCNameTable::value_type, sema::IdentId>);
    static_assert(std::is_same_v<CheckedGenericLayoutTable, sema::SemaDeque<sema::GenericSideTableLayout>>);
    static_assert(std::is_same_v<CheckedGenericInstanceTable, sema::SemaDeque<sema::GenericFunctionInstanceInfo>>);
    static_assert(std::is_same_v<AnalyzerGenericMethodIndex::mapped_type, sema::SemanticAnalyzer::GenericTemplateList>);
    static_assert(std::is_same_v<AnalyzerEnumCaseTypeIndex::mapped_type, sema::SemanticAnalyzer::EnumCaseList>);
    static_assert(std::is_same_v<AnalyzerVisibleModuleCache::mapped_type, sema::SemanticAnalyzer::ModuleIdList>);
    static_assert(std::is_same_v<GenericTemplateParams, sema::SemaVector<sema::IdentId>>);
    static_assert(std::is_same_v<GenericTemplateConstraints, sema::SemanticAnalyzer::CapabilityMap>);
    static_assert(std::is_same_v<FunctionParamTypes, sema::TypeHandleList>);
    static_assert(std::is_same_v<FunctionGenericArgs, sema::TypeHandleList>);
    static_assert(std::is_same_v<StructFields, sema::SemaVector<sema::StructFieldInfo>>);
    static_assert(std::is_same_v<EnumPayloadTypes, sema::TypeHandleList>);
    static_assert(std::is_same_v<TypeTupleElements, sema::TypeHandleList>);
    static_assert(std::is_same_v<TypeFunctionParams, sema::TypeHandleList>);
    static_assert(std::is_same_v<TypeGenericArgs, sema::TypeHandleList>);
    static_assert(std::is_same_v<decltype(sema::CheckedModule {}.normalized_ast), sema::NormalizedAstOverlay>);
    static_assert(sizeof(sema::NormalizedAstOverlay) < sizeof(syntax::AstModule));

    syntax::AstModule module;
    module.module_path = module_path({"parser_only"});

    syntax::TypeNode i32_type_node = primitive_node(syntax::PrimitiveTypeKind::i32);
    const TypeId i32_type = module.push_type(i32_type_node);

    const ExprId zero = push_integer_text(module, "0");

    syntax::StmtNode return_stmt;
    return_stmt.kind = syntax::StmtKind::return_;
    return_stmt.return_value = zero;
    const syntax::StmtId return_stmt_id = module.push_stmt(return_stmt);
    const syntax::StmtId body = push_block(module, {return_stmt_id});

    syntax::ItemNode main_function;
    main_function.kind = syntax::ItemKind::fn_decl;
    main_function.name = "main";
    main_function.return_type = i32_type;
    main_function.body = body;
    static_cast<void>(module.push_item(main_function));

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(module, diagnostics);
    auto checked_result = analyzer.analyze();
    ASSERT_TRUE(checked_result) << checked_result.error().message;
    EXPECT_GT(checked_result.value().arena_bytes(), 0U);
    EXPECT_GT(checked_result.value().arena_blocks(), 0U);
    EXPECT_TRUE(checked_result.value().normalized_ast.parser_only_module_contract_added);
    EXPECT_EQ(checked_result.value().normalized_ast.original_expr_count, 1U);
    EXPECT_EQ(checked_result.value().normalized_ast.final_expr_count, module.exprs.size());
    EXPECT_EQ(checked_result.value().normalized_ast.original_type_count, 1U);
    EXPECT_EQ(checked_result.value().normalized_ast.final_type_count, module.types.size());
    EXPECT_EQ(module.modules.size(), 1U);
    EXPECT_EQ(module.item_modules.size(), 1U);
    EXPECT_EQ(module.item_modules.front().value, 0U);

    syntax::AstModule discard_module;
    discard_module.modules = {module_info({"root"})};
    const TypeId discard_i32_type = discard_module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const ExprId discard_zero = push_integer(discard_module);
    syntax::StmtNode discard_return_stmt;
    discard_return_stmt.kind = syntax::StmtKind::return_;
    discard_return_stmt.return_value = discard_zero;
    const syntax::StmtId discard_return_stmt_id = discard_module.push_stmt(discard_return_stmt);
    const syntax::StmtId discard_body = push_block(discard_module, {discard_return_stmt_id});

    syntax::ItemNode discard_main_function;
    discard_main_function.kind = syntax::ItemKind::fn_decl;
    discard_main_function.name = "main";
    discard_main_function.return_type = discard_i32_type;
    discard_main_function.body = discard_body;
    const syntax::ItemId discard_main_item = discard_module.push_item(discard_main_function);
    discard_module.item_modules[discard_main_item.value] = module_id(0);

    base::DiagnosticSink snapshot_diagnostics;
    sema::SemanticAnalyzer snapshot_analyzer(std::move(discard_module), snapshot_diagnostics);
    auto snapshot_result = snapshot_analyzer.analyze();
    ASSERT_TRUE(snapshot_result) << snapshot_result.error().message;
    EXPECT_FALSE(snapshot_result.value().normalized_ast.parser_only_module_contract_added);
    EXPECT_EQ(snapshot_result.value().normalized_ast.original_expr_count, 1U);
    EXPECT_EQ(snapshot_result.value().normalized_ast.final_expr_count, 1U);
    EXPECT_EQ(snapshot_result.value().normalized_ast.original_type_count, 1U);
    EXPECT_EQ(snapshot_result.value().normalized_ast.final_type_count, 1U);
}

TEST(CoreUnit, SemanticWhiteBoxParserAstRequiresItemModulesWhenModulesExist) {
    syntax::AstModule module;
    module.modules = {module_info({"root"})};

    syntax::TypeNode i32_type_node = primitive_node(syntax::PrimitiveTypeKind::i32);
    const TypeId i32_type = module.push_type(i32_type_node);

    syntax::ItemNode main_function;
    main_function.kind = syntax::ItemKind::fn_decl;
    main_function.name = "main";
    main_function.return_type = i32_type;
    static_cast<void>(module.push_item(main_function));
    module.item_modules.clear();

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(std::move(module), diagnostics);
    auto checked_result = analyzer.analyze();
    EXPECT_FALSE(checked_result);
    ASSERT_TRUE(diagnostics.has_error());
    bool found = false;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        found = found || diagnostic.message.find("item_modules must contain one module owner per item") != std::string::npos;
    }
    EXPECT_TRUE(found);
}

TEST(CoreUnit, SemanticWhiteBoxSyntaxTypeCacheDisabledDoesNotRead) {
    syntax::AstModule module;
    module.modules = {module_info({"root"})};
    const TypeId bool_type_id = module.push_type(primitive_node(syntax::PrimitiveTypeKind::bool_));

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(module, diagnostics);
    analyzer.checked_.syntax_type_handles.assign(module.types.size(), INVALID_TYPE_HANDLE);
    const TypeHandle bool_type = analyzer.checked_.types.builtin(BuiltinType::bool_);
    analyzer.checked_.syntax_type_handles[bool_type_id.value] = bool_type;

    EXPECT_EQ(analyzer.cached_syntax_type(bool_type_id).value, bool_type.value);
    analyzer.current_side_tables_.cache_syntax_types = false;
    EXPECT_FALSE(is_valid(analyzer.cached_syntax_type(bool_type_id)));
}

TEST(CoreUnit, SemanticWhiteBoxStringBuiltinExpressions) {
    syntax::AstModule module;
    module.modules = {module_info({"root"})};

    syntax::TypeNode u8_type;
    u8_type.kind = syntax::TypeKind::primitive;
    u8_type.primitive = syntax::PrimitiveTypeKind::u8;
    const TypeId u8_type_id = module.push_type(u8_type);
    syntax::TypeNode const_u8_ptr_type;
    const_u8_ptr_type.kind = syntax::TypeKind::pointer;
    const_u8_ptr_type.pointer_mutability = syntax::PointerMutability::const_;
    const_u8_ptr_type.pointee = u8_type_id;
    const TypeId const_u8_ptr_type_id = module.push_type(const_u8_ptr_type);

    const ExprId str_value = push_name(module, "text");
    const ExprId data_value = push_name(module, "data");
    const ExprId length_value = push_name(module, "len");
    const ExprId str_data_id = module.push_cast_like_expr(
        syntax::ExprKind::str_data,
        {},
        syntax::CastExprPayload {syntax::INVALID_TYPE_ID, str_value}
    );
    const ExprId str_byte_len_id = module.push_cast_like_expr(
        syntax::ExprKind::str_byte_len,
        {},
        syntax::CastExprPayload {syntax::INVALID_TYPE_ID, str_value}
    );
    const ExprId str_from_bytes_id = module.push_call_expr(
        syntax::ExprKind::str_from_bytes_unchecked,
        {},
        syntax::CallExprPayload {syntax::INVALID_EXPR_ID, {data_value, length_value}}
    );
    const ExprId malformed_id = module.push_call_expr(
        syntax::ExprKind::str_from_bytes_unchecked,
        {},
        syntax::CallExprPayload {syntax::INVALID_EXPR_ID, {data_value}}
    );
    const ExprId raw_literal_id =
        module.push_literal_expr(syntax::ExprKind::raw_string_literal, {}, "r\"C:\\tmp\\a\"");
    const ExprId byte_string_literal_id =
        module.push_literal_expr(syntax::ExprKind::byte_string_literal, {}, "b\"a\\n\\0\"");
    const ExprId invalid_byte_string_literal_id =
        module.push_literal_expr(syntax::ExprKind::byte_string_literal, {}, "b\"\\u{41}\"");
    const ExprId char_literal_id =
        module.push_literal_expr(syntax::ExprKind::char_literal, {}, "'\\u{03BB}'");

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(module, diagnostics);
    analyzer.checked_.expr_types.assign(module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.checked_.expr_c_name_ids.assign(module.exprs.size(), sema::INVALID_IDENT_ID);
    analyzer.checked_.syntax_type_handles.assign(module.types.size(), INVALID_TYPE_HANDLE);
    analyzer.current_module_ = module_id(0);

    sema::TypeTable& types = analyzer.checked_.types;
    const TypeHandle str = types.builtin(BuiltinType::str);
    const TypeHandle u8 = types.builtin(BuiltinType::u8);
    const TypeHandle usize = types.builtin(BuiltinType::usize);
    const TypeHandle const_u8_ptr = types.pointer(PointerMutability::const_, u8);
    analyzer.checked_.syntax_type_handles[u8_type_id.value] = u8;
    analyzer.checked_.syntax_type_handles[const_u8_ptr_type_id.value] = const_u8_ptr;
    const sema::FunctionLookupKey text_key = add_global_value(
        analyzer,
        module_id(0),
        "text",
        str,
        SymbolKind::local
    ).first;
    const sema::FunctionLookupKey data_key = add_global_value(
        analyzer,
        module_id(0),
        "data",
        const_u8_ptr,
        SymbolKind::local
    ).first;
    const sema::FunctionLookupKey len_key = add_global_value(
        analyzer,
        module_id(0),
        "len",
        usize,
        SymbolKind::local
    ).first;

    EXPECT_TRUE(types.same(analyzer.analyze_str_projection_expr(str_data_id, analyzer.expr_view(str_data_id)), const_u8_ptr));
    EXPECT_TRUE(types.same(analyzer.analyze_str_projection_expr(str_byte_len_id, analyzer.expr_view(str_byte_len_id)), usize));
    EXPECT_TRUE(types.same(analyzer.analyze_str_from_bytes_unchecked_expr(str_from_bytes_id, analyzer.expr_view(str_from_bytes_id)), str));
    EXPECT_TRUE(types.same(analyzer.analyze_str_from_bytes_unchecked_expr(malformed_id, analyzer.expr_view(malformed_id)), str));
    EXPECT_TRUE(types.same(analyzer.analyze_expr(raw_literal_id), str));
    const TypeHandle byte_string_type = analyzer.analyze_expr(byte_string_literal_id);
    ASSERT_TRUE(types.is_array(byte_string_type));
    EXPECT_EQ(types.get(byte_string_type).array_count, 3U);
    EXPECT_TRUE(types.same(types.get(byte_string_type).array_element, u8));
    const base::usize diagnostics_before_invalid_byte_string = diagnostics.diagnostics().size();
    const TypeHandle invalid_byte_string_type = analyzer.analyze_expr(invalid_byte_string_literal_id);
    ASSERT_TRUE(types.is_array(invalid_byte_string_type));
    EXPECT_TRUE(types.same(types.get(invalid_byte_string_type).array_element, u8));
    EXPECT_GT(diagnostics.diagnostics().size(), diagnostics_before_invalid_byte_string);
    EXPECT_TRUE(types.is_char(analyzer.analyze_expr(char_literal_id)));

    analyzer.global_values_[text_key].type = usize;
    analyzer.global_values_[data_key].type = usize;
    analyzer.global_values_[len_key].type = str;
    static_cast<void>(analyzer.analyze_str_projection_expr(str_data_id, analyzer.expr_view(str_data_id)));
    static_cast<void>(analyzer.analyze_str_projection_expr(str_byte_len_id, analyzer.expr_view(str_byte_len_id)));
    static_cast<void>(analyzer.analyze_str_from_bytes_unchecked_expr(str_from_bytes_id, analyzer.expr_view(str_from_bytes_id)));

    EXPECT_GT(diagnostics.diagnostics().size(), 0U);
}

TEST(CoreUnit, SemanticWhiteBoxArrayLiteralEdges) {
    syntax::AstModule module;
    module.modules = {module_info({"root"})};

    const ExprId repeat_value = push_integer(module);
    const ExprId repeat_literal_id = module.push_array_expr(
        {},
        syntax::ArrayExprPayload {
            {},
            repeat_value,
            syntax::INVALID_EXPR_ID,
        }
    );

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(module, diagnostics);
    analyzer.checked_.expr_types.assign(module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.current_module_ = module_id(0);

    sema::TypeTable& types = analyzer.checked_.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle expected_array = types.array(SEMA_TEST_SMALL_ARRAY_COUNT, i32);
    EXPECT_TRUE(types.same(
        analyzer.analyze_array_literal_expr(repeat_literal_id, analyzer.expr_view(repeat_literal_id), expected_array),
        expected_array
    ));
    EXPECT_TRUE(diagnostics.has_error());
}

TEST(CoreUnit, SemanticWhiteBoxExpectedTypeSensitiveExprCache) {
    syntax::AstModule module;
    module.modules = {module_info({"root"})};

    const ExprId integer_literal = push_integer_text(module, "2147483648");
    const ExprId small_integer_literal = push_integer_text(module, "7");
    const ExprId null_literal_id = module.push_literal_expr(syntax::ExprKind::null_literal, {}, "null");

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(module, diagnostics);
    analyzer.checked_.expr_types.assign(module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.checked_.prepare_analysis_only_storage(module.exprs.size());
    analyzer.checked_.expr_expected_types.assign(module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.current_module_ = module_id(0);

    sema::TypeTable& types = analyzer.checked_.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle i64 = types.builtin(BuiltinType::i64);
    const TypeHandle ptr_i32 = types.pointer(PointerMutability::const_, i32);

    EXPECT_TRUE(types.same(analyzer.analyze_expr(integer_literal), i32));
    EXPECT_TRUE(types.same(analyzer.analyze_expr(integer_literal, i64), i64));
    EXPECT_TRUE(types.same(analyzer.checked_.expr_types[integer_literal.value], i64));
    EXPECT_TRUE(types.same(analyzer.checked_.expr_expected_types[integer_literal.value], i64));

    EXPECT_TRUE(types.same(analyzer.analyze_expr(small_integer_literal, i64), i64));
    ASSERT_FALSE(analyzer.checked_.coercions.empty());
    const sema::CoercionRecord& coercion = analyzer.checked_.coercions.back();
    EXPECT_EQ(coercion.expr.value, small_integer_literal.value);
    EXPECT_TRUE(types.same(coercion.from_type, i32));
    EXPECT_TRUE(types.same(coercion.to_type, i64));
    EXPECT_EQ(coercion.kind, sema::CoercionKind::contextual_integer_literal);

    EXPECT_FALSE(is_valid(analyzer.analyze_expr(null_literal_id)));
    const std::size_t null_coercion_index = analyzer.checked_.coercions.size();
    EXPECT_TRUE(types.same(analyzer.analyze_expr(null_literal_id, ptr_i32), ptr_i32));
    EXPECT_TRUE(types.same(analyzer.checked_.expr_types[null_literal_id.value], ptr_i32));
    EXPECT_TRUE(types.same(analyzer.checked_.expr_expected_types[null_literal_id.value], ptr_i32));
    ASSERT_GT(analyzer.checked_.coercions.size(), null_coercion_index);
    const sema::CoercionRecord& null_coercion = analyzer.checked_.coercions.back();
    EXPECT_EQ(null_coercion.expr.value, null_literal_id.value);
    EXPECT_FALSE(is_valid(null_coercion.from_type));
    EXPECT_TRUE(types.same(null_coercion.to_type, ptr_i32));
    EXPECT_EQ(null_coercion.kind, sema::CoercionKind::null_to_pointer);
}

TEST(CoreUnit, SemanticWhiteBoxStatementControlFlowQueries) {
    syntax::AstModule module;
    module.modules = {module_info({"root"})};

    const syntax::StmtId return_stmt = push_stmt(module, syntax::StmtKind::return_);
    const syntax::StmtId expr_stmt = push_stmt(module, syntax::StmtKind::expr);
    const syntax::StmtId break_stmt = push_stmt(module, syntax::StmtKind::break_);
    const syntax::StmtId continue_stmt = push_stmt(module, syntax::StmtKind::continue_);

    const syntax::StmtId nested_return_block = push_block(module, {return_stmt});
    const syntax::StmtId nested_fallthrough_block = push_block(module, {expr_stmt});
    const syntax::StmtId mixed_block = push_block(module, {expr_stmt, nested_return_block});

    syntax::StmtNode full_if;
    full_if.kind = syntax::StmtKind::if_;
    full_if.then_block = nested_return_block;
    full_if.else_block = push_block(module, {return_stmt});
    const syntax::StmtId full_if_stmt = module.push_stmt(full_if);

    syntax::StmtNode partial_if = full_if;
    partial_if.else_block = nested_fallthrough_block;
    const syntax::StmtId partial_if_stmt = module.push_stmt(partial_if);

    syntax::StmtNode else_if_leaf;
    else_if_leaf.kind = syntax::StmtKind::if_;
    else_if_leaf.then_block = nested_return_block;
    else_if_leaf.else_block = push_block(module, {return_stmt});
    const syntax::StmtId else_if_leaf_stmt = module.push_stmt(else_if_leaf);

    syntax::StmtNode else_if_root;
    else_if_root.kind = syntax::StmtKind::if_;
    else_if_root.then_block = nested_return_block;
    else_if_root.else_if = else_if_leaf_stmt;
    const syntax::StmtId else_if_root_stmt = module.push_stmt(else_if_root);

    syntax::StmtNode missing_else_if_root = else_if_root;
    missing_else_if_root.else_if = syntax::INVALID_STMT_ID;
    const syntax::StmtId missing_else_if_stmt = module.push_stmt(missing_else_if_root);

    const syntax::StmtId fallthrough_block = push_block(module, {expr_stmt, partial_if_stmt});
    const syntax::StmtId non_fallthrough_block = push_block(module, {expr_stmt, full_if_stmt, expr_stmt});
    const syntax::StmtId abrupt_block = push_block(module, {continue_stmt, expr_stmt});

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(module, diagnostics);

    EXPECT_FALSE(analyzer.block_guarantees_return(syntax::INVALID_STMT_ID));
    EXPECT_FALSE(analyzer.stmt_guarantees_return(syntax::INVALID_STMT_ID));
    EXPECT_TRUE(analyzer.block_may_fallthrough(syntax::INVALID_STMT_ID));
    EXPECT_TRUE(analyzer.stmt_may_fallthrough(syntax::INVALID_STMT_ID));

    EXPECT_TRUE(analyzer.block_guarantees_return(return_stmt));
    EXPECT_TRUE(analyzer.block_guarantees_return(mixed_block));
    EXPECT_TRUE(analyzer.stmt_guarantees_return(full_if_stmt));
    EXPECT_TRUE(analyzer.stmt_guarantees_return(else_if_root_stmt));
    EXPECT_FALSE(analyzer.stmt_guarantees_return(partial_if_stmt));
    EXPECT_FALSE(analyzer.stmt_guarantees_return(missing_else_if_stmt));
    EXPECT_FALSE(analyzer.stmt_guarantees_return(expr_stmt));

    EXPECT_FALSE(analyzer.stmt_may_fallthrough(return_stmt));
    EXPECT_FALSE(analyzer.stmt_may_fallthrough(break_stmt));
    EXPECT_FALSE(analyzer.stmt_may_fallthrough(continue_stmt));
    EXPECT_FALSE(analyzer.stmt_may_fallthrough(full_if_stmt));
    EXPECT_FALSE(analyzer.stmt_may_fallthrough(else_if_root_stmt));
    EXPECT_FALSE(analyzer.block_may_fallthrough(non_fallthrough_block));
    EXPECT_FALSE(analyzer.block_may_fallthrough(abrupt_block));
    EXPECT_TRUE(analyzer.stmt_may_fallthrough(partial_if_stmt));
    EXPECT_TRUE(analyzer.stmt_may_fallthrough(missing_else_if_stmt));
    EXPECT_TRUE(analyzer.block_may_fallthrough(fallthrough_block));
}

TEST(CoreUnit, SemanticWhiteBoxIterativeTypeLayoutEdges) {
    syntax::AstModule module;
    module.modules = {module_info({"root"})};

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(module, diagnostics);
    analyzer.current_module_ = module_id(0);

    sema::TypeTable& types = analyzer.checked_.types;
    const TypeHandle u8 = types.builtin(BuiltinType::u8);
    const TypeHandle u64 = types.builtin(BuiltinType::u64);

    const TypeHandle max_array_u8 = types.array(SEMA_TEST_LAYOUT_MAX_ARRAY_COUNT, u8);
    EXPECT_EQ(analyzer.abi_size(max_array_u8), SEMA_TEST_LAYOUT_MAX_ARRAY_COUNT);
    EXPECT_EQ(analyzer.abi_align(max_array_u8), alignof(std::uint8_t));

    const TypeHandle overflow_array_u64 = types.array(SEMA_TEST_LAYOUT_MAX_ARRAY_COUNT, u64);
    EXPECT_EQ(analyzer.abi_size(overflow_array_u64), SEMA_TEST_LAYOUT_MAX_ARRAY_COUNT);
    EXPECT_EQ(analyzer.abi_align(overflow_array_u64), alignof(std::uint64_t));

    const TypeHandle overflow_struct_type = types.named_struct("OverflowStruct", "OverflowStruct", true);
    StructInfo overflow_struct;
    overflow_struct.name = "OverflowStruct";
    overflow_struct.name_id = intern_identifier(analyzer, "OverflowStruct");
    overflow_struct.module = module_id(0);
    overflow_struct.type = overflow_struct_type;
    overflow_struct.fields = {
        StructFieldInfo {"huge", intern_identifier(analyzer, "huge"), "huge", module_id(0), max_array_u8},
        StructFieldInfo {"tail", intern_identifier(analyzer, "tail"), "tail", module_id(0), u64},
    };
    analyzer.checked_.structs.emplace(semantic_module_key(analyzer, module_id(0), "OverflowStruct"), overflow_struct);

    const TypeHandle overflow_enum_type = types.named_enum("OverflowEnum", "OverflowEnum");
    types.set_enum_underlying(overflow_enum_type, max_array_u8);
    EnumCaseInfo overflow_case;
    overflow_case.name = "payload";
    overflow_case.name_id = intern_identifier(analyzer, "payload");
    overflow_case.case_name = "payload";
    overflow_case.case_name_id = intern_identifier(analyzer, "payload");
    overflow_case.module = module_id(0);
    overflow_case.type = overflow_enum_type;
    overflow_case.payload_type = u64;
    analyzer.checked_.enum_cases.emplace(semantic_module_key(analyzer, module_id(0), "payload"), overflow_case);
    static_cast<void>(add_named_type(analyzer, module_id(0), "OverflowEnum", overflow_enum_type));

    analyzer.validate_type_layouts();
    EXPECT_GT(diagnostics.diagnostics().size(), 0U);
}

TEST(CoreUnit, SemanticWhiteBoxRecordTypeAndAssociatedOwnerEdges) {
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"lib", "one"}),
    };
    module.modules[SEMA_TEST_ROOT_MODULE_INDEX].imports = {resolved_import(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "one")};

    const TypeId u8_type_id = module.push_type(primitive_node(syntax::PrimitiveTypeKind::u8));
    const TypeId i32_type_id = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const ExprId hex_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_HEX_UPPER);
    const ExprId lower_hex_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_HEX_LOWER);
    const ExprId upper_hex_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_HEX_UPPER);
    const ExprId bin_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_BIN_LOWER);
    const ExprId upper_bin_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_BIN_UPPER);
    const ExprId invalid_digit_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_INVALID_BINARY);
    const ExprId invalid_char_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_INVALID_CHAR);
    const ExprId empty_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_EMPTY);
    const ExprId overflow_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_UNSIGNED_OVERFLOW);
    const ExprId signed_overflow_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_SIGNED_OVERFLOW);
    const ExprId suffixed_u8_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_U8_SUFFIX);
    const ExprId suffixed_i8_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_I8_SUFFIX);
    const ExprId invalid_suffix_expr = push_integer_text(module, SEMA_TEST_INTEGER_LITERAL_INVALID_SUFFIX);
    const ExprId float_overflow_expr_id =
        module.push_literal_expr(syntax::ExprKind::float_literal, {}, SEMA_TEST_FLOAT_OVERFLOW_LITERAL);
    const ExprId separated_float_expr_id =
        module.push_literal_expr(syntax::ExprKind::float_literal, {}, SEMA_TEST_FLOAT_WITH_SEPARATOR_LITERAL);
    const ExprId invalid_float_expr_id =
        module.push_literal_expr(syntax::ExprKind::float_literal, {}, SEMA_TEST_FLOAT_INVALID_TRAILING_LITERAL);
    const ExprId suffixed_float_expr_id =
        module.push_literal_expr(syntax::ExprKind::float_literal, {}, SEMA_TEST_FLOAT_LITERAL_F32_SUFFIX);
    const ExprId invalid_suffix_float_expr_id =
        module.push_literal_expr(syntax::ExprKind::float_literal, {}, SEMA_TEST_FLOAT_LITERAL_INVALID_SUFFIX);

    syntax::TypeNode scoped_missing_alias_type = named_node("Missing");
    scoped_missing_alias_type.scope_name = "missing";
    const TypeId scoped_missing_alias_type_id = module.push_type(scoped_missing_alias_type);
    syntax::TypeNode scoped_choice_missing_args_type = named_node("Choice");
    scoped_choice_missing_args_type.scope_name = "one";
    const TypeId scoped_choice_missing_args_type_id = module.push_type(scoped_choice_missing_args_type);
    syntax::TypeNode invalid_primitive_type;
    invalid_primitive_type.kind = syntax::TypeKind::primitive;
    invalid_primitive_type.primitive = SEMA_TEST_INVALID_PRIMITIVE_KIND;
    const TypeId invalid_primitive_type_id = module.push_type(invalid_primitive_type);
    syntax::TypeNode scoped_opaque_type = named_node("ScopedOpaque");
    scoped_opaque_type.scope_name = "one";
    const TypeId scoped_opaque_type_id = module.push_type(scoped_opaque_type);

    syntax::ItemNode enum_item;
    enum_item.kind = syntax::ItemKind::enum_decl;
    enum_item.name = "Choice";
    enum_item.enum_base_type = u8_type_id;
    enum_item.enum_cases = {
        syntax::EnumCaseDecl {"none", syntax::INVALID_TYPE_ID, {}, SEMA_TEST_INTEGER_LITERAL_ONE, {}},
    };
    const syntax::ItemId enum_item_id = module.push_item(enum_item);
    module.item_modules[enum_item_id.value] = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(module, diagnostics);
    analyzer.checked_.expr_types.assign(module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.checked_.expr_c_name_ids.assign(module.exprs.size(), sema::INVALID_IDENT_ID);
    analyzer.checked_.pattern_c_name_ids.assign(SEMA_TEST_PATTERN_TRACKED_COUNT, sema::INVALID_IDENT_ID);
    analyzer.checked_.syntax_type_handles.assign(module.types.size(), INVALID_TYPE_HANDLE);
    analyzer.current_module_ = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    sema::TypeTable& types = analyzer.checked_.types;
    const TypeHandle u8 = types.builtin(BuiltinType::u8);
    const TypeHandle u16 = types.builtin(BuiltinType::u16);
    const TypeHandle u32 = types.builtin(BuiltinType::u32);
    const TypeHandle u64 = types.builtin(BuiltinType::u64);
    const TypeHandle i8 = types.builtin(BuiltinType::i8);
    const TypeHandle i16 = types.builtin(BuiltinType::i16);
    const TypeHandle i64 = types.builtin(BuiltinType::i64);
    const TypeHandle isize = types.builtin(BuiltinType::isize);
    const TypeHandle bool_type = types.builtin(BuiltinType::bool_);
    const TypeHandle scoped_opaque = types.opaque_struct("one.ScopedOpaque", "one_ScopedOpaque");

    EXPECT_TRUE(analyzer.can_assign(u8, u8, hex_expr));
    EXPECT_TRUE(analyzer.can_assign(u8, u8, lower_hex_expr));
    EXPECT_TRUE(analyzer.can_assign(u16, u16, upper_hex_expr));
    EXPECT_TRUE(analyzer.can_assign(u32, u32, bin_expr));
    EXPECT_TRUE(analyzer.can_assign(u64, u64, upper_bin_expr));
    EXPECT_TRUE(analyzer.can_assign(i8, i8, hex_expr));
    EXPECT_TRUE(analyzer.can_assign(i16, i16, bin_expr));
    EXPECT_TRUE(analyzer.can_assign(isize, isize, bin_expr));
    EXPECT_FALSE(analyzer.can_assign(u8, u8, invalid_digit_expr));
    EXPECT_FALSE(analyzer.can_assign(u8, u8, invalid_char_expr));
    EXPECT_FALSE(analyzer.can_assign(u8, u8, empty_expr));
    EXPECT_FALSE(analyzer.can_assign(u64, u64, overflow_expr));
    EXPECT_FALSE(analyzer.can_assign(i64, i64, signed_overflow_expr));
    EXPECT_TRUE(analyzer.can_assign(u8, u8, suffixed_u8_expr));
    EXPECT_FALSE(analyzer.can_assign(u16, u16, suffixed_u8_expr));
    EXPECT_FALSE(analyzer.can_assign(bool_type, u8, hex_expr));
    const auto analyze_integer_literal_id = [&](const ExprId expr, const TypeHandle expected) {
        const sema::SemanticAnalyzer::ExprView view = analyzer.expr_view(expr);
        return analyzer.analyze_integer_literal(expr, view.text, view.range, expected);
    };
    const auto analyze_float_literal_id = [&](const ExprId expr, const TypeHandle expected) {
        const sema::SemanticAnalyzer::ExprView view = analyzer.expr_view(expr);
        return analyzer.analyze_float_literal(expr, view.text, view.range, expected);
    };
    EXPECT_TRUE(types.same(analyze_integer_literal_id(suffixed_i8_expr, INVALID_TYPE_HANDLE), i8));
    EXPECT_TRUE(types.same(analyze_integer_literal_id(invalid_suffix_expr, INVALID_TYPE_HANDLE), types.builtin(BuiltinType::i32)));
    EXPECT_TRUE(types.same(analyze_float_literal_id(float_overflow_expr_id, types.builtin(BuiltinType::f32)), types.builtin(BuiltinType::f32)));
    EXPECT_TRUE(types.same(analyze_float_literal_id(separated_float_expr_id, INVALID_TYPE_HANDLE), types.builtin(BuiltinType::f64)));
    EXPECT_TRUE(types.same(analyze_float_literal_id(invalid_float_expr_id, types.builtin(BuiltinType::f64)), types.builtin(BuiltinType::f64)));
    EXPECT_TRUE(types.same(analyze_float_literal_id(suffixed_float_expr_id, INVALID_TYPE_HANDLE), types.builtin(BuiltinType::f32)));
    EXPECT_TRUE(types.same(analyze_float_literal_id(invalid_suffix_float_expr_id, INVALID_TYPE_HANDLE), types.builtin(BuiltinType::f64)));
    EXPECT_TRUE(types.is_str(types.builtin(BuiltinType::str)));

    const TypeHandle zero_align_enum = types.named_enum("ZeroAlign", "ZeroAlign");
    types.set_enum_underlying(zero_align_enum, u8);
    types.set_enum_payload_layout(
        zero_align_enum,
        u8,
        SEMA_TEST_ZERO_ALIGN_ENUM_PAYLOAD_SIZE,
        SEMA_TEST_ZERO_ALIGN_ENUM_PAYLOAD_ALIGN
    );
    EXPECT_GE(analyzer.abi_size(zero_align_enum), SEMA_TEST_ZERO_ALIGN_ENUM_PAYLOAD_SIZE);

    analyzer.record_pattern_c_name(syntax::INVALID_PATTERN_ID, "ignored");
    analyzer.record_pattern_c_name(syntax::PatternId {SEMA_TEST_PATTERN_FIRST_INDEX}, {});
    analyzer.record_pattern_case_name(syntax::INVALID_PATTERN_ID, "ignored");
    analyzer.record_pattern_case_name(syntax::PatternId {SEMA_TEST_PATTERN_FIRST_INDEX}, {});
    analyzer.merge_pattern_case_names(syntax::INVALID_PATTERN_ID, syntax::PatternId {SEMA_TEST_PATTERN_FIRST_INDEX});
    analyzer.checked_.pattern_case_name_ids[SEMA_TEST_PATTERN_SECOND_INDEX].insert(
        analyzer.checked_.intern_c_name("from_checked")
    );
    analyzer.merge_pattern_case_names(
        syntax::PatternId {SEMA_TEST_PATTERN_FIRST_INDEX},
        syntax::PatternId {SEMA_TEST_PATTERN_SECOND_INDEX}
    );
    EXPECT_TRUE(analyzer.checked_.pattern_case_name_ids[SEMA_TEST_PATTERN_FIRST_INDEX].contains(
        analyzer.checked_.intern_c_name("from_checked")
    ));

    EXPECT_FALSE(is_valid(analyzer.resolve_associated_type_owner(syntax::INVALID_EXPR_ID, false)));

    const TypeHandle choice_type = types.named_enum("lib.one.Choice", "lib_one_Choice");
    types.set_enum_underlying(choice_type, u8);
    static_cast<void>(add_named_type(
        analyzer,
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX),
        "Choice",
        choice_type
    ));
    static_cast<void>(add_named_type(
        analyzer,
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX),
        "ScopedOpaque",
        scoped_opaque
    ));
    static_cast<void>(add_enum_case(
        analyzer,
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX),
        "Choice_none",
        "none",
        choice_type
    ));

    const ExprId import_alias_expr = push_name(analyzer.module_, "one");
    const ExprId scoped_enum_id = push_field(analyzer.module_, import_alias_expr, "Choice");
    analyzer.checked_.expr_types.resize(analyzer.module_.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.checked_.expr_c_name_ids.resize(analyzer.module_.exprs.size(), sema::INVALID_IDENT_ID);
    const TypeHandle choice_i32 = analyzer.resolve_associated_type_owner(scoped_enum_id, false);
    EXPECT_TRUE(is_valid(choice_i32));

    const ExprId scoped_missing_type_id = push_field(analyzer.module_, import_alias_expr, "MissingScoped");
    analyzer.checked_.expr_types.resize(analyzer.module_.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.checked_.expr_c_name_ids.resize(analyzer.module_.exprs.size(), sema::INVALID_IDENT_ID);
    EXPECT_FALSE(is_valid(analyzer.resolve_associated_type_owner(scoped_missing_type_id, false)));

    const TypeHandle opaque = types.opaque_struct("Opaque", "Opaque");
    analyzer.checked_.syntax_type_handles[i32_type_id.value] = opaque;
    EXPECT_TRUE(analyzer.checked_.types.same(analyzer.resolve_type(i32_type_id), opaque));
    analyzer.checked_.syntax_type_handles[i32_type_id.value] = INVALID_TYPE_HANDLE;
    EXPECT_TRUE(types.is_void(analyzer.resolve_type(invalid_primitive_type_id)));
    EXPECT_TRUE(types.same(analyzer.resolve_type(scoped_opaque_type_id), scoped_opaque));
    EXPECT_FALSE(is_valid(analyzer.resolve_type(scoped_missing_alias_type_id)));
    EXPECT_TRUE(types.same(analyzer.resolve_type(scoped_choice_missing_args_type_id), choice_type));

    const TypeHandle record_type = types.named_struct("Record", "Record", false);
    const TypeHandle array_record = types.named_struct("ArrayRecord", "ArrayRecord", true);
    types.set_record_contains_array(array_record, true);
    static_cast<void>(add_named_type(analyzer, module_id(0), "Record", record_type));
    static_cast<void>(add_global_value(
        analyzer,
        module_id(0),
        "array_value",
        array_record,
        SymbolKind::local
    ));

    FunctionSignature static_method = indexed_function_signature(analyzer, "needs", module_id(0), bool_type);
    static_method.is_method = true;
    static_method.method_owner_type = record_type;
    static_cast<void>(add_method(analyzer, static_method, record_type));
    FunctionSignature needs_arg = indexed_function_signature(analyzer, "needs_arg", module_id(0), bool_type);
    needs_arg.is_method = true;
    needs_arg.method_owner_type = record_type;
    needs_arg.param_types = {u8};
    static_cast<void>(add_method(analyzer, needs_arg, record_type));
    FunctionSignature takes_array = indexed_function_signature(analyzer, "takes_array", module_id(0), bool_type);
    takes_array.is_method = true;
    takes_array.method_owner_type = record_type;
    takes_array.param_types = {array_record};
    static_cast<void>(add_method(analyzer, takes_array, record_type));

    const ExprId record_name = push_name(module, "Record");
    const ExprId static_method_field_id = push_field(module, record_name, "needs");
    const ExprId static_method_call_id = push_call(module, static_method_field_id);

    const ExprId missing_arg_field_id = push_field(module, record_name, "needs_arg");
    const ExprId missing_arg_call_id = push_call(module, missing_arg_field_id);

    const ExprId array_value = push_name(module, "array_value");
    const ExprId array_arg_field_id = push_field(module, record_name, "takes_array");
    const ExprId array_arg_call_id = push_call(module, array_arg_field_id, {array_value});

    const ExprId choice_name = push_name(module, "Choice");
    const ExprId none_field_id = push_field(module, choice_name, "none");
    const ExprId none_call_id = push_call(module, none_field_id);

    analyzer.checked_.expr_types.resize(module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.checked_.expr_c_name_ids.resize(module.exprs.size(), sema::INVALID_IDENT_ID);
    static_cast<void>(analyzer.analyze_call_expr(static_method_call_id, analyzer.expr_view(static_method_call_id), INVALID_TYPE_HANDLE));
    static_cast<void>(analyzer.analyze_call_expr(missing_arg_call_id, analyzer.expr_view(missing_arg_call_id), INVALID_TYPE_HANDLE));
    static_cast<void>(analyzer.analyze_call_expr(array_arg_call_id, analyzer.expr_view(array_arg_call_id), INVALID_TYPE_HANDLE));
    static_cast<void>(analyzer.analyze_call_expr(none_call_id, analyzer.expr_view(none_call_id), choice_i32));
}

TEST(CoreUnit, SemanticWhiteBoxMatchEdges) {
    syntax::AstModule module;
    module.modules = {module_info({"root"})};

    const ExprId choice_value = push_name(module, "choice");
    const ExprId int_guard = push_integer(module);
    const ExprId bool_result = push_bool(module, "true");
    const ExprId bool_subject = push_bool(module, "false");
    const ExprId int_result = push_integer(module);
    const ExprId void_value = push_name(module, "void_value");

    syntax::PatternNode payload_pattern;
    payload_pattern.kind = syntax::PatternKind::enum_case;
    payload_pattern.scoped = true;
    payload_pattern.case_name = "some";
    payload_pattern.binding_names = {"payload"};
    const syntax::PatternId payload_pattern_id = module.push_pattern(payload_pattern);

    syntax::PatternNode true_binding_pattern;
    true_binding_pattern.kind = syntax::PatternKind::literal;
    true_binding_pattern.case_name = "true";
    true_binding_pattern.binding_names = {"flag"};
    const syntax::PatternId true_binding_pattern_id = module.push_pattern(true_binding_pattern);

    syntax::PatternNode wildcard_pattern;
    wildcard_pattern.kind = syntax::PatternKind::wildcard;
    const syntax::PatternId wildcard_pattern_id = module.push_pattern(wildcard_pattern);

    syntax::PatternNode unsupported_literal_pattern;
    unsupported_literal_pattern.kind = syntax::PatternKind::literal;
    unsupported_literal_pattern.case_name = "1";
    const syntax::PatternId unsupported_literal_pattern_id = module.push_pattern(unsupported_literal_pattern);

    syntax::PatternNode missing_const_pattern;
    missing_const_pattern.kind = syntax::PatternKind::const_;
    missing_const_pattern.binding_name = "MISSING";
    const syntax::PatternId missing_const_pattern_id = module.push_pattern(missing_const_pattern);

    const ExprId enum_match_id = module.push_match_expr(
        {},
        syntax::MatchExprPayload {
            choice_value,
            {syntax::MatchArm {payload_pattern_id, int_guard, bool_result, {}}},
        }
    );

    const ExprId binding_value_match_id = module.push_match_expr(
        {},
        syntax::MatchExprPayload {
            bool_subject,
            {
                syntax::MatchArm {true_binding_pattern_id, syntax::INVALID_EXPR_ID, int_result, {}},
                syntax::MatchArm {wildcard_pattern_id, syntax::INVALID_EXPR_ID, int_result, {}},
            },
        }
    );

    const ExprId void_match_id = module.push_match_expr(
        {},
        syntax::MatchExprPayload {
            bool_subject,
            {syntax::MatchArm {wildcard_pattern_id, syntax::INVALID_EXPR_ID, void_value, {}}},
        }
    );

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(module, diagnostics);
    analyzer.checked_.expr_types.assign(module.exprs.size(), INVALID_TYPE_HANDLE);
    analyzer.checked_.expr_c_name_ids.assign(module.exprs.size(), sema::INVALID_IDENT_ID);
    analyzer.checked_.pattern_c_name_ids.assign(module.patterns.size(), sema::INVALID_IDENT_ID);
    analyzer.current_module_ = module_id(0);

    sema::TypeTable& types = analyzer.checked_.types;
    const TypeHandle void_type = types.builtin(BuiltinType::void_);
    const TypeHandle u8 = types.builtin(BuiltinType::u8);
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle choice_type = types.named_enum("Choice", "Choice");
    const TypeHandle record_type = types.named_struct("Record", "Record", false);
    types.set_enum_underlying(choice_type, u8);

    const EnumCaseInfo* const some_case = add_enum_case(
        analyzer,
        module_id(0),
        "some",
        "some",
        choice_type,
        i32,
        {i32}
    ).second;
    ASSERT_NE(some_case, nullptr);
    static_cast<void>(add_global_value(analyzer, module_id(0), "choice", choice_type, SymbolKind::local));
    static_cast<void>(add_global_value(analyzer, module_id(0), "void_value", void_type, SymbolKind::local));

    std::vector<sema::SemanticAnalyzer::PatternBinding> invalid_bindings;
    EXPECT_FALSE(analyzer.analyze_pattern(syntax::INVALID_PATTERN_ID, choice_type, invalid_bindings));
    EXPECT_FALSE(analyzer.pattern_is_irrefutable(syntax::INVALID_PATTERN_ID, choice_type));
    std::vector<sema::SemanticAnalyzer::PatternBinding> missing_const_bindings;
    EXPECT_FALSE(analyzer.analyze_pattern(missing_const_pattern_id, i32, missing_const_bindings));

    EXPECT_TRUE(types.is_bool(analyzer.analyze_match_expr(enum_match_id, analyzer.expr_view(enum_match_id), INVALID_TYPE_HANDLE)));
    EXPECT_TRUE(types.is_integer(analyzer.analyze_match_expr(
        binding_value_match_id,
        analyzer.expr_view(binding_value_match_id),
        INVALID_TYPE_HANDLE
    )));
    EXPECT_FALSE(is_valid(analyzer.analyze_match_expr(void_match_id, analyzer.expr_view(void_match_id), INVALID_TYPE_HANDLE)));

    bool covered_true = false;
    bool covered_false = false;
    bool value_saw_wildcard = false;
    EXPECT_EQ(analyzer.analyze_single_value_pattern(
        syntax::INVALID_PATTERN_ID,
        types.builtin(BuiltinType::bool_),
        covered_true,
        covered_false,
        value_saw_wildcard
    ), nullptr);
    EXPECT_EQ(analyzer.analyze_single_value_pattern(
        unsupported_literal_pattern_id,
        record_type,
        covered_true,
        covered_false,
        value_saw_wildcard
    ), nullptr);
    value_saw_wildcard = true;
    EXPECT_EQ(analyzer.analyze_single_value_pattern(
        unsupported_literal_pattern_id,
        types.builtin(BuiltinType::bool_),
        covered_true,
        covered_false,
        value_saw_wildcard
    ), nullptr);
    value_saw_wildcard = false;
    EXPECT_EQ(analyzer.analyze_single_value_pattern(
        wildcard_pattern_id,
        types.builtin(BuiltinType::bool_),
        covered_true,
        covered_false,
        value_saw_wildcard
    ), nullptr);
    EXPECT_EQ(analyzer.analyze_single_value_pattern(
        payload_pattern_id,
        types.builtin(BuiltinType::bool_),
        covered_true,
        covered_false,
        value_saw_wildcard
    ), nullptr);
}

TEST(CoreUnit, SemanticWhiteBoxConstEvaluationTraversal) {
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"lib", "one"}),
    };
    module.modules[0].imports = {
        resolved_import(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), SEMA_TEST_IMPORT_ALIAS_ONE),
    };

    const ExprId scoped_value_expr = push_name(module, SEMA_TEST_CONST_VALUE_NAME, SEMA_TEST_IMPORT_ALIAS_ONE);

    const ExprId field_expr_id = module.push_field_expr({}, syntax::FieldExprPayload {});

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(module, diagnostics);
    analyzer.checked_.expr_c_name_ids.assign(module.exprs.size(), sema::INVALID_IDENT_ID);
    analyzer.checked_.enum_cases.clear();
    analyzer.current_module_ = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    const TypeHandle i32 = analyzer.checked_.types.builtin(BuiltinType::i32);
    const Symbol* const_value_symbol = add_global_value(
        analyzer,
        module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX),
        SEMA_TEST_CONST_VALUE_NAME,
        i32,
        SymbolKind::const_
    ).second;
    ASSERT_NE(const_value_symbol, nullptr);
    const sema::ModuleLookupKey const_value_key {
        const_value_symbol->module.value,
        const_value_symbol->name_id,
    };

    sema::SemaSet<sema::ModuleLookupKey, sema::ModuleLookupKeyHash> dependencies =
        sema::make_sema_set<sema::ModuleLookupKey, sema::ModuleLookupKeyHash>(
            *analyzer.arena_,
            sema::ModuleLookupKeyHash {}
        );
    EXPECT_TRUE(analyzer.is_const_evaluable_expr(scoped_value_expr, dependencies));
    EXPECT_EQ(dependencies.count(const_value_key), 1u);

    sema::EnumCaseInfo case_info;
    case_info.c_name = SEMA_TEST_ENUM_CASE_C_NAME;
    case_info.name = std::string(SEMA_TEST_ENUM_CASE_C_NAME);
    case_info.name_id = intern_identifier(analyzer, SEMA_TEST_ENUM_CASE_C_NAME);
    case_info.module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);
    analyzer.checked_.enum_cases.emplace(
        sema::ModuleLookupKey {case_info.module.value, case_info.name_id},
        case_info
    );
    analyzer.checked_.expr_c_name_ids[field_expr_id.value] = analyzer.checked_.intern_c_name(SEMA_TEST_ENUM_CASE_C_NAME);

    dependencies.clear();
    EXPECT_TRUE(analyzer.is_const_evaluable_expr(field_expr_id, dependencies));
    EXPECT_TRUE(dependencies.empty());
}

TEST(CoreUnit, SemanticWhiteBoxConstEvaluationRejectsUnsupportedShapes) {
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
    };

    const ExprId missing_name = push_name(module, SEMA_TEST_MISSING_VALUE_NAME);
    const ExprId local_name = push_name(module, SEMA_TEST_LOCAL_VALUE_NAME);
    const ExprId enum_name = push_name(module, SEMA_TEST_ENUM_VALUE_NAME);
    const ExprId integer_literal = push_integer(module);

    const ExprId unsupported_unary_id = push_unary(module, syntax::UnaryOp::address_of, integer_literal);
    const ExprId invalid_child_cast_id = module.push_cast_like_expr(
        syntax::ExprKind::cast,
        {},
        syntax::CastExprPayload {syntax::INVALID_TYPE_ID, syntax::INVALID_EXPR_ID}
    );
    const ExprId empty_struct_literal_id = module.push_struct_literal_expr({}, syntax::StructLiteralExprPayload {});
    const ExprId invalid_binary_id = push_binary(
        module,
        SEMA_TEST_INVALID_BINARY_OP,
        integer_literal,
        integer_literal
    );
    const ExprId plain_field_id = module.push_field_expr({}, syntax::FieldExprPayload {});

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(module, diagnostics);
    analyzer.checked_.expr_c_name_ids.assign(module.exprs.size(), sema::INVALID_IDENT_ID);
    analyzer.current_module_ = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    const TypeHandle i32 = analyzer.checked_.types.builtin(BuiltinType::i32);
    static_cast<void>(add_global_value(
        analyzer,
        module_id(SEMA_TEST_ROOT_MODULE_INDEX),
        SEMA_TEST_LOCAL_VALUE_NAME,
        i32,
        SymbolKind::local
    ));
    static_cast<void>(add_global_value(
        analyzer,
        module_id(SEMA_TEST_ROOT_MODULE_INDEX),
        SEMA_TEST_ENUM_VALUE_NAME,
        i32,
        SymbolKind::enum_case
    ));

    sema::SemaSet<sema::ModuleLookupKey, sema::ModuleLookupKeyHash> dependencies =
        sema::make_sema_set<sema::ModuleLookupKey, sema::ModuleLookupKeyHash>(
            *analyzer.arena_,
            sema::ModuleLookupKeyHash {}
        );
    EXPECT_FALSE(analyzer.is_const_evaluable_expr(syntax::INVALID_EXPR_ID, dependencies));
    EXPECT_FALSE(analyzer.is_const_evaluable_expr(missing_name, dependencies));
    EXPECT_FALSE(analyzer.is_const_evaluable_expr(local_name, dependencies));
    EXPECT_FALSE(analyzer.is_const_evaluable_expr(enum_name, dependencies));
    EXPECT_FALSE(analyzer.is_const_evaluable_expr(unsupported_unary_id, dependencies));
    EXPECT_FALSE(analyzer.is_const_evaluable_expr(invalid_child_cast_id, dependencies));
    EXPECT_TRUE(analyzer.is_const_evaluable_expr(empty_struct_literal_id, dependencies));
    EXPECT_FALSE(analyzer.is_const_evaluable_expr(invalid_binary_id, dependencies));
    EXPECT_FALSE(analyzer.is_const_evaluable_expr(plain_field_id, dependencies));
    EXPECT_TRUE(dependencies.empty());
}

TEST(CoreUnit, SemanticWhiteBoxTypeTableUnknownDisplayFallbacks) {
    sema::TypeTable builtin_table;
    const TypeHandle builtin_type = builtin_table.builtin(BuiltinType::i32);
    ASSERT_LT(builtin_type.value, builtin_table.types_.size());
    builtin_table.types_[builtin_type.value].builtin = SEMA_TEST_INVALID_BUILTIN_TYPE;
    EXPECT_EQ(builtin_table.display_name(builtin_type), SEMA_TEST_UNKNOWN_TYPE_DISPLAY);

    sema::TypeTable kind_table;
    const TypeHandle kind_type = kind_table.builtin(BuiltinType::i32);
    ASSERT_LT(kind_type.value, kind_table.types_.size());
    kind_table.types_[kind_type.value].kind = SEMA_TEST_INVALID_TYPE_KIND;
    EXPECT_EQ(kind_table.display_name(kind_type), SEMA_TEST_UNKNOWN_TYPE_DISPLAY);

    const TypeHandle out_of_range_type {static_cast<base::u32>(kind_table.types_.size())};
    EXPECT_FALSE(kind_table.is_integer(out_of_range_type));
    EXPECT_FALSE(kind_table.is_float(out_of_range_type));
    EXPECT_FALSE(kind_table.is_bool(out_of_range_type));
    EXPECT_FALSE(kind_table.is_str(out_of_range_type));
    EXPECT_FALSE(kind_table.is_void(out_of_range_type));
    EXPECT_FALSE(kind_table.is_pointer(out_of_range_type));
    EXPECT_FALSE(kind_table.is_array(out_of_range_type));
    EXPECT_FALSE(kind_table.contains_array(out_of_range_type));
    EXPECT_EQ(kind_table.display_name(out_of_range_type), SEMA_TEST_INVALID_TYPE_DISPLAY);
    EXPECT_EQ(kind_table.c_name(out_of_range_type), "void");

    EXPECT_FALSE(kind_table.is_integer(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(kind_table.is_float(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(kind_table.is_bool(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(kind_table.is_str(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(kind_table.is_char(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(kind_table.is_void(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(kind_table.is_pointer(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(kind_table.is_array(INVALID_TYPE_HANDLE));
    EXPECT_FALSE(kind_table.contains_array(INVALID_TYPE_HANDLE));

    EXPECT_FALSE(kind_table.is_integer(builtin_type));
    EXPECT_FALSE(kind_table.is_float(builtin_type));
    EXPECT_FALSE(kind_table.is_bool(builtin_type));
    EXPECT_FALSE(kind_table.is_str(builtin_type));
    EXPECT_FALSE(kind_table.is_char(builtin_type));
    EXPECT_FALSE(kind_table.is_void(builtin_type));
    EXPECT_FALSE(kind_table.is_pointer(builtin_type));
    EXPECT_FALSE(kind_table.is_array(builtin_type));
    EXPECT_FALSE(kind_table.contains_array(builtin_type));

    sema::TypeTable storage_table;
    const TypeHandle i32 = storage_table.builtin(BuiltinType::i32);
    const TypeHandle bool_type = storage_table.builtin(BuiltinType::bool_);
    const TypeHandle pointer = storage_table.pointer(PointerMutability::const_, i32);
    const TypeHandle reference = storage_table.reference(PointerMutability::mut, bool_type);
    const TypeHandle array = storage_table.array(4, i32);
    const TypeHandle slice = storage_table.slice(PointerMutability::const_, i32);
    const TypeHandle tuple = storage_table.tuple({i32, bool_type});
    const TypeHandle function = storage_table.function(
        sema::FunctionCallConv::aurex,
        false,
        {i32, pointer},
        bool_type
    );
    const TypeHandle generic = storage_table.generic_param("test.T", "T");
    storage_table.set_generic_instance(tuple, "tuple.origin", {generic});
    storage_table.set_record_contains_array(tuple, true);
    storage_table.set_enum_underlying(function, i32);
    storage_table.set_enum_payload_layout(function, array, 8, 4);

    sema::TypeTable copied_table(storage_table);
    EXPECT_EQ(copied_table.display_name(pointer), "*const i32");
    EXPECT_TRUE(copied_table.is_reference(reference));
    EXPECT_TRUE(copied_table.is_slice(slice));
    sema::TypeTable assigned_table;
    assigned_table = storage_table;
    EXPECT_TRUE(assigned_table.contains_array(tuple));
    sema::TypeTable moved_table(std::move(copied_table));
    EXPECT_EQ(moved_table.get(function).enum_payload_align, 4U);
    sema::TypeTable move_assigned_table;
    move_assigned_table = std::move(assigned_table);
    EXPECT_TRUE(move_assigned_table.is_array(array));
}

TEST(CoreUnit, IdentifierInternerStableIdsAndNonAllocatingMisses) {
    IdentifierInterner interner;
    interner.reserve(2);
    EXPECT_GT(interner.arena_blocks(), 0U);
    EXPECT_GT(interner.arena_bytes(), 0U);

    EXPECT_FALSE(sema::is_valid(interner.find("alpha")));
    const IdentId alpha = interner.intern("alpha");
    const IdentId beta = interner.intern("beta");

    EXPECT_TRUE(sema::is_valid(alpha));
    EXPECT_TRUE(sema::is_valid(beta));
    EXPECT_NE(alpha, beta);
    EXPECT_EQ(interner.intern("alpha"), alpha);
    EXPECT_EQ(interner.find("alpha"), alpha);
    EXPECT_EQ(interner.find("beta"), beta);
    EXPECT_EQ(interner.text(alpha), "alpha");
    EXPECT_EQ(interner.text(beta), "beta");
    EXPECT_EQ(interner.text(sema::INVALID_IDENT_ID), "");
    EXPECT_EQ(interner.text(IdentId {IdentId::INVALID_VALUE - 1}), "");
    EXPECT_EQ(interner.find("missing"), sema::INVALID_IDENT_ID);
    EXPECT_EQ(interner.size(), 2U);
    EXPECT_GT(interner.arena_blocks(), 0U);
    EXPECT_GT(interner.arena_bytes(), 0U);

    IdentifierInterner copied = interner;
    EXPECT_EQ(copied.find("alpha"), alpha);
    EXPECT_EQ(copied.text(beta), "beta");
    EXPECT_GT(copied.arena_blocks(), 0U);

    IdentifierInterner assigned;
    assigned = copied;
    EXPECT_EQ(assigned.find("alpha"), alpha);
    EXPECT_EQ(assigned.text(beta), "beta");
    IdentifierInterner* const assigned_ref = &assigned;
    assigned = *assigned_ref;
    EXPECT_EQ(assigned.size(), 2U);
    EXPECT_EQ(assigned.find("beta"), beta);

    IdentifierInterner moved(std::move(assigned));
    EXPECT_EQ(moved.find("alpha"), alpha);
    EXPECT_EQ(moved.text(beta), "beta");
    EXPECT_EQ(assigned.size(), 0U);
    EXPECT_EQ(assigned.find("alpha"), sema::INVALID_IDENT_ID);
    EXPECT_EQ(assigned.arena_blocks(), 0U);

    const IdentId gamma = assigned.intern("gamma");
    EXPECT_EQ(gamma.value, 0U);
    EXPECT_EQ(assigned.find("gamma"), gamma);
    EXPECT_EQ(assigned.text(gamma), "gamma");
    EXPECT_GT(assigned.arena_blocks(), 0U);

    IdentifierInterner move_assigned;
    static_cast<void>(move_assigned.intern("stale"));
    move_assigned = std::move(moved);
    EXPECT_EQ(move_assigned.find("alpha"), alpha);
    EXPECT_EQ(move_assigned.find("stale"), sema::INVALID_IDENT_ID);
    EXPECT_EQ(move_assigned.text(beta), "beta");
    EXPECT_EQ(moved.size(), 0U);
    EXPECT_EQ(moved.arena_blocks(), 0U);
    moved.reserve(1);
    EXPECT_GT(moved.arena_blocks(), 0U);
    EXPECT_EQ(move_assigned.intern(""), sema::INVALID_IDENT_ID);

    IdentifierInterner* const move_assigned_ref = &move_assigned;
    move_assigned = std::move(*move_assigned_ref);
    EXPECT_EQ(move_assigned.find("alpha"), alpha);
}

TEST(CoreUnit, SymbolTableCoversLookupsScopeRemovalAndInvalidIds) {
    base::DiagnosticSink diagnostics;
    sema::SymbolTable symbols;
    IdentifierInterner identifiers;
    const IdentId outer_id = identifiers.intern(SEMA_TEST_SYMBOL_OUTER_NAME);
    const IdentId inner_id = identifiers.intern(SEMA_TEST_SYMBOL_INNER_NAME);
    const IdentId duplicate_id = identifiers.intern(SEMA_TEST_SYMBOL_DUPLICATE_NAME);
    const IdentId missing_id = identifiers.intern("missing_symbol");
    EXPECT_EQ(symbols.find(syntax::INVALID_IDENT_ID), nullptr);

    const auto outer_inserted = symbols.insert(
        symbol(SymbolKind::local, SEMA_TEST_SYMBOL_OUTER_NAME, module_id(0), INVALID_TYPE_HANDLE, false, syntax::Visibility::public_, outer_id),
        diagnostics
    );
    ASSERT_TRUE(outer_inserted) << outer_inserted.error().message;
    ASSERT_NE(symbols.find(outer_id), nullptr);
    EXPECT_EQ(symbols.find(missing_id), nullptr);
    EXPECT_EQ(symbols.get(sema::INVALID_SYMBOL_ID), nullptr);
    EXPECT_EQ(symbols.get(sema::SymbolId {1}), nullptr);

    symbols.push_scope();
    const auto inner_inserted = symbols.insert(
        symbol(SymbolKind::local, SEMA_TEST_SYMBOL_INNER_NAME, module_id(0), INVALID_TYPE_HANDLE, false, syntax::Visibility::public_, inner_id),
        diagnostics
    );
    ASSERT_TRUE(inner_inserted) << inner_inserted.error().message;
    EXPECT_NE(symbols.find(inner_id), nullptr);
    EXPECT_NE(symbols.find(outer_id), nullptr);
    const auto shadowed_outer_inserted = symbols.insert(
        symbol(SymbolKind::local, SEMA_TEST_SYMBOL_OUTER_NAME, module_id(0), INVALID_TYPE_HANDLE, false, syntax::Visibility::public_, outer_id),
        diagnostics
    );
    ASSERT_TRUE(shadowed_outer_inserted) << shadowed_outer_inserted.error().message;
    EXPECT_NE(symbols.find(outer_id), nullptr);

    sema::SymbolTable copied_symbols(symbols);
    EXPECT_NE(copied_symbols.find(inner_id), nullptr);
    EXPECT_NE(copied_symbols.find(outer_id), nullptr);
    sema::SymbolTable assigned_symbols;
    assigned_symbols = symbols;
    EXPECT_NE(assigned_symbols.find(inner_id), nullptr);
    sema::SymbolTable moved_symbols(std::move(copied_symbols));
    EXPECT_NE(moved_symbols.find(outer_id), nullptr);
    sema::SymbolTable move_assigned_symbols;
    move_assigned_symbols = std::move(assigned_symbols);
    EXPECT_NE(move_assigned_symbols.find(inner_id), nullptr);

    const auto duplicate_name_inserted = symbols.insert(
        symbol(SymbolKind::local, SEMA_TEST_SYMBOL_DUPLICATE_NAME, module_id(0), INVALID_TYPE_HANDLE, false, syntax::Visibility::public_, duplicate_id),
        diagnostics
    );
    ASSERT_TRUE(duplicate_name_inserted) << duplicate_name_inserted.error().message;
    const auto duplicate_shadow = symbols.insert(
        symbol(SymbolKind::local, SEMA_TEST_SYMBOL_DUPLICATE_NAME, module_id(0), INVALID_TYPE_HANDLE, false, syntax::Visibility::public_, duplicate_id),
        diagnostics
    );
    ASSERT_FALSE(duplicate_shadow);
    EXPECT_EQ(duplicate_shadow.error().code, base::ErrorCode::sema_error);
    EXPECT_TRUE(diagnostics.has_error());

    symbols.pop_scope();
    EXPECT_EQ(symbols.find(inner_id), nullptr);
    ASSERT_NE(symbols.find(outer_id), nullptr);
}

} // namespace aurex::test
