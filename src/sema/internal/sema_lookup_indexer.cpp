#include <aurex/sema/sema_messages.hpp>

#include <sema/internal/sema_lookup_indexer.hpp>

namespace aurex::sema {

SemanticAnalyzerCore::LookupIndexer::LookupIndexer(SemanticAnalyzerCore& core) noexcept : core_(core)
{
}

void SemanticAnalyzerCore::LookupIndexer::index_named_type(
    const syntax::ModuleId module, const IdentId name_id, const TypeHandle type, const syntax::Visibility visibility)
{
    const ModuleLookupKey key = this->core_.intern_module_lookup_key(module, name_id);
    this->core_.state_.names.named_types_by_name[key] = IndexedTypeInfo{type, visibility};
}

void SemanticAnalyzerCore::LookupIndexer::index_type_alias(const TypeAliasInfo& info)
{
    const ModuleLookupKey key = this->core_.intern_module_lookup_key(info.module, info.name_id);
    this->core_.state_.names.type_aliases_by_name[key] = &info;
}

void SemanticAnalyzerCore::LookupIndexer::index_generic_struct_template(const GenericTemplateInfo& info)
{
    const ModuleLookupKey key = this->core_.intern_module_lookup_key(info.module, info.name_id);
    this->core_.state_.names.generic_struct_templates_by_name[key] = &info;
}

void SemanticAnalyzerCore::LookupIndexer::index_generic_enum_template(const GenericTemplateInfo& info)
{
    const ModuleLookupKey key = this->core_.intern_module_lookup_key(info.module, info.name_id);
    this->core_.state_.names.generic_enum_templates_by_name[key] = &info;
}

void SemanticAnalyzerCore::LookupIndexer::index_generic_type_alias_template(const GenericTemplateInfo& info)
{
    const ModuleLookupKey key = this->core_.intern_module_lookup_key(info.module, info.name_id);
    this->core_.state_.names.generic_type_alias_templates_by_name[key] = &info;
}

void SemanticAnalyzerCore::LookupIndexer::index_generic_function_template(const GenericTemplateInfo& info)
{
    const ModuleLookupKey key = this->core_.intern_module_lookup_key(info.module, info.name_id);
    this->core_.state_.names.generic_function_templates_by_name[key] = &info;
}

void SemanticAnalyzerCore::LookupIndexer::index_generic_method_template(const GenericTemplateInfo& info)
{
    const ModuleLookupKey key = this->core_.intern_module_lookup_key(info.module, info.name_id);
    this->core_.generic_method_template_bucket(key).push_back(&info);
    this->core_.state_.names.generic_method_lookup_indexed_count += 1;
}

void SemanticAnalyzerCore::LookupIndexer::index_function_lookup(const FunctionSignature& signature)
{
    if (signature.is_method) {
        this->core_.index_method_lookup(signature.module, signature.method_owner_type, signature.name_id, signature);
        return;
    }
    const ModuleLookupKey key = this->core_.intern_module_lookup_key(signature.module, signature.name_id);
    this->core_.state_.names.functions_by_name[key] = &signature;
}

void SemanticAnalyzerCore::LookupIndexer::index_method_lookup(const syntax::ModuleId module,
    const TypeHandle owner_type, const IdentId name_id, const FunctionSignature& signature)
{
    const MethodLookupKey key = this->core_.intern_method_lookup_key(module, owner_type, name_id);
    this->core_.state_.names.methods_by_name[key] = &signature;
}

void SemanticAnalyzerCore::LookupIndexer::index_function_value(const FunctionSignature& signature)
{
    if (signature.is_method) {
        const auto found = this->core_.state_.functions.global_values.find(signature.semantic_key);
        if (found != this->core_.state_.functions.global_values.end()) {
            const MethodLookupKey key =
                this->core_.intern_method_lookup_key(signature.module, signature.method_owner_type, signature.name_id);
            this->core_.state_.names.method_global_values_by_name[key] = &found->second;
        }
        return;
    }
    const auto found = this->core_.state_.functions.global_values.find(signature.semantic_key);
    if (found != this->core_.state_.functions.global_values.end()) {
        this->core_.index_global_value(found->second);
    }
}

void SemanticAnalyzerCore::LookupIndexer::index_global_value(const Symbol& symbol)
{
    const ModuleLookupKey key = this->core_.intern_module_lookup_key(symbol.module, symbol.name_id);
    this->core_.state_.names.global_values_by_name[key] = &symbol;
}

bool SemanticAnalyzerCore::LookupIndexer::named_type_lookup_complete() const noexcept
{
    return this->core_.state_.names.named_types_by_name.size() == this->core_.state_.types.named_types.size();
}

bool SemanticAnalyzerCore::LookupIndexer::type_alias_lookup_complete() const noexcept
{
    return this->core_.state_.names.type_aliases_by_name.size() == this->core_.state_.checked.type_aliases.size();
}

bool SemanticAnalyzerCore::LookupIndexer::generic_struct_lookup_complete() const noexcept
{
    return this->core_.state_.names.generic_struct_templates_by_name.size()
        == this->core_.state_.generics.struct_templates.size();
}

bool SemanticAnalyzerCore::LookupIndexer::generic_enum_lookup_complete() const noexcept
{
    return this->core_.state_.names.generic_enum_templates_by_name.size()
        == this->core_.state_.generics.enum_templates.size();
}

bool SemanticAnalyzerCore::LookupIndexer::generic_type_alias_lookup_complete() const noexcept
{
    return this->core_.state_.names.generic_type_alias_templates_by_name.size()
        == this->core_.state_.generics.type_alias_templates.size();
}

bool SemanticAnalyzerCore::LookupIndexer::generic_function_lookup_complete() const noexcept
{
    return this->core_.state_.names.generic_function_templates_by_name.size()
        == this->core_.state_.generics.function_templates.size();
}

bool SemanticAnalyzerCore::LookupIndexer::generic_method_lookup_complete() const noexcept
{
    return this->core_.state_.names.generic_method_lookup_indexed_count
        == this->core_.state_.generics.method_templates.size();
}

bool SemanticAnalyzerCore::LookupIndexer::function_lookup_complete() const noexcept
{
    return this->core_.state_.names.functions_by_name.size() + this->core_.state_.names.methods_by_name.size()
        + this->core_.state_.names.internal_function_lookup_exclusions
        == this->core_.state_.checked.functions.size();
}

bool SemanticAnalyzerCore::LookupIndexer::global_value_lookup_complete() const noexcept
{
    return this->core_.state_.names.global_values_by_name.size()
        + this->core_.state_.names.method_global_values_by_name.size()
        == this->core_.state_.functions.global_values.size();
}

bool SemanticAnalyzerCore::LookupIndexer::enum_case_module_lookup_complete() const noexcept
{
    return this->core_.state_.names.enum_cases_by_module_name.size() == this->core_.state_.checked.enum_cases.size();
}

bool SemanticAnalyzerCore::LookupIndexer::top_level_value_name_exists(
    const syntax::ModuleId module, const IdentId name_id, const std::string_view name) const
{
    static_cast<void>(name);
    const ModuleLookupKey lookup_key = this->core_.find_module_lookup_key(module, name_id);
    if (is_valid(lookup_key)) {
        if (const auto found = this->core_.state_.names.global_values_by_name.find(lookup_key);
            found != this->core_.state_.names.global_values_by_name.end() && found->second != nullptr) {
            return found->second->kind == SymbolKind::function || found->second->kind == SymbolKind::const_;
        }
    }
    return false;
}

bool SemanticAnalyzerCore::LookupIndexer::module_type_or_value_name_exists(
    const syntax::ModuleId module, const IdentId name_id, const std::string_view name) const
{
    const ModuleLookupKey lookup_key = this->core_.find_module_lookup_key(module, name_id);
    const bool typed_type_found = is_valid(lookup_key)
        && (this->core_.state_.names.named_types_by_name.contains(lookup_key)
            || this->core_.state_.names.type_aliases_by_name.contains(lookup_key)
            || this->core_.state_.names.traits_by_name.contains(lookup_key));
    if (typed_type_found || this->core_.find_any_generic_type_template_in_module(module, name_id, name) != nullptr
        || this->core_.top_level_value_name_exists(module, name_id, name)
        || (is_valid(lookup_key) && this->core_.state_.names.generic_function_templates_by_name.contains(lookup_key))) {
        return true;
    }
    return false;
}

bool SemanticAnalyzerCore::LookupIndexer::current_generic_param_exists(
    const IdentId name_id, const std::string_view) const
{
    return this->core_.state_.flow.current_generic_context != nullptr
        && this->core_.state_.flow.current_generic_context->params.contains(name_id);
}

bool SemanticAnalyzerCore::LookupIndexer::visible_type_name_exists(
    const IdentId name_id, const std::string_view name) const
{
    if (this->core_.current_generic_param_exists(name_id, name)) {
        return true;
    }
    const auto type_visible_in_module = [&](const syntax::ModuleId module) {
        if (!syntax::is_valid(module)) {
            return false;
        }
        const ModuleLookupKey lookup_key = this->core_.find_module_lookup_key(module, name_id);
        if (is_valid(lookup_key)) {
            if (const auto found = this->core_.state_.names.named_types_by_name.find(lookup_key);
                found != this->core_.state_.names.named_types_by_name.end()) {
                return module.value == this->core_.state_.flow.current_module.value
                    || this->core_.can_access_module(module, found->second.visibility);
            }
            if (const auto alias = this->core_.state_.names.type_aliases_by_name.find(lookup_key);
                alias != this->core_.state_.names.type_aliases_by_name.end() && alias->second != nullptr) {
                return this->core_.can_access_module(module, alias->second->visibility);
            }
            if (const auto trait = this->core_.state_.names.traits_by_name.find(lookup_key);
                trait != this->core_.state_.names.traits_by_name.end() && trait->second != nullptr) {
                return this->core_.can_access_module(module, trait->second->visibility);
            }
        }
        const GenericTemplateInfo* const generic =
            this->core_.find_any_generic_type_template_in_module(module, name_id, name);
        return generic != nullptr && this->core_.can_access_module(module, generic->visibility);
    };
    if (type_visible_in_module(this->core_.state_.flow.current_module)) {
        return true;
    }
    if (!syntax::is_valid(this->core_.state_.flow.current_module)
        || this->core_.state_.flow.current_module.value >= this->core_.ctx_.module.modules.size()) {
        return false;
    }
    for (const syntax::ResolvedImport& import : this->core_.imports_for_scope(this->core_.state_.flow.current_module)) {
        if (type_visible_in_module(import.module)) {
            return true;
        }
    }
    return false;
}

bool SemanticAnalyzerCore::LookupIndexer::can_define_local_name(
    const IdentId name_id, const std::string_view name, const base::SourceRange& range) const
{
    if (name.empty()) {
        return true;
    }
    if (this->core_.module_alias_visible(name)) {
        this->core_.report_duplicate(range, sema_local_shadows_import_alias_message(name));
        return false;
    }
    if (this->core_.visible_root_module_name_exists(name)) {
        this->core_.report_duplicate(range, sema_local_shadows_root_module_message(name));
        return false;
    }
    if (this->core_.current_generic_param_exists(name_id, name)) {
        this->core_.report_duplicate(range, sema_local_shadows_generic_type_parameter_message(name));
        return false;
    }
    if (this->core_.visible_type_name_exists(name_id, name)) {
        this->core_.report_duplicate(range, sema_local_shadows_type_name_message(name));
        return false;
    }
    return true;
}

bool SemanticAnalyzerCore::LookupIndexer::type_member_name_exists(
    const TypeHandle owner_type, const IdentId name_id, const std::string_view name) const
{
    if (!is_valid(owner_type)) {
        return false;
    }
    if (const auto found = this->core_.state_.names.enum_cases_by_type.find(owner_type.value);
        found != this->core_.state_.names.enum_cases_by_type.end()) {
        for (const EnumCaseInfo* enum_case : found->second) {
            if (enum_case != nullptr && enum_case->case_name_id == name_id) {
                return true;
            }
        }
    }
    for (const auto& entry : this->core_.state_.checked.functions) {
        const FunctionSignature& signature = entry.second;
        if (signature.is_method && this->core_.state_.checked.types.same(signature.method_owner_type, owner_type)
            && signature.name_id == name_id) {
            return true;
        }
    }
    for (const auto& entry : this->core_.state_.generics.method_templates) {
        const GenericTemplateInfo& info = entry.second;
        if (info.name_id == name_id && is_valid(info.impl_type_pattern)
            && this->core_.state_.checked.types.same(info.impl_type_pattern, owner_type)) {
            return true;
        }
    }
    static_cast<void>(name);
    return false;
}

void SemanticAnalyzerCore::LookupIndexer::index_enum_case(const EnumCaseInfo& info)
{
    if (!is_valid(info.type)) {
        return;
    }
    this->core_.state_.names.enum_cases_by_type_and_case.emplace(
        EnumCaseLookupKey{
            info.type.value,
            info.case_name_id,
        },
        &info);
    const ModuleLookupKey module_key = this->core_.intern_module_lookup_key(info.module, info.name_id);
    if (is_valid(module_key)) {
        this->core_.state_.names.enum_cases_by_module_name[module_key] = &info;
    }
    this->core_.enum_case_type_bucket(info.type).push_back(&info);
}

} // namespace aurex::sema
