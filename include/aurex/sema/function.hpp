#pragma once

#include <aurex/base/source.hpp>
#include <aurex/sema/identifier.hpp>
#include <aurex/sema/type.hpp>
#include <aurex/syntax/ast.hpp>
#include <aurex/syntax/ast_ids.hpp>

#include <string>

namespace aurex::sema {

struct FunctionSignature {
    InternedText name;
    IdentId name_id = INVALID_IDENT_ID;
    FunctionLookupKey semantic_key;
    InternedText c_name;
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    TypeHandle method_owner_type = INVALID_TYPE_HANDLE;
    TypeHandle return_type = INVALID_TYPE_HANDLE;
    TypeHandleList param_types;
    TypeHandleList generic_args;
    base::SourceRange range {};
    bool is_extern_c = false;
    bool is_export_c = false;
    bool is_unsafe = false;
    bool is_variadic = false;
    bool has_prototype = false;
    bool has_definition = false;
    bool has_conflict = false;
    bool is_method = false;
    bool has_self_param = false;
    syntax::Visibility visibility = syntax::Visibility::public_;
    syntax::ItemId prototype_item = syntax::INVALID_ITEM_ID;
    syntax::ItemId definition_item = syntax::INVALID_ITEM_ID;
};

[[nodiscard]] std::string function_display_name(const TypeTable& types, const FunctionSignature& signature);

} // namespace aurex::sema
