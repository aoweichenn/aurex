#include <aurex/sema/sema_messages.hpp>

#include <array>
#include <unordered_set>

#include <sema/internal/name_resolution.hpp>
#include <sema/internal/sema_lookup_resolver.hpp>

namespace aurex::sema {

namespace {

[[nodiscard]] std::string module_path_parts_name(const std::vector<std::string_view>& parts)
{
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

SemanticAnalyzerCore::LookupResolver::LookupResolver(SemanticAnalyzerCore& core) noexcept : core_(core)
{
}

SemanticAnalyzerCore::LookupResolver::LookupResolver(const SemanticAnalyzerCore& core) noexcept : core_(core)
{
}

SemanticAnalyzerCore& SemanticAnalyzerCore::LookupResolver::mutable_core() const noexcept
{
    return const_cast<SemanticAnalyzerCore&>(this->core_);
}

syntax::ModuleId SemanticAnalyzerCore::LookupResolver::resolve_import_alias(
    const std::string_view alias, const base::SourceRange& range, const bool report_unknown) const
{
    return ModuleVisibilityResolver(this->core_).resolve_import_alias(alias, range, report_unknown);
}

const SemanticAnalyzerCore::ModuleIdList& SemanticAnalyzerCore::LookupResolver::visible_modules(
    const syntax::ModuleId module) const
{
    return ModuleVisibilityResolver(this->core_).visible_modules(module);
}

bool SemanticAnalyzerCore::LookupResolver::module_alias_visible(const std::string_view name) const
{
    return ModuleVisibilityResolver(this->core_).module_alias_visible(name);
}

bool SemanticAnalyzerCore::LookupResolver::visible_root_module_name_exists(const std::string_view name) const
{
    return ModuleVisibilityResolver(this->core_).visible_root_module_name_exists(name);
}

std::vector<std::string_view> SemanticAnalyzerCore::LookupResolver::type_scope_parts(const syntax::TypeNode& type) const
{
    if (!type.scope_parts.empty()) {
        return type.scope_parts;
    }
    if (!type.scope_name.empty()) {
        return {type.scope_name};
    }
    return {};
}

syntax::ModuleId SemanticAnalyzerCore::LookupResolver::find_visible_module_path(
    const std::vector<std::string_view>& parts) const
{
    return ModuleVisibilityResolver(this->core_).find_visible_module_path(parts);
}

bool SemanticAnalyzerCore::LookupResolver::visible_module_path_prefix_exists(
    const std::vector<std::string_view>& parts) const
{
    return ModuleVisibilityResolver(this->core_).visible_module_path_prefix_exists(parts);
}

syntax::ModuleId SemanticAnalyzerCore::LookupResolver::resolve_type_scope(
    const syntax::TypeNode& type, const bool report_unknown)
{
    const std::vector<std::string_view> parts = this->type_scope_parts(type);
    if (parts.empty()) {
        return syntax::INVALID_MODULE_ID;
    }
    if (parts.size() == 1) {
        return this->resolve_import_alias(parts.front(), type.scope_range, report_unknown);
    }

    const syntax::ModuleId module = this->find_visible_module_path(parts);
    if (!syntax::is_valid(module) && report_unknown) {
        this->mutable_core().report_lookup(
            type.scope_range, sema_unknown_module_path_message(module_path_parts_name(parts)));
    }
    return module;
}

const SemanticAnalyzerCore::ModuleIdList& SemanticAnalyzerCore::LookupResolver::module_export_modules(
    const syntax::ModuleId module) const
{
    return ModuleVisibilityResolver(this->core_).module_export_modules(module);
}

SemanticAnalyzerCore::ModuleIdList SemanticAnalyzerCore::LookupResolver::accessible_module_export_modules(
    const syntax::ModuleId module) const
{
    return ModuleVisibilityResolver(this->core_).accessible_module_export_modules(module);
}

SemanticAnalyzerCore::SelectiveReexportTargetList SemanticAnalyzerCore::LookupResolver::accessible_selective_reexports(
    const syntax::ModuleId module, const IdentId name_id, const std::string_view name) const
{
    return ModuleVisibilityResolver(this->core_).accessible_selective_reexports(module, name_id, name);
}

void SemanticAnalyzerCore::LookupResolver::append_public_reexports(
    const syntax::ModuleId module, ModuleIdList& result, std::unordered_set<base::u32>& seen) const
{
    ModuleVisibilityResolver(this->core_).append_public_reexports(module, result, seen);
}

std::string SemanticAnalyzerCore::LookupResolver::module_name(const syntax::ModuleId module) const
{
    return ModuleVisibilityResolver(this->core_).module_name(module);
}

const FunctionSignature* SemanticAnalyzerCore::LookupResolver::find_method_in_visible_modules(
    const TypeHandle owner_type, const IdentId name_id, const std::string_view name, const base::SourceRange& range,
    const bool require_self, const bool report_unknown)
{
    const FunctionSignature* result = nullptr;
    const FunctionSignature* inaccessible_result = nullptr;
    syntax::ModuleId result_module = syntax::INVALID_MODULE_ID;
    const std::array<syntax::ModuleId, 2> modules{
        this->core_.state_.flow.current_module, this->core_.owner_module(owner_type)};
    std::unordered_set<base::u32> seen_modules;
    for (const syntax::ModuleId module : modules) {
        if (!syntax::is_valid(module) || !seen_modules.insert(module.value).second) {
            continue;
        }
        const FunctionSignature* signature = nullptr;
        const MethodLookupKey lookup_key = this->core_.find_method_lookup_key(module, owner_type, name_id);
        if (is_valid(lookup_key)) {
            if (const auto found = this->core_.state_.names.methods_by_name.find(lookup_key);
                found != this->core_.state_.names.methods_by_name.end()) {
                signature = found->second;
            }
        }
        if (signature == nullptr || !signature->is_method || (require_self && !signature->has_self_param)) {
            continue;
        }
        if (!this->core_.can_access_module(module, signature->visibility)) {
            if (inaccessible_result == nullptr) {
                inaccessible_result = signature;
            }
            continue;
        }
        if (result != nullptr) {
            this->mutable_core().report_lookup(range,
                sema_ambiguous_method_message(this->core_.state_.checked.types.display_name(owner_type), name,
                    this->core_.module_name(result_module), this->core_.module_name(module)));
            return nullptr;
        }
        result = signature;
        result_module = module;
    }
    if (result == nullptr && report_unknown) {
        if (inaccessible_result != nullptr) {
            this->mutable_core().report_visibility(
                range, sema_private_method_message(this->core_.state_.checked.types.display_name(owner_type), name));
            return nullptr;
        }
        this->mutable_core().report_lookup(
            range, sema_unknown_method_message(this->core_.state_.checked.types.display_name(owner_type), name));
    }
    return result;
}

TypeHandle SemanticAnalyzerCore::LookupResolver::find_type_in_visible_modules(const IdentId name_id,
    const std::string_view name, const base::SourceRange& range, const bool opaque_allowed_as_pointee,
    const bool report_unknown)
{
    const ModuleLookupKey lookup_key =
        this->core_.find_module_lookup_key(this->core_.state_.flow.current_module, name_id);
    if (is_valid(lookup_key)) {
        if (const auto found = this->core_.state_.names.named_types_by_name.find(lookup_key);
            found != this->core_.state_.names.named_types_by_name.end()) {
            return found->second.type;
        }
        if (const auto found = this->core_.state_.names.type_aliases_by_name.find(lookup_key);
            found != this->core_.state_.names.type_aliases_by_name.end() && found->second != nullptr) {
            return this->mutable_core().resolve_type_alias(*found->second, opaque_allowed_as_pointee);
        }
    }

    if (report_unknown) {
        this->mutable_core().report_lookup(range, sema_unknown_type_message(name));
        this->mutable_core().report_lookup_suggestion(range, this->core_.nearest_visible_type_name(name));
    }
    return INVALID_TYPE_HANDLE;
}

TypeHandle SemanticAnalyzerCore::LookupResolver::find_type_in_module(const syntax::ModuleId module,
    const IdentId name_id, const std::string_view name, const base::SourceRange& range,
    const bool opaque_allowed_as_pointee, const bool report_unknown)
{
    if (!syntax::is_valid(module)) {
        if (report_unknown) {
            this->mutable_core().report_lookup(range, sema_unknown_type_message(name));
            this->mutable_core().report_lookup_suggestion(range, this->core_.nearest_visible_type_name(name));
        }
        return INVALID_TYPE_HANDLE;
    }

    TypeHandle result = INVALID_TYPE_HANDLE;
    syntax::ModuleId result_module = syntax::INVALID_MODULE_ID;
    const auto consider_candidate = [&](const syntax::ModuleId candidate_module, const IdentId lookup_name_id,
                                        const std::string_view lookup_name) -> bool {
        static_cast<void>(lookup_name);
        TypeHandle candidate = INVALID_TYPE_HANDLE;
        const ModuleLookupKey lookup_key = this->core_.find_module_lookup_key(candidate_module, lookup_name_id);
        if (is_valid(lookup_key)) {
            if (const auto found = this->core_.state_.names.named_types_by_name.find(lookup_key);
                found != this->core_.state_.names.named_types_by_name.end()) {
                if (!this->core_.can_access_module(candidate_module, found->second.visibility)) {
                    if (candidate_module.value == module.value && report_unknown) {
                        this->mutable_core().report_visibility(
                            range, sema_private_type_message(this->core_.module_name(candidate_module), name));
                        return true;
                    }
                    return false;
                }
                candidate = found->second.type;
            } else if (const auto alias_found = this->core_.state_.names.type_aliases_by_name.find(lookup_key);
                alias_found != this->core_.state_.names.type_aliases_by_name.end() && alias_found->second != nullptr) {
                if (!this->core_.can_access_module(candidate_module, alias_found->second->visibility)) {
                    if (candidate_module.value == module.value && report_unknown) {
                        this->mutable_core().report_visibility(
                            range, sema_private_type_message(this->core_.module_name(candidate_module), name));
                        return true;
                    }
                    return false;
                }
                candidate = this->mutable_core().resolve_type_alias(*alias_found->second, opaque_allowed_as_pointee);
            }
        }
        if (!is_valid(candidate)) {
            return false;
        }
        if (is_valid(result) && result.value == candidate.value) {
            return false;
        }
        if (is_valid(result)) {
            if (report_unknown) {
                this->mutable_core().report_lookup(range,
                    sema_ambiguous_type_name_message(
                        name, this->core_.module_name(result_module), this->core_.module_name(candidate_module)));
            }
            result = INVALID_TYPE_HANDLE;
            return true;
        }
        result = candidate;
        result_module = candidate_module;
        return false;
    };
    const auto consider_exported_modules = [&](const syntax::ModuleId exported_module, const IdentId lookup_name_id,
                                               const std::string_view lookup_name) -> bool {
        for (const syntax::ModuleId candidate_module : this->core_.accessible_module_export_modules(exported_module)) {
            if (consider_candidate(candidate_module, lookup_name_id, lookup_name)) {
                return true;
            }
        }
        return false;
    };

    if (consider_exported_modules(module, name_id, name)) {
        return INVALID_TYPE_HANDLE;
    }
    for (const SelectiveReexportTarget& target : this->core_.accessible_selective_reexports(module, name_id, name)) {
        if (consider_exported_modules(target.module, target.name_id, target.name)) {
            return INVALID_TYPE_HANDLE;
        }
    }
    if (is_valid(result)) {
        return result;
    }

    if (report_unknown) {
        this->mutable_core().report_lookup(
            range, sema_unknown_type_in_module_message(this->core_.module_name(module), name));
        this->mutable_core().report_lookup_suggestion(range, this->core_.nearest_type_name_in_module(module, name));
    }
    return INVALID_TYPE_HANDLE;
}

const FunctionSignature* SemanticAnalyzerCore::LookupResolver::find_function_in_visible_modules(
    const IdentId name_id, const std::string_view name, const base::SourceRange& range, const bool report_unknown)
{
    const ModuleLookupKey lookup_key =
        this->core_.find_module_lookup_key(this->core_.state_.flow.current_module, name_id);
    if (is_valid(lookup_key)) {
        if (const auto found = this->core_.state_.names.functions_by_name.find(lookup_key);
            found != this->core_.state_.names.functions_by_name.end()) {
            return found->second;
        }
    }

    if (report_unknown) {
        this->mutable_core().report_lookup(range, sema_unknown_function_message(name));
        this->mutable_core().report_lookup_suggestion(range, this->core_.nearest_visible_function_name(name));
    }
    return nullptr;
}

const FunctionSignature* SemanticAnalyzerCore::LookupResolver::find_function_in_module(const syntax::ModuleId module,
    const IdentId name_id, const std::string_view name, const base::SourceRange& range, const bool report_unknown)
{
    if (!syntax::is_valid(module)) {
        if (report_unknown) {
            this->mutable_core().report_lookup(
                range, sema_unknown_function_in_module_message(this->core_.module_name(module), name));
            this->mutable_core().report_lookup_suggestion(range, this->core_.nearest_visible_function_name(name));
        }
        return nullptr;
    }

    const FunctionSignature* result = nullptr;
    syntax::ModuleId result_module = syntax::INVALID_MODULE_ID;
    const auto consider_candidate = [&](const syntax::ModuleId candidate_module, const IdentId lookup_name_id) -> bool {
        const FunctionSignature* candidate = nullptr;
        const ModuleLookupKey lookup_key = this->core_.find_module_lookup_key(candidate_module, lookup_name_id);
        if (is_valid(lookup_key)) {
            if (const auto found = this->core_.state_.names.functions_by_name.find(lookup_key);
                found != this->core_.state_.names.functions_by_name.end()) {
                candidate = found->second;
            }
        }
        if (candidate == nullptr) {
            return false;
        }
        if (!this->core_.can_access_module(candidate_module, candidate->visibility)) {
            if (candidate_module.value == module.value && report_unknown) {
                this->mutable_core().report_visibility(
                    range, sema_private_function_message(this->core_.module_name(candidate_module), name));
                return true;
            }
            return false;
        }
        if (result == candidate) {
            return false;
        }
        if (result != nullptr) {
            if (report_unknown) {
                this->mutable_core().report_lookup(range,
                    sema_ambiguous_function_name_message(
                        name, this->core_.module_name(result_module), this->core_.module_name(candidate_module)));
            }
            result = nullptr;
            return true;
        }
        result = candidate;
        result_module = candidate_module;
        return false;
    };
    const auto consider_exported_modules = [&](const syntax::ModuleId exported_module, const IdentId lookup_name_id) {
        for (const syntax::ModuleId candidate_module : this->core_.accessible_module_export_modules(exported_module)) {
            if (consider_candidate(candidate_module, lookup_name_id)) {
                return true;
            }
        }
        return false;
    };

    if (consider_exported_modules(module, name_id)) {
        return nullptr;
    }
    for (const SelectiveReexportTarget& target : this->core_.accessible_selective_reexports(module, name_id, name)) {
        if (consider_exported_modules(target.module, target.name_id)) {
            return nullptr;
        }
    }
    if (result == nullptr && report_unknown) {
        this->mutable_core().report_lookup(
            range, sema_unknown_function_in_module_message(this->core_.module_name(module), name));
        this->mutable_core().report_lookup_suggestion(range, this->core_.nearest_function_name_in_module(module, name));
    }
    return result;
}

const Symbol* SemanticAnalyzerCore::LookupResolver::find_symbol_in_module(const syntax::ModuleId module,
    const IdentId name_id, const std::string_view name, const base::SourceRange& range, const bool report_unknown)
{
    if (!syntax::is_valid(module)) {
        if (report_unknown) {
            this->mutable_core().report_lookup(
                range, sema_unknown_name_in_module_message(this->core_.module_name(module), name));
            this->mutable_core().report_lookup_suggestion(range, this->core_.nearest_visible_value_name(name));
        }
        return nullptr;
    }

    const Symbol* result = nullptr;
    syntax::ModuleId result_module = syntax::INVALID_MODULE_ID;
    const auto consider_candidate = [&](const syntax::ModuleId candidate_module, const IdentId lookup_name_id) -> bool {
        const Symbol* candidate = nullptr;
        const ModuleLookupKey lookup_key = this->core_.find_module_lookup_key(candidate_module, lookup_name_id);
        if (is_valid(lookup_key)) {
            if (const auto found = this->core_.state_.names.global_values_by_name.find(lookup_key);
                found != this->core_.state_.names.global_values_by_name.end()) {
                candidate = found->second;
            }
        }
        if (candidate == nullptr) {
            return false;
        }
        if (!this->core_.can_access_module(candidate_module, candidate->visibility)) {
            if (candidate_module.value == module.value && report_unknown) {
                this->mutable_core().report_visibility(
                    range, sema_private_name_message(this->core_.module_name(candidate_module), name));
                return true;
            }
            return false;
        }
        if (result == candidate) {
            return false;
        }
        if (result != nullptr) {
            if (report_unknown) {
                this->mutable_core().report_lookup(range,
                    sema_ambiguous_name_message(
                        name, this->core_.module_name(result_module), this->core_.module_name(candidate_module)));
            }
            result = nullptr;
            return true;
        }
        result = candidate;
        result_module = candidate_module;
        return false;
    };
    const auto consider_exported_modules = [&](const syntax::ModuleId exported_module, const IdentId lookup_name_id) {
        for (const syntax::ModuleId candidate_module : this->core_.accessible_module_export_modules(exported_module)) {
            if (consider_candidate(candidate_module, lookup_name_id)) {
                return true;
            }
        }
        return false;
    };

    if (consider_exported_modules(module, name_id)) {
        return nullptr;
    }
    for (const SelectiveReexportTarget& target : this->core_.accessible_selective_reexports(module, name_id, name)) {
        if (consider_exported_modules(target.module, target.name_id)) {
            return nullptr;
        }
    }
    if (result == nullptr && report_unknown) {
        this->mutable_core().report_lookup(
            range, sema_unknown_name_in_module_message(this->core_.module_name(module), name));
        this->mutable_core().report_lookup_suggestion(range, this->core_.nearest_value_name_in_module(module, name));
    }
    return result;
}

const EnumCaseInfo* SemanticAnalyzerCore::LookupResolver::find_enum_case_in_visible_modules(
    const IdentId name_id, const std::string_view name, const base::SourceRange& range, const bool report_unknown)
{
    const ModuleLookupKey lookup_key =
        this->core_.find_module_lookup_key(this->core_.state_.flow.current_module, name_id);
    if (is_valid(lookup_key)) {
        if (const auto found = this->core_.state_.names.enum_cases_by_module_name.find(lookup_key);
            found != this->core_.state_.names.enum_cases_by_module_name.end()) {
            return found->second;
        }
    }

    if (report_unknown) {
        this->mutable_core().report_lookup(range, sema_unknown_enum_case_message(name));
        this->mutable_core().report_lookup_suggestion(range, this->core_.nearest_visible_enum_case_name(name));
    }
    return nullptr;
}

const EnumCaseInfo* SemanticAnalyzerCore::LookupResolver::find_enum_case_by_scoped_name(const IdentId enum_name_id,
    const std::string_view enum_name, const IdentId case_name_id, const std::string_view case_name,
    const base::SourceRange& range, const bool report_unknown)
{
    const ModuleLookupKey enum_lookup_key =
        this->core_.find_module_lookup_key(this->core_.state_.flow.current_module, enum_name_id);
    const bool enum_type_name_exists = is_valid(enum_lookup_key)
        && (this->core_.state_.names.named_types_by_name.contains(enum_lookup_key)
            || this->core_.state_.names.type_aliases_by_name.contains(enum_lookup_key));
    if (!report_unknown && !enum_type_name_exists && this->core_.named_type_lookup_complete()
        && this->core_.type_alias_lookup_complete()) {
        return nullptr;
    }
    const TypeHandle enum_type =
        this->mutable_core().find_type_in_visible_modules(enum_name_id, enum_name, range, false);
    if (!is_valid(enum_type) || this->core_.state_.checked.types.get(enum_type).kind != TypeKind::enum_) {
        if (is_valid(enum_type) && report_unknown) {
            this->mutable_core().report_general(range, std::string(SEMA_ENUM_CASE_SCOPE_TYPE));
        }
        return nullptr;
    }
    if (const EnumCaseInfo* result = this->core_.find_enum_case_by_type_and_case(enum_type, case_name_id, case_name);
        result != nullptr) {
        return result;
    }
    if (report_unknown) {
        this->mutable_core().report_lookup(range, sema_unknown_scoped_enum_case_message(enum_name, case_name));
        this->mutable_core().report_lookup_suggestion(range, this->core_.nearest_enum_case_name(enum_type, case_name));
    }
    return nullptr;
}

const EnumCaseInfo* SemanticAnalyzerCore::LookupResolver::find_enum_constructor(
    const syntax::ExprId callee_id, const bool report_unknown)
{
    if (!syntax::is_valid(callee_id) || callee_id.value >= this->core_.ctx_.module.exprs.size()) {
        return nullptr;
    }
    const syntax::FieldExprPayload* const callee = this->core_.ctx_.module.exprs.field_payload(callee_id.value);
    if (callee == nullptr || !syntax::is_valid(callee->object)
        || callee->object.value >= this->core_.ctx_.module.exprs.size()) {
        return nullptr;
    }
    const TypeHandle enum_type = this->mutable_core().resolve_type_selector(callee->object, report_unknown);
    if (!is_valid(enum_type)) {
        return nullptr;
    }
    if (this->core_.state_.checked.types.get(enum_type).kind != TypeKind::enum_) {
        if (report_unknown) {
            this->mutable_core().report_general(
                this->core_.ctx_.module.exprs.range(callee_id.value), std::string(SEMA_ENUM_CASE_SCOPE_TYPE));
        }
        return nullptr;
    }
    if (const EnumCaseInfo* result =
            this->core_.find_enum_case_by_type_and_case(enum_type, callee->field_name_id, callee->field_name);
        result != nullptr) {
        return result;
    }
    if (report_unknown) {
        this->mutable_core().report_lookup(this->core_.ctx_.module.exprs.range(callee_id.value),
            sema_unknown_scoped_enum_case_message(
                this->core_.state_.checked.types.display_name(enum_type), callee->field_name));
        this->mutable_core().report_lookup_suggestion(this->core_.ctx_.module.exprs.range(callee_id.value),
            this->core_.nearest_enum_case_name(enum_type, callee->field_name));
    }
    return nullptr;
}

const Symbol* SemanticAnalyzerCore::LookupResolver::find_symbol(
    const IdentId name_id, const std::string_view name, const base::SourceRange& range)
{
    if (const Symbol* local = this->core_.state_.names.symbols.find(name_id); local != nullptr) {
        return local;
    }

    const ModuleLookupKey lookup_key =
        this->core_.find_module_lookup_key(this->core_.state_.flow.current_module, name_id);
    if (is_valid(lookup_key)) {
        if (const auto found = this->core_.state_.names.global_values_by_name.find(lookup_key);
            found != this->core_.state_.names.global_values_by_name.end()) {
            return found->second;
        }
    }

    this->mutable_core().report_lookup(range, sema_unknown_name_message(name));
    this->mutable_core().report_lookup_suggestion(range, this->core_.nearest_visible_value_name(name));
    return nullptr;
}

SemanticAnalyzerCore::LookupResolver SemanticAnalyzerCore::lookup_resolver() noexcept
{
    return LookupResolver(*this);
}

SemanticAnalyzerCore::LookupResolver SemanticAnalyzerCore::lookup_resolver() const noexcept
{
    return LookupResolver(*this);
}

syntax::ModuleId SemanticAnalyzerCore::resolve_import_alias(
    const std::string_view alias, const base::SourceRange& range, const bool report_unknown) const
{
    return this->lookup_resolver().resolve_import_alias(alias, range, report_unknown);
}

const SemanticAnalyzerCore::ModuleIdList& SemanticAnalyzerCore::visible_modules(const syntax::ModuleId module) const
{
    return this->lookup_resolver().visible_modules(module);
}

bool SemanticAnalyzerCore::module_alias_visible(const std::string_view name) const
{
    return this->lookup_resolver().module_alias_visible(name);
}

bool SemanticAnalyzerCore::visible_root_module_name_exists(const std::string_view name) const
{
    return this->lookup_resolver().visible_root_module_name_exists(name);
}

std::vector<std::string_view> SemanticAnalyzerCore::type_scope_parts(const syntax::TypeNode& type) const
{
    return this->lookup_resolver().type_scope_parts(type);
}

syntax::ModuleId SemanticAnalyzerCore::find_visible_module_path(const std::vector<std::string_view>& parts) const
{
    return this->lookup_resolver().find_visible_module_path(parts);
}

bool SemanticAnalyzerCore::visible_module_path_prefix_exists(const std::vector<std::string_view>& parts) const
{
    return this->lookup_resolver().visible_module_path_prefix_exists(parts);
}

syntax::ModuleId SemanticAnalyzerCore::resolve_type_scope(const syntax::TypeNode& type, const bool report_unknown)
{
    return this->lookup_resolver().resolve_type_scope(type, report_unknown);
}

const SemanticAnalyzerCore::ModuleIdList& SemanticAnalyzerCore::module_export_modules(
    const syntax::ModuleId module) const
{
    return this->lookup_resolver().module_export_modules(module);
}

SemanticAnalyzerCore::ModuleIdList SemanticAnalyzerCore::accessible_module_export_modules(
    const syntax::ModuleId module) const
{
    return this->lookup_resolver().accessible_module_export_modules(module);
}

SemanticAnalyzerCore::SelectiveReexportTargetList SemanticAnalyzerCore::accessible_selective_reexports(
    const syntax::ModuleId module, const IdentId name_id, const std::string_view name) const
{
    return this->lookup_resolver().accessible_selective_reexports(module, name_id, name);
}

void SemanticAnalyzerCore::append_public_reexports(
    const syntax::ModuleId module, ModuleIdList& result, std::unordered_set<base::u32>& seen) const
{
    this->lookup_resolver().append_public_reexports(module, result, seen);
}

std::string SemanticAnalyzerCore::module_name(const syntax::ModuleId module) const
{
    return this->lookup_resolver().module_name(module);
}

const FunctionSignature* SemanticAnalyzerCore::find_method_in_visible_modules(const TypeHandle owner_type,
    const IdentId name_id, const std::string_view name, const base::SourceRange& range, const bool require_self,
    const bool report_unknown)
{
    return this->lookup_resolver().find_method_in_visible_modules(
        owner_type, name_id, name, range, require_self, report_unknown);
}

TypeHandle SemanticAnalyzerCore::find_type_in_visible_modules(const IdentId name_id, const std::string_view name,
    const base::SourceRange& range, const bool opaque_allowed_as_pointee, const bool report_unknown)
{
    return this->lookup_resolver().find_type_in_visible_modules(
        name_id, name, range, opaque_allowed_as_pointee, report_unknown);
}

TypeHandle SemanticAnalyzerCore::find_type_in_module(const syntax::ModuleId module, const IdentId name_id,
    const std::string_view name, const base::SourceRange& range, const bool opaque_allowed_as_pointee,
    const bool report_unknown)
{
    return this->lookup_resolver().find_type_in_module(
        module, name_id, name, range, opaque_allowed_as_pointee, report_unknown);
}

const FunctionSignature* SemanticAnalyzerCore::find_function_in_visible_modules(
    const IdentId name_id, const std::string_view name, const base::SourceRange& range, const bool report_unknown)
{
    return this->lookup_resolver().find_function_in_visible_modules(name_id, name, range, report_unknown);
}

const FunctionSignature* SemanticAnalyzerCore::find_function_in_module(const syntax::ModuleId module,
    const IdentId name_id, const std::string_view name, const base::SourceRange& range, const bool report_unknown)
{
    return this->lookup_resolver().find_function_in_module(module, name_id, name, range, report_unknown);
}

const Symbol* SemanticAnalyzerCore::find_symbol_in_module(const syntax::ModuleId module, const IdentId name_id,
    const std::string_view name, const base::SourceRange& range, const bool report_unknown)
{
    return this->lookup_resolver().find_symbol_in_module(module, name_id, name, range, report_unknown);
}

const EnumCaseInfo* SemanticAnalyzerCore::find_enum_case_in_visible_modules(
    const IdentId name_id, const std::string_view name, const base::SourceRange& range, const bool report_unknown)
{
    return this->lookup_resolver().find_enum_case_in_visible_modules(name_id, name, range, report_unknown);
}

const EnumCaseInfo* SemanticAnalyzerCore::find_enum_case_by_scoped_name(const IdentId enum_name_id,
    const std::string_view enum_name, const IdentId case_name_id, const std::string_view case_name,
    const base::SourceRange& range, const bool report_unknown)
{
    return this->lookup_resolver().find_enum_case_by_scoped_name(
        enum_name_id, enum_name, case_name_id, case_name, range, report_unknown);
}

const EnumCaseInfo* SemanticAnalyzerCore::find_enum_constructor(
    const syntax::ExprId callee_id, const bool report_unknown)
{
    return this->lookup_resolver().find_enum_constructor(callee_id, report_unknown);
}

const Symbol* SemanticAnalyzerCore::find_symbol(
    const IdentId name_id, const std::string_view name, const base::SourceRange& range)
{
    return this->lookup_resolver().find_symbol(name_id, name, range);
}

} // namespace aurex::sema
