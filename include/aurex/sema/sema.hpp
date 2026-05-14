#pragma once

#include <aurex/base/diagnostic.hpp>
#include <aurex/base/result.hpp>
#include <aurex/sema/checked_module.hpp>
#include <aurex/sema/function.hpp>
#include <aurex/sema/symbol.hpp>
#include <aurex/sema/type.hpp>
#include <aurex/syntax/ast.hpp>

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace aurex::sema {

class SemanticAnalyzer final {
public:
    SemanticAnalyzer(const syntax::AstModule& module, base::DiagnosticSink& diagnostics) noexcept;

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
    };

    struct GenericTemplateInfo {
        syntax::ItemId item = syntax::INVALID_ITEM_ID;
        syntax::ModuleId module = syntax::INVALID_MODULE_ID;
        std::string name;
        std::string key;
        std::vector<std::string> params;
        std::unordered_map<std::string, std::unordered_set<std::string>> constraints;
        TypeHandle impl_type_pattern = INVALID_TYPE_HANDLE;
        syntax::Visibility visibility = syntax::Visibility::private_;
    };

    struct GenericContext {
        std::unordered_map<std::string, TypeHandle> params;
        std::unordered_map<std::string, std::unordered_set<std::string>> constraints;
    };

    struct GenericSideTableScope {
        GenericSideTables* side_tables = nullptr;
        bool cache_syntax_types = true;
    };

    struct ModuleSelector {
        syntax::ModuleId module = syntax::INVALID_MODULE_ID;
        bool failed_as_import_alias = false;
    };

    struct NamedTypeSelector {
        syntax::ModuleId module = syntax::INVALID_MODULE_ID;
        std::string_view name;
        base::SourceRange range {};
        std::vector<syntax::TypeId> type_args;
        bool qualified = false;
    };

    struct PatternBinding {
        std::string name;
        TypeHandle type = INVALID_TYPE_HANDLE;
        base::SourceRange range {};
    };

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

    struct TypeAbiLayout {
        base::u64 size = SEMA_TYPE_ABI_INVALID_SIZE;
        base::u64 align = SEMA_TYPE_ABI_MIN_ALIGNMENT;
    };

    void register_type_names();
    void register_generic_template(const syntax::ItemNode& item, syntax::ItemId item_id);
    void validate_generic_parameter_list(const syntax::ItemNode& item);
    void validate_generic_constraints(const syntax::ItemNode& item, GenericTemplateInfo& info);
    [[nodiscard]] bool generic_param_has_capability(std::string_view param, std::string_view capability) const;
    [[nodiscard]] bool type_satisfies_capability(TypeHandle type, std::string_view capability) const;
    [[nodiscard]] bool validate_generic_arguments(
        const GenericTemplateInfo& info,
        const std::vector<TypeHandle>& args,
        base::SourceRange use_range
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
    void analyze_function_body(const syntax::ItemNode& function);
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
    void validate_function_return_type(const syntax::ItemNode& function, TypeHandle return_type);
    void ensure_function_return_known(const FunctionSignature& signature, base::SourceRange use_range);
    [[nodiscard]] TypeHandle analyze_expr(syntax::ExprId expr);
    [[nodiscard]] TypeHandle analyze_expr(syntax::ExprId expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_postfix_chain_expr(syntax::ExprId expr_id, TypeHandle expected_type);
    [[nodiscard]] syntax::ExprId materialize_postfix_chain(syntax::ExprId expr_id);
    [[nodiscard]] syntax::ExprId materialize_postfix_op(
        syntax::ExprId chain_expr,
        syntax::ExprId base,
        const syntax::PostfixOp& op,
        const syntax::PostfixOp* next_op,
        bool is_last
    );
    [[nodiscard]] syntax::ExprId materialize_postfix_bracket_op(
        syntax::ExprId chain_expr,
        syntax::ExprId base,
        const syntax::PostfixOp& op,
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
    [[nodiscard]] TypeHandle analyze_name_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr);
    [[nodiscard]] TypeHandle analyze_generic_apply_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr);
    [[nodiscard]] TypeHandle analyze_unary_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_binary_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_field_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_index_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr);
    [[nodiscard]] TypeHandle analyze_slice_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_struct_literal_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_cast_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr);
    [[nodiscard]] TypeHandle analyze_size_or_align_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr);
    [[nodiscard]] TypeHandle analyze_ptr_addr_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr);
    [[nodiscard]] TypeHandle analyze_paddr_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr);
    [[nodiscard]] TypeHandle analyze_str_projection_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr);
    [[nodiscard]] TypeHandle analyze_str_utf8_slice_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr);
    [[nodiscard]] TypeHandle analyze_str_from_bytes_unchecked_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr);
    [[nodiscard]] TypeHandle analyze_call_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_enum_constructor_call(syntax::ExprId expr_id, const syntax::ExprNode& expr, const EnumCaseInfo& enum_case);
    [[nodiscard]] TypeHandle analyze_field_call_expr(
        syntax::ExprId expr_id,
        const syntax::ExprNode& expr,
        const syntax::ExprNode& callee,
        std::string_view name,
        TypeHandle expected_type
    );
    [[nodiscard]] TypeHandle analyze_function_call_expr(
        syntax::ExprId expr_id,
        const syntax::ExprNode& expr,
        const syntax::ExprNode& callee,
        std::string_view name,
        TypeHandle expected_type
    );
    [[nodiscard]] TypeHandle analyze_function_value_call_expr(
        syntax::ExprId expr_id,
        const syntax::ExprNode& expr,
        std::string_view name
    );
    [[nodiscard]] TypeHandle analyze_explicit_generic_function_call_expr(
        syntax::ExprId expr_id,
        const syntax::ExprNode& expr,
        const syntax::ExprNode& apply,
        std::string_view name
    );
    void validate_call_arguments(
        const syntax::ExprNode& expr,
        std::string_view name,
        const std::vector<TypeHandle>& param_types,
        base::usize receiver_count,
        bool is_variadic
    );
    [[nodiscard]] TypeHandle analyze_try_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr);
    [[nodiscard]] TypeHandle analyze_if_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_block_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_unsafe_block_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_match_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_array_literal_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_tuple_literal_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr, TypeHandle expected_type);
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
        const syntax::ExprNode& call,
        std::vector<TypeHandle>& args
    );
    [[nodiscard]] bool unify_generic_type(
        TypeHandle pattern,
        TypeHandle actual,
        std::unordered_map<std::string, TypeHandle>& inferred
    ) const;
    [[nodiscard]] const GenericTemplateInfo* find_generic_function_in_visible_modules(
        std::string_view name,
        base::SourceRange range,
        bool report_unknown = true
    );
    [[nodiscard]] const GenericTemplateInfo* find_generic_function_in_module(
        syntax::ModuleId module,
        std::string_view name,
        base::SourceRange range,
        bool report_unknown = true
    );
    [[nodiscard]] const GenericTemplateInfo* find_generic_struct_in_visible_modules(
        std::string_view name,
        base::SourceRange range,
        bool report_unknown = true
    );
    [[nodiscard]] const GenericTemplateInfo* find_generic_struct_in_module(
        syntax::ModuleId module,
        std::string_view name,
        base::SourceRange range,
        bool report_unknown = true
    );
    [[nodiscard]] const GenericTemplateInfo* find_generic_enum_in_visible_modules(
        std::string_view name,
        base::SourceRange range,
        bool report_unknown = true
    );
    [[nodiscard]] const GenericTemplateInfo* find_generic_enum_in_module(
        syntax::ModuleId module,
        std::string_view name,
        base::SourceRange range,
        bool report_unknown = true
    );
    [[nodiscard]] const GenericTemplateInfo* find_generic_type_alias_in_visible_modules(
        std::string_view name,
        base::SourceRange range,
        bool report_unknown = true
    );
    [[nodiscard]] const GenericTemplateInfo* find_generic_type_alias_in_module(
        syntax::ModuleId module,
        std::string_view name,
        base::SourceRange range,
        bool report_unknown = true
    );
    [[nodiscard]] bool generic_type_template_exists_in_module(
        syntax::ModuleId module,
        std::string_view name
    ) const;
    [[nodiscard]] const GenericTemplateInfo* find_any_generic_type_template_in_module(
        syntax::ModuleId module,
        std::string_view name
    ) const;
    [[nodiscard]] bool report_generic_type_requires_args_if_visible(
        std::string_view name,
        base::SourceRange range
    );
    void report_generic_type_template_in_module(
        syntax::ModuleId module,
        std::string_view name,
        base::SourceRange range
    );
    [[nodiscard]] FunctionSignature* instantiate_generic_function(
        const GenericTemplateInfo& info,
        const std::vector<TypeHandle>& args,
        base::SourceRange use_range
    );
    [[nodiscard]] FunctionSignature* instantiate_generic_method(
        const GenericTemplateInfo& info,
        TypeHandle owner_type,
        const std::vector<TypeHandle>& args,
        base::SourceRange use_range
    );
    [[nodiscard]] FunctionSignature* instantiate_generic_placeholder_function(
        const GenericTemplateInfo& info,
        const std::vector<TypeHandle>& args,
        base::SourceRange use_range
    );
    [[nodiscard]] FunctionSignature* find_generic_method_in_visible_modules(
        TypeHandle owner_type,
        std::string_view name,
        base::SourceRange range,
        bool require_self,
        bool report_unknown = true
    );
    [[nodiscard]] bool type_contains_generic_param(TypeHandle type) const;
    [[nodiscard]] std::string generic_instance_suffix(const std::vector<TypeHandle>& args) const;
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
    [[nodiscard]] TypeHandle analyze_integer_literal(syntax::ExprId expr, const syntax::ExprNode& node, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_negative_integer_literal(
        syntax::ExprId expr,
        const syntax::ExprNode& node,
        TypeHandle expected_type
    );
    [[nodiscard]] TypeHandle analyze_float_literal(syntax::ExprId expr, const syntax::ExprNode& node, TypeHandle expected_type);
    [[nodiscard]] bool is_const_evaluable_expr(syntax::ExprId expr, std::unordered_set<std::string>& dependencies);
    [[nodiscard]] TypeAbiLayout abi_layout(TypeHandle type) const;
    [[nodiscard]] base::u64 abi_size(TypeHandle type) const;
    [[nodiscard]] base::u64 abi_align(TypeHandle type) const;
    [[nodiscard]] bool is_integer_literal(syntax::ExprId expr) const noexcept;
    [[nodiscard]] bool is_null_literal(syntax::ExprId expr) const noexcept;
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
    [[nodiscard]] bool selector_base_has_non_module_meaning(std::string_view name) const;
    [[nodiscard]] bool module_alias_visible(std::string_view name) const;
    [[nodiscard]] bool current_generic_param_exists(std::string_view name) const;
    [[nodiscard]] bool visible_type_name_exists(std::string_view name) const;
    [[nodiscard]] bool can_define_local_name(std::string_view name, base::SourceRange range);
    [[nodiscard]] bool module_type_or_value_name_exists(syntax::ModuleId module, std::string_view name) const;
    [[nodiscard]] bool top_level_value_name_exists(syntax::ModuleId module, std::string_view name) const;
    [[nodiscard]] bool type_member_name_exists(TypeHandle owner_type, std::string_view name) const;
    [[nodiscard]] const FunctionSignature* find_function_selector(
        syntax::ExprId callee,
        std::string_view name,
        base::SourceRange range,
        bool report_unknown
    );
    [[nodiscard]] const GenericTemplateInfo* find_generic_function_selector(
        const NamedTypeSelector& selector,
        base::SourceRange range,
        bool report_unknown
    );
    [[nodiscard]] TypeHandle analyze_module_member_expr(
        syntax::ExprId expr_id,
        syntax::ModuleId module,
        const syntax::ExprNode& expr
    );
    [[nodiscard]] bool record_no_payload_enum_case_expr(
        syntax::ExprId expr_id,
        const EnumCaseInfo& enum_case,
        base::SourceRange range
    );
    [[nodiscard]] TypeHandle resolve_associated_type_owner(const syntax::ExprNode& object, bool report_unknown);
    [[nodiscard]] TypeHandle resolve_associated_generic_type_owner(const syntax::ExprNode& apply, bool report_unknown);
    [[nodiscard]] TypeHandle function_type_from_signature(const FunctionSignature& signature);
    [[nodiscard]] TypeHandle function_type_from_symbol(const Symbol& symbol, base::SourceRange range);
    [[nodiscard]] bool in_unsafe_context() const noexcept;
    void require_unsafe_context(base::SourceRange range, std::string_view operation);
    void validate_unsafe_call(const FunctionSignature& signature, base::SourceRange range);
    void validate_unsafe_function_value_call(TypeHandle callee_type, base::SourceRange range);
    [[nodiscard]] syntax::ModuleId item_module(const syntax::ItemNode& item) const noexcept;
    [[nodiscard]] syntax::ModuleId resolve_import_alias(std::string_view alias, base::SourceRange range, bool report_unknown = true);
    [[nodiscard]] const std::vector<syntax::ModuleId>& visible_modules(syntax::ModuleId module) const;
    [[nodiscard]] std::vector<syntax::ModuleId> module_export_modules(syntax::ModuleId module) const;
    void append_public_reexports(syntax::ModuleId module, std::vector<syntax::ModuleId>& result, std::unordered_set<base::u32>& seen) const;
    [[nodiscard]] std::string module_name(syntax::ModuleId module) const;
    [[nodiscard]] std::string qualified_name(syntax::ModuleId module, std::string_view name) const;
    [[nodiscard]] std::string c_symbol_name(syntax::ModuleId module, std::string_view name) const;
    [[nodiscard]] std::string module_key(syntax::ModuleId module, std::string_view name) const;
    [[nodiscard]] std::string function_key(const syntax::ItemNode& function) const;
    [[nodiscard]] std::string method_key(syntax::ModuleId module, TypeHandle owner_type, std::string_view name) const;
    [[nodiscard]] std::string method_c_symbol_name(TypeHandle owner_type, std::string_view name) const;
    [[nodiscard]] bool can_access(syntax::ModuleId owner, syntax::Visibility visibility) const noexcept;
    void record_stmt_local_type(syntax::StmtId stmt, TypeHandle type) noexcept;
    void record_expr_c_name(syntax::ExprId expr, std::string_view c_name);
    void record_pattern_c_name(syntax::PatternId pattern, std::string_view c_name);
    void record_pattern_case_name(syntax::PatternId pattern, std::string_view c_name);
    void merge_pattern_case_names(syntax::PatternId pattern, syntax::PatternId alternative);
    void record_syntax_type_handle(syntax::TypeId type, TypeHandle resolved) noexcept;
    [[nodiscard]] bool method_receiver_matches(const FunctionSignature& signature, TypeHandle receiver_type, syntax::ExprId receiver);
    [[nodiscard]] syntax::ModuleId owner_module(TypeHandle owner_type) const noexcept;
    [[nodiscard]] const FunctionSignature* find_method_in_owner_module(
        TypeHandle owner_type,
        std::string_view name,
        bool require_self
    ) const;
    [[nodiscard]] const FunctionSignature* find_method_in_visible_modules(
        TypeHandle owner_type,
        std::string_view name,
        base::SourceRange range,
        bool require_self,
        bool report_unknown = true
    );
    [[nodiscard]] TypeHandle find_type_in_visible_modules(
        std::string_view name,
        base::SourceRange range,
        bool opaque_allowed_as_pointee,
        bool report_unknown = true
    );
    [[nodiscard]] TypeHandle find_type_in_module(
        syntax::ModuleId module,
        std::string_view name,
        base::SourceRange range,
        bool opaque_allowed_as_pointee,
        bool report_unknown = true
    );
    [[nodiscard]] const FunctionSignature* find_function_in_visible_modules(std::string_view name, base::SourceRange range, bool report_unknown = true);
    [[nodiscard]] const FunctionSignature* find_function_in_module(syntax::ModuleId module, std::string_view name, base::SourceRange range, bool report_unknown = true);
    [[nodiscard]] const EnumCaseInfo* find_enum_case_in_visible_modules(std::string_view name, base::SourceRange range, bool report_unknown = true);
    [[nodiscard]] const EnumCaseInfo* find_enum_case_by_type_and_case(TypeHandle enum_type, std::string_view case_name) const;
    [[nodiscard]] const std::vector<const EnumCaseInfo*>* find_enum_cases_by_type(TypeHandle enum_type) const noexcept;
    [[nodiscard]] const EnumCaseInfo* find_enum_case_by_scoped_name(
        std::string_view enum_name,
        std::string_view case_name,
        base::SourceRange range,
        bool report_unknown = true
    );
    [[nodiscard]] const EnumCaseInfo* find_enum_constructor(syntax::ExprId callee, bool report_unknown);
    [[nodiscard]] const Symbol* find_symbol(std::string_view name, base::SourceRange range);
    [[nodiscard]] const Symbol* find_symbol_in_module(syntax::ModuleId module, std::string_view name, base::SourceRange range, bool report_unknown = true);
    [[nodiscard]] TypeHandle record_expr_type(syntax::ExprId expr, TypeHandle type) noexcept;
    [[nodiscard]] std::vector<TypeHandle>& active_expr_types() noexcept;
    [[nodiscard]] std::vector<std::string>& active_expr_c_names() noexcept;
    [[nodiscard]] std::vector<std::string>& active_pattern_c_names() noexcept;
    [[nodiscard]] std::vector<std::unordered_set<std::string>>& active_pattern_case_sets() noexcept;
    [[nodiscard]] std::vector<TypeHandle>& active_syntax_type_handles() noexcept;
    [[nodiscard]] std::vector<TypeHandle>& active_stmt_local_types() noexcept;
    [[nodiscard]] syntax::ExprId push_synthetic_expr(syntax::ExprNode node);
    [[nodiscard]] syntax::TypeId push_synthetic_type(syntax::TypeNode node);
    void ensure_expr_side_table_size(base::usize size);
    void ensure_type_side_table_size(base::usize size);
    void index_enum_case(const EnumCaseInfo& info);
    void report(base::SourceRange range, std::string message);

    syntax::AstModule module_;
    base::DiagnosticSink& diagnostics_;
    CheckedModule checked_;
    SymbolTable symbols_;
    std::unordered_map<std::string, TypeHandle> named_types_;
    std::unordered_map<std::string, syntax::Visibility> type_visibilities_;
    std::unordered_map<std::string, GenericTemplateInfo> generic_struct_templates_;
    std::unordered_map<std::string, GenericTemplateInfo> generic_enum_templates_;
    std::unordered_map<std::string, GenericTemplateInfo> generic_type_alias_templates_;
    std::unordered_map<std::string, GenericTemplateInfo> generic_function_templates_;
    std::unordered_map<std::string, GenericTemplateInfo> generic_method_templates_;
    std::unordered_map<std::string, TypeHandle> generic_struct_instances_;
    std::unordered_map<std::string, TypeHandle> generic_enum_instances_;
    std::unordered_map<std::string, TypeHandle> resolved_generic_type_aliases_;
    std::unordered_map<std::string, base::usize> generic_function_instances_;
    std::unordered_map<std::string, FunctionSignature> generic_placeholder_functions_;
    std::unordered_map<std::string, TypeHandle> resolved_type_aliases_;
    std::vector<std::string> resolving_type_aliases_;
    std::unordered_map<std::string, Symbol> global_values_;
    std::unordered_map<std::string, syntax::ItemId> function_definition_items_;
    std::unordered_map<std::string, FunctionBodyState> function_body_states_;
    std::unordered_map<base::u32, const StructInfo*> struct_infos_by_type_;
    std::unordered_map<std::string, const EnumCaseInfo*> enum_cases_by_type_and_case_;
    std::unordered_map<base::u32, std::vector<const EnumCaseInfo*>> enum_cases_by_type_;
    mutable std::unordered_map<base::u32, std::vector<syntax::ModuleId>> visible_modules_cache_;
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
