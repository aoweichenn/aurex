#pragma once

#include <optional>
#include <vector>

#include <frontend/sema/internal/core/private/sema_core.hpp>

namespace aurex::sema {

class SemanticAnalyzerCore::TraitAnalyzer final {
public:
    explicit TraitAnalyzer(SemanticAnalyzerCore& core) noexcept;

    void register_trait_name(const syntax::ItemNode& item, syntax::ItemId item_id);
    void register_trait_signatures();
    void validate_trait_impls();
    void validate_trait_impl_borrow_contracts();
    void analyze_trait_default_method_bodies();
    [[nodiscard]] const TraitSignature* find_trait_in_visible_modules(
        IdentId name_id, std::string_view name, const base::SourceRange& range, bool report_unknown = true);
    [[nodiscard]] const TraitSignature* find_trait_in_module(syntax::ModuleId module, IdentId name_id,
        std::string_view name, const base::SourceRange& range, bool report_unknown = true);
    [[nodiscard]] query::DefKey trait_query_key(const TraitSignature& trait) const noexcept;
    [[nodiscard]] query::StableFingerprint128 trait_object_slot_schema(const TraitSignature& trait,
        std::span<const TypeHandle> trait_args,
        std::span<const TraitImplAssociatedTypeInfo> associated_equalities) const;
    [[nodiscard]] bool validate_trait_object_callability(const TraitSignature& trait, TypeHandle object_type,
        std::span<const TypeHandle> trait_args,
        std::span<const TraitImplAssociatedTypeInfo> associated_equalities, const base::SourceRange& range,
        bool report_failure);
    void record_trait_object_callability(TypeHandle object_type, const TraitSignature& trait,
        std::span<const TypeHandle> trait_args,
        std::span<const TraitImplAssociatedTypeInfo> associated_equalities, const base::SourceRange& range);
    [[nodiscard]] const TraitImplInfo* find_trait_object_impl(
        TypeHandle concrete_type, const TypeInfo& object_info, const base::SourceRange& range, bool report_failure);
    [[nodiscard]] query::VTableLayoutKey record_vtable_layout(
        TypeHandle concrete_type, TypeHandle object_type, const base::SourceRange& range);
    [[nodiscard]] TraitMethodCallResolution resolve_dyn_trait_method_call(TypeHandle object_type, IdentId name_id,
        std::string_view name, const base::SourceRange& range, bool report_failure = true);
    [[nodiscard]] TraitMethodCallResolution resolve_trait_method_call(TypeHandle owner_type, IdentId name_id,
        std::string_view name, const base::SourceRange& range, bool require_self, bool report_failure = true);
    [[nodiscard]] TypeHandle resolve_associated_type_projection(TypeHandle base_type, IdentId associated_name_id,
        std::string_view associated_name, const base::SourceRange& range, bool report_failure = true);

private:
    struct TraitAnalysisScope;
    class TraitImplCanonicalResolver;

    struct ResolvedTraitReference {
        const TraitSignature* signature = nullptr;
        std::vector<TypeHandle> trait_args;
        bool ok = false;
    };

