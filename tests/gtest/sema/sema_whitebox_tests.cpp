#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
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
using sema::GenericEnumTemplateInfo;
using sema::GenericFunctionTemplateInfo;
using sema::GenericStructTemplateInfo;
using sema::PointerMutability;
using sema::StructFieldInfo;
using sema::StructInfo;
using sema::Symbol;
using sema::SymbolKind;
using sema::TypeHandle;
using sema::TypeKind;
using sema::invalid_type_handle;
using sema::is_valid;
using syntax::ExprId;
using syntax::ModuleId;
using syntax::TypeId;

constexpr u32 SEMA_TEST_ROOT_MODULE_INDEX = 0;
constexpr u32 SEMA_TEST_LIB_ONE_MODULE_INDEX = 1;
constexpr u32 SEMA_TEST_MISSING_MODULE_INDEX = 99;
constexpr base::u32 SEMA_TEST_PATTERN_FIRST_INDEX = 0;
constexpr base::u32 SEMA_TEST_PATTERN_SECOND_INDEX = 1;
constexpr base::u32 SEMA_TEST_PATTERN_THIRD_INDEX = 2;
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
constexpr syntax::PrimitiveTypeKind SEMA_TEST_INVALID_PRIMITIVE_KIND = static_cast<syntax::PrimitiveTypeKind>(99);
constexpr std::string_view SEMA_TEST_FLOAT_OVERFLOW_LITERAL = "1e999999999";
constexpr std::string_view SEMA_TEST_FLOAT_WITH_SEPARATOR_LITERAL = "1_2.5";
constexpr std::string_view SEMA_TEST_FLOAT_INVALID_TRAILING_LITERAL = "1.0x";
constexpr syntax::BinaryOp SEMA_TEST_INVALID_BINARY_OP = static_cast<syntax::BinaryOp>(99);

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
    const TypeHandle return_type
) {
    FunctionSignature signature;
    signature.name = std::string(name);
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
    const syntax::Visibility visibility = syntax::Visibility::public_
) {
    return Symbol {
        kind,
        std::string(name),
        std::string(name),
        module,
        type,
        {},
        is_mutable,
        visibility,
    };
}

[[nodiscard]] ExprId push_name(
    syntax::AstModule& module,
    const std::string_view text,
    const std::string_view scope = {}
) {
    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::name;
    expr.text = text;
    expr.scope_name = scope;
    return module.push_expr(expr);
}

[[nodiscard]] ExprId push_integer(syntax::AstModule& module) {
    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::integer_literal;
    expr.text = SEMA_TEST_INTEGER_LITERAL_ONE;
    return module.push_expr(expr);
}

[[nodiscard]] ExprId push_integer_text(syntax::AstModule& module, const std::string_view text) {
    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::integer_literal;
    expr.text = text;
    return module.push_expr(expr);
}

