#pragma once

#include "aurex/base/source.hpp"
#include "aurex/sema/type.hpp"
#include "aurex/syntax/ast.hpp"
#include "aurex/syntax/ast_ids.hpp"

#include <string>
#include <vector>

namespace aurex::sema {

struct FunctionSignature {
    std::string name;
    std::string c_name;
    syntax::ModuleId module = syntax::invalid_module_id;
    TypeHandle method_owner_type = invalid_type_handle;
    TypeHandle return_type = invalid_type_handle;
    std::vector<TypeHandle> param_types;
    base::SourceRange range {};
    bool is_extern_c = false;
    bool is_export_c = false;
    bool is_variadic = false;
    bool has_prototype = false;
    bool has_definition = false;
    bool has_conflict = false;
    bool is_method = false;
    bool has_self_param = false;
    syntax::Visibility visibility = syntax::Visibility::public_;
    syntax::ItemId prototype_item = syntax::invalid_item_id;
    syntax::ItemId definition_item = syntax::invalid_item_id;
};

} // namespace aurex::sema
