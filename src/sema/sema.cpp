#include <aurex/sema/sema.hpp>

#include <aurex/sema/sema_messages.hpp>

#include <algorithm>
#include <memory>
#include <utility>

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

    [[nodiscard]] base::usize generic_type_templates() const noexcept {
        return this->generic_struct_templates +
               this->generic_enum_templates +
               this->generic_type_alias_templates;
    }

    [[nodiscard]] base::usize generic_templates() const noexcept {
        return this->generic_type_templates() +
               this->generic_function_templates +
               this->generic_method_templates;
    }

    [[nodiscard]] base::usize checked_function_entries() const noexcept {
        return this->non_generic_functions +
               this->non_generic_methods +
               this->generic_function_templates +
               this->generic_method_templates;
    }

    [[nodiscard]] base::usize function_like_entries() const noexcept {
        return this->checked_function_entries();
    }

    [[nodiscard]] base::usize named_value_entries() const noexcept {
        return this->non_generic_functions +
               this->generic_function_templates +
               this->const_items;
    }
};

[[nodiscard]] bool sema_item_has_generic_params(const syntax::ItemNode& item) noexcept {
    return !item.generic_params.empty();
}

[[nodiscard]] bool sema_item_is_type(const syntax::ItemKind kind) noexcept {
    return kind == syntax::ItemKind::struct_decl ||
           kind == syntax::ItemKind::enum_decl ||
           kind == syntax::ItemKind::opaque_struct_decl ||
           kind == syntax::ItemKind::type_alias;
}

