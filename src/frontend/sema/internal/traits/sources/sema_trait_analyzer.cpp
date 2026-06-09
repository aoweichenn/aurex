#include <aurex/frontend/sema/canonical_type_builder.hpp>
#include <aurex/frontend/sema/sema_messages.hpp>

#include <algorithm>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <frontend/sema/internal/borrow/private/contract.hpp>
#include <frontend/sema/internal/traits/private/sema_trait_analyzer.hpp>

namespace aurex::sema {

namespace {

constexpr std::string_view SEMA_TRAIT_INCREMENTAL_TAG = "|trait";
constexpr std::string_view SEMA_TRAIT_IMPL_INCREMENTAL_TAG = "|trait_impl";
constexpr std::string_view SEMA_TRAIT_SELF_NAME = "Self";
constexpr std::string_view SEMA_TRAIT_DEFAULT_INSTANCE_KEY_PREFIX = "trait_default:";
constexpr std::string_view SEMA_TRAIT_DEFAULT_INSTANCE_STABLE_PREFIX = "trait-default:";
constexpr std::string_view SEMA_TRAIT_DEFAULT_OVERRIDE_SIGNATURE_NOTE =
    "trait method has a default body, but an explicit override must still match the requirement signature";
constexpr std::string_view SEMA_RESERVED_DROP_TRAIT_NAME = "Drop";
constexpr base::usize SEMA_TRAIT_SUBSTITUTION_STACK_INITIAL_CAPACITY = 8;
constexpr std::string_view SEMA_TRAIT_PREDICATE_ID_CONTEXT = "semantic trait predicate id";
constexpr base::u64 SEMA_TRAIT_IMPL_COHERENCE_KEY_MARKER = 0x53454d4154524348ULL;
constexpr base::u64 SEMA_TRAIT_SELF_PREDICATE_KEY_MARKER = 0x53454d4154525346ULL;
constexpr std::string_view SEMA_DYN_TRAIT_SLOT_SCHEMA_TAG = "dyn-trait-slot-schema:v1";
constexpr base::u32 SEMA_DYN_TRAIT_RECEIVER_PARAM_COUNT = 1;
constexpr std::string_view SEMA_SUPERTRAIT_EDGE_FINGERPRINT_TAG = "supertrait-edge:v1";
constexpr std::string_view SEMA_SUPERTRAIT_ENDPOINT_FINGERPRINT_TAG = "supertrait-endpoint:v1";

enum class TraitTypeSubstitutionActionKind {
    visit,
    build_pointer,
    build_reference,
    build_array,
    build_slice,
    build_tuple,
    build_function,
    build_associated_projection,
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
    query::MemberKey associated_member;
    InternedText associated_name;
    InternedText reference_origin_key;
};

[[nodiscard]] bool is_type_generic_param(const syntax::GenericParamDecl& param) noexcept
{
    return param.kind == syntax::GenericParamKind::type;
}

[[nodiscard]] base::usize type_generic_param_count(const std::vector<syntax::GenericParamDecl>& params) noexcept
{
    return static_cast<base::usize>(std::ranges::count_if(params, is_type_generic_param));
}

[[nodiscard]] TraitTypeSubstitutionAction trait_substitution_visit(const TypeHandle type) noexcept
{
    TraitTypeSubstitutionAction action;
    action.kind = TraitTypeSubstitutionActionKind::visit;
    action.type = type;
    return action;
}

[[nodiscard]] TraitTypeSubstitutionAction trait_substitution_unary(const TraitTypeSubstitutionActionKind kind,
    const PointerMutability mutability, const InternedText reference_origin_key = {}) noexcept
{
    TraitTypeSubstitutionAction action;
    action.kind = kind;
    action.mutability = mutability;
    action.reference_origin_key = reference_origin_key;
    return action;
}

[[nodiscard]] TraitTypeSubstitutionAction trait_substitution_array(const base::u64 count) noexcept
{
    TraitTypeSubstitutionAction action;
    action.kind = TraitTypeSubstitutionActionKind::build_array;
    action.array_count = count;
    return action;
}

[[nodiscard]] TraitTypeSubstitutionAction trait_substitution_tuple(const base::usize child_count) noexcept
{
    TraitTypeSubstitutionAction action;
    action.kind = TraitTypeSubstitutionActionKind::build_tuple;
    action.child_count = child_count;
    return action;
}

[[nodiscard]] TraitTypeSubstitutionAction trait_substitution_function(const base::usize child_count,
    const FunctionCallConv call_conv, const bool is_unsafe, const bool is_variadic) noexcept
{
    TraitTypeSubstitutionAction action;
    action.kind = TraitTypeSubstitutionActionKind::build_function;
    action.child_count = child_count;
    action.call_conv = call_conv;
    action.is_unsafe = is_unsafe;
    action.is_variadic = is_variadic;
    return action;
}

[[nodiscard]] TraitTypeSubstitutionAction trait_substitution_associated_projection(
    const query::MemberKey member, const InternedText name) noexcept
{
    TraitTypeSubstitutionAction action;
    action.kind = TraitTypeSubstitutionActionKind::build_associated_projection;
    action.associated_member = member;
    action.associated_name = name;
    return action;
}

[[nodiscard]] TypeHandle substitute_trait_type_iterative(TypeTable& types, const TypeHandle root,
    const std::unordered_map<GenericParamIdentity, TypeHandle, GenericParamIdentityHash>& substitutions,
    const std::span<const TraitImplAssociatedTypeInfo> associated_types)
{
    if (!is_valid(root) || (substitutions.empty() && associated_types.empty())) {
        return root;
    }

    std::vector<TraitTypeSubstitutionAction> actions;
    std::vector<TypeHandle> values;
    actions.reserve(SEMA_TRAIT_SUBSTITUTION_STACK_INITIAL_CAPACITY);
    values.reserve(SEMA_TRAIT_SUBSTITUTION_STACK_INITIAL_CAPACITY);
    actions.push_back(trait_substitution_visit(root));

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
                values.push_back(
                    types.reference_with_origin_key(action.mutability, pointee, action.reference_origin_key.view()));
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
            case TraitTypeSubstitutionActionKind::build_associated_projection: {
                const TypeHandle base = values.back();
                values.pop_back();
                const auto found =
                    std::ranges::find_if(associated_types, [&](const TraitImplAssociatedTypeInfo& associated_type) {
                        return associated_type.member_key == action.associated_member;
                    });
                values.push_back(found == associated_types.end()
                        ? types.associated_projection(base, action.associated_member, action.associated_name)
                        : found->value_type);
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
                actions.push_back(
                    trait_substitution_unary(TraitTypeSubstitutionActionKind::build_pointer, info.pointer_mutability));
                actions.push_back(trait_substitution_visit(info.pointee));
                break;
            case TypeKind::reference:
                actions.push_back(trait_substitution_unary(TraitTypeSubstitutionActionKind::build_reference,
                    info.pointer_mutability, info.reference_origin_key));
                actions.push_back(trait_substitution_visit(info.pointee));
                break;
            case TypeKind::array:
                actions.push_back(trait_substitution_array(info.array_count));
                actions.push_back(trait_substitution_visit(info.array_element));
                break;
            case TypeKind::slice:
                actions.push_back(
                    trait_substitution_unary(TraitTypeSubstitutionActionKind::build_slice, info.slice_mutability));
                actions.push_back(trait_substitution_visit(info.slice_element));
                break;
            case TypeKind::tuple:
                actions.push_back(trait_substitution_tuple(info.tuple_elements.size()));
                for (base::usize index = info.tuple_elements.size(); index > 0; --index) {
                    actions.push_back(trait_substitution_visit(info.tuple_elements[index - 1]));
                }
                break;
            case TypeKind::function:
                actions.push_back(trait_substitution_function(info.function_params.size(), info.function_call_conv,
                    info.function_is_unsafe, info.function_is_variadic));
                actions.push_back(trait_substitution_visit(info.function_return));
                for (base::usize index = info.function_params.size(); index > 0; --index) {
                    actions.push_back(trait_substitution_visit(info.function_params[index - 1]));
                }
                break;
            case TypeKind::associated_projection:
                actions.push_back(trait_substitution_associated_projection(info.associated_member, info.name));
                actions.push_back(trait_substitution_visit(info.associated_base));
                break;
            case TypeKind::builtin:
            case TypeKind::struct_:
            case TypeKind::enum_:
            case TypeKind::opaque_struct:
            case TypeKind::trait_object:
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

void append_checked_canonical_type_or_display(SemanticAnalyzerCore& core,
    query::StableKeyWriter& writer,
    const TypeHandle type)
{
    base::Result<query::CanonicalTypeKey> key = core.checked_canonical_type_key(type);
    writer.write_bool(static_cast<bool>(key));
    if (key) {
        query::append_stable_key(writer, key.value());
    } else {
        writer.write_string(core.state_.checked.types.display_name(type));
    }
}

void append_supertrait_args_to_writer(SemanticAnalyzerCore& core,
    query::StableKeyWriter& writer,
    const std::span<const TypeHandle> args)
{
    writer.write_u64(static_cast<base::u64>(args.size()));
    for (const TypeHandle arg : args) {
        append_checked_canonical_type_or_display(core, writer, arg);
    }
}

[[nodiscard]] query::StableFingerprint128 make_supertrait_endpoint_fingerprint(SemanticAnalyzerCore& core,
    const query::DefKey child_trait,
    const query::DefKey parent_trait,
    const std::span<const TypeHandle> parent_args)
{
    query::StableKeyWriter writer;
    writer.write_string(SEMA_SUPERTRAIT_ENDPOINT_FINGERPRINT_TAG);
    query::append_stable_key(writer, child_trait);
    query::append_stable_key(writer, parent_trait);
    append_supertrait_args_to_writer(core, writer, parent_args);
    return writer.fingerprint();
}

[[nodiscard]] query::StableFingerprint128 make_supertrait_edge_fingerprint(SemanticAnalyzerCore& core,
    const query::DefKey child_trait,
    const query::DefKey parent_trait,
    const base::u32 direct_edge_ordinal,
    const base::u32 closure_depth,
    const std::span<const TypeHandle> parent_args)
{
    query::StableKeyWriter writer;
    writer.write_string(SEMA_SUPERTRAIT_EDGE_FINGERPRINT_TAG);
    query::append_stable_key(writer, child_trait);
    query::append_stable_key(writer, parent_trait);
    writer.write_u32(direct_edge_ordinal);
    writer.write_u32(closure_depth);
    append_supertrait_args_to_writer(core, writer, parent_args);
    return writer.fingerprint();
}

[[nodiscard]] bool dyn_trait_type_is_self_param(const TypeTable& types, const TypeHandle type) noexcept
{
    if (!is_valid(type) || type.value >= types.size()) {
        return false;
    }
    const TypeInfo& info = types.get(type);
    return info.kind == TypeKind::generic_param
        && info.generic_identity == generic_param_identity_from_text(SEMA_TRAIT_SELF_NAME);
}

[[nodiscard]] bool dyn_trait_receiver_access(
    const TypeTable& types, const TypeHandle type, ReceiverAccessKind& access) noexcept
{
    if (!is_valid(type) || type.value >= types.size()) {
        return false;
    }
    const TypeInfo& info = types.get(type);
    if (info.kind != TypeKind::reference || !dyn_trait_type_is_self_param(types, info.pointee)) {
        return false;
    }
    access =
        info.pointer_mutability == PointerMutability::mut ? ReceiverAccessKind::mutable_ : ReceiverAccessKind::shared;
    return true;
}

[[nodiscard]] bool dyn_trait_associated_projection_is_constrained_self(
    const TypeTable& types,
    const TypeInfo& info,
    const std::span<const TraitImplAssociatedTypeInfo> associated_equalities)
{
    return dyn_trait_type_is_self_param(types, info.associated_base)
        && std::ranges::any_of(associated_equalities, [&](const TraitImplAssociatedTypeInfo& equality) {
               return equality.member_key == info.associated_member;
           });
}

[[nodiscard]] bool dyn_trait_type_has_unconstrained_self(
    const TypeTable& types,
    const TypeHandle type,
    const std::span<const TraitImplAssociatedTypeInfo> associated_equalities)
{
    std::vector<TypeHandle> pending;
    std::unordered_set<base::u32> visited;
    pending.push_back(type);
    while (!pending.empty()) {
        const TypeHandle current = pending.back();
        pending.pop_back();
        if (!is_valid(current) || current.value >= types.size() || !visited.insert(current.value).second) {
            continue;
        }
        const TypeInfo& info = types.get(current);
        switch (info.kind) {
            case TypeKind::generic_param:
                if (info.generic_identity == generic_param_identity_from_text(SEMA_TRAIT_SELF_NAME)) {
                    return true;
                }
                break;
            case TypeKind::pointer:
            case TypeKind::reference:
                pending.push_back(info.pointee);
                break;
            case TypeKind::slice:
                pending.push_back(info.slice_element);
                break;
            case TypeKind::array:
                pending.push_back(info.array_element);
                break;
            case TypeKind::tuple:
                for (const TypeHandle element : info.tuple_elements) {
                    pending.push_back(element);
                }
                break;
            case TypeKind::function:
                pending.push_back(info.function_return);
                for (const TypeHandle param : info.function_params) {
                    pending.push_back(param);
                }
                break;
            case TypeKind::associated_projection:
                if (dyn_trait_associated_projection_is_constrained_self(types, info, associated_equalities)) {
                    break;
                }
                pending.push_back(info.associated_base);
                break;
            case TypeKind::struct_:
            case TypeKind::enum_:
            case TypeKind::opaque_struct:
                for (const TypeHandle arg : info.generic_args) {
                    pending.push_back(arg);
                }
                break;
            case TypeKind::trait_object:
                for (const TypeHandle arg : info.trait_object_args) {
                    pending.push_back(arg);
                }
                for (const TraitObjectAssociatedTypeEquality& equality :
                    info.trait_object_associated_equalities) {
                    pending.push_back(equality.value_type);
                }
                break;
            case TypeKind::builtin:
                break;
        }
    }
    return false;
}

[[nodiscard]] const TraitImplAssociatedTypeInfo* dyn_trait_find_equality(
    const std::span<const TraitImplAssociatedTypeInfo> associated_equalities, const query::MemberKey member)
{
    const auto found = std::ranges::find_if(associated_equalities,
        [&](const TraitImplAssociatedTypeInfo& equality) { return equality.member_key == member; });
    return found == associated_equalities.end() ? nullptr : &*found;
}

[[nodiscard]] std::vector<TraitImplAssociatedTypeInfo> dyn_trait_object_equalities_as_impl_equalities(
    const CheckedModule& checked,
    const TypeInfo& object_info)
{
    std::vector<TraitImplAssociatedTypeInfo> associated_equalities;
    associated_equalities.reserve(object_info.trait_object_associated_equalities.size());
    for (const TraitObjectAssociatedTypeEquality& equality : object_info.trait_object_associated_equalities) {
        TraitImplAssociatedTypeInfo info = checked.make_trait_impl_associated_type_info();
        info.name = equality.name;
        info.name_id = equality.name.id;
        info.value_type = equality.value_type;
        info.member_key = equality.associated_member;
        associated_equalities.push_back(info);
    }
    return associated_equalities;
}

[[nodiscard]] bool dyn_trait_associated_equalities_match(const TypeTable& types,
    const std::span<const TraitImplAssociatedTypeInfo> object_equalities,
    const std::span<const TraitImplAssociatedTypeInfo> impl_equalities)
{
    for (const TraitImplAssociatedTypeInfo& object_equality : object_equalities) {
        const TraitImplAssociatedTypeInfo* const impl_equality =
            dyn_trait_find_equality(impl_equalities, object_equality.member_key);
        if (impl_equality == nullptr || !types.same(object_equality.value_type, impl_equality->value_type)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool trait_keys_match(const query::DefKey lhs, const query::DefKey rhs) noexcept
{
    return query::is_valid(lhs) && query::is_valid(rhs) && lhs.global_id == rhs.global_id;
}

[[nodiscard]] base::u32 dyn_trait_method_slot_count(const CheckedModule& checked,
    const query::TraitObjectTypeKey& object_key) noexcept
{
    base::u32 count = 0;
    for (const TraitObjectMethodSlotFact& fact : checked.trait_object_method_slots) {
        if (fact.object_type_key == object_key && fact.slot != SEMA_TRAIT_PREDICATE_INVALID_INDEX) {
            count = std::max(count, fact.slot + 1U);
        }
    }
    return count;
}

[[nodiscard]] query::TraitObjectMetadataPolicyKey dyn_trait_vtable_metadata_policy(
    const CheckedModule& checked, const query::TraitObjectTypeKey& object_key) noexcept
{
    for (const TraitSupertraitEdgeFact& edge : checked.trait_supertrait_edges) {
        if (trait_keys_match(edge.child_trait_key, object_key.principal_trait)) {
            return query::TraitObjectMetadataPolicyKey::supertrait_vptr_metadata_v1;
        }
    }
    return query::TraitObjectMetadataPolicyKey::borrowed_methods_only_v1;
}

[[nodiscard]] const TraitObjectMethodSlotFact* dyn_trait_method_slot_fact(const CheckedModule& checked,
    const query::TraitObjectTypeKey& object_key, const IdentId method_name_id)
{
    for (const TraitObjectMethodSlotFact& fact : checked.trait_object_method_slots) {
        if (fact.object_type_key == object_key && fact.method_name_id == method_name_id) {
            return &fact;
        }
    }
    return nullptr;
}

[[nodiscard]] base::u32 sema_part_index_or_zero(const SemanticAnalyzerCore& core) noexcept
{
    return syntax::is_valid(core.state_.flow.current_item) ? core.item_part_index(core.state_.flow.current_item) : 0U;
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
    TraitAnalysisScope(SemanticAnalyzerCore& owner_analyzer, const syntax::ModuleId module, const syntax::ItemId item,
        GenericContext* const generic_context = nullptr, GenericSideTables* const side_tables = nullptr,
        const bool cache_syntax_types = false)
        : analyzer(owner_analyzer), previous_module(owner_analyzer.state_.flow.current_module),
          previous_item(owner_analyzer.state_.flow.current_item),
          previous_generic_context(owner_analyzer.state_.flow.current_generic_context),
          previous_side_tables(owner_analyzer.state_.flow.current_side_tables)
    {
        this->analyzer.state_.flow.current_module = module;
        this->analyzer.state_.flow.current_item = item;
        this->analyzer.state_.flow.current_generic_context = generic_context;
        if (side_tables != nullptr) {
            this->analyzer.state_.flow.current_side_tables.side_tables = side_tables;
        }
        this->analyzer.state_.flow.current_side_tables.cache_syntax_types = cache_syntax_types;
    }

    TraitAnalysisScope(const TraitAnalysisScope&) = delete;
    TraitAnalysisScope& operator=(const TraitAnalysisScope&) = delete;

    ~TraitAnalysisScope()
    {
        this->analyzer.state_.flow.current_module = this->previous_module;
        this->analyzer.state_.flow.current_item = this->previous_item;
        this->analyzer.state_.flow.current_generic_context = this->previous_generic_context;
        this->analyzer.state_.flow.current_side_tables = this->previous_side_tables;
    }

    SemanticAnalyzerCore& analyzer;
    syntax::ModuleId previous_module;
    syntax::ItemId previous_item;
    GenericContext* previous_generic_context = nullptr;
    GenericSideTableScope previous_side_tables{};
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
    if (item.name == SEMA_RESERVED_DROP_TRAIT_NAME) {
        this->core_.report_general(item.range, std::string(SEMA_DROP_TRAIT_RESERVED));
        return;
    }
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
            + std::to_string(type_generic_param_count(item.generic_params)) + "|"
            + std::to_string(item.trait_supertraits.size()) + "|"
            + std::to_string(item.trait_items.size()));
    signature.generic_params.reserve(type_generic_param_count(item.generic_params));
    for (const syntax::GenericParamDecl& param : item.generic_params) {
        if (!is_type_generic_param(param)) {
            continue;
        }
        signature.generic_params.push_back(param.name_id);
    }
    signature.associated_types.reserve(item.trait_items.size());
    for (base::u32 ordinal = 0; ordinal < item.trait_items.size(); ++ordinal) {
        const syntax::ItemId requirement_id = item.trait_items[ordinal];
        if (!syntax::is_valid(requirement_id) || requirement_id.value >= this->core_.ctx_.module.items.size()) {
            continue;
        }
        const syntax::ItemNode requirement = this->core_.ctx_.module.items[requirement_id.value];
        if (requirement.kind != syntax::ItemKind::type_alias) {
            continue;
        }
        TraitAssociatedTypeRequirement associated_type =
            this->core_.state_.checked.make_trait_associated_type_requirement();
        associated_type.name = this->core_.source_name_text(requirement.name_id, requirement.name);
        associated_type.name_id = requirement.name_id;
        associated_type.module = owner;
        associated_type.item = requirement_id;
        associated_type.range = requirement.range;
        associated_type.visibility = requirement.visibility;
        associated_type.stable_key = this->core_.stable_member_key(
            signature.stable_id, StableSymbolKind::type, requirement.name_id, requirement.name, ordinal);
        associated_type.member_key = this->trait_associated_type_member_key(signature, associated_type.name, ordinal);
        associated_type.ordinal = ordinal;
        signature.associated_types.push_back(associated_type);
    }
    signature.range = item.range;
    signature.part_index = this->core_.item_part_index(item_id);

    const auto inserted = this->core_.state_.checked.traits.emplace(key, std::move(signature));
    if (!inserted.second) {
        this->core_.report_duplicate(
            item.range, sema_duplicate_trait_definition_message(this->core_.module_name(owner), item.name));
        return;
    }
    this->core_.record_lifetime_origin_params(item, item_id);
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
        if (!is_type_generic_param(param)) {
            continue;
        }
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

void SemanticAnalyzerCore::TraitAnalyzer::merge_trait_where_constraints(
    const syntax::ItemNode& trait, const TraitSignature& signature, GenericContext& context)
{
    if (trait.where_constraints.empty()) {
        return;
    }

    GenericTemplateInfo info = this->core_.make_generic_template_info();
    info.item = signature.item;
    info.module = signature.module;
    info.part_index = signature.part_index;
    info.name = this->core_.state_.checked.intern_text(signature.name);
    info.name_id = signature.name_id;
    info.key = this->core_.module_lookup_key(signature.module, signature.name_id);
    info.function_key = this->core_.function_lookup_key(signature.module, signature.name_id);
    info.stable_id = signature.stable_id;
    info.visibility = signature.visibility;
    info.params.reserve(type_generic_param_count(trait.generic_params));
    info.param_identities.reserve(type_generic_param_count(trait.generic_params));
    for (const syntax::GenericParamDecl& param : trait.generic_params) {
        if (!is_type_generic_param(param)) {
            continue;
        }
        info.params.push_back(param.name_id);
        info.param_identities.push_back(generic_param_identity_from_text(param.name));
    }

    this->core_.validate_generic_constraints(trait, info);
    this->core_.copy_capability_map(context.constraints, info.constraints);
    context.predicate_indices.insert(
        context.predicate_indices.end(), info.predicate_indices.begin(), info.predicate_indices.end());
    context.obligation_indices.insert(
        context.obligation_indices.end(), info.obligation_indices.begin(), info.obligation_indices.end());
    context.param_env_key = info.param_env_key;
    for (const syntax::GenericParamDecl& param : trait.generic_params) {
        if (!is_type_generic_param(param)) {
            continue;
        }
        const auto identity = context.param_identities.find(param.name_id);
        const auto constraints = context.constraints.find(param.name_id);
        if (identity == context.param_identities.end() || constraints == context.constraints.end()) {
            continue;
        }
        context.constraints_by_identity.emplace(identity->second, this->core_.copy_capability_set(constraints->second));
    }
}

base::u32 SemanticAnalyzerCore::TraitAnalyzer::record_trait_self_predicate(
    const TraitSignature& trait, const syntax::ItemNode& trait_item, const GenericContext& context) const
{
    const IdentId self_id = this->core_.ctx_.module.intern_identifier(SEMA_TRAIT_SELF_NAME);
    const auto self_type = context.params.find(self_id);
    if (self_type == context.params.end()) {
        return SEMA_TRAIT_PREDICATE_INVALID_INDEX;
    }

    query::StableKeyWriter writer;
    writer.write_u64(SEMA_TRAIT_SELF_PREDICATE_KEY_MARKER);
    query::append_stable_key(writer, trait.stable_id);
    writer.write_u64(static_cast<base::u64>(trait.generic_params.size()));
    for (const IdentId param : trait.generic_params) {
        writer.write_u64(param.value);
    }

    TraitPredicate predicate = this->core_.state_.checked.make_trait_predicate();
    predicate.index =
        base::checked_u32(this->core_.state_.checked.trait_predicates.size(), SEMA_TRAIT_PREDICATE_ID_CONTEXT);
    predicate.kind = TraitPredicateKind::declared_trait;
    predicate.origin = TraitPredicateOrigin::trait_self;
    predicate.subject_type = self_type->second;
    predicate.subject_param_name_id = self_id;
    predicate.subject_param_identity = generic_param_identity_from_text(SEMA_TRAIT_SELF_NAME);
    predicate.trait_name = this->core_.state_.checked.intern_text(trait.name);
    predicate.trait_name_id = trait.name_id;
    predicate.trait_module = trait.module;
    predicate.trait_stable_id = trait.stable_id;
    predicate.trait_args.reserve(trait.generic_params.size());
    for (const IdentId param : trait.generic_params) {
        if (const auto arg = context.params.find(param); arg != context.params.end()) {
            predicate.trait_args.push_back(arg->second);
        }
    }
    predicate.canonical_fingerprint = writer.fingerprint();
    predicate.module = trait.module;
    predicate.item = trait.item;
    predicate.range = trait_item.range;
    predicate.part_index = trait.part_index;

    const base::u32 predicate_index = predicate.index;
    this->core_.state_.checked.trait_predicates.push_back(std::move(predicate));

    TraitEvidence evidence = this->core_.state_.checked.make_trait_evidence();
    evidence.kind = TraitEvidenceKind::param_env;
    evidence.predicate_index = predicate_index;
    evidence.predicate_fingerprint = this->core_.state_.checked.trait_predicates[predicate_index].canonical_fingerprint;
    evidence.module = trait.module;
    evidence.item = trait.item;
    evidence.range = trait_item.range;
    evidence.part_index = trait.part_index;
    this->core_.state_.checked.trait_evidence.push_back(evidence);
    return predicate_index;
}

query::DefKey SemanticAnalyzerCore::TraitAnalyzer::trait_query_key(const TraitSignature& trait) const noexcept
{
    return query::def_key_from_stable_id(this->core_.query_package_key(trait.module), trait.stable_id,
        query::DefNamespace::trait_, query::DefKind::trait_);
}

query::MemberKey SemanticAnalyzerCore::TraitAnalyzer::trait_associated_type_member_key(
    const TraitSignature& trait, const std::string_view name, const base::u32 ordinal) const noexcept
{
    return query::member_key(this->trait_query_key(trait), query::MemberKind::associated_type, name, ordinal);
}

query::StableFingerprint128 SemanticAnalyzerCore::TraitAnalyzer::trait_object_slot_schema(
    const TraitSignature& trait,
    const std::span<const TypeHandle> trait_args,
    const std::span<const TraitImplAssociatedTypeInfo> associated_equalities) const
{
    query::StableKeyWriter writer;
    writer.write_string(SEMA_DYN_TRAIT_SLOT_SCHEMA_TAG);
    query::append_stable_key(writer, this->trait_query_key(trait));
    writer.write_u64(static_cast<base::u64>(trait_args.size()));
    for (const TypeHandle arg : trait_args) {
        append_checked_canonical_type_or_display(this->core_, writer, arg);
    }
    std::vector<const TraitImplAssociatedTypeInfo*> equalities;
    equalities.reserve(associated_equalities.size());
    for (const TraitImplAssociatedTypeInfo& equality : associated_equalities) {
        equalities.push_back(&equality);
    }
    std::ranges::sort(equalities, [](const TraitImplAssociatedTypeInfo* lhs, const TraitImplAssociatedTypeInfo* rhs) {
        return lhs->member_key.global_id < rhs->member_key.global_id;
    });
    writer.write_u64(static_cast<base::u64>(equalities.size()));
    for (const TraitImplAssociatedTypeInfo* const equality : equalities) {
        query::append_stable_key(writer, equality->member_key);
        append_checked_canonical_type_or_display(this->core_, writer, equality->value_type);
    }
    std::vector<const TraitMethodRequirement*> requirements;
    requirements.reserve(trait.requirements.size());
    for (const TraitMethodRequirement& requirement : trait.requirements) {
        requirements.push_back(&requirement);
    }
    std::ranges::sort(requirements, [](const TraitMethodRequirement* lhs, const TraitMethodRequirement* rhs) {
        return lhs->ordinal < rhs->ordinal;
    });
    for (const TraitMethodRequirement* const requirement : requirements) {
        ReceiverAccessKind receiver_access = ReceiverAccessKind::none;
        if (!requirement->has_self_param || requirement->param_types.empty()
            || !dyn_trait_receiver_access(this->core_.state_.checked.types, requirement->param_types.front(),
                receiver_access)) {
            continue;
        }
        bool uses_self_outside_receiver = dyn_trait_type_has_unconstrained_self(
            this->core_.state_.checked.types, requirement->return_type, associated_equalities);
        for (base::usize index = SEMA_DYN_TRAIT_RECEIVER_PARAM_COUNT; index < requirement->param_types.size();
             ++index) {
            uses_self_outside_receiver =
                uses_self_outside_receiver
                || dyn_trait_type_has_unconstrained_self(
                    this->core_.state_.checked.types, requirement->param_types[index], associated_equalities);
        }
        if (uses_self_outside_receiver) {
            continue;
        }
        writer.write_u32(requirement->ordinal);
        writer.write_string(requirement->name);
        writer.write_u8(static_cast<base::u8>(receiver_access));
        writer.write_bool(requirement->is_unsafe);
        writer.write_bool(requirement->is_variadic);
        writer.write_u64(static_cast<base::u64>(requirement->param_types.size()));
        for (const TypeHandle param : requirement->param_types) {
            append_checked_canonical_type_or_display(this->core_, writer, param);
        }
        append_checked_canonical_type_or_display(this->core_, writer, requirement->return_type);
    }
    return writer.fingerprint();
}

bool SemanticAnalyzerCore::TraitAnalyzer::validate_trait_object_callability(const TraitSignature& trait,
    const TypeHandle,
    const std::span<const TypeHandle>,
    const std::span<const TraitImplAssociatedTypeInfo> associated_equalities,
    const base::SourceRange& range,
    const bool report_failure)
{
    bool ok = true;
    for (const TraitAssociatedTypeRequirement& associated_type : trait.associated_types) {
        if (dyn_trait_find_equality(associated_equalities, associated_type.member_key) == nullptr) {
            ok = false;
            if (report_failure) {
                this->core_.report_type(
                    range, sema_dyn_trait_missing_associated_type_message(trait.name, associated_type.name));
            }
        }
    }
    for (const TraitMethodRequirement& requirement : trait.requirements) {
        if (!requirement.has_self_param || requirement.param_types.empty()) {
            ok = false;
            if (report_failure) {
                this->core_.report_type(
                    requirement.range, sema_dyn_trait_method_requires_self_message(trait.name, requirement.name));
            }
            continue;
        }
        ReceiverAccessKind receiver_access = ReceiverAccessKind::none;
        if (!dyn_trait_receiver_access(
                this->core_.state_.checked.types, requirement.param_types.front(), receiver_access)) {
            ok = false;
            if (report_failure) {
                this->core_.report_type(
                    requirement.range, sema_dyn_trait_receiver_message(trait.name, requirement.name));
            }
            continue;
        }
        bool uses_self_outside_receiver = dyn_trait_type_has_unconstrained_self(
            this->core_.state_.checked.types, requirement.return_type, associated_equalities);
        for (base::usize index = SEMA_DYN_TRAIT_RECEIVER_PARAM_COUNT; index < requirement.param_types.size();
             ++index) {
            uses_self_outside_receiver =
                uses_self_outside_receiver
                || dyn_trait_type_has_unconstrained_self(
                    this->core_.state_.checked.types, requirement.param_types[index], associated_equalities);
        }
        if (uses_self_outside_receiver) {
            ok = false;
            if (report_failure) {
                this->core_.report_type(
                    requirement.range, sema_dyn_trait_self_usage_message(trait.name, requirement.name));
            }
        }
    }
    return ok;
}

void SemanticAnalyzerCore::TraitAnalyzer::record_trait_object_callability(const TypeHandle object_type,
    const TraitSignature& trait,
    const std::span<const TypeHandle> trait_args,
    const std::span<const TraitImplAssociatedTypeInfo> associated_equalities,
    const base::SourceRange& range)
{
    if (!is_valid(object_type) || object_type.value >= this->core_.state_.checked.types.size()) {
        return;
    }
    const TypeInfo& object_info = this->core_.state_.checked.types.get(object_type);
    if (object_info.kind != TypeKind::trait_object || !query::is_valid(object_info.trait_object_key)) {
        return;
    }
    if (std::ranges::any_of(this->core_.state_.checked.trait_object_callability,
            [&](const TraitObjectCallabilityFact& fact) {
                return fact.object_type_key == object_info.trait_object_key;
            })) {
        return;
    }
    if (!this->validate_trait_object_callability(trait, object_type, trait_args, associated_equalities, range, true)) {
        return;
    }
    const query::StableFingerprint128 slot_schema =
        this->trait_object_slot_schema(trait, trait_args, associated_equalities);
    base::u32 slot = 0;
    for (const TraitMethodRequirement& requirement : trait.requirements) {
        ReceiverAccessKind receiver_access = ReceiverAccessKind::none;
        if (!dyn_trait_receiver_access(
                this->core_.state_.checked.types, requirement.param_types.front(), receiver_access)) {
            continue;
        }
        bool uses_self_outside_receiver = dyn_trait_type_has_unconstrained_self(
            this->core_.state_.checked.types, requirement.return_type, associated_equalities);
        for (base::usize index = SEMA_DYN_TRAIT_RECEIVER_PARAM_COUNT; index < requirement.param_types.size();
             ++index) {
            uses_self_outside_receiver =
                uses_self_outside_receiver
                || dyn_trait_type_has_unconstrained_self(
                    this->core_.state_.checked.types, requirement.param_types[index], associated_equalities);
        }
        if (uses_self_outside_receiver) {
            continue;
        }
        TraitObjectMethodSlotFact fact = this->core_.state_.checked.make_trait_object_method_slot_fact();
        fact.object_type_key = object_info.trait_object_key;
        fact.object_type = object_type;
        fact.trait_module = trait.module;
        fact.trait_name_id = trait.name_id;
        fact.method_name = this->core_.state_.checked.intern_text(requirement.name);
        fact.method_name_id = requirement.name_id;
        fact.requirement_ordinal = requirement.ordinal;
        fact.slot = slot;
        fact.receiver_type = this->substitute_requirement_type(
            requirement.param_types.front(), object_type, trait_args, trait, associated_equalities);
        fact.return_type = this->substitute_requirement_type(
            requirement.return_type, object_type, trait_args, trait, associated_equalities);
        fact.receiver_access = receiver_access;
        fact.slot_schema = slot_schema;
        fact.range = requirement.range;
        fact.part_index = sema_part_index_or_zero(this->core_);
        this->core_.state_.checked.trait_object_method_slots.push_back(fact);
        ++slot;
    }
    TraitObjectCallabilityFact callability = this->core_.state_.checked.make_trait_object_callability_fact();
    callability.object_type_key = object_info.trait_object_key;
    callability.object_type = object_type;
    callability.trait_module = trait.module;
    callability.trait_name_id = trait.name_id;
    callability.trait_name = this->core_.state_.checked.intern_text(trait.name);
    callability.method_slot_count = slot;
    callability.slot_schema = slot_schema;
    callability.range = range;
    callability.part_index = sema_part_index_or_zero(this->core_);
    this->core_.state_.checked.trait_object_callability.push_back(callability);
}

const TraitAssociatedTypeRequirement* SemanticAnalyzerCore::TraitAnalyzer::find_trait_associated_type_requirement(
    const TraitSignature& trait, const IdentId name_id) const
{
    for (const TraitAssociatedTypeRequirement& associated_type : trait.associated_types) {
        if (associated_type.name_id == name_id) {
            return &associated_type;
        }
    }
    return nullptr;
}

const TraitSignature* SemanticAnalyzerCore::TraitAnalyzer::find_current_trait_signature() const
{
    for (const auto& entry : this->core_.state_.checked.traits) {
        const TraitSignature& trait = entry.second;
        if (trait.item.value == this->core_.state_.flow.current_item.value) {
            return &trait;
        }
        for (const TraitAssociatedTypeRequirement& associated_type : trait.associated_types) {
            if (associated_type.item.value == this->core_.state_.flow.current_item.value) {
                return &trait;
            }
        }
        for (const TraitMethodRequirement& requirement : trait.requirements) {
            if (requirement.item.value == this->core_.state_.flow.current_item.value) {
                return &trait;
            }
        }
    }
    return nullptr;
}

const TraitAssociatedTypeRequirement*
SemanticAnalyzerCore::TraitAnalyzer::find_current_trait_associated_type_requirement(const IdentId name_id) const
{
    const TraitSignature* const trait = this->find_current_trait_signature();
    return trait == nullptr ? nullptr : this->find_trait_associated_type_requirement(*trait, name_id);
}

TypeHandle SemanticAnalyzerCore::TraitAnalyzer::resolve_current_trait_concrete_associated_type(
    const TypeHandle base_type, const TraitAssociatedTypeRequirement& requirement) const
{
    const TraitSignature* const trait = this->find_current_trait_signature();
    if (trait == nullptr) {
        return INVALID_TYPE_HANDLE;
    }
    for (const auto& entry : this->core_.state_.checked.trait_impls) {
        const TraitImplInfo& impl = entry.second;
        if (impl.trait_module.value != trait->module.value || impl.trait_name_id != trait->name_id
            || !this->core_.state_.checked.types.same(impl.self_type, base_type)) {
            continue;
        }
        for (const TraitImplAssociatedTypeInfo& associated_type : impl.associated_types) {
            if (associated_type.member_key == requirement.member_key) {
                return associated_type.value_type;
            }
        }
    }
    return INVALID_TYPE_HANDLE;
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

    std::unordered_map<base::u64, base::SourceRange> seen_supertraits;
    seen_supertraits.reserve(trait.trait_supertraits.size());
    signature.supertraits.clear();
    signature.supertraits.reserve(trait.trait_supertraits.size());
    for (const syntax::TraitSupertraitDecl& supertrait : trait.trait_supertraits) {
        const ResolvedTraitReference parent_reference =
            this->resolve_supertrait_reference(trait, supertrait, signature.item, context);
        if (!parent_reference.ok || parent_reference.signature == nullptr) {
            continue;
        }
        const TraitSignature& parent = *parent_reference.signature;
        const query::DefKey parent_key = this->trait_query_key(parent);
        const auto inserted = seen_supertraits.emplace(parent_key.global_id, supertrait.range);
        if (!inserted.second) {
            this->core_.report_duplicate(supertrait.range,
                "duplicate supertrait `" + std::string(parent.name) + "` in trait `" + std::string(signature.name)
                    + "`");
            this->core_.report_note(inserted.first->second, SemanticDiagnosticKind::duplicate,
                sema_previous_declaration_note_message(parent.name));
            continue;
        }
        if (trait_keys_match(this->trait_query_key(signature), parent_key)) {
            this->core_.report_type(supertrait.range,
                "supertrait cycle detected: trait `" + std::string(signature.name) + "` inherits itself");
            continue;
        }
        if (syntax::visibility_rank(signature.visibility) > syntax::visibility_rank(parent.visibility)) {
            this->core_.report_visibility(supertrait.range,
                "public trait exposes private supertrait `" + std::string(parent.name) + "`");
        }
        TraitSupertraitInfo info =
            this->make_supertrait_info(signature, parent, parent_reference.trait_args, supertrait);
        signature.supertraits.push_back(this->core_.state_.checked.clone_trait_supertrait_info(info));
        this->append_supertrait_fact_if_new(info);
    }

    std::unordered_map<IdentId, base::SourceRange, IdentIdHash> seen_items;
    seen_items.reserve(trait.trait_items.size());
    signature.associated_types.clear();
    signature.associated_types.reserve(trait.trait_items.size());
    for (base::u32 ordinal = 0; ordinal < trait.trait_items.size(); ++ordinal) {
        const syntax::ItemId requirement_id = trait.trait_items[ordinal];
        if (!syntax::is_valid(requirement_id) || requirement_id.value >= this->core_.ctx_.module.items.size()) {
            continue;
        }
        const syntax::ItemNode requirement = this->core_.ctx_.module.items[requirement_id.value];
        if (requirement.kind != syntax::ItemKind::type_alias) {
            continue;
        }
        const auto inserted = seen_items.emplace(requirement.name_id, requirement.range);
        if (!inserted.second) {
            this->core_.report_duplicate(
                requirement.range, sema_duplicate_trait_associated_item_message(signature.name, requirement.name));
            this->core_.report_note(inserted.first->second, SemanticDiagnosticKind::duplicate,
                sema_previous_declaration_note_message(requirement.name));
            continue;
        }
        signature.associated_types.push_back(
            this->resolve_trait_associated_type_requirement(signature, requirement, requirement_id, ordinal));
    }

    signature.requirements.clear();
    signature.requirements.reserve(trait.trait_items.size());
    for (base::u32 ordinal = 0; ordinal < trait.trait_items.size(); ++ordinal) {
        const syntax::ItemId requirement_id = trait.trait_items[ordinal];
        if (!syntax::is_valid(requirement_id) || requirement_id.value >= this->core_.ctx_.module.items.size()) {
            continue;
        }
        const syntax::ItemNode requirement = this->core_.ctx_.module.items[requirement_id.value];
        if (requirement.kind == syntax::ItemKind::type_alias) {
            continue;
        }
        if (requirement.kind != syntax::ItemKind::fn_decl) {
            this->core_.report_internal_contract(
                requirement.range, "trait associated item must be a function or associated type");
            continue;
        }
        const auto inserted = seen_items.emplace(requirement.name_id, requirement.range);
        if (!inserted.second) {
            this->core_.report_duplicate(
                requirement.range, sema_duplicate_trait_associated_item_message(signature.name, requirement.name));
            this->core_.report_note(inserted.first->second, SemanticDiagnosticKind::duplicate,
                sema_previous_declaration_note_message(requirement.name));
            continue;
        }
        signature.requirements.push_back(
            this->resolve_trait_requirement(signature, requirement, requirement_id, ordinal));
    }
}

TraitAssociatedTypeRequirement SemanticAnalyzerCore::TraitAnalyzer::resolve_trait_associated_type_requirement(
    const TraitSignature& trait, const syntax::ItemNode& requirement, const syntax::ItemId requirement_id,
    const base::u32 ordinal)
{
    TraitAssociatedTypeRequirement info = this->core_.state_.checked.make_trait_associated_type_requirement();
    info.name = this->core_.source_name_text(requirement.name_id, requirement.name);
    info.name_id = requirement.name_id;
    info.module = trait.module;
    info.item = requirement_id;
    info.range = requirement.range;
    info.visibility = requirement.visibility;
    info.stable_key = this->core_.stable_member_key(
        trait.stable_id, StableSymbolKind::type, requirement.name_id, requirement.name, ordinal);
    info.member_key = this->trait_associated_type_member_key(trait, info.name, ordinal);
    info.ordinal = ordinal;
    if (!requirement.generic_params.empty() || !requirement.where_constraints.empty()) {
        this->core_.report_unsupported(requirement.range, std::string(SEMA_ASSOCIATED_TYPE_GENERIC_UNSUPPORTED));
    }
    if (syntax::is_valid(requirement.alias_type)) {
        this->core_.report_general(requirement.range, "trait associated type requirement must not have a target type");
    }
    return info;
}

SemanticAnalyzerCore::TraitAnalyzer::ResolvedTraitReference
SemanticAnalyzerCore::TraitAnalyzer::resolve_supertrait_reference(const syntax::ItemNode&,
    const syntax::TraitSupertraitDecl& supertrait,
    const syntax::ItemId trait_id,
    GenericContext& context)
{
    ResolvedTraitReference result;
    if (!syntax::is_valid(supertrait.trait_type)
        || supertrait.trait_type.value >= this->core_.ctx_.module.types.size()) {
        this->core_.report_lookup(supertrait.range, "supertrait must name a trait");
        return result;
    }
    const syntax::TypeNode& supertrait_type = this->core_.ctx_.module.types[supertrait.trait_type.value];
    if (supertrait_type.kind != syntax::TypeKind::named) {
        this->core_.report_lookup(supertrait_type.range, "supertrait must name a trait");
        return result;
    }

    const std::vector<std::string_view> scope_parts = this->core_.type_scope_parts(supertrait_type);
    const bool qualified = !scope_parts.empty();
    const TraitSignature* parent = nullptr;
    TraitAnalysisScope scope(this->core_, this->core_.item_module(trait_id), trait_id, &context);
    if (qualified) {
        const syntax::ModuleId module = this->core_.resolve_type_scope(supertrait_type, true);
        parent = this->find_trait_in_module(
            module, supertrait_type.name_id, supertrait_type.name, supertrait_type.range, true);
    } else {
        parent = this->find_trait_in_visible_modules(
            supertrait_type.name_id, supertrait_type.name, supertrait_type.range, true);
    }
    if (parent == nullptr) {
        return result;
    }

    if (supertrait_type.type_args.empty() && !parent->generic_params.empty()) {
        this->core_.report_type(supertrait_type.range,
            sema_generic_argument_count_message(
                "supertrait type arguments", parent->name, 0, parent->generic_params.size()));
        return result;
    }
    if (!supertrait_type.type_args.empty() && parent->generic_params.empty()) {
        this->core_.report_type(supertrait_type.range, sema_trait_not_generic_message(parent->name));
        return result;
    }
    if (supertrait_type.type_args.size() != parent->generic_params.size()) {
        this->core_.report_type(supertrait_type.range,
            sema_generic_argument_count_message("supertrait type arguments", parent->name,
                supertrait_type.type_args.size(), parent->generic_params.size()));
        return result;
    }

    result.trait_args.reserve(supertrait_type.type_args.size());
    for (const syntax::TypeId arg : supertrait_type.type_args) {
        result.trait_args.push_back(this->core_.resolve_type(arg));
    }
    result.signature = parent;
    result.ok = true;
    return result;
}

TraitSupertraitInfo SemanticAnalyzerCore::TraitAnalyzer::make_supertrait_info(const TraitSignature& child,
    const TraitSignature& parent,
    const std::span<const TypeHandle> parent_args,
    const syntax::TraitSupertraitDecl& supertrait) const
{
    TraitSupertraitInfo info = this->core_.state_.checked.make_trait_supertrait_info();
    info.child_trait_key = this->trait_query_key(child);
    info.parent_trait_key = this->trait_query_key(parent);
    info.child_trait_name = this->core_.state_.checked.intern_text(child.name);
    info.child_trait_name_id = child.name_id;
    info.child_trait_module = child.module;
    info.parent_trait_name = this->core_.state_.checked.intern_text(parent.name);
    info.parent_trait_name_id = parent.name_id;
    info.parent_trait_module = parent.module;
    info.parent_trait_args = this->core_.state_.checked.copy_type_handle_list(parent_args);
    info.direct_edge_ordinal = supertrait.ordinal;
    info.closure_depth = 1U;
    info.range = supertrait.range;
    info.part_index = child.part_index;

    info.edge_fingerprint = make_supertrait_edge_fingerprint(this->core_, info.child_trait_key, info.parent_trait_key,
        info.direct_edge_ordinal, info.closure_depth, parent_args);
    return info;
}

TraitSupertraitEdgeFact SemanticAnalyzerCore::TraitAnalyzer::make_supertrait_edge_fact(
    const TraitSupertraitInfo& info) const
{
    TraitSupertraitEdgeFact fact = this->core_.state_.checked.make_trait_supertrait_edge_fact();
    fact.child_trait_key = info.child_trait_key;
    fact.parent_trait_key = info.parent_trait_key;
    fact.child_trait_name = this->core_.state_.checked.intern_text(info.child_trait_name);
    fact.child_trait_name_id = info.child_trait_name_id;
    fact.child_trait_module = info.child_trait_module;
    fact.parent_trait_name = this->core_.state_.checked.intern_text(info.parent_trait_name);
    fact.parent_trait_name_id = info.parent_trait_name_id;
    fact.parent_trait_module = info.parent_trait_module;
    fact.parent_trait_args = this->core_.state_.checked.copy_type_handle_list(info.parent_trait_args);
    fact.edge_fingerprint = info.edge_fingerprint;
    fact.direct_edge_ordinal = info.direct_edge_ordinal;
    fact.closure_depth = info.closure_depth;
    fact.range = info.range;
    fact.part_index = info.part_index;
    return fact;
}

TypeHandle SemanticAnalyzerCore::TraitAnalyzer::instantiate_supertrait_arg(
    const TypeHandle arg, const TraitSupertraitEdgeFact& edge, const TypeInfo& source_object) const
{
    const auto child_trait = std::ranges::find_if(this->core_.state_.checked.traits,
        [&](const auto& entry) {
            return trait_keys_match(this->trait_query_key(entry.second), edge.child_trait_key);
        });
    if (child_trait == this->core_.state_.checked.traits.end()) {
        return arg;
    }
    return this->substitute_requirement_type(
        arg, INVALID_TYPE_HANDLE, source_object.trait_object_args, child_trait->second, {});
}

TypeHandle SemanticAnalyzerCore::TraitAnalyzer::instantiate_supertrait_object_type(
    const TraitSupertraitEdgeFact& edge, const TypeInfo& source_object, const TraitSignature& parent_trait) const
{
    std::vector<TypeHandle> parent_args;
    parent_args.reserve(edge.parent_trait_args.size());
    for (const TypeHandle arg : edge.parent_trait_args) {
        parent_args.push_back(this->instantiate_supertrait_arg(arg, edge, source_object));
    }
    std::vector<query::CanonicalTypeKey> canonical_args;
    canonical_args.reserve(parent_args.size());
    for (const TypeHandle arg : parent_args) {
        base::Result<query::CanonicalTypeKey> canonical_arg = this->core_.checked_canonical_type_key(arg);
        if (!canonical_arg) {
            return INVALID_TYPE_HANDLE;
        }
        canonical_args.push_back(canonical_arg.take_value());
    }
    const std::span<const TraitImplAssociatedTypeInfo> associated_equalities{};
    const query::StableFingerprint128 slot_schema =
        this->trait_object_slot_schema(parent_trait, parent_args, associated_equalities);
    query::StableKeyWriter origin_writer;
    origin_writer.write_string("dyn-trait-object-origin:v1");
    query::append_stable_key(origin_writer, this->trait_query_key(parent_trait));
    origin_writer.write_u64(static_cast<base::u64>(canonical_args.size()));
    for (const query::CanonicalTypeKey& arg : canonical_args) {
        query::append_stable_key(origin_writer, arg);
    }
    origin_writer.write_u64(0U);
    origin_writer.write_fingerprint(slot_schema);

    const query::TraitObjectTypeKey key = query::trait_object_type_key(
        this->trait_query_key(parent_trait), canonical_args,
        std::span<const query::TraitObjectAssociatedTypeEqualityKey>{}, origin_writer.fingerprint(), slot_schema);
    return this->core_.state_.checked.types.trait_object(
        key, parent_trait.name, parent_trait.module, parent_trait.name_id, parent_args, {});
}

bool SemanticAnalyzerCore::TraitAnalyzer::supertrait_edge_matches_target_object(
    const TraitSupertraitEdgeFact& edge, const TypeInfo& source_object, const TypeInfo& target_object) const
{
    if (edge.parent_trait_args.size() != target_object.trait_object_args.size()) {
        return false;
    }
    for (base::usize index = 0; index < edge.parent_trait_args.size(); ++index) {
        const TypeHandle instantiated =
            this->instantiate_supertrait_arg(edge.parent_trait_args[index], edge, source_object);
        if (!this->core_.state_.checked.types.same(instantiated, target_object.trait_object_args[index])) {
            return false;
        }
    }
    return true;
}

void SemanticAnalyzerCore::TraitAnalyzer::append_supertrait_fact_if_new(const TraitSupertraitInfo& info)
{
    const auto found =
        std::ranges::find_if(this->core_.state_.checked.trait_supertrait_edges,
            [&](const TraitSupertraitEdgeFact& fact) {
                return fact.edge_fingerprint == info.edge_fingerprint
                    && trait_keys_match(fact.child_trait_key, info.child_trait_key)
                    && trait_keys_match(fact.parent_trait_key, info.parent_trait_key);
            });
    if (found == this->core_.state_.checked.trait_supertrait_edges.end()) {
        this->core_.state_.checked.trait_supertrait_edges.push_back(this->make_supertrait_edge_fact(info));
    }
}

void SemanticAnalyzerCore::TraitAnalyzer::validate_supertrait_graph()
{
    struct PendingEdge {
        TraitSupertraitInfo info;
        const TraitSignature* current_parent = nullptr;
        base::u32 depth = 1;
        std::vector<base::u64> path_trait_keys;
    };

    for (auto& entry : this->core_.state_.checked.traits) {
        TraitSignature& child = entry.second;
        std::unordered_set<query::StableFingerprint128, query::StableFingerprintHash> recorded_endpoints;
        recorded_endpoints.reserve(child.supertraits.size());
        std::vector<PendingEdge> pending;
        pending.reserve(child.supertraits.size());
        const query::DefKey child_key = this->trait_query_key(child);
        for (const TraitSupertraitInfo& direct : child.supertraits) {
            const TraitSignature* parent_signature = nullptr;
            for (const auto& candidate : this->core_.state_.checked.traits) {
                if (trait_keys_match(this->trait_query_key(candidate.second), direct.parent_trait_key)) {
                    parent_signature = &candidate.second;
                    break;
                }
            }
            PendingEdge pending_edge;
            pending_edge.info = direct;
            pending_edge.current_parent = parent_signature;
            pending_edge.depth = direct.closure_depth;
            pending_edge.path_trait_keys.push_back(child_key.global_id);
            pending.push_back(std::move(pending_edge));
        }

        while (!pending.empty()) {
            PendingEdge edge = std::move(pending.back());
            pending.pop_back();
            if (!query::is_valid(edge.info.parent_trait_key)) {
                continue;
            }
            if (trait_keys_match(edge.info.child_trait_key, edge.info.parent_trait_key)) {
                this->core_.report_type(edge.info.range,
                    "supertrait cycle detected: trait `" + std::string(edge.info.child_trait_name.view())
                        + "` reaches itself");
                continue;
            }
            if (std::ranges::find(edge.path_trait_keys, edge.info.parent_trait_key.global_id)
                != edge.path_trait_keys.end()) {
                continue;
            }

            edge.info.closure_depth = edge.depth;
            const query::StableFingerprint128 endpoint_fingerprint = make_supertrait_endpoint_fingerprint(
                this->core_, edge.info.child_trait_key, edge.info.parent_trait_key, edge.info.parent_trait_args);
            if (!recorded_endpoints.insert(endpoint_fingerprint).second) {
                continue;
            }
            if (edge.depth > 1U) {
                edge.info.edge_fingerprint =
                    make_supertrait_edge_fingerprint(this->core_, edge.info.child_trait_key, edge.info.parent_trait_key,
                        edge.info.direct_edge_ordinal, edge.info.closure_depth, edge.info.parent_trait_args);
                this->append_supertrait_fact_if_new(edge.info);
            }

            if (edge.current_parent == nullptr) {
                continue;
            }
            for (const TraitSupertraitInfo& parent_edge : edge.current_parent->supertraits) {
                const TraitSignature* next_parent = nullptr;
                for (const auto& candidate : this->core_.state_.checked.traits) {
                    if (trait_keys_match(this->trait_query_key(candidate.second), parent_edge.parent_trait_key)) {
                        next_parent = &candidate.second;
                        break;
                    }
                }
                TraitSupertraitInfo inherited = parent_edge;
                inherited.child_trait_key = this->trait_query_key(child);
                inherited.child_trait_name = this->core_.state_.checked.intern_text(child.name);
                inherited.child_trait_name_id = child.name_id;
                inherited.child_trait_module = child.module;
                inherited.direct_edge_ordinal = edge.info.direct_edge_ordinal;
                inherited.closure_depth = edge.depth + 1U;
                inherited.parent_trait_args.clear();
                inherited.parent_trait_args.reserve(parent_edge.parent_trait_args.size());
                for (const TypeHandle arg : parent_edge.parent_trait_args) {
                    inherited.parent_trait_args.push_back(this->substitute_requirement_type(
                        arg, INVALID_TYPE_HANDLE, edge.info.parent_trait_args, *edge.current_parent, {}));
                }
                inherited.part_index = child.part_index;
                PendingEdge inherited_edge;
                inherited_edge.info = std::move(inherited);
                inherited_edge.current_parent = next_parent;
                inherited_edge.depth = edge.depth + 1U;
                inherited_edge.path_trait_keys = edge.path_trait_keys;
                inherited_edge.path_trait_keys.push_back(edge.info.parent_trait_key.global_id);
                pending.push_back(std::move(inherited_edge));
            }
        }
    }
}

const TraitSupertraitEdgeFact* SemanticAnalyzerCore::TraitAnalyzer::find_supertrait_edge_path(
    const TypeInfo& source_object, const TypeInfo& target_object) const
{
    if (source_object.kind != TypeKind::trait_object || target_object.kind != TypeKind::trait_object
        || !query::is_valid(source_object.trait_object_key) || !query::is_valid(target_object.trait_object_key)) {
        return nullptr;
    }
    const query::DefKey source_trait = source_object.trait_object_key.principal_trait;
    const query::DefKey target_trait = target_object.trait_object_key.principal_trait;
    const TraitSupertraitEdgeFact* best = nullptr;
    bool ambiguous = false;
    for (const TraitSupertraitEdgeFact& fact : this->core_.state_.checked.trait_supertrait_edges) {
        if (!trait_keys_match(fact.child_trait_key, source_trait)
            || !trait_keys_match(fact.parent_trait_key, target_trait)) {
            continue;
        }
        if (!this->supertrait_edge_matches_target_object(fact, source_object, target_object)) {
            continue;
        }
        if (best == nullptr || fact.closure_depth < best->closure_depth) {
            best = &fact;
            ambiguous = false;
            continue;
        }
        if (fact.closure_depth == best->closure_depth && fact.edge_fingerprint != best->edge_fingerprint) {
            ambiguous = true;
        }
    }
    return ambiguous ? nullptr : best;
}

void SemanticAnalyzerCore::TraitAnalyzer::validate_supertrait_impl_obligations()
{
    for (const auto& entry : this->core_.state_.checked.trait_impls) {
        const TraitImplInfo& impl = entry.second;
        const TraitSignature* const child_trait = this->trait_signature_for_impl(impl);
        if (child_trait == nullptr || child_trait->supertraits.empty()) {
            continue;
        }
        for (const TraitSupertraitInfo& parent : child_trait->supertraits) {
            const TraitSignature* parent_signature = nullptr;
            for (const auto& trait_entry : this->core_.state_.checked.traits) {
                if (trait_keys_match(this->trait_query_key(trait_entry.second), parent.parent_trait_key)) {
                    parent_signature = &trait_entry.second;
                    break;
                }
            }
            if (parent_signature == nullptr) {
                continue;
            }
            std::vector<TypeHandle> instantiated_parent_args;
            instantiated_parent_args.reserve(parent.parent_trait_args.size());
            for (const TypeHandle arg : parent.parent_trait_args) {
                instantiated_parent_args.push_back(
                    this->substitute_requirement_type(arg, INVALID_TYPE_HANDLE, impl.trait_args, *child_trait, {}));
            }
            const TraitImplLookupKey parent_key =
                this->make_trait_impl_lookup_key(*parent_signature, impl.self_type, instantiated_parent_args);
            if (this->core_.state_.checked.trait_impls.contains(parent_key)) {
                continue;
            }
            this->core_.report_type(impl.range,
                "impl `" + std::string(child_trait->name) + "` requires supertrait evidence `"
                    + std::string(parent.parent_trait_name.view()) + "` for "
                    + this->core_.state_.checked.types.display_name(impl.self_type));
            this->core_.report_note(parent.range, SemanticDiagnosticKind::type_mismatch,
                "supertrait declared here");
        }
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
    info.has_default_body = requirement.is_trait_default_method || syntax::is_valid(requirement.body);
    info.default_body = info.has_default_body ? requirement.body : syntax::INVALID_STMT_ID;
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
    const FunctionLookupKey requirement_key{
        syntax::is_valid(trait.module) ? trait.module.value : SEMA_LOOKUP_INVALID_KEY_PART,
        SEMA_LOOKUP_INVALID_KEY_PART,
        requirement.name_id,
    };
    info.borrow_contract = SemanticAnalyzerCore::BorrowContractAnalyzer(this->core_)
                               .resolve_signature_contract(requirement, requirement_key, info.return_type,
                                   info.param_types, trait.part_index, info.has_default_body);
    info.has_borrow_contract = requirement.borrow_contract.present
        || info.borrow_contract.source == FunctionBorrowContractSource::conservative_unknown;
    if (info.has_borrow_contract) {
        this->core_.state_.checked.borrow_contracts[requirement_key] = info.borrow_contract;
    }
    if (!info.has_default_body) {
        FunctionSignature signature = this->core_.state_.checked.make_function_signature();
        signature.name = this->core_.state_.checked.intern_text(info.name);
        signature.name_id = info.name_id;
        signature.semantic_key = requirement_key;
        signature.module = info.module;
        signature.trait_module = trait.module;
        signature.trait_name_id = trait.name_id;
        signature.return_type = info.return_type;
        signature.param_types = this->core_.state_.checked.copy_type_handle_list(info.param_types);
        signature.range = info.range;
        signature.is_unsafe = info.is_unsafe;
        signature.is_variadic = info.is_variadic;
        signature.has_prototype = true;
        signature.has_definition = false;
        signature.is_method = true;
        signature.has_self_param = info.has_self_param;
        signature.visibility = info.visibility;
        signature.prototype_item = requirement_id;
        signature.part_index = this->core_.item_part_index(requirement_id);
        this->core_.analyze_signature_lifetimes(requirement, requirement_key, signature);
    }
    return info;
}

void SemanticAnalyzerCore::TraitAnalyzer::register_trait_signatures()
{
    for (auto& entry : this->core_.state_.checked.traits) {
        this->resolve_trait_signature(entry.second);
    }
    this->validate_supertrait_graph();
}

FunctionSignature SemanticAnalyzerCore::TraitAnalyzer::make_trait_default_method_signature(
    const TraitSignature& trait, const TraitMethodRequirement& requirement) const
{
    FunctionSignature signature = this->core_.state_.checked.make_function_signature();
    signature.name = this->core_.state_.checked.intern_text(requirement.name);
    signature.name_id = requirement.name_id;
    signature.semantic_key = this->core_.function_lookup_key(requirement.module, requirement.name_id);
    signature.module = requirement.module;
    signature.trait_module = trait.module;
    signature.trait_name_id = trait.name_id;
    signature.return_type = requirement.return_type;
    signature.param_types = this->core_.state_.checked.copy_type_handle_list(requirement.param_types);
    signature.range = requirement.range;
    signature.is_unsafe = requirement.is_unsafe;
    signature.is_variadic = requirement.is_variadic;
    signature.has_definition = true;
    signature.is_method = true;
    signature.has_self_param = requirement.has_self_param;
    signature.visibility = requirement.visibility;
    signature.definition_item = requirement.item;
    signature.part_index = this->core_.item_part_index(requirement.item);
    return signature;
}

void SemanticAnalyzerCore::TraitAnalyzer::analyze_trait_default_method_body(
    const TraitSignature& trait, const TraitMethodRequirement& requirement, GenericContext& context)
{
    if (!requirement.has_default_body || !syntax::is_valid(requirement.item)
        || requirement.item.value >= this->core_.ctx_.module.items.size()) {
        return;
    }
    const syntax::ItemNode method = this->core_.ctx_.module.items[requirement.item.value];
    if (!syntax::is_valid(method.body)) {
        return;
    }

    TraitAnalysisScope scope(this->core_, trait.module, requirement.item, &context);
    FunctionBodyState state = FunctionBodyState::not_started;
    const FunctionSignature signature = this->make_trait_default_method_signature(trait, requirement);
    this->core_.analyze_function_body_with_signature(method, signature.semantic_key, signature, state);
}

void SemanticAnalyzerCore::TraitAnalyzer::analyze_trait_default_method_bodies()
{
    for (const auto& entry : this->core_.state_.checked.traits) {
        const TraitSignature& trait = entry.second;
        const bool has_default_body =
            std::ranges::any_of(trait.requirements, [](const TraitMethodRequirement& requirement) {
                return requirement.has_default_body;
            });
        if (!has_default_body || !syntax::is_valid(trait.item)
            || trait.item.value >= this->core_.ctx_.module.items.size()) {
            continue;
        }

        const syntax::ItemNode trait_item = this->core_.ctx_.module.items[trait.item.value];
        GenericContext context = this->make_trait_generic_context(trait_item);
        this->merge_trait_where_constraints(trait_item, trait, context);
        const base::u32 self_predicate = this->record_trait_self_predicate(trait, trait_item, context);
        if (self_predicate != SEMA_TRAIT_PREDICATE_INVALID_INDEX) {
            context.predicate_indices.push_back(self_predicate);
        }
        for (const TraitMethodRequirement& requirement : trait.requirements) {
            this->analyze_trait_default_method_body(trait, requirement, context);
        }
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
    predicate.index =
        base::checked_u32(this->core_.state_.checked.trait_predicates.size(), SEMA_TRAIT_PREDICATE_ID_CONTEXT);
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
    const base::u32 predicate_index = predicate.index;
    this->core_.state_.checked.trait_predicates.push_back(std::move(predicate));
    return predicate_index;
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
    resolution.return_type = this->substitute_requirement_type(
        requirement.return_type, owner_type, predicate.trait_args, trait, predicate.associated_type_equalities);
    resolution.param_types.reserve(requirement.param_types.size());
    for (const TypeHandle param : requirement.param_types) {
        resolution.param_types.push_back(this->substitute_requirement_type(
            param, owner_type, predicate.trait_args, trait, predicate.associated_type_equalities));
    }
    resolution.dispatch = TraitMethodDispatchKind::param_env;
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
    resolution.dispatch = TraitMethodDispatchKind::impl_override;
    resolution.found = true;
    return resolution;
}

std::string SemanticAnalyzerCore::TraitAnalyzer::trait_default_method_instance_key_name(
    const TraitSignature& trait, const TraitImplInfo& impl, const TraitMethodRequirement& requirement) const
{
    std::string key;
    key += SEMA_TRAIT_DEFAULT_INSTANCE_KEY_PREFIX;
    key += std::to_string(trait.module.value);
    key.push_back(':');
    key += trait.name.view();
    key.push_back('[');
    key += this->core_.state_.checked.types.display_name(impl.self_type);
    for (const TypeHandle arg : impl.trait_args) {
        key.push_back(',');
        key += this->core_.state_.checked.types.display_name(arg);
    }
    key += "]::";
    key += std::to_string(requirement.ordinal);
    key.push_back(':');
    key += requirement.name.view();
    return key;
}

std::string SemanticAnalyzerCore::TraitAnalyzer::trait_default_method_instance_stable_name(
    const TraitSignature& trait, const TraitImplInfo& impl, const TraitMethodRequirement& requirement) const
{
    std::string stable_name;
    stable_name += SEMA_TRAIT_DEFAULT_INSTANCE_STABLE_PREFIX;
    stable_name += this->trait_display_name(trait, impl.trait_args);
    stable_name += ":";
    stable_name += this->core_.state_.checked.types.display_name(impl.self_type);
    stable_name += ":";
    stable_name += std::to_string(requirement.ordinal);
    stable_name += ":";
    stable_name += requirement.name.view();
    return stable_name;
}

FunctionLookupKey SemanticAnalyzerCore::TraitAnalyzer::trait_default_method_instance_key(
    const TraitSignature& trait, const TraitImplInfo& impl, const TraitMethodRequirement& requirement) const
{
    const IdentId key_id =
        this->core_.intern_generated_key(this->trait_default_method_instance_key_name(trait, impl, requirement));
    return this->core_.method_function_lookup_key(requirement.module, impl.self_type, key_id);
}

std::string SemanticAnalyzerCore::TraitAnalyzer::trait_default_method_instance_c_symbol_name(
    const TraitSignature& trait, const TraitImplInfo& impl, const TraitMethodRequirement& requirement) const
{
    return this->core_.method_c_symbol_name(
        impl.self_type, this->trait_default_method_instance_stable_name(trait, impl, requirement));
}

FunctionSignature SemanticAnalyzerCore::TraitAnalyzer::make_trait_default_method_instance_signature(
    const TraitSignature& trait, const TraitImplInfo& impl, const TraitMethodRequirement& requirement,
    const FunctionLookupKey& key) const
{
    FunctionSignature signature = this->core_.state_.checked.make_function_signature();
    signature.name = this->core_.state_.checked.intern_text(requirement.name);
    signature.name_id = requirement.name_id;
    signature.semantic_key = key;
    signature.module = requirement.module;
    signature.trait_module = trait.module;
    signature.trait_name_id = trait.name_id;
    signature.method_owner_type = impl.self_type;
    signature.return_type = this->substitute_requirement_type(
        requirement.return_type, impl.self_type, impl.trait_args, trait, impl.associated_types);
    signature.param_types.reserve(requirement.param_types.size());
    for (const TypeHandle param : requirement.param_types) {
        signature.param_types.push_back(
            this->substitute_requirement_type(param, impl.self_type, impl.trait_args, trait, impl.associated_types));
    }
    signature.range = requirement.range;
    signature.is_unsafe = requirement.is_unsafe;
    signature.is_variadic = requirement.is_variadic;
    signature.has_definition = true;
    signature.is_method = true;
    signature.has_self_param = requirement.has_self_param;
    signature.is_trait_default_method_instance = true;
    signature.visibility = requirement.visibility;
    signature.definition_item = requirement.item;
    signature.part_index = this->core_.item_part_index(requirement.item);
    const std::string stable_name = this->trait_default_method_instance_stable_name(trait, impl, requirement);
    signature.stable_id = sema::stable_definition_id(
        this->core_.stable_module_id(requirement.module), StableSymbolKind::method, stable_name);
    signature.c_name = this->core_.state_.checked.intern_text(
        this->trait_default_method_instance_c_symbol_name(trait, impl, requirement));
    signature.incremental_key = this->core_.stable_incremental_key(signature.stable_id,
        this->core_.function_incremental_fingerprint(
            stable_name, signature.return_type, signature.param_types, true, signature.is_variadic));
    return signature;
}

SemanticAnalyzerCore::GenericContext SemanticAnalyzerCore::TraitAnalyzer::make_trait_default_method_instance_context(
    const TraitSignature& trait, const TraitImplInfo& impl) const
{
    GenericContext context = this->core_.make_generic_context();
    const IdentId self_id = this->core_.ctx_.module.intern_identifier(SEMA_TRAIT_SELF_NAME);
    context.params.emplace(self_id, impl.self_type);
    context.param_identities.emplace(self_id, generic_param_identity_from_text(SEMA_TRAIT_SELF_NAME));
    for (base::usize index = 0; index < trait.generic_params.size() && index < impl.trait_args.size(); ++index) {
        const IdentId param_id = trait.generic_params[index];
        const std::string_view param_name = this->core_.ctx_.module.identifier_text(param_id);
        context.params.emplace(param_id, impl.trait_args[index]);
        context.param_identities.emplace(param_id, generic_param_identity_from_text(param_name));
    }
    return context;
}

SemanticAnalyzerCore::GenericTemplateInfo
SemanticAnalyzerCore::TraitAnalyzer::make_trait_default_method_instance_layout_info(
    const TraitSignature& trait, const TraitImplInfo& impl, const TraitMethodRequirement& requirement) const
{
    GenericTemplateInfo info = this->core_.make_generic_template_info();
    info.item = requirement.item;
    info.module = requirement.module;
    info.part_index = this->core_.item_part_index(requirement.item);
    info.name = this->core_.state_.checked.intern_text(requirement.name);
    info.name_id = requirement.name_id;
    info.function_key = this->trait_default_method_instance_key(trait, impl, requirement);
    info.stable_id = sema::stable_definition_id(this->core_.stable_module_id(requirement.module),
        StableSymbolKind::method, this->trait_default_method_instance_stable_name(trait, impl, requirement));
    info.visibility = requirement.visibility;
    if (syntax::is_valid(requirement.item) && requirement.item.value < this->core_.ctx_.module.items.size()) {
        this->core_.populate_generic_template_node_spans(info, this->core_.ctx_.module.items[requirement.item.value]);
    }
    return info;
}

FunctionSignature* SemanticAnalyzerCore::TraitAnalyzer::instantiate_trait_default_method(
    const TraitSignature& trait, const TraitImplInfo& impl, const TraitMethodRequirement& requirement)
{
    if (!requirement.has_default_body || !syntax::is_valid(requirement.item)
        || requirement.item.value >= this->core_.ctx_.module.items.size()) {
        return nullptr;
    }
    const syntax::ItemNode function = this->core_.ctx_.module.items[requirement.item.value];
    if (!syntax::is_valid(function.body)) {
        return nullptr;
    }

    const FunctionLookupKey key = this->trait_default_method_instance_key(trait, impl, requirement);
    if (const auto found = this->core_.state_.traits.default_method_instances.find(key);
        found != this->core_.state_.traits.default_method_instances.end()) {
        return &this->core_.state_.checked.trait_default_method_instances[found->second].signature;
    }
    if (!this->core_.ctx_.options.retain_generic_side_tables) {
        if (const auto found = this->core_.state_.checked.functions.find(key);
            found != this->core_.state_.checked.functions.end()) {
            return &found->second;
        }
    }

    GenericTemplateInfo layout_info = this->make_trait_default_method_instance_layout_info(trait, impl, requirement);
    FunctionSignature signature = this->make_trait_default_method_instance_signature(trait, impl, requirement, key);
    if (!this->core_.ctx_.options.retain_generic_side_tables) {
        const auto function_inserted = this->core_.state_.checked.functions.emplace(key, signature);
        if (!function_inserted.second) {
            function_inserted.first->second = signature;
        } else {
            this->core_.state_.names.internal_function_lookup_exclusions += 1;
        }
        this->core_.state_.functions.definition_items[key] = requirement.item;
        this->core_.state_.functions.body_states[key] = FunctionBodyState::not_started;
        GenericSideTables transient_side_tables = this->core_.make_generic_instance_side_tables(layout_info);
        GenericContext body_context = this->make_trait_default_method_instance_context(trait, impl);
        {
            TraitAnalysisScope scope(
                this->core_, requirement.module, requirement.item, &body_context, &transient_side_tables, true);
            this->core_.analyze_function_body_with_signature(
                function, key, function_inserted.first->second, this->core_.state_.functions.body_states[key]);
        }
        return &this->core_.state_.checked.functions.at(key);
    }

    TraitDefaultMethodInstanceInfo instance;
    instance.key = key;
    instance.item = requirement.item;
    instance.body = function.body;
    instance.impl_key = impl.key;
    instance.trait_module = trait.module;
    instance.trait_name_id = trait.name_id;
    instance.requirement_ordinal = requirement.ordinal;
    instance.signature = std::move(signature);
    if (layout_info.has_sparse_node_ids()) {
        instance.side_table_layout_index = this->core_.generic_side_table_layout_index(layout_info);
    }
    instance.side_tables = this->core_.make_generic_instance_side_tables(layout_info);
    const base::usize instance_index = this->core_.state_.checked.trait_default_method_instances.size();
    this->core_.state_.checked.trait_default_method_instances.push_back(std::move(instance));
    if (const GenericSideTableLayout* const layout = this->core_.state_.checked.generic_side_table_layout(
            this->core_.state_.checked.trait_default_method_instances[instance_index].side_table_layout_index);
        layout != nullptr) {
        this->core_.state_.checked.trait_default_method_instances[instance_index].side_tables.bind_local_dense_layout(
            *layout);
    }
    this->core_.state_.traits.default_method_instances[key] = instance_index;

    FunctionSignature checked_signature =
        this->core_.state_.checked.trait_default_method_instances[instance_index].signature;
    const auto function_inserted = this->core_.state_.checked.functions.emplace(key, checked_signature);
    if (!function_inserted.second) {
        function_inserted.first->second = checked_signature;
    } else {
        this->core_.state_.names.internal_function_lookup_exclusions += 1;
    }
    this->core_.state_.functions.definition_items[key] = requirement.item;
    this->core_.state_.functions.body_states[key] = FunctionBodyState::not_started;
    GenericContext body_context = this->make_trait_default_method_instance_context(trait, impl);
    {
        TraitAnalysisScope scope(this->core_, requirement.module, requirement.item, &body_context,
            &this->core_.state_.checked.trait_default_method_instances[instance_index].side_tables, true);
        this->core_.analyze_function_body_with_signature(function, key,
            this->core_.state_.checked.trait_default_method_instances[instance_index].signature,
            this->core_.state_.functions.body_states[key]);
    }
    this->core_.state_.checked.trait_default_method_instances[instance_index].signature =
        this->core_.state_.checked.functions.at(key);
    this->core_.state_.checked.trait_default_method_instances[instance_index]
        .side_tables.release_analysis_only_storage();
    return &this->core_.state_.checked.trait_default_method_instances[instance_index].signature;
}

const TraitObjectMethodSlotFact* SemanticAnalyzerCore::TraitAnalyzer::trait_object_method_slot_for_requirement(
    const query::TraitObjectTypeKey& object_key, const base::u32 requirement_ordinal) const noexcept
{
    for (const TraitObjectMethodSlotFact& fact : this->core_.state_.checked.trait_object_method_slots) {
        if (fact.object_type_key == object_key && fact.requirement_ordinal == requirement_ordinal) {
            return &fact;
        }
    }
    return nullptr;
}

VTableMethodSlotFact SemanticAnalyzerCore::TraitAnalyzer::make_vtable_method_slot_fact(const TraitSignature& trait,
    const TraitImplInfo& impl,
    const TraitMethodRequirement& requirement,
    const TraitObjectMethodSlotFact& slot_fact,
    const TypeHandle concrete_type,
    const TypeHandle object_type)
{
    const TraitImplMethodInfo* method_info = nullptr;
    for (const TraitImplMethodInfo& method : impl.methods) {
        if (method.requirement_ordinal == requirement.ordinal) {
            method_info = &method;
            break;
        }
    }

    FunctionLookupKey function_key{};
    const FunctionSignature* signature = nullptr;
    TraitImplMethodOrigin origin = TraitImplMethodOrigin::impl_override;
    if (method_info != nullptr) {
        origin = method_info->origin;
        function_key = method_info->function_key;
        if (origin == TraitImplMethodOrigin::trait_default) {
            signature = this->instantiate_trait_default_method(trait, impl, requirement);
            if (signature != nullptr) {
                function_key = signature->semantic_key;
            }
        } else if (const auto found = this->core_.state_.checked.functions.find(function_key);
                   found != this->core_.state_.checked.functions.end()) {
            signature = &found->second;
        }
    }

    VTableMethodSlotFact fact;
    fact.object_type_key = slot_fact.object_type_key;
    fact.concrete_type = concrete_type;
    fact.object_type = object_type;
    fact.method_name = this->core_.state_.checked.intern_text(requirement.name);
    fact.method_name_id = requirement.name_id;
    fact.function_key = function_key;
    fact.requirement_ordinal = requirement.ordinal;
    fact.slot = slot_fact.slot;
    fact.receiver_type = slot_fact.receiver_type;
    fact.return_type = slot_fact.return_type;
    fact.param_types = this->core_.state_.checked.make_type_handle_list();
    fact.receiver_access = slot_fact.receiver_access;
    fact.origin = origin;
    fact.range = requirement.range;
    fact.part_index = sema_part_index_or_zero(this->core_);

    if (signature != nullptr) {
        fact.receiver_type = signature->param_types.empty() ? slot_fact.receiver_type : signature->param_types.front();
        fact.return_type = signature->return_type;
        fact.param_types.reserve(signature->param_types.size());
        for (const TypeHandle param : signature->param_types) {
            fact.param_types.push_back(param);
        }
        return fact;
    }

    fact.param_types.reserve(requirement.param_types.size());
    for (const TypeHandle param : requirement.param_types) {
        fact.param_types.push_back(
            this->substitute_requirement_type(param, impl.self_type, impl.trait_args, trait, impl.associated_types));
    }
    fact.receiver_type = fact.param_types.empty() ? slot_fact.receiver_type : fact.param_types.front();
    fact.return_type = this->substitute_requirement_type(
        requirement.return_type, impl.self_type, impl.trait_args, trait, impl.associated_types);
    return fact;
}

SemanticAnalyzerCore::TraitMethodCallResolution
SemanticAnalyzerCore::TraitAnalyzer::make_trait_default_method_resolution(
    const TraitSignature& trait, const TraitImplInfo& impl, const TraitMethodRequirement& requirement)
{
    TraitMethodCallResolution resolution;
    resolution.trait = &trait;
    resolution.requirement = &requirement;
    resolution.impl = &impl;
    if (impl.predicate_index < this->core_.state_.checked.trait_predicates.size()) {
        resolution.predicate = &this->core_.state_.checked.trait_predicates[impl.predicate_index];
    }
    const FunctionSignature* const signature = this->instantiate_trait_default_method(trait, impl, requirement);
    if (signature != nullptr) {
        resolution.signature = signature;
        resolution.return_type = signature->return_type;
        resolution.param_types.assign(signature->param_types.begin(), signature->param_types.end());
    } else {
        resolution.return_type = this->substitute_requirement_type(
            requirement.return_type, impl.self_type, impl.trait_args, trait, impl.associated_types);
        resolution.param_types.reserve(requirement.param_types.size());
        for (const TypeHandle param : requirement.param_types) {
            resolution.param_types.push_back(this->substitute_requirement_type(
                param, impl.self_type, impl.trait_args, trait, impl.associated_types));
        }
    }
    resolution.dispatch = TraitMethodDispatchKind::trait_default;
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

bool SemanticAnalyzerCore::TraitAnalyzer::visible_trait_has_associated_type(
    const IdentId name_id, const std::string_view name, const base::SourceRange& range)
{
    static_cast<void>(name);
    for (const auto& entry : this->core_.state_.checked.traits) {
        const TraitSignature& trait = entry.second;
        if (!this->trait_visible_for_method_call(trait, range)) {
            continue;
        }
        if (this->find_trait_associated_type_requirement(trait, name_id) != nullptr) {
            return true;
        }
    }
    return false;
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

TypeHandle SemanticAnalyzerCore::TraitAnalyzer::resolve_associated_type_projection(const TypeHandle base_type,
    const IdentId associated_name_id, const std::string_view associated_name, const base::SourceRange& range,
    const bool report_failure)
{
    if (!is_valid(base_type) || base_type.value >= this->core_.state_.checked.types.size()) {
        return INVALID_TYPE_HANDLE;
    }
    const TypeInfo& base_info = this->core_.state_.checked.types.get(base_type);
    if (base_info.kind != TypeKind::generic_param || !is_valid(base_info.generic_identity)) {
        if (const TraitAssociatedTypeRequirement* const current_trait_associated =
                this->find_current_trait_associated_type_requirement(associated_name_id);
            current_trait_associated != nullptr) {
            const TypeHandle resolved =
                this->resolve_current_trait_concrete_associated_type(base_type, *current_trait_associated);
            if (is_valid(resolved)) {
                return resolved;
            }
        }
        if (report_failure) {
            this->core_.report_lookup(range,
                sema_unknown_associated_type_projection_message(
                    this->core_.state_.checked.types.display_name(base_type), associated_name));
        }
        return INVALID_TYPE_HANDLE;
    }

    if (const TraitAssociatedTypeRequirement* const current_trait_associated =
            this->find_current_trait_associated_type_requirement(associated_name_id);
        current_trait_associated != nullptr) {
        return this->core_.state_.checked.types.associated_projection(
            base_type, current_trait_associated->member_key, current_trait_associated->name);
    }

    if (this->core_.state_.flow.current_generic_context == nullptr) {
        if (report_failure) {
            this->core_.report_lookup(range,
                sema_unknown_associated_type_projection_message(
                    this->core_.state_.checked.types.display_name(base_type), associated_name));
        }
        return INVALID_TYPE_HANDLE;
    }

    const TraitSignature* selected_trait = nullptr;
    const TraitAssociatedTypeRequirement* selected_associated_type = nullptr;
    const TraitImplAssociatedTypeInfo* selected_equality = nullptr;
    std::string first_trait_name;
    for (const base::u32 predicate_index : this->core_.state_.flow.current_generic_context->predicate_indices) {
        if (predicate_index >= this->core_.state_.checked.trait_predicates.size()) {
            continue;
        }
        const TraitPredicate& predicate = this->core_.state_.checked.trait_predicates[predicate_index];
        if (predicate.kind != TraitPredicateKind::declared_trait
            || predicate.subject_param_identity != base_info.generic_identity) {
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
        const TraitAssociatedTypeRequirement* const associated_type =
            this->find_trait_associated_type_requirement(trait, associated_name_id);
        if (associated_type == nullptr) {
            continue;
        }
        if (selected_associated_type != nullptr) {
            if (report_failure) {
                this->core_.report_lookup(range,
                    sema_ambiguous_associated_type_projection_message(
                        this->core_.state_.checked.types.display_name(base_type), associated_name, first_trait_name,
                        this->trait_display_name(trait, predicate.trait_args)));
            }
            return INVALID_TYPE_HANDLE;
        }
        selected_trait = &trait;
        selected_associated_type = associated_type;
        first_trait_name = this->trait_display_name(trait, predicate.trait_args);
        const auto equality = std::ranges::find_if(
            predicate.associated_type_equalities, [&](const TraitImplAssociatedTypeInfo& candidate) {
                return candidate.member_key == associated_type->member_key;
            });
        if (equality != predicate.associated_type_equalities.end()) {
            selected_equality = &*equality;
        }
    }

    if (selected_equality != nullptr) {
        return selected_equality->value_type;
    }
    if (selected_associated_type != nullptr) {
        static_cast<void>(selected_trait);
        return this->core_.state_.checked.types.associated_projection(
            base_type, selected_associated_type->member_key, selected_associated_type->name);
    }
    if (report_failure) {
        if (this->visible_trait_has_associated_type(associated_name_id, associated_name, range)) {
            this->core_.report_capability(range,
                sema_associated_type_projection_missing_bound_message(
                    this->core_.state_.checked.types.display_name(base_type), associated_name));
        } else {
            this->core_.report_lookup(range,
                sema_unknown_associated_type_projection_message(
                    this->core_.state_.checked.types.display_name(base_type), associated_name));
        }
    }
    return INVALID_TYPE_HANDLE;
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
        TraitMethodCallResolution candidate_resolution;
        if (method->origin == TraitImplMethodOrigin::trait_default) {
            if (!requirement->has_default_body) {
                continue;
            }
            candidate_resolution = this->make_trait_default_method_resolution(*trait, impl, *requirement);
        } else {
            const auto signature = this->core_.state_.checked.functions.find(method->function_key);
            if (signature == this->core_.state_.checked.functions.end()) {
                continue;
            }
            if (require_self && !signature->second.has_self_param) {
                continue;
            }
            candidate_resolution =
                this->make_impl_trait_method_resolution(*trait, impl, *requirement, signature->second);
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
        result = std::move(candidate_resolution);
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

const TraitImplInfo* SemanticAnalyzerCore::TraitAnalyzer::find_trait_object_impl(const TypeHandle concrete_type,
    const TypeInfo& object_info,
    const base::SourceRange& range,
    const bool report_failure)
{
    if (!is_valid(concrete_type) || object_info.kind != TypeKind::trait_object) {
        return nullptr;
    }
    const TraitImplInfo* result = nullptr;
    for (const auto& entry : this->core_.state_.checked.trait_impls) {
        const TraitImplInfo& impl = entry.second;
        if (impl.trait_module.value != object_info.trait_object_module.value
            || impl.trait_name_id != object_info.trait_object_name_id
            || !this->core_.state_.checked.types.same(impl.self_type, concrete_type)
            || impl.trait_args.size() != object_info.trait_object_args.size()) {
            continue;
        }
        bool args_match = true;
        for (base::usize index = 0; index < impl.trait_args.size(); ++index) {
            args_match = args_match
                && this->core_.state_.checked.types.same(impl.trait_args[index], object_info.trait_object_args[index]);
        }
        if (!args_match) {
            continue;
        }
        const std::vector<TraitImplAssociatedTypeInfo> object_equalities =
            dyn_trait_object_equalities_as_impl_equalities(this->core_.state_.checked, object_info);
        if (!dyn_trait_associated_equalities_match(
                this->core_.state_.checked.types, object_equalities, impl.associated_types)) {
            continue;
        }
        if (result != nullptr) {
            if (report_failure) {
                const TraitSignature* const first_trait = this->trait_signature_for_impl(*result);
                const TraitSignature* const second_trait = this->trait_signature_for_impl(impl);
                this->core_.report_lookup(range,
                    sema_ambiguous_trait_method_message(this->core_.state_.checked.types.display_name(concrete_type),
                        object_info.trait_object_name,
                        first_trait == nullptr ? result->trait_name.view()
                                               : this->trait_display_name(*first_trait, result->trait_args),
                        second_trait == nullptr ? impl.trait_name.view()
                                                : this->trait_display_name(*second_trait, impl.trait_args)));
            }
            return nullptr;
        }
        result = &impl;
    }
    if (result == nullptr && report_failure) {
        this->core_.report_type(range,
            sema_dyn_trait_impl_missing_message(this->core_.state_.checked.types.display_name(concrete_type),
                object_info.trait_object_name));
    }
    return result;
}

query::VTableLayoutKey SemanticAnalyzerCore::TraitAnalyzer::record_vtable_layout(const TypeHandle concrete_type,
    const TypeHandle object_type,
    const base::SourceRange& range)
{
    if (!is_valid(object_type) || object_type.value >= this->core_.state_.checked.types.size()) {
        return {};
    }
    const TypeInfo& object_info = this->core_.state_.checked.types.get(object_type);
    if (object_info.kind != TypeKind::trait_object || !query::is_valid(object_info.trait_object_key)) {
        return {};
    }
    const TraitImplInfo* const impl = this->find_trait_object_impl(concrete_type, object_info, range, true);
    if (impl == nullptr) {
        return {};
    }
    base::Result<query::CanonicalTypeKey> concrete_key = this->core_.checked_canonical_type_key(concrete_type);
    if (!concrete_key) {
        this->core_.report_internal_contract(range, concrete_key.error().message);
        return {};
    }
    const base::u32 slot_count =
        dyn_trait_method_slot_count(this->core_.state_.checked, object_info.trait_object_key);
    const query::TraitObjectMetadataPolicyKey metadata_policy =
        dyn_trait_vtable_metadata_policy(this->core_.state_.checked, object_info.trait_object_key);
    const query::VTableLayoutKey layout = query::vtable_layout_key(concrete_key.take_value(),
        object_info.trait_object_key, object_info.trait_object_key.object_callability_schema,
        impl->coherence_fingerprint, slot_count, metadata_policy);
    if (!query::is_valid(layout)) {
        this->core_.report_internal_contract(range, "failed to create dyn trait vtable layout key");
        return {};
    }
    if (!std::ranges::any_of(this->core_.state_.checked.vtable_layouts, [&](const VTableLayoutFact& fact) {
            return fact.layout_key == layout;
        })) {
        const TraitSignature* const trait = this->trait_signature_for_impl(*impl);
        if (trait == nullptr) {
            this->core_.report_internal_contract(range, "failed to resolve dyn trait vtable trait signature");
            return {};
        }
        VTableLayoutFact fact = this->core_.state_.checked.make_vtable_layout_fact();
        fact.layout_key = layout;
        fact.concrete_type = concrete_type;
        fact.object_type = object_type;
        fact.impl_key = impl->key;
        fact.impl_evidence = impl->coherence_fingerprint;
        fact.method_slot_count = slot_count;
        fact.method_slots.reserve(slot_count);
        for (const TraitMethodRequirement& requirement : trait->requirements) {
            const TraitObjectMethodSlotFact* const slot_fact =
                this->trait_object_method_slot_for_requirement(object_info.trait_object_key, requirement.ordinal);
            if (slot_fact == nullptr) {
                continue;
            }
            VTableMethodSlotFact slot =
                this->make_vtable_method_slot_fact(*trait, *impl, requirement, *slot_fact, concrete_type, object_type);
            if (!is_valid(slot.function_key)) {
                this->core_.report_internal_contract(
                    requirement.range, "failed to bind dyn trait vtable method slot to a function");
                return {};
            }
            fact.method_slots.push_back(std::move(slot));
        }
        if (fact.method_slots.size() != slot_count) {
            this->core_.report_internal_contract(range, "dyn trait vtable method slot witness count mismatch");
            return {};
        }
        std::ranges::sort(fact.method_slots, [](const VTableMethodSlotFact& lhs,
                                                 const VTableMethodSlotFact& rhs) noexcept {
            return lhs.slot < rhs.slot;
        });
        fact.range = range;
        fact.part_index = sema_part_index_or_zero(this->core_);
        this->core_.state_.checked.vtable_layouts.push_back(fact);
    }
    return layout;
}

SemanticAnalyzerCore::TraitMethodCallResolution
SemanticAnalyzerCore::TraitAnalyzer::resolve_direct_dyn_trait_method_call(const TypeHandle object_type,
    const TypeInfo& object_info,
    const TraitSignature& trait,
    const IdentId name_id,
    const std::string_view name,
    const base::SourceRange& range,
    const bool report_failure) const
{
    TraitMethodCallResolution resolution;
    if (!object_info.trait_object_principal_types.empty()) {
        if (report_failure) {
            this->core_.report_type(range, sema_dyn_trait_composition_method_ambiguous_message(name));
        }
        resolution.reported_failure = report_failure;
        return resolution;
    }
    const TraitMethodRequirement* const requirement = this->find_trait_method_requirement(trait, name_id, true);
    if (requirement == nullptr) {
        if (report_failure) {
            this->core_.report_lookup(range,
                sema_trait_method_impl_missing_message(
                    this->core_.state_.checked.types.display_name(object_type), name));
        }
        resolution.reported_failure = report_failure;
        return resolution;
    }
    const TraitObjectMethodSlotFact* const slot_fact =
        dyn_trait_method_slot_fact(this->core_.state_.checked, object_info.trait_object_key, name_id);
    if (slot_fact == nullptr) {
        if (report_failure) {
            this->core_.report_type(range, sema_dyn_trait_receiver_message(trait.name, requirement->name));
        }
        resolution.reported_failure = report_failure;
        return resolution;
    }
    const std::vector<TraitImplAssociatedTypeInfo> associated_equalities =
        dyn_trait_object_equalities_as_impl_equalities(this->core_.state_.checked, object_info);
    resolution.trait = &trait;
    resolution.requirement = requirement;
    resolution.return_type =
        this->substitute_requirement_type(requirement->return_type, object_type, object_info.trait_object_args, trait,
            associated_equalities);
    resolution.param_types.reserve(requirement->param_types.size());
    for (const TypeHandle param : requirement->param_types) {
        resolution.param_types.push_back(
            this->substitute_requirement_type(param, object_type, object_info.trait_object_args, trait,
                associated_equalities));
    }
    resolution.dispatch = TraitMethodDispatchKind::vtable_slot;
    resolution.vtable_slot = slot_fact->slot;
    resolution.dispatch_receiver_type = object_type;
    resolution.found = true;
    return resolution;
}

SemanticAnalyzerCore::TraitMethodCallResolution
SemanticAnalyzerCore::TraitAnalyzer::resolve_supertrait_dyn_trait_method_call(const TypeHandle object_type,
    const TypeInfo& object_info,
    const IdentId name_id,
    const std::string_view name,
    const base::SourceRange& range)
{
    TraitMethodCallResolution result;
    for (const TraitSupertraitEdgeFact& edge : this->core_.state_.checked.trait_supertrait_edges) {
        if (!trait_keys_match(edge.child_trait_key, object_info.trait_object_key.principal_trait)) {
            continue;
        }
        const TraitSignature* parent_trait = nullptr;
        for (const auto& entry : this->core_.state_.checked.traits) {
            if (trait_keys_match(this->trait_query_key(entry.second), edge.parent_trait_key)) {
                parent_trait = &entry.second;
                break;
            }
        }
        if (parent_trait == nullptr || this->find_trait_method_requirement(*parent_trait, name_id, true) == nullptr) {
            continue;
        }
        const TypeHandle parent_object_type =
            this->instantiate_supertrait_object_type(edge, object_info, *parent_trait);
        if (!is_valid(parent_object_type)) {
            continue;
        }
        const TypeInfo& parent_object_info = this->core_.state_.checked.types.get(parent_object_type);
        if (!this->supertrait_edge_matches_target_object(edge, object_info, parent_object_info)) {
            continue;
        }
        const std::vector<TraitImplAssociatedTypeInfo> associated_equalities =
            dyn_trait_object_equalities_as_impl_equalities(this->core_.state_.checked, parent_object_info);
        this->record_trait_object_callability(
            parent_object_type, *parent_trait, parent_object_info.trait_object_args, associated_equalities, range);
        TraitMethodCallResolution candidate = this->resolve_direct_dyn_trait_method_call(
            parent_object_type, parent_object_info, *parent_trait, name_id, name, range, false);
        if (!candidate.found) {
            continue;
        }
        if (result.found) {
            TraitMethodCallResolution ambiguous;
            ambiguous.reported_failure = true;
            this->core_.report_lookup(range,
                "ambiguous supertrait dyn method `" + std::string(name) + "` on `"
                    + this->core_.state_.checked.types.display_name(object_type) + "`");
            return ambiguous;
        }
        candidate.dispatch_receiver_type = parent_object_type;
        result = std::move(candidate);
    }
    return result;
}

SemanticAnalyzerCore::TraitMethodCallResolution
SemanticAnalyzerCore::TraitAnalyzer::resolve_principal_set_dyn_trait_method_call(const TypeHandle object_type,
    const TypeInfo& object_info,
    const IdentId name_id,
    const std::string_view name,
    const base::SourceRange& range,
    const bool report_failure)
{
    TraitMethodCallResolution result;
    for (const TypeHandle principal : object_info.trait_object_principal_types) {
        if (!is_valid(principal) || principal.value >= this->core_.state_.checked.types.size()) {
            continue;
        }
        const TypeInfo& principal_info = this->core_.state_.checked.types.get(principal);
        if (principal_info.kind != TypeKind::trait_object || !query::is_valid(principal_info.trait_object_key)) {
            continue;
        }
        const auto trait_found = this->core_.state_.checked.traits.find(ModuleLookupKey{
            principal_info.trait_object_module.value,
            principal_info.trait_object_name_id,
        });
        if (trait_found == this->core_.state_.checked.traits.end()) {
            continue;
        }
        TraitMethodCallResolution candidate = this->resolve_direct_dyn_trait_method_call(
            principal, principal_info, trait_found->second, name_id, name, range, false);
        if (!candidate.found) {
            continue;
        }
        if (result.found) {
            TraitMethodCallResolution ambiguous;
            ambiguous.reported_failure = report_failure;
            if (report_failure) {
                this->core_.report_type(range, sema_dyn_trait_composition_method_ambiguous_message(name));
            }
            return ambiguous;
        }
        candidate.dispatch_receiver_type = principal;
        result = std::move(candidate);
    }
    if (!result.found && report_failure) {
        this->core_.report_lookup(range,
            sema_trait_method_impl_missing_message(this->core_.state_.checked.types.display_name(object_type), name));
        result.reported_failure = true;
    }
    return result;
}

SemanticAnalyzerCore::TraitMethodCallResolution
SemanticAnalyzerCore::TraitAnalyzer::resolve_dyn_trait_method_call(const TypeHandle object_type,
    const IdentId name_id,
    const std::string_view name,
    const base::SourceRange& range,
    const bool report_failure)
{
    TraitMethodCallResolution resolution;
    if (!is_valid(object_type) || object_type.value >= this->core_.state_.checked.types.size()) {
        return resolution;
    }
    const TypeInfo& object_info = this->core_.state_.checked.types.get(object_type);
    if (object_info.kind != TypeKind::trait_object) {
        return resolution;
    }
    if (!object_info.trait_object_principal_types.empty()) {
        return this->resolve_principal_set_dyn_trait_method_call(
            object_type, object_info, name_id, name, range, report_failure);
    }
    if (!query::is_valid(object_info.trait_object_key)) {
        return resolution;
    }
    const auto trait_found = this->core_.state_.checked.traits.find(ModuleLookupKey{
        object_info.trait_object_module.value,
        object_info.trait_object_name_id,
    });
    if (trait_found == this->core_.state_.checked.traits.end()) {
        return resolution;
    }
    const TraitSignature& trait = trait_found->second;
    resolution = this->resolve_direct_dyn_trait_method_call(
        object_type, object_info, trait, name_id, name, range, false);
    if (resolution.found) {
        return resolution;
    }
    resolution = this->resolve_supertrait_dyn_trait_method_call(object_type, object_info, name_id, name, range);
    if (resolution.found || resolution.reported_failure) {
        return resolution;
    }
    if (report_failure) {
        return this->resolve_direct_dyn_trait_method_call(
            object_type, object_info, trait, name_id, name, range, true);
    }
    return resolution;
}

std::string SemanticAnalyzerCore::TraitAnalyzer::trait_display_name(
    const TraitSignature& trait, const std::span<const TypeHandle> trait_args) const
{
    return this->core_.state_.checked.types.display_name(trait.name, trait_args);
}

TypeHandle SemanticAnalyzerCore::TraitAnalyzer::substitute_requirement_type(const TypeHandle type,
    const TypeHandle self_type, const std::span<const TypeHandle> trait_args, const TraitSignature& trait,
    const std::span<const TraitImplAssociatedTypeInfo> associated_types) const
{
    std::unordered_map<GenericParamIdentity, TypeHandle, GenericParamIdentityHash> substitutions;
    substitutions.reserve(trait_args.size() + 1U);
    if (is_valid(self_type)) {
        substitutions.emplace(generic_param_identity_from_text(SEMA_TRAIT_SELF_NAME), self_type);
    }
    for (base::usize index = 0; index < trait_args.size() && index < trait.generic_params.size(); ++index) {
        const std::string_view name = this->core_.ctx_.module.identifier_text(trait.generic_params[index]);
        substitutions.emplace(generic_param_identity_from_text(name), trait_args[index]);
    }
    return substitute_trait_type_iterative(this->core_.state_.checked.types, type, substitutions, associated_types);
}

bool SemanticAnalyzerCore::TraitAnalyzer::trait_impl_method_matches(const TraitMethodRequirement& requirement,
    const FunctionSignature& signature, const TypeHandle self_type, const std::span<const TypeHandle> trait_args,
    const TraitSignature& trait, const std::span<const TraitImplAssociatedTypeInfo> associated_types) const
{
    if (!trait_method_signature_shape_matches(requirement, signature)) {
        return false;
    }
    const TypeHandle expected_return =
        this->substitute_requirement_type(requirement.return_type, self_type, trait_args, trait, associated_types);
    if (!this->core_.state_.checked.types.same(expected_return, signature.return_type)) {
        return false;
    }
    for (base::usize index = 0; index < requirement.param_types.size(); ++index) {
        const TypeHandle expected_param = this->substitute_requirement_type(
            requirement.param_types[index], self_type, trait_args, trait, associated_types);
        if (!this->core_.state_.checked.types.same(expected_param, signature.param_types[index])) {
            return false;
        }
    }
    if (requirement.has_borrow_contract) {
        const auto impl_contract = this->core_.state_.checked.borrow_contracts.find(signature.semantic_key);
        if (impl_contract != this->core_.state_.checked.borrow_contracts.end()
            && !SemanticAnalyzerCore::BorrowContractAnalyzer(this->core_)
                .contract_is_subset(impl_contract->second, requirement.borrow_contract)) {
            return false;
        }
    }
    return true;
}

void SemanticAnalyzerCore::TraitAnalyzer::validate_trait_impl_method_borrow_contract(
    const TraitSignature& trait, const TraitImplInfo& impl, const TraitImplMethodInfo& method)
{
    if (method.origin != TraitImplMethodOrigin::impl_override) {
        return;
    }
    const auto requirement =
        std::ranges::find_if(trait.requirements, [&method](const TraitMethodRequirement& candidate) {
            return candidate.ordinal == method.requirement_ordinal;
        });
    if (requirement == trait.requirements.end() || !requirement->has_borrow_contract) {
        return;
    }
    const auto impl_contract = this->core_.state_.checked.borrow_contracts.find(method.function_key);
    if (impl_contract != this->core_.state_.checked.borrow_contracts.end()
        && SemanticAnalyzerCore::BorrowContractAnalyzer(this->core_)
            .contract_is_subset(impl_contract->second, requirement->borrow_contract)) {
        return;
    }

    base::SourceRange method_range = impl.range;
    if (syntax::is_valid(method.item) && method.item.value < this->core_.ctx_.module.items.size()) {
        method_range = this->core_.ctx_.module.items.range(method.item.value);
    }
    const base::SourceRange requirement_range = requirement->borrow_contract.range;
    this->core_.report_general(method_range, std::string(SEMA_BORROW_CONTRACT_MISMATCH));
    this->core_.report_note(
        requirement_range, SemanticDiagnosticKind::general, std::string(SEMA_BORROW_CONTRACT_DECLARED_HERE));
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
        this->core_.report_note(impl_block.range, SemanticDiagnosticKind::type_mismatch,
            sema_trait_impl_orphan_rule_note_message(trait_name, self_name));
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
    std::unordered_map<IdentId, const TraitAssociatedTypeRequirement*, IdentIdHash> associated_types_by_name;
    associated_types_by_name.reserve(trait.associated_types.size());
    for (const TraitAssociatedTypeRequirement& associated_type : trait.associated_types) {
        associated_types_by_name.emplace(associated_type.name_id, &associated_type);
    }
    std::unordered_map<IdentId, base::SourceRange, IdentIdHash> seen_associated_types;
    seen_associated_types.reserve(impl_block.impl_items.size());
    std::unordered_set<IdentId, IdentIdHash> implemented_associated_types;
    implemented_associated_types.reserve(impl_block.impl_items.size());
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
    impl_info.associated_types.reserve(impl_block.impl_items.size());
    impl_info.methods.reserve(trait.requirements.size());
    impl_info.predicate_index = this->record_trait_impl_predicate(trait, impl_info, *coherence_fingerprint);

    for (const syntax::ItemId associated_type_id : impl_block.impl_items) {
        if (!syntax::is_valid(associated_type_id) || associated_type_id.value >= this->core_.ctx_.module.items.size()) {
            continue;
        }
        const syntax::ItemNode associated_item = this->core_.ctx_.module.items[associated_type_id.value];
        if (associated_item.kind != syntax::ItemKind::type_alias) {
            continue;
        }
        const auto inserted = seen_associated_types.emplace(associated_item.name_id, associated_item.range);
        if (!inserted.second) {
            this->core_.report_duplicate(associated_item.range,
                sema_duplicate_trait_impl_associated_type_message(trait_name, self_name, associated_item.name));
            this->core_.report_note(inserted.first->second, SemanticDiagnosticKind::duplicate,
                sema_previous_declaration_note_message(associated_item.name));
            continue;
        }
        const auto requirement = associated_types_by_name.find(associated_item.name_id);
        if (requirement == associated_types_by_name.end() || requirement->second == nullptr) {
            this->core_.report_lookup(associated_item.range,
                sema_trait_impl_unknown_associated_type_message(trait_name, self_name, associated_item.name));
            continue;
        }
        if (!associated_item.generic_params.empty() || !associated_item.where_constraints.empty()) {
            this->core_.report_unsupported(
                associated_item.range, std::string(SEMA_ASSOCIATED_TYPE_GENERIC_UNSUPPORTED));
            continue;
        }
        if (!syntax::is_valid(associated_item.alias_type)) {
            this->core_.report_type(associated_item.range,
                sema_trait_impl_missing_associated_type_message(trait_name, self_name, associated_item.name));
            continue;
        }

        TraitImplAssociatedTypeInfo associated_type = this->core_.state_.checked.make_trait_impl_associated_type_info();
        associated_type.name = this->core_.source_name_text(associated_item.name_id, associated_item.name);
        associated_type.name_id = associated_item.name_id;
        associated_type.item = associated_type_id;
        associated_type.syntax_type = associated_item.alias_type;
        associated_type.value_type = this->core_.resolve_type(associated_item.alias_type);
        associated_type.member_key = requirement->second->member_key;
        associated_type.requirement_ordinal = requirement->second->ordinal;
        implemented_associated_types.insert(associated_item.name_id);
        impl_info.associated_types.push_back(associated_type);
    }

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
        if (!this->trait_impl_method_matches(*requirement->second, signature->second, self_type,
                trait_reference.trait_args, trait, impl_info.associated_types)) {
            this->core_.report_type(
                method.range, sema_trait_impl_method_signature_message(trait_name, self_name, method.name));
            if (requirement->second->has_default_body) {
                this->core_.report_note(requirement->second->range, SemanticDiagnosticKind::type_mismatch,
                    std::string(SEMA_TRAIT_DEFAULT_OVERRIDE_SIGNATURE_NOTE));
            }
            continue;
        }
        implemented.insert(method.name_id);
        TraitImplMethodInfo method_info = this->core_.state_.checked.make_trait_impl_method_info();
        method_info.name = this->core_.source_name_text(method.name_id, method.name);
        method_info.name_id = method.name_id;
        method_info.item = method_id;
        method_info.function_key = function_key;
        method_info.requirement_ordinal = requirement->second->ordinal;
        method_info.origin = TraitImplMethodOrigin::impl_override;
        impl_info.methods.push_back(method_info);
    }

    for (const TraitAssociatedTypeRequirement& associated_type : trait.associated_types) {
        if (!implemented_associated_types.contains(associated_type.name_id)) {
            this->core_.report_type(impl_block.range,
                sema_trait_impl_missing_associated_type_message(trait_name, self_name, associated_type.name));
        }
    }

    for (const TraitMethodRequirement& requirement : trait.requirements) {
        if (implemented.contains(requirement.name_id)) {
            continue;
        }
        if (requirement.has_default_body && !seen_methods.contains(requirement.name_id)) {
            TraitImplMethodInfo method_info = this->core_.state_.checked.make_trait_impl_method_info();
            method_info.name = this->core_.state_.checked.intern_text(requirement.name);
            method_info.name_id = requirement.name_id;
            method_info.item = requirement.item;
            method_info.requirement_ordinal = requirement.ordinal;
            method_info.origin = TraitImplMethodOrigin::trait_default;
            impl_info.methods.push_back(method_info);
            continue;
        }
        this->core_.report_type(
            impl_block.range, sema_trait_impl_missing_method_message(trait_name, self_name, requirement.name));
    }

    this->record_trait_impl_evidence(impl_info);
    this->core_.state_.checked.trait_impls.emplace(impl_key, std::move(impl_info));
}

void SemanticAnalyzerCore::TraitAnalyzer::validate_trait_impls()
{
    for (base::u32 item_index = 0; item_index < this->core_.ctx_.module.items.size(); ++item_index) {
        const syntax::ItemNode item = this->core_.ctx_.module.items[item_index];
        if (item.kind == syntax::ItemKind::impl_block && syntax::is_valid(item.trait_type)) {
            if (this->core_.is_destructor_impl_block(item)) {
                continue;
            }
            this->validate_trait_impl_block(item, syntax::ItemId{item_index});
        }
    }
    this->validate_supertrait_impl_obligations();
}

void SemanticAnalyzerCore::TraitAnalyzer::validate_trait_impl_borrow_contracts()
{
    for (const auto& entry : this->core_.state_.checked.trait_impls) {
        const TraitImplInfo& impl = entry.second;
        const TraitSignature* const trait = this->trait_signature_for_impl(impl);
        if (trait == nullptr) {
            continue;
        }
        for (const TraitImplMethodInfo& method : impl.methods) {
            this->validate_trait_impl_method_borrow_contract(*trait, impl, method);
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

void SemanticAnalyzerCore::validate_trait_impl_borrow_contracts()
{
    TraitAnalyzer(*this).validate_trait_impl_borrow_contracts();
}

void SemanticAnalyzerCore::analyze_trait_default_method_bodies()
{
    TraitAnalyzer(*this).analyze_trait_default_method_bodies();
}

const TraitSignature* SemanticAnalyzerCore::find_trait_in_visible_modules(
    const IdentId name_id, const std::string_view name, const base::SourceRange& range, const bool report_unknown)
{
    return TraitAnalyzer(*this).find_trait_in_visible_modules(name_id, name, range, report_unknown);
}

const TraitSignature* SemanticAnalyzerCore::find_trait_in_module(const syntax::ModuleId module,
    const IdentId name_id,
    const std::string_view name,
    const base::SourceRange& range,
    const bool report_unknown)
{
    return TraitAnalyzer(*this).find_trait_in_module(module, name_id, name, range, report_unknown);
}

query::DefKey SemanticAnalyzerCore::trait_query_key(const TraitSignature& trait) noexcept
{
    return TraitAnalyzer(*this).trait_query_key(trait);
}

query::StableFingerprint128 SemanticAnalyzerCore::trait_object_slot_schema(const TraitSignature& trait,
    const std::span<const TypeHandle> trait_args,
    const std::span<const TraitImplAssociatedTypeInfo> associated_equalities) const
{
    return TraitAnalyzer(const_cast<SemanticAnalyzerCore&>(*this))
        .trait_object_slot_schema(trait, trait_args, associated_equalities);
}

bool SemanticAnalyzerCore::validate_trait_object_callability(const TraitSignature& trait,
    const TypeHandle object_type,
    const std::span<const TypeHandle> trait_args,
    const std::span<const TraitImplAssociatedTypeInfo> associated_equalities,
    const base::SourceRange& range,
    const bool report_failure)
{
    return TraitAnalyzer(*this).validate_trait_object_callability(
        trait, object_type, trait_args, associated_equalities, range, report_failure);
}

void SemanticAnalyzerCore::record_trait_object_callability(const TypeHandle object_type,
    const TraitSignature& trait,
    const std::span<const TypeHandle> trait_args,
    const std::span<const TraitImplAssociatedTypeInfo> associated_equalities,
    const base::SourceRange& range)
{
    TraitAnalyzer(*this).record_trait_object_callability(object_type, trait, trait_args, associated_equalities, range);
}

const TraitSupertraitEdgeFact* SemanticAnalyzerCore::find_supertrait_edge_path(
    const TypeInfo& source_object, const TypeInfo& target_object) const
{
    return TraitAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).find_supertrait_edge_path(
        source_object, target_object);
}

const TraitImplInfo* SemanticAnalyzerCore::find_trait_object_impl(const TypeHandle concrete_type,
    const TypeInfo& object_info,
    const base::SourceRange& range,
    const bool report_failure)
{
    return TraitAnalyzer(*this).find_trait_object_impl(concrete_type, object_info, range, report_failure);
}

query::VTableLayoutKey SemanticAnalyzerCore::record_vtable_layout(
    const TypeHandle concrete_type, const TypeHandle object_type, const base::SourceRange& range)
{
    return TraitAnalyzer(*this).record_vtable_layout(concrete_type, object_type, range);
}

SemanticAnalyzerCore::TraitMethodCallResolution SemanticAnalyzerCore::resolve_dyn_trait_method_call(
    const TypeHandle object_type, const IdentId name_id, const std::string_view name, const base::SourceRange& range,
    const bool report_failure)
{
    return TraitAnalyzer(*this).resolve_dyn_trait_method_call(object_type, name_id, name, range, report_failure);
}

SemanticAnalyzerCore::TraitMethodCallResolution SemanticAnalyzerCore::resolve_trait_method_call(
    const TypeHandle owner_type, const IdentId name_id, const std::string_view name, const base::SourceRange& range,
    const bool require_self, const bool report_failure)
{
    return TraitAnalyzer(*this).resolve_trait_method_call(
        owner_type, name_id, name, range, require_self, report_failure);
}

TypeHandle SemanticAnalyzerCore::resolve_associated_type_projection(const TypeHandle base_type,
    const IdentId associated_name_id, const std::string_view associated_name, const base::SourceRange& range,
    const bool report_failure)
{
    return TraitAnalyzer(*this).resolve_associated_type_projection(
        base_type, associated_name_id, associated_name, range, report_failure);
}

bool SemanticAnalyzerCore::is_trait_requirement_item(const syntax::ItemId item) const
{
    return syntax::is_valid(item) && this->state_.traits.requirement_items.contains(item.value);
}

} // namespace aurex::sema
