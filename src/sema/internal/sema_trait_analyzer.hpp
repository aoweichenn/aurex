#pragma once

#include <optional>
#include <vector>

#include <sema/internal/sema_core.hpp>

namespace aurex::sema {

class SemanticAnalyzerCore::TraitAnalyzer final {
public:
    explicit TraitAnalyzer(SemanticAnalyzerCore& core) noexcept;

    void register_trait_name(const syntax::ItemNode& item, syntax::ItemId item_id);
    void register_trait_signatures();
    void validate_trait_impls();
    [[nodiscard]] const TraitSignature* find_trait_in_visible_modules(
        IdentId name_id, std::string_view name, const base::SourceRange& range, bool report_unknown = true);
    [[nodiscard]] TraitMethodCallResolution resolve_trait_method_call(TypeHandle owner_type, IdentId name_id,
        std::string_view name, const base::SourceRange& range, bool require_self, bool report_failure = true);

private:
    struct TraitAnalysisScope;
    class TraitImplCanonicalResolver;

    struct ResolvedTraitReference {
        const TraitSignature* signature = nullptr;
        std::vector<TypeHandle> trait_args;
        bool ok = false;
    };

    void mark_trait_requirements(const syntax::ItemNode& item);
    [[nodiscard]] const TraitSignature* find_trait_in_module(syntax::ModuleId module, IdentId name_id,
        std::string_view name, const base::SourceRange& range, bool report_unknown = true);
    void resolve_trait_signature(TraitSignature& signature);
    [[nodiscard]] TraitMethodRequirement resolve_trait_requirement(const TraitSignature& trait,
        const syntax::ItemNode& requirement, syntax::ItemId requirement_id, base::u32 ordinal);
    [[nodiscard]] GenericContext make_trait_generic_context(const syntax::ItemNode& trait);
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
    [[nodiscard]] const TraitSignature* trait_signature_for_impl(const TraitImplInfo& impl) const;
    [[nodiscard]] bool trait_visible_for_method_call(const TraitSignature& trait, const base::SourceRange& range);
    [[nodiscard]] bool visible_trait_has_method(IdentId name_id, bool require_self, const base::SourceRange& range);
    [[nodiscard]] TraitMethodCallResolution resolve_param_env_trait_method_call(TypeHandle owner_type, IdentId name_id,
        std::string_view name, const base::SourceRange& range, bool require_self, bool report_failure);
    [[nodiscard]] TraitMethodCallResolution resolve_impl_trait_method_call(TypeHandle owner_type, IdentId name_id,
        std::string_view name, const base::SourceRange& range, bool require_self, bool report_failure);
    [[nodiscard]] bool trait_impl_method_matches(const TraitMethodRequirement& requirement,
        const FunctionSignature& signature, TypeHandle self_type, std::span<const TypeHandle> trait_args,
        const TraitSignature& trait) const;
    [[nodiscard]] TypeHandle substitute_requirement_type(TypeHandle type, TypeHandle self_type,
        std::span<const TypeHandle> trait_args, const TraitSignature& trait) const;
    [[nodiscard]] std::string trait_display_name(
        const TraitSignature& trait, std::span<const TypeHandle> trait_args) const;

    SemanticAnalyzerCore& core_;
};

} // namespace aurex::sema