[[nodiscard]] ExprId push_bool(syntax::AstModule& module, const std::string_view text) {
    syntax::ExprNode expr;
    expr.kind = syntax::ExprKind::bool_literal;
    expr.text = text;
    return module.push_expr(expr);
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
        resolved_import(syntax::invalid_module_id, "broken"),
    };
    module.modules[1].imports = {
        resolved_import(module_id(3), "pub", syntax::Visibility::public_),
    };

    const ExprId value_expr = push_name(module, "value");
    const ExprId scoped_value_expr = push_name(module, "shared", "one");
    const ExprId missing_scoped_value_expr = push_name(module, "shared", "missing");
    const ExprId ptr_expr = push_name(module, "ptr");
    syntax::ExprNode field_expr;
    field_expr.kind = syntax::ExprKind::field;
    field_expr.object = ptr_expr;
    field_expr.field_name = "field";
    const ExprId field_id = module.push_expr(field_expr);
    syntax::ExprNode value_field_expr = field_expr;
    value_field_expr.object = value_expr;
    const ExprId value_field_id = module.push_expr(value_field_expr);
    syntax::ExprNode nested_value_field_expr = field_expr;
    nested_value_field_expr.object = value_field_id;
    const ExprId nested_value_field_id = module.push_expr(nested_value_field_expr);
    syntax::ExprNode index_expr;
    index_expr.kind = syntax::ExprKind::index;
    index_expr.object = ptr_expr;
    index_expr.index = push_integer(module);
    const ExprId index_id = module.push_expr(index_expr);
    syntax::ExprNode value_index_expr = index_expr;
    value_index_expr.object = value_expr;
    const ExprId value_index_id = module.push_expr(value_index_expr);
    syntax::ExprNode nested_value_index_expr = index_expr;
    nested_value_index_expr.object = nested_value_field_id;
    const ExprId nested_value_index_id = module.push_expr(nested_value_index_expr);
    syntax::ExprNode deref_expr;
    deref_expr.kind = syntax::ExprKind::unary;
    deref_expr.unary_op = syntax::UnaryOp::dereference;
    deref_expr.unary_operand = ptr_expr;
    const ExprId deref_id = module.push_expr(deref_expr);
    syntax::ExprNode not_expr = deref_expr;
    not_expr.unary_op = syntax::UnaryOp::logical_not;
    const ExprId not_id = module.push_expr(not_expr);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(module, diagnostics);
    analyzer.checked_.expr_types.assign(module.exprs.size(), invalid_type_handle);
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
    types.set_enum_underlying(enum_type, u16);
    types.set_enum_underlying(payload_enum_type, u8);
    types.set_enum_payload_layout(payload_enum_type, u64, sizeof(std::uint64_t), alignof(std::uint64_t));

    StructInfo record;
    record.name = "Record";
    record.module = module_id(1);
    record.type = record_type;
    record.fields = {
        StructFieldInfo {"tag", "tag", module_id(1), u8},
        StructFieldInfo {"value", "value", module_id(1), u64},
    };
    analyzer.checked_.structs.emplace(analyzer.module_key(module_id(1), "Record"), record);

    EXPECT_EQ(analyzer.abi_size(invalid_type_handle), SEMA_TEST_ABI_INVALID_SIZE);
    EXPECT_EQ(analyzer.abi_align(invalid_type_handle), SEMA_TEST_ABI_MIN_ALIGNMENT);
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
    EXPECT_EQ(analyzer.abi_align(i8), alignof(std::uint8_t));
    EXPECT_EQ(analyzer.abi_align(i64), alignof(std::uint64_t));
    EXPECT_EQ(analyzer.abi_align(isize), alignof(std::ptrdiff_t));
    EXPECT_EQ(analyzer.abi_align(usize), alignof(std::size_t));
    EXPECT_EQ(analyzer.abi_align(f32), alignof(float));
    EXPECT_EQ(analyzer.abi_align(f64), alignof(double));
    EXPECT_EQ(analyzer.abi_align(str), alignof(void*));
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

    EXPECT_FALSE(analyzer.is_valid_cast(syntax::ExprKind::cast, invalid_type_handle, i32));
    EXPECT_TRUE(analyzer.is_valid_cast(syntax::ExprKind::cast, f64, i32));
    EXPECT_TRUE(analyzer.is_valid_cast(syntax::ExprKind::ptr_cast, const_ptr_i32, ptr_i32));
    EXPECT_TRUE(analyzer.is_valid_cast(syntax::ExprKind::bit_cast, i32, i32));
    EXPECT_TRUE(analyzer.is_valid_cast(syntax::ExprKind::bit_cast, u32, i32));
    EXPECT_FALSE(analyzer.is_valid_cast(syntax::ExprKind::bit_cast, bool_type, i8));
    EXPECT_FALSE(analyzer.is_valid_cast(syntax::ExprKind::bit_cast, str, ptr_i32));
    EXPECT_FALSE(analyzer.is_valid_cast(syntax::ExprKind::ptr_addr, u32, ptr_i32));
    EXPECT_FALSE(analyzer.is_valid_storage_type(invalid_type_handle));
    EXPECT_FALSE(analyzer.is_valid_storage_type(void_type));
    EXPECT_FALSE(analyzer.is_valid_storage_type(opaque_type));
    EXPECT_TRUE(analyzer.is_valid_storage_type(nested_array_i16));
    EXPECT_FALSE(analyzer.is_valid_storage_type(array_void));
    EXPECT_FALSE(analyzer.is_valid_storage_type(array_opaque));
    EXPECT_FALSE(analyzer.is_valid_storage_type(overflowing_array));

    analyzer.global_values_.emplace(analyzer.module_key(module_id(0), "value"), symbol(SymbolKind::local, "value", module_id(0), i32, true));
    analyzer.global_values_.emplace(analyzer.module_key(module_id(0), "ptr"), symbol(SymbolKind::local, "ptr", module_id(0), ptr_i32, true));
    analyzer.global_values_.emplace(analyzer.module_key(module_id(1), "shared"), symbol(SymbolKind::local, "shared", module_id(1), i32, true));

    EXPECT_TRUE(analyzer.is_place_expr(value_expr));
    EXPECT_TRUE(analyzer.is_place_expr(scoped_value_expr));
    EXPECT_FALSE(analyzer.is_place_expr(missing_scoped_value_expr));
    EXPECT_TRUE(analyzer.is_place_expr(field_id));
    EXPECT_TRUE(analyzer.is_place_expr(index_id));
    EXPECT_TRUE(analyzer.is_place_expr(nested_value_field_id));
    EXPECT_TRUE(analyzer.is_place_expr(nested_value_index_id));
    EXPECT_TRUE(analyzer.is_place_expr(deref_id));
    EXPECT_FALSE(analyzer.is_place_expr(not_id));
    EXPECT_FALSE(analyzer.is_place_expr(syntax::invalid_expr_id));
    EXPECT_TRUE(analyzer.is_writable_place(value_expr));
    EXPECT_TRUE(analyzer.is_writable_place(scoped_value_expr));
    EXPECT_FALSE(analyzer.is_writable_place(missing_scoped_value_expr));
    EXPECT_TRUE(analyzer.is_writable_place(field_id));
    EXPECT_TRUE(analyzer.is_writable_place(index_id));
    EXPECT_TRUE(analyzer.is_writable_place(value_field_id));
    EXPECT_TRUE(analyzer.is_writable_place(value_index_id));
    EXPECT_TRUE(analyzer.is_writable_place(nested_value_field_id));
    EXPECT_TRUE(analyzer.is_writable_place(nested_value_index_id));
    EXPECT_TRUE(analyzer.is_writable_place(deref_id));
    EXPECT_FALSE(analyzer.is_writable_place(not_id));
    EXPECT_FALSE(analyzer.is_writable_place(syntax::invalid_expr_id));

    analyzer.current_module_ = syntax::invalid_module_id;
    EXPECT_FALSE(syntax::is_valid(analyzer.resolve_import_alias("missing", {})));
    analyzer.current_module_ = module_id(0);
    module.modules[0].imports.push_back(resolved_import(module_id(2), "one"));
    EXPECT_FALSE(syntax::is_valid(analyzer.resolve_import_alias("one", {})));
    module.modules[0].imports.pop_back();
    EXPECT_FALSE(syntax::is_valid(analyzer.resolve_import_alias("missing", {})));

    EXPECT_TRUE(analyzer.visible_modules(syntax::invalid_module_id).empty());
    EXPECT_EQ(analyzer.visible_modules(module_id(SEMA_TEST_MISSING_MODULE_INDEX)).size(), 1U);
    const std::vector<ModuleId> visible = analyzer.visible_modules(module_id(0));
    ASSERT_GE(visible.size(), 4U);
    EXPECT_EQ(analyzer.module_name(syntax::invalid_module_id), "<unknown>");
    EXPECT_EQ(analyzer.qualified_name(syntax::invalid_module_id, "Name"), "Name");
    EXPECT_EQ(analyzer.c_symbol_name(syntax::invalid_module_id, "Name"), "Name");
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
    analyzer.checked_.expr_types.assign(module.exprs.size(), invalid_type_handle);
    analyzer.current_module_ = module_id(0);
    sema::TypeTable& types = analyzer.checked_.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle bool_type = types.builtin(BuiltinType::bool_);
    const TypeHandle record_type = types.named_struct("Record", "Record", false);
    const TypeHandle array_record = types.named_struct("ArrayRecord", "ArrayRecord", true);
    types.set_record_properties(array_record, true, false);
    const TypeHandle ptr_record = types.pointer(PointerMutability::mut, record_type);
    const TypeHandle const_ptr_record = types.pointer(PointerMutability::const_, record_type);
    const TypeHandle enum_type = types.named_enum("Choice", "Choice");

    analyzer.global_values_.emplace(analyzer.module_key(module_id(0), "value"), symbol(SymbolKind::local, "value", module_id(0), record_type, true));

    analyzer.named_types_.emplace(analyzer.module_key(module_id(1), "Shared"), i32);
    analyzer.named_types_.emplace(analyzer.module_key(module_id(2), "Shared"), bool_type);
    EXPECT_FALSE(is_valid(analyzer.find_type_in_visible_modules("Shared", {}, false)));

    analyzer.named_types_.emplace(analyzer.module_key(module_id(1), "Private"), record_type);
    analyzer.type_visibilities_.emplace(analyzer.module_key(module_id(1), "Private"), syntax::Visibility::private_);
    EXPECT_FALSE(is_valid(analyzer.find_type_in_module(module_id(1), "Private", {}, false)));
    EXPECT_FALSE(is_valid(analyzer.find_type_in_module(syntax::invalid_module_id, "Missing", {}, false)));
    EXPECT_FALSE(is_valid(analyzer.find_type_in_module(module_id(1), "Missing", {}, false)));

    FunctionSignature one = function_signature("ambiguous", module_id(1), i32);
    FunctionSignature two = function_signature("ambiguous", module_id(2), i32);
    analyzer.checked_.functions.emplace(analyzer.module_key(module_id(1), "ambiguous"), one);
    analyzer.checked_.functions.emplace(analyzer.module_key(module_id(2), "ambiguous"), two);
    EXPECT_EQ(analyzer.find_function_in_visible_modules("ambiguous", {}), nullptr);
    FunctionSignature private_function = function_signature("hidden", module_id(1), i32);
    private_function.visibility = syntax::Visibility::private_;
    analyzer.checked_.functions.emplace(analyzer.module_key(module_id(1), "hidden"), private_function);
    EXPECT_EQ(analyzer.find_function_in_module(module_id(1), "hidden", {}), nullptr);
    EXPECT_EQ(analyzer.find_function_in_module(module_id(1), "missing", {}), nullptr);

    EnumCaseInfo one_case;
    one_case.name = "same";
    one_case.case_name = "same";
    one_case.module = module_id(1);
    one_case.type = enum_type;
    EnumCaseInfo two_case = one_case;
    two_case.module = module_id(2);
    analyzer.checked_.enum_cases.emplace(analyzer.module_key(module_id(1), "same"), one_case);
    analyzer.checked_.enum_cases.emplace(analyzer.module_key(module_id(2), "same"), two_case);
    EXPECT_EQ(analyzer.find_enum_case_in_visible_modules("same", {}), nullptr);
    EXPECT_EQ(analyzer.find_enum_case_in_visible_modules("missing", {}), nullptr);

    analyzer.named_types_.emplace(analyzer.module_key(module_id(0), "Record"), record_type);
    analyzer.named_types_.emplace(analyzer.module_key(module_id(0), "Choice"), enum_type);
    EXPECT_EQ(analyzer.find_enum_case_by_scoped_name("Record", "missing", {}, true), nullptr);
    EXPECT_EQ(analyzer.find_enum_case_by_scoped_name("Choice", "missing", {}, true), nullptr);
    EXPECT_EQ(analyzer.find_enum_constructor(syntax::invalid_expr_id, true), nullptr);

    analyzer.global_values_.emplace(analyzer.module_key(module_id(1), "ambiguous"), symbol(SymbolKind::const_, "ambiguous", module_id(1), i32));
    analyzer.global_values_.emplace(analyzer.module_key(module_id(2), "ambiguous"), symbol(SymbolKind::const_, "ambiguous", module_id(2), i32));
    EXPECT_EQ(analyzer.find_symbol("ambiguous", {}), nullptr);
    EXPECT_EQ(analyzer.find_symbol("missing", {}), nullptr);
    analyzer.global_values_.emplace(
        analyzer.module_key(module_id(1), "private_value"),
        symbol(SymbolKind::const_, "private_value", module_id(1), i32, false, syntax::Visibility::private_)
    );
    EXPECT_EQ(analyzer.find_symbol_in_module(module_id(1), "missing", {}, true), nullptr);
    EXPECT_EQ(analyzer.find_symbol_in_module(module_id(1), "private_value", {}, true), nullptr);

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

    FunctionSignature method = function_signature("run", module_id(1), i32);
    method.is_method = true;
    method.has_self_param = true;
    method.method_owner_type = record_type;
    FunctionSignature other_method = method;
    other_method.module = module_id(2);
    analyzer.checked_.functions.emplace(analyzer.method_key(module_id(1), record_type, "run"), method);
    analyzer.checked_.functions.emplace(analyzer.method_key(module_id(2), record_type, "run"), other_method);
    FunctionSignature static_method = function_signature("static_only", module_id(1), i32);
    static_method.is_method = true;
    static_method.method_owner_type = record_type;
    analyzer.checked_.functions.emplace(analyzer.method_key(module_id(1), record_type, "static_only"), static_method);
    FunctionSignature not_method = function_signature("free", module_id(1), i32);
    not_method.method_owner_type = record_type;
    analyzer.checked_.functions.emplace(analyzer.method_key(module_id(1), record_type, "free"), not_method);
    EXPECT_EQ(analyzer.find_method_in_visible_modules(record_type, "run", {}, true), nullptr);
    EXPECT_EQ(analyzer.find_method_in_visible_modules(record_type, "static_only", {}, true), nullptr);
    EXPECT_EQ(analyzer.find_method_in_visible_modules(record_type, "free", {}, false), nullptr);
    EXPECT_EQ(analyzer.find_method_in_visible_modules(record_type, "missing", {}, true), nullptr);
}

