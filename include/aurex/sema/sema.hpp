#pragma once

#include "aurex/base/diagnostic.hpp"
#include "aurex/base/result.hpp"
#include "aurex/sema/checked_module.hpp"
#include "aurex/sema/function.hpp"
#include "aurex/sema/generic.hpp"
#include "aurex/sema/symbol.hpp"
#include "aurex/sema/type.hpp"
#include "aurex/syntax/ast.hpp"

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
        TypeHandle inferred_type = invalid_type_handle;
        std::vector<syntax::StmtId> returns;
    };

    void register_type_names();
    void register_value_names();
    void validate_function_prototypes();
    void validate_abi_symbols();
    void validate_type_layouts();
    void analyze_entry_points();
    void resolve_type_alias_decls();
    void analyze_struct_properties();
    void analyze_const_decls();
    void analyze_function_body(const syntax::ItemNode& function);
    void analyze_function_body_with_signature(
        const syntax::ItemNode& function,
        const std::string& key,
        const FunctionSignature& signature,
        FunctionBodyState& state,
        const GenericTypeSubstitution* substitution
    );
    void analyze_block(syntax::StmtId block, TypeHandle expected_return, ReturnTypeInference* return_inference);
    void analyze_stmt(syntax::StmtId stmt, TypeHandle expected_return, ReturnTypeInference* return_inference);
    [[nodiscard]] bool block_guarantees_return(syntax::StmtId block) const noexcept;
    [[nodiscard]] bool stmt_guarantees_return(syntax::StmtId stmt) const noexcept;
    void record_inferred_return(syntax::StmtId stmt, TypeHandle actual, ReturnTypeInference& inference);
    void finalize_inferred_return(const syntax::ItemNode& function, const std::string& key, ReturnTypeInference& inference);
    void validate_function_return_type(const syntax::ItemNode& function, TypeHandle return_type);
    void ensure_function_return_known(const FunctionSignature& signature, base::SourceRange use_range);
    [[nodiscard]] TypeHandle analyze_expr(syntax::ExprId expr);
    [[nodiscard]] TypeHandle analyze_expr(syntax::ExprId expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_call_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_try_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr);
    [[nodiscard]] TypeHandle analyze_if_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_block_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_match_expr(syntax::ExprId expr_id, const syntax::ExprNode& expr, TypeHandle expected_type);
    [[nodiscard]] const EnumCaseInfo* analyze_enum_case_pattern(
        syntax::PatternId pattern,
        TypeHandle matched,
        std::vector<std::string>& covered,
        bool& saw_wildcard
    );
    [[nodiscard]] const EnumCaseInfo* analyze_single_enum_case_pattern(
        syntax::PatternId pattern,
        TypeHandle matched,
        std::vector<std::string>& covered,
        bool& saw_wildcard
    );
    [[nodiscard]] const EnumCaseInfo* analyze_value_pattern(
        syntax::PatternId pattern,
        TypeHandle matched,
        bool& covered_true,
        bool& covered_false,
        bool& saw_wildcard
    );
    [[nodiscard]] const EnumCaseInfo* analyze_single_value_pattern(
        syntax::PatternId pattern,
        TypeHandle matched,
        bool& covered_true,
        bool& covered_false,
        bool& saw_wildcard
    );
    [[nodiscard]] TypeHandle resolve_type(syntax::TypeId type);
    [[nodiscard]] TypeHandle resolve_type(syntax::TypeId type, bool opaque_allowed_as_pointee);
    [[nodiscard]] TypeHandle resolve_type_with_substitution(syntax::TypeId type, const GenericTypeSubstitution* substitution, bool opaque_allowed_as_pointee);
    [[nodiscard]] TypeHandle resolve_type_alias(const TypeAliasInfo& alias, bool opaque_allowed_as_pointee);
    [[nodiscard]] bool can_assign(TypeHandle dst, TypeHandle src, syntax::ExprId value) const noexcept;
    [[nodiscard]] bool is_valid_storage_type(TypeHandle type) const noexcept;
    [[nodiscard]] bool is_valid_cast(syntax::ExprKind kind, TypeHandle dst, TypeHandle src) const noexcept;
    [[nodiscard]] bool parse_integer_literal_text(std::string_view text, base::u64& value) const noexcept;
    [[nodiscard]] bool integer_literal_fits_type(TypeHandle destination, std::string_view text) const noexcept;
    [[nodiscard]] TypeHandle analyze_integer_literal(syntax::ExprId expr, const syntax::ExprNode& node, TypeHandle expected_type);
    [[nodiscard]] bool is_const_evaluable_expr(syntax::ExprId expr, std::unordered_set<std::string>& dependencies);
    [[nodiscard]] base::u64 abi_size(TypeHandle type) const noexcept;
    [[nodiscard]] base::u64 abi_align(TypeHandle type) const noexcept;
    [[nodiscard]] bool is_integer_literal(syntax::ExprId expr) const noexcept;
    [[nodiscard]] bool is_null_literal(syntax::ExprId expr) const noexcept;
    [[nodiscard]] bool is_place_expr(syntax::ExprId expr);
    [[nodiscard]] bool is_writable_place(syntax::ExprId expr);
    [[nodiscard]] bool is_copy_forbidden_value(TypeHandle type) const noexcept;
    [[nodiscard]] const StructInfo* find_struct(TypeHandle type) const noexcept;
    [[nodiscard]] TypeHandle resolve_associated_type_owner(const syntax::ExprNode& object, bool report_unknown);
    [[nodiscard]] syntax::ModuleId item_module(const syntax::ItemNode& item) const noexcept;
    [[nodiscard]] syntax::ModuleId resolve_import_alias(std::string_view alias, base::SourceRange range, bool report_unknown = true);
    [[nodiscard]] const std::vector<syntax::ModuleId>& visible_modules(syntax::ModuleId module) const;
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
    [[nodiscard]] const FunctionSignature* find_method_in_visible_modules(
        TypeHandle owner_type,
        std::string_view name,
        base::SourceRange range,
        bool require_self,
        bool report_unknown = true
    );
    [[nodiscard]] const GenericFunctionInstanceInfo* find_generic_method_in_visible_modules(
        TypeHandle owner_type,
        std::string_view name,
        base::SourceRange range,
        bool require_self,
        const std::vector<TypeHandle>* arg_types,
        TypeHandle expected_type,
        const std::vector<syntax::TypeId>* explicit_type_args,
        bool report_unknown = true
    );
    [[nodiscard]] const GenericEnumTemplateInfo* find_generic_enum_template_in_visible_modules(std::string_view name, base::SourceRange range, bool report_unknown = true);
    [[nodiscard]] const GenericStructTemplateInfo* find_generic_struct_template_in_visible_modules(std::string_view name, base::SourceRange range, bool report_unknown = true);
    [[nodiscard]] const GenericFunctionTemplateInfo* find_generic_function_template_in_visible_modules(std::string_view name, base::SourceRange range, bool report_unknown = true);
    [[nodiscard]] const GenericEnumTemplateInfo* find_generic_enum_template_in_module(syntax::ModuleId module, std::string_view name, base::SourceRange range, bool report_unknown = true);
    [[nodiscard]] const GenericStructTemplateInfo* find_generic_struct_template_in_module(syntax::ModuleId module, std::string_view name, base::SourceRange range, bool report_unknown = true);
    [[nodiscard]] const GenericFunctionTemplateInfo* find_generic_function_template_in_module(syntax::ModuleId module, std::string_view name, base::SourceRange range, bool report_unknown = true);
    [[nodiscard]] TypeHandle instantiate_generic_enum(const GenericEnumTemplateInfo& info, const std::vector<TypeHandle>& args, base::SourceRange range);
    [[nodiscard]] TypeHandle instantiate_generic_enum_from_syntax(
        const GenericEnumTemplateInfo& info,
        const std::vector<syntax::TypeId>& args,
        base::SourceRange range,
        bool opaque_allowed_as_pointee
    );
    [[nodiscard]] TypeHandle instantiate_generic_struct(const GenericStructTemplateInfo& info, const std::vector<TypeHandle>& args, base::SourceRange range);
    [[nodiscard]] TypeHandle instantiate_generic_struct_from_syntax(
        const GenericStructTemplateInfo& info,
        const std::vector<syntax::TypeId>& args,
        base::SourceRange range,
        bool opaque_allowed_as_pointee
    );
    [[nodiscard]] const EnumCaseInfo* instantiate_generic_enum_constructor(
        syntax::ExprId callee,
        const std::vector<TypeHandle>& arg_types,
        TypeHandle expected_type,
        bool report_unknown
    );
    [[nodiscard]] bool infer_generic_enum_args(
        syntax::TypeId pattern_type,
        TypeHandle actual,
        const GenericEnumTemplateInfo& info,
        std::vector<TypeHandle>& inferred,
        base::SourceRange range
    );
    [[nodiscard]] bool infer_generic_args_from_type_pattern(
        syntax::TypeId pattern_type,
        TypeHandle actual,
        const std::vector<std::string>& params,
        std::vector<TypeHandle>& inferred,
        base::SourceRange range,
        std::string_view context,
        syntax::ModuleId pattern_module
    );
    [[nodiscard]] TypeHandle infer_generic_struct_literal_type(
        const GenericStructTemplateInfo& info,
        const syntax::ExprNode& expr,
        TypeHandle expected_type
    );
    [[nodiscard]] const GenericFunctionInstanceInfo* instantiate_generic_function_from_syntax(
        const GenericFunctionTemplateInfo& info,
        const std::vector<syntax::TypeId>& args,
        base::SourceRange range
    );
    [[nodiscard]] const GenericFunctionInstanceInfo* instantiate_generic_function(
        const GenericFunctionTemplateInfo& info,
        const std::vector<TypeHandle>& args,
        base::SourceRange range
    );
    [[nodiscard]] bool infer_generic_function_args(
        const GenericFunctionTemplateInfo& info,
        const std::vector<TypeHandle>& arg_types,
        TypeHandle expected_type,
        std::vector<TypeHandle>& inferred,
        base::SourceRange range
    );
    [[nodiscard]] const GenericEnumInstanceInfo* generic_enum_instance(TypeHandle type) const noexcept;
    [[nodiscard]] const GenericStructInstanceInfo* generic_struct_instance(TypeHandle type) const noexcept;
    [[nodiscard]] std::string generic_instance_key(const GenericEnumTemplateInfo& info, const std::vector<TypeHandle>& args) const;
    [[nodiscard]] std::string generic_display_name(const GenericEnumTemplateInfo& info, const std::vector<TypeHandle>& args) const;
    [[nodiscard]] std::string generic_c_name(const GenericEnumTemplateInfo& info, const std::vector<TypeHandle>& args) const;
    [[nodiscard]] std::string generic_case_name(const GenericEnumTemplateInfo& info, const std::vector<TypeHandle>& args, std::string_view case_name) const;
    [[nodiscard]] std::string generic_case_c_name(const GenericEnumTemplateInfo& info, const std::vector<TypeHandle>& args, std::string_view case_name) const;
    [[nodiscard]] std::string generic_instance_key(const GenericStructTemplateInfo& info, const std::vector<TypeHandle>& args) const;
    [[nodiscard]] std::string generic_display_name(const GenericStructTemplateInfo& info, const std::vector<TypeHandle>& args) const;
    [[nodiscard]] std::string generic_c_name(const GenericStructTemplateInfo& info, const std::vector<TypeHandle>& args) const;
    [[nodiscard]] std::string generic_instance_key(const GenericFunctionTemplateInfo& info, const std::vector<TypeHandle>& args) const;
    [[nodiscard]] std::string generic_display_name(const GenericFunctionTemplateInfo& info, const std::vector<TypeHandle>& args) const;
    [[nodiscard]] std::string generic_c_name(const GenericFunctionTemplateInfo& info, const std::vector<TypeHandle>& args) const;
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
    [[nodiscard]] const EnumCaseInfo* find_enum_case_by_type_and_case(TypeHandle enum_type, std::string_view case_name) const noexcept;
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
    void report(base::SourceRange range, std::string message);

    const syntax::AstModule& module_;
    base::DiagnosticSink& diagnostics_;
    CheckedModule checked_;
    SymbolTable symbols_;
    std::unordered_map<std::string, TypeHandle> named_types_;
    std::unordered_map<std::string, syntax::Visibility> type_visibilities_;
    std::unordered_map<std::string, GenericEnumTemplateInfo> generic_enum_templates_;
    std::unordered_map<std::string, TypeHandle> generic_enum_instances_;
    std::unordered_map<base::u32, GenericEnumInstanceInfo> generic_enum_instance_infos_;
    std::unordered_map<std::string, GenericStructTemplateInfo> generic_struct_templates_;
    std::unordered_map<std::string, TypeHandle> generic_struct_instances_;
    std::unordered_map<base::u32, GenericStructInstanceInfo> generic_struct_instance_infos_;
    std::unordered_map<std::string, GenericFunctionTemplateInfo> generic_function_templates_;
    std::vector<GenericFunctionTemplateInfo> generic_method_templates_;
    std::unordered_multimap<std::string, base::u32> generic_method_template_indices_;
    std::unordered_map<std::string, base::u32> generic_function_instances_;
    std::unordered_map<std::string, FunctionBodyState> generic_function_body_states_;
    std::unordered_map<std::string, TypeHandle> resolved_type_aliases_;
    std::vector<std::string> resolving_type_aliases_;
    std::unordered_map<std::string, Symbol> global_values_;
    std::unordered_map<std::string, syntax::ItemId> function_definition_items_;
    std::unordered_map<std::string, FunctionBodyState> function_body_states_;
    std::unordered_map<base::u32, const StructInfo*> struct_infos_by_type_;
    mutable std::unordered_map<base::u32, std::vector<syntax::ModuleId>> visible_modules_cache_;
    syntax::ModuleId current_module_ = syntax::invalid_module_id;
    TypeHandle current_function_return_type_ = invalid_type_handle;
    const GenericTypeSubstitution* current_type_substitution_ = nullptr;
    std::unordered_map<base::u32, TypeHandle>* current_generic_syntax_type_handles_ = nullptr;
    std::unordered_map<base::u32, TypeHandle>* current_generic_expr_types_ = nullptr;
    std::unordered_map<base::u32, std::string>* current_generic_expr_c_names_ = nullptr;
    std::unordered_map<base::u32, std::string>* current_generic_pattern_c_names_ = nullptr;
    std::unordered_map<base::u32, std::unordered_set<std::string>>* current_generic_pattern_case_sets_ = nullptr;
    std::unordered_map<base::u32, TypeHandle>* current_generic_stmt_local_types_ = nullptr;
    int loop_depth_ = 0;
    bool in_const_initializer_ = false;
};

} // namespace aurex::sema
