#pragma once

#include <aurex/base/diagnostic.hpp>
#include <aurex/base/result.hpp>
#include <aurex/sema/checked_module.hpp>
#include <aurex/sema/function.hpp>
#include <aurex/sema/identifier.hpp>
#include <aurex/sema/symbol.hpp>
#include <aurex/sema/type.hpp>
#include <aurex/syntax/ast.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace aurex::sema {

enum class CapabilityKind {
    sized,
    eq,
    ord,
    hash,
};

struct CapabilityKindHash {
    [[nodiscard]] std::size_t operator()(const CapabilityKind kind) const noexcept {
        return static_cast<std::size_t>(kind);
    }
};

[[nodiscard]] std::string_view capability_name(CapabilityKind capability) noexcept;

struct SemanticOptions {
    bool retain_generic_side_tables = true;
};

class SemanticAnalyzer final {
public:
    SemanticAnalyzer(
        syntax::AstModule& module,
        base::DiagnosticSink& diagnostics,
        SemanticOptions options = {}
    ) noexcept;
    SemanticAnalyzer(
        const syntax::AstModule& module,
        base::DiagnosticSink& diagnostics,
        SemanticOptions options = {}
    ) = delete;
    SemanticAnalyzer(
        syntax::AstModule&& module,
        base::DiagnosticSink& diagnostics,
        SemanticOptions options = {}
    ) noexcept;

    [[nodiscard]] base::Result<CheckedModule> analyze();

private:
    enum class FunctionBodyState {
        not_started,
        analyzing,
        analyzed,
    };

    struct ReturnTypeInference {
        TypeHandle inferred_type = INVALID_TYPE_HANDLE;
        std::vector<syntax::StmtId> returns;
        std::vector<syntax::StmtId> pending_null_returns;
    };

    using CapabilitySet = SemaSet<CapabilityKind, CapabilityKindHash>;
    using CapabilityMap = SemaMap<std::string, CapabilitySet>;

    struct GenericTemplateInfo {
        syntax::ItemId item = syntax::INVALID_ITEM_ID;
        syntax::ModuleId module = syntax::INVALID_MODULE_ID;
        std::string name;
        IdentId name_id = INVALID_IDENT_ID;
        std::string key;
        SemaVector<std::string> params;
        SemaVector<IdentId> param_ids;
        SemaVector<std::string> param_identity_keys;
        CapabilityMap constraints;
        TypeHandle impl_type_pattern = INVALID_TYPE_HANDLE;
        syntax::Visibility visibility = syntax::Visibility::private_;
    };

    struct GenericContext {
        SemaMap<IdentId, TypeHandle, IdentIdHash> params;
        SemaMap<IdentId, std::string, IdentIdHash> param_identities;
        CapabilityMap constraints;
        CapabilityMap constraints_by_identity;
    };

    struct GenericSideTableScope {
        GenericSideTables* side_tables = nullptr;
        bool cache_syntax_types = true;
    };

    struct PlaceInfo {
        TypeHandle type = INVALID_TYPE_HANDLE;
        bool is_place = false;
        bool is_writable = false;
        bool crosses_raw_pointer = false;
    };

    struct TryShape {
        enum class Kind {
            none,
            result,
            option,
            malformed_result,
            malformed_option,
        };

        Kind kind = Kind::none;
        const EnumCaseInfo* success_case = nullptr;
        const EnumCaseInfo* failure_case = nullptr;
    };

    struct ModuleSelector {
        syntax::ModuleId module = syntax::INVALID_MODULE_ID;
        bool failed_as_module_selector = false;
    };

    struct IndexedTypeInfo {
        TypeHandle type = INVALID_TYPE_HANDLE;
        syntax::Visibility visibility = syntax::Visibility::public_;
    };

    struct ModuleSelectorPath {
        std::vector<std::string_view> parts;
        std::vector<IdentId> part_ids;
        base::SourceRange range {};
    };

    struct NamedTypeSelector {
        syntax::ModuleId module = syntax::INVALID_MODULE_ID;
        std::string_view name;
        IdentId name_id = INVALID_IDENT_ID;
        base::SourceRange range {};
        std::vector<syntax::TypeId> type_args;
        bool qualified = false;
    };

    struct ExprView {
        syntax::ExprKind kind = syntax::ExprKind::invalid;
        base::SourceRange range {};
        std::string_view scope_name;
        IdentId scope_name_id = INVALID_IDENT_ID;
        base::SourceRange scope_range {};
        std::string_view text;
        IdentId text_id = INVALID_IDENT_ID;
        syntax::UnaryOp unary_op = syntax::UnaryOp::logical_not;
        syntax::ExprId unary_operand = syntax::INVALID_EXPR_ID;
        syntax::BinaryOp binary_op = syntax::BinaryOp::add;
        syntax::ExprId binary_lhs = syntax::INVALID_EXPR_ID;
        syntax::ExprId binary_rhs = syntax::INVALID_EXPR_ID;
        syntax::ExprId callee = syntax::INVALID_EXPR_ID;
        std::span<const syntax::ExprId> args {};
        syntax::ExprId condition = syntax::INVALID_EXPR_ID;
        syntax::PatternId condition_pattern = syntax::INVALID_PATTERN_ID;
        syntax::ExprId then_expr = syntax::INVALID_EXPR_ID;
        syntax::ExprId else_expr = syntax::INVALID_EXPR_ID;
        syntax::StmtId block = syntax::INVALID_STMT_ID;
        syntax::ExprId block_result = syntax::INVALID_EXPR_ID;
        syntax::ExprId match_value = syntax::INVALID_EXPR_ID;
        std::span<const syntax::MatchArm> match_arms {};
        std::span<const syntax::ExprId> array_elements {};
        std::span<const syntax::ExprId> tuple_elements {};
        syntax::ExprId array_repeat_value = syntax::INVALID_EXPR_ID;
        syntax::ExprId array_repeat_count = syntax::INVALID_EXPR_ID;
        syntax::ExprId postfix_base = syntax::INVALID_EXPR_ID;
        std::span<const syntax::PostfixOp> postfix_ops {};
        syntax::ExprId object = syntax::INVALID_EXPR_ID;
        std::string_view field_name;
        IdentId field_name_id = INVALID_IDENT_ID;
        syntax::ExprId index = syntax::INVALID_EXPR_ID;
        syntax::ExprId slice_start = syntax::INVALID_EXPR_ID;
        syntax::ExprId slice_end = syntax::INVALID_EXPR_ID;
        std::string_view struct_name;
        IdentId struct_name_id = INVALID_IDENT_ID;
        std::span<const syntax::TypeId> type_args {};
        std::span<const syntax::FieldInit> field_inits {};
        syntax::TypeId cast_type = syntax::INVALID_TYPE_ID;
        syntax::ExprId cast_expr = syntax::INVALID_EXPR_ID;
    };

