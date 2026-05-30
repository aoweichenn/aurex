#include <aurex/sema/canonical_type_builder.hpp>
#include <aurex/sema/sema_messages.hpp>

#include <algorithm>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <sema/internal/sema_trait_analyzer.hpp>

namespace aurex::sema {

namespace {

constexpr std::string_view SEMA_TRAIT_INCREMENTAL_TAG = "|trait";
constexpr std::string_view SEMA_TRAIT_IMPL_INCREMENTAL_TAG = "|trait_impl";
constexpr std::string_view SEMA_TRAIT_SELF_NAME = "Self";
constexpr base::usize SEMA_TRAIT_SUBSTITUTION_STACK_INITIAL_CAPACITY = 8;
constexpr base::u64 SEMA_TRAIT_IMPL_COHERENCE_KEY_MARKER = 0x53454d4154524348ULL;

enum class TraitTypeSubstitutionActionKind {
    visit,
    build_pointer,
    build_reference,
    build_array,
    build_slice,
    build_tuple,
    build_function,
};

struct TraitTypeSubstitutionAction {
    TraitTypeSubstitutionActionKind kind = TraitTypeSubstitutionActionKind::visit;
    TypeHandle type = INVALID_TYPE_HANDLE;
    PointerMutability mutability = PointerMutability::const_;
    base::u64 array_count = 0;
    base::usize child_count = 0;
    FunctionCallConv call_conv = FunctionCallConv::aurex;
    bool is_unsafe = false;
    bool is_variadic = false;
};

[[nodiscard]] TypeHandle substitute_trait_type_iterative(TypeTable& types, const TypeHandle root,
    const std::unordered_map<GenericParamIdentity, TypeHandle, GenericParamIdentityHash>& substitutions)
{
    if (!is_valid(root) || substitutions.empty()) {
        return root;
    }

    std::vector<TraitTypeSubstitutionAction> actions;
    std::vector<TypeHandle> values;
    actions.reserve(SEMA_TRAIT_SUBSTITUTION_STACK_INITIAL_CAPACITY);
    values.reserve(SEMA_TRAIT_SUBSTITUTION_STACK_INITIAL_CAPACITY);
    actions.push_back(TraitTypeSubstitutionAction{TraitTypeSubstitutionActionKind::visit, root});

    while (!actions.empty()) {
        const TraitTypeSubstitutionAction action = actions.back();
        actions.pop_back();
        switch (action.kind) {
            case TraitTypeSubstitutionActionKind::visit:
                break;
            case TraitTypeSubstitutionActionKind::build_pointer: {
                const TypeHandle pointee = values.back();
                values.pop_back();
                values.push_back(types.pointer(action.mutability, pointee));
                continue;
            }
            case TraitTypeSubstitutionActionKind::build_reference: {
                const TypeHandle pointee = values.back();
                values.pop_back();
                values.push_back(types.reference(action.mutability, pointee));
                continue;
            }
            case TraitTypeSubstitutionActionKind::build_array: {
                const TypeHandle element = values.back();
                values.pop_back();
                values.push_back(types.array(action.array_count, element));
                continue;
            }
            case TraitTypeSubstitutionActionKind::build_slice: {
                const TypeHandle element = values.back();
                values.pop_back();
                values.push_back(types.slice(action.mutability, element));
                continue;
            }
            case TraitTypeSubstitutionActionKind::build_tuple: {
                std::vector<TypeHandle> elements(action.child_count, INVALID_TYPE_HANDLE);
                for (base::usize index = action.child_count; index > 0; --index) {
                    elements[index - 1] = values.back();
                    values.pop_back();
                }
                values.push_back(types.tuple(elements));
                continue;
            }
            case TraitTypeSubstitutionActionKind::build_function: {
                const TypeHandle return_type = values.back();
                values.pop_back();
                std::vector<TypeHandle> params(action.child_count, INVALID_TYPE_HANDLE);
                for (base::usize index = action.child_count; index > 0; --index) {
                    params[index - 1] = values.back();
                    values.pop_back();
                }
                values.push_back(
                    types.function(action.call_conv, action.is_unsafe, action.is_variadic, params, return_type));
                continue;
            }
        }

        if (!is_valid(action.type) || action.type.value >= types.size()) {
            values.push_back(action.type);
            continue;
        }
        const TypeInfo& info = types.get(action.type);
        switch (info.kind) {
            case TypeKind::generic_param: {
                const auto found = substitutions.find(info.generic_identity);
                values.push_back(found == substitutions.end() ? action.type : found->second);
                break;
            }
            case TypeKind::pointer:
                actions.push_back(TraitTypeSubstitutionAction{
                    TraitTypeSubstitutionActionKind::build_pointer,
                    INVALID_TYPE_HANDLE,
                    info.pointer_mutability,
                });
                actions.push_back(TraitTypeSubstitutionAction{TraitTypeSubstitutionActionKind::visit, info.pointee});
                break;
            case TypeKind::reference:
                actions.push_back(TraitTypeSubstitutionAction{
                    TraitTypeSubstitutionActionKind::build_reference,
                    INVALID_TYPE_HANDLE,
                    info.pointer_mutability,
                });
                actions.push_back(TraitTypeSubstitutionAction{TraitTypeSubstitutionActionKind::visit, info.pointee});
                break;
            case TypeKind::array:
                actions.push_back(TraitTypeSubstitutionAction{
                    TraitTypeSubstitutionActionKind::build_array,
                    INVALID_TYPE_HANDLE,
                    PointerMutability::const_,
                    info.array_count,
                });
                actions.push_back(
                    TraitTypeSubstitutionAction{TraitTypeSubstitutionActionKind::visit, info.array_element});
                break;
            case TypeKind::slice:
                actions.push_back(TraitTypeSubstitutionAction{
                    TraitTypeSubstitutionActionKind::build_slice,
                    INVALID_TYPE_HANDLE,
                    info.slice_mutability,
                });
                actions.push_back(
                    TraitTypeSubstitutionAction{TraitTypeSubstitutionActionKind::visit, info.slice_element});
                break;
            case TypeKind::tuple:
                actions.push_back(TraitTypeSubstitutionAction{
                    TraitTypeSubstitutionActionKind::build_tuple,
                    INVALID_TYPE_HANDLE,
                    PointerMutability::const_,
                    0,
                    info.tuple_elements.size(),
                });
                for (base::usize index = info.tuple_elements.size(); index > 0; --index) {
                    actions.push_back(TraitTypeSubstitutionAction{
                        TraitTypeSubstitutionActionKind::visit, info.tuple_elements[index - 1]});
                }
                break;
            case TypeKind::function:
                actions.push_back(TraitTypeSubstitutionAction{
                    TraitTypeSubstitutionActionKind::build_function,
                    INVALID_TYPE_HANDLE,
                    PointerMutability::const_,
                    0,
                    info.function_params.size(),
                    info.function_call_conv,
                    info.function_is_unsafe,
                    info.function_is_variadic,
                });
                actions.push_back(
                    TraitTypeSubstitutionAction{TraitTypeSubstitutionActionKind::visit, info.function_return});
                for (base::usize index = info.function_params.size(); index > 0; --index) {
                    actions.push_back(TraitTypeSubstitutionAction{
                        TraitTypeSubstitutionActionKind::visit, info.function_params[index - 1]});
                }
                break;
            case TypeKind::builtin:
            case TypeKind::struct_:
            case TypeKind::enum_:
            case TypeKind::opaque_struct:
                values.push_back(action.type);
                break;
        }
    }

    return values.back();
}

[[nodiscard]] bool trait_method_signature_shape_matches(
    const TraitMethodRequirement& requirement, const FunctionSignature& signature) noexcept
{
    return requirement.param_types.size() == signature.param_types.size()
        && requirement.is_unsafe == signature.is_unsafe && requirement.is_variadic == signature.is_variadic;
}

[[nodiscard]] base::Result<void> append_trait_impl_canonical_type(query::StableKeyWriter& writer,
    const TypeTable& types, const TypeHandle type, const CanonicalTypeKeyResolver& resolver)
{
    base::Result<query::CanonicalTypeKey> key = build_canonical_type_key(types, type, resolver);
    if (!key) {
        return base::Result<void>::fail(key.error());
    }
    query::append_stable_key(writer, key.value());
    return base::Result<void>::ok();
}

} // namespace

class SemanticAnalyzerCore::TraitAnalyzer::TraitImplCanonicalResolver final : public CanonicalTypeKeyResolver {
public:
    explicit TraitImplCanonicalResolver(const TraitAnalyzer& analyzer) : analyzer_(analyzer)
    {
    }

