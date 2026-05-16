#include <aurex/sema/function_registry.hpp>

#include <aurex/base/abi.hpp>
#include <aurex/sema/sema_messages.hpp>
#include <aurex/sema/sema.hpp>
#include <aurex/sema/symbol.hpp>

#include <utility>

namespace aurex::sema {

namespace {

[[nodiscard]] std::string abi_or_c_name(const syntax::ItemNode& item, const std::string& c_name) {
    if (!item.abi_name.empty()) {
        return std::string(item.abi_name);
    }
    return c_name;
}

[[nodiscard]] FunctionCallConv signature_call_conv(const FunctionSignature& signature) noexcept {
    return signature.is_extern_c || signature.is_export_c ? FunctionCallConv::c : FunctionCallConv::aurex;
}

[[nodiscard]] TypeHandle signature_function_type(TypeTable& types, const FunctionSignature& signature) {
    return types.function(
        signature_call_conv(signature),
        signature.is_unsafe,
        signature.is_variadic,
        signature.param_types,
        signature.return_type
    );
}

} // namespace

FunctionRegistry::FunctionRegistry(
    CheckedModule& checked,
    std::unordered_map<std::string, Symbol>& global_values,
    base::DiagnosticSink& diagnostics
) noexcept
    : checked_(checked),
      global_values_(global_values),
      diagnostics_(diagnostics) {}

void FunctionRegistry::register_function(
    const syntax::ItemNode& item,
    const syntax::ModuleId owner,
    std::string key,
    const std::string c_name,
    const TypeHandle method_owner_type,
    const TypeHandle return_type,
    std::vector<TypeHandle> param_types,
    const syntax::ItemId item_id
) {
    const bool is_prototype = item.is_prototype;

    FunctionSignature signature;
    signature.name = std::string(item.name);
    signature.name_id = item.name_id;
    signature.semantic_key = key;
    signature.c_name = abi_or_c_name(item, c_name);
    signature.module = owner;
    signature.method_owner_type = method_owner_type;
    signature.return_type = return_type;
    signature.param_types = std::move(param_types);
    signature.range = item.range;
    signature.is_extern_c = item.is_extern_c;
    signature.is_export_c = item.is_export_c;
    signature.is_unsafe = item.is_unsafe;
    signature.is_variadic = item.is_variadic;
    signature.has_prototype = is_prototype;
    signature.has_definition = !is_prototype && !item.is_extern_c;
    signature.is_method = syntax::is_valid(item.impl_type);
    signature.has_self_param = signature.is_method && !item.params.empty() && item.params.front().name == "self";
    signature.visibility = item.visibility;
    signature.prototype_item = is_prototype ? item_id : syntax::INVALID_ITEM_ID;
    signature.definition_item = signature.has_definition ? item_id : syntax::INVALID_ITEM_ID;

    if (syntax::is_valid(item_id) && item_id.value < this->checked_.item_c_names.size()) {
        this->checked_.item_c_names[item_id.value] = signature.c_name;
    }

    this->merge_function(std::move(key), std::move(signature), is_prototype);
}

void FunctionRegistry::merge_function(
    std::string key,
    FunctionSignature signature,
    const bool is_prototype
) {
    const auto existing = this->checked_.functions.find(key);
    if (existing == this->checked_.functions.end()) {
        auto inserted = this->checked_.functions.emplace(std::move(key), std::move(signature));
        this->insert_function_value(inserted.first->first, inserted.first->second);
        return;
    }

    FunctionSignature& prior = existing->second;
    if (!this->same_signature(prior, signature.return_type, signature.param_types, signature.is_variadic)) {
        prior.has_conflict = true;
        this->report(signature.range, sema_function_signature_mismatch_message(signature.name));
        return;
    }
    if (prior.is_extern_c ||
        signature.is_extern_c ||
        prior.is_export_c != signature.is_export_c ||
        prior.is_unsafe != signature.is_unsafe ||
        prior.c_name != signature.c_name) {
        prior.has_conflict = true;
        this->report(signature.range, sema_function_declaration_conflict_message(signature.name));
        return;
    }
    if (is_prototype) {
        if (prior.has_prototype) {
            prior.has_conflict = true;
            this->report(signature.range, sema_duplicate_function_prototype_message(signature.name));
            return;
        }
        if (prior.has_definition) {
            prior.has_conflict = true;
            this->report(signature.range, sema_function_prototype_order_message(signature.name));
            return;
        }
        prior.has_prototype = true;
        prior.visibility = signature.visibility;
        prior.prototype_item = signature.prototype_item;
        prior.range = signature.range;
        this->refresh_function_value(key, prior);
        return;
    }
    if (prior.has_definition) {
        prior.has_conflict = true;
        this->report(signature.range, sema_duplicate_function_definition_simple_message(signature.name));
        return;
    }
    prior.has_definition = true;
    prior.visibility = signature.visibility;
    prior.definition_item = signature.definition_item;
    prior.range = signature.range;
    this->refresh_function_value(key, prior);
}


bool FunctionRegistry::same_signature(
    const FunctionSignature& existing,
    const TypeHandle return_type,
    const std::vector<TypeHandle>& param_types,
    const bool is_variadic
) const noexcept {
    if (!this->checked_.types.same(existing.return_type, return_type)) {
        return false;
    }
    if (existing.is_variadic != is_variadic) {
        return false;
    }
    if (existing.param_types.size() != param_types.size()) {
        return false;
    }
    for (base::usize i = 0; i < param_types.size(); ++i) {
        if (!this->checked_.types.same(existing.param_types[i], param_types[i])) {
            return false;
        }
    }
    return true;
}

void FunctionRegistry::insert_function_value(const std::string& key, const FunctionSignature& signature) {
    const auto value_inserted = this->global_values_.emplace(key, Symbol {
        SymbolKind::function,
        signature.name,
        signature.name_id,
        signature.c_name,
        signature.module,
        signature_function_type(this->checked_.types, signature),
        signature.range,
        false,
        signature.visibility,
    });
    if (!value_inserted.second) {
        this->report(signature.range, sema_duplicate_value_definition_in_module_message(signature.name));
    }
}

void FunctionRegistry::refresh_function_value(const std::string& key, const FunctionSignature& signature) {
    const auto found = this->global_values_.find(key);
    if (found == this->global_values_.end()) {
        this->insert_function_value(key, signature);
        return;
    }
    found->second.c_name = signature.c_name;
    found->second.type = signature_function_type(this->checked_.types, signature);
    found->second.range = signature.range;
}

void FunctionRegistry::report(base::SourceRange range, std::string message) {
    this->diagnostics_.push(base::Diagnostic {
        base::Severity::error,
        range,
        std::move(message),
    });
}

} // namespace aurex::sema
