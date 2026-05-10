#pragma once

#include <aurex/base/source.hpp>
#include <aurex/sema/type.hpp>
#include <aurex/syntax/ast.hpp>
#include <aurex/syntax/ast_ids.hpp>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace aurex::sema {

struct GenericEnumTemplateInfo {
    std::string name;
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    syntax::ItemId item = syntax::INVALID_ITEM_ID;
    std::vector<std::string> params;
    base::SourceRange range {};
    syntax::Visibility visibility = syntax::Visibility::public_;
};

struct GenericStructTemplateInfo {
    std::string name;
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    syntax::ItemId item = syntax::INVALID_ITEM_ID;
    std::vector<std::string> params;
    base::SourceRange range {};
    syntax::Visibility visibility = syntax::Visibility::public_;
};

struct GenericFunctionTemplateInfo {
    std::string name;
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    syntax::ItemId item = syntax::INVALID_ITEM_ID;
    syntax::TypeId impl_type = syntax::INVALID_TYPE_ID;
    std::vector<std::string> params;
    base::SourceRange range {};
    syntax::Visibility visibility = syntax::Visibility::public_;
    bool is_method = false;
    base::usize impl_generic_param_count = 0;
};

struct GenericTypeSubstitution {
    std::unordered_map<std::string, TypeHandle> types;
};

struct GenericEnumInstanceInfo {
    std::string name;
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    std::vector<TypeHandle> args;
};

struct GenericStructInstanceInfo {
    std::string name;
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    std::vector<TypeHandle> args;
};

struct GenericFunctionInstanceInfo {
    std::string name;
    std::string c_name;
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    syntax::ItemId item = syntax::INVALID_ITEM_ID;
    TypeHandle method_owner_type = INVALID_TYPE_HANDLE;
    std::vector<TypeHandle> args;
    TypeHandle return_type = INVALID_TYPE_HANDLE;
    std::vector<TypeHandle> param_types;
    syntax::Visibility visibility = syntax::Visibility::public_;
    bool is_method = false;
    bool has_self_param = false;
    std::unordered_map<base::u32, TypeHandle> syntax_type_handles;
    std::unordered_map<base::u32, TypeHandle> expr_types;
    std::unordered_map<base::u32, std::string> expr_c_names;
    std::unordered_map<base::u32, std::string> pattern_c_names;
    std::unordered_map<base::u32, std::unordered_set<std::string>> pattern_case_sets;
    std::unordered_map<base::u32, TypeHandle> stmt_local_types;
};

} // namespace aurex::sema
