#pragma once

#include "aurex/base/source.hpp"
#include "aurex/sema/type.hpp"
#include "aurex/syntax/ast.hpp"
#include "aurex/syntax/ast_ids.hpp"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace aurex::sema {

struct GenericEnumTemplateInfo {
    std::string name;
    syntax::ModuleId module = syntax::invalid_module_id;
    syntax::ItemId item = syntax::invalid_item_id;
    std::vector<std::string> params;
    base::SourceRange range {};
    syntax::Visibility visibility = syntax::Visibility::public_;
};

struct GenericStructTemplateInfo {
    std::string name;
    syntax::ModuleId module = syntax::invalid_module_id;
    syntax::ItemId item = syntax::invalid_item_id;
    std::vector<std::string> params;
    base::SourceRange range {};
    syntax::Visibility visibility = syntax::Visibility::public_;
};

struct GenericFunctionTemplateInfo {
    std::string name;
    syntax::ModuleId module = syntax::invalid_module_id;
    syntax::ItemId item = syntax::invalid_item_id;
    std::vector<std::string> params;
    base::SourceRange range {};
    syntax::Visibility visibility = syntax::Visibility::public_;
};

struct GenericTypeSubstitution {
    std::unordered_map<std::string, TypeHandle> types;
};

struct GenericEnumInstanceInfo {
    std::string name;
    syntax::ModuleId module = syntax::invalid_module_id;
    std::vector<TypeHandle> args;
};

struct GenericStructInstanceInfo {
    std::string name;
    syntax::ModuleId module = syntax::invalid_module_id;
    std::vector<TypeHandle> args;
};

struct GenericFunctionInstanceInfo {
    std::string name;
    std::string c_name;
    syntax::ModuleId module = syntax::invalid_module_id;
    syntax::ItemId item = syntax::invalid_item_id;
    std::vector<TypeHandle> args;
    TypeHandle return_type = invalid_type_handle;
    std::vector<TypeHandle> param_types;
    std::vector<TypeHandle> syntax_type_handles;
    std::vector<TypeHandle> expr_types;
    std::vector<std::string> expr_c_names;
    std::vector<std::string> pattern_c_names;
    std::vector<std::unordered_set<std::string>> pattern_case_sets;
    std::vector<TypeHandle> stmt_local_types;
};

} // namespace aurex::sema