[[nodiscard]] SemaItemCounts count_sema_items(const syntax::AstModule& module) noexcept {
    SemaItemCounts counts;
    counts.items = module.items.size();
    counts.modules = module.modules.size();
    for (base::usize i = 0; i < module.items.size(); ++i) {
        const syntax::ItemNode item = module.items[i];
        if (sema_item_is_type(item.kind)) {
            counts.type_items += 1;
        }
        if (item.kind == syntax::ItemKind::struct_decl ||
            item.kind == syntax::ItemKind::opaque_struct_decl) {
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

SemanticAnalyzer::SemanticAnalyzer(
    syntax::AstModule& module,
    base::DiagnosticSink& diagnostics,
    const SemanticOptions options
) noexcept
    : module_(module),
      diagnostics_(diagnostics),
      options_(options),
      arena_(std::make_unique<base::BumpAllocator>()),
      named_types_(make_sema_map<ModuleLookupKey, TypeHandle, ModuleLookupKeyHash>(*this->arena_, ModuleLookupKeyHash {})),
      generic_struct_templates_(make_sema_map<ModuleLookupKey, GenericTemplateInfo, ModuleLookupKeyHash>(*this->arena_, ModuleLookupKeyHash {})),
      generic_enum_templates_(make_sema_map<ModuleLookupKey, GenericTemplateInfo, ModuleLookupKeyHash>(*this->arena_, ModuleLookupKeyHash {})),
      generic_type_alias_templates_(make_sema_map<ModuleLookupKey, GenericTemplateInfo, ModuleLookupKeyHash>(*this->arena_, ModuleLookupKeyHash {})),
      generic_function_templates_(make_sema_map<ModuleLookupKey, GenericTemplateInfo, ModuleLookupKeyHash>(*this->arena_, ModuleLookupKeyHash {})),
      generic_method_templates_(make_sema_map<FunctionLookupKey, GenericTemplateInfo, FunctionLookupKeyHash>(*this->arena_, FunctionLookupKeyHash {})),
      generic_struct_instances_(make_sema_map<IdentId, TypeHandle, IdentIdHash>(*this->arena_, IdentIdHash {})),
      generic_enum_instances_(make_sema_map<IdentId, TypeHandle, IdentIdHash>(*this->arena_, IdentIdHash {})),
      resolved_generic_type_aliases_(make_sema_map<IdentId, TypeHandle, IdentIdHash>(*this->arena_, IdentIdHash {})),
      generic_function_instances_(make_sema_map<FunctionLookupKey, base::usize, FunctionLookupKeyHash>(*this->arena_, FunctionLookupKeyHash {})),
      generic_placeholder_functions_(make_sema_map<FunctionLookupKey, FunctionSignature, FunctionLookupKeyHash>(*this->arena_, FunctionLookupKeyHash {})),
      resolved_type_aliases_(make_sema_map<ModuleLookupKey, TypeHandle, ModuleLookupKeyHash>(*this->arena_, ModuleLookupKeyHash {})),
      resolving_type_aliases_(make_sema_vector<ModuleLookupKey>(*this->arena_)),
      global_values_(make_sema_map<FunctionLookupKey, Symbol, FunctionLookupKeyHash>(*this->arena_, FunctionLookupKeyHash {})),
      function_definition_items_(make_sema_map<FunctionLookupKey, syntax::ItemId, FunctionLookupKeyHash>(*this->arena_, FunctionLookupKeyHash {})),
      function_body_states_(make_sema_map<FunctionLookupKey, FunctionBodyState, FunctionLookupKeyHash>(*this->arena_, FunctionLookupKeyHash {})),
      struct_infos_by_type_(make_sema_map<base::u32, const StructInfo*>(*this->arena_)),
      named_types_by_name_(make_sema_map<ModuleLookupKey, IndexedTypeInfo, ModuleLookupKeyHash>(
          *this->arena_,
          ModuleLookupKeyHash {}
      )),
      type_aliases_by_name_(make_sema_map<ModuleLookupKey, const TypeAliasInfo*, ModuleLookupKeyHash>(
          *this->arena_,
          ModuleLookupKeyHash {}
      )),
      generic_struct_templates_by_name_(make_sema_map<ModuleLookupKey, const GenericTemplateInfo*, ModuleLookupKeyHash>(
          *this->arena_,
          ModuleLookupKeyHash {}
      )),
      generic_enum_templates_by_name_(make_sema_map<ModuleLookupKey, const GenericTemplateInfo*, ModuleLookupKeyHash>(
          *this->arena_,
          ModuleLookupKeyHash {}
      )),
      generic_type_alias_templates_by_name_(make_sema_map<ModuleLookupKey, const GenericTemplateInfo*, ModuleLookupKeyHash>(
          *this->arena_,
          ModuleLookupKeyHash {}
      )),
      generic_function_templates_by_name_(make_sema_map<ModuleLookupKey, const GenericTemplateInfo*, ModuleLookupKeyHash>(
          *this->arena_,
          ModuleLookupKeyHash {}
      )),
      generic_method_templates_by_name_(make_sema_map<ModuleLookupKey, GenericTemplateList, ModuleLookupKeyHash>(
          *this->arena_,
          ModuleLookupKeyHash {}
      )),
      functions_by_name_(make_sema_map<ModuleLookupKey, const FunctionSignature*, ModuleLookupKeyHash>(
          *this->arena_,
          ModuleLookupKeyHash {}
      )),
      methods_by_name_(make_sema_map<MethodLookupKey, const FunctionSignature*, MethodLookupKeyHash>(
          *this->arena_,
          MethodLookupKeyHash {}
      )),
      global_values_by_name_(make_sema_map<ModuleLookupKey, const Symbol*, ModuleLookupKeyHash>(
          *this->arena_,
          ModuleLookupKeyHash {}
      )),
      method_global_values_by_name_(make_sema_map<MethodLookupKey, const Symbol*, MethodLookupKeyHash>(
          *this->arena_,
          MethodLookupKeyHash {}
      )),
      enum_cases_by_module_name_(make_sema_map<ModuleLookupKey, const EnumCaseInfo*, ModuleLookupKeyHash>(
          *this->arena_,
          ModuleLookupKeyHash {}
      )),
      enum_cases_by_type_and_case_(make_sema_map<EnumCaseLookupKey, const EnumCaseInfo*, EnumCaseLookupKeyHash>(
          *this->arena_,
          EnumCaseLookupKeyHash {}
      )),
      enum_cases_by_type_(make_sema_map<base::u32, EnumCaseList>(*this->arena_)),
      visible_modules_cache_(make_sema_map<base::u32, ModuleIdList>(*this->arena_)),
      module_export_modules_cache_(make_sema_map<base::u32, ModuleIdList>(*this->arena_)) {}

SemanticAnalyzer::SemanticAnalyzer(
    syntax::AstModule&& module,
    base::DiagnosticSink& diagnostics,
    const SemanticOptions options
) noexcept
    : owned_module_(std::move(module)),
      module_(*this->owned_module_),
      diagnostics_(diagnostics),
      options_(options),
      arena_(std::make_unique<base::BumpAllocator>()),
      named_types_(make_sema_map<ModuleLookupKey, TypeHandle, ModuleLookupKeyHash>(*this->arena_, ModuleLookupKeyHash {})),
      generic_struct_templates_(make_sema_map<ModuleLookupKey, GenericTemplateInfo, ModuleLookupKeyHash>(*this->arena_, ModuleLookupKeyHash {})),
      generic_enum_templates_(make_sema_map<ModuleLookupKey, GenericTemplateInfo, ModuleLookupKeyHash>(*this->arena_, ModuleLookupKeyHash {})),
      generic_type_alias_templates_(make_sema_map<ModuleLookupKey, GenericTemplateInfo, ModuleLookupKeyHash>(*this->arena_, ModuleLookupKeyHash {})),
      generic_function_templates_(make_sema_map<ModuleLookupKey, GenericTemplateInfo, ModuleLookupKeyHash>(*this->arena_, ModuleLookupKeyHash {})),
      generic_method_templates_(make_sema_map<FunctionLookupKey, GenericTemplateInfo, FunctionLookupKeyHash>(*this->arena_, FunctionLookupKeyHash {})),
      generic_struct_instances_(make_sema_map<IdentId, TypeHandle, IdentIdHash>(*this->arena_, IdentIdHash {})),
      generic_enum_instances_(make_sema_map<IdentId, TypeHandle, IdentIdHash>(*this->arena_, IdentIdHash {})),
      resolved_generic_type_aliases_(make_sema_map<IdentId, TypeHandle, IdentIdHash>(*this->arena_, IdentIdHash {})),
      generic_function_instances_(make_sema_map<FunctionLookupKey, base::usize, FunctionLookupKeyHash>(*this->arena_, FunctionLookupKeyHash {})),
      generic_placeholder_functions_(make_sema_map<FunctionLookupKey, FunctionSignature, FunctionLookupKeyHash>(*this->arena_, FunctionLookupKeyHash {})),
      resolved_type_aliases_(make_sema_map<ModuleLookupKey, TypeHandle, ModuleLookupKeyHash>(*this->arena_, ModuleLookupKeyHash {})),
      resolving_type_aliases_(make_sema_vector<ModuleLookupKey>(*this->arena_)),
      global_values_(make_sema_map<FunctionLookupKey, Symbol, FunctionLookupKeyHash>(*this->arena_, FunctionLookupKeyHash {})),
      function_definition_items_(make_sema_map<FunctionLookupKey, syntax::ItemId, FunctionLookupKeyHash>(*this->arena_, FunctionLookupKeyHash {})),
      function_body_states_(make_sema_map<FunctionLookupKey, FunctionBodyState, FunctionLookupKeyHash>(*this->arena_, FunctionLookupKeyHash {})),
      struct_infos_by_type_(make_sema_map<base::u32, const StructInfo*>(*this->arena_)),
      named_types_by_name_(make_sema_map<ModuleLookupKey, IndexedTypeInfo, ModuleLookupKeyHash>(
          *this->arena_,
          ModuleLookupKeyHash {}
      )),
      type_aliases_by_name_(make_sema_map<ModuleLookupKey, const TypeAliasInfo*, ModuleLookupKeyHash>(
          *this->arena_,
          ModuleLookupKeyHash {}
      )),
      generic_struct_templates_by_name_(make_sema_map<ModuleLookupKey, const GenericTemplateInfo*, ModuleLookupKeyHash>(
          *this->arena_,
          ModuleLookupKeyHash {}
      )),
      generic_enum_templates_by_name_(make_sema_map<ModuleLookupKey, const GenericTemplateInfo*, ModuleLookupKeyHash>(
          *this->arena_,
          ModuleLookupKeyHash {}
      )),
      generic_type_alias_templates_by_name_(make_sema_map<ModuleLookupKey, const GenericTemplateInfo*, ModuleLookupKeyHash>(
          *this->arena_,
          ModuleLookupKeyHash {}
      )),
      generic_function_templates_by_name_(make_sema_map<ModuleLookupKey, const GenericTemplateInfo*, ModuleLookupKeyHash>(
          *this->arena_,
          ModuleLookupKeyHash {}
      )),
      generic_method_templates_by_name_(make_sema_map<ModuleLookupKey, GenericTemplateList, ModuleLookupKeyHash>(
          *this->arena_,
          ModuleLookupKeyHash {}
      )),
      functions_by_name_(make_sema_map<ModuleLookupKey, const FunctionSignature*, ModuleLookupKeyHash>(
          *this->arena_,
          ModuleLookupKeyHash {}
      )),
      methods_by_name_(make_sema_map<MethodLookupKey, const FunctionSignature*, MethodLookupKeyHash>(
          *this->arena_,
          MethodLookupKeyHash {}
      )),
      global_values_by_name_(make_sema_map<ModuleLookupKey, const Symbol*, ModuleLookupKeyHash>(
          *this->arena_,
          ModuleLookupKeyHash {}
      )),
      method_global_values_by_name_(make_sema_map<MethodLookupKey, const Symbol*, MethodLookupKeyHash>(
          *this->arena_,
          MethodLookupKeyHash {}
      )),
      enum_cases_by_module_name_(make_sema_map<ModuleLookupKey, const EnumCaseInfo*, ModuleLookupKeyHash>(
          *this->arena_,
          ModuleLookupKeyHash {}
      )),
      enum_cases_by_type_and_case_(make_sema_map<EnumCaseLookupKey, const EnumCaseInfo*, EnumCaseLookupKeyHash>(
          *this->arena_,
          EnumCaseLookupKeyHash {}
      )),
      enum_cases_by_type_(make_sema_map<base::u32, EnumCaseList>(*this->arena_)),
      visible_modules_cache_(make_sema_map<base::u32, ModuleIdList>(*this->arena_)),
      module_export_modules_cache_(make_sema_map<base::u32, ModuleIdList>(*this->arena_)) {}

SemanticAnalyzer::GenericTemplateList& SemanticAnalyzer::generic_method_template_bucket(
    const ModuleLookupKey& key
) {
    if (const auto found = this->generic_method_templates_by_name_.find(key);
        found != this->generic_method_templates_by_name_.end()) {
        return found->second;
    }
    auto bucket = make_sema_vector<const GenericTemplateInfo*>(*this->arena_);
    const auto inserted = this->generic_method_templates_by_name_.emplace(key, std::move(bucket));
    return inserted.first->second;
}

SemanticAnalyzer::EnumCaseList& SemanticAnalyzer::enum_case_type_bucket(const TypeHandle enum_type) {
    if (const auto found = this->enum_cases_by_type_.find(enum_type.value);
        found != this->enum_cases_by_type_.end()) {
        return found->second;
    }
    auto bucket = make_sema_vector<const EnumCaseInfo*>(*this->arena_);
    const auto inserted = this->enum_cases_by_type_.emplace(enum_type.value, std::move(bucket));
    return inserted.first->second;
}

SemanticAnalyzer::ModuleIdList SemanticAnalyzer::make_module_id_list() const {
    return make_sema_vector<syntax::ModuleId>(*this->arena_);
}

SemanticAnalyzer::GenericTemplateInfo SemanticAnalyzer::make_generic_template_info() const {
    GenericTemplateInfo info;
    info.params = make_sema_vector<IdentId>(*this->arena_);
    info.param_identities = make_sema_vector<GenericParamIdentity>(*this->arena_);
    info.constraints = make_sema_map<IdentId, CapabilitySet, IdentIdHash>(*this->arena_, IdentIdHash {});
    info.expr_node_ids = make_sema_vector<base::u32>(*this->arena_);
    info.pattern_node_ids = make_sema_vector<base::u32>(*this->arena_);
    info.type_node_ids = make_sema_vector<base::u32>(*this->arena_);
    info.stmt_node_ids = make_sema_vector<base::u32>(*this->arena_);
    return info;
}

SemanticAnalyzer::GenericContext SemanticAnalyzer::make_generic_context() const {
    GenericContext context;
    context.params = make_sema_map<IdentId, TypeHandle, IdentIdHash>(*this->arena_, IdentIdHash {});
    context.param_identities = make_sema_map<IdentId, GenericParamIdentity, IdentIdHash>(*this->arena_, IdentIdHash {});
    context.constraints = make_sema_map<IdentId, CapabilitySet, IdentIdHash>(*this->arena_, IdentIdHash {});
    context.constraints_by_identity =
        make_sema_map<GenericParamIdentity, CapabilitySet, GenericParamIdentityHash>(
            *this->arena_,
            GenericParamIdentityHash {}
        );
    return context;
}

SemanticAnalyzer::CapabilitySet SemanticAnalyzer::make_capability_set() const {
    return make_sema_set<CapabilityKind, CapabilityKindHash>(*this->arena_, CapabilityKindHash {});
}

SemanticAnalyzer::CapabilitySet SemanticAnalyzer::copy_capability_set(const CapabilitySet& source) const {
    CapabilitySet copy = this->make_capability_set();
    copy.reserve(source.size());
    copy.insert(source.begin(), source.end());
    return copy;
}

void SemanticAnalyzer::copy_capability_map(CapabilityMap& target, const CapabilityMap& source) const {
    target.clear();
    target.reserve(source.size());
    for (const auto& entry : source) {
        target.emplace(entry.first, this->copy_capability_set(entry.second));
    }
}

SemanticAnalyzer::CapabilitySet& SemanticAnalyzer::capability_bucket(
    CapabilityMap& map,
    const IdentId key
) const {
    if (const auto found = map.find(key); found != map.end()) {
        return found->second;
    }
    const auto inserted = map.emplace(key, this->make_capability_set());
    return inserted.first->second;
}

SemanticAnalyzer::CapabilitySet& SemanticAnalyzer::capability_bucket(
    CapabilityIdentityMap& map,
    const GenericParamIdentity key
) const {
    if (const auto found = map.find(key); found != map.end()) {
        return found->second;
    }
    const auto inserted = map.emplace(key, this->make_capability_set());
    return inserted.first->second;
}

base::Result<CheckedModule> SemanticAnalyzer::analyze() {
    this->checked_.normalized_ast.original_expr_count = this->module_.exprs.size();
    this->checked_.normalized_ast.original_type_count = this->module_.types.size();
    if (!this->module_.identifiers_ready()) {
        this->module_.intern_identifiers();
    }
    this->normalize_parser_only_module_contract();
    this->module_.finalize_identifiers();
    if (!this->validate_ast_contract()) {
        return base::Result<CheckedModule>::fail({base::ErrorCode::sema_error, std::string(SEMA_ANALYSIS_FAILED)});
    }

    this->checked_.reserve_side_table_storage(
        this->module_.exprs.size(),
        this->module_.patterns.size(),
        this->module_.types.size(),
        this->module_.stmts.size(),
        this->module_.items.size()
    );
    this->checked_.expr_intrinsic_types.assign(this->module_.exprs.size(), INVALID_TYPE_HANDLE);
    this->checked_.expr_types.assign(this->module_.exprs.size(), INVALID_TYPE_HANDLE);
    this->checked_.prepare_analysis_only_storage(this->module_.exprs.size());
    this->checked_.expr_expected_types.assign(this->module_.exprs.size(), INVALID_TYPE_HANDLE);
    this->checked_.expr_c_name_ids.assign(this->module_.exprs.size(), INVALID_IDENT_ID);
    this->checked_.pattern_c_name_ids.assign(this->module_.patterns.size(), INVALID_IDENT_ID);
    this->checked_.syntax_type_handles.assign(this->module_.types.size(), INVALID_TYPE_HANDLE);
    this->checked_.stmt_local_types.assign(this->module_.stmts.size(), INVALID_TYPE_HANDLE);
    this->checked_.item_c_name_ids.assign(this->module_.items.size(), INVALID_IDENT_ID);
    const SemaItemCounts item_counts = count_sema_items(this->module_);
    base::usize expected_function_entries = item_counts.function_like_entries();
    if (item_counts.generic_function_templates != 0 ||
        item_counts.generic_method_templates != 0) {
        expected_function_entries = std::max(expected_function_entries, item_counts.items);
    }
    const base::usize expected_generic_struct_instances =
        item_counts.generic_struct_templates == 0 ? 0 : item_counts.items;
    const base::usize expected_generic_enum_instances =
        item_counts.generic_enum_templates == 0 ? 0 : item_counts.items;
    const base::usize expected_generic_type_alias_resolutions =
        item_counts.generic_type_alias_templates == 0 ? 0 : item_counts.items;
    const bool retains_generic_function_instances = this->options_.retain_generic_side_tables &&
        (item_counts.generic_function_templates != 0 || item_counts.generic_method_templates != 0);
    const base::usize expected_generic_function_instances = !retains_generic_function_instances
            ? 0
            : item_counts.items;
    this->checked_.functions.reserve(expected_function_entries);
    this->checked_.structs.reserve(item_counts.struct_like_items);
    this->checked_.enum_cases.reserve(item_counts.enum_cases);
    this->checked_.type_aliases.reserve(item_counts.type_aliases);
    this->named_types_.reserve(item_counts.type_items);
    this->generic_struct_templates_.reserve(item_counts.generic_struct_templates);
    this->generic_enum_templates_.reserve(item_counts.generic_enum_templates);
    this->generic_type_alias_templates_.reserve(item_counts.generic_type_alias_templates);
    this->generic_function_templates_.reserve(item_counts.generic_function_templates);
    this->generic_method_templates_.reserve(item_counts.generic_method_templates);
    this->generic_struct_instances_.reserve(expected_generic_struct_instances);
    this->generic_enum_instances_.reserve(expected_generic_enum_instances);
    this->resolved_generic_type_aliases_.reserve(expected_generic_type_alias_resolutions);
    this->generic_function_instances_.reserve(expected_generic_function_instances);
    this->generic_placeholder_functions_.reserve(
        item_counts.generic_function_templates + item_counts.generic_method_templates
    );
    this->resolved_type_aliases_.reserve(item_counts.type_aliases);
    this->global_values_.reserve(item_counts.named_value_entries());
    this->function_definition_items_.reserve(item_counts.function_like_entries());
    this->function_body_states_.reserve(expected_function_entries);
    this->struct_infos_by_type_.reserve(item_counts.struct_like_items);
    const base::usize expected_identifier_count =
        this->module_.identifiers.size() +
        item_counts.items +
        item_counts.enum_cases +
        item_counts.modules;
    this->module_.identifiers.reserve(expected_identifier_count);
    this->named_types_by_name_.reserve(item_counts.type_items);
    this->type_aliases_by_name_.reserve(item_counts.type_aliases);
    this->generic_struct_templates_by_name_.reserve(item_counts.generic_struct_templates);
    this->generic_enum_templates_by_name_.reserve(item_counts.generic_enum_templates);
    this->generic_type_alias_templates_by_name_.reserve(item_counts.generic_type_alias_templates);
    this->generic_function_templates_by_name_.reserve(item_counts.generic_function_templates);
    this->generic_method_templates_by_name_.reserve(item_counts.generic_method_templates);
    this->functions_by_name_.reserve(item_counts.non_generic_functions + item_counts.generic_function_templates);
    this->methods_by_name_.reserve(item_counts.non_generic_methods + item_counts.generic_method_templates);
    this->global_values_by_name_.reserve(item_counts.named_value_entries());
    this->method_global_values_by_name_.reserve(item_counts.non_generic_methods + item_counts.generic_method_templates);
    this->enum_cases_by_module_name_.reserve(item_counts.enum_cases);
    this->enum_cases_by_type_.reserve(item_counts.enum_items);
    this->enum_cases_by_type_and_case_.reserve(item_counts.enum_cases);
    this->visible_modules_cache_.reserve(item_counts.modules);
    this->module_export_modules_cache_.reserve(item_counts.modules);
    this->register_type_names();
    this->resolve_type_alias_decls();
    this->analyze_struct_properties();
    this->register_value_names();
    this->validate_module_namespace_conflicts();
    this->validate_function_prototypes();

    for (const auto& entry : this->generic_function_templates_) {
        this->analyze_generic_function_definition(entry.second);
    }
    for (const auto& entry : this->generic_method_templates_) {
        this->analyze_generic_function_definition(entry.second);
    }

    for (base::u32 index = 0; index < this->module_.items.size(); ++index) {
        if (this->module_.items.kind(index) != syntax::ItemKind::fn_decl) {
            continue;
        }
        const syntax::ItemNode item = this->module_.items[index];
        if (!this->has_generic_params(item) &&
            !item.is_extern_c &&
            !item.is_prototype &&
            syntax::is_valid(item.body)) {
            this->analyze_function_body(item, syntax::ItemId {index});
        }
    }

    this->analyze_entry_points();
    this->analyze_const_decls();
    this->validate_type_layouts();
    this->validate_abi_symbols();

    if (this->diagnostics_.has_error()) {
        return base::Result<CheckedModule>::fail({base::ErrorCode::sema_error, std::string(SEMA_ANALYSIS_FAILED)});
    }
    this->checked_.normalized_ast.final_expr_count = this->module_.exprs.size();
    this->checked_.normalized_ast.final_type_count = this->module_.types.size();
    this->checked_.release_analysis_only_storage();
    return base::Result<CheckedModule>::ok(std::move(this->checked_));
}

void SemanticAnalyzer::normalize_parser_only_module_contract() {
    if (!this->module_.modules.empty()) {
        this->checked_.normalized_ast.parser_only_module_contract_added = false;
        return;
    }
    syntax::ModuleInfo root;
    root.path = this->module_.module_path;
    this->module_.modules.push_back(std::move(root));
    this->module_.item_modules.assign(this->module_.items.size(), syntax::ModuleId {0});
    this->checked_.normalized_ast.parser_only_module_contract_added = true;
}

bool SemanticAnalyzer::validate_ast_contract() const
{
    bool valid = true;
    if (this->module_.item_modules.size() != this->module_.items.size()) {
        this->report_internal_contract({}, std::string(SEMA_AST_ITEM_MODULE_CONTRACT));
        valid = false;
    }
    const base::usize count = std::min(this->module_.item_modules.size(), this->module_.items.size());
    for (base::usize i = 0; i < count; ++i) {
        const syntax::ModuleId owner = this->module_.item_modules[i];
        if (!syntax::is_valid(owner) || owner.value >= this->module_.modules.size()) {
            this->report_internal_contract(
                this->module_.items.range(i),
                std::string(SEMA_AST_ITEM_MODULE_INVALID)
            );
            valid = false;
        }
    }
    return valid;
}

} // namespace aurex::sema