    [[nodiscard]] std::optional<query::DefKey> nominal_type_key(
        const TypeHandle handle, const TypeInfo& info) const override
    {
        return this->analyzer_.core_.canonical_nominal_type_query_key(handle, info);
    }

    [[nodiscard]] std::optional<query::GenericParamKey> generic_param_key(
        const TypeHandle, const TypeInfo&) const override
    {
        return std::nullopt;
    }

private:
    const TraitAnalyzer& analyzer_;
};

struct SemanticAnalyzerCore::TraitAnalyzer::TraitAnalysisScope {
    TraitAnalysisScope(SemanticAnalyzerCore& analyzer, const syntax::ModuleId module, const syntax::ItemId item,
        GenericContext* const generic_context = nullptr)
        : analyzer(analyzer), previous_module(analyzer.state_.flow.current_module),
          previous_item(analyzer.state_.flow.current_item),
          previous_generic_context(analyzer.state_.flow.current_generic_context)
    {
        this->analyzer.state_.flow.current_module = module;
        this->analyzer.state_.flow.current_item = item;
        this->analyzer.state_.flow.current_generic_context = generic_context;
    }

    TraitAnalysisScope(const TraitAnalysisScope&) = delete;
    TraitAnalysisScope& operator=(const TraitAnalysisScope&) = delete;

    ~TraitAnalysisScope()
    {
        this->analyzer.state_.flow.current_module = this->previous_module;
        this->analyzer.state_.flow.current_item = this->previous_item;
        this->analyzer.state_.flow.current_generic_context = this->previous_generic_context;
    }

