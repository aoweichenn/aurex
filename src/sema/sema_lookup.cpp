#include <aurex/sema/sema_messages.hpp>
#include <aurex/syntax/module.hpp>

#include <algorithm>
#include <limits>
#include <vector>

#include <sema/internal/sema_core.hpp>
#include <sema/internal/sema_lookup_indexer.hpp>

namespace aurex::sema {

namespace {

constexpr base::usize SEMA_LOOKUP_SHORT_SUGGESTION_NAME_LENGTH = 4;
constexpr base::usize SEMA_LOOKUP_MEDIUM_SUGGESTION_NAME_LENGTH = 8;
constexpr base::usize SEMA_LOOKUP_SHORT_SUGGESTION_MAX_DISTANCE = 1;
constexpr base::usize SEMA_LOOKUP_MEDIUM_SUGGESTION_MAX_DISTANCE = 2;
constexpr base::usize SEMA_LOOKUP_LONG_SUGGESTION_MAX_DISTANCE = 3;
constexpr base::usize SEMA_INCREMENTAL_FINGERPRINT_TYPE_TEXT_BUDGET = 12;
constexpr std::string_view SEMA_INCREMENTAL_FINGERPRINT_METHOD_TAG = "method";
constexpr std::string_view SEMA_INCREMENTAL_FINGERPRINT_FUNCTION_TAG = "function";
constexpr std::string_view SEMA_INCREMENTAL_FINGERPRINT_VARIADIC_TAG = "variadic";
constexpr std::string_view SEMA_INCREMENTAL_FINGERPRINT_FIXED_TAG = "fixed";

struct NameSuggestion {
    std::string_view name;
    base::usize distance = std::numeric_limits<base::usize>::max();
};

[[nodiscard]] base::usize suggestion_distance_limit(const std::string_view name) noexcept
{
    if (name.size() < SEMA_LOOKUP_SHORT_SUGGESTION_NAME_LENGTH) {
        return SEMA_LOOKUP_SHORT_SUGGESTION_MAX_DISTANCE;
    }
    if (name.size() < SEMA_LOOKUP_MEDIUM_SUGGESTION_NAME_LENGTH) {
        return SEMA_LOOKUP_MEDIUM_SUGGESTION_MAX_DISTANCE;
    }
    return SEMA_LOOKUP_LONG_SUGGESTION_MAX_DISTANCE;
}

[[nodiscard]] base::usize bounded_edit_distance(
    const std::string_view lhs, const std::string_view rhs, const base::usize limit)
{
    if (lhs == rhs) {
        return 0;
    }
    const base::usize lhs_size = lhs.size();
    const base::usize rhs_size = rhs.size();
    const base::usize size_delta = lhs_size > rhs_size ? lhs_size - rhs_size : rhs_size - lhs_size;
    if (size_delta > limit) {
        return limit + 1;
    }

    std::vector<base::usize> previous(rhs_size + 1);
    std::vector<base::usize> current(rhs_size + 1);
    for (base::usize column = 0; column <= rhs_size; ++column) {
        previous[column] = column;
    }

    for (base::usize row = 1; row <= lhs_size; ++row) {
        current[0] = row;
        base::usize row_minimum = current[0];
        for (base::usize column = 1; column <= rhs_size; ++column) {
            const base::usize substitution_cost = lhs[row - 1] == rhs[column - 1] ? 0 : 1;
            current[column] = std::min({
                previous[column] + 1,
                current[column - 1] + 1,
                previous[column - 1] + substitution_cost,
            });
            row_minimum = std::min(row_minimum, current[column]);
        }
        if (row_minimum > limit) {
            return limit + 1;
        }
        previous.swap(current);
    }
    return previous[rhs_size];
}

void consider_suggestion(NameSuggestion& best, const std::string_view requested, const std::string_view candidate)
{
    if (candidate.empty() || candidate == requested) {
        return;
    }
    const base::usize limit = suggestion_distance_limit(requested);
    const base::usize distance = bounded_edit_distance(requested, candidate, limit);
    if (distance > limit) {
        return;
    }
    if (distance < best.distance || (distance == best.distance && (best.name.empty() || candidate < best.name))) {
        best = NameSuggestion{candidate, distance};
    }
}

} // namespace

syntax::ModuleId SemanticAnalyzerCore::item_module(const syntax::ItemId item) const noexcept
{
    if (!syntax::is_valid(item) || item.value >= this->ctx_.module.item_modules.size()) {
        return syntax::INVALID_MODULE_ID;
    }
    return this->ctx_.module.item_modules[item.value];
}

const syntax::ItemImportScope* SemanticAnalyzerCore::item_import_scope(const syntax::ItemId item) const noexcept
{
    if (!syntax::is_valid(item)) {
        return nullptr;
    }
    for (const syntax::ItemImportScope& scope : this->ctx_.module.item_import_scopes) {
        if (scope.contains(item)) {
            return &scope;
        }
    }
    return nullptr;
}

bool SemanticAnalyzerCore::uses_item_import_scope(const syntax::ModuleId module) const noexcept
{
    if (!syntax::is_valid(module) || !syntax::is_valid(this->state_.flow.current_item)) {
        return false;
    }
    const syntax::ModuleId item_owner = this->item_module(this->state_.flow.current_item);
    return syntax::is_valid(item_owner) && item_owner.value == module.value
        && this->item_import_scope(this->state_.flow.current_item) != nullptr;
}

std::span<const syntax::ResolvedImport> SemanticAnalyzerCore::imports_for_scope(
    const syntax::ModuleId module) const noexcept
{
    if (!syntax::is_valid(module)) {
        return {};
    }
    if (syntax::is_valid(this->state_.flow.current_item)) {
        const syntax::ModuleId item_owner = this->item_module(this->state_.flow.current_item);
        if (syntax::is_valid(item_owner) && item_owner.value == module.value) {
            if (const syntax::ItemImportScope* const scope = this->item_import_scope(this->state_.flow.current_item);
                scope != nullptr) {
                return scope->imports;
            }
        }
    }
    if (module.value >= this->ctx_.module.modules.size()) {
        return {};
    }
    return this->ctx_.module.modules[module.value].imports;
}

bool SemanticAnalyzerCore::import_alias_exists_outside_current_scope(const std::string_view alias) const noexcept
{
    if (!syntax::is_valid(this->state_.flow.current_module)) {
        return false;
    }
    const syntax::ItemImportScope* const current_scope = this->item_import_scope(this->state_.flow.current_item);
    for (const syntax::ItemImportScope& scope : this->ctx_.module.item_import_scopes) {
        if (&scope == current_scope) {
            continue;
        }
        const syntax::ModuleId scope_owner = this->item_module(syntax::ItemId{scope.item_begin});
        if (!syntax::is_valid(scope_owner) || scope_owner.value != this->state_.flow.current_module.value) {
            continue;
        }
        for (const syntax::ResolvedImport& import : scope.imports) {
            if (import.alias == alias) {
                return true;
            }
        }
    }
    if (this->state_.flow.current_module.value < this->ctx_.module.modules.size()) {
        for (const syntax::ResolvedImport& import :
            this->ctx_.module.modules[this->state_.flow.current_module.value].imports) {
            if (import.alias == alias) {
                return true;
            }
        }
    }
    return false;
}

std::string SemanticAnalyzerCore::qualified_name(const syntax::ModuleId module, const std::string_view name) const
{
    if (!syntax::is_valid(module) || module.value >= this->ctx_.module.modules.size()) {
        return std::string(name);
    }
    const std::string module_text = module_name(module);
    if (module_text.empty()) {
        return std::string(name);
    }
    return module_text + "." + std::string(name);
}

std::string_view SemanticAnalyzerCore::nearest_visible_value_name(const std::string_view name) const
{
    NameSuggestion best;
    std::vector<std::string_view> local_names;
    this->state_.names.symbols.append_visible_names(local_names);
    for (const std::string_view candidate : local_names) {
        consider_suggestion(best, name, candidate);
    }
    if (syntax::is_valid(this->state_.flow.current_module)) {
        for (const auto& entry : this->state_.names.global_values_by_name) {
            if (entry.first.module != this->state_.flow.current_module.value) {
                continue;
            }
            const std::string_view candidate = this->ctx_.module.identifier_text(entry.first.name);
            consider_suggestion(best, name, candidate);
        }
    }
    return best.name;
}

std::string_view SemanticAnalyzerCore::nearest_value_name_in_module(
    const syntax::ModuleId module, const std::string_view name) const
{
    NameSuggestion best;
    if (!syntax::is_valid(module)) {
        return best.name;
    }
    for (const syntax::ModuleId candidate_module : this->module_export_modules(module)) {
        for (const auto& entry : this->state_.names.global_values_by_name) {
            if (entry.first.module != candidate_module.value || entry.second == nullptr) {
                continue;
            }
            if (!this->can_access(candidate_module, entry.second->visibility)) {
                continue;
            }
            const std::string_view candidate = this->ctx_.module.identifier_text(entry.first.name);
            consider_suggestion(best, name, candidate);
        }
    }
    return best.name;
}

std::string_view SemanticAnalyzerCore::nearest_visible_type_name(const std::string_view name) const
{
    NameSuggestion best;
    if (!syntax::is_valid(this->state_.flow.current_module)) {
        return best.name;
    }
    const auto consider_local_module_key = [&](const ModuleLookupKey key) {
        if (key.module == this->state_.flow.current_module.value) {
            consider_suggestion(best, name, this->ctx_.module.identifier_text(key.name));
        }
    };
    for (const auto& entry : this->state_.names.named_types_by_name) {
        consider_local_module_key(entry.first);
    }
    for (const auto& entry : this->state_.names.type_aliases_by_name) {
        consider_local_module_key(entry.first);
    }
    for (const auto& entry : this->state_.names.generic_struct_templates_by_name) {
        consider_local_module_key(entry.first);
    }
    for (const auto& entry : this->state_.names.generic_enum_templates_by_name) {
        consider_local_module_key(entry.first);
    }
    for (const auto& entry : this->state_.names.generic_type_alias_templates_by_name) {
        consider_local_module_key(entry.first);
    }
    return best.name;
}

std::string_view SemanticAnalyzerCore::nearest_type_name_in_module(
    const syntax::ModuleId module, const std::string_view name) const
{
    NameSuggestion best;
    if (!syntax::is_valid(module)) {
        return best.name;
    }
    const auto consider_module_key = [&](const ModuleLookupKey key, const syntax::Visibility visibility) {
        const syntax::ModuleId owner{key.module};
        if (this->can_access(owner, visibility)) {
            consider_suggestion(best, name, this->ctx_.module.identifier_text(key.name));
        }
    };
    for (const syntax::ModuleId candidate_module : this->module_export_modules(module)) {
        for (const auto& entry : this->state_.names.named_types_by_name) {
            if (entry.first.module == candidate_module.value) {
                consider_module_key(entry.first, entry.second.visibility);
            }
        }
        for (const auto& entry : this->state_.names.type_aliases_by_name) {
            if (entry.first.module == candidate_module.value && entry.second != nullptr) {
                consider_module_key(entry.first, entry.second->visibility);
            }
        }
        for (const auto& entry : this->state_.names.generic_struct_templates_by_name) {
            if (entry.first.module == candidate_module.value && entry.second != nullptr) {
                consider_module_key(entry.first, entry.second->visibility);
            }
        }
        for (const auto& entry : this->state_.names.generic_enum_templates_by_name) {
            if (entry.first.module == candidate_module.value && entry.second != nullptr) {
                consider_module_key(entry.first, entry.second->visibility);
            }
        }
        for (const auto& entry : this->state_.names.generic_type_alias_templates_by_name) {
            if (entry.first.module == candidate_module.value && entry.second != nullptr) {
                consider_module_key(entry.first, entry.second->visibility);
            }
        }
    }
    return best.name;
}

std::string_view SemanticAnalyzerCore::nearest_visible_function_name(const std::string_view name) const
{
    NameSuggestion best;
    if (!syntax::is_valid(this->state_.flow.current_module)) {
        return best.name;
    }
    const auto consider_local_module_key = [&](const ModuleLookupKey key) {
        if (key.module == this->state_.flow.current_module.value) {
            consider_suggestion(best, name, this->ctx_.module.identifier_text(key.name));
        }
    };
    for (const auto& entry : this->state_.names.functions_by_name) {
        consider_local_module_key(entry.first);
    }
    for (const auto& entry : this->state_.names.generic_function_templates_by_name) {
        consider_local_module_key(entry.first);
    }
    return best.name;
}

std::string_view SemanticAnalyzerCore::nearest_function_name_in_module(
    const syntax::ModuleId module, const std::string_view name) const
{
    NameSuggestion best;
    if (!syntax::is_valid(module)) {
        return best.name;
    }
    for (const syntax::ModuleId candidate_module : this->module_export_modules(module)) {
        for (const auto& entry : this->state_.names.functions_by_name) {
            if (entry.first.module != candidate_module.value || entry.second == nullptr) {
                continue;
            }
            if (!this->can_access(candidate_module, entry.second->visibility)) {
                continue;
            }
            consider_suggestion(best, name, this->ctx_.module.identifier_text(entry.first.name));
        }
        for (const auto& entry : this->state_.names.generic_function_templates_by_name) {
            if (entry.first.module != candidate_module.value || entry.second == nullptr) {
                continue;
            }
            if (!this->can_access(candidate_module, entry.second->visibility)) {
                continue;
            }
            consider_suggestion(best, name, this->ctx_.module.identifier_text(entry.first.name));
        }
    }
    return best.name;
}

std::string_view SemanticAnalyzerCore::nearest_import_alias_name(const std::string_view name) const
{
    NameSuggestion best;
    if (!syntax::is_valid(this->state_.flow.current_module)
        || this->state_.flow.current_module.value >= this->ctx_.module.modules.size()) {
        return best.name;
    }
    for (const syntax::ResolvedImport& import : this->imports_for_scope(this->state_.flow.current_module)) {
        consider_suggestion(best, name, import.alias);
    }
    return best.name;
}

std::string_view SemanticAnalyzerCore::nearest_field_name(const StructInfo& info, const std::string_view name) const
{
    NameSuggestion best;
    for (const StructFieldInfo& field : info.fields) {
        if (!this->can_access(info.module, field.visibility)) {
            continue;
        }
        consider_suggestion(best, name, field.name);
    }
    return best.name;
}

std::string_view SemanticAnalyzerCore::nearest_enum_case_name(
    const TypeHandle enum_type, const std::string_view name) const
{
    NameSuggestion best;
    const EnumCaseList* const cases = this->find_enum_cases_by_type(enum_type);
    if (cases == nullptr) {
        return best.name;
    }
    for (const EnumCaseInfo* const enum_case : *cases) {
        if (enum_case == nullptr || !this->can_access(enum_case->module, enum_case->visibility)) {
            continue;
        }
        consider_suggestion(best, name, enum_case->case_name);
    }
    return best.name;
}

std::string_view SemanticAnalyzerCore::nearest_visible_enum_case_name(const std::string_view name) const
{
    NameSuggestion best;
    if (!syntax::is_valid(this->state_.flow.current_module)) {
        return best.name;
    }
    for (const auto& entry : this->state_.names.enum_cases_by_module_name) {
        const syntax::ModuleId owner{entry.first.module};
        if (entry.second == nullptr || owner.value != this->state_.flow.current_module.value
            || !this->can_access(owner, entry.second->visibility)) {
            continue;
        }
        consider_suggestion(best, name, entry.second->case_name);
    }
    return best.name;
}

std::string SemanticAnalyzerCore::c_symbol_name(const syntax::ModuleId module, const std::string_view name) const
{
    if (!syntax::is_valid(module) || module.value >= this->ctx_.module.modules.size()) {
        return std::string(name);
    }
    return syntax::mangle_c_symbol(this->ctx_.module.modules[module.value].path, name);
}

std::string SemanticAnalyzerCore::generic_template_key_prefix(
    const syntax::ModuleId module, const IdentId name_id, const std::string_view fallback_name) const
{
    const std::string_view name = this->ctx_.module.identifier_text(name_id);
    const std::string_view key_name = name.empty() ? fallback_name : name;
    std::string key = std::to_string(module.value);
    key.reserve(key.size() + 1 + key_name.size());
    key.push_back(':');
    key += key_name;
    return key;
}

FunctionLookupKey SemanticAnalyzerCore::function_key(
    const syntax::ItemNode& function, const syntax::ItemId function_id) const
{
    const syntax::ModuleId module = this->item_module(function_id);
    if (this->has_generic_params(function)) {
        return this->function_lookup_key(module, function.name_id);
    }
    if (!syntax::is_valid(function.impl_type)) {
        return this->function_lookup_key(module, function.name_id);
    }
    const TypeHandle owner_type = function.impl_type.value < this->state_.checked.syntax_type_handles.size()
        ? this->state_.checked.syntax_type_handles[function.impl_type.value]
        : INVALID_TYPE_HANDLE;
    return this->method_function_lookup_key(module, owner_type, function.name_id);
}

ModuleLookupKey SemanticAnalyzerCore::module_lookup_key(
    const syntax::ModuleId module, const IdentId name) const noexcept
{
    return ModuleLookupKey{
        module.value,
        name,
    };
}

MethodLookupKey SemanticAnalyzerCore::method_lookup_key(
    const syntax::ModuleId module, const TypeHandle owner_type, const IdentId name) const noexcept
{
    return MethodLookupKey{
        module.value,
        owner_type.value,
        name,
    };
}

FunctionLookupKey SemanticAnalyzerCore::function_lookup_key(
    const syntax::ModuleId module, const IdentId name) const noexcept
{
    return FunctionLookupKey{
        module.value,
        SEMA_LOOKUP_INVALID_KEY_PART,
        name,
    };
}

FunctionLookupKey SemanticAnalyzerCore::method_function_lookup_key(
    const syntax::ModuleId module, const TypeHandle owner_type, const IdentId name) const noexcept
{
    return FunctionLookupKey{
        module.value,
        owner_type.value,
        name,
    };
}

FunctionLookupKey SemanticAnalyzerCore::function_lookup_key_from_method(const MethodLookupKey key) const noexcept
{
    return FunctionLookupKey{
        key.module,
        key.owner_type,
        key.name,
    };
}

StableModuleId SemanticAnalyzerCore::stable_module_id(const syntax::ModuleId module) const noexcept
{
    if (!syntax::is_valid(module) || module.value >= this->ctx_.module.modules.size()) {
        return sema::stable_module_id(std::span<const std::string_view>{});
    }
    return sema::stable_module_id(this->ctx_.module.modules[module.value].path.parts);
}

StableDefId SemanticAnalyzerCore::stable_definition_id(const syntax::ModuleId module, const StableSymbolKind kind,
    const IdentId name_id, const std::string_view fallback_name, const base::u32 disambiguator) const
{
    const std::string_view name =
        this->ctx_.module.identifier_text(name_id).empty() ? fallback_name : this->ctx_.module.identifier_text(name_id);
    return sema::stable_definition_id(this->stable_module_id(module), kind, name, disambiguator);
}

StableMemberKey SemanticAnalyzerCore::stable_member_key(const StableDefId& owner, const StableSymbolKind kind,
    const IdentId name_id, const std::string_view fallback_name, const base::u32 disambiguator) const
{
    const std::string_view name =
        this->ctx_.module.identifier_text(name_id).empty() ? fallback_name : this->ctx_.module.identifier_text(name_id);
    return sema::stable_member_key(owner, kind, name, disambiguator);
}

IncrementalKey SemanticAnalyzerCore::stable_incremental_key(
    const StableDefId& definition, const std::string_view semantic_fingerprint) const
{
    return sema::stable_incremental_key(definition, semantic_fingerprint);
}

std::string SemanticAnalyzerCore::function_incremental_fingerprint(const std::string_view name,
    const TypeHandle return_type, const std::span<const TypeHandle> param_types, const bool is_method,
    const bool is_variadic) const
{
    std::string fingerprint;
    fingerprint.reserve(name.size() + (param_types.size() + 1U) * SEMA_INCREMENTAL_FINGERPRINT_TYPE_TEXT_BUDGET);
    fingerprint += name;
    fingerprint.push_back('|');
    fingerprint += is_method ? SEMA_INCREMENTAL_FINGERPRINT_METHOD_TAG : SEMA_INCREMENTAL_FINGERPRINT_FUNCTION_TAG;
    fingerprint.push_back('|');
    fingerprint += is_variadic ? SEMA_INCREMENTAL_FINGERPRINT_VARIADIC_TAG : SEMA_INCREMENTAL_FINGERPRINT_FIXED_TAG;
    fingerprint.push_back('|');
    fingerprint += std::to_string(return_type.value);
    for (const TypeHandle param_type : param_types) {
        fingerprint.push_back(',');
        fingerprint += std::to_string(param_type.value);
    }
    return fingerprint;
}

InternedText SemanticAnalyzerCore::source_name_text(const IdentId name_id, const std::string_view fallback_name)
{
    if (!this->owned_module_.has_value() && is_valid(name_id)) {
        if (!this->ctx_.module.identifier_text(name_id).empty()) {
            return InternedText{name_id, &this->ctx_.module.identifiers};
        }
    }
    return this->state_.checked.intern_text(fallback_name);
}

IdentId SemanticAnalyzerCore::intern_generated_key(const std::string_view key)
{
    return this->ctx_.module.intern_identifier(key);
}

ModuleLookupKey SemanticAnalyzerCore::intern_module_lookup_key(
    const syntax::ModuleId module, const IdentId name) const noexcept
{
    return this->module_lookup_key(module, name);
}

ModuleLookupKey SemanticAnalyzerCore::find_module_lookup_key(
    const syntax::ModuleId module, const IdentId name) const noexcept
{
    return this->module_lookup_key(module, name);
}

MethodLookupKey SemanticAnalyzerCore::intern_method_lookup_key(
    const syntax::ModuleId module, const TypeHandle owner_type, const IdentId name) const noexcept
{
    return this->method_lookup_key(module, owner_type, name);
}

MethodLookupKey SemanticAnalyzerCore::find_method_lookup_key(
    const syntax::ModuleId module, const TypeHandle owner_type, const IdentId name) const noexcept
{
    return this->method_lookup_key(module, owner_type, name);
}

std::string SemanticAnalyzerCore::method_c_symbol_name(const TypeHandle owner_type, const std::string_view name) const
{
    return this->state_.checked.types.c_name(owner_type) + "_" + std::string(name);
}

bool SemanticAnalyzerCore::can_access(const syntax::ModuleId owner, const syntax::Visibility visibility) const noexcept
{
    return owner.value == this->state_.flow.current_module.value || visibility == syntax::Visibility::public_;
}

syntax::ModuleId SemanticAnalyzerCore::owner_module(const TypeHandle owner_type) const noexcept
{
    if (!is_valid(owner_type)) {
        return syntax::INVALID_MODULE_ID;
    }
    if (const StructInfo* info = this->find_struct(owner_type); info != nullptr) {
        return info->module;
    }
    if (const auto found = this->state_.names.enum_cases_by_type.find(owner_type.value);
        found != this->state_.names.enum_cases_by_type.end() && !found->second.empty()) {
        return found->second.front()->module;
    }
    return syntax::INVALID_MODULE_ID;
}

const FunctionSignature* SemanticAnalyzerCore::find_method_in_owner_module(
    const TypeHandle owner_type, const IdentId name_id, const std::string_view name, const bool require_self) const
{
    static_cast<void>(name);
    const syntax::ModuleId owner = this->owner_module(owner_type);
    if (!syntax::is_valid(owner)) {
        return nullptr;
    }
    const FunctionSignature* signature = nullptr;
    const MethodLookupKey lookup_key = this->find_method_lookup_key(owner, owner_type, name_id);
    if (is_valid(lookup_key)) {
        if (const auto found = this->state_.names.methods_by_name.find(lookup_key);
            found != this->state_.names.methods_by_name.end()) {
            signature = found->second;
        }
    }
    if (signature == nullptr || !signature->is_method || (require_self && !signature->has_self_param)) {
        return nullptr;
    }
    if (!this->can_access(owner, signature->visibility)) {
        return nullptr;
    }
    return signature;
}

bool SemanticAnalyzerCore::method_receiver_matches(
    const FunctionSignature& signature, const TypeHandle receiver_type, const syntax::ExprId receiver)
{
    if (!signature.has_self_param || signature.param_types.empty()) {
        return false;
    }
    const TypeHandle self_type = signature.param_types.front();
    const base::SourceRange receiver_range =
        syntax::is_valid(receiver) && receiver.value < this->ctx_.module.exprs.size()
        ? this->ctx_.module.exprs.range(receiver.value)
        : signature.range;
    const auto report_receiver_type_mismatch = [&]() {
        this->report_type_mismatch(
            receiver_range, std::string(SEMA_METHOD_RECEIVER_TYPE_MISMATCH), self_type, receiver_type);
    };
    if (this->state_.checked.types.same(self_type, receiver_type)) {
        if (!this->check_m2_value_abi(self_type, ValueAbiContext::argument, receiver_range)) {
            return false;
        }
        return true;
    }
    if (!this->state_.checked.types.is_pointer(self_type) && !this->state_.checked.types.is_reference(self_type)) {
        report_receiver_type_mismatch();
        return false;
    }
    const TypeHandle pointee = this->state_.checked.types.get(self_type).pointee;
    if (this->state_.checked.types.is_pointer(receiver_type)
        || this->state_.checked.types.is_reference(receiver_type)) {
        const TypeInfo& self_info = this->state_.checked.types.get(self_type);
        const TypeInfo& receiver_info = this->state_.checked.types.get(receiver_type);
        if (self_info.kind != receiver_info.kind) {
            report_receiver_type_mismatch();
            return false;
        }
        if (!this->state_.checked.types.same(self_info.pointee, receiver_info.pointee)) {
            report_receiver_type_mismatch();
            return false;
        }
        if (self_info.pointer_mutability == PointerMutability::mut
            && receiver_info.pointer_mutability != PointerMutability::mut) {
            this->report_general(receiver_range, std::string(SEMA_MUTABLE_METHOD_RECEIVER_POINTER));
            return false;
        }
        return true;
    }
    if (!this->state_.checked.types.same(pointee, receiver_type)) {
        report_receiver_type_mismatch();
        return false;
    }
    const PointerMutability self_mutability = this->state_.checked.types.get(self_type).pointer_mutability;
    if (self_mutability == PointerMutability::mut) {
        if (!this->is_place_expr(receiver)) {
            this->report_general(receiver_range, std::string(SEMA_METHOD_RECEIVER_PLACE));
            return false;
        }
        if (!this->is_writable_place(receiver)) {
            this->report_general(receiver_range, std::string(SEMA_MUTABLE_METHOD_RECEIVER_WRITABLE));
            return false;
        }
    }
    return true;
}

const EnumCaseInfo* SemanticAnalyzerCore::find_enum_case_by_type_and_case(
    const TypeHandle enum_type, const IdentId case_name_id, const std::string_view case_name) const
{
    if (!is_valid(enum_type)) {
        return nullptr;
    }
    if (!is_valid(case_name_id)) {
        return nullptr;
    }
    const auto found = this->state_.names.enum_cases_by_type_and_case.find(EnumCaseLookupKey{
        enum_type.value,
        case_name_id,
    });
    if (found == this->state_.names.enum_cases_by_type_and_case.end() || found->second == nullptr
        || !this->state_.checked.types.same(found->second->type, enum_type) || found->second->case_name != case_name) {
        return nullptr;
    }
    return found->second;
}

const SemanticAnalyzerCore::EnumCaseList* SemanticAnalyzerCore::find_enum_cases_by_type(
    const TypeHandle enum_type) const noexcept
{
    if (!is_valid(enum_type)) {
        return nullptr;
    }
    const auto found = this->state_.names.enum_cases_by_type.find(enum_type.value);
    return found == this->state_.names.enum_cases_by_type.end() ? nullptr : &found->second;
}

const EnumCaseInfo* SemanticAnalyzerCore::find_enum_case_by_pattern_type(const syntax::TypeId enum_type_id,
    const IdentId case_name_id, const std::string_view case_name, const base::SourceRange& range)
{
    const TypeHandle enum_type = this->resolve_type(enum_type_id);
    if (!is_valid(enum_type)) {
        return nullptr;
    }
    if (this->state_.checked.types.get(enum_type).kind != TypeKind::enum_) {
        this->report_general(range, std::string(SEMA_ENUM_CASE_SCOPE_TYPE));
        return nullptr;
    }
    if (const EnumCaseInfo* result = this->find_enum_case_by_type_and_case(enum_type, case_name_id, case_name);
        result != nullptr) {
        return result;
    }
    this->report_lookup(
        range, sema_unknown_scoped_enum_case_message(this->state_.checked.types.display_name(enum_type), case_name));
    this->report_lookup_suggestion(range, this->nearest_enum_case_name(enum_type, case_name));
    return nullptr;
}

void SemanticAnalyzerCore::index_named_type(
    const syntax::ModuleId module, const IdentId name_id, const TypeHandle type, const syntax::Visibility visibility)
{
    LookupIndexer(*this).index_named_type(module, name_id, type, visibility);
}

void SemanticAnalyzerCore::index_type_alias(const TypeAliasInfo& info)
{
    LookupIndexer(*this).index_type_alias(info);
}

void SemanticAnalyzerCore::index_generic_struct_template(const GenericTemplateInfo& info)
{
    LookupIndexer(*this).index_generic_struct_template(info);
}

void SemanticAnalyzerCore::index_generic_enum_template(const GenericTemplateInfo& info)
{
    LookupIndexer(*this).index_generic_enum_template(info);
}

void SemanticAnalyzerCore::index_generic_type_alias_template(const GenericTemplateInfo& info)
{
    LookupIndexer(*this).index_generic_type_alias_template(info);
}

void SemanticAnalyzerCore::index_generic_function_template(const GenericTemplateInfo& info)
{
    LookupIndexer(*this).index_generic_function_template(info);
}

void SemanticAnalyzerCore::index_generic_method_template(const GenericTemplateInfo& info)
{
    LookupIndexer(*this).index_generic_method_template(info);
}

void SemanticAnalyzerCore::index_function_lookup(const FunctionSignature& signature)
{
    LookupIndexer(*this).index_function_lookup(signature);
}

void SemanticAnalyzerCore::index_method_lookup(const syntax::ModuleId module, const TypeHandle owner_type,
    const IdentId name_id, const FunctionSignature& signature)
{
    LookupIndexer(*this).index_method_lookup(module, owner_type, name_id, signature);
}

void SemanticAnalyzerCore::index_function_value(const FunctionSignature& signature)
{
    LookupIndexer(*this).index_function_value(signature);
}

void SemanticAnalyzerCore::index_global_value(const Symbol& symbol)
{
    LookupIndexer(*this).index_global_value(symbol);
}

bool SemanticAnalyzerCore::named_type_lookup_complete() const noexcept
{
    return LookupIndexer(const_cast<SemanticAnalyzerCore&>(*this)).named_type_lookup_complete();
}

bool SemanticAnalyzerCore::type_alias_lookup_complete() const noexcept
{
    return LookupIndexer(const_cast<SemanticAnalyzerCore&>(*this)).type_alias_lookup_complete();
}

bool SemanticAnalyzerCore::generic_struct_lookup_complete() const noexcept
{
    return LookupIndexer(const_cast<SemanticAnalyzerCore&>(*this)).generic_struct_lookup_complete();
}

bool SemanticAnalyzerCore::generic_enum_lookup_complete() const noexcept
{
    return LookupIndexer(const_cast<SemanticAnalyzerCore&>(*this)).generic_enum_lookup_complete();
}

bool SemanticAnalyzerCore::generic_type_alias_lookup_complete() const noexcept
{
    return LookupIndexer(const_cast<SemanticAnalyzerCore&>(*this)).generic_type_alias_lookup_complete();
}

bool SemanticAnalyzerCore::generic_function_lookup_complete() const noexcept
{
    return LookupIndexer(const_cast<SemanticAnalyzerCore&>(*this)).generic_function_lookup_complete();
}

bool SemanticAnalyzerCore::generic_method_lookup_complete() const noexcept
{
    return LookupIndexer(const_cast<SemanticAnalyzerCore&>(*this)).generic_method_lookup_complete();
}

bool SemanticAnalyzerCore::function_lookup_complete() const noexcept
{
    return LookupIndexer(const_cast<SemanticAnalyzerCore&>(*this)).function_lookup_complete();
}

bool SemanticAnalyzerCore::global_value_lookup_complete() const noexcept
{
    return LookupIndexer(const_cast<SemanticAnalyzerCore&>(*this)).global_value_lookup_complete();
}

bool SemanticAnalyzerCore::enum_case_module_lookup_complete() const noexcept
{
    return LookupIndexer(const_cast<SemanticAnalyzerCore&>(*this)).enum_case_module_lookup_complete();
}

bool SemanticAnalyzerCore::top_level_value_name_exists(
    const syntax::ModuleId module, const IdentId name_id, const std::string_view name) const
{
    return LookupIndexer(const_cast<SemanticAnalyzerCore&>(*this)).top_level_value_name_exists(module, name_id, name);
}

bool SemanticAnalyzerCore::module_type_or_value_name_exists(
    const syntax::ModuleId module, const IdentId name_id, const std::string_view name) const
{
    return LookupIndexer(const_cast<SemanticAnalyzerCore&>(*this))
        .module_type_or_value_name_exists(module, name_id, name);
}

bool SemanticAnalyzerCore::current_generic_param_exists(
    const IdentId name_id, const std::string_view fallback_name) const
{
    return LookupIndexer(const_cast<SemanticAnalyzerCore&>(*this)).current_generic_param_exists(name_id, fallback_name);
}

bool SemanticAnalyzerCore::visible_type_name_exists(const IdentId name_id, const std::string_view name) const
{
    return LookupIndexer(const_cast<SemanticAnalyzerCore&>(*this)).visible_type_name_exists(name_id, name);
}

bool SemanticAnalyzerCore::can_define_local_name(
    const IdentId name_id, const std::string_view name, const base::SourceRange& range) const
{
    return LookupIndexer(const_cast<SemanticAnalyzerCore&>(*this)).can_define_local_name(name_id, name, range);
}

bool SemanticAnalyzerCore::type_member_name_exists(
    const TypeHandle owner_type, const IdentId name_id, const std::string_view name) const
{
    return LookupIndexer(const_cast<SemanticAnalyzerCore&>(*this)).type_member_name_exists(owner_type, name_id, name);
}

void SemanticAnalyzerCore::index_enum_case(const EnumCaseInfo& info)
{
    LookupIndexer(*this).index_enum_case(info);
}

} // namespace aurex::sema
