#pragma once

#include <aurex/base/diagnostic.hpp>
#include <aurex/sema/diagnostic_kind.hpp>
#include <aurex/sema/function.hpp>
#include <aurex/sema/storage.hpp>
#include <aurex/syntax/ast.hpp>

#include <span>
#include <string>
#include <string_view>

namespace aurex::sema {

struct CheckedModule;
struct Symbol;

struct FunctionRegistrationRequest {
    explicit FunctionRegistrationRequest(const syntax::ItemNode& function_item) noexcept : item(function_item)
    {
    }

    const syntax::ItemNode& item;
    syntax::ModuleId owner = syntax::INVALID_MODULE_ID;
    FunctionLookupKey key;
    std::string_view c_name;
    TypeHandle method_owner_type = INVALID_TYPE_HANDLE;
    syntax::ModuleId trait_module = syntax::INVALID_MODULE_ID;
    IdentId trait_name_id = INVALID_IDENT_ID;
    TypeHandle return_type = INVALID_TYPE_HANDLE;
    std::span<const TypeHandle> param_types;
    syntax::ItemId item_id = syntax::INVALID_ITEM_ID;
    base::u32 part_index = 0;
    StableDefId stable_id;
    IncrementalKey incremental_key;
    bool is_trait_impl_method = false;
};

class FunctionRegistry final {
public:
    FunctionRegistry(CheckedModule& checked, SemaMap<FunctionLookupKey, Symbol, FunctionLookupKeyHash>& global_values,
        base::DiagnosticSink& diagnostics, const IdentifierInterner* source_names = nullptr) noexcept;

    void register_function(const FunctionRegistrationRequest& request);

private:
    [[nodiscard]] bool same_signature(const FunctionSignature& existing, TypeHandle return_type,
        std::span<const TypeHandle> param_types, bool is_variadic) const noexcept;
    [[nodiscard]] InternedText source_name_text(IdentId name_id, std::string_view fallback_name);
    void merge_function(FunctionLookupKey key, FunctionSignature signature, bool is_prototype);
    void insert_function_value(const FunctionLookupKey& key, const FunctionSignature& signature);
    void refresh_function_value(const FunctionLookupKey& key, const FunctionSignature& signature);
    void report(const base::SourceRange& range, SemanticDiagnosticKind kind, std::string message) const;
    void report_previous_declaration(const FunctionSignature& signature) const;

    CheckedModule& checked_;
    SemaMap<FunctionLookupKey, Symbol, FunctionLookupKeyHash>& global_values_;
    base::DiagnosticSink& diagnostics_;
    const IdentifierInterner* source_names_ = nullptr;
};

} // namespace aurex::sema