    struct PatternBinding {
        std::string name;
        IdentId name_id = INVALID_IDENT_ID;
        TypeHandle type = INVALID_TYPE_HANDLE;
        base::SourceRange range {};
    };

    struct MatchUsefulnessChecker;

    static constexpr base::u64 SEMA_TYPE_ABI_INVALID_SIZE = 0;
    static constexpr base::u64 SEMA_TYPE_ABI_MIN_ALIGNMENT = 1;
    static constexpr int SEMA_NO_LOOP_DEPTH = 0;

    enum class StatementAnalysisRootKind {
        statement,
        scoped_block,
        block_statements,
    };

    enum class StatementAnalysisActionKind {
        statement,
        scoped_block,
        block_statements,
        pattern_scoped_block,
        local_pattern,
        pop_scope,
        enter_loop,
        exit_loop,
        for_condition,
    };

    struct StatementAnalysisAction {
        StatementAnalysisActionKind kind = StatementAnalysisActionKind::statement;
        syntax::StmtId stmt = syntax::INVALID_STMT_ID;
        syntax::StmtId block = syntax::INVALID_STMT_ID;
        syntax::PatternId pattern = syntax::INVALID_PATTERN_ID;
        TypeHandle pattern_type = INVALID_TYPE_HANDLE;
        bool is_mutable = false;
        bool allow_refutable = false;
    };

    using GenericTemplateList = SemaVector<const GenericTemplateInfo*>;
    using EnumCaseList = SemaVector<const EnumCaseInfo*>;
    using ModuleIdList = SemaVector<syntax::ModuleId>;
    using ModuleLookupList = SemaVector<ModuleLookupKey>;
    using ModuleLookupSet = SemaSet<ModuleLookupKey, ModuleLookupKeyHash>;

    struct TypeAbiLayout {
        base::u64 size = SEMA_TYPE_ABI_INVALID_SIZE;
        base::u64 align = SEMA_TYPE_ABI_MIN_ALIGNMENT;
    };