    void mark_trait_requirements(const syntax::ItemNode& item);
    void resolve_trait_signature(TraitSignature& signature);
    [[nodiscard]] TraitMethodRequirement resolve_trait_requirement(const TraitSignature& trait,
        const syntax::ItemNode& requirement, syntax::ItemId requirement_id, base::u32 ordinal);
    [[nodiscard]] TraitAssociatedTypeRequirement resolve_trait_associated_type_requirement(const TraitSignature& trait,
        const syntax::ItemNode& requirement, syntax::ItemId requirement_id, base::u32 ordinal);
    [[nodiscard]] GenericContext make_trait_generic_context(const syntax::ItemNode& trait);
    void merge_trait_where_constraints(
        const syntax::ItemNode& trait, const TraitSignature& signature, GenericContext& context);
    [[nodiscard]] base::u32 record_trait_self_predicate(
        const TraitSignature& trait, const syntax::ItemNode& trait_item, const GenericContext& context) const;
    [[nodiscard]] FunctionSignature make_trait_default_method_signature(
        const TraitSignature& trait, const TraitMethodRequirement& requirement) const;
    void analyze_trait_default_method_body(
        const TraitSignature& trait, const TraitMethodRequirement& requirement, GenericContext& context);
    [[nodiscard]] query::MemberKey trait_associated_type_member_key(
        const TraitSignature& trait, std::string_view name, base::u32 ordinal) const noexcept;
    [[nodiscard]] const TraitAssociatedTypeRequirement* find_trait_associated_type_requirement(
        const TraitSignature& trait, IdentId name_id) const;
    [[nodiscard]] const TraitSignature* find_current_trait_signature() const;
    [[nodiscard]] const TraitAssociatedTypeRequirement* find_current_trait_associated_type_requirement(
        IdentId name_id) const;
    [[nodiscard]] TypeHandle resolve_current_trait_concrete_associated_type(
        TypeHandle base_type, const TraitAssociatedTypeRequirement& requirement) const;
    [[nodiscard]] ResolvedTraitReference resolve_trait_reference(
        const syntax::ItemNode& impl_block, syntax::ItemId impl_id);
    void validate_trait_impl_block(const syntax::ItemNode& impl_block, syntax::ItemId impl_id);
    [[nodiscard]] TraitImplLookupKey make_trait_impl_lookup_key(
        const TraitSignature& trait, TypeHandle self_type, std::span<const TypeHandle> trait_args) const;
    [[nodiscard]] std::optional<query::StableFingerprint128> make_trait_impl_coherence_fingerprint(
        const TraitSignature& trait, TypeHandle self_type, std::span<const TypeHandle> trait_args) const;
    [[nodiscard]] syntax::ModuleId nominal_type_module(TypeHandle type) const;
    [[nodiscard]] bool trait_impl_obeys_orphan_rule(
        const TraitSignature& trait, TypeHandle self_type, syntax::ModuleId impl_module) const;
    [[nodiscard]] const TraitImplInfo* find_overlapping_trait_impl(
        query::StableFingerprint128 coherence_fingerprint) const;
    [[nodiscard]] base::u32 record_trait_impl_predicate(
        const TraitSignature& trait, TraitImplInfo& impl_info, query::StableFingerprint128 fingerprint) const;
    void record_trait_impl_evidence(const TraitImplInfo& impl_info) const;
    [[nodiscard]] const TraitMethodRequirement* find_trait_method_requirement(
        const TraitSignature& trait, IdentId name_id, bool require_self) const;
    [[nodiscard]] TraitMethodCallResolution make_param_env_trait_method_resolution(const TraitSignature& trait,
        const TraitPredicate& predicate, const TraitMethodRequirement& requirement, TypeHandle owner_type) const;
    [[nodiscard]] TraitMethodCallResolution make_impl_trait_method_resolution(const TraitSignature& trait,
        const TraitImplInfo& impl, const TraitMethodRequirement& requirement, const FunctionSignature& signature) const;
    [[nodiscard]] TraitMethodCallResolution make_trait_default_method_resolution(
        const TraitSignature& trait, const TraitImplInfo& impl, const TraitMethodRequirement& requirement);
    [[nodiscard]] FunctionLookupKey trait_default_method_instance_key(
        const TraitSignature& trait, const TraitImplInfo& impl, const TraitMethodRequirement& requirement) const;
    [[nodiscard]] std::string trait_default_method_instance_key_name(
        const TraitSignature& trait, const TraitImplInfo& impl, const TraitMethodRequirement& requirement) const;
    [[nodiscard]] std::string trait_default_method_instance_stable_name(
        const TraitSignature& trait, const TraitImplInfo& impl, const TraitMethodRequirement& requirement) const;
    [[nodiscard]] std::string trait_default_method_instance_c_symbol_name(
        const TraitSignature& trait, const TraitImplInfo& impl, const TraitMethodRequirement& requirement) const;
    [[nodiscard]] FunctionSignature make_trait_default_method_instance_signature(const TraitSignature& trait,
        const TraitImplInfo& impl, const TraitMethodRequirement& requirement, const FunctionLookupKey& key) const;
    [[nodiscard]] GenericContext make_trait_default_method_instance_context(
        const TraitSignature& trait, const TraitImplInfo& impl) const;
    [[nodiscard]] GenericTemplateInfo make_trait_default_method_instance_layout_info(
        const TraitSignature& trait, const TraitImplInfo& impl, const TraitMethodRequirement& requirement) const;
    [[nodiscard]] FunctionSignature* instantiate_trait_default_method(
        const TraitSignature& trait, const TraitImplInfo& impl, const TraitMethodRequirement& requirement);
    [[nodiscard]] const TraitSignature* trait_signature_for_impl(const TraitImplInfo& impl) const;
    [[nodiscard]] bool trait_visible_for_method_call(const TraitSignature& trait, const base::SourceRange& range);
    [[nodiscard]] bool visible_trait_has_associated_type(
        IdentId name_id, std::string_view name, const base::SourceRange& range);
    [[nodiscard]] bool visible_trait_has_method(IdentId name_id, bool require_self, const base::SourceRange& range);
    [[nodiscard]] TraitMethodCallResolution resolve_param_env_trait_method_call(TypeHandle owner_type, IdentId name_id,
        std::string_view name, const base::SourceRange& range, bool require_self, bool report_failure);
    [[nodiscard]] TraitMethodCallResolution resolve_impl_trait_method_call(TypeHandle owner_type, IdentId name_id,
        std::string_view name, const base::SourceRange& range, bool require_self, bool report_failure);
    [[nodiscard]] bool trait_impl_method_matches(const TraitMethodRequirement& requirement,
        const FunctionSignature& signature, TypeHandle self_type, std::span<const TypeHandle> trait_args,
        const TraitSignature& trait, std::span<const TraitImplAssociatedTypeInfo> associated_types) const;
    void validate_trait_impl_method_borrow_contract(
        const TraitSignature& trait, const TraitImplInfo& impl, const TraitImplMethodInfo& method);
    [[nodiscard]] TypeHandle substitute_requirement_type(TypeHandle type, TypeHandle self_type,
        std::span<const TypeHandle> trait_args, const TraitSignature& trait,
        std::span<const TraitImplAssociatedTypeInfo> associated_types) const;
    [[nodiscard]] std::string trait_display_name(
        const TraitSignature& trait, std::span<const TypeHandle> trait_args) const;

    SemanticAnalyzerCore& core_;
};

} // namespace aurex::sema