TEST(CoreUnit, SemanticWhiteBoxGenericHelperEdges) {
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"lib", "one"}),
    };
    module.modules[SEMA_TEST_ROOT_MODULE_INDEX].imports = {resolved_import(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "one")};

    const TypeId i32_type_id = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const TypeId bool_type_id = module.push_type(primitive_node(syntax::PrimitiveTypeKind::bool_));
    syntax::TypeNode generic_t = named_node("T");
    const TypeId t_type_id = module.push_type(generic_t);
    syntax::ItemNode missing_return;
    missing_return.kind = syntax::ItemKind::fn_decl;
    missing_return.name = "missing_return";
    missing_return.generic_params = {"T"};
    missing_return.params = {syntax::ParamDecl {"value", t_type_id}};
    const syntax::ItemId missing_return_item = module.push_item(missing_return);
    module.item_modules[missing_return_item.value] = module_id(1);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(module, diagnostics);
    analyzer.checked_.syntax_type_handles.assign(module.types.size(), invalid_type_handle);
    analyzer.current_module_ = module_id(0);
    const TypeHandle i32 = analyzer.checked_.types.builtin(BuiltinType::i32);
    const TypeHandle bool_type = analyzer.checked_.types.builtin(BuiltinType::bool_);

    GenericEnumTemplateInfo private_enum;
    private_enum.name = "Secret";
    private_enum.module = module_id(1);
    private_enum.params = {"T"};
    private_enum.visibility = syntax::Visibility::private_;
    analyzer.generic_enum_templates_.emplace(analyzer.module_key(module_id(1), "Secret"), private_enum);
    EXPECT_EQ(analyzer.find_generic_enum_template_in_module(module_id(1), "Missing", {}, true), nullptr);
    EXPECT_EQ(analyzer.find_generic_enum_template_in_module(module_id(1), "Secret", {}, true), nullptr);

    GenericStructTemplateInfo private_struct;
    private_struct.name = "Hidden";
    private_struct.module = module_id(1);
    private_struct.params = {"T"};
    private_struct.visibility = syntax::Visibility::private_;
    analyzer.generic_struct_templates_.emplace(analyzer.module_key(module_id(1), "Hidden"), private_struct);
    EXPECT_EQ(analyzer.find_generic_struct_template_in_module(module_id(1), "Missing", {}, true), nullptr);
    EXPECT_EQ(analyzer.find_generic_struct_template_in_module(module_id(1), "Hidden", {}, true), nullptr);

    GenericFunctionTemplateInfo private_function;
    private_function.name = "hidden";
    private_function.module = module_id(1);
    private_function.params = {"T"};
    private_function.visibility = syntax::Visibility::private_;
    analyzer.generic_function_templates_.emplace(analyzer.module_key(module_id(1), "hidden"), private_function);
    EXPECT_EQ(analyzer.find_generic_function_template_in_module(module_id(1), "Missing", {}, true), nullptr);
    EXPECT_EQ(analyzer.find_generic_function_template_in_module(module_id(1), "hidden", {}, true), nullptr);

    GenericEnumTemplateInfo enum_info;
    enum_info.name = "Option";
    enum_info.module = module_id(1);
    enum_info.params = {"T", "E"};
    enum_info.item = syntax::invalid_item_id;
    EXPECT_FALSE(is_valid(analyzer.instantiate_generic_enum_from_syntax(enum_info, {i32_type_id}, {}, false)));
    EXPECT_FALSE(is_valid(analyzer.instantiate_generic_enum(enum_info, {i32}, {})));
    enum_info.params = {"T"};
    EXPECT_FALSE(is_valid(analyzer.instantiate_generic_enum(enum_info, {i32}, {})));

    GenericStructTemplateInfo struct_info;
    struct_info.name = "Box";
    struct_info.module = module_id(1);
    struct_info.params = {"T", "E"};
    struct_info.item = syntax::invalid_item_id;
    EXPECT_FALSE(is_valid(analyzer.instantiate_generic_struct_from_syntax(struct_info, {i32_type_id}, {}, false)));
    EXPECT_FALSE(is_valid(analyzer.instantiate_generic_struct(struct_info, {i32}, {})));
    struct_info.params = {"T"};
    EXPECT_FALSE(is_valid(analyzer.instantiate_generic_struct(struct_info, {i32}, {})));

    std::vector<TypeHandle> inferred = {invalid_type_handle};
    EXPECT_FALSE(analyzer.infer_generic_args_from_type_pattern(syntax::invalid_type_id, i32, {"T"}, inferred, {}, "test", module_id(0)));
    EXPECT_FALSE(analyzer.infer_generic_args_from_type_pattern(i32_type_id, invalid_type_handle, {"T"}, inferred, {}, "test", module_id(0)));
    EXPECT_TRUE(analyzer.infer_generic_args_from_type_pattern(i32_type_id, i32, {"T"}, inferred, {}, "test", module_id(0)));
    EXPECT_FALSE(analyzer.infer_generic_args_from_type_pattern(bool_type_id, i32, {"T"}, inferred, {}, "test", module_id(0)));

    GenericFunctionTemplateInfo function_info;
    function_info.name = "identity";
    function_info.module = module_id(1);
    function_info.params = {"T", "E"};
    function_info.item = syntax::invalid_item_id;
    EXPECT_EQ(analyzer.instantiate_generic_function_from_syntax(function_info, {i32_type_id}, {}), nullptr);
    EXPECT_EQ(analyzer.instantiate_generic_function(function_info, {i32}, {}), nullptr);
    function_info.params = {"T"};
    EXPECT_EQ(analyzer.instantiate_generic_function(function_info, {invalid_type_handle}, {}), nullptr);
    EXPECT_EQ(analyzer.instantiate_generic_function(function_info, {i32}, {}), nullptr);

    function_info.item = missing_return_item;
    EXPECT_EQ(analyzer.instantiate_generic_function(function_info, {i32}, {}), nullptr);
    module.items[missing_return_item.value].return_type = t_type_id;
    std::vector<TypeHandle> function_inferred = {invalid_type_handle};
    EXPECT_FALSE(analyzer.infer_generic_function_args(function_info, {bool_type}, i32, function_inferred, {}));
}

