#pragma once

#include <aurex/frontend/sema/identifier.hpp>
#include <aurex/frontend/sema/type.hpp>
#include <aurex/frontend/syntax/core/ast.hpp>
#include <aurex/frontend/syntax/core/ast_ids.hpp>
#include <aurex/infrastructure/base/source.hpp>
#include <aurex/infrastructure/query/generic_instance_key.hpp>

#include <string>

namespace aurex::sema {

struct FunctionSignature {
    InternedText name;
    IdentId name_id = INVALID_IDENT_ID;
    FunctionLookupKey semantic_key;
    StableDefId stable_id;
    IncrementalKey incremental_key;
    query::GenericInstanceKey generic_instance_key;
    InternedText c_name;
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    TypeHandle method_owner_type = INVALID_TYPE_HANDLE;
    syntax::ModuleId trait_module = syntax::INVALID_MODULE_ID;
    IdentId trait_name_id = INVALID_IDENT_ID;
    TypeHandle return_type = INVALID_TYPE_HANDLE;
    TypeHandleList param_types;
    TypeHandleList generic_args;
    base::SourceRange range{};
    bool is_extern_c = false;
    bool is_export_c = false;
    bool is_unsafe = false;
    bool is_variadic = false;
    bool has_prototype = false;
    bool has_definition = false;
    bool has_conflict = false;
    bool is_method = false;
    bool has_self_param = false;
    bool is_trait_impl_method = false;
    bool is_trait_default_method_instance = false;
    syntax::Visibility visibility = syntax::Visibility::public_;
    syntax::ItemId prototype_item = syntax::INVALID_ITEM_ID;
    syntax::ItemId definition_item = syntax::INVALID_ITEM_ID;
    base::u32 part_index = 0;
};

[[nodiscard]] std::string function_display_name(const TypeTable& types, const FunctionSignature& signature);

} // namespace aurex::sema
