#pragma once

#include <aurex/sema/function.hpp>
#include <aurex/sema/type.hpp>
#include <aurex/syntax/ast.hpp>

#include <deque>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace aurex::sema {

struct StructFieldInfo {
    std::string name;
    std::string c_name;
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    TypeHandle type = INVALID_TYPE_HANDLE;
    base::SourceRange range {};
    syntax::Visibility visibility = syntax::Visibility::public_;
};

struct StructInfo {
    std::string name;
    IdentId name_id = INVALID_IDENT_ID;
    std::string c_name;
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    TypeHandle type = INVALID_TYPE_HANDLE;
    std::vector<StructFieldInfo> fields;
    bool is_opaque = false;
    bool is_generic_placeholder = false;
    syntax::Visibility visibility = syntax::Visibility::public_;
};

struct EnumCaseInfo {
    std::string name;
    IdentId name_id = INVALID_IDENT_ID;
    std::string c_name;
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    TypeHandle type = INVALID_TYPE_HANDLE;
    TypeHandle payload_type = INVALID_TYPE_HANDLE;
    std::vector<TypeHandle> payload_types;
    std::string value_text;
    base::SourceRange range {};
    std::string enum_name;
    std::string case_name;
    IdentId case_name_id = INVALID_IDENT_ID;
    syntax::Visibility visibility = syntax::Visibility::public_;
};

struct TypeAliasInfo {
    std::string name;
    IdentId name_id = INVALID_IDENT_ID;
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    syntax::TypeId target = syntax::INVALID_TYPE_ID;
    base::SourceRange range {};
    syntax::Visibility visibility = syntax::Visibility::public_;
};

enum class CoercionKind {
    contextual_integer_literal,
    contextual_float_literal,
    null_to_pointer,
    slice_to_expected_slice,
};

struct CoercionRecord {
    syntax::ExprId expr = syntax::INVALID_EXPR_ID;
    TypeHandle from_type = INVALID_TYPE_HANDLE;
    TypeHandle to_type = INVALID_TYPE_HANDLE;
    CoercionKind kind = CoercionKind::contextual_integer_literal;
};

struct GenericSideTables {
    bool sparse = false;
    std::vector<TypeHandle> expr_types;
    std::vector<TypeHandle> expr_expected_types;
    std::vector<std::string> expr_c_names;
    std::vector<std::string> pattern_c_names;
    std::vector<std::unordered_set<std::string>> pattern_case_sets;
    std::vector<TypeHandle> syntax_type_handles;
    std::vector<TypeHandle> stmt_local_types;
    std::unordered_map<base::u32, TypeHandle> sparse_expr_types;
    std::unordered_map<base::u32, TypeHandle> sparse_expr_expected_types;
    std::unordered_map<base::u32, std::string> sparse_expr_c_names;
    std::unordered_map<base::u32, std::string> sparse_pattern_c_names;
    std::unordered_map<base::u32, std::unordered_set<std::string>> sparse_pattern_case_sets;
    std::unordered_map<base::u32, TypeHandle> sparse_syntax_type_handles;
    std::unordered_map<base::u32, TypeHandle> sparse_stmt_local_types;
};

struct GenericFunctionInstanceInfo {
    std::string key;
    syntax::ItemId item = syntax::INVALID_ITEM_ID;
    FunctionSignature signature;
    GenericSideTables side_tables;
};

struct CheckedModule {
    // CheckedModule is the bridge between syntax and codegen. It is deliberately
    // side-table based so AST nodes remain parse-only data.
    TypeTable types;
    std::vector<TypeHandle> expr_types;
    std::vector<TypeHandle> expr_expected_types;
    std::vector<std::string> expr_c_names;
    std::vector<std::string> pattern_c_names;
    std::vector<std::unordered_set<std::string>> pattern_case_sets;
    std::vector<TypeHandle> syntax_type_handles;
    std::vector<TypeHandle> stmt_local_types;
    std::vector<std::string> item_c_names;
    std::vector<CoercionRecord> coercions;
    std::unordered_map<std::string, FunctionSignature> functions;
    std::unordered_map<std::string, StructInfo> structs;
    std::unordered_map<std::string, EnumCaseInfo> enum_cases;
    std::unordered_map<std::string, TypeAliasInfo> type_aliases;
    std::deque<GenericFunctionInstanceInfo> generic_function_instances;
    std::optional<syntax::AstModule> normalized_ast;
};

[[nodiscard]] std::string dump_checked_module(const CheckedModule& checked);
[[nodiscard]] std::string struct_display_name(const TypeTable& types, const StructInfo& info);
[[nodiscard]] std::string enum_display_name(const TypeTable& types, const EnumCaseInfo& info);
[[nodiscard]] std::string enum_case_display_name(const TypeTable& types, const EnumCaseInfo& info);

} // namespace aurex::sema
