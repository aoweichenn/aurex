#include <aurex/sema/sema.hpp>

#include <aurex/sema/sema_messages.hpp>
#include <aurex/syntax/module.hpp>

#include <algorithm>
#include <array>
#include <limits>
#include <vector>

namespace aurex::sema {

namespace {

constexpr std::string_view SEMA_LOOKUP_UNKNOWN_MODULE_NAME = "<unknown>";
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

[[nodiscard]] base::usize suggestion_distance_limit(const std::string_view name) noexcept {
    if (name.size() < SEMA_LOOKUP_SHORT_SUGGESTION_NAME_LENGTH) {
        return SEMA_LOOKUP_SHORT_SUGGESTION_MAX_DISTANCE;
    }
    if (name.size() < SEMA_LOOKUP_MEDIUM_SUGGESTION_NAME_LENGTH) {
        return SEMA_LOOKUP_MEDIUM_SUGGESTION_MAX_DISTANCE;
    }
    return SEMA_LOOKUP_LONG_SUGGESTION_MAX_DISTANCE;
}

[[nodiscard]] base::usize bounded_edit_distance(
    const std::string_view lhs,
    const std::string_view rhs,
    const base::usize limit
) {
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

void consider_suggestion(
    NameSuggestion& best,
    const std::string_view requested,
    const std::string_view candidate
) {
    if (candidate.empty() || candidate == requested) {
        return;
    }
    const base::usize limit = suggestion_distance_limit(requested);
    const base::usize distance = bounded_edit_distance(requested, candidate, limit);
    if (distance > limit) {
        return;
    }
    if (distance < best.distance ||
        (distance == best.distance && (best.name.empty() || candidate < best.name))) {
        best = NameSuggestion {candidate, distance};
    }
}

[[nodiscard]] bool module_path_matches_parts(
    const syntax::ModulePath& path,
    const std::vector<std::string_view>& parts
) noexcept {
    if (path.parts.size() != parts.size()) {
        return false;
    }
    for (base::usize i = 0; i < parts.size(); ++i) {
        if (path.parts[i] != parts[i]) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool module_path_matches_prefix(
    const syntax::ModulePath& path,
    const std::vector<std::string_view>& parts,
    const base::usize prefix_size
) noexcept {
    if (path.parts.size() != prefix_size || parts.size() < prefix_size) {
        return false;
    }
    for (base::usize i = 0; i < prefix_size; ++i) {
        if (path.parts[i] != parts[i]) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::string module_path_parts_name(const std::vector<std::string_view>& parts) {
    std::string name;
    for (base::usize i = 0; i < parts.size(); ++i) {
        if (i != 0) {
            name.push_back('.');
        }
        name += parts[i];
    }
    return name;
}

} // namespace

syntax::ModuleId SemanticAnalyzer::item_module(const syntax::ItemId item) const noexcept {
    if (!syntax::is_valid(item) || item.value >= this->module_.item_modules.size()) {
        return syntax::INVALID_MODULE_ID;
    }
    return this->module_.item_modules[item.value];
}

syntax::ModuleId SemanticAnalyzer::resolve_import_alias(
    const std::string_view alias,
    const base::SourceRange& range,
    const bool report_unknown
) const
{
    if (!syntax::is_valid(this->current_module_) || this->current_module_.value >= this->module_.modules.size()) {
        if (report_unknown) {
            this->report_lookup(range, sema_unknown_import_alias_message(alias));
        }
        return syntax::INVALID_MODULE_ID;
    }
    syntax::ModuleId resolved = syntax::INVALID_MODULE_ID;
    for (const syntax::ResolvedImport& import : this->module_.modules[this->current_module_.value].imports) {
        if (import.alias != alias) {
            continue;
        }
        if (syntax::is_valid(resolved)) {
            if (report_unknown) {
                this->report_lookup(range, sema_ambiguous_import_alias_message(alias));
            }
            return syntax::INVALID_MODULE_ID;
        }
        resolved = import.module;
    }
    if (!syntax::is_valid(resolved) && report_unknown) {
        this->report_lookup(range, sema_unknown_import_alias_message(alias));
        this->report_lookup_suggestion(range, this->nearest_import_alias_name(alias));
    }
    return resolved;
}

const SemanticAnalyzer::ModuleIdList& SemanticAnalyzer::visible_modules(const syntax::ModuleId module) const {
    static const ModuleIdList empty;
    if (!syntax::is_valid(module)) {
        return empty;
    }
    if (const auto found = this->visible_modules_cache_.find(module.value);
        found != this->visible_modules_cache_.end()) {
        return found->second;
    }
    if (module.value >= this->module_.modules.size()) {
        ModuleIdList result = this->make_module_id_list();
        result.reserve(1);
        result.push_back(module);
        const auto inserted = this->visible_modules_cache_.emplace(module.value, std::move(result));
        return inserted.first->second;
    }
    const base::usize import_count = this->module_.modules[module.value].imports.size();
    ModuleIdList result = this->make_module_id_list();
    result.reserve(import_count + 1);
    result.push_back(module);
    std::unordered_set<base::u32> seen;
    seen.reserve(import_count + 1);
    seen.insert(module.value);
    for (const syntax::ResolvedImport& import : this->module_.modules[module.value].imports) {
        if (!syntax::is_valid(import.module)) {
            continue;
        }
        if (seen.insert(import.module.value).second) {
            result.push_back(import.module);
        }
        append_public_reexports(import.module, result, seen);
    }
    const auto inserted = this->visible_modules_cache_.emplace(module.value, std::move(result));
    return inserted.first->second;
}

bool SemanticAnalyzer::module_alias_visible(const std::string_view name) const {
    if (!syntax::is_valid(this->current_module_) ||
        this->current_module_.value >= this->module_.modules.size()) {
        return false;
    }
    for (const syntax::ResolvedImport& import : this->module_.modules[this->current_module_.value].imports) {
        if (import.alias == name) {
            return true;
        }
    }
    return false;
}

bool SemanticAnalyzer::visible_root_module_name_exists(const std::string_view name) const {
    if (name.empty()) {
        return false;
    }
    for (const syntax::ModuleId module : this->visible_modules(this->current_module_)) {
        if (!syntax::is_valid(module) || module.value >= this->module_.modules.size()) {
            continue;
        }
        const syntax::ModulePath& path = this->module_.modules[module.value].path;
        if (!path.parts.empty() && path.parts.front() == name) {
            return true;
        }
    }
    return false;
}

std::vector<std::string_view> SemanticAnalyzer::type_scope_parts(const syntax::TypeNode& type) const {
    if (!type.scope_parts.empty()) {
        return type.scope_parts;
    }
    if (!type.scope_name.empty()) {
        return {type.scope_name};
    }
    return {};
}

syntax::ModuleId SemanticAnalyzer::find_visible_module_path(const std::vector<std::string_view>& parts) const {
    if (parts.empty()) {
        return syntax::INVALID_MODULE_ID;
    }
    for (const syntax::ModuleId module : this->visible_modules(this->current_module_)) {
        if (!syntax::is_valid(module) || module.value >= this->module_.modules.size()) {
            continue;
        }
        if (module_path_matches_parts(this->module_.modules[module.value].path, parts)) {
            return module;
        }
    }
    return syntax::INVALID_MODULE_ID;
}

bool SemanticAnalyzer::visible_module_path_prefix_exists(
    const std::vector<std::string_view>& parts
) const {
    if (parts.size() < 2) {
        return false;
    }
    for (base::usize prefix_size = 1; prefix_size < parts.size(); ++prefix_size) {
        for (const syntax::ModuleId module : this->visible_modules(this->current_module_)) {
            if (!syntax::is_valid(module) || module.value >= this->module_.modules.size()) {
                continue;
            }
            if (module_path_matches_prefix(this->module_.modules[module.value].path, parts, prefix_size)) {
                return true;
            }
        }
    }
    return false;
}

syntax::ModuleId SemanticAnalyzer::resolve_type_scope(
    const syntax::TypeNode& type,
    const bool report_unknown
) {
    const std::vector<std::string_view> parts = this->type_scope_parts(type);
    if (parts.empty()) {
        return syntax::INVALID_MODULE_ID;
    }
    if (parts.size() == 1) {
        return this->resolve_import_alias(parts.front(), type.scope_range, report_unknown);
    }

    const syntax::ModuleId module = this->find_visible_module_path(parts);
    if (!syntax::is_valid(module) && report_unknown) {
        this->report_lookup(type.scope_range, sema_unknown_module_path_message(module_path_parts_name(parts)));
    }
    return module;
}

const SemanticAnalyzer::ModuleIdList& SemanticAnalyzer::module_export_modules(const syntax::ModuleId module) const {
    static const ModuleIdList empty;
    if (!syntax::is_valid(module)) {
        return empty;
    }
    if (const auto found = this->module_export_modules_cache_.find(module.value);
        found != this->module_export_modules_cache_.end()) {
        return found->second;
    }
    if (module.value >= this->module_.modules.size()) {
        ModuleIdList result = this->make_module_id_list();
        result.reserve(1);
        result.push_back(module);
        const auto inserted = this->module_export_modules_cache_.emplace(module.value, std::move(result));
        return inserted.first->second;
    }
    const base::usize import_count = this->module_.modules[module.value].imports.size();
    ModuleIdList result = this->make_module_id_list();
    result.reserve(import_count + 1);
    result.push_back(module);
    std::unordered_set<base::u32> seen;
    seen.reserve(import_count + 1);
    seen.insert(module.value);
    this->append_public_reexports(module, result, seen);
    const auto inserted = this->module_export_modules_cache_.emplace(module.value, std::move(result));
    return inserted.first->second;
}

void SemanticAnalyzer::append_public_reexports(
    const syntax::ModuleId module,
    ModuleIdList& result,
    std::unordered_set<base::u32>& seen
) const {
    ModuleIdList pending = this->make_module_id_list();
    pending.reserve(result.size());
    pending.push_back(module);
    while (!pending.empty()) {
        const syntax::ModuleId current = pending.back();
        pending.pop_back();
        if (!syntax::is_valid(current) || current.value >= this->module_.modules.size()) {
            continue;
        }
        for (const syntax::ResolvedImport& import : this->module_.modules[current.value].imports) {
            if (import.visibility != syntax::Visibility::public_ || !syntax::is_valid(import.module)) {
                continue;
            }
            if (seen.insert(import.module.value).second) {
                result.push_back(import.module);
                pending.push_back(import.module);
            }
        }
    }
}

std::string SemanticAnalyzer::module_name(const syntax::ModuleId module) const {
    if (!syntax::is_valid(module) || module.value >= module_.modules.size()) {
        return std::string(SEMA_LOOKUP_UNKNOWN_MODULE_NAME);
    }
    return syntax::module_path_to_string(module_.modules[module.value].path);
}

std::string SemanticAnalyzer::qualified_name(const syntax::ModuleId module, const std::string_view name) const {
    const std::string module_text = module_name(module);
    if (module_text.empty() || module_text == SEMA_LOOKUP_UNKNOWN_MODULE_NAME) {
        return std::string(name);
    }
    return module_text + "." + std::string(name);
}

std::string_view SemanticAnalyzer::nearest_visible_value_name(const std::string_view name) const {
    NameSuggestion best;
    std::vector<std::string_view> local_names;
    this->symbols_.append_visible_names(local_names);
    for (const std::string_view candidate : local_names) {
        consider_suggestion(best, name, candidate);
    }
    if (syntax::is_valid(this->current_module_)) {
        for (const auto& entry : this->global_values_by_name_) {
            if (entry.first.module != this->current_module_.value) {
                continue;
            }
            const std::string_view candidate = this->module_.identifier_text(entry.first.name);
            consider_suggestion(best, name, candidate);
        }
    }
    return best.name;
}

std::string_view SemanticAnalyzer::nearest_value_name_in_module(
    const syntax::ModuleId module,
    const std::string_view name
) const {
    NameSuggestion best;
    if (!syntax::is_valid(module)) {
        return best.name;
    }
    for (const syntax::ModuleId candidate_module : this->module_export_modules(module)) {
        for (const auto& entry : this->global_values_by_name_) {
            if (entry.first.module != candidate_module.value || entry.second == nullptr) {
                continue;
            }
            if (!this->can_access(candidate_module, entry.second->visibility)) {
                continue;
            }
            const std::string_view candidate = this->module_.identifier_text(entry.first.name);
            consider_suggestion(best, name, candidate);
        }
    }
    return best.name;
}

std::string_view SemanticAnalyzer::nearest_visible_type_name(const std::string_view name) const {
    NameSuggestion best;
    if (!syntax::is_valid(this->current_module_)) {
        return best.name;
    }
    const auto consider_current_module_key = [&](const ModuleLookupKey key) {
        if (key.module == this->current_module_.value) {
            consider_suggestion(best, name, this->module_.identifier_text(key.name));
        }
    };
    for (const auto& entry : this->named_types_by_name_) {
        consider_current_module_key(entry.first);
    }
    for (const auto& entry : this->type_aliases_by_name_) {
        consider_current_module_key(entry.first);
    }
    for (const auto& entry : this->generic_struct_templates_by_name_) {
        consider_current_module_key(entry.first);
    }
    for (const auto& entry : this->generic_enum_templates_by_name_) {
        consider_current_module_key(entry.first);
    }
    for (const auto& entry : this->generic_type_alias_templates_by_name_) {
        consider_current_module_key(entry.first);
    }
    return best.name;
}

std::string_view SemanticAnalyzer::nearest_type_name_in_module(
    const syntax::ModuleId module,
    const std::string_view name
) const {
    NameSuggestion best;
    if (!syntax::is_valid(module)) {
        return best.name;
    }
    const auto consider_module_key = [&](const ModuleLookupKey key, const syntax::Visibility visibility) {
        const syntax::ModuleId owner {key.module};
        if (this->can_access(owner, visibility)) {
            consider_suggestion(best, name, this->module_.identifier_text(key.name));
        }
    };
    for (const syntax::ModuleId candidate_module : this->module_export_modules(module)) {
        for (const auto& entry : this->named_types_by_name_) {
            if (entry.first.module == candidate_module.value) {
                consider_module_key(entry.first, entry.second.visibility);
            }
        }
        for (const auto& entry : this->type_aliases_by_name_) {
            if (entry.first.module == candidate_module.value && entry.second != nullptr) {
                consider_module_key(entry.first, entry.second->visibility);
            }
        }
        for (const auto& entry : this->generic_struct_templates_by_name_) {
            if (entry.first.module == candidate_module.value && entry.second != nullptr) {
                consider_module_key(entry.first, entry.second->visibility);
            }
        }
        for (const auto& entry : this->generic_enum_templates_by_name_) {
            if (entry.first.module == candidate_module.value && entry.second != nullptr) {
                consider_module_key(entry.first, entry.second->visibility);
            }
        }
        for (const auto& entry : this->generic_type_alias_templates_by_name_) {
            if (entry.first.module == candidate_module.value && entry.second != nullptr) {
                consider_module_key(entry.first, entry.second->visibility);
            }
        }
    }
    return best.name;
}

std::string_view SemanticAnalyzer::nearest_visible_function_name(const std::string_view name) const {
    NameSuggestion best;
    if (!syntax::is_valid(this->current_module_)) {
        return best.name;
    }
    const auto consider_current_module_key = [&](const ModuleLookupKey key) {
        if (key.module == this->current_module_.value) {
            consider_suggestion(best, name, this->module_.identifier_text(key.name));
        }
    };
    for (const auto& entry : this->functions_by_name_) {
        consider_current_module_key(entry.first);
    }
    for (const auto& entry : this->generic_function_templates_by_name_) {
        consider_current_module_key(entry.first);
    }
    return best.name;
}

std::string_view SemanticAnalyzer::nearest_function_name_in_module(
    const syntax::ModuleId module,
    const std::string_view name
) const {
    NameSuggestion best;
    if (!syntax::is_valid(module)) {
        return best.name;
    }
    for (const syntax::ModuleId candidate_module : this->module_export_modules(module)) {
        for (const auto& entry : this->functions_by_name_) {
            if (entry.first.module != candidate_module.value || entry.second == nullptr) {
                continue;
            }
            if (!this->can_access(candidate_module, entry.second->visibility)) {
                continue;
            }
            consider_suggestion(best, name, this->module_.identifier_text(entry.first.name));
        }
        for (const auto& entry : this->generic_function_templates_by_name_) {
            if (entry.first.module != candidate_module.value || entry.second == nullptr) {
                continue;
            }
            if (!this->can_access(candidate_module, entry.second->visibility)) {
                continue;
            }
            consider_suggestion(best, name, this->module_.identifier_text(entry.first.name));
        }
    }
    return best.name;
}

std::string_view SemanticAnalyzer::nearest_import_alias_name(const std::string_view name) const {
    NameSuggestion best;
    if (!syntax::is_valid(this->current_module_) ||
        this->current_module_.value >= this->module_.modules.size()) {
        return best.name;
    }
    for (const syntax::ResolvedImport& import : this->module_.modules[this->current_module_.value].imports) {
        consider_suggestion(best, name, import.alias);
    }
    return best.name;
}

std::string_view SemanticAnalyzer::nearest_field_name(
    const StructInfo& info,
    const std::string_view name
) const {
    NameSuggestion best;
    for (const StructFieldInfo& field : info.fields) {
        if (!this->can_access(info.module, field.visibility)) {
            continue;
        }
        consider_suggestion(best, name, field.name);
    }
    return best.name;
}

std::string_view SemanticAnalyzer::nearest_enum_case_name(
    const TypeHandle enum_type,
    const std::string_view name
) const {
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

std::string_view SemanticAnalyzer::nearest_visible_enum_case_name(const std::string_view name) const {
    NameSuggestion best;
    if (!syntax::is_valid(this->current_module_)) {
        return best.name;
    }
    for (const auto& entry : this->enum_cases_by_module_name_) {
        const syntax::ModuleId owner {entry.first.module};
        if (entry.second == nullptr ||
            owner.value != this->current_module_.value ||
            !this->can_access(owner, entry.second->visibility)) {
            continue;
        }
        consider_suggestion(best, name, entry.second->case_name);
    }
    return best.name;
}

std::string SemanticAnalyzer::c_symbol_name(const syntax::ModuleId module, const std::string_view name) const {
    if (!syntax::is_valid(module) || module.value >= module_.modules.size()) {
        return std::string(name);
    }
    return syntax::mangle_c_symbol(module_.modules[module.value].path, name);
}

std::string SemanticAnalyzer::generic_template_key_prefix(
    const syntax::ModuleId module,
    const IdentId name_id,
    const std::string_view fallback_name
) const {
    const std::string_view name = this->module_.identifier_text(name_id);
    const std::string_view key_name = name.empty() ? fallback_name : name;
    std::string key = std::to_string(module.value);
    key.reserve(key.size() + 1 + key_name.size());
    key.push_back(':');
    key += key_name;
    return key;
}

FunctionLookupKey SemanticAnalyzer::function_key(
    const syntax::ItemNode& function,
    const syntax::ItemId function_id
) const {
    const syntax::ModuleId module = this->item_module(function_id);
    if (this->has_generic_params(function)) {
        return this->function_lookup_key(module, function.name_id);
    }
    if (!syntax::is_valid(function.impl_type)) {
        return this->function_lookup_key(module, function.name_id);
    }
    const TypeHandle owner_type =
        function.impl_type.value < this->checked_.syntax_type_handles.size()
            ? this->checked_.syntax_type_handles[function.impl_type.value]
            : INVALID_TYPE_HANDLE;
    return this->method_function_lookup_key(module, owner_type, function.name_id);
}

ModuleLookupKey SemanticAnalyzer::module_lookup_key(
    const syntax::ModuleId module,
    const IdentId name
) const noexcept {
    return ModuleLookupKey {
        module.value,
        name,
    };
}

MethodLookupKey SemanticAnalyzer::method_lookup_key(
    const syntax::ModuleId module,
    const TypeHandle owner_type,
    const IdentId name
) const noexcept {
    return MethodLookupKey {
        module.value,
        owner_type.value,
        name,
    };
}

FunctionLookupKey SemanticAnalyzer::function_lookup_key(
    const syntax::ModuleId module,
    const IdentId name
) const noexcept {
    return FunctionLookupKey {
        module.value,
        SEMA_LOOKUP_INVALID_KEY_PART,
        name,
    };
}

FunctionLookupKey SemanticAnalyzer::method_function_lookup_key(
    const syntax::ModuleId module,
    const TypeHandle owner_type,
    const IdentId name
) const noexcept {
    return FunctionLookupKey {
        module.value,
        owner_type.value,
        name,
    };
}

FunctionLookupKey SemanticAnalyzer::function_lookup_key_from_method(
    const MethodLookupKey key
) const noexcept {
    return FunctionLookupKey {
        key.module,
        key.owner_type,
        key.name,
    };
}

StableModuleId SemanticAnalyzer::stable_module_id(const syntax::ModuleId module) const noexcept {
    if (!syntax::is_valid(module) || module.value >= this->module_.modules.size()) {
        return sema::stable_module_id(std::span<const std::string_view> {});
    }
    return sema::stable_module_id(this->module_.modules[module.value].path.parts);
}

StableDefId SemanticAnalyzer::stable_definition_id(
    const syntax::ModuleId module,
    const StableSymbolKind kind,
    const IdentId name_id,
    const std::string_view fallback_name,
    const base::u32 disambiguator
) const {
    const std::string_view name = this->module_.identifier_text(name_id).empty()
        ? fallback_name
        : this->module_.identifier_text(name_id);
    return sema::stable_definition_id(
        this->stable_module_id(module),
        kind,
        name,
        disambiguator
    );
}

StableMemberKey SemanticAnalyzer::stable_member_key(
    const StableDefId& owner,
    const StableSymbolKind kind,
    const IdentId name_id,
    const std::string_view fallback_name,
    const base::u32 disambiguator
) const {
    const std::string_view name = this->module_.identifier_text(name_id).empty()
        ? fallback_name
        : this->module_.identifier_text(name_id);
    return sema::stable_member_key(owner, kind, name, disambiguator);
}

IncrementalKey SemanticAnalyzer::stable_incremental_key(
    const StableDefId& definition,
    const std::string_view semantic_fingerprint
) const {
    return sema::stable_incremental_key(definition, semantic_fingerprint);
}

std::string SemanticAnalyzer::function_incremental_fingerprint(
    const std::string_view name,
    const TypeHandle return_type,
    const std::span<const TypeHandle> param_types,
    const bool is_method,
    const bool is_variadic
) const {
    std::string fingerprint;
    fingerprint.reserve(name.size() + (param_types.size() + 1U) * SEMA_INCREMENTAL_FINGERPRINT_TYPE_TEXT_BUDGET);
    fingerprint += name;
    fingerprint.push_back('|');
    fingerprint += is_method
        ? SEMA_INCREMENTAL_FINGERPRINT_METHOD_TAG
        : SEMA_INCREMENTAL_FINGERPRINT_FUNCTION_TAG;
    fingerprint.push_back('|');
    fingerprint += is_variadic
        ? SEMA_INCREMENTAL_FINGERPRINT_VARIADIC_TAG
        : SEMA_INCREMENTAL_FINGERPRINT_FIXED_TAG;
    fingerprint.push_back('|');
    fingerprint += std::to_string(return_type.value);
    for (const TypeHandle param_type : param_types) {
        fingerprint.push_back(',');
        fingerprint += std::to_string(param_type.value);
    }
    return fingerprint;
}

InternedText SemanticAnalyzer::source_name_text(
    const IdentId name_id,
    const std::string_view fallback_name
) {
    if (!this->owned_module_.has_value() && is_valid(name_id)) {
        if (!this->module_.identifier_text(name_id).empty()) {
            return InternedText {name_id, &this->module_.identifiers};
        }
    }
    return this->checked_.intern_text(fallback_name);
}

IdentId SemanticAnalyzer::intern_generated_key(const std::string_view key) {
    return this->module_.intern_identifier(key);
}

ModuleLookupKey SemanticAnalyzer::intern_module_lookup_key(
    const syntax::ModuleId module,
    const IdentId name
) const noexcept {
    return this->module_lookup_key(module, name);
}

ModuleLookupKey SemanticAnalyzer::find_module_lookup_key(
    const syntax::ModuleId module,
    const IdentId name
) const noexcept {
    return this->module_lookup_key(module, name);
}

MethodLookupKey SemanticAnalyzer::intern_method_lookup_key(
    const syntax::ModuleId module,
    const TypeHandle owner_type,
    const IdentId name
) const noexcept {
    return this->method_lookup_key(module, owner_type, name);
}

MethodLookupKey SemanticAnalyzer::find_method_lookup_key(
    const syntax::ModuleId module,
    const TypeHandle owner_type,
    const IdentId name
) const noexcept {
    return this->method_lookup_key(module, owner_type, name);
}

void SemanticAnalyzer::index_named_type(
    const syntax::ModuleId module,
    const IdentId name_id,
    const TypeHandle type,
    const syntax::Visibility visibility
) {
    const ModuleLookupKey key = this->intern_module_lookup_key(module, name_id);
    this->named_types_by_name_[key] = IndexedTypeInfo {type, visibility};
}

void SemanticAnalyzer::index_type_alias(const TypeAliasInfo& info) {
    const ModuleLookupKey key = this->intern_module_lookup_key(info.module, info.name_id);
    this->type_aliases_by_name_[key] = &info;
}

void SemanticAnalyzer::index_generic_struct_template(const GenericTemplateInfo& info) {
    const ModuleLookupKey key = this->intern_module_lookup_key(info.module, info.name_id);
    this->generic_struct_templates_by_name_[key] = &info;
}

void SemanticAnalyzer::index_generic_enum_template(const GenericTemplateInfo& info) {
    const ModuleLookupKey key = this->intern_module_lookup_key(info.module, info.name_id);
    this->generic_enum_templates_by_name_[key] = &info;
}

void SemanticAnalyzer::index_generic_type_alias_template(const GenericTemplateInfo& info) {
    const ModuleLookupKey key = this->intern_module_lookup_key(info.module, info.name_id);
    this->generic_type_alias_templates_by_name_[key] = &info;
}

void SemanticAnalyzer::index_generic_function_template(const GenericTemplateInfo& info) {
    const ModuleLookupKey key = this->intern_module_lookup_key(info.module, info.name_id);
    this->generic_function_templates_by_name_[key] = &info;
}

void SemanticAnalyzer::index_generic_method_template(const GenericTemplateInfo& info) {
    const ModuleLookupKey key = this->intern_module_lookup_key(info.module, info.name_id);
    this->generic_method_template_bucket(key).push_back(&info);
    this->generic_method_lookup_indexed_count_ += 1;
}

void SemanticAnalyzer::index_function_lookup(const FunctionSignature& signature) {
    if (signature.is_method) {
        this->index_method_lookup(signature.module, signature.method_owner_type, signature.name_id, signature);
        return;
    }
    const ModuleLookupKey key = this->intern_module_lookup_key(signature.module, signature.name_id);
    this->functions_by_name_[key] = &signature;
}

void SemanticAnalyzer::index_method_lookup(
    const syntax::ModuleId module,
    const TypeHandle owner_type,
    const IdentId name_id,
    const FunctionSignature& signature
) {
    const MethodLookupKey key = this->intern_method_lookup_key(module, owner_type, name_id);
    this->methods_by_name_[key] = &signature;
}

void SemanticAnalyzer::index_function_value(const FunctionSignature& signature) {
    if (signature.is_method) {
        const auto found = this->global_values_.find(signature.semantic_key);
        if (found != this->global_values_.end()) {
            const MethodLookupKey key = this->intern_method_lookup_key(signature.module, signature.method_owner_type, signature.name_id);
            this->method_global_values_by_name_[key] = &found->second;
        }
        return;
    }
    const auto found = this->global_values_.find(signature.semantic_key);
    if (found != this->global_values_.end()) {
        this->index_global_value(found->second);
    }
}

void SemanticAnalyzer::index_global_value(const Symbol& symbol) {
    const ModuleLookupKey key = this->intern_module_lookup_key(symbol.module, symbol.name_id);
    this->global_values_by_name_[key] = &symbol;
}

bool SemanticAnalyzer::named_type_lookup_complete() const noexcept {
    return this->named_types_by_name_.size() == this->named_types_.size();
}

bool SemanticAnalyzer::type_alias_lookup_complete() const noexcept {
    return this->type_aliases_by_name_.size() == this->checked_.type_aliases.size();
}

bool SemanticAnalyzer::generic_struct_lookup_complete() const noexcept {
    return this->generic_struct_templates_by_name_.size() == this->generic_struct_templates_.size();
}

bool SemanticAnalyzer::generic_enum_lookup_complete() const noexcept {
    return this->generic_enum_templates_by_name_.size() == this->generic_enum_templates_.size();
}

bool SemanticAnalyzer::generic_type_alias_lookup_complete() const noexcept {
    return this->generic_type_alias_templates_by_name_.size() == this->generic_type_alias_templates_.size();
}

bool SemanticAnalyzer::generic_function_lookup_complete() const noexcept {
    return this->generic_function_templates_by_name_.size() == this->generic_function_templates_.size();
}

bool SemanticAnalyzer::generic_method_lookup_complete() const noexcept {
    return this->generic_method_lookup_indexed_count_ == this->generic_method_templates_.size();
}

bool SemanticAnalyzer::function_lookup_complete() const noexcept {
    return this->functions_by_name_.size() +
           this->methods_by_name_.size() +
           this->internal_function_lookup_exclusions_ == this->checked_.functions.size();
}

bool SemanticAnalyzer::global_value_lookup_complete() const noexcept {
    return this->global_values_by_name_.size() +
           this->method_global_values_by_name_.size() == this->global_values_.size();
}

bool SemanticAnalyzer::enum_case_module_lookup_complete() const noexcept {
    return this->enum_cases_by_module_name_.size() == this->checked_.enum_cases.size();
}

bool SemanticAnalyzer::top_level_value_name_exists(
    const syntax::ModuleId module,
    const IdentId name_id,
    const std::string_view name
) const {
    static_cast<void>(name);
    const ModuleLookupKey lookup_key = this->find_module_lookup_key(module, name_id);
    if (is_valid(lookup_key)) {
        if (const auto found = this->global_values_by_name_.find(lookup_key);
            found != this->global_values_by_name_.end() && found->second != nullptr) {
            return found->second->kind == SymbolKind::function || found->second->kind == SymbolKind::const_;
        }
    }
    return false;
}

bool SemanticAnalyzer::module_type_or_value_name_exists(
    const syntax::ModuleId module,
    const IdentId name_id,
    const std::string_view name
) const {
    const ModuleLookupKey lookup_key = this->find_module_lookup_key(module, name_id);
    const bool typed_type_found = is_valid(lookup_key) &&
        (this->named_types_by_name_.contains(lookup_key) ||
         this->type_aliases_by_name_.contains(lookup_key));
    if (typed_type_found ||
        this->find_any_generic_type_template_in_module(module, name_id, name) != nullptr ||
        this->top_level_value_name_exists(module, name_id, name) ||
        (is_valid(lookup_key) && this->generic_function_templates_by_name_.contains(lookup_key))) {
        return true;
    }
    return false;
}

bool SemanticAnalyzer::current_generic_param_exists(
    const IdentId name_id,
    const std::string_view
) const {
    return this->current_generic_context_ != nullptr &&
           this->current_generic_context_->params.contains(name_id);
}

bool SemanticAnalyzer::visible_type_name_exists(
    const IdentId name_id,
    const std::string_view name
) const {
    if (this->current_generic_param_exists(name_id, name)) {
        return true;
    }
    const auto type_visible_in_module = [&](const syntax::ModuleId module) {
        if (!syntax::is_valid(module)) {
            return false;
        }
        const ModuleLookupKey lookup_key = this->find_module_lookup_key(module, name_id);
        if (is_valid(lookup_key)) {
            if (const auto found = this->named_types_by_name_.find(lookup_key);
                found != this->named_types_by_name_.end()) {
                return module.value == this->current_module_.value ||
                       this->can_access(module, found->second.visibility);
            }
            if (const auto alias = this->type_aliases_by_name_.find(lookup_key);
                alias != this->type_aliases_by_name_.end() && alias->second != nullptr) {
                return this->can_access(module, alias->second->visibility);
            }
        }
        const GenericTemplateInfo* const generic = this->find_any_generic_type_template_in_module(module, name_id, name);
        return generic != nullptr && this->can_access(module, generic->visibility);
    };
    if (type_visible_in_module(this->current_module_)) {
        return true;
    }
    if (!syntax::is_valid(this->current_module_) || this->current_module_.value >= this->module_.modules.size()) {
        return false;
    }
    for (const syntax::ResolvedImport& import : this->module_.modules[this->current_module_.value].imports) {
        if (type_visible_in_module(import.module)) {
            return true;
        }
    }
    return false;
}

bool SemanticAnalyzer::can_define_local_name(
    const IdentId name_id,
    const std::string_view name,
    const base::SourceRange& range
) const
{
    if (name.empty()) {
        return true;
    }
    if (this->module_alias_visible(name)) {
        this->report_duplicate(range, sema_local_shadows_import_alias_message(name));
        return false;
    }
    if (this->visible_root_module_name_exists(name)) {
        this->report_duplicate(range, sema_local_shadows_root_module_message(name));
        return false;
    }
    if (this->current_generic_param_exists(name_id, name)) {
        this->report_duplicate(range, sema_local_shadows_generic_type_parameter_message(name));
        return false;
    }
    if (this->visible_type_name_exists(name_id, name)) {
        this->report_duplicate(range, sema_local_shadows_type_name_message(name));
        return false;
    }
    return true;
}

bool SemanticAnalyzer::type_member_name_exists(
    const TypeHandle owner_type,
    const IdentId name_id,
    const std::string_view name
) const {
    if (!is_valid(owner_type)) {
        return false;
    }
    if (const auto found = this->enum_cases_by_type_.find(owner_type.value);
        found != this->enum_cases_by_type_.end()) {
        for (const EnumCaseInfo* enum_case : found->second) {
            if (enum_case != nullptr && enum_case->case_name_id == name_id) {
                return true;
            }
        }
    }
    for (const auto& entry : this->checked_.functions) {
        const FunctionSignature& signature = entry.second;
        if (signature.is_method &&
            this->checked_.types.same(signature.method_owner_type, owner_type) &&
            signature.name_id == name_id) {
            return true;
        }
    }
    for (const auto& entry : this->generic_method_templates_) {
        const GenericTemplateInfo& info = entry.second;
        if (info.name_id == name_id &&
            is_valid(info.impl_type_pattern) &&
            this->checked_.types.same(info.impl_type_pattern, owner_type)) {
            return true;
        }
    }
    static_cast<void>(name);
    return false;
}

std::string SemanticAnalyzer::method_c_symbol_name(
    const TypeHandle owner_type,
    const std::string_view name
) const {
    return this->checked_.types.c_name(owner_type) + "_" + std::string(name);
}

bool SemanticAnalyzer::can_access(const syntax::ModuleId owner, const syntax::Visibility visibility) const noexcept {
    return owner.value == this->current_module_.value || visibility == syntax::Visibility::public_;
}

syntax::ModuleId SemanticAnalyzer::owner_module(const TypeHandle owner_type) const noexcept {
    if (!is_valid(owner_type)) {
        return syntax::INVALID_MODULE_ID;
    }
    if (const StructInfo* info = this->find_struct(owner_type); info != nullptr) {
        return info->module;
    }
    if (const auto found = this->enum_cases_by_type_.find(owner_type.value);
        found != this->enum_cases_by_type_.end() &&
        !found->second.empty()) {
        return found->second.front()->module;
    }
    return syntax::INVALID_MODULE_ID;
}

const FunctionSignature* SemanticAnalyzer::find_method_in_owner_module(
    const TypeHandle owner_type,
    const IdentId name_id,
    const std::string_view name,
    const bool require_self
) const {
    static_cast<void>(name);
    const syntax::ModuleId owner = this->owner_module(owner_type);
    if (!syntax::is_valid(owner)) {
        return nullptr;
    }
    const FunctionSignature* signature = nullptr;
    const MethodLookupKey lookup_key = this->find_method_lookup_key(owner, owner_type, name_id);
    if (is_valid(lookup_key)) {
        if (const auto found = this->methods_by_name_.find(lookup_key);
            found != this->methods_by_name_.end()) {
            signature = found->second;
        }
    }
    if (signature == nullptr ||
        !signature->is_method ||
        (require_self && !signature->has_self_param)) {
        return nullptr;
    }
    if (!this->can_access(owner, signature->visibility)) {
        return nullptr;
    }
    return signature;
}

bool SemanticAnalyzer::method_receiver_matches(
    const FunctionSignature& signature,
    const TypeHandle receiver_type,
    const syntax::ExprId receiver
) {
    if (!signature.has_self_param || signature.param_types.empty()) {
        return false;
    }
    const TypeHandle self_type = signature.param_types.front();
    if (this->checked_.types.same(self_type, receiver_type)) {
        if (!this->check_m2_value_abi(
                self_type,
                ValueAbiContext::argument,
                this->module_.exprs.range(receiver.value))) {
            return false;
        }
        return true;
    }
    if (!this->checked_.types.is_pointer(self_type) && !this->checked_.types.is_reference(self_type)) {
        return false;
    }
    const TypeHandle pointee = this->checked_.types.get(self_type).pointee;
    if (this->checked_.types.is_pointer(receiver_type) || this->checked_.types.is_reference(receiver_type)) {
        const TypeInfo& self_info = this->checked_.types.get(self_type);
        const TypeInfo& receiver_info = this->checked_.types.get(receiver_type);
        if (self_info.kind != receiver_info.kind) {
            return false;
        }
        if (!this->checked_.types.same(self_info.pointee, receiver_info.pointee)) {
            return false;
        }
        if (self_info.pointer_mutability == PointerMutability::mut &&
            receiver_info.pointer_mutability != PointerMutability::mut) {
            this->report_general(
                this->module_.exprs.range(receiver.value),
                std::string(SEMA_MUTABLE_METHOD_RECEIVER_POINTER)
            );
            return false;
        }
        return true;
    }
    if (!this->checked_.types.same(pointee, receiver_type)) {
        return false;
    }
    const PointerMutability self_mutability = this->checked_.types.get(self_type).pointer_mutability;
    if (self_mutability == PointerMutability::mut) {
        if (!this->is_place_expr(receiver)) {
            this->report_general(
                this->module_.exprs.range(receiver.value),
                std::string(SEMA_METHOD_RECEIVER_PLACE)
            );
            return false;
        }
        if (!this->is_writable_place(receiver)) {
            this->report_general(
                this->module_.exprs.range(receiver.value),
                std::string(SEMA_MUTABLE_METHOD_RECEIVER_WRITABLE)
            );
            return false;
        }
    }
    return true;
}

const FunctionSignature* SemanticAnalyzer::find_method_in_visible_modules(
    const TypeHandle owner_type,
    const IdentId name_id,
    const std::string_view name,
    const base::SourceRange& range,
    const bool require_self,
    const bool report_unknown
) {
    const FunctionSignature* result = nullptr;
    const FunctionSignature* inaccessible_result = nullptr;
    syntax::ModuleId result_module = syntax::INVALID_MODULE_ID;
    const std::array<syntax::ModuleId, 2> modules {this->current_module_, this->owner_module(owner_type)};
    std::unordered_set<base::u32> seen_modules;
    for (const syntax::ModuleId module : modules) {
        if (!syntax::is_valid(module) || !seen_modules.insert(module.value).second) {
            continue;
        }
        const FunctionSignature* signature = nullptr;
        const MethodLookupKey lookup_key = this->find_method_lookup_key(module, owner_type, name_id);
        if (is_valid(lookup_key)) {
            if (const auto found = this->methods_by_name_.find(lookup_key);
                found != this->methods_by_name_.end()) {
                signature = found->second;
            }
        }
        if (signature == nullptr ||
            !signature->is_method ||
            (require_self && !signature->has_self_param)) {
            continue;
        }
        if (!this->can_access(module, signature->visibility)) {
            if (inaccessible_result == nullptr) {
                inaccessible_result = signature;
            }
            continue;
        }
        if (result != nullptr) {
            this->report_lookup(
                range,
                sema_ambiguous_method_message(
                    this->checked_.types.display_name(owner_type),
                    name,
                    this->module_name(result_module),
                    this->module_name(module)
                )
            );
            return nullptr;
        }
        result = signature;
        result_module = module;
    }
    if (result == nullptr && report_unknown) {
        if (inaccessible_result != nullptr) {
            this->report_visibility(
                range,
                sema_private_method_message(this->checked_.types.display_name(owner_type), name)
            );
            return nullptr;
        }
        this->report_lookup(range, sema_unknown_method_message(this->checked_.types.display_name(owner_type), name));
    }
    return result;
}

TypeHandle SemanticAnalyzer::find_type_in_visible_modules(
    const IdentId name_id,
    const std::string_view name,
    const base::SourceRange& range,
    const bool opaque_allowed_as_pointee,
    const bool report_unknown
) {
    const ModuleLookupKey lookup_key = this->find_module_lookup_key(this->current_module_, name_id);
    if (is_valid(lookup_key)) {
        if (const auto found = this->named_types_by_name_.find(lookup_key);
            found != this->named_types_by_name_.end()) {
            return found->second.type;
        }
        if (const auto found = this->type_aliases_by_name_.find(lookup_key);
            found != this->type_aliases_by_name_.end() && found->second != nullptr) {
            return this->resolve_type_alias(*found->second, opaque_allowed_as_pointee);
        }
    }

    if (report_unknown) {
        this->report_lookup(range, sema_unknown_type_message(name));
        this->report_lookup_suggestion(range, this->nearest_visible_type_name(name));
    }
    return INVALID_TYPE_HANDLE;
}

TypeHandle SemanticAnalyzer::find_type_in_module(
    const syntax::ModuleId module,
    const IdentId name_id,
    const std::string_view name,
    const base::SourceRange& range,
    const bool opaque_allowed_as_pointee,
    const bool report_unknown
) {
    if (!syntax::is_valid(module)) {
        if (report_unknown) {
            this->report_lookup(range, sema_unknown_type_message(name));
            this->report_lookup_suggestion(range, this->nearest_visible_type_name(name));
        }
        return INVALID_TYPE_HANDLE;
    }

    TypeHandle result = INVALID_TYPE_HANDLE;
    syntax::ModuleId result_module = syntax::INVALID_MODULE_ID;
    for (const syntax::ModuleId candidate_module : this->module_export_modules(module)) {
        TypeHandle candidate = INVALID_TYPE_HANDLE;
        const ModuleLookupKey lookup_key = this->find_module_lookup_key(candidate_module, name_id);
        if (is_valid(lookup_key)) {
            if (const auto found = this->named_types_by_name_.find(lookup_key);
                found != this->named_types_by_name_.end()) {
                if (!this->can_access(candidate_module, found->second.visibility)) {
                    if (candidate_module.value == module.value && report_unknown) {
                        this->report_visibility(
                            range,
                            sema_private_type_message(this->module_name(candidate_module), name)
                        );
                        return INVALID_TYPE_HANDLE;
                    }
                    continue;
                }
                candidate = found->second.type;
            } else if (const auto alias_found = this->type_aliases_by_name_.find(lookup_key);
                       alias_found != this->type_aliases_by_name_.end() &&
                       alias_found->second != nullptr) {
                if (!this->can_access(candidate_module, alias_found->second->visibility)) {
                    if (candidate_module.value == module.value && report_unknown) {
                        this->report_visibility(
                            range,
                            sema_private_type_message(this->module_name(candidate_module), name)
                        );
                        return INVALID_TYPE_HANDLE;
                    }
                    continue;
                }
                candidate = this->resolve_type_alias(*alias_found->second, opaque_allowed_as_pointee);
            }
        }
        if (!is_valid(candidate)) {
            continue;
        }
        if (is_valid(result)) {
            if (report_unknown) {
                this->report_lookup(
                    range,
                    sema_ambiguous_type_name_message(name, this->module_name(result_module), this->module_name(candidate_module))
                );
            }
            return INVALID_TYPE_HANDLE;
        }
        result = candidate;
        result_module = candidate_module;
    }
    if (is_valid(result)) {
        return result;
    }

    if (report_unknown) {
        this->report_lookup(range, sema_unknown_type_in_module_message(this->module_name(module), name));
        this->report_lookup_suggestion(range, this->nearest_type_name_in_module(module, name));
    }
    return INVALID_TYPE_HANDLE;
}

const FunctionSignature* SemanticAnalyzer::find_function_in_visible_modules(
    const IdentId name_id,
    const std::string_view name,
    const base::SourceRange& range,
    const bool report_unknown
) {
    const ModuleLookupKey lookup_key = this->find_module_lookup_key(this->current_module_, name_id);
    if (is_valid(lookup_key)) {
        if (const auto found = this->functions_by_name_.find(lookup_key);
            found != this->functions_by_name_.end()) {
            return found->second;
        }
    }

    if (report_unknown) {
        this->report_lookup(range, sema_unknown_function_message(name));
        this->report_lookup_suggestion(range, this->nearest_visible_function_name(name));
    }
    return nullptr;
}

const FunctionSignature* SemanticAnalyzer::find_function_in_module(
    const syntax::ModuleId module,
    const IdentId name_id,
    const std::string_view name,
    const base::SourceRange& range,
    const bool report_unknown
) {
    if (!syntax::is_valid(module)) {
        if (report_unknown) {
            this->report_lookup(range, sema_unknown_function_in_module_message(this->module_name(module), name));
            this->report_lookup_suggestion(range, this->nearest_visible_function_name(name));
        }
        return nullptr;
    }

    const FunctionSignature* result = nullptr;
    syntax::ModuleId result_module = syntax::INVALID_MODULE_ID;
    for (const syntax::ModuleId candidate_module : this->module_export_modules(module)) {
        const FunctionSignature* candidate = nullptr;
        const ModuleLookupKey lookup_key = this->find_module_lookup_key(candidate_module, name_id);
        if (is_valid(lookup_key)) {
            if (const auto found = this->functions_by_name_.find(lookup_key);
                found != this->functions_by_name_.end()) {
                candidate = found->second;
            }
        }
        if (candidate == nullptr) {
            continue;
        }
        if (!this->can_access(candidate_module, candidate->visibility)) {
            if (candidate_module.value == module.value && report_unknown) {
                this->report_visibility(
                    range,
                    sema_private_function_message(this->module_name(candidate_module), name)
                );
                return nullptr;
            }
            continue;
        }
        if (result != nullptr) {
            if (report_unknown) {
                this->report_lookup(
                    range,
                    sema_ambiguous_function_name_message(
                        name,
                        this->module_name(result_module),
                        this->module_name(candidate_module)
                    )
                );
            }
            return nullptr;
        }
        result = candidate;
        result_module = candidate_module;
    }
    if (result == nullptr && report_unknown) {
        this->report_lookup(range, sema_unknown_function_in_module_message(this->module_name(module), name));
        this->report_lookup_suggestion(range, this->nearest_function_name_in_module(module, name));
    }
    return result;
}

const Symbol* SemanticAnalyzer::find_symbol_in_module(
    const syntax::ModuleId module,
    const IdentId name_id,
    const std::string_view name,
    const base::SourceRange& range,
    const bool report_unknown
) {
    if (!syntax::is_valid(module)) {
        if (report_unknown) {
            this->report_lookup(range, sema_unknown_name_in_module_message(this->module_name(module), name));
            this->report_lookup_suggestion(range, this->nearest_visible_value_name(name));
        }
        return nullptr;
    }

    const Symbol* result = nullptr;
    syntax::ModuleId result_module = syntax::INVALID_MODULE_ID;
    for (const syntax::ModuleId candidate_module : this->module_export_modules(module)) {
        const Symbol* candidate = nullptr;
        const ModuleLookupKey lookup_key = this->find_module_lookup_key(candidate_module, name_id);
        if (is_valid(lookup_key)) {
            if (const auto found = this->global_values_by_name_.find(lookup_key);
                found != this->global_values_by_name_.end()) {
                candidate = found->second;
            }
        }
        if (candidate == nullptr) {
            continue;
        }
        if (!this->can_access(candidate_module, candidate->visibility)) {
            if (candidate_module.value == module.value && report_unknown) {
                this->report_visibility(range, sema_private_name_message(this->module_name(candidate_module), name));
                return nullptr;
            }
            continue;
        }
        if (result != nullptr) {
            if (report_unknown) {
                this->report_lookup(
                    range,
                    sema_ambiguous_name_message(
                        name,
                        this->module_name(result_module),
                        this->module_name(candidate_module)
                    )
                );
            }
            return nullptr;
        }
        result = candidate;
        result_module = candidate_module;
    }
    if (result == nullptr && report_unknown) {
        this->report_lookup(range, sema_unknown_name_in_module_message(this->module_name(module), name));
        this->report_lookup_suggestion(range, this->nearest_value_name_in_module(module, name));
    }
    return result;
}

const EnumCaseInfo* SemanticAnalyzer::find_enum_case_in_visible_modules(
    const IdentId name_id,
    const std::string_view name,
    const base::SourceRange& range,
    const bool report_unknown
) {
    const ModuleLookupKey lookup_key = this->find_module_lookup_key(this->current_module_, name_id);
    if (is_valid(lookup_key)) {
        if (const auto found = this->enum_cases_by_module_name_.find(lookup_key);
            found != this->enum_cases_by_module_name_.end()) {
            return found->second;
        }
    }

    if (report_unknown) {
        this->report_lookup(range, sema_unknown_enum_case_message(name));
        this->report_lookup_suggestion(range, this->nearest_visible_enum_case_name(name));
    }
    return nullptr;
}

const EnumCaseInfo* SemanticAnalyzer::find_enum_case_by_type_and_case(
    const TypeHandle enum_type,
    const IdentId case_name_id,
    const std::string_view case_name
) const {
    if (!is_valid(enum_type)) {
        return nullptr;
    }
    if (!is_valid(case_name_id)) {
        return nullptr;
    }
    const auto found = this->enum_cases_by_type_and_case_.find(EnumCaseLookupKey {
        enum_type.value,
        case_name_id,
    });
    if (found == this->enum_cases_by_type_and_case_.end() ||
        found->second == nullptr ||
        !this->checked_.types.same(found->second->type, enum_type) ||
        found->second->case_name != case_name) {
        return nullptr;
    }
    return found->second;
}

const SemanticAnalyzer::EnumCaseList* SemanticAnalyzer::find_enum_cases_by_type(
    const TypeHandle enum_type
) const noexcept {
    if (!is_valid(enum_type)) {
        return nullptr;
    }
    const auto found = this->enum_cases_by_type_.find(enum_type.value);
    return found == this->enum_cases_by_type_.end() ? nullptr : &found->second;
}

void SemanticAnalyzer::index_enum_case(const EnumCaseInfo& info) {
    if (!is_valid(info.type)) {
        return;
    }
    this->enum_cases_by_type_and_case_.emplace(EnumCaseLookupKey {
        info.type.value,
        info.case_name_id,
    }, &info);
    const ModuleLookupKey module_key = this->intern_module_lookup_key(info.module, info.name_id);
    if (is_valid(module_key)) {
        this->enum_cases_by_module_name_[module_key] = &info;
    }
    this->enum_case_type_bucket(info.type).push_back(&info);
}

const EnumCaseInfo* SemanticAnalyzer::find_enum_case_by_scoped_name(
    const IdentId enum_name_id,
    const std::string_view enum_name,
    const IdentId case_name_id,
    const std::string_view case_name,
    const base::SourceRange& range,
    const bool report_unknown
) {
    const ModuleLookupKey enum_lookup_key = this->find_module_lookup_key(this->current_module_, enum_name_id);
    const bool enum_type_name_exists = is_valid(enum_lookup_key) &&
        (this->named_types_by_name_.contains(enum_lookup_key) ||
         this->type_aliases_by_name_.contains(enum_lookup_key));
    if (!report_unknown &&
        !enum_type_name_exists &&
        this->named_type_lookup_complete() &&
        this->type_alias_lookup_complete()) {
        return nullptr;
    }
    const TypeHandle enum_type = this->find_type_in_visible_modules(enum_name_id, enum_name, range, false);
    if (!is_valid(enum_type) || this->checked_.types.get(enum_type).kind != TypeKind::enum_) {
        if (is_valid(enum_type) && report_unknown) {
            this->report_general(range, std::string(SEMA_ENUM_CASE_SCOPE_TYPE));
        }
        return nullptr;
    }
    if (const EnumCaseInfo* result = this->find_enum_case_by_type_and_case(enum_type, case_name_id, case_name);
        result != nullptr) {
        return result;
    }
    if (report_unknown) {
        this->report_lookup(range, sema_unknown_scoped_enum_case_message(enum_name, case_name));
        this->report_lookup_suggestion(range, this->nearest_enum_case_name(enum_type, case_name));
    }
    return nullptr;
}

const EnumCaseInfo* SemanticAnalyzer::find_enum_case_by_pattern_type(
    const syntax::TypeId enum_type_id,
    const IdentId case_name_id,
    const std::string_view case_name,
    const base::SourceRange& range
) {
    const TypeHandle enum_type = this->resolve_type(enum_type_id);
    if (!is_valid(enum_type)) {
        return nullptr;
    }
    if (this->checked_.types.get(enum_type).kind != TypeKind::enum_) {
        this->report_general(range, std::string(SEMA_ENUM_CASE_SCOPE_TYPE));
        return nullptr;
    }
    if (const EnumCaseInfo* result = this->find_enum_case_by_type_and_case(enum_type, case_name_id, case_name);
        result != nullptr) {
        return result;
    }
    this->report_lookup(
        range,
        sema_unknown_scoped_enum_case_message(this->checked_.types.display_name(enum_type), case_name)
    );
    this->report_lookup_suggestion(range, this->nearest_enum_case_name(enum_type, case_name));
    return nullptr;
}

const EnumCaseInfo* SemanticAnalyzer::find_enum_constructor(const syntax::ExprId callee_id, const bool report_unknown) {
    if (!syntax::is_valid(callee_id) || callee_id.value >= this->module_.exprs.size()) {
        return nullptr;
    }
    const syntax::FieldExprPayload* const callee = this->module_.exprs.field_payload(callee_id.value);
    if (callee == nullptr ||
        !syntax::is_valid(callee->object) ||
        callee->object.value >= this->module_.exprs.size()) {
        return nullptr;
    }
    const TypeHandle enum_type = this->resolve_type_selector(callee->object, report_unknown);
    if (!is_valid(enum_type)) {
        return nullptr;
    }
    if (this->checked_.types.get(enum_type).kind != TypeKind::enum_) {
        if (report_unknown) {
            this->report_general(
                this->module_.exprs.range(callee_id.value),
                std::string(SEMA_ENUM_CASE_SCOPE_TYPE)
            );
        }
        return nullptr;
    }
    if (const EnumCaseInfo* result =
            this->find_enum_case_by_type_and_case(enum_type, callee->field_name_id, callee->field_name);
        result != nullptr) {
        return result;
    }
    if (report_unknown) {
        this->report_lookup(
            this->module_.exprs.range(callee_id.value),
            sema_unknown_scoped_enum_case_message(this->checked_.types.display_name(enum_type), callee->field_name)
        );
        this->report_lookup_suggestion(
            this->module_.exprs.range(callee_id.value),
            this->nearest_enum_case_name(enum_type, callee->field_name)
        );
    }
    return nullptr;
}

const Symbol* SemanticAnalyzer::find_symbol(
    const IdentId name_id,
    const std::string_view name,
    const base::SourceRange& range
) {
    if (const Symbol* local = this->symbols_.find(name_id); local != nullptr) {
        return local;
    }

    const ModuleLookupKey lookup_key = this->find_module_lookup_key(this->current_module_, name_id);
    if (is_valid(lookup_key)) {
        if (const auto found = this->global_values_by_name_.find(lookup_key);
            found != this->global_values_by_name_.end()) {
            return found->second;
        }
    }

    this->report_lookup(range, sema_unknown_name_message(name));
    this->report_lookup_suggestion(range, this->nearest_visible_value_name(name));
    return nullptr;
}

} // namespace aurex::sema
