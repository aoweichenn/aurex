#pragma once

#include <aurex/base/source.hpp>
#include <aurex/syntax/ast.hpp>

#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <sema/internal/sema_core.hpp>

namespace aurex::sema {

class ModuleVisibilityResolver final {
public:
    explicit ModuleVisibilityResolver(const SemanticAnalyzerCore& core) noexcept;

    [[nodiscard]] syntax::ModuleId resolve_import_alias(
        std::string_view alias, const base::SourceRange& range, bool report_unknown) const;
    [[nodiscard]] const SemanticAnalyzerCore::ModuleIdList& visible_modules(syntax::ModuleId module) const;
    [[nodiscard]] bool module_alias_visible(std::string_view name) const;
    [[nodiscard]] bool visible_root_module_name_exists(std::string_view name) const;
    [[nodiscard]] syntax::ModuleId find_visible_module_path(const std::vector<std::string_view>& parts) const;
    [[nodiscard]] bool visible_module_path_prefix_exists(const std::vector<std::string_view>& parts) const;
    [[nodiscard]] const SemanticAnalyzerCore::ModuleIdList& module_export_modules(syntax::ModuleId module) const;
    [[nodiscard]] SemanticAnalyzerCore::ModuleIdList accessible_module_export_modules(syntax::ModuleId module) const;
    void append_public_reexports(
        syntax::ModuleId module, SemanticAnalyzerCore::ModuleIdList& result, std::unordered_set<base::u32>& seen) const;
    [[nodiscard]] std::string module_name(syntax::ModuleId module) const;

private:
    [[nodiscard]] bool reexport_visible_to_module(
        syntax::ModuleId exporter, syntax::Visibility visibility, syntax::ModuleId access_module) const noexcept;
    [[nodiscard]] SemanticAnalyzerCore::ModuleIdList make_module_id_list() const;
    void append_reexports_for_access(syntax::ModuleId module, syntax::ModuleId access_module,
        SemanticAnalyzerCore::ModuleIdList& result, std::unordered_set<base::u32>& seen) const;
    void report_lookup(const base::SourceRange& range, std::string message) const;
    void report_lookup_suggestion(const base::SourceRange& range, std::string_view suggestion) const;
    [[nodiscard]] std::string_view nearest_import_alias_name(std::string_view name) const;

    const SemanticAnalyzerCore& core_;
};

} // namespace aurex::sema
