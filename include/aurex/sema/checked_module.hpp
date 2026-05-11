#pragma once

#include <aurex/sema/function.hpp>
#include <aurex/sema/type.hpp>
#include <aurex/syntax/ast.hpp>

#include <deque>
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
    std::string c_name;
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    TypeHandle type = INVALID_TYPE_HANDLE;
    TypeHandle payload_type = INVALID_TYPE_HANDLE;
    std::string value_text;
    base::SourceRange range {};
    std::string enum_name;
    std::string case_name;
    syntax::Visibility visibility = syntax::Visibility::public_;
};

struct TypeAliasInfo {
    std::string name;
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    syntax::TypeId target = syntax::INVALID_TYPE_ID;
    base::SourceRange range {};
    syntax::Visibility visibility = syntax::Visibility::public_;
};

struct GenericSideTables {
    std::vector<TypeHandle> expr_types;
    std::vector<std::string> expr_c_names;
    std::vector<std::string> pattern_c_names;
    std::vector<std::unordered_set<std::string>> pattern_case_sets;
    std::vector<TypeHandle> syntax_type_handles;
    std::vector<TypeHandle> stmt_local_types;
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
    std::vector<std::string> expr_c_names;
    std::vector<std::string> pattern_c_names;
    std::vector<std::unordered_set<std::string>> pattern_case_sets;
    std::vector<TypeHandle> syntax_type_handles;
    std::vector<TypeHandle> stmt_local_types;
    std::vector<std::string> item_c_names;
    std::unordered_map<std::string, FunctionSignature> functions;
    std::unordered_map<std::string, StructInfo> structs;
    std::unordered_map<std::string, EnumCaseInfo> enum_cases;
    std::unordered_map<std::string, TypeAliasInfo> type_aliases;
    std::deque<GenericFunctionInstanceInfo> generic_function_instances;
};

[[nodiscard]] std::string dump_checked_module(const CheckedModule& checked);

} // namespace aurex::sema
