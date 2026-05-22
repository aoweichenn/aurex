#include <aurex/sema/sema_messages.hpp>

#include <algorithm>
#include <utility>

#include <sema/internal/sema_core.hpp>

namespace aurex::sema {

namespace {

struct SemaItemCounts {
    base::usize items = 0;
    base::usize modules = 0;
    base::usize enum_cases = 0;
    base::usize type_items = 0;
    base::usize non_generic_type_items = 0;
    base::usize struct_like_items = 0;
    base::usize enum_items = 0;
    base::usize generic_struct_templates = 0;
    base::usize generic_enum_templates = 0;
    base::usize generic_type_alias_templates = 0;
    base::usize generic_function_templates = 0;
    base::usize generic_method_templates = 0;
    base::usize non_generic_functions = 0;
    base::usize non_generic_methods = 0;
    base::usize const_items = 0;
    base::usize type_aliases = 0;
    base::usize value_items = 0;

    [[nodiscard]] base::usize generic_type_templates() const noexcept
    {
        return this->generic_struct_templates + this->generic_enum_templates + this->generic_type_alias_templates;
    }

    [[nodiscard]] base::usize generic_templates() const noexcept
    {
        return this->generic_type_templates() + this->generic_function_templates + this->generic_method_templates;
    }

    [[nodiscard]] base::usize checked_function_entries() const noexcept
    {
        return this->non_generic_functions + this->non_generic_methods + this->generic_function_templates
            + this->generic_method_templates;
    }

    [[nodiscard]] base::usize function_like_entries() const noexcept
    {
        return this->checked_function_entries();
    }

