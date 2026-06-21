#pragma once

#include <frontend/sema/internal/core/private/sema_core.hpp>

namespace aurex::sema {

class SemanticAnalyzerCore::GenericAnalyzer final {
public:
    explicit GenericAnalyzer(SemanticAnalyzerCore& core) noexcept;

    bool has_generic_params(const syntax::ItemNode& item) const noexcept;
    bool has_lifetime_origin_params(const syntax::ItemNode& item) const noexcept;
    bool has_generic_constraints(const syntax::ItemNode& item) const noexcept;
    void record_lifetime_origin_params(const syntax::ItemNode& item, syntax::ItemId item_id);
    void validate_generic_parameter_list(const syntax::ItemNode& item);
    void validate_generic_constraints(const syntax::ItemNode& item, GenericTemplateInfo& info);
    base::u32 record_generic_trait_predicate(GenericTemplateInfo& info, const syntax::GenericConstraintDecl& constraint,
        base::usize param_index, base::usize capability_index, TraitPredicateKind kind, CapabilityKind capability,
        const TraitSignature* trait);
    void record_generic_param_env(GenericTemplateInfo& info, const syntax::ItemNode& item);
    bool generic_param_has_capability(const std::string_view param, const CapabilityKind capability) const;
    bool generic_param_has_capability(const TypeHandle param, const CapabilityKind capability) const;
    bool type_satisfies_capability(const TypeHandle type, const CapabilityKind capability) const;
    bool type_satisfies_equality_capability(const TypeHandle type) const;
    bool type_satisfies_ordering_capability(const TypeHandle type) const;
    bool type_supports_equality_operator(const TypeHandle type) const;
    bool type_supports_ordering_operator(const TypeHandle type) const;
    bool type_supports_hash_capability(const TypeHandle type) const;
    bool type_is_const_generic_scalar(const TypeHandle type) const;
    bool validate_generic_arguments(
        const GenericTemplateInfo& info, const std::vector<TypeHandle>& args, const base::SourceRange& use_range);
    bool validate_generic_argument_bundle(
        const GenericTemplateInfo& info, const GenericArgumentBundle& args, const base::SourceRange& use_range);
    base::Result<query::StableFingerprint128> const_generic_arg_fingerprint(
        const syntax::GenericArgDecl& arg, TypeHandle expected_type, const base::SourceRange& use_range);
    base::Result<GenericArgumentBundle> resolve_generic_argument_bundle(
        const GenericTemplateInfo& info, std::span<const syntax::GenericArgDecl> args,
        const base::SourceRange& use_range);
    [[nodiscard]] bool type_arg_is_simple_const_param_name(const syntax::TypeNode& type) const noexcept;
    [[nodiscard]] bool type_arg_names_current_const_param(const syntax::TypeNode& type) const;
    [[nodiscard]] bool type_satisfies_trait_predicate(
        TypeHandle type, const TraitPredicate& predicate, const base::SourceRange& use_range);
    [[nodiscard]] bool generic_param_has_trait_predicate(TypeHandle param, const TraitPredicate& predicate) const;
    void populate_generic_template_node_spans(GenericTemplateInfo& info, const syntax::ItemNode& item) const;
    std::string generic_template_incremental_fingerprint(
        const syntax::ItemNode& item, const GenericTemplateInfo& info) const;
    void record_generic_template_signature(const GenericTemplateInfo& info, const query::DefNamespace name_space);
    GenericSideTables make_generic_instance_side_tables(const GenericTemplateInfo& info);
    base::usize generic_side_table_layout_index(const GenericTemplateInfo& info);
    void register_generic_template(const syntax::ItemNode& item, const syntax::ItemId item_id);
    void populate_generic_param_identities(GenericTemplateInfo& info);
    GenericParamIdentity make_generic_param_identity(const GenericTemplateInfo& info, const base::usize index) const;
    std::string_view generic_param_name(const GenericTemplateInfo& info, const base::usize index) const;
    GenericParamIdentity generic_param_identity(const GenericTemplateInfo& info, const base::usize index) const;
    GenericParamIdentity generic_param_identity(const TypeInfo& info) const;
    TypeHandle generic_param_placeholder(const GenericTemplateInfo& info, const base::usize index);
    void populate_generic_placeholder_context(const GenericTemplateInfo& info, GenericContext& context);
    void populate_generic_concrete_context(
        const GenericTemplateInfo& info, const std::vector<TypeHandle>& args, GenericContext& context) const;
    void populate_generic_concrete_context(
        const GenericTemplateInfo& info, const GenericArgumentBundle& args, GenericContext& context) const;
    std::string generic_instance_key_suffix(const std::vector<TypeHandle>& args) const;
    std::string generic_instance_key_suffix(const GenericArgumentBundle& args) const;
    std::string generic_instance_abi_suffix(const query::GenericInstanceKey& key) const;
    std::string generic_instance_key(const GenericTemplateInfo& info, const std::vector<TypeHandle>& args) const;
    std::string generic_instance_key(const GenericTemplateInfo& info, const GenericArgumentBundle& args) const;
    std::string generic_struct_instance_key(const GenericTemplateInfo& info, const std::vector<TypeHandle>& args) const;
    std::string generic_struct_instance_key(const GenericTemplateInfo& info, const GenericArgumentBundle& args) const;
    std::string generic_enum_instance_key(const GenericTemplateInfo& info, const std::vector<TypeHandle>& args) const;
    std::string generic_enum_instance_key(const GenericTemplateInfo& info, const GenericArgumentBundle& args) const;
    std::string generic_type_alias_instance_key(
        const GenericTemplateInfo& info, const std::vector<TypeHandle>& args) const;
    std::string generic_type_alias_instance_key(
        const GenericTemplateInfo& info, const GenericArgumentBundle& args) const;
    std::string generic_function_instance_key(
        const GenericTemplateInfo& info, const std::vector<TypeHandle>& args) const;
    std::string generic_function_instance_key(
        const GenericTemplateInfo& info, const GenericArgumentBundle& args) const;
    const SemanticAnalyzerCore::GenericTemplateInfo* find_generic_struct_in_visible_modules(
        const IdentId name_id, const std::string_view name, const base::SourceRange& range, const bool report_unknown);
    const SemanticAnalyzerCore::GenericTemplateInfo* find_generic_struct_in_module(const syntax::ModuleId module,
        const IdentId name_id, const std::string_view name, const base::SourceRange& range, const bool report_unknown);
    const SemanticAnalyzerCore::GenericTemplateInfo* find_generic_enum_in_visible_modules(
        const IdentId name_id, const std::string_view name, const base::SourceRange& range, const bool report_unknown);
    const SemanticAnalyzerCore::GenericTemplateInfo* find_generic_enum_in_module(const syntax::ModuleId module,
        const IdentId name_id, const std::string_view name, const base::SourceRange& range, const bool report_unknown);
    const SemanticAnalyzerCore::GenericTemplateInfo* find_generic_type_alias_in_visible_modules(
        const IdentId name_id, const std::string_view name, const base::SourceRange& range, const bool report_unknown);
    const SemanticAnalyzerCore::GenericTemplateInfo* find_generic_type_alias_in_module(const syntax::ModuleId module,
        const IdentId name_id, const std::string_view name, const base::SourceRange& range, const bool report_unknown);
    bool generic_type_template_exists_in_module(
        const syntax::ModuleId module, const IdentId name_id, const std::string_view name) const;
    const SemanticAnalyzerCore::GenericTemplateInfo* find_any_generic_type_template_in_module(
        const syntax::ModuleId module, const IdentId name_id, const std::string_view name) const;
    bool report_generic_type_requires_args_if_visible(
        const IdentId name_id, const std::string_view name, const base::SourceRange& range);
    void report_generic_type_template_in_module(const syntax::ModuleId module, const IdentId name_id,
        const std::string_view name, const base::SourceRange& range);
    const SemanticAnalyzerCore::GenericTemplateInfo* find_generic_function_in_visible_modules(
        const IdentId name_id, const std::string_view name, const base::SourceRange& range, const bool report_unknown);
    const SemanticAnalyzerCore::GenericTemplateInfo* find_generic_function_in_module(const syntax::ModuleId module,
        const IdentId name_id, const std::string_view name, const base::SourceRange& range, const bool report_unknown);
    TypeHandle instantiate_generic_struct(const GenericTemplateInfo& info, const syntax::TypeNode& use_type,
        syntax::TypeId use_type_id, const std::vector<TypeHandle>& args);
    TypeHandle instantiate_generic_struct(const GenericTemplateInfo& info, const syntax::TypeNode& use_type,
        syntax::TypeId use_type_id, const GenericArgumentBundle& args);
    TypeHandle instantiate_generic_enum(const GenericTemplateInfo& info, const syntax::TypeNode& use_type,
        syntax::TypeId use_type_id, const std::vector<TypeHandle>& args);
    TypeHandle instantiate_generic_enum(const GenericTemplateInfo& info, const syntax::TypeNode& use_type,
        syntax::TypeId use_type_id, const GenericArgumentBundle& args);
    TypeHandle instantiate_generic_type_alias(const GenericTemplateInfo& info, const syntax::TypeNode& use_type,
        syntax::TypeId use_type_id, const std::vector<TypeHandle>& args, const bool opaque_allowed_as_pointee);
    TypeHandle instantiate_generic_type_alias(const GenericTemplateInfo& info, const syntax::TypeNode& use_type,
        syntax::TypeId use_type_id, const GenericArgumentBundle& args, const bool opaque_allowed_as_pointee);
    bool unify_generic_type(const TypeHandle pattern, const TypeHandle actual,
        std::unordered_map<GenericParamIdentity, TypeHandle, GenericParamIdentityHash>& inferred) const;
    bool infer_generic_arguments(
        const GenericTemplateInfo& info, const SemanticAnalyzerCore::ExprView& call, std::vector<TypeHandle>& args);
    std::unordered_set<GenericParamIdentity, GenericParamIdentityHash> generic_param_identities_in_type(
        const TypeHandle type) const;
    std::vector<base::usize> method_local_generic_param_indices(const GenericTemplateInfo& info) const;
    bool infer_generic_method_arguments(const GenericTemplateInfo& info, const TypeHandle owner_type,
        const SemanticAnalyzerCore::ExprView& call, base::usize receiver_count, std::vector<TypeHandle>& args);
    bool apply_explicit_generic_method_arguments(const GenericTemplateInfo& info, const TypeHandle owner_type,
        std::span<const syntax::TypeId> explicit_type_args, const base::SourceRange& use_range,
        std::vector<TypeHandle>& args);
    FunctionSignature* instantiate_generic_placeholder_function(
        const GenericTemplateInfo& info, const std::vector<TypeHandle>& args, const base::SourceRange& use_range);
    FunctionSignature* instantiate_generic_placeholder_function(
        const GenericTemplateInfo& info, const GenericArgumentBundle& args, const base::SourceRange& use_range);
    bool type_contains_generic_param(const TypeHandle type) const;
    FunctionSignature* instantiate_generic_function(
        const GenericTemplateInfo& info, const std::vector<TypeHandle>& args, const base::SourceRange& use_range);
    FunctionSignature* instantiate_generic_function(
        const GenericTemplateInfo& info, const GenericArgumentBundle& args, const base::SourceRange& use_range);
    FunctionSignature* instantiate_generic_method(const GenericTemplateInfo& info, const TypeHandle owner_type,
        const std::vector<TypeHandle>& args, const base::SourceRange& use_range);
    FunctionSignature* find_generic_method_in_visible_modules(const TypeHandle owner_type, const IdentId name_id,
        const std::string_view name, const base::SourceRange& range, const bool require_self, const bool report_unknown,
        const SemanticAnalyzerCore::ExprView* call = nullptr, base::usize receiver_count = 0,
        bool has_explicit_type_args = false, std::span<const syntax::TypeId> explicit_type_args = {},
        bool* saw_matching_template = nullptr);
    void analyze_generic_function_definition(const GenericTemplateInfo& info);
    void analyze_generic_function_body(const syntax::ItemNode& function, const GenericTemplateInfo& info,
        const FunctionSignature& signature, FunctionBodyState& state);

private:
    SemanticAnalyzerCore& core_;
};

} // namespace aurex::sema
