#pragma once

#include "aurex/base/source.hpp"
#include "aurex/sema/type.hpp"
#include "aurex/syntax/ast.hpp"
#include "aurex/syntax/ast_ids.hpp"

#include <string>
#include <unordered_map>
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

} // namespace aurex::sema