    [[nodiscard]] base::usize named_value_entries() const noexcept
    {
        return this->non_generic_functions + this->generic_function_templates + this->const_items;
    }
};

[[nodiscard]] bool sema_item_has_generic_params(const syntax::ItemNode& item) noexcept
{
    return !item.generic_params.empty();
}

[[nodiscard]] bool sema_item_is_type(const syntax::ItemKind kind) noexcept
{
    return kind == syntax::ItemKind::struct_decl || kind == syntax::ItemKind::enum_decl
        || kind == syntax::ItemKind::opaque_struct_decl || kind == syntax::ItemKind::type_alias;
}

[[nodiscard]] SemaItemCounts count_sema_items(const syntax::AstModule& module) noexcept
{
    SemaItemCounts counts;
    counts.items = module.items.size();
    counts.modules = module.modules.size();
    for (base::usize i = 0; i < module.items.size(); ++i) {
        const syntax::ItemNode item = module.items[i];
        if (sema_item_is_type(item.kind)) {
            counts.type_items += 1;
        }
        if (item.kind == syntax::ItemKind::struct_decl || item.kind == syntax::ItemKind::opaque_struct_decl) {
            counts.struct_like_items += 1;
        }
        if (item.kind == syntax::ItemKind::enum_decl) {
            counts.enum_items += 1;
        }
        if (item.kind == syntax::ItemKind::enum_decl) {
            counts.enum_cases += item.enum_cases.size();
        }
        if (item.kind == syntax::ItemKind::type_alias) {
            counts.type_aliases += 1;
        }
        if (item.kind == syntax::ItemKind::const_decl) {
            counts.const_items += 1;
            counts.value_items += 1;
            continue;
        }
        const bool generic = sema_item_has_generic_params(item);
        if (generic) {
            switch (item.kind) {
                case syntax::ItemKind::struct_decl:
                    counts.generic_struct_templates += 1;
                    break;
                case syntax::ItemKind::enum_decl:
                    counts.generic_enum_templates += 1;
                    break;
                case syntax::ItemKind::type_alias:
                    counts.generic_type_alias_templates += 1;
                    break;
                case syntax::ItemKind::fn_decl:
                    if (syntax::is_valid(item.impl_type)) {
                        counts.generic_method_templates += 1;
                    } else {
                        counts.generic_function_templates += 1;
                        counts.value_items += 1;
                    }
                    break;
                default:
                    break;
            }
            continue;
        }
        if (sema_item_is_type(item.kind)) {
            counts.non_generic_type_items += 1;
        }
        if (item.kind == syntax::ItemKind::fn_decl) {
            if (syntax::is_valid(item.impl_type)) {
                counts.non_generic_methods += 1;
            } else {
                counts.non_generic_functions += 1;
                counts.value_items += 1;
            }
        }
    }
    return counts;
}

} // namespace

SemanticAnalyzerCore::SemanticAnalyzerCore(
    syntax::AstModule& module, base::DiagnosticSink& diagnostics, const SemanticOptions options) noexcept
    : ctx_{module, diagnostics, options}
{
}

SemanticAnalyzerCore::SemanticAnalyzerCore(
    syntax::AstModule&& module, base::DiagnosticSink& diagnostics, const SemanticOptions options) noexcept
    : owned_module_(std::move(module)), ctx_{*this->owned_module_, diagnostics, options}
{
}

SemanticAnalyzerCore::GenericTemplateList& SemanticAnalyzerCore::generic_method_template_bucket(
    const ModuleLookupKey& key)
{
    if (const auto found = this->state_.names.generic_method_templates_by_name.find(key);
        found != this->state_.names.generic_method_templates_by_name.end()) {
        return found->second;
    }
    auto bucket = make_sema_vector<const GenericTemplateInfo*>(*this->state_.arena);
    const auto inserted = this->state_.names.generic_method_templates_by_name.emplace(key, std::move(bucket));
    return inserted.first->second;
}

SemanticAnalyzerCore::EnumCaseList& SemanticAnalyzerCore::enum_case_type_bucket(const TypeHandle enum_type)
{
    if (const auto found = this->state_.names.enum_cases_by_type.find(enum_type.value);
        found != this->state_.names.enum_cases_by_type.end()) {
        return found->second;
    }
    auto bucket = make_sema_vector<const EnumCaseInfo*>(*this->state_.arena);
    const auto inserted = this->state_.names.enum_cases_by_type.emplace(enum_type.value, std::move(bucket));
    return inserted.first->second;
}

SemanticAnalyzerCore::ModuleIdList SemanticAnalyzerCore::make_module_id_list() const
{
    return make_sema_vector<syntax::ModuleId>(*this->state_.arena);
}

SemanticAnalyzerCore::GenericTemplateInfo SemanticAnalyzerCore::make_generic_template_info() const
{
    GenericTemplateInfo info;
    info.params = make_sema_vector<IdentId>(*this->state_.arena);
    info.param_identities = make_sema_vector<GenericParamIdentity>(*this->state_.arena);
    info.constraints = make_sema_map<IdentId, CapabilitySet, IdentIdHash>(*this->state_.arena, IdentIdHash{});
    info.expr_node_ids = make_sema_vector<base::u32>(*this->state_.arena);
    info.pattern_node_ids = make_sema_vector<base::u32>(*this->state_.arena);
    info.type_node_ids = make_sema_vector<base::u32>(*this->state_.arena);
    info.stmt_node_ids = make_sema_vector<base::u32>(*this->state_.arena);
    return info;
}

SemanticAnalyzerCore::GenericContext SemanticAnalyzerCore::make_generic_context() const
{
    GenericContext context;
    context.params = make_sema_map<IdentId, TypeHandle, IdentIdHash>(*this->state_.arena, IdentIdHash{});
    context.param_identities =
        make_sema_map<IdentId, GenericParamIdentity, IdentIdHash>(*this->state_.arena, IdentIdHash{});
    context.constraints = make_sema_map<IdentId, CapabilitySet, IdentIdHash>(*this->state_.arena, IdentIdHash{});
    context.constraints_by_identity = make_sema_map<GenericParamIdentity, CapabilitySet, GenericParamIdentityHash>(
        *this->state_.arena, GenericParamIdentityHash{});
    return context;
}

SemanticAnalyzerCore::CapabilitySet SemanticAnalyzerCore::make_capability_set() const
{
    return make_sema_set<CapabilityKind, CapabilityKindHash>(*this->state_.arena, CapabilityKindHash{});
}

SemanticAnalyzerCore::CapabilitySet SemanticAnalyzerCore::copy_capability_set(const CapabilitySet& source) const
{
    CapabilitySet copy = this->make_capability_set();
    copy.reserve(source.size());
    copy.insert(source.begin(), source.end());
    return copy;
}

void SemanticAnalyzerCore::copy_capability_map(CapabilityMap& target, const CapabilityMap& source) const
{
    target.clear();
    target.reserve(source.size());
    for (const auto& entry : source) {
        target.emplace(entry.first, this->copy_capability_set(entry.second));
    }
}

SemanticAnalyzerCore::CapabilitySet& SemanticAnalyzerCore::capability_bucket(
    CapabilityMap& map, const IdentId key) const
{
    if (const auto found = map.find(key); found != map.end()) {
        return found->second;
    }
    const auto inserted = map.emplace(key, this->make_capability_set());
    return inserted.first->second;
}

SemanticAnalyzerCore::CapabilitySet& SemanticAnalyzerCore::capability_bucket(
    CapabilityIdentityMap& map, const GenericParamIdentity key) const
{
    if (const auto found = map.find(key); found != map.end()) {
        return found->second;
    }
    const auto inserted = map.emplace(key, this->make_capability_set());
    return inserted.first->second;
}

base::Result<CheckedModule> SemanticAnalyzerCore::analyze()
{
    if (!this->prepare_analysis_session()) {
        return base::Result<CheckedModule>::fail({base::ErrorCode::sema_error, std::string(SEMA_ANALYSIS_FAILED)});
    }

    this->reserve_analysis_storage();
    this->run_declaration_phases();
    this->run_function_body_phases();
    this->run_validation_phases();
    return this->finish_analysis();
}

bool SemanticAnalyzerCore::prepare_analysis_session()
{
    this->state_.checked.normalized_ast.original_expr_count = this->ctx_.module.exprs.size();
    this->state_.checked.normalized_ast.original_type_count = this->ctx_.module.types.size();
    if (!this->ctx_.module.identifiers_ready()) {
        this->ctx_.module.intern_identifiers();
    }
    this->normalize_parser_only_module_contract();
    this->ctx_.module.finalize_identifiers();
    if (!this->validate_ast_contract()) {
        return false;
    }
    return true;
}

void SemanticAnalyzerCore::reserve_analysis_storage()
{
    this->state_.checked.reserve_side_table_storage(this->ctx_.module.exprs.size(), this->ctx_.module.patterns.size(),
        this->ctx_.module.types.size(), this->ctx_.module.stmts.size(), this->ctx_.module.items.size());
    this->state_.checked.expr_intrinsic_types.assign(this->ctx_.module.exprs.size(), INVALID_TYPE_HANDLE);
    this->state_.checked.expr_types.assign(this->ctx_.module.exprs.size(), INVALID_TYPE_HANDLE);
    this->state_.checked.prepare_analysis_only_storage(this->ctx_.module.exprs.size());
    this->state_.checked.expr_expected_types.assign(this->ctx_.module.exprs.size(), INVALID_TYPE_HANDLE);
    this->state_.checked.expr_c_name_ids.assign(this->ctx_.module.exprs.size(), INVALID_IDENT_ID);
    this->state_.checked.pattern_c_name_ids.assign(this->ctx_.module.patterns.size(), INVALID_IDENT_ID);
    this->state_.checked.syntax_type_handles.assign(this->ctx_.module.types.size(), INVALID_TYPE_HANDLE);
    this->state_.checked.stmt_local_types.assign(this->ctx_.module.stmts.size(), INVALID_TYPE_HANDLE);
    this->state_.checked.item_c_name_ids.assign(this->ctx_.module.items.size(), INVALID_IDENT_ID);
    const SemaItemCounts item_counts = count_sema_items(this->ctx_.module);
    base::usize expected_function_entries = item_counts.function_like_entries();
    if (item_counts.generic_function_templates != 0 || item_counts.generic_method_templates != 0) {
        expected_function_entries = std::max(expected_function_entries, item_counts.items);
    }
    const base::usize expected_generic_struct_instances =
        item_counts.generic_struct_templates == 0 ? 0 : item_counts.items;
    const base::usize expected_generic_enum_instances = item_counts.generic_enum_templates == 0 ? 0 : item_counts.items;
    const base::usize expected_generic_type_alias_resolutions =
        item_counts.generic_type_alias_templates == 0 ? 0 : item_counts.items;
    const bool retains_generic_function_instances = this->ctx_.options.retain_generic_side_tables
        && (item_counts.generic_function_templates != 0 || item_counts.generic_method_templates != 0);
    const base::usize expected_generic_function_instances = !retains_generic_function_instances ? 0 : item_counts.items;
    this->state_.checked.functions.reserve(expected_function_entries);
    this->state_.checked.structs.reserve(item_counts.struct_like_items);
    this->state_.checked.enum_cases.reserve(item_counts.enum_cases);
    this->state_.checked.type_aliases.reserve(item_counts.type_aliases);
    this->state_.types.named_types.reserve(item_counts.type_items);
    this->state_.generics.struct_templates.reserve(item_counts.generic_struct_templates);
    this->state_.generics.enum_templates.reserve(item_counts.generic_enum_templates);
    this->state_.generics.type_alias_templates.reserve(item_counts.generic_type_alias_templates);
    this->state_.generics.function_templates.reserve(item_counts.generic_function_templates);
    this->state_.generics.method_templates.reserve(item_counts.generic_method_templates);
    this->state_.generics.struct_instances.reserve(expected_generic_struct_instances);
    this->state_.generics.enum_instances.reserve(expected_generic_enum_instances);
    this->state_.generics.resolved_type_aliases.reserve(expected_generic_type_alias_resolutions);
    this->state_.generics.function_instances.reserve(expected_generic_function_instances);
    this->state_.generics.placeholder_functions.reserve(
        item_counts.generic_function_templates + item_counts.generic_method_templates);
    this->state_.types.resolved_type_aliases.reserve(item_counts.type_aliases);
    this->state_.functions.global_values.reserve(item_counts.named_value_entries());
    this->state_.functions.definition_items.reserve(item_counts.function_like_entries());
    this->state_.functions.body_states.reserve(expected_function_entries);
    this->state_.types.struct_infos_by_type.reserve(item_counts.struct_like_items);
    const base::usize expected_identifier_count =
        this->ctx_.module.identifiers.size() + item_counts.items + item_counts.enum_cases + item_counts.modules;
    this->ctx_.module.identifiers.reserve(expected_identifier_count);
    this->state_.names.named_types_by_name.reserve(item_counts.type_items);
    this->state_.names.type_aliases_by_name.reserve(item_counts.type_aliases);
    this->state_.names.generic_struct_templates_by_name.reserve(item_counts.generic_struct_templates);
    this->state_.names.generic_enum_templates_by_name.reserve(item_counts.generic_enum_templates);
    this->state_.names.generic_type_alias_templates_by_name.reserve(item_counts.generic_type_alias_templates);
    this->state_.names.generic_function_templates_by_name.reserve(item_counts.generic_function_templates);
    this->state_.names.generic_method_templates_by_name.reserve(item_counts.generic_method_templates);
    this->state_.names.functions_by_name.reserve(
        item_counts.non_generic_functions + item_counts.generic_function_templates);
    this->state_.names.methods_by_name.reserve(item_counts.non_generic_methods + item_counts.generic_method_templates);
    this->state_.names.global_values_by_name.reserve(item_counts.named_value_entries());
    this->state_.names.method_global_values_by_name.reserve(
        item_counts.non_generic_methods + item_counts.generic_method_templates);
    this->state_.names.enum_cases_by_module_name.reserve(item_counts.enum_cases);
    this->state_.names.enum_cases_by_type.reserve(item_counts.enum_items);
    this->state_.names.enum_cases_by_type_and_case.reserve(item_counts.enum_cases);
    this->state_.modules.visible_modules_cache.reserve(item_counts.modules);
    this->state_.modules.export_modules_cache.reserve(item_counts.modules);
}

void SemanticAnalyzerCore::run_declaration_phases()
{
    this->register_type_names();
    this->resolve_type_alias_decls();
    this->analyze_struct_properties();
    this->register_value_names();
    this->validate_module_namespace_conflicts();
    this->validate_function_prototypes();
}

void SemanticAnalyzerCore::run_function_body_phases()
{
    for (const auto& entry : this->state_.generics.function_templates) {
        this->analyze_generic_function_definition(entry.second);
    }
    for (const auto& entry : this->state_.generics.method_templates) {
        this->analyze_generic_function_definition(entry.second);
    }

    for (base::u32 index = 0; index < this->ctx_.module.items.size(); ++index) {
        if (this->ctx_.module.items.kind(index) != syntax::ItemKind::fn_decl) {
            continue;
        }
        const syntax::ItemNode item = this->ctx_.module.items[index];
        if (!this->has_generic_params(item) && !item.is_extern_c && !item.is_prototype && syntax::is_valid(item.body)) {
            this->analyze_function_body(item, syntax::ItemId{index});
        }
    }
}

void SemanticAnalyzerCore::run_validation_phases()
{
    this->analyze_entry_points();
    this->analyze_const_decls();
    this->validate_type_layouts();
    this->validate_abi_symbols();
}

base::Result<CheckedModule> SemanticAnalyzerCore::finish_analysis()
{
    if (this->ctx_.diagnostics.has_error()) {
        return base::Result<CheckedModule>::fail({base::ErrorCode::sema_error, std::string(SEMA_ANALYSIS_FAILED)});
    }
    this->state_.checked.normalized_ast.final_expr_count = this->ctx_.module.exprs.size();
    this->state_.checked.normalized_ast.final_type_count = this->ctx_.module.types.size();
    this->state_.checked.release_analysis_only_storage();
    return base::Result<CheckedModule>::ok(std::move(this->state_.checked));
}

void SemanticAnalyzerCore::normalize_parser_only_module_contract()
{
    if (!this->ctx_.module.modules.empty()) {
        this->state_.checked.normalized_ast.parser_only_module_contract_added = false;
        return;
    }
    syntax::ModuleInfo root;
    root.path = this->ctx_.module.module_path;
    this->ctx_.module.modules.push_back(std::move(root));
    this->ctx_.module.item_modules.assign(this->ctx_.module.items.size(), syntax::ModuleId{0});
    this->state_.checked.normalized_ast.parser_only_module_contract_added = true;
}

bool SemanticAnalyzerCore::validate_ast_contract() const
{
    bool valid = true;
    if (this->ctx_.module.item_modules.size() != this->ctx_.module.items.size()) {
        this->report_internal_contract({}, std::string(SEMA_AST_ITEM_MODULE_CONTRACT));
        valid = false;
    }
    const base::usize count = std::min(this->ctx_.module.item_modules.size(), this->ctx_.module.items.size());
    for (base::usize i = 0; i < count; ++i) {
        const syntax::ModuleId owner = this->ctx_.module.item_modules[i];
        if (!syntax::is_valid(owner) || owner.value >= this->ctx_.module.modules.size()) {
            this->report_internal_contract(this->ctx_.module.items.range(i), std::string(SEMA_AST_ITEM_MODULE_INVALID));
            valid = false;
        }
    }
    return valid;
}

} // namespace aurex::sema