TEST(CoreUnit, SemanticWhiteBoxBodyInferenceAndGenericPatternEdges) {
    syntax::AstModule module;
    module.modules = {module_info({"root"})};
    const TypeId void_type_id = module.push_type(primitive_node(syntax::PrimitiveTypeKind::void_));
    const TypeId i32_type_id = module.push_type(primitive_node(syntax::PrimitiveTypeKind::i32));
    const TypeId bool_type_id = module.push_type(primitive_node(syntax::PrimitiveTypeKind::bool_));
    const TypeId t_type_id = module.push_type(named_node("T"));
    syntax::TypeNode pointer_t;
    pointer_t.kind = syntax::TypeKind::pointer;
    pointer_t.pointer_mutability = syntax::PointerMutability::mut;
    pointer_t.pointee = t_type_id;
    const TypeId pointer_t_id = module.push_type(pointer_t);
    syntax::TypeNode array_t;
    array_t.kind = syntax::TypeKind::array;
    array_t.array_count = 2;
    array_t.array_element = t_type_id;
    const TypeId array_t_id = module.push_type(array_t);
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

    syntax::ExprNode invalid_expr;
    invalid_expr.kind = syntax::ExprKind::invalid;
    const ExprId invalid_expr_id = module.push_expr(invalid_expr);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(module, diagnostics);
    analyzer.checked_.syntax_type_handles.assign(module.types.size(), invalid_type_handle);
    analyzer.checked_.expr_types.assign(module.exprs.size(), invalid_type_handle);
    analyzer.checked_.stmt_local_types.assign(module.stmts.size(), invalid_type_handle);
    analyzer.current_module_ = module_id(0);
    const TypeHandle i32 = analyzer.checked_.types.builtin(BuiltinType::i32);
    const TypeHandle bool_type = analyzer.checked_.types.builtin(BuiltinType::bool_);
    const TypeHandle const_ptr_i32 = analyzer.checked_.types.pointer(PointerMutability::const_, i32);
    const TypeHandle array_i32_3 = analyzer.checked_.types.array(3, i32);
    const TypeHandle plain_type = analyzer.checked_.types.named_struct("Plain", "Plain", false);
    analyzer.named_types_.emplace(analyzer.module_key(module_id(0), "Plain"), plain_type);

    FunctionSignature conflict_signature = function_signature("infer", module_id(0), invalid_type_handle);
    sema::SemanticAnalyzer::FunctionBodyState state = sema::SemanticAnalyzer::FunctionBodyState::analyzing;
    analyzer.analyze_function_body_with_signature(function, "0:infer", conflict_signature, state, nullptr);
    state = sema::SemanticAnalyzer::FunctionBodyState::analyzed;
    analyzer.analyze_function_body_with_signature(function, "0:infer", conflict_signature, state, nullptr);
    state = sema::SemanticAnalyzer::FunctionBodyState::not_started;
    conflict_signature.has_conflict = true;
    analyzer.analyze_function_body_with_signature(function, "0:infer", conflict_signature, state, nullptr);

    analyzer.analyze_block(syntax::invalid_stmt_id, i32, nullptr);
    analyzer.analyze_stmt(syntax::invalid_stmt_id, i32, nullptr);
    sema::SemanticAnalyzer::ReturnTypeInference inference;
    analyzer.finalize_inferred_return(function, "0:infer", inference);
    conflict_signature.has_conflict = false;
    analyzer.ensure_function_return_known(conflict_signature, {});
    EXPECT_FALSE(is_valid(analyzer.analyze_expr(syntax::invalid_expr_id)));
    EXPECT_FALSE(is_valid(analyzer.analyze_expr(invalid_expr_id)));
    EXPECT_FALSE(is_valid(analyzer.resolve_type(syntax::invalid_type_id)));

    analyzer.checked_.syntax_type_handles[plain_type_id.value] = plain_type;
    EXPECT_TRUE(analyzer.checked_.types.same(analyzer.resolve_type(plain_type_id), plain_type));
    const TypeHandle opaque = analyzer.checked_.types.opaque_struct("Opaque", "Opaque");
    analyzer.checked_.syntax_type_handles[plain_type_id.value] = opaque;
    EXPECT_TRUE(analyzer.checked_.types.same(analyzer.resolve_type(plain_type_id), opaque));
    analyzer.checked_.syntax_type_handles[plain_type_id.value] = invalid_type_handle;

    std::vector<TypeHandle> inferred = {invalid_type_handle};
    EXPECT_FALSE(analyzer.infer_generic_args_from_type_pattern(pointer_t_id, i32, {"T"}, inferred, {}, "pattern", module_id(0)));
    EXPECT_FALSE(analyzer.infer_generic_args_from_type_pattern(pointer_t_id, const_ptr_i32, {"T"}, inferred, {}, "pattern", module_id(0)));
    EXPECT_FALSE(analyzer.infer_generic_args_from_type_pattern(array_t_id, i32, {"T"}, inferred, {}, "pattern", module_id(0)));
    EXPECT_FALSE(analyzer.infer_generic_args_from_type_pattern(array_t_id, array_i32_3, {"T"}, inferred, {}, "pattern", module_id(0)));
    EXPECT_TRUE(analyzer.infer_generic_args_from_type_pattern(plain_type_id, plain_type, {"T"}, inferred, {}, "pattern", module_id(0)));

    GenericStructTemplateInfo struct_info;
    struct_info.name = "BrokenStruct";
    struct_info.module = module_id(0);
    struct_info.params = {"T"};
    struct_info.item = syntax::invalid_item_id;
    syntax::ExprNode literal;
    literal.kind = syntax::ExprKind::struct_literal;
    EXPECT_FALSE(is_valid(analyzer.infer_generic_struct_literal_type(struct_info, literal, invalid_type_handle)));

    syntax::ItemNode void_param_function;
    void_param_function.kind = syntax::ItemKind::fn_decl;
    void_param_function.name = "void_param";
    void_param_function.generic_params = {"T"};
    void_param_function.params = {syntax::ParamDecl {"value", void_type_id}};
    void_param_function.return_type = i32_type_id;
    const syntax::ItemId void_param_item = module.push_item(void_param_function);
    module.item_modules[void_param_item.value] = module_id(0);
    GenericFunctionTemplateInfo void_param_info;
    void_param_info.name = "void_param";
    void_param_info.module = module_id(0);
    void_param_info.params = {"T"};
    void_param_info.item = void_param_item;
    EXPECT_NE(analyzer.instantiate_generic_function(void_param_info, {i32}, {}), nullptr);

    syntax::ExprNode name_expr;
    name_expr.kind = syntax::ExprKind::name;
    name_expr.text = "Option";
    const ExprId option_name = module.push_expr(name_expr);
    syntax::ExprNode field_expr;
    field_expr.kind = syntax::ExprKind::field;
    field_expr.object = option_name;
    field_expr.field_name = "missing";
    const ExprId missing_case = module.push_expr(field_expr);
    GenericEnumTemplateInfo enum_info;
    enum_info.name = "Option";
    enum_info.module = module_id(0);
    enum_info.params = {"T"};
    enum_info.item = syntax::invalid_item_id;
    analyzer.generic_enum_templates_.emplace(analyzer.module_key(module_id(0), "Option"), enum_info);
    EXPECT_EQ(analyzer.instantiate_generic_enum_constructor(missing_case, {i32}, invalid_type_handle, true), nullptr);
    EXPECT_EQ(analyzer.instantiate_generic_enum_constructor(syntax::invalid_expr_id, {i32}, invalid_type_handle, true), nullptr);

    static_cast<void>(bool_type_id);
    static_cast<void>(bool_type);
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
    syntax::ExprNode str_data;
    str_data.kind = syntax::ExprKind::str_data;
    str_data.cast_expr = str_value;
    const ExprId str_data_id = module.push_expr(str_data);
    syntax::ExprNode str_byte_len = str_data;
    str_byte_len.kind = syntax::ExprKind::str_byte_len;
    const ExprId str_byte_len_id = module.push_expr(str_byte_len);
    syntax::ExprNode str_from_bytes;
    str_from_bytes.kind = syntax::ExprKind::str_from_bytes_unchecked;
    str_from_bytes.args = {data_value, length_value};
    const ExprId str_from_bytes_id = module.push_expr(str_from_bytes);
    syntax::ExprNode malformed = str_from_bytes;
    malformed.args = {data_value};
    const ExprId malformed_id = module.push_expr(malformed);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(module, diagnostics);
    analyzer.checked_.expr_types.assign(module.exprs.size(), invalid_type_handle);
    analyzer.checked_.expr_c_names.assign(module.exprs.size(), {});
    analyzer.checked_.syntax_type_handles.assign(module.types.size(), invalid_type_handle);
    analyzer.current_module_ = module_id(0);

    sema::TypeTable& types = analyzer.checked_.types;
    const TypeHandle str = types.builtin(BuiltinType::str);
    const TypeHandle u8 = types.builtin(BuiltinType::u8);
    const TypeHandle usize = types.builtin(BuiltinType::usize);
    const TypeHandle const_u8_ptr = types.pointer(PointerMutability::const_, u8);
    analyzer.checked_.syntax_type_handles[u8_type_id.value] = u8;
    analyzer.checked_.syntax_type_handles[const_u8_ptr_type_id.value] = const_u8_ptr;
    analyzer.global_values_.emplace(analyzer.module_key(module_id(0), "text"), symbol(SymbolKind::local, "text", module_id(0), str));
    analyzer.global_values_.emplace(analyzer.module_key(module_id(0), "data"), symbol(SymbolKind::local, "data", module_id(0), const_u8_ptr));
    analyzer.global_values_.emplace(analyzer.module_key(module_id(0), "len"), symbol(SymbolKind::local, "len", module_id(0), usize));

    EXPECT_TRUE(types.same(analyzer.analyze_str_projection_expr(str_data_id, module.exprs[str_data_id.value]), const_u8_ptr));
    EXPECT_TRUE(types.same(analyzer.analyze_str_projection_expr(str_byte_len_id, module.exprs[str_byte_len_id.value]), usize));
    EXPECT_TRUE(types.same(analyzer.analyze_str_from_bytes_unchecked_expr(str_from_bytes_id, module.exprs[str_from_bytes_id.value]), str));
    EXPECT_TRUE(types.same(analyzer.analyze_str_from_bytes_unchecked_expr(malformed_id, module.exprs[malformed_id.value]), str));

    analyzer.global_values_[analyzer.module_key(module_id(0), "text")].type = usize;
    analyzer.global_values_[analyzer.module_key(module_id(0), "data")].type = usize;
    analyzer.global_values_[analyzer.module_key(module_id(0), "len")].type = str;
    static_cast<void>(analyzer.analyze_str_projection_expr(str_data_id, module.exprs[str_data_id.value]));
    static_cast<void>(analyzer.analyze_str_projection_expr(str_byte_len_id, module.exprs[str_byte_len_id.value]));
    static_cast<void>(analyzer.analyze_str_from_bytes_unchecked_expr(str_from_bytes_id, module.exprs[str_from_bytes_id.value]));

    EXPECT_GT(diagnostics.diagnostics().size(), 0U);
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
    missing_else_if_root.else_if = syntax::invalid_stmt_id;
    const syntax::StmtId missing_else_if_stmt = module.push_stmt(missing_else_if_root);

    const syntax::StmtId fallthrough_block = push_block(module, {expr_stmt, partial_if_stmt});
    const syntax::StmtId non_fallthrough_block = push_block(module, {expr_stmt, full_if_stmt, expr_stmt});
    const syntax::StmtId abrupt_block = push_block(module, {continue_stmt, expr_stmt});

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(module, diagnostics);

    EXPECT_FALSE(analyzer.block_guarantees_return(syntax::invalid_stmt_id));
    EXPECT_FALSE(analyzer.stmt_guarantees_return(syntax::invalid_stmt_id));
    EXPECT_TRUE(analyzer.block_may_fallthrough(syntax::invalid_stmt_id));
    EXPECT_TRUE(analyzer.stmt_may_fallthrough(syntax::invalid_stmt_id));

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

TEST(CoreUnit, SemanticWhiteBoxIterativeTypeLayoutAndGenericInference) {
    syntax::AstModule module;
    module.modules = {
        module_info({"root"}),
        module_info({"lib", "one"}),
    };
    module.modules[SEMA_TEST_ROOT_MODULE_INDEX].imports = {resolved_import(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "one")};

    const TypeId t_type_id = module.push_type(named_node("T"));
    const TypeId e_type_id = module.push_type(named_node("E"));

    syntax::TypeNode pointer_t;
    pointer_t.kind = syntax::TypeKind::pointer;
    pointer_t.pointer_mutability = syntax::PointerMutability::mut;
    pointer_t.pointee = t_type_id;
    const TypeId pointer_t_id = module.push_type(pointer_t);

    syntax::TypeNode array_pointer_t;
    array_pointer_t.kind = syntax::TypeKind::array;
    array_pointer_t.array_count = SEMA_TEST_SMALL_ARRAY_COUNT;
    array_pointer_t.array_element = pointer_t_id;
    const TypeId array_pointer_t_id = module.push_type(array_pointer_t);

    syntax::TypeNode pair_pattern = named_node("Pair");
    pair_pattern.type_args = {array_pointer_t_id, e_type_id};
    const TypeId pair_pattern_id = module.push_type(pair_pattern);

    syntax::TypeNode qualified_pair_pattern = pair_pattern;
    qualified_pair_pattern.scope_name = "one";
    const TypeId qualified_pair_pattern_id = module.push_type(qualified_pair_pattern);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(module, diagnostics);
    analyzer.checked_.syntax_type_handles.assign(module.types.size(), invalid_type_handle);
    analyzer.current_module_ = module_id(0);

    sema::TypeTable& types = analyzer.checked_.types;
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle u8 = types.builtin(BuiltinType::u8);
    const TypeHandle u64 = types.builtin(BuiltinType::u64);
    const TypeHandle bool_type = types.builtin(BuiltinType::bool_);
    const TypeHandle mut_ptr_i32 = types.pointer(PointerMutability::mut, i32);
    const TypeHandle array_mut_ptr_i32 = types.array(SEMA_TEST_SMALL_ARRAY_COUNT, mut_ptr_i32);
    const TypeHandle pair_instance = types.named_struct("Pair<i32,u64>", "Pair_i32_u64", false);
    const TypeHandle scoped_pair_instance = types.named_struct("one.Pair<i32,u64>", "one_Pair_i32_u64", false);
    const TypeHandle option_instance = types.named_enum("Option<i32>", "Option_i32");

    GenericStructTemplateInfo pair_info;
    pair_info.name = "Pair";
    pair_info.module = module_id(0);
    pair_info.params = {"T", "E"};
    analyzer.generic_struct_templates_.emplace(analyzer.module_key(module_id(0), "Pair"), pair_info);

    GenericStructTemplateInfo scoped_pair_info = pair_info;
    scoped_pair_info.module = module_id(1);
    analyzer.generic_struct_templates_.emplace(analyzer.module_key(module_id(1), "Pair"), scoped_pair_info);

    sema::GenericStructInstanceInfo pair_instance_info;
    pair_instance_info.name = "Pair";
    pair_instance_info.module = module_id(0);
    pair_instance_info.args = {array_mut_ptr_i32, u64};
    analyzer.generic_struct_instances_.emplace("Pair:i32:u64", pair_instance);
    analyzer.generic_struct_instance_infos_.emplace(pair_instance.value, pair_instance_info);

    sema::GenericStructInstanceInfo scoped_pair_instance_info = pair_instance_info;
    scoped_pair_instance_info.module = module_id(1);
    analyzer.generic_struct_instances_.emplace("one.Pair:i32:u64", scoped_pair_instance);
    analyzer.generic_struct_instance_infos_.emplace(scoped_pair_instance.value, scoped_pair_instance_info);

    GenericEnumTemplateInfo option_info;
    option_info.name = "Option";
    option_info.module = module_id(0);
    option_info.params = {"T"};
    analyzer.generic_enum_templates_.emplace(analyzer.module_key(module_id(0), "Option"), option_info);

    sema::GenericEnumInstanceInfo option_instance_info;
    option_instance_info.name = "Option";
    option_instance_info.module = module_id(0);
    option_instance_info.args = {i32};
    analyzer.generic_enum_instances_.emplace("Option:i32", option_instance);
    analyzer.generic_enum_instance_infos_.emplace(option_instance.value, option_instance_info);

    std::vector<TypeHandle> inferred = {invalid_type_handle, invalid_type_handle};
    EXPECT_TRUE(analyzer.infer_generic_args_from_type_pattern(pair_pattern_id, pair_instance, {"T", "E"}, inferred, {}, "pattern", module_id(0)));
    EXPECT_TRUE(types.same(inferred[0], i32));
    EXPECT_TRUE(types.same(inferred[1], u64));

    std::vector<TypeHandle> scoped_inferred = {invalid_type_handle, invalid_type_handle};
    EXPECT_TRUE(analyzer.infer_generic_args_from_type_pattern(
        qualified_pair_pattern_id,
        scoped_pair_instance,
        {"T", "E"},
        scoped_inferred,
        {},
        "pattern",
        module_id(0)
    ));
    EXPECT_TRUE(types.same(scoped_inferred[0], i32));
    EXPECT_TRUE(types.same(scoped_inferred[1], u64));

    std::vector<TypeHandle> too_small_inferred;
    EXPECT_FALSE(analyzer.infer_generic_args_from_type_pattern(t_type_id, i32, {"T"}, too_small_inferred, {}, "pattern", module_id(0)));

    std::vector<TypeHandle> conflicting_inferred = {bool_type};
    EXPECT_FALSE(analyzer.infer_generic_args_from_type_pattern(t_type_id, i32, {"T"}, conflicting_inferred, {}, "pattern", module_id(0)));

    syntax::TypeNode option_pattern = named_node("Option");
    option_pattern.type_args = {t_type_id};
    const TypeId option_pattern_id = module.push_type(option_pattern);
    analyzer.checked_.syntax_type_handles.resize(module.types.size(), invalid_type_handle);
    std::vector<TypeHandle> enum_inferred = {invalid_type_handle};
    EXPECT_TRUE(analyzer.infer_generic_args_from_type_pattern(option_pattern_id, option_instance, {"T"}, enum_inferred, {}, "pattern", module_id(0)));
    EXPECT_TRUE(types.same(enum_inferred[0], i32));

    const TypeHandle max_array_u8 = types.array(SEMA_TEST_LAYOUT_MAX_ARRAY_COUNT, u8);
    EXPECT_EQ(analyzer.abi_size(max_array_u8), SEMA_TEST_LAYOUT_MAX_ARRAY_COUNT);
    EXPECT_EQ(analyzer.abi_align(max_array_u8), alignof(std::uint8_t));

    const TypeHandle overflow_array_u64 = types.array(SEMA_TEST_LAYOUT_MAX_ARRAY_COUNT, u64);
    EXPECT_EQ(analyzer.abi_size(overflow_array_u64), SEMA_TEST_LAYOUT_MAX_ARRAY_COUNT);
    EXPECT_EQ(analyzer.abi_align(overflow_array_u64), alignof(std::uint64_t));

    const TypeHandle overflow_struct_type = types.named_struct("OverflowStruct", "OverflowStruct", true);
    StructInfo overflow_struct;
    overflow_struct.name = "OverflowStruct";
    overflow_struct.module = module_id(0);
    overflow_struct.type = overflow_struct_type;
    overflow_struct.fields = {
        StructFieldInfo {"huge", "huge", module_id(0), max_array_u8},
        StructFieldInfo {"tail", "tail", module_id(0), u64},
    };
    analyzer.checked_.structs.emplace(analyzer.module_key(module_id(0), "OverflowStruct"), overflow_struct);

    const TypeHandle overflow_enum_type = types.named_enum("OverflowEnum", "OverflowEnum");
    types.set_enum_underlying(overflow_enum_type, max_array_u8);
    EnumCaseInfo overflow_case;
    overflow_case.name = "payload";
    overflow_case.case_name = "payload";
    overflow_case.module = module_id(0);
    overflow_case.type = overflow_enum_type;
    overflow_case.payload_type = u64;
    analyzer.checked_.enum_cases.emplace(analyzer.module_key(module_id(0), "payload"), overflow_case);
    analyzer.named_types_.emplace(analyzer.module_key(module_id(0), "OverflowEnum"), overflow_enum_type);

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
    syntax::ExprNode float_overflow_expr;
    float_overflow_expr.kind = syntax::ExprKind::float_literal;
    float_overflow_expr.text = SEMA_TEST_FLOAT_OVERFLOW_LITERAL;
    const ExprId float_overflow_expr_id = module.push_expr(float_overflow_expr);
    syntax::ExprNode separated_float_expr = float_overflow_expr;
    separated_float_expr.text = SEMA_TEST_FLOAT_WITH_SEPARATOR_LITERAL;
    const ExprId separated_float_expr_id = module.push_expr(separated_float_expr);
    syntax::ExprNode invalid_float_expr = float_overflow_expr;
    invalid_float_expr.text = SEMA_TEST_FLOAT_INVALID_TRAILING_LITERAL;
    const ExprId invalid_float_expr_id = module.push_expr(invalid_float_expr);

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
    syntax::TypeNode scoped_bad_box_type = named_node("BadBox");
    scoped_bad_box_type.scope_name = "one";
    scoped_bad_box_type.type_args = {i32_type_id};
    const TypeId scoped_bad_box_type_id = module.push_type(scoped_bad_box_type);
    syntax::TypeNode scoped_bad_enum_type = named_node("BadEnum");
    scoped_bad_enum_type.scope_name = "one";
    scoped_bad_enum_type.type_args = {i32_type_id};
    const TypeId scoped_bad_enum_type_id = module.push_type(scoped_bad_enum_type);
    syntax::TypeNode unqualified_bad_box_type = named_node("LocalBadBox");
    unqualified_bad_box_type.type_args = {i32_type_id};
    const TypeId unqualified_bad_box_type_id = module.push_type(unqualified_bad_box_type);
    syntax::TypeNode unqualified_bad_enum_type = named_node("LocalBadEnum");
    unqualified_bad_enum_type.type_args = {i32_type_id};
    const TypeId unqualified_bad_enum_type_id = module.push_type(unqualified_bad_enum_type);

    syntax::ItemNode enum_item;
    enum_item.kind = syntax::ItemKind::enum_decl;
    enum_item.name = "Choice";
    enum_item.generic_params = {"T"};
    enum_item.enum_base_type = u8_type_id;
    enum_item.enum_cases = {
        syntax::EnumCaseDecl {"none", syntax::invalid_type_id, SEMA_TEST_INTEGER_LITERAL_ONE, {}},
    };
    const syntax::ItemId enum_item_id = module.push_item(enum_item);
    module.item_modules[enum_item_id.value] = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(module, diagnostics);
    analyzer.checked_.expr_types.assign(module.exprs.size(), invalid_type_handle);
    analyzer.checked_.expr_c_names.assign(module.exprs.size(), {});
    analyzer.checked_.pattern_c_names.assign(SEMA_TEST_PATTERN_TRACKED_COUNT, {});
    analyzer.checked_.pattern_case_sets.assign(SEMA_TEST_PATTERN_TRACKED_COUNT, {});
    analyzer.checked_.syntax_type_handles.assign(module.types.size(), invalid_type_handle);
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
    EXPECT_FALSE(analyzer.can_assign(bool_type, u8, hex_expr));
    EXPECT_TRUE(types.same(
        analyzer.analyze_float_literal(float_overflow_expr_id, module.exprs[float_overflow_expr_id.value], types.builtin(BuiltinType::f32)),
        types.builtin(BuiltinType::f32)
    ));
    EXPECT_TRUE(types.same(
        analyzer.analyze_float_literal(separated_float_expr_id, module.exprs[separated_float_expr_id.value], invalid_type_handle),
        types.builtin(BuiltinType::f64)
    ));
    EXPECT_TRUE(types.same(
        analyzer.analyze_float_literal(invalid_float_expr_id, module.exprs[invalid_float_expr_id.value], types.builtin(BuiltinType::f64)),
        types.builtin(BuiltinType::f64)
    ));
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

    analyzer.record_pattern_c_name(syntax::invalid_pattern_id, "ignored");
    analyzer.record_pattern_c_name(syntax::PatternId {SEMA_TEST_PATTERN_FIRST_INDEX}, {});
    analyzer.record_pattern_case_name(syntax::invalid_pattern_id, "ignored");
    analyzer.record_pattern_case_name(syntax::PatternId {SEMA_TEST_PATTERN_FIRST_INDEX}, {});
    analyzer.merge_pattern_case_names(syntax::invalid_pattern_id, syntax::PatternId {SEMA_TEST_PATTERN_FIRST_INDEX});

    std::unordered_map<base::u32, std::unordered_set<std::string>> generic_case_sets;
    generic_case_sets[SEMA_TEST_PATTERN_SECOND_INDEX].insert("from_generic");
    analyzer.current_generic_pattern_case_sets_ = &generic_case_sets;
    analyzer.merge_pattern_case_names(
        syntax::PatternId {SEMA_TEST_PATTERN_FIRST_INDEX},
        syntax::PatternId {SEMA_TEST_PATTERN_SECOND_INDEX}
    );
    EXPECT_TRUE(generic_case_sets[SEMA_TEST_PATTERN_FIRST_INDEX].contains("from_generic"));
    analyzer.current_generic_pattern_case_sets_->erase(SEMA_TEST_PATTERN_SECOND_INDEX);
    analyzer.checked_.pattern_case_sets[SEMA_TEST_PATTERN_SECOND_INDEX].insert("from_checked");
    analyzer.merge_pattern_case_names(
        syntax::PatternId {SEMA_TEST_PATTERN_THIRD_INDEX},
        syntax::PatternId {SEMA_TEST_PATTERN_SECOND_INDEX}
    );
    EXPECT_TRUE(generic_case_sets[SEMA_TEST_PATTERN_THIRD_INDEX].contains("from_checked"));
    analyzer.current_generic_pattern_case_sets_ = nullptr;

    syntax::ExprNode unqualified_missing_generic;
    unqualified_missing_generic.kind = syntax::ExprKind::name;
    unqualified_missing_generic.text = "Missing";
    unqualified_missing_generic.type_args = {i32_type_id};
    EXPECT_FALSE(is_valid(analyzer.resolve_associated_type_owner(unqualified_missing_generic, false)));

    syntax::ExprNode missing_alias_generic = unqualified_missing_generic;
    missing_alias_generic.scope_name = "missing";
    EXPECT_FALSE(is_valid(analyzer.resolve_associated_type_owner(missing_alias_generic, false)));

    GenericEnumTemplateInfo enum_info;
    enum_info.name = "Choice";
    enum_info.module = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    enum_info.params = {"T"};
    enum_info.item = enum_item_id;
    analyzer.generic_enum_templates_.emplace(analyzer.module_key(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "Choice"), enum_info);
    GenericStructTemplateInfo bad_box_info;
    bad_box_info.name = "BadBox";
    bad_box_info.module = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    bad_box_info.params = {"T", "E"};
    analyzer.generic_struct_templates_.emplace(analyzer.module_key(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "BadBox"), bad_box_info);
    GenericEnumTemplateInfo bad_enum_info;
    bad_enum_info.name = "BadEnum";
    bad_enum_info.module = module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX);
    bad_enum_info.params = {"T", "E"};
    analyzer.generic_enum_templates_.emplace(analyzer.module_key(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "BadEnum"), bad_enum_info);
    GenericStructTemplateInfo local_bad_box_info = bad_box_info;
    local_bad_box_info.name = "LocalBadBox";
    local_bad_box_info.module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);
    analyzer.generic_struct_templates_.emplace(analyzer.module_key(module_id(SEMA_TEST_ROOT_MODULE_INDEX), "LocalBadBox"), local_bad_box_info);
    GenericEnumTemplateInfo local_bad_enum_info = bad_enum_info;
    local_bad_enum_info.name = "LocalBadEnum";
    local_bad_enum_info.module = module_id(SEMA_TEST_ROOT_MODULE_INDEX);
    analyzer.generic_enum_templates_.emplace(analyzer.module_key(module_id(SEMA_TEST_ROOT_MODULE_INDEX), "LocalBadEnum"), local_bad_enum_info);
    analyzer.named_types_.emplace(analyzer.module_key(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), "ScopedOpaque"), scoped_opaque);

    syntax::ExprNode scoped_enum = unqualified_missing_generic;
    scoped_enum.text = "Choice";
    scoped_enum.scope_name = "one";
    const TypeHandle choice_i32 = analyzer.resolve_associated_type_owner(scoped_enum, false);
    EXPECT_TRUE(is_valid(choice_i32));

    syntax::ExprNode scoped_missing_generic = unqualified_missing_generic;
    scoped_missing_generic.text = "MissingScoped";
    scoped_missing_generic.scope_name = "one";
    EXPECT_FALSE(is_valid(analyzer.resolve_associated_type_owner(scoped_missing_generic, false)));

    sema::GenericTypeSubstitution substitution;
    std::unordered_map<base::u32, TypeHandle> generic_type_cache;
    const TypeHandle opaque = types.opaque_struct("Opaque", "Opaque");
    generic_type_cache[i32_type_id.value] = opaque;
    analyzer.current_generic_syntax_type_handles_ = &generic_type_cache;
    EXPECT_TRUE(analyzer.checked_.types.same(analyzer.resolve_type_with_substitution(i32_type_id, &substitution, false), opaque));
    analyzer.current_generic_syntax_type_handles_ = nullptr;
    EXPECT_TRUE(types.is_void(analyzer.resolve_type(invalid_primitive_type_id)));
    EXPECT_TRUE(types.same(analyzer.resolve_type(scoped_opaque_type_id), scoped_opaque));
    EXPECT_FALSE(is_valid(analyzer.resolve_type(scoped_missing_alias_type_id)));
    EXPECT_FALSE(is_valid(analyzer.resolve_type(scoped_choice_missing_args_type_id)));
    EXPECT_FALSE(is_valid(analyzer.resolve_type_with_substitution(scoped_bad_box_type_id, &substitution, false)));
    EXPECT_FALSE(is_valid(analyzer.resolve_type_with_substitution(scoped_bad_enum_type_id, &substitution, false)));
    EXPECT_FALSE(is_valid(analyzer.resolve_type_with_substitution(unqualified_bad_box_type_id, &substitution, false)));
    EXPECT_FALSE(is_valid(analyzer.resolve_type_with_substitution(unqualified_bad_enum_type_id, &substitution, false)));

    const TypeHandle record_type = types.named_struct("Record", "Record", false);
    const TypeHandle array_record = types.named_struct("ArrayRecord", "ArrayRecord", true);
    types.set_record_properties(array_record, true, false);
    analyzer.named_types_.emplace(analyzer.module_key(module_id(0), "Record"), record_type);
    analyzer.global_values_.emplace(
        analyzer.module_key(module_id(0), "array_value"),
        symbol(SymbolKind::local, "array_value", module_id(0), array_record, false)
    );

    FunctionSignature static_method = function_signature("needs", module_id(0), bool_type);
    static_method.is_method = true;
    static_method.method_owner_type = record_type;
    analyzer.checked_.functions.emplace(analyzer.method_key(module_id(0), record_type, "needs"), static_method);
    FunctionSignature needs_arg = function_signature("needs_arg", module_id(0), bool_type);
    needs_arg.is_method = true;
    needs_arg.method_owner_type = record_type;
    needs_arg.param_types = {u8};
    analyzer.checked_.functions.emplace(analyzer.method_key(module_id(0), record_type, "needs_arg"), needs_arg);
    FunctionSignature takes_array = function_signature("takes_array", module_id(0), bool_type);
    takes_array.is_method = true;
    takes_array.method_owner_type = record_type;
    takes_array.param_types = {array_record};
    analyzer.checked_.functions.emplace(analyzer.method_key(module_id(0), record_type, "takes_array"), takes_array);

    const ExprId record_name = push_name(module, "Record");
    syntax::ExprNode type_arg_field;
    type_arg_field.kind = syntax::ExprKind::field;
    type_arg_field.object = record_name;
    type_arg_field.field_name = "needs";
    type_arg_field.type_args = {i32_type_id};
    const ExprId type_arg_field_id = module.push_expr(type_arg_field);
    syntax::ExprNode type_arg_call;
    type_arg_call.kind = syntax::ExprKind::call;
    type_arg_call.callee = type_arg_field_id;
    const ExprId type_arg_call_id = module.push_expr(type_arg_call);

    syntax::ExprNode missing_arg_field = type_arg_field;
    missing_arg_field.field_name = "needs_arg";
    missing_arg_field.type_args = {};
    const ExprId missing_arg_field_id = module.push_expr(missing_arg_field);
    syntax::ExprNode missing_arg_call = type_arg_call;
    missing_arg_call.callee = missing_arg_field_id;
    const ExprId missing_arg_call_id = module.push_expr(missing_arg_call);

    const ExprId array_value = push_name(module, "array_value");
    syntax::ExprNode array_arg_field = missing_arg_field;
    array_arg_field.field_name = "takes_array";
    const ExprId array_arg_field_id = module.push_expr(array_arg_field);
    syntax::ExprNode array_arg_call = type_arg_call;
    array_arg_call.callee = array_arg_field_id;
    array_arg_call.args = {array_value};
    const ExprId array_arg_call_id = module.push_expr(array_arg_call);

    const ExprId choice_name = push_name(module, "Choice");
    syntax::ExprNode none_field;
    none_field.kind = syntax::ExprKind::field;
    none_field.object = choice_name;
    none_field.field_name = "none";
    const ExprId none_field_id = module.push_expr(none_field);
    syntax::ExprNode none_call;
    none_call.kind = syntax::ExprKind::call;
    none_call.callee = none_field_id;
    const ExprId none_call_id = module.push_expr(none_call);

    analyzer.checked_.expr_types.resize(module.exprs.size(), invalid_type_handle);
    analyzer.checked_.expr_c_names.resize(module.exprs.size());
    static_cast<void>(analyzer.analyze_call_expr(type_arg_call_id, module.exprs[type_arg_call_id.value], invalid_type_handle));
    static_cast<void>(analyzer.analyze_call_expr(missing_arg_call_id, module.exprs[missing_arg_call_id.value], invalid_type_handle));
    static_cast<void>(analyzer.analyze_call_expr(array_arg_call_id, module.exprs[array_arg_call_id.value], invalid_type_handle));
    static_cast<void>(analyzer.analyze_call_expr(none_call_id, module.exprs[none_call_id.value], choice_i32));
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
    payload_pattern.binding_name = "payload";
    const syntax::PatternId payload_pattern_id = module.push_pattern(payload_pattern);

    syntax::PatternNode true_binding_pattern;
    true_binding_pattern.kind = syntax::PatternKind::literal;
    true_binding_pattern.case_name = "true";
    true_binding_pattern.binding_name = "flag";
    const syntax::PatternId true_binding_pattern_id = module.push_pattern(true_binding_pattern);

    syntax::PatternNode wildcard_pattern;
    wildcard_pattern.kind = syntax::PatternKind::wildcard;
    const syntax::PatternId wildcard_pattern_id = module.push_pattern(wildcard_pattern);

    syntax::PatternNode missing_scoped_pattern;
    missing_scoped_pattern.kind = syntax::PatternKind::enum_case;
    missing_scoped_pattern.scoped = true;
    missing_scoped_pattern.case_name = "missing";
    const syntax::PatternId missing_scoped_pattern_id = module.push_pattern(missing_scoped_pattern);

    syntax::PatternNode unsupported_literal_pattern;
    unsupported_literal_pattern.kind = syntax::PatternKind::literal;
    unsupported_literal_pattern.case_name = "1";
    const syntax::PatternId unsupported_literal_pattern_id = module.push_pattern(unsupported_literal_pattern);

    syntax::ExprNode enum_match;
    enum_match.kind = syntax::ExprKind::match_expr;
    enum_match.match_value = choice_value;
    enum_match.match_arms = {
        syntax::MatchArm {payload_pattern_id, int_guard, bool_result, {}},
    };
    const ExprId enum_match_id = module.push_expr(enum_match);

    syntax::ExprNode binding_value_match;
    binding_value_match.kind = syntax::ExprKind::match_expr;
    binding_value_match.match_value = bool_subject;
    binding_value_match.match_arms = {
        syntax::MatchArm {true_binding_pattern_id, syntax::invalid_expr_id, int_result, {}},
        syntax::MatchArm {wildcard_pattern_id, syntax::invalid_expr_id, int_result, {}},
    };
    const ExprId binding_value_match_id = module.push_expr(binding_value_match);

    syntax::ExprNode void_match;
    void_match.kind = syntax::ExprKind::match_expr;
    void_match.match_value = bool_subject;
    void_match.match_arms = {
        syntax::MatchArm {wildcard_pattern_id, syntax::invalid_expr_id, void_value, {}},
    };
    const ExprId void_match_id = module.push_expr(void_match);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(module, diagnostics);
    analyzer.checked_.expr_types.assign(module.exprs.size(), invalid_type_handle);
    analyzer.checked_.expr_c_names.assign(module.exprs.size(), {});
    analyzer.checked_.pattern_c_names.assign(module.patterns.size(), {});
    analyzer.checked_.pattern_case_sets.assign(module.patterns.size(), {});
    analyzer.current_module_ = module_id(0);

    sema::TypeTable& types = analyzer.checked_.types;
    const TypeHandle void_type = types.builtin(BuiltinType::void_);
    const TypeHandle u8 = types.builtin(BuiltinType::u8);
    const TypeHandle i32 = types.builtin(BuiltinType::i32);
    const TypeHandle choice_type = types.named_enum("Choice", "Choice");
    const TypeHandle record_type = types.named_struct("Record", "Record", false);
    types.set_enum_underlying(choice_type, u8);

    EnumCaseInfo some_case;
    some_case.name = "some";
    some_case.c_name = "Choice_some";
    some_case.module = module_id(0);
    some_case.type = choice_type;
    some_case.payload_type = i32;
    some_case.enum_name = "Choice";
    some_case.case_name = "some";
    analyzer.checked_.enum_cases.emplace(analyzer.module_key(module_id(0), "some"), some_case);
    analyzer.global_values_.emplace(
        analyzer.module_key(module_id(0), "choice"),
        symbol(SymbolKind::local, "choice", module_id(0), choice_type)
    );
    analyzer.global_values_.emplace(
        analyzer.module_key(module_id(0), "void_value"),
        symbol(SymbolKind::local, "void_value", module_id(0), void_type)
    );

    EXPECT_TRUE(types.is_bool(analyzer.analyze_match_expr(enum_match_id, module.exprs[enum_match_id.value], invalid_type_handle)));
    EXPECT_TRUE(types.is_integer(analyzer.analyze_match_expr(
        binding_value_match_id,
        module.exprs[binding_value_match_id.value],
        invalid_type_handle
    )));
    EXPECT_FALSE(is_valid(analyzer.analyze_match_expr(void_match_id, module.exprs[void_match_id.value], invalid_type_handle)));

    std::vector<std::string> covered;
    bool saw_wildcard = false;
    EXPECT_EQ(analyzer.analyze_enum_case_pattern(syntax::invalid_pattern_id, choice_type, covered, saw_wildcard), nullptr);
    EXPECT_EQ(analyzer.analyze_single_enum_case_pattern(syntax::invalid_pattern_id, choice_type, covered, saw_wildcard), nullptr);
    EXPECT_EQ(analyzer.analyze_single_enum_case_pattern(missing_scoped_pattern_id, choice_type, covered, saw_wildcard), nullptr);

    bool covered_true = false;
    bool covered_false = false;
    bool value_saw_wildcard = false;
    EXPECT_EQ(analyzer.analyze_value_pattern(
        syntax::invalid_pattern_id,
        types.builtin(BuiltinType::bool_),
        covered_true,
        covered_false,
        value_saw_wildcard
    ), nullptr);
    EXPECT_EQ(analyzer.analyze_single_value_pattern(
        syntax::invalid_pattern_id,
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

    syntax::ExprNode field_expr;
    field_expr.kind = syntax::ExprKind::field;
    const ExprId field_expr_id = module.push_expr(field_expr);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(module, diagnostics);
    analyzer.checked_.expr_c_names.assign(module.exprs.size(), {});
    analyzer.checked_.enum_cases.clear();
    analyzer.current_module_ = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    const TypeHandle i32 = analyzer.checked_.types.builtin(BuiltinType::i32);
    analyzer.global_values_.emplace(
        analyzer.module_key(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), SEMA_TEST_CONST_VALUE_NAME),
        symbol(SymbolKind::const_, SEMA_TEST_CONST_VALUE_NAME, module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), i32)
    );

    std::unordered_set<std::string> dependencies;
    EXPECT_TRUE(analyzer.is_const_evaluable_expr(scoped_value_expr, dependencies));
    EXPECT_EQ(
        dependencies.count(analyzer.module_key(module_id(SEMA_TEST_LIB_ONE_MODULE_INDEX), SEMA_TEST_CONST_VALUE_NAME)),
        1u
    );

    sema::EnumCaseInfo case_info;
    case_info.c_name = SEMA_TEST_ENUM_CASE_C_NAME;
    analyzer.checked_.enum_cases.emplace(std::string(SEMA_TEST_ENUM_CASE_C_NAME), case_info);
    analyzer.checked_.expr_c_names[field_expr_id.value] = std::string(SEMA_TEST_ENUM_CASE_C_NAME);

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

    syntax::ExprNode unsupported_unary;
    unsupported_unary.kind = syntax::ExprKind::unary;
    unsupported_unary.unary_op = syntax::UnaryOp::address_of;
    unsupported_unary.unary_operand = integer_literal;
    const ExprId unsupported_unary_id = module.push_expr(unsupported_unary);

    syntax::ExprNode invalid_child_cast;
    invalid_child_cast.kind = syntax::ExprKind::cast;
    invalid_child_cast.cast_expr = syntax::invalid_expr_id;
    const ExprId invalid_child_cast_id = module.push_expr(invalid_child_cast);

    syntax::ExprNode empty_struct_literal;
    empty_struct_literal.kind = syntax::ExprKind::struct_literal;
    const ExprId empty_struct_literal_id = module.push_expr(empty_struct_literal);

    syntax::ExprNode invalid_binary;
    invalid_binary.kind = syntax::ExprKind::binary;
    invalid_binary.binary_op = SEMA_TEST_INVALID_BINARY_OP;
    invalid_binary.binary_lhs = integer_literal;
    invalid_binary.binary_rhs = integer_literal;
    const ExprId invalid_binary_id = module.push_expr(invalid_binary);

    syntax::ExprNode plain_field;
    plain_field.kind = syntax::ExprKind::field;
    const ExprId plain_field_id = module.push_expr(plain_field);

    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(module, diagnostics);
    analyzer.checked_.expr_c_names.assign(module.exprs.size(), {});
    analyzer.current_module_ = module_id(SEMA_TEST_ROOT_MODULE_INDEX);

    const TypeHandle i32 = analyzer.checked_.types.builtin(BuiltinType::i32);
    analyzer.global_values_.emplace(
        analyzer.module_key(module_id(SEMA_TEST_ROOT_MODULE_INDEX), SEMA_TEST_LOCAL_VALUE_NAME),
        symbol(SymbolKind::local, SEMA_TEST_LOCAL_VALUE_NAME, module_id(SEMA_TEST_ROOT_MODULE_INDEX), i32)
    );
    analyzer.global_values_.emplace(
        analyzer.module_key(module_id(SEMA_TEST_ROOT_MODULE_INDEX), SEMA_TEST_ENUM_VALUE_NAME),
        symbol(SymbolKind::enum_case, SEMA_TEST_ENUM_VALUE_NAME, module_id(SEMA_TEST_ROOT_MODULE_INDEX), i32)
    );

    std::unordered_set<std::string> dependencies;
    EXPECT_FALSE(analyzer.is_const_evaluable_expr(syntax::invalid_expr_id, dependencies));
    EXPECT_FALSE(analyzer.is_const_evaluable_expr(missing_name, dependencies));
    EXPECT_FALSE(analyzer.is_const_evaluable_expr(local_name, dependencies));
    EXPECT_TRUE(analyzer.is_const_evaluable_expr(enum_name, dependencies));
    EXPECT_FALSE(analyzer.is_const_evaluable_expr(unsupported_unary_id, dependencies));
    EXPECT_FALSE(analyzer.is_const_evaluable_expr(invalid_child_cast_id, dependencies));
    EXPECT_TRUE(analyzer.is_const_evaluable_expr(empty_struct_literal_id, dependencies));
    EXPECT_FALSE(analyzer.is_const_evaluable_expr(invalid_binary_id, dependencies));
    EXPECT_FALSE(analyzer.is_const_evaluable_expr(plain_field_id, dependencies));
    EXPECT_TRUE(dependencies.empty());
}

} // namespace aurex::test
