#include <aurex/sema/sema_messages.hpp>

#include <algorithm>
#include <utility>
#include <vector>

#include <sema/internal/sema_core.hpp>
#include <sema/internal/sema_pipeline.hpp>
#include <sema/internal/sema_services.hpp>

namespace aurex::sema {

namespace {

struct SemaItemCounts {
    base::usize items = 0;
    base::usize modules = 0;
    base::usize enum_cases = 0;
    base::usize type_items = 0;
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
    base::usize traits = 0;
    base::usize trait_impls = 0;
    base::usize trait_requirements = 0;
    base::usize value_items = 0;

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
        if (item.kind == syntax::ItemKind::trait_decl) {
            counts.traits += 1;
            counts.trait_requirements += item.trait_items.size();
        }
        if (item.kind == syntax::ItemKind::impl_block && syntax::is_valid(item.trait_type)) {
            counts.trait_impls += 1;
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

[[nodiscard]] bool sema_item_import_scope_range_is_valid(
    const syntax::ItemImportScope& scope, const base::usize item_count) noexcept
{
    const base::usize item_begin = scope.item_begin;
    const base::usize item_count_in_scope = scope.item_count;
    return item_count_in_scope != 0 && item_begin < item_count && item_count_in_scope <= item_count - item_begin;
}

[[nodiscard]] bool sema_item_import_scope_part_matches_items(
    const syntax::ItemImportScope& scope, const std::vector<base::u32>& item_part_indices) noexcept
{
    if (!sema_item_import_scope_range_is_valid(scope, item_part_indices.size())) {
        return false;
    }
    const base::usize item_begin = scope.item_begin;
    const base::usize item_end = item_begin + scope.item_count;
    for (base::usize item = item_begin; item < item_end; ++item) {
        if (item_part_indices[item] != scope.part_index) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool sema_item_import_scope_module_matches_items(
    const syntax::ItemImportScope& scope, const std::vector<syntax::ModuleId>& item_modules) noexcept
{
    if (!sema_item_import_scope_range_is_valid(scope, item_modules.size())) {
        return false;
    }
    const base::usize item_begin = scope.item_begin;
    const base::usize item_end = item_begin + scope.item_count;
    const syntax::ModuleId scope_owner = item_modules[item_begin];
    for (base::usize item = item_begin; item < item_end; ++item) {
        if (item_modules[item].value != scope_owner.value) {
            return false;
        }
    }
    return true;
}

} // namespace

SemanticAnalysisPipeline::SemanticAnalysisPipeline(SemanticAnalyzerCore& core) noexcept : core_(core)
{
}

base::Result<CheckedModule> SemanticAnalysisPipeline::run()
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

bool SemanticAnalysisPipeline::prepare_analysis_session()
{
    this->core_.state_.checked.normalized_ast.original_expr_count = this->core_.ctx_.module.exprs.size();
    this->core_.state_.checked.normalized_ast.original_type_count = this->core_.ctx_.module.types.size();
    if (!this->core_.ctx_.module.identifiers_ready()) {
        this->core_.ctx_.module.intern_identifiers();
    }
    this->normalize_parser_only_module_contract();
    this->core_.ctx_.module.finalize_identifiers();
    if (!this->validate_ast_contract()) {
        return false;
    }
    return true;
}

void SemanticAnalysisPipeline::reserve_analysis_storage()
{
    this->core_.state_.checked.reserve_side_table_storage(this->core_.ctx_.module.exprs.size(),
        this->core_.ctx_.module.patterns.size(), this->core_.ctx_.module.types.size(),
        this->core_.ctx_.module.stmts.size(), this->core_.ctx_.module.items.size());
    this->core_.state_.checked.expr_intrinsic_types.assign(this->core_.ctx_.module.exprs.size(), INVALID_TYPE_HANDLE);
    this->core_.state_.checked.expr_types.assign(this->core_.ctx_.module.exprs.size(), INVALID_TYPE_HANDLE);
    this->core_.state_.checked.expr_owned_use_modes.clear();
    this->core_.state_.checked.prepare_analysis_only_storage(this->core_.ctx_.module.exprs.size());
    this->core_.state_.checked.expr_expected_types.assign(this->core_.ctx_.module.exprs.size(), INVALID_TYPE_HANDLE);
    this->core_.state_.checked.expr_c_name_ids.assign(this->core_.ctx_.module.exprs.size(), INVALID_IDENT_ID);
    this->core_.state_.checked.pattern_c_name_ids.assign(this->core_.ctx_.module.patterns.size(), INVALID_IDENT_ID);
    this->core_.state_.checked.syntax_type_handles.assign(this->core_.ctx_.module.types.size(), INVALID_TYPE_HANDLE);
    this->core_.state_.checked.stmt_local_types.assign(this->core_.ctx_.module.stmts.size(), INVALID_TYPE_HANDLE);
    this->core_.state_.checked.item_c_name_ids.assign(this->core_.ctx_.module.items.size(), INVALID_IDENT_ID);
    const SemaItemCounts item_counts = count_sema_items(this->core_.ctx_.module);
    base::usize expected_function_entries = item_counts.function_like_entries();
    if (item_counts.generic_function_templates != 0 || item_counts.generic_method_templates != 0) {
        expected_function_entries = std::max(expected_function_entries, item_counts.items);
    }
    const base::usize expected_generic_struct_instances =
        item_counts.generic_struct_templates == 0 ? 0 : item_counts.items;
    const base::usize expected_generic_enum_instances = item_counts.generic_enum_templates == 0 ? 0 : item_counts.items;
    const base::usize expected_generic_type_alias_resolutions =
        item_counts.generic_type_alias_templates == 0 ? 0 : item_counts.items;
    const bool retains_generic_function_instances = this->core_.ctx_.options.retain_generic_side_tables
        && (item_counts.generic_function_templates != 0 || item_counts.generic_method_templates != 0);
    const base::usize expected_generic_function_instances = !retains_generic_function_instances ? 0 : item_counts.items;
    this->core_.state_.checked.functions.reserve(expected_function_entries);
    this->core_.state_.checked.structs.reserve(item_counts.struct_like_items);
    this->core_.state_.checked.enum_cases.reserve(item_counts.enum_cases);
    this->core_.state_.checked.type_aliases.reserve(item_counts.type_aliases);
    this->core_.state_.checked.traits.reserve(item_counts.traits);
    this->core_.state_.checked.trait_impls.reserve(item_counts.trait_impls);
    this->core_.state_.checked.trait_predicates.reserve(item_counts.trait_impls + item_counts.items);
    this->core_.state_.checked.trait_obligations.reserve(item_counts.items);
    this->core_.state_.checked.trait_evidence.reserve(item_counts.trait_impls + item_counts.items);
    this->core_.state_.checked.trait_method_calls.reserve(item_counts.items);
    this->core_.state_.checked.function_calls.reserve(item_counts.items);
    this->core_.state_.checked.borrow_summaries.reserve(expected_function_entries);
    this->core_.state_.checked.param_envs.reserve(item_counts.items);
    this->core_.state_.types.named_types.reserve(item_counts.type_items);
    this->core_.state_.generics.struct_templates.reserve(item_counts.generic_struct_templates);
    this->core_.state_.generics.enum_templates.reserve(item_counts.generic_enum_templates);
    this->core_.state_.generics.type_alias_templates.reserve(item_counts.generic_type_alias_templates);
    this->core_.state_.generics.function_templates.reserve(item_counts.generic_function_templates);
    this->core_.state_.generics.method_templates.reserve(item_counts.generic_method_templates);
    this->core_.state_.generics.struct_instances.reserve(expected_generic_struct_instances);
    this->core_.state_.generics.enum_instances.reserve(expected_generic_enum_instances);
    this->core_.state_.generics.resolved_type_aliases.reserve(expected_generic_type_alias_resolutions);
    this->core_.state_.generics.function_instances.reserve(expected_generic_function_instances);
    this->core_.state_.generics.placeholder_functions.reserve(
        item_counts.generic_function_templates + item_counts.generic_method_templates);
    this->core_.state_.types.resolved_type_aliases.reserve(item_counts.type_aliases);
    this->core_.state_.functions.global_values.reserve(item_counts.named_value_entries());
    this->core_.state_.functions.definition_items.reserve(item_counts.function_like_entries());
    this->core_.state_.functions.body_states.reserve(expected_function_entries);
    this->core_.state_.traits.requirement_items.reserve(item_counts.trait_requirements);
    this->core_.state_.traits.default_method_instances.reserve(item_counts.trait_impls + item_counts.items);
    this->core_.state_.types.struct_infos_by_type.reserve(item_counts.struct_like_items);
    const base::usize expected_identifier_count =
        this->core_.ctx_.module.identifiers.size() + item_counts.items + item_counts.enum_cases + item_counts.modules;
    this->core_.ctx_.module.identifiers.reserve(expected_identifier_count);
    this->core_.state_.names.named_types_by_name.reserve(item_counts.type_items);
    this->core_.state_.names.type_aliases_by_name.reserve(item_counts.type_aliases);
    this->core_.state_.names.generic_struct_templates_by_name.reserve(item_counts.generic_struct_templates);
    this->core_.state_.names.generic_enum_templates_by_name.reserve(item_counts.generic_enum_templates);
    this->core_.state_.names.generic_type_alias_templates_by_name.reserve(item_counts.generic_type_alias_templates);
    this->core_.state_.names.generic_function_templates_by_name.reserve(item_counts.generic_function_templates);
    this->core_.state_.names.generic_method_templates_by_name.reserve(item_counts.generic_method_templates);
    this->core_.state_.names.functions_by_name.reserve(
        item_counts.non_generic_functions + item_counts.generic_function_templates);
    this->core_.state_.names.methods_by_name.reserve(
        item_counts.non_generic_methods + item_counts.generic_method_templates);
    this->core_.state_.names.traits_by_name.reserve(item_counts.traits);
    this->core_.state_.names.global_values_by_name.reserve(item_counts.named_value_entries());
    this->core_.state_.names.method_global_values_by_name.reserve(
        item_counts.non_generic_methods + item_counts.generic_method_templates);
    this->core_.state_.names.enum_cases_by_module_name.reserve(item_counts.enum_cases);
    this->core_.state_.names.enum_cases_by_type.reserve(item_counts.enum_items);
    this->core_.state_.names.enum_cases_by_type_and_case.reserve(item_counts.enum_cases);
    this->core_.state_.modules.visible_modules_cache.reserve(item_counts.modules);
    this->core_.state_.modules.item_visible_modules_cache.reserve(item_counts.items);
    this->core_.state_.modules.export_modules_cache.reserve(item_counts.modules);
}

void SemanticAnalysisPipeline::run_declaration_phases()
{
    this->core_.register_type_names();
    this->core_.resolve_type_alias_decls();
    this->core_.analyze_struct_properties();
    this->core_.register_trait_signatures();
    this->core_.register_value_names();
    this->core_.validate_trait_impls();
    this->core_.validate_module_namespace_conflicts();
    this->core_.validate_function_prototypes();
}

void SemanticAnalysisPipeline::run_function_body_phases()
{
    SemanticServiceBundle services = this->core_.services();
    SemanticGenericService generic = services.generic();
    SemanticBodyCheckService body_check = services.body_check();

    for (const auto& entry : this->core_.state_.generics.function_templates) {
        generic.analyze_function_definition(entry.second);
    }
    for (const auto& entry : this->core_.state_.generics.method_templates) {
        generic.analyze_function_definition(entry.second);
    }

    this->core_.analyze_trait_default_method_bodies();

    for (base::u32 index = 0; index < this->core_.ctx_.module.items.size(); ++index) {
        if (this->core_.ctx_.module.items.kind(index) != syntax::ItemKind::fn_decl) {
            continue;
        }
        if (this->core_.is_trait_requirement_item(syntax::ItemId{index})) {
            continue;
        }
        const syntax::ItemNode item = this->core_.ctx_.module.items[index];
        if (!generic.has_generic_params(item) && !item.is_extern_c && !item.is_prototype
            && syntax::is_valid(item.body)) {
            body_check.analyze_function_body(item, syntax::ItemId{index});
        }
    }
}

void SemanticAnalysisPipeline::run_validation_phases()
{
    SemanticServiceBundle services = this->core_.services();
    this->core_.analyze_entry_points();
    this->core_.analyze_const_decls();
    this->core_.validate_exported_signature_surfaces();
    services.type().validate_type_layouts();
    this->core_.validate_abi_symbols();
}

base::Result<CheckedModule> SemanticAnalysisPipeline::finish_analysis()
{
    if (this->core_.ctx_.diagnostics.has_error()) {
        return base::Result<CheckedModule>::fail({base::ErrorCode::sema_error, std::string(SEMA_ANALYSIS_FAILED)});
    }
    this->core_.state_.checked.normalized_ast.final_expr_count = this->core_.ctx_.module.exprs.size();
    this->core_.state_.checked.normalized_ast.final_type_count = this->core_.ctx_.module.types.size();
    this->core_.state_.checked.release_analysis_only_storage();
    return base::Result<CheckedModule>::ok(std::move(this->core_.state_.checked));
}

void SemanticAnalysisPipeline::normalize_parser_only_module_contract()
{
    if (!this->core_.ctx_.module.modules.empty()) {
        this->core_.state_.checked.normalized_ast.parser_only_module_contract_added = false;
        return;
    }
    syntax::ModuleInfo root;
    root.path = this->core_.ctx_.module.module_path;
    this->core_.ctx_.module.modules.push_back(std::move(root));
    this->core_.ctx_.module.item_modules.assign(this->core_.ctx_.module.items.size(), syntax::ModuleId{0});
    this->core_.ctx_.module.item_part_indices.assign(this->core_.ctx_.module.items.size(), 0);
    this->core_.state_.checked.normalized_ast.parser_only_module_contract_added = true;
}

bool SemanticAnalysisPipeline::validate_ast_contract() const
{
    bool valid = true;
    if (this->core_.ctx_.module.item_modules.size() != this->core_.ctx_.module.items.size()) {
        this->core_.report_internal_contract({}, std::string(SEMA_AST_ITEM_MODULE_CONTRACT));
        valid = false;
    }
    if (this->core_.ctx_.module.item_part_indices.size() != this->core_.ctx_.module.items.size()) {
        this->core_.report_internal_contract({}, std::string(SEMA_AST_ITEM_PART_CONTRACT));
        valid = false;
    }
    for (const syntax::ItemImportScope& scope : this->core_.ctx_.module.item_import_scopes) {
        if (scope.item_count == 0) {
            continue;
        }
        if (!sema_item_import_scope_range_is_valid(scope, this->core_.ctx_.module.items.size())) {
            this->core_.report_internal_contract({}, std::string(SEMA_AST_ITEM_MODULE_CONTRACT));
            valid = false;
            continue;
        }
        if (!sema_item_import_scope_range_is_valid(scope, this->core_.ctx_.module.item_modules.size())) {
            this->core_.report_internal_contract({}, std::string(SEMA_AST_ITEM_MODULE_CONTRACT));
            valid = false;
            continue;
        }
        if (!sema_item_import_scope_module_matches_items(scope, this->core_.ctx_.module.item_modules)) {
            this->core_.report_internal_contract({}, std::string(SEMA_AST_ITEM_IMPORT_SCOPE_MODULE_INVALID));
            valid = false;
            continue;
        }
        if (!sema_item_import_scope_part_matches_items(scope, this->core_.ctx_.module.item_part_indices)) {
            this->core_.report_internal_contract({}, std::string(SEMA_AST_ITEM_IMPORT_SCOPE_PART_INVALID));
            valid = false;
            continue;
        }
        const syntax::ModuleId scope_owner = this->core_.ctx_.module.item_modules[scope.item_begin];
        if (syntax::is_valid(scope_owner) && scope_owner.value < this->core_.ctx_.options.module_part_keys.size()
            && !this->core_.ctx_.options.module_part_keys[scope_owner.value].empty()) {
            const std::vector<query::ModulePartKey>& part_keys =
                this->core_.ctx_.options.module_part_keys[scope_owner.value];
            if (scope.part_index >= part_keys.size() || !query::is_valid(part_keys[scope.part_index])) {
                this->core_.report_internal_contract({}, std::string(SEMA_AST_ITEM_IMPORT_SCOPE_PART_INVALID));
                valid = false;
            }
        }
    }
    const base::usize count =
        std::min(this->core_.ctx_.module.item_modules.size(), this->core_.ctx_.module.items.size());
    for (base::usize i = 0; i < count; ++i) {
        const syntax::ModuleId owner = this->core_.ctx_.module.item_modules[i];
        if (!syntax::is_valid(owner) || owner.value >= this->core_.ctx_.module.modules.size()) {
            this->core_.report_internal_contract(
                this->core_.ctx_.module.items.range(i), std::string(SEMA_AST_ITEM_MODULE_INVALID));
            valid = false;
        }
        if (owner.value < this->core_.ctx_.options.module_part_keys.size()
            && !this->core_.ctx_.options.module_part_keys[owner.value].empty()) {
            const std::vector<query::ModulePartKey>& part_keys = this->core_.ctx_.options.module_part_keys[owner.value];
            if (i >= this->core_.ctx_.module.item_part_indices.size()
                || this->core_.ctx_.module.item_part_indices[i] >= part_keys.size()
                || !query::is_valid(part_keys[this->core_.ctx_.module.item_part_indices[i]])) {
                this->core_.report_internal_contract(
                    this->core_.ctx_.module.items.range(i), std::string(SEMA_AST_ITEM_PART_INVALID));
                valid = false;
            }
        }
    }
    return valid;
}

} // namespace aurex::sema
