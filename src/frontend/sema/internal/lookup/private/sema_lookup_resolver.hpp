#pragma once

#include <frontend/sema/internal/core/private/sema_core.hpp>

namespace aurex::sema {

class SemanticAnalyzerCore::LookupResolver final {
public:
    explicit LookupResolver(SemanticAnalyzerCore& core) noexcept;
    explicit LookupResolver(const SemanticAnalyzerCore& core) noexcept;

    [[nodiscard]] syntax::ModuleId resolve_import_alias(
        std::string_view alias, const base::SourceRange& range, bool report_unknown) const;
    [[nodiscard]] const ModuleIdList& visible_modules(syntax::ModuleId module) const;
    [[nodiscard]] bool module_alias_visible(std::string_view name) const;
    [[nodiscard]] bool visible_root_module_name_exists(std::string_view name) const;
    [[nodiscard]] std::vector<std::string_view> type_scope_parts(const syntax::TypeNode& type) const;
    [[nodiscard]] syntax::ModuleId find_visible_module_path(const std::vector<std::string_view>& parts) const;
    [[nodiscard]] bool visible_module_path_prefix_exists(const std::vector<std::string_view>& parts) const;
    [[nodiscard]] syntax::ModuleId resolve_type_scope(const syntax::TypeNode& type, bool report_unknown);
    [[nodiscard]] const ModuleIdList& module_export_modules(syntax::ModuleId module) const;
    [[nodiscard]] ModuleIdList accessible_module_export_modules(syntax::ModuleId module) const;
    [[nodiscard]] SelectiveReexportTargetList accessible_selective_reexports(
        syntax::ModuleId module, IdentId name_id, std::string_view name) const;
    void append_public_reexports(
        syntax::ModuleId module, ModuleIdList& result, std::unordered_set<base::u32>& seen) const;
    [[nodiscard]] std::string module_name(syntax::ModuleId module) const;
    [[nodiscard]] const FunctionSignature* find_method_in_visible_modules(TypeHandle owner_type, IdentId name_id,
        std::string_view name, const base::SourceRange& range, bool require_self, bool report_unknown);
    [[nodiscard]] TypeHandle find_type_in_visible_modules(IdentId name_id, std::string_view name,
        const base::SourceRange& range, bool opaque_allowed_as_pointee, bool report_unknown);
    [[nodiscard]] TypeHandle find_type_in_module(syntax::ModuleId module, IdentId name_id, std::string_view name,
        const base::SourceRange& range, bool opaque_allowed_as_pointee, bool report_unknown);
    [[nodiscard]] const FunctionSignature* find_function_in_visible_modules(
        IdentId name_id, std::string_view name, const base::SourceRange& range, bool report_unknown);
    [[nodiscard]] const FunctionSignature* find_function_in_module(syntax::ModuleId module, IdentId name_id,
        std::string_view name, const base::SourceRange& range, bool report_unknown);
    [[nodiscard]] const Symbol* find_symbol_in_module(syntax::ModuleId module, IdentId name_id, std::string_view name,
        const base::SourceRange& range, bool report_unknown);
    [[nodiscard]] const EnumCaseInfo* find_enum_case_in_visible_modules(
        IdentId name_id, std::string_view name, const base::SourceRange& range, bool report_unknown);
    [[nodiscard]] const EnumCaseInfo* find_enum_case_by_scoped_name(IdentId enum_name_id, std::string_view enum_name,
        IdentId case_name_id, std::string_view case_name, const base::SourceRange& range, bool report_unknown);
    [[nodiscard]] const EnumCaseInfo* find_enum_constructor(syntax::ExprId callee, bool report_unknown);
    [[nodiscard]] const Symbol* find_symbol(IdentId name_id, std::string_view name, const base::SourceRange& range);

private:
    [[nodiscard]] SemanticAnalyzerCore& mutable_core() const noexcept;

    const SemanticAnalyzerCore& core_;
};

} // namespace aurex::sema