    SemanticAnalyzerCore& analyzer;
    syntax::ModuleId previous_module;
    syntax::ItemId previous_item;
    GenericContext* previous_generic_context = nullptr;
};

SemanticAnalyzerCore::TraitAnalyzer::TraitAnalyzer(SemanticAnalyzerCore& core) noexcept : core_(core)
{
}

void SemanticAnalyzerCore::TraitAnalyzer::mark_trait_requirements(const syntax::ItemNode& item)
{
    for (const syntax::ItemId requirement : item.trait_items) {
        if (syntax::is_valid(requirement)) {
            this->core_.state_.traits.requirement_items.insert(requirement.value);
        }
    }
}

void SemanticAnalyzerCore::TraitAnalyzer::register_trait_name(
    const syntax::ItemNode& item, const syntax::ItemId item_id)
{
    this->mark_trait_requirements(item);
    const syntax::ModuleId owner = this->core_.item_module(item_id);
    const ModuleLookupKey key = this->core_.module_lookup_key(owner, item.name_id);
    const bool conflicts_with_type_namespace = this->core_.state_.types.named_types.contains(key)
        || this->core_.state_.checked.type_aliases.contains(key) || this->core_.state_.checked.traits.contains(key)
        || this->core_.state_.generics.struct_templates.contains(key)
        || this->core_.state_.generics.enum_templates.contains(key)
        || this->core_.state_.generics.type_alias_templates.contains(key);
    if (conflicts_with_type_namespace) {
        this->core_.report_duplicate(
            item.range, sema_duplicate_trait_definition_message(this->core_.module_name(owner), item.name));
        return;
    }

    TraitSignature signature = this->core_.state_.checked.make_trait_signature();
    signature.name = this->core_.source_name_text(item.name_id, item.name);
    signature.name_id = item.name_id;
    signature.module = owner;
    signature.item = item_id;
    signature.visibility = item.visibility;
    signature.stable_id = this->core_.stable_definition_id(owner, StableSymbolKind::type, item.name_id, item.name);
    signature.incremental_key = this->core_.stable_incremental_key(signature.stable_id,
        std::string(item.name) + std::string(SEMA_TRAIT_INCREMENTAL_TAG) + "|"
            + std::to_string(item.generic_params.size()) + "|" + std::to_string(item.trait_items.size()));
    signature.generic_params.reserve(item.generic_params.size());
    for (const syntax::GenericParamDecl& param : item.generic_params) {
        signature.generic_params.push_back(param.name_id);
    }
    signature.range = item.range;
    signature.part_index = this->core_.item_part_index(item_id);

    const auto inserted = this->core_.state_.checked.traits.emplace(key, std::move(signature));
    if (!inserted.second) {
        this->core_.report_duplicate(
            item.range, sema_duplicate_trait_definition_message(this->core_.module_name(owner), item.name));
        return;
    }
    this->core_.state_.names.traits_by_name[key] = &inserted.first->second;
}

SemanticAnalyzerCore::GenericContext SemanticAnalyzerCore::TraitAnalyzer::make_trait_generic_context(
    const syntax::ItemNode& trait)
{
    GenericContext context = this->core_.make_generic_context();
    const IdentId self_id = this->core_.ctx_.module.intern_identifier(SEMA_TRAIT_SELF_NAME);
    context.params.emplace(self_id, this->core_.state_.checked.types.generic_param(SEMA_TRAIT_SELF_NAME));
    context.param_identities.emplace(self_id, generic_param_identity_from_text(SEMA_TRAIT_SELF_NAME));
    for (const syntax::GenericParamDecl& param : trait.generic_params) {
        if (param.name_id == self_id) {
            this->core_.report_duplicate(param.range, sema_duplicate_generic_parameter_message(param.name));
            continue;
        }
        const GenericParamIdentity identity = generic_param_identity_from_text(param.name);
        context.params.emplace(param.name_id, this->core_.state_.checked.types.generic_param(identity, param.name));
        context.param_identities.emplace(param.name_id, identity);
    }
    return context;
}

void SemanticAnalyzerCore::TraitAnalyzer::resolve_trait_signature(TraitSignature& signature)
{
    if (!syntax::is_valid(signature.item) || signature.item.value >= this->core_.ctx_.module.items.size()) {
        return;
    }
    const syntax::ItemNode trait = this->core_.ctx_.module.items[signature.item.value];
    this->core_.validate_generic_parameter_list(trait);
    GenericContext context = this->make_trait_generic_context(trait);
    TraitAnalysisScope scope(this->core_, signature.module, signature.item, &context);

    std::unordered_map<IdentId, base::SourceRange, IdentIdHash> seen_requirements;
    seen_requirements.reserve(trait.trait_items.size());
    signature.requirements.clear();
    signature.requirements.reserve(trait.trait_items.size());
    for (base::u32 ordinal = 0; ordinal < trait.trait_items.size(); ++ordinal) {
        const syntax::ItemId requirement_id = trait.trait_items[ordinal];
        if (!syntax::is_valid(requirement_id) || requirement_id.value >= this->core_.ctx_.module.items.size()) {
            continue;
        }
        const syntax::ItemNode requirement = this->core_.ctx_.module.items[requirement_id.value];
        if (requirement.kind != syntax::ItemKind::fn_decl) {
            this->core_.report_internal_contract(requirement.range, "trait requirement item must be a function");
            continue;
        }
        const auto inserted = seen_requirements.emplace(requirement.name_id, requirement.range);
        if (!inserted.second) {
            this->core_.report_duplicate(
                requirement.range, sema_duplicate_trait_requirement_message(signature.name, requirement.name));
            this->core_.report_note(inserted.first->second, SemanticDiagnosticKind::duplicate,
                sema_previous_declaration_note_message(requirement.name));
            continue;
        }
        signature.requirements.push_back(
            this->resolve_trait_requirement(signature, requirement, requirement_id, ordinal));
    }
}

TraitMethodRequirement SemanticAnalyzerCore::TraitAnalyzer::resolve_trait_requirement(const TraitSignature& trait,
    const syntax::ItemNode& requirement, const syntax::ItemId requirement_id, const base::u32 ordinal)
{
    TraitMethodRequirement info = this->core_.state_.checked.make_trait_method_requirement();
    info.name = this->core_.source_name_text(requirement.name_id, requirement.name);
    info.name_id = requirement.name_id;
    info.module = trait.module;
    info.item = requirement_id;
    info.range = requirement.range;
    info.is_unsafe = requirement.is_unsafe;
    info.is_variadic = requirement.is_variadic;
    info.has_self_param = !requirement.params.empty() && requirement.params.front().name == "self";
    info.visibility = requirement.visibility;
    info.stable_key = this->core_.stable_member_key(
        trait.stable_id, StableSymbolKind::method, requirement.name_id, requirement.name, ordinal);
    info.ordinal = ordinal;
    if (!requirement.generic_params.empty()) {
        this->core_.report_unsupported(requirement.range, std::string(SEMA_GENERIC_METHODS_UNSUPPORTED));
    }
    if (!syntax::is_valid(requirement.return_type)) {
        this->core_.report_general(requirement.range, std::string(SEMA_PROTOTYPE_RETURN_TYPE_EXPLICIT));
        info.return_type = this->core_.state_.checked.types.builtin(BuiltinType::void_);
    } else {
        info.return_type = this->core_.resolve_type(requirement.return_type);
    }
    info.param_types.reserve(requirement.params.size());
    for (const syntax::ParamDecl& param : requirement.params) {
        const TypeHandle param_type = this->core_.resolve_type(param.type);
        if (!this->core_.is_valid_storage_type(param_type)) {
            this->core_.report_general(param.range, std::string(SEMA_FUNCTION_PARAMETER_STORAGE));
        }
        info.param_types.push_back(param_type);
    }
    if (is_valid(info.return_type)) {
        this->core_.validate_function_return_type(requirement, info.return_type);
    }
    return info;
}

void SemanticAnalyzerCore::TraitAnalyzer::register_trait_signatures()
{
    for (auto& entry : this->core_.state_.checked.traits) {
        this->resolve_trait_signature(entry.second);
    }
}

const TraitSignature* SemanticAnalyzerCore::TraitAnalyzer::find_trait_in_visible_modules(
    const IdentId name_id, const std::string_view name, const base::SourceRange& range, const bool report_unknown)
{
    const ModuleLookupKey key = this->core_.find_module_lookup_key(this->core_.state_.flow.current_module, name_id);
    if (is_valid(key)) {
        if (const auto found = this->core_.state_.names.traits_by_name.find(key);
            found != this->core_.state_.names.traits_by_name.end() && found->second != nullptr) {
            return found->second;
        }
    }
    if (report_unknown) {
        this->core_.report_lookup(range, sema_unknown_trait_message(name));
        this->core_.report_lookup_suggestion(range, this->core_.nearest_visible_type_name(name));
    }
    return nullptr;
}

const TraitSignature* SemanticAnalyzerCore::TraitAnalyzer::find_trait_in_module(const syntax::ModuleId module,
    const IdentId name_id, const std::string_view name, const base::SourceRange& range, const bool report_unknown)
{
    if (!syntax::is_valid(module)) {
        if (report_unknown) {
            this->core_.report_lookup(
                range, sema_unknown_trait_in_module_message(this->core_.module_name(module), name));
            this->core_.report_lookup_suggestion(range, this->core_.nearest_visible_type_name(name));
        }
        return nullptr;
    }

    const TraitSignature* result = nullptr;
    const auto consider_candidate = [&](const syntax::ModuleId candidate_module, const IdentId candidate_name_id,
                                        const std::string_view candidate_name) -> bool {
        static_cast<void>(candidate_name);
        const ModuleLookupKey key = this->core_.find_module_lookup_key(candidate_module, candidate_name_id);
        if (!is_valid(key)) {
            return false;
        }
        const auto found = this->core_.state_.names.traits_by_name.find(key);
        if (found == this->core_.state_.names.traits_by_name.end() || found->second == nullptr) {
            return false;
        }
        if (!this->core_.can_access_module(candidate_module, found->second->visibility)) {
            if (candidate_module.value == module.value && report_unknown) {
                this->core_.report_visibility(
                    range, sema_private_trait_message(this->core_.module_name(candidate_module), name));
                return true;
            }
            return false;
        }
        if (result != nullptr && result != found->second) {
            if (report_unknown) {
                this->core_.report_lookup(
                    range, sema_unknown_trait_in_module_message(this->core_.module_name(module), name));
            }
            result = nullptr;
            return true;
        }
        result = found->second;
        return false;
    };

    const auto consider_exported_modules = [&](const syntax::ModuleId exported_module, const IdentId lookup_name_id,
                                               const std::string_view lookup_name) {
        for (const syntax::ModuleId candidate_module : this->core_.accessible_module_export_modules(exported_module)) {
            if (consider_candidate(candidate_module, lookup_name_id, lookup_name)) {
                return true;
            }
        }
        return false;
    };

    if (consider_exported_modules(module, name_id, name)) {
        return nullptr;
    }
    for (const SelectiveReexportTarget& target : this->core_.accessible_selective_reexports(module, name_id, name)) {
        if (consider_exported_modules(target.module, target.name_id, target.name)) {
            return nullptr;
        }
    }
    if (result != nullptr) {
        return result;
    }
    if (report_unknown) {
        this->core_.report_lookup(range, sema_unknown_trait_in_module_message(this->core_.module_name(module), name));
        this->core_.report_lookup_suggestion(range, this->core_.nearest_type_name_in_module(module, name));
    }
    return nullptr;
}

SemanticAnalyzerCore::TraitAnalyzer::ResolvedTraitReference
SemanticAnalyzerCore::TraitAnalyzer::resolve_trait_reference(
    const syntax::ItemNode& impl_block, const syntax::ItemId impl_id)
{
    ResolvedTraitReference result;
    if (!syntax::is_valid(impl_block.trait_type)
        || impl_block.trait_type.value >= this->core_.ctx_.module.types.size()) {
        this->core_.report_lookup(impl_block.range, std::string(SEMA_TRAIT_IMPL_TARGET_NAMED_TRAIT));
        return result;
    }
    const syntax::TypeNode& trait_type = this->core_.ctx_.module.types[impl_block.trait_type.value];
    if (trait_type.kind != syntax::TypeKind::named) {
        this->core_.report_lookup(trait_type.range, std::string(SEMA_TRAIT_IMPL_TARGET_NAMED_TRAIT));
        return result;
    }

    const std::vector<std::string_view> scope_parts = this->core_.type_scope_parts(trait_type);
    const bool qualified = !scope_parts.empty();
    const TraitSignature* trait = nullptr;
    TraitAnalysisScope scope(this->core_, this->core_.item_module(impl_id), impl_id);
    if (qualified) {
        const syntax::ModuleId module = this->core_.resolve_type_scope(trait_type, true);
        trait = this->find_trait_in_module(module, trait_type.name_id, trait_type.name, trait_type.range, true);
    } else {
        trait = this->find_trait_in_visible_modules(trait_type.name_id, trait_type.name, trait_type.range, true);
    }
    if (trait == nullptr) {
        return result;
    }

    if (trait_type.type_args.empty() && !trait->generic_params.empty()) {
        this->core_.report_type(trait_type.range,
            sema_generic_argument_count_message("trait type arguments", trait->name, 0, trait->generic_params.size()));
        return result;
    }
    if (!trait_type.type_args.empty() && trait->generic_params.empty()) {
        this->core_.report_type(trait_type.range, sema_trait_not_generic_message(trait->name));
        return result;
    }
    if (trait_type.type_args.size() != trait->generic_params.size()) {
        this->core_.report_type(trait_type.range,
            sema_generic_argument_count_message(
                "trait type arguments", trait->name, trait_type.type_args.size(), trait->generic_params.size()));
        return result;
    }

    result.trait_args.reserve(trait_type.type_args.size());
    for (const syntax::TypeId arg : trait_type.type_args) {
        result.trait_args.push_back(this->core_.resolve_type(arg));
    }
    result.signature = trait;
    result.ok = true;
    return result;
}

TraitImplLookupKey SemanticAnalyzerCore::TraitAnalyzer::make_trait_impl_lookup_key(
    const TraitSignature& trait, const TypeHandle self_type, const std::span<const TypeHandle> trait_args) const
{
    query::StableHashBuilder hash;
    for (const TypeHandle arg : trait_args) {
        hash.mix_string(this->core_.state_.checked.types.display_name(arg));
    }
    return TraitImplLookupKey{
        trait.module.value,
        trait.name_id,
        self_type.value,
        hash.finish(),
    };
}

std::optional<query::StableFingerprint128> SemanticAnalyzerCore::TraitAnalyzer::make_trait_impl_coherence_fingerprint(
    const TraitSignature& trait, const TypeHandle self_type, const std::span<const TypeHandle> trait_args) const
{
    TraitImplCanonicalResolver resolver(*this);
    query::StableKeyWriter writer;
    writer.write_u64(SEMA_TRAIT_IMPL_COHERENCE_KEY_MARKER);
    query::append_stable_key(writer, trait.stable_id);
    base::Result<void> self_result =
        append_trait_impl_canonical_type(writer, this->core_.state_.checked.types, self_type, resolver);
    if (!self_result) {
        this->core_.report_internal_contract({}, self_result.error().message);
        return std::nullopt;
    }
    writer.write_u64(static_cast<base::u64>(trait_args.size()));
    for (const TypeHandle arg : trait_args) {
        base::Result<void> arg_result =
            append_trait_impl_canonical_type(writer, this->core_.state_.checked.types, arg, resolver);
        if (!arg_result) {
            this->core_.report_internal_contract({}, arg_result.error().message);
            return std::nullopt;
        }
    }
    return writer.fingerprint();
}

syntax::ModuleId SemanticAnalyzerCore::TraitAnalyzer::nominal_type_module(const TypeHandle type) const
{
    if (!is_valid(type) || type.value >= this->core_.state_.checked.types.size()) {
        return syntax::INVALID_MODULE_ID;
    }
    if (const auto found = this->core_.state_.types.struct_infos_by_type.find(type.value);
        found != this->core_.state_.types.struct_infos_by_type.end() && found->second != nullptr) {
        return found->second->module;
    }
    for (const auto& entry : this->core_.state_.checked.enum_cases) {
        if (entry.second.type.value == type.value) {
            return entry.second.module;
        }
    }
    return syntax::INVALID_MODULE_ID;
}

bool SemanticAnalyzerCore::TraitAnalyzer::trait_impl_obeys_orphan_rule(
    const TraitSignature& trait, const TypeHandle self_type, const syntax::ModuleId impl_module) const
{
    const query::PackageKey impl_package = this->core_.query_package_key(impl_module);
    const bool trait_is_local = this->core_.query_package_key(trait.module) == impl_package;
    const syntax::ModuleId self_module = this->nominal_type_module(self_type);
    const bool self_is_local =
        syntax::is_valid(self_module) && this->core_.query_package_key(self_module) == impl_package;
    return trait_is_local || self_is_local;
}

const TraitImplInfo* SemanticAnalyzerCore::TraitAnalyzer::find_overlapping_trait_impl(
    const query::StableFingerprint128 coherence_fingerprint) const
{
    for (const auto& entry : this->core_.state_.checked.trait_impls) {
        if (entry.second.coherence_fingerprint == coherence_fingerprint) {
            return &entry.second;
        }
    }
    return nullptr;
}

base::u32 SemanticAnalyzerCore::TraitAnalyzer::record_trait_impl_predicate(
    const TraitSignature& trait, TraitImplInfo& impl_info, const query::StableFingerprint128 fingerprint) const
{
    TraitPredicate predicate = this->core_.state_.checked.make_trait_predicate();
    predicate.index = static_cast<base::u32>(this->core_.state_.checked.trait_predicates.size());
    predicate.kind = TraitPredicateKind::declared_trait;
    predicate.origin = TraitPredicateOrigin::explicit_impl;
    predicate.subject_type = impl_info.self_type;
    predicate.trait_name = this->core_.state_.checked.intern_text(trait.name);
    predicate.trait_name_id = trait.name_id;
    predicate.trait_module = trait.module;
    predicate.trait_stable_id = trait.stable_id;
    predicate.trait_args = this->core_.state_.checked.copy_type_handle_list(impl_info.trait_args);
    predicate.canonical_fingerprint = fingerprint;
    predicate.module = impl_info.module;
    predicate.item = impl_info.item;
    predicate.range = impl_info.range;
    predicate.part_index = impl_info.part_index;
    this->core_.state_.checked.trait_predicates.push_back(std::move(predicate));
    return static_cast<base::u32>(this->core_.state_.checked.trait_predicates.size() - 1U);
}

void SemanticAnalyzerCore::TraitAnalyzer::record_trait_impl_evidence(const TraitImplInfo& impl_info) const
{
    TraitEvidence evidence = this->core_.state_.checked.make_trait_evidence();
    evidence.kind = TraitEvidenceKind::explicit_impl;
    evidence.predicate_index = impl_info.predicate_index;
    evidence.predicate_fingerprint = impl_info.coherence_fingerprint;
    evidence.impl_key = impl_info.key;
    evidence.module = impl_info.module;
    evidence.item = impl_info.item;
    evidence.range = impl_info.range;
    evidence.part_index = impl_info.part_index;
    this->core_.state_.checked.trait_evidence.push_back(evidence);
}

const TraitMethodRequirement* SemanticAnalyzerCore::TraitAnalyzer::find_trait_method_requirement(
    const TraitSignature& trait, const IdentId name_id, const bool require_self) const
{
    for (const TraitMethodRequirement& requirement : trait.requirements) {
        if (requirement.name_id == name_id && (!require_self || requirement.has_self_param)) {
            return &requirement;
        }
    }
    return nullptr;
}

SemanticAnalyzerCore::TraitMethodCallResolution
SemanticAnalyzerCore::TraitAnalyzer::make_param_env_trait_method_resolution(const TraitSignature& trait,
    const TraitPredicate& predicate, const TraitMethodRequirement& requirement, const TypeHandle owner_type) const
{
    TraitMethodCallResolution resolution;
    resolution.trait = &trait;
    resolution.requirement = &requirement;
    resolution.predicate = &predicate;
    resolution.return_type =
        this->substitute_requirement_type(requirement.return_type, owner_type, predicate.trait_args, trait);
    resolution.param_types.reserve(requirement.param_types.size());
    for (const TypeHandle param : requirement.param_types) {
        resolution.param_types.push_back(
            this->substitute_requirement_type(param, owner_type, predicate.trait_args, trait));
    }
    resolution.from_param_env = true;
    resolution.found = true;
    return resolution;
}

SemanticAnalyzerCore::TraitMethodCallResolution SemanticAnalyzerCore::TraitAnalyzer::make_impl_trait_method_resolution(
    const TraitSignature& trait, const TraitImplInfo& impl, const TraitMethodRequirement& requirement,
    const FunctionSignature& signature) const
{
    TraitMethodCallResolution resolution;
    resolution.trait = &trait;
    resolution.requirement = &requirement;
    resolution.impl = &impl;
    if (impl.predicate_index < this->core_.state_.checked.trait_predicates.size()) {
        resolution.predicate = &this->core_.state_.checked.trait_predicates[impl.predicate_index];
    }
    resolution.signature = &signature;
    resolution.return_type = signature.return_type;
    resolution.param_types.assign(signature.param_types.begin(), signature.param_types.end());
    resolution.found = true;
    return resolution;
}

const TraitSignature* SemanticAnalyzerCore::TraitAnalyzer::trait_signature_for_impl(const TraitImplInfo& impl) const
{
    const auto found = this->core_.state_.checked.traits.find(ModuleLookupKey{
        impl.trait_module.value,
        impl.trait_name_id,
    });
    return found == this->core_.state_.checked.traits.end() ? nullptr : &found->second;
}

bool SemanticAnalyzerCore::TraitAnalyzer::trait_visible_for_method_call(
    const TraitSignature& trait, const base::SourceRange& range)
{
    const TraitSignature* const visible = this->find_trait_in_visible_modules(trait.name_id, trait.name, range, false);
    return visible != nullptr && visible->stable_id == trait.stable_id;
}

bool SemanticAnalyzerCore::TraitAnalyzer::visible_trait_has_method(
    const IdentId name_id, const bool require_self, const base::SourceRange& range)
{
    for (const auto& entry : this->core_.state_.checked.traits) {
        const TraitSignature& trait = entry.second;
        if (!this->trait_visible_for_method_call(trait, range)) {
            continue;
        }
        if (this->find_trait_method_requirement(trait, name_id, require_self) != nullptr) {
            return true;
        }
    }
    return false;
}

SemanticAnalyzerCore::TraitMethodCallResolution
SemanticAnalyzerCore::TraitAnalyzer::resolve_param_env_trait_method_call(const TypeHandle owner_type,
    const IdentId name_id, const std::string_view name, const base::SourceRange& range, const bool require_self,
    const bool report_failure)
{
    TraitMethodCallResolution result;
    if (this->core_.state_.flow.current_generic_context == nullptr || !is_valid(owner_type)
        || owner_type.value >= this->core_.state_.checked.types.size()) {
        return result;
    }
    const TypeInfo& owner = this->core_.state_.checked.types.get(owner_type);
    if (owner.kind != TypeKind::generic_param || !is_valid(owner.generic_identity)) {
        return result;
    }

    std::string first_trait_name;
    for (const base::u32 predicate_index : this->core_.state_.flow.current_generic_context->predicate_indices) {
        if (predicate_index >= this->core_.state_.checked.trait_predicates.size()) {
            continue;
        }
        const TraitPredicate& predicate = this->core_.state_.checked.trait_predicates[predicate_index];
        if (predicate.kind != TraitPredicateKind::declared_trait
            || predicate.subject_param_identity != owner.generic_identity) {
            continue;
        }
        const auto trait_found = this->core_.state_.checked.traits.find(ModuleLookupKey{
            predicate.trait_module.value,
            predicate.trait_name_id,
        });
        if (trait_found == this->core_.state_.checked.traits.end()) {
            continue;
        }
        const TraitSignature& trait = trait_found->second;
        const TraitMethodRequirement* const requirement =
            this->find_trait_method_requirement(trait, name_id, require_self);
        if (requirement == nullptr) {
            continue;
        }
        if (result.found) {
            TraitMethodCallResolution failure;
            failure.reported_failure = report_failure;
            if (report_failure) {
                const std::string second_trait_name = this->trait_display_name(trait, predicate.trait_args);
                this->core_.report_lookup(range,
                    sema_ambiguous_trait_method_message(this->core_.state_.checked.types.display_name(owner_type), name,
                        first_trait_name, second_trait_name));
            }
            return failure;
        }
        first_trait_name = this->trait_display_name(trait, predicate.trait_args);
        result = this->make_param_env_trait_method_resolution(trait, predicate, *requirement, owner_type);
    }

    if (!result.found && report_failure && this->visible_trait_has_method(name_id, require_self, range)) {
        this->core_.report_capability(range,
            sema_trait_method_missing_bound_message(this->core_.state_.checked.types.display_name(owner_type), name));
        result.reported_failure = true;
    }
    return result;
}

SemanticAnalyzerCore::TraitMethodCallResolution SemanticAnalyzerCore::TraitAnalyzer::resolve_impl_trait_method_call(
    const TypeHandle owner_type, const IdentId name_id, const std::string_view name, const base::SourceRange& range,
    const bool require_self, const bool report_failure)
{
    TraitMethodCallResolution result;
    bool saw_visible_trait_method = false;
    std::string first_trait_name;
    for (const auto& entry : this->core_.state_.checked.trait_impls) {
        const TraitImplInfo& impl = entry.second;
        if (!this->core_.state_.checked.types.same(impl.self_type, owner_type)) {
            continue;
        }
        const TraitSignature* const trait = this->trait_signature_for_impl(impl);
        if (trait == nullptr || !this->trait_visible_for_method_call(*trait, range)) {
            continue;
        }
        const TraitMethodRequirement* const requirement =
            this->find_trait_method_requirement(*trait, name_id, require_self);
        if (requirement == nullptr) {
            continue;
        }
        saw_visible_trait_method = true;
        const auto method = std::ranges::find_if(impl.methods, [&](const TraitImplMethodInfo& candidate) {
            return candidate.name_id == name_id;
        });
        if (method == impl.methods.end()) {
            continue;
        }
        const auto signature = this->core_.state_.checked.functions.find(method->function_key);
        if (signature == this->core_.state_.checked.functions.end()) {
            continue;
        }
        if (require_self && !signature->second.has_self_param) {
            continue;
        }
        if (result.found) {
            TraitMethodCallResolution failure;
            failure.reported_failure = report_failure;
            if (report_failure) {
                const std::string second_trait_name = this->trait_display_name(*trait, impl.trait_args);
                this->core_.report_lookup(range,
                    sema_ambiguous_trait_method_message(this->core_.state_.checked.types.display_name(owner_type), name,
                        first_trait_name, second_trait_name));
            }
            return failure;
        }
        first_trait_name = this->trait_display_name(*trait, impl.trait_args);
        result = this->make_impl_trait_method_resolution(*trait, impl, *requirement, signature->second);
    }
    if (!result.found && report_failure
        && (saw_visible_trait_method || this->visible_trait_has_method(name_id, require_self, range))) {
        this->core_.report_capability(range,
            sema_trait_method_impl_missing_message(this->core_.state_.checked.types.display_name(owner_type), name));
        result.reported_failure = true;
    }
    return result;
}

SemanticAnalyzerCore::TraitMethodCallResolution SemanticAnalyzerCore::TraitAnalyzer::resolve_trait_method_call(
    const TypeHandle owner_type, const IdentId name_id, const std::string_view name, const base::SourceRange& range,
    const bool require_self, const bool report_failure)
{
    if (!is_valid(owner_type) || owner_type.value >= this->core_.state_.checked.types.size()) {
        return {};
    }
    TraitMethodCallResolution resolution =
        this->resolve_param_env_trait_method_call(owner_type, name_id, name, range, require_self, false);
    if (resolution.found) {
        return resolution;
    }
    resolution = this->resolve_impl_trait_method_call(owner_type, name_id, name, range, require_self, false);
    if (resolution.found) {
        return resolution;
    }
    if (report_failure) {
        if (this->core_.state_.checked.types.get(owner_type).kind == TypeKind::generic_param) {
            return this->resolve_param_env_trait_method_call(owner_type, name_id, name, range, require_self, true);
        } else {
            return this->resolve_impl_trait_method_call(owner_type, name_id, name, range, require_self, true);
        }
    }
    return {};
}

std::string SemanticAnalyzerCore::TraitAnalyzer::trait_display_name(
    const TraitSignature& trait, const std::span<const TypeHandle> trait_args) const
{
    return this->core_.state_.checked.types.display_name(trait.name, trait_args);
}

TypeHandle SemanticAnalyzerCore::TraitAnalyzer::substitute_requirement_type(const TypeHandle type,
    const TypeHandle self_type, const std::span<const TypeHandle> trait_args, const TraitSignature& trait) const
{
    std::unordered_map<GenericParamIdentity, TypeHandle, GenericParamIdentityHash> substitutions;
    substitutions.reserve(trait_args.size() + 1U);
    substitutions.emplace(generic_param_identity_from_text(SEMA_TRAIT_SELF_NAME), self_type);
    for (base::usize index = 0; index < trait_args.size() && index < trait.generic_params.size(); ++index) {
        const std::string_view name = this->core_.ctx_.module.identifier_text(trait.generic_params[index]);
        substitutions.emplace(generic_param_identity_from_text(name), trait_args[index]);
    }
    return substitute_trait_type_iterative(this->core_.state_.checked.types, type, substitutions);
}

bool SemanticAnalyzerCore::TraitAnalyzer::trait_impl_method_matches(const TraitMethodRequirement& requirement,
    const FunctionSignature& signature, const TypeHandle self_type, const std::span<const TypeHandle> trait_args,
    const TraitSignature& trait) const
{
    if (!trait_method_signature_shape_matches(requirement, signature)) {
        return false;
    }
    const TypeHandle expected_return =
        this->substitute_requirement_type(requirement.return_type, self_type, trait_args, trait);
    if (!this->core_.state_.checked.types.same(expected_return, signature.return_type)) {
        return false;
    }
    for (base::usize index = 0; index < requirement.param_types.size(); ++index) {
        const TypeHandle expected_param =
            this->substitute_requirement_type(requirement.param_types[index], self_type, trait_args, trait);
        if (!this->core_.state_.checked.types.same(expected_param, signature.param_types[index])) {
            return false;
        }
    }
    return true;
}

void SemanticAnalyzerCore::TraitAnalyzer::validate_trait_impl_block(
    const syntax::ItemNode& impl_block, const syntax::ItemId impl_id)
{
    if (!impl_block.generic_params.empty() || !impl_block.where_constraints.empty()) {
        this->core_.report_unsupported(impl_block.range, std::string(SEMA_TRAIT_IMPL_GENERIC_UNSUPPORTED));
        return;
    }

    const ResolvedTraitReference trait_reference = this->resolve_trait_reference(impl_block, impl_id);
    if (!trait_reference.ok || trait_reference.signature == nullptr) {
        return;
    }
    const TraitSignature& trait = *trait_reference.signature;
    TraitAnalysisScope scope(this->core_, this->core_.item_module(impl_id), impl_id);

    if (!syntax::is_valid(impl_block.impl_type) || impl_block.impl_type.value >= this->core_.ctx_.module.types.size()
        || this->core_.ctx_.module.types[impl_block.impl_type.value].kind != syntax::TypeKind::named) {
        this->core_.report_general(impl_block.range, std::string(SEMA_IMPL_TARGET_NAMED_TYPE));
        return;
    }
    const TypeHandle self_type = this->core_.resolve_type(impl_block.impl_type);
    if (!is_valid(self_type)) {
        return;
    }
    const TypeKind self_kind = this->core_.state_.checked.types.get(self_type).kind;
    if (self_kind != TypeKind::struct_ && self_kind != TypeKind::enum_ && self_kind != TypeKind::opaque_struct) {
        this->core_.report_general(impl_block.range, std::string(SEMA_IMPL_TARGET_NAMED_TYPE));
        return;
    }

    const TraitImplLookupKey impl_key = this->make_trait_impl_lookup_key(trait, self_type, trait_reference.trait_args);
    const std::string trait_name = this->trait_display_name(trait, trait_reference.trait_args);
    const std::string self_name = this->core_.state_.checked.types.display_name(self_type);
    const syntax::ModuleId impl_module = this->core_.item_module(impl_id);
    if (!this->trait_impl_obeys_orphan_rule(trait, self_type, impl_module)) {
        this->core_.report_type(impl_block.range,
            sema_trait_impl_orphan_rule_message(trait_name, self_name, this->core_.module_name(impl_module)));
        return;
    }
    if (this->core_.state_.checked.trait_impls.contains(impl_key)) {
        this->core_.report_duplicate(impl_block.range, sema_duplicate_trait_impl_message(trait_name, self_name));
        return;
    }
    const std::optional<query::StableFingerprint128> coherence_fingerprint =
        this->make_trait_impl_coherence_fingerprint(trait, self_type, trait_reference.trait_args);
    if (!coherence_fingerprint.has_value()) {
        return;
    }
    if (const TraitImplInfo* const overlap = this->find_overlapping_trait_impl(*coherence_fingerprint);
        overlap != nullptr) {
        this->core_.report_duplicate(impl_block.range, sema_overlapping_trait_impl_message(trait_name, self_name));
        this->core_.report_note(overlap->range, SemanticDiagnosticKind::duplicate,
            sema_previous_trait_impl_note_message(this->trait_display_name(trait, overlap->trait_args),
                this->core_.state_.checked.types.display_name(overlap->self_type)));
        return;
    }

    std::unordered_map<IdentId, const TraitMethodRequirement*, IdentIdHash> requirements_by_name;
    requirements_by_name.reserve(trait.requirements.size());
    for (const TraitMethodRequirement& requirement : trait.requirements) {
        requirements_by_name.emplace(requirement.name_id, &requirement);
    }
    std::unordered_map<IdentId, base::SourceRange, IdentIdHash> seen_methods;
    seen_methods.reserve(impl_block.impl_items.size());
    std::unordered_set<IdentId, IdentIdHash> implemented;
    implemented.reserve(impl_block.impl_items.size());

    TraitImplInfo impl_info = this->core_.state_.checked.make_trait_impl_info();
    impl_info.key = impl_key;
    impl_info.trait_name = this->core_.state_.checked.intern_text(trait.name);
    impl_info.trait_name_id = trait.name_id;
    impl_info.trait_module = trait.module;
    impl_info.self_type = self_type;
    impl_info.trait_args = this->core_.state_.checked.copy_type_handle_list(trait_reference.trait_args);
    impl_info.coherence_fingerprint = *coherence_fingerprint;
    impl_info.item = impl_id;
    impl_info.module = impl_module;
    impl_info.visibility = impl_block.visibility;
    const std::string impl_fingerprint =
        trait_name + " for " + self_name + std::string(SEMA_TRAIT_IMPL_INCREMENTAL_TAG);
    impl_info.stable_id = sema::stable_definition_id(
        this->core_.stable_module_id(impl_info.module), StableSymbolKind::synthetic, impl_fingerprint);
    impl_info.incremental_key = this->core_.stable_incremental_key(impl_info.stable_id, impl_fingerprint);
    impl_info.range = impl_block.range;
    impl_info.part_index = this->core_.item_part_index(impl_id);
    impl_info.methods.reserve(impl_block.impl_items.size());
    impl_info.predicate_index = this->record_trait_impl_predicate(trait, impl_info, *coherence_fingerprint);

    for (const syntax::ItemId method_id : impl_block.impl_items) {
        if (!syntax::is_valid(method_id) || method_id.value >= this->core_.ctx_.module.items.size()) {
            continue;
        }
        const syntax::ItemNode method = this->core_.ctx_.module.items[method_id.value];
        if (method.kind != syntax::ItemKind::fn_decl) {
            continue;
        }
        const auto inserted = seen_methods.emplace(method.name_id, method.range);
        if (!inserted.second) {
            this->core_.report_duplicate(
                method.range, sema_duplicate_trait_impl_method_message(trait_name, self_name, method.name));
            this->core_.report_note(inserted.first->second, SemanticDiagnosticKind::duplicate,
                sema_previous_declaration_note_message(method.name));
            continue;
        }
        const auto requirement = requirements_by_name.find(method.name_id);
        if (requirement == requirements_by_name.end() || requirement->second == nullptr) {
            this->core_.report_lookup(
                method.range, sema_trait_impl_unknown_method_message(trait_name, self_name, method.name));
            continue;
        }
        const FunctionLookupKey function_key = this->core_.function_key(method, method_id);
        const auto signature = this->core_.state_.checked.functions.find(function_key);
        if (signature == this->core_.state_.checked.functions.end()) {
            continue;
        }
        if (!this->trait_impl_method_matches(
                *requirement->second, signature->second, self_type, trait_reference.trait_args, trait)) {
            this->core_.report_type(
                method.range, sema_trait_impl_method_signature_message(trait_name, self_name, method.name));
            continue;
        }
        implemented.insert(method.name_id);
        TraitImplMethodInfo method_info = this->core_.state_.checked.make_trait_impl_method_info();
        method_info.name = this->core_.source_name_text(method.name_id, method.name);
        method_info.name_id = method.name_id;
        method_info.item = method_id;
        method_info.function_key = function_key;
        method_info.requirement_ordinal = requirement->second->ordinal;
        impl_info.methods.push_back(method_info);
    }

    for (const TraitMethodRequirement& requirement : trait.requirements) {
        if (!implemented.contains(requirement.name_id)) {
            this->core_.report_type(
                impl_block.range, sema_trait_impl_missing_method_message(trait_name, self_name, requirement.name));
        }
    }

    this->record_trait_impl_evidence(impl_info);
    this->core_.state_.checked.trait_impls.emplace(impl_key, std::move(impl_info));
}

void SemanticAnalyzerCore::TraitAnalyzer::validate_trait_impls()
{
    for (base::u32 item_index = 0; item_index < this->core_.ctx_.module.items.size(); ++item_index) {
        const syntax::ItemNode item = this->core_.ctx_.module.items[item_index];
        if (item.kind == syntax::ItemKind::impl_block && syntax::is_valid(item.trait_type)) {
            this->validate_trait_impl_block(item, syntax::ItemId{item_index});
        }
    }
}

void SemanticAnalyzerCore::register_trait_name(const syntax::ItemNode& item, const syntax::ItemId item_id)
{
    TraitAnalyzer(*this).register_trait_name(item, item_id);
}

void SemanticAnalyzerCore::register_trait_signatures()
{
    TraitAnalyzer(*this).register_trait_signatures();
}

void SemanticAnalyzerCore::validate_trait_impls()
{
    TraitAnalyzer(*this).validate_trait_impls();
}

const TraitSignature* SemanticAnalyzerCore::find_trait_in_visible_modules(
    const IdentId name_id, const std::string_view name, const base::SourceRange& range, const bool report_unknown)
{
    return TraitAnalyzer(*this).find_trait_in_visible_modules(name_id, name, range, report_unknown);
}

SemanticAnalyzerCore::TraitMethodCallResolution SemanticAnalyzerCore::resolve_trait_method_call(
    const TypeHandle owner_type, const IdentId name_id, const std::string_view name, const base::SourceRange& range,
    const bool require_self, const bool report_failure)
{
    return TraitAnalyzer(*this).resolve_trait_method_call(
        owner_type, name_id, name, range, require_self, report_failure);
}

bool SemanticAnalyzerCore::is_trait_requirement_item(const syntax::ItemId item) const
{
    return syntax::is_valid(item) && this->state_.traits.requirement_items.contains(item.value);
}

} // namespace aurex::sema