    void register_type_names();
    void register_generic_template(const syntax::ItemNode& item, syntax::ItemId item_id);
    void validate_generic_parameter_list(const syntax::ItemNode& item);
    void validate_generic_constraints(const syntax::ItemNode& item, GenericTemplateInfo& info);
    [[nodiscard]] GenericTemplateInfo make_generic_template_info() const;
    [[nodiscard]] GenericContext make_generic_context() const;
    [[nodiscard]] CapabilitySet make_capability_set() const;
    [[nodiscard]] CapabilitySet copy_capability_set(const CapabilitySet& source) const;
    void copy_capability_map(CapabilityMap& target, const CapabilityMap& source) const;
    [[nodiscard]] CapabilitySet& capability_bucket(CapabilityMap& map, std::string key) const;
    [[nodiscard]] bool generic_param_has_capability(std::string_view param, CapabilityKind capability) const;
    [[nodiscard]] bool generic_param_has_capability(TypeHandle param, CapabilityKind capability) const;
    [[nodiscard]] bool type_satisfies_capability(TypeHandle type, CapabilityKind capability) const;
    [[nodiscard]] bool type_satisfies_equality_capability(TypeHandle type) const;
    [[nodiscard]] bool type_satisfies_ordering_capability(TypeHandle type) const;
    [[nodiscard]] bool type_supports_equality_operator(TypeHandle type) const;
    [[nodiscard]] bool type_supports_ordering_operator(TypeHandle type) const;
    [[nodiscard]] bool type_supports_hash_capability(TypeHandle type) const;
    [[nodiscard]] bool validate_generic_arguments(
        const GenericTemplateInfo& info,
        const std::vector<TypeHandle>& args,
        const base::SourceRange& use_range
    );
    [[nodiscard]] bool has_generic_params(const syntax::ItemNode& item) const noexcept;
    [[nodiscard]] bool has_generic_constraints(const syntax::ItemNode& item) const noexcept;
    void register_value_names();
    void register_enum_cases_for_item(
        const syntax::ItemNode& item,
        syntax::ModuleId owner,
        TypeHandle named_enum_type,
        std::string enum_display_name,
        std::string case_prefix,
        std::string c_prefix,
        syntax::Visibility visibility
    );
    void validate_function_prototypes();
    void validate_abi_symbols();
    void validate_type_layouts();
    void validate_module_namespace_conflicts();
    void analyze_entry_points();
    void resolve_type_alias_decls();
    void analyze_struct_properties();
    void analyze_const_decls();
    void analyze_function_body(const syntax::ItemNode& function, syntax::ItemId function_id);
    void analyze_function_body_with_signature(
        const syntax::ItemNode& function,
        const std::string& key,
        const FunctionSignature& signature,
        FunctionBodyState& state
    );
    void analyze_generic_function_definition(const GenericTemplateInfo& info);
    void analyze_generic_function_body(
        const syntax::ItemNode& function,
        const GenericTemplateInfo& info,
        const FunctionSignature& signature,
        FunctionBodyState& state
    );
    void analyze_block(syntax::StmtId block, TypeHandle expected_return, ReturnTypeInference* return_inference);
    void analyze_block_statements(syntax::StmtId block, TypeHandle expected_return, ReturnTypeInference* return_inference);
    void analyze_stmt(syntax::StmtId stmt, TypeHandle expected_return, ReturnTypeInference* return_inference);
    void analyze_statement_tree(
        syntax::StmtId root,
        TypeHandle expected_return,
        ReturnTypeInference* return_inference,
        StatementAnalysisRootKind root_kind
    );
    void analyze_statement_action(
        const StatementAnalysisAction& action,
        std::vector<StatementAnalysisAction>& stack,
        TypeHandle expected_return,
        ReturnTypeInference* return_inference
    );
    void analyze_statement_node(
        syntax::StmtId stmt,
        std::vector<StatementAnalysisAction>& stack,
        TypeHandle expected_return,
        ReturnTypeInference* return_inference
    );
    void analyze_statement_block(syntax::StmtId block, std::vector<StatementAnalysisAction>& stack);
    void analyze_pattern_scoped_block(
        syntax::PatternId pattern,
        TypeHandle pattern_type,
        syntax::StmtId block,
        std::vector<StatementAnalysisAction>& stack
    );
    void analyze_for_condition(syntax::StmtId stmt);
    [[nodiscard]] TypeHandle analyze_for_range_bounds(syntax::StmtId stmt, const syntax::StmtNode& node);
    void define_for_range_local(const syntax::StmtNode& node, TypeHandle type);
    [[nodiscard]] TypeHandle analyze_assignment_target(syntax::ExprId expr);
    [[nodiscard]] bool block_guarantees_return(syntax::StmtId block) const;
    [[nodiscard]] bool stmt_guarantees_return(syntax::StmtId stmt) const;
    [[nodiscard]] bool block_may_fallthrough(syntax::StmtId block) const;
    [[nodiscard]] bool stmt_may_fallthrough(syntax::StmtId stmt) const;
    void record_inferred_return(syntax::StmtId stmt, TypeHandle actual, ReturnTypeInference& inference);
    void finalize_inferred_return(const syntax::ItemNode& function, const std::string& key, ReturnTypeInference& inference);
    void resolve_pending_null_returns(ReturnTypeInference& inference);
    void report_return_inference_diagnostic(syntax::StmtId stmt, std::string_view message);
    void validate_function_return_type(const syntax::ItemNode& function, TypeHandle return_type);
    void ensure_function_return_known(const FunctionSignature& signature, const base::SourceRange& use_range);
    [[nodiscard]] TypeHandle analyze_expr(syntax::ExprId expr);
    [[nodiscard]] TypeHandle analyze_expr(syntax::ExprId expr, TypeHandle expected_type);
    [[nodiscard]] ExprView expr_view(syntax::ExprId expr) const noexcept;
    [[nodiscard]] TypeHandle analyze_expr(syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_postfix_chain_expr(syntax::ExprId expr_id, TypeHandle expected_type);
    [[nodiscard]] syntax::ExprId materialize_postfix_chain(syntax::ExprId expr_id);
    [[nodiscard]] syntax::ExprId materialize_postfix_op(
        syntax::ExprId chain_expr,
        syntax::ExprId base,
        syntax::PostfixOp&& op,
        const syntax::PostfixOp* next_op,
        bool is_last
    );
    [[nodiscard]] syntax::ExprId materialize_postfix_bracket_op(
        syntax::ExprId chain_expr,
        syntax::ExprId base,
        syntax::PostfixOp&& op,
        const syntax::PostfixOp* next_op,
        bool is_last
    );
    [[nodiscard]] bool postfix_bracket_is_generic_apply(
        syntax::ExprId base,
        const syntax::PostfixOp& op,
        const syntax::PostfixOp* next_op
    );
    [[nodiscard]] std::vector<syntax::TypeId> postfix_bracket_type_args(const syntax::PostfixOp& op);
    [[nodiscard]] syntax::TypeId postfix_arg_expr_to_type(syntax::ExprId expr);
    [[nodiscard]] syntax::TypeId postfix_chain_expr_to_type(syntax::ExprId expr);
    [[nodiscard]] syntax::TypeId append_postfix_type_selector(
        syntax::TypeId current,
        std::string_view name,
        const base::SourceRange& range
    );
    [[nodiscard]] TypeHandle analyze_name_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] TypeHandle analyze_generic_apply_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] TypeHandle analyze_unary_expr(syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_binary_expr(syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_field_expr(syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_index_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] TypeHandle analyze_slice_expr(syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_struct_literal_expr(syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_cast_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] TypeHandle analyze_size_or_align_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] TypeHandle analyze_ptr_addr_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] TypeHandle analyze_paddr_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] TypeHandle analyze_str_projection_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] TypeHandle analyze_str_utf8_slice_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] TypeHandle analyze_str_from_bytes_unchecked_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] TypeHandle analyze_call_expr(syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_enum_constructor_call(syntax::ExprId expr_id, const ExprView& expr, const EnumCaseInfo& enum_case);
    [[nodiscard]] TypeHandle analyze_field_call_expr(
        syntax::ExprId expr_id,
        const ExprView& expr,
        const ExprView& callee,
        std::string_view name,
        TypeHandle expected_type
    );
    [[nodiscard]] TypeHandle analyze_function_call_expr(
        syntax::ExprId expr_id,
        const ExprView& expr,
        const ExprView& callee,
        std::string_view name,
        TypeHandle expected_type
    );
    [[nodiscard]] TypeHandle analyze_function_value_call_expr(
        syntax::ExprId expr_id,
        const ExprView& expr,
        std::string_view name
    );
    [[nodiscard]] TypeHandle analyze_explicit_generic_function_call_expr(
        syntax::ExprId expr_id,
        const ExprView& expr,
        const ExprView& apply,
        std::string_view name
    );
    void validate_call_arguments(
        const ExprView& expr,
        std::string_view name,
        std::span<const TypeHandle> param_types,
        base::usize receiver_count,
        bool is_variadic
    );
    [[nodiscard]] TypeHandle analyze_try_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] TypeHandle analyze_if_expr(syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_block_expr(syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_unsafe_block_expr(syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_match_expr(syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_array_literal_expr(syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_tuple_literal_expr(syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    void define_local_pattern(syntax::PatternId pattern, TypeHandle type, bool is_mutable, bool allow_refutable = false);
    [[nodiscard]] bool analyze_pattern(
        syntax::PatternId pattern,
        TypeHandle matched,
        std::vector<PatternBinding>& bindings
    );
    [[nodiscard]] bool pattern_is_irrefutable(syntax::PatternId pattern, TypeHandle matched) const;
    void define_pattern_bindings(const std::vector<PatternBinding>& bindings, bool is_mutable);
    [[nodiscard]] const EnumCaseInfo* analyze_single_value_pattern(
        syntax::PatternId pattern,
        TypeHandle matched,
        bool& covered_true,
        bool& covered_false,
        bool& saw_wildcard
    );
    [[nodiscard]] TypeHandle resolve_type(syntax::TypeId type);
    [[nodiscard]] TypeHandle resolve_type(syntax::TypeId type, bool opaque_allowed_as_pointee);
    [[nodiscard]] TypeHandle resolve_named_type(
        syntax::TypeId type_id,
        const syntax::TypeNode& type,
        bool opaque_allowed_as_pointee
    );
    [[nodiscard]] TypeHandle instantiate_generic_struct(
        const GenericTemplateInfo& info,
        const syntax::TypeNode& use_type,
        syntax::TypeId use_type_id,
        const std::vector<TypeHandle>& args
    );
    [[nodiscard]] TypeHandle instantiate_generic_enum(
        const GenericTemplateInfo& info,
        const syntax::TypeNode& use_type,
        syntax::TypeId use_type_id,
        const std::vector<TypeHandle>& args
    );
    [[nodiscard]] TypeHandle instantiate_generic_type_alias(
        const GenericTemplateInfo& info,
        const syntax::TypeNode& use_type,
        syntax::TypeId use_type_id,
        const std::vector<TypeHandle>& args,
        bool opaque_allowed_as_pointee
    );
    [[nodiscard]] TypeHandle resolve_type_alias(const TypeAliasInfo& alias, bool opaque_allowed_as_pointee);
    [[nodiscard]] bool infer_generic_arguments(
        const GenericTemplateInfo& info,
        const ExprView& call,
        std::vector<TypeHandle>& args
    );
    [[nodiscard]] bool unify_generic_type(
        TypeHandle pattern,
        TypeHandle actual,
        std::unordered_map<std::string, TypeHandle>& inferred
    ) const;
    [[nodiscard]] const GenericTemplateInfo* find_generic_function_in_visible_modules(
        IdentId name_id,
        std::string_view name,
        const base::SourceRange& range,
        bool report_unknown = true
    );
    [[nodiscard]] const GenericTemplateInfo* find_generic_function_in_module(
        syntax::ModuleId module,
        IdentId name_id,
        std::string_view name,
        const base::SourceRange& range,
        bool report_unknown = true
    );
    [[nodiscard]] const GenericTemplateInfo* find_generic_struct_in_visible_modules(
        IdentId name_id,
        std::string_view name,
        const base::SourceRange& range,
        bool report_unknown = true
    );
    [[nodiscard]] const GenericTemplateInfo* find_generic_struct_in_module(
        syntax::ModuleId module,
        IdentId name_id,
        std::string_view name,
        const base::SourceRange& range,
        bool report_unknown = true
    );
    [[nodiscard]] const GenericTemplateInfo* find_generic_enum_in_visible_modules(
        IdentId name_id,
        std::string_view name,
        const base::SourceRange& range,
        bool report_unknown = true
    );
    [[nodiscard]] const GenericTemplateInfo* find_generic_enum_in_module(
        syntax::ModuleId module,
        IdentId name_id,
        std::string_view name,
        const base::SourceRange& range,
        bool report_unknown = true
    );
    [[nodiscard]] const GenericTemplateInfo* find_generic_type_alias_in_visible_modules(
        IdentId name_id,
        std::string_view name,
        const base::SourceRange& range,
        bool report_unknown = true
    );
    [[nodiscard]] const GenericTemplateInfo* find_generic_type_alias_in_module(
        syntax::ModuleId module,
        IdentId name_id,
        std::string_view name,
        const base::SourceRange& range,
        bool report_unknown = true
    );
    [[nodiscard]] bool generic_type_template_exists_in_module(
        syntax::ModuleId module,
        IdentId name_id,
        std::string_view name
    ) const;
    [[nodiscard]] const GenericTemplateInfo* find_any_generic_type_template_in_module(
        syntax::ModuleId module,
        IdentId name_id,
        std::string_view name
    ) const;
    [[nodiscard]] bool report_generic_type_requires_args_if_visible(
        IdentId name_id,
        std::string_view name,
        const base::SourceRange& range
    );
    void report_generic_type_template_in_module(
        syntax::ModuleId module,
        IdentId name_id,
        std::string_view name,
        const base::SourceRange& range
    );
    [[nodiscard]] FunctionSignature* instantiate_generic_function(
        const GenericTemplateInfo& info,
        const std::vector<TypeHandle>& args,
        const base::SourceRange& use_range
    );
    [[nodiscard]] FunctionSignature* instantiate_generic_method(
        const GenericTemplateInfo& info,
        TypeHandle owner_type,
        const std::vector<TypeHandle>& args,
        const base::SourceRange& use_range
    );
    [[nodiscard]] FunctionSignature* instantiate_generic_placeholder_function(
        const GenericTemplateInfo& info,
        const std::vector<TypeHandle>& args,
        const base::SourceRange& use_range
    );
    [[nodiscard]] FunctionSignature* find_generic_method_in_visible_modules(
        TypeHandle owner_type,
        IdentId name_id,
        std::string_view name,
        const base::SourceRange& range,
        bool require_self,
        bool report_unknown = true
    );
    [[nodiscard]] bool type_contains_generic_param(TypeHandle type) const;
    void populate_generic_param_identity_keys(GenericTemplateInfo& info) const;
    [[nodiscard]] std::string make_generic_param_identity_key(const GenericTemplateInfo& info, base::usize index) const;
    [[nodiscard]] std::string generic_param_identity_key(const GenericTemplateInfo& info, base::usize index) const;
    [[nodiscard]] std::string generic_param_identity_key(const TypeInfo& info) const;
    [[nodiscard]] TypeHandle generic_param_placeholder(const GenericTemplateInfo& info, base::usize index);
    void populate_generic_placeholder_context(const GenericTemplateInfo& info, GenericContext& context);
    void populate_generic_concrete_context(
        const GenericTemplateInfo& info,
        const std::vector<TypeHandle>& args,
        GenericContext& context
    );
    [[nodiscard]] std::string generic_instance_key_suffix(const std::vector<TypeHandle>& args) const;
    [[nodiscard]] std::string generic_instance_abi_suffix(const std::vector<TypeHandle>& args) const;
    [[nodiscard]] std::string generic_instance_key(const GenericTemplateInfo& info, const std::vector<TypeHandle>& args) const;
    [[nodiscard]] std::string generic_struct_instance_key(const GenericTemplateInfo& info, const std::vector<TypeHandle>& args) const;
    [[nodiscard]] std::string generic_enum_instance_key(const GenericTemplateInfo& info, const std::vector<TypeHandle>& args) const;
    [[nodiscard]] std::string generic_type_alias_instance_key(const GenericTemplateInfo& info, const std::vector<TypeHandle>& args) const;
    [[nodiscard]] std::string generic_function_instance_key(const GenericTemplateInfo& info, const std::vector<TypeHandle>& args) const;
    [[nodiscard]] bool can_assign(TypeHandle dst, TypeHandle src, syntax::ExprId value) const noexcept;
    [[nodiscard]] bool is_valid_storage_type(TypeHandle type) const;
    [[nodiscard]] bool is_valid_cast(syntax::ExprKind kind, TypeHandle dst, TypeHandle src) const;
    [[nodiscard]] bool parse_integer_literal_text(std::string_view text, base::u64& value) const noexcept;
    [[nodiscard]] bool integer_literal_fits_type(TypeHandle destination, std::string_view text) const noexcept;
    [[nodiscard]] bool negative_integer_literal_fits_type(TypeHandle destination, std::string_view text) const noexcept;
    [[nodiscard]] TypeHandle analyze_integer_literal(
        syntax::ExprId expr,
        std::string_view text,
        const base::SourceRange& range,
        TypeHandle expected_type
    );
    [[nodiscard]] TypeHandle analyze_negative_integer_literal(
        syntax::ExprId expr,
        std::string_view text,
        const base::SourceRange& range,
        TypeHandle expected_type
    );
    [[nodiscard]] TypeHandle analyze_float_literal(
        syntax::ExprId expr,
        std::string_view text,
        const base::SourceRange& range,
        TypeHandle expected_type
    );
    [[nodiscard]] bool is_const_evaluable_expr(syntax::ExprId expr, ModuleLookupSet& dependencies);
    [[nodiscard]] TypeAbiLayout abi_layout(TypeHandle type) const;
    [[nodiscard]] base::u64 abi_size(TypeHandle type) const;
    [[nodiscard]] base::u64 abi_align(TypeHandle type) const;
    [[nodiscard]] bool is_integer_literal(syntax::ExprId expr) const noexcept;
    [[nodiscard]] bool is_null_literal(syntax::ExprId expr) const noexcept;
    [[nodiscard]] bool is_null_result_expr(syntax::ExprId expr) const noexcept;
    [[nodiscard]] bool is_place_expr(syntax::ExprId expr);
    [[nodiscard]] bool is_writable_place(syntax::ExprId expr);
    [[nodiscard]] bool is_array_containing_value_type(TypeHandle type) const noexcept;
    [[nodiscard]] const StructInfo* find_struct(TypeHandle type) const noexcept;
    [[nodiscard]] ModuleSelector resolve_module_selector(syntax::ExprId expr, bool report_unknown);
    [[nodiscard]] NamedTypeSelector resolve_named_type_selector(syntax::ExprId expr, bool report_unknown);
    [[nodiscard]] TypeHandle resolve_type_selector(syntax::ExprId expr, bool report_unknown);
    [[nodiscard]] TypeHandle resolve_named_type_selector_type(
        const NamedTypeSelector& selector,
        bool opaque_allowed_as_pointee,
        bool report_unknown
    );
    [[nodiscard]] TypeHandle resolve_generic_type_selector(
        const NamedTypeSelector& selector,
        syntax::TypeId use_type_id,
        bool opaque_allowed_as_pointee,
        bool report_unknown
    );
    [[nodiscard]] bool selector_base_has_non_module_meaning(IdentId name_id, std::string_view name) const;
    [[nodiscard]] bool module_alias_visible(std::string_view name) const;
    [[nodiscard]] bool visible_root_module_name_exists(std::string_view name) const;
    [[nodiscard]] bool visible_module_path_prefix_exists(const std::vector<std::string_view>& parts) const;
    [[nodiscard]] ModuleSelectorPath expr_selector_path(syntax::ExprId expr) const;
    [[nodiscard]] bool current_generic_param_exists(IdentId name_id, std::string_view name) const;
    [[nodiscard]] bool visible_type_name_exists(IdentId name_id, std::string_view name) const;
    [[nodiscard]] bool can_define_local_name(IdentId name_id, std::string_view name, const base::SourceRange& range);
    [[nodiscard]] bool module_type_or_value_name_exists(syntax::ModuleId module, IdentId name_id, std::string_view name) const;
    [[nodiscard]] bool top_level_value_name_exists(syntax::ModuleId module, IdentId name_id, std::string_view name) const;
    [[nodiscard]] bool type_member_name_exists(TypeHandle owner_type, IdentId name_id, std::string_view name) const;
    [[nodiscard]] const FunctionSignature* find_function_selector(
        syntax::ExprId callee,
        IdentId name_id,
        std::string_view name,
        const base::SourceRange& range,
        bool report_unknown
    );
    [[nodiscard]] const GenericTemplateInfo* find_generic_function_selector(
        const NamedTypeSelector& selector,
        const base::SourceRange& range,
        bool report_unknown
    );
    [[nodiscard]] TypeHandle analyze_module_member_expr(
        syntax::ExprId expr_id,
        syntax::ModuleId module,
        const ExprView& expr
    );
    [[nodiscard]] bool record_no_payload_enum_case_expr(
        syntax::ExprId expr_id,
        const EnumCaseInfo& enum_case,
        const base::SourceRange& range
    );
    [[nodiscard]] TypeHandle resolve_associated_type_owner(syntax::ExprId object, bool report_unknown);
    [[nodiscard]] TypeHandle resolve_associated_generic_type_owner(syntax::ExprId apply, bool report_unknown);
    [[nodiscard]] TypeHandle function_type_from_signature(const FunctionSignature& signature);
    [[nodiscard]] TypeHandle function_type_from_symbol(const Symbol& symbol, const base::SourceRange& range);
    [[nodiscard]] TryShape classify_try_shape(TypeHandle type) const noexcept;
    [[nodiscard]] bool in_unsafe_context() const noexcept;
    void require_unsafe_context(const base::SourceRange& range, std::string_view operation);
    void validate_unsafe_call(const FunctionSignature& signature, const base::SourceRange& range);
    void validate_unsafe_function_value_call(TypeHandle callee_type, const base::SourceRange& range);
    [[nodiscard]] syntax::ModuleId item_module(syntax::ItemId item) const noexcept;
    void normalize_parser_only_module_contract();
    [[nodiscard]] bool validate_ast_contract();
    [[nodiscard]] syntax::ModuleId resolve_import_alias(std::string_view alias, const base::SourceRange& range, bool report_unknown = true);
    [[nodiscard]] std::vector<std::string_view> type_scope_parts(const syntax::TypeNode& type) const;
    [[nodiscard]] syntax::ModuleId resolve_type_scope(const syntax::TypeNode& type, bool report_unknown);
    [[nodiscard]] syntax::ModuleId find_visible_module_path(const std::vector<std::string_view>& parts) const;
    [[nodiscard]] const ModuleIdList& visible_modules(syntax::ModuleId module) const;
    [[nodiscard]] const ModuleIdList& module_export_modules(syntax::ModuleId module) const;
    void append_public_reexports(syntax::ModuleId module, ModuleIdList& result, std::unordered_set<base::u32>& seen) const;
    [[nodiscard]] std::string module_name(syntax::ModuleId module) const;
    [[nodiscard]] std::string qualified_name(syntax::ModuleId module, std::string_view name) const;
    [[nodiscard]] std::string c_symbol_name(syntax::ModuleId module, std::string_view name) const;
    [[nodiscard]] std::string module_key(syntax::ModuleId module, std::string_view name) const;
    [[nodiscard]] std::string module_key(syntax::ModuleId module, IdentId name_id, std::string_view fallback_name = {}) const;
    [[nodiscard]] std::string function_key(const syntax::ItemNode& function, syntax::ItemId function_id) const;
    [[nodiscard]] std::string method_key(syntax::ModuleId module, TypeHandle owner_type, std::string_view name) const;
    [[nodiscard]] std::string method_key(syntax::ModuleId module, TypeHandle owner_type, IdentId name_id, std::string_view fallback_name = {}) const;
    [[nodiscard]] std::string method_c_symbol_name(TypeHandle owner_type, std::string_view name) const;
    [[nodiscard]] ModuleLookupKey module_lookup_key(syntax::ModuleId module, IdentId name) const noexcept;
    [[nodiscard]] MethodLookupKey method_lookup_key(syntax::ModuleId module, TypeHandle owner_type, IdentId name) const noexcept;
    [[nodiscard]] ModuleLookupKey intern_module_lookup_key(syntax::ModuleId module, IdentId name) const noexcept;
    [[nodiscard]] ModuleLookupKey find_module_lookup_key(syntax::ModuleId module, IdentId name) const noexcept;
    [[nodiscard]] MethodLookupKey intern_method_lookup_key(syntax::ModuleId module, TypeHandle owner_type, IdentId name) const noexcept;
    [[nodiscard]] MethodLookupKey find_method_lookup_key(syntax::ModuleId module, TypeHandle owner_type, IdentId name) const noexcept;
    void index_named_type(syntax::ModuleId module, IdentId name_id, TypeHandle type, syntax::Visibility visibility);
    void index_type_alias(const TypeAliasInfo& info);
    void index_generic_struct_template(const GenericTemplateInfo& info);
    void index_generic_enum_template(const GenericTemplateInfo& info);
    void index_generic_type_alias_template(const GenericTemplateInfo& info);
    void index_generic_function_template(const GenericTemplateInfo& info);
    void index_generic_method_template(const GenericTemplateInfo& info);
    void index_function_lookup(const FunctionSignature& signature);
    void index_method_lookup(syntax::ModuleId module, TypeHandle owner_type, IdentId name_id, const FunctionSignature& signature);
    void index_function_value(const FunctionSignature& signature);
    void index_global_value(const Symbol& symbol);
    [[nodiscard]] bool named_type_lookup_complete() const noexcept;
    [[nodiscard]] bool type_alias_lookup_complete() const noexcept;
    [[nodiscard]] bool generic_struct_lookup_complete() const noexcept;
    [[nodiscard]] bool generic_enum_lookup_complete() const noexcept;
    [[nodiscard]] bool generic_type_alias_lookup_complete() const noexcept;
    [[nodiscard]] bool generic_function_lookup_complete() const noexcept;
    [[nodiscard]] bool generic_method_lookup_complete() const noexcept;
    [[nodiscard]] bool function_lookup_complete() const noexcept;
    [[nodiscard]] bool global_value_lookup_complete() const noexcept;
    [[nodiscard]] bool enum_case_module_lookup_complete() const noexcept;
    [[nodiscard]] bool can_access(syntax::ModuleId owner, syntax::Visibility visibility) const noexcept;
    void record_stmt_local_type(syntax::StmtId stmt, TypeHandle type);
    void record_expr_c_name(syntax::ExprId expr, std::string_view c_name);
    void record_pattern_c_name(syntax::PatternId pattern, std::string_view c_name);
    void record_pattern_case_name(syntax::PatternId pattern, std::string_view c_name);
    void merge_pattern_case_names(syntax::PatternId pattern, syntax::PatternId alternative);
    void record_syntax_type_handle(syntax::TypeId type, TypeHandle resolved);
    [[nodiscard]] bool method_receiver_matches(const FunctionSignature& signature, TypeHandle receiver_type, syntax::ExprId receiver);
    [[nodiscard]] syntax::ModuleId owner_module(TypeHandle owner_type) const noexcept;
    [[nodiscard]] const FunctionSignature* find_method_in_owner_module(
        TypeHandle owner_type,
        IdentId name_id,
        std::string_view name,
        bool require_self
    ) const;
    [[nodiscard]] const FunctionSignature* find_method_in_visible_modules(
        TypeHandle owner_type,
        IdentId name_id,
        std::string_view name,
        const base::SourceRange& range,
        bool require_self,
        bool report_unknown = true
    );
    [[nodiscard]] TypeHandle find_type_in_visible_modules(
        IdentId name_id,
        std::string_view name,
        const base::SourceRange& range,
        bool opaque_allowed_as_pointee,
        bool report_unknown = true
    );
    [[nodiscard]] TypeHandle find_type_in_module(
        syntax::ModuleId module,
        IdentId name_id,
        std::string_view name,
        const base::SourceRange& range,
        bool opaque_allowed_as_pointee,
        bool report_unknown = true
    );
    [[nodiscard]] const FunctionSignature* find_function_in_visible_modules(IdentId name_id, std::string_view name, const base::SourceRange& range, bool report_unknown = true);
    [[nodiscard]] const FunctionSignature* find_function_in_module(syntax::ModuleId module, IdentId name_id, std::string_view name, const base::SourceRange& range, bool report_unknown = true);
    [[nodiscard]] const EnumCaseInfo* find_enum_case_in_visible_modules(IdentId name_id, std::string_view name, const base::SourceRange& range, bool report_unknown = true);
    [[nodiscard]] const EnumCaseInfo* find_enum_case_by_type_and_case(
        TypeHandle enum_type,
        IdentId case_name_id,
        std::string_view case_name
    ) const;
    [[nodiscard]] const EnumCaseList* find_enum_cases_by_type(TypeHandle enum_type) const noexcept;
    [[nodiscard]] const EnumCaseInfo* find_enum_case_by_scoped_name(
        IdentId enum_name_id,
        std::string_view enum_name,
        IdentId case_name_id,
        std::string_view case_name,
        const base::SourceRange& range,
        bool report_unknown = true
    );
    [[nodiscard]] const EnumCaseInfo* find_enum_case_by_pattern_type(
        syntax::TypeId enum_type,
        IdentId case_name_id,
        std::string_view case_name,
        const base::SourceRange& range
    );
    [[nodiscard]] const EnumCaseInfo* find_enum_constructor(syntax::ExprId callee, bool report_unknown);
    [[nodiscard]] const Symbol* find_symbol(IdentId name_id, std::string_view name, const base::SourceRange& range);
    [[nodiscard]] const Symbol* find_symbol_in_module(
        syntax::ModuleId module,
        IdentId name_id,
        std::string_view name,
        const base::SourceRange& range,
        bool report_unknown = true
    );
    [[nodiscard]] TypeHandle record_expr_type(syntax::ExprId expr, TypeHandle type);
    void record_expr_expected_type(syntax::ExprId expr, TypeHandle expected_type);
    void record_coercion(syntax::ExprId expr, TypeHandle from_type, TypeHandle to_type, CoercionKind kind);
    [[nodiscard]] TypeHandle cached_expr_type(syntax::ExprId expr) const noexcept;
    [[nodiscard]] TypeHandle cached_expr_expected_type(syntax::ExprId expr) const noexcept;
    [[nodiscard]] TypeHandle cached_expr_type_for_expected(syntax::ExprId expr, TypeHandle expected_type) const noexcept;
    [[nodiscard]] TypeHandle cached_syntax_type(syntax::TypeId type) const noexcept;
    [[nodiscard]] std::string_view cached_expr_c_name(syntax::ExprId expr) const noexcept;
    [[nodiscard]] std::string_view cached_pattern_c_name(syntax::PatternId pattern) const noexcept;
    [[nodiscard]] SemaTypeTable& active_expr_types() noexcept;
    [[nodiscard]] SemaTypeTable& active_expr_expected_types() noexcept;
    [[nodiscard]] SemaIdentTable& active_expr_c_name_ids() noexcept;
    [[nodiscard]] SemaIdentTable& active_pattern_c_name_ids() noexcept;
    [[nodiscard]] PatternCaseNameTable& active_pattern_case_name_ids() noexcept;
    [[nodiscard]] SemaTypeTable& active_syntax_type_handles() noexcept;
    [[nodiscard]] SemaTypeTable& active_stmt_local_types() noexcept;
    [[nodiscard]] PlaceInfo analyze_place_info(syntax::ExprId expr_id, bool emit_diagnostics);
    void require_place_projection_safety(const PlaceInfo& place, const base::SourceRange& range);
    [[nodiscard]] syntax::TypeId push_synthetic_type(syntax::TypeNode node);
    void ensure_expr_side_table_size(base::usize size);
    void ensure_type_side_table_size(base::usize size);
    void index_enum_case(const EnumCaseInfo& info);
    [[nodiscard]] GenericTemplateList& generic_method_template_bucket(const ModuleLookupKey& key);
    [[nodiscard]] EnumCaseList& enum_case_type_bucket(TypeHandle enum_type);
    [[nodiscard]] ModuleIdList make_module_id_list() const;
    void report(const base::SourceRange& range, std::string message);

    std::optional<syntax::AstModule> owned_module_;
    syntax::AstModule& module_;
    base::DiagnosticSink& diagnostics_;
    SemanticOptions options_;
    std::unique_ptr<base::BumpAllocator> arena_;
    CheckedModule checked_;
    SymbolTable symbols_;
    SemaMap<std::string, TypeHandle> named_types_;
    SemaMap<std::string, syntax::Visibility> type_visibilities_;
    SemaMap<std::string, GenericTemplateInfo> generic_struct_templates_;
    SemaMap<std::string, GenericTemplateInfo> generic_enum_templates_;
    SemaMap<std::string, GenericTemplateInfo> generic_type_alias_templates_;
    SemaMap<std::string, GenericTemplateInfo> generic_function_templates_;
    SemaMap<std::string, GenericTemplateInfo> generic_method_templates_;
    SemaMap<std::string, TypeHandle> generic_struct_instances_;
    SemaMap<std::string, TypeHandle> generic_enum_instances_;
    SemaMap<std::string, TypeHandle> resolved_generic_type_aliases_;
    SemaMap<std::string, base::usize> generic_function_instances_;
    SemaMap<std::string, FunctionSignature> generic_placeholder_functions_;
    SemaMap<std::string, TypeHandle> resolved_type_aliases_;
    SemaVector<std::string> resolving_type_aliases_;
    SemaMap<std::string, Symbol> global_values_;
    SemaMap<std::string, syntax::ItemId> function_definition_items_;
    SemaMap<std::string, FunctionBodyState> function_body_states_;
    SemaMap<base::u32, const StructInfo*> struct_infos_by_type_;
    SemaMap<ModuleLookupKey, IndexedTypeInfo, ModuleLookupKeyHash> named_types_by_name_;
    SemaMap<ModuleLookupKey, const TypeAliasInfo*, ModuleLookupKeyHash> type_aliases_by_name_;
    SemaMap<ModuleLookupKey, const GenericTemplateInfo*, ModuleLookupKeyHash> generic_struct_templates_by_name_;
    SemaMap<ModuleLookupKey, const GenericTemplateInfo*, ModuleLookupKeyHash> generic_enum_templates_by_name_;
    SemaMap<ModuleLookupKey, const GenericTemplateInfo*, ModuleLookupKeyHash> generic_type_alias_templates_by_name_;
    SemaMap<ModuleLookupKey, const GenericTemplateInfo*, ModuleLookupKeyHash> generic_function_templates_by_name_;
    SemaMap<ModuleLookupKey, GenericTemplateList, ModuleLookupKeyHash> generic_method_templates_by_name_;
    base::usize generic_method_lookup_indexed_count_ = 0;
    SemaMap<ModuleLookupKey, const FunctionSignature*, ModuleLookupKeyHash> functions_by_name_;
    SemaMap<MethodLookupKey, const FunctionSignature*, MethodLookupKeyHash> methods_by_name_;
    base::usize internal_function_lookup_exclusions_ = 0;
    SemaMap<ModuleLookupKey, const Symbol*, ModuleLookupKeyHash> global_values_by_name_;
    SemaMap<MethodLookupKey, const Symbol*, MethodLookupKeyHash> method_global_values_by_name_;
    SemaMap<ModuleLookupKey, const EnumCaseInfo*, ModuleLookupKeyHash> enum_cases_by_module_name_;
    SemaMap<EnumCaseLookupKey, const EnumCaseInfo*, EnumCaseLookupKeyHash> enum_cases_by_type_and_case_;
    SemaMap<base::u32, EnumCaseList> enum_cases_by_type_;
    mutable SemaMap<base::u32, ModuleIdList> visible_modules_cache_;
    mutable SemaMap<base::u32, ModuleIdList> module_export_modules_cache_;
    syntax::ModuleId current_module_ = syntax::INVALID_MODULE_ID;
    TypeHandle current_function_return_type_ = INVALID_TYPE_HANDLE;
    ReturnTypeInference* current_return_inference_ = nullptr;
    GenericContext* current_generic_context_ = nullptr;
    GenericSideTableScope current_side_tables_ {};
    int loop_depth_ = SEMA_NO_LOOP_DEPTH;
    int unsafe_context_depth_ = 0;
    bool in_const_initializer_ = false;
};

} // namespace aurex::sema
