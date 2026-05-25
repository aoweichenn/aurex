#include <aurex/sema/sema_messages.hpp>
#include <aurex/syntax/module.hpp>

#include <algorithm>
#include <limits>
#include <span>
#include <vector>

#include <sema/internal/name_resolution.hpp>

namespace aurex::sema {

namespace {

constexpr std::string_view SEMA_LOOKUP_UNKNOWN_MODULE_NAME = "<unknown>";
constexpr base::usize SEMA_LOOKUP_SHORT_SUGGESTION_NAME_LENGTH = 4;
constexpr base::usize SEMA_LOOKUP_MEDIUM_SUGGESTION_NAME_LENGTH = 8;
constexpr base::usize SEMA_LOOKUP_SHORT_SUGGESTION_MAX_DISTANCE = 1;
constexpr base::usize SEMA_LOOKUP_MEDIUM_SUGGESTION_MAX_DISTANCE = 2;
constexpr base::usize SEMA_LOOKUP_LONG_SUGGESTION_MAX_DISTANCE = 3;

struct NameSuggestion {
    std::string_view name;
    base::usize distance = std::numeric_limits<base::usize>::max();
};

[[nodiscard]] base::usize suggestion_distance_limit(const std::string_view name) noexcept
{
    if (name.size() < SEMA_LOOKUP_SHORT_SUGGESTION_NAME_LENGTH) {
        return SEMA_LOOKUP_SHORT_SUGGESTION_MAX_DISTANCE;
    }
    if (name.size() < SEMA_LOOKUP_MEDIUM_SUGGESTION_NAME_LENGTH) {
        return SEMA_LOOKUP_MEDIUM_SUGGESTION_MAX_DISTANCE;
    }
    return SEMA_LOOKUP_LONG_SUGGESTION_MAX_DISTANCE;
}

[[nodiscard]] base::usize bounded_edit_distance(
    const std::string_view lhs, const std::string_view rhs, const base::usize limit)
{
    const base::usize lhs_size = lhs.size();
    const base::usize rhs_size = rhs.size();
    const base::usize size_delta = lhs_size > rhs_size ? lhs_size - rhs_size : rhs_size - lhs_size;
    if (size_delta > limit) {
        return limit + 1;
    }

    std::vector<base::usize> previous(rhs_size + 1);
    std::vector<base::usize> current(rhs_size + 1);
    for (base::usize column = 0; column <= rhs_size; ++column) {
        previous[column] = column;
    }

    for (base::usize row = 1; row <= lhs_size; ++row) {
        current[0] = row;
        base::usize row_minimum = current[0];
        for (base::usize column = 1; column <= rhs_size; ++column) {
            const base::usize substitution_cost = lhs[row - 1] == rhs[column - 1] ? 0 : 1;
            current[column] = std::min({
                previous[column] + 1,
                current[column - 1] + 1,
                previous[column - 1] + substitution_cost,
            });
            row_minimum = std::min(row_minimum, current[column]);
        }
        if (row_minimum > limit) {
            return limit + 1;
        }
        previous.swap(current);
    }
    return previous[rhs_size];
}

void consider_suggestion(NameSuggestion& best, const std::string_view requested, const std::string_view candidate)
{
    if (candidate.empty()) {
        return;
    }
    const base::usize limit = suggestion_distance_limit(requested);
    const base::usize distance = bounded_edit_distance(requested, candidate, limit);
    if (distance > limit) {
        return;
    }
    if (distance < best.distance || (distance == best.distance && (best.name.empty() || candidate < best.name))) {
        best = NameSuggestion{candidate, distance};
    }
}

[[nodiscard]] bool module_path_matches_parts(
    const syntax::ModulePath& path, const std::vector<std::string_view>& parts) noexcept
{
    if (path.parts.size() != parts.size()) {
        return false;
    }
    for (base::usize i = 0; i < parts.size(); ++i) {
        if (path.parts[i] != parts[i]) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool module_path_matches_prefix(
    const syntax::ModulePath& path, const std::vector<std::string_view>& parts, const base::usize prefix_size) noexcept
{
    if (path.parts.size() != prefix_size || parts.size() < prefix_size) {
        return false;
    }
    for (base::usize i = 0; i < prefix_size; ++i) {
        if (path.parts[i] != parts[i]) {
            return false;
        }
    }
    return true;
}

} // namespace

ModuleVisibilityResolver::ModuleVisibilityResolver(const SemanticAnalyzerCore& core) noexcept : core_(core)
{
}

syntax::ModuleId ModuleVisibilityResolver::resolve_import_alias(
    const std::string_view alias, const base::SourceRange& range, const bool report_unknown) const
{
    if (!syntax::is_valid(this->core_.state_.flow.current_module)
        || this->core_.state_.flow.current_module.value >= this->core_.ctx_.module.modules.size()) {
        if (report_unknown) {
            this->report_lookup(range, sema_unknown_import_alias_message(alias));
        }
        return syntax::INVALID_MODULE_ID;
    }
    syntax::ModuleId resolved = syntax::INVALID_MODULE_ID;
    for (const syntax::ResolvedImport& import : this->core_.imports_for_scope(this->core_.state_.flow.current_module)) {
        if (import.alias != alias) {
            continue;
        }
        if (syntax::is_valid(resolved)) {
            if (report_unknown) {
                this->report_lookup(range, sema_ambiguous_import_alias_message(alias));
            }
            return syntax::INVALID_MODULE_ID;
        }
        resolved = import.module;
    }
    if (!syntax::is_valid(resolved) && report_unknown) {
        this->report_lookup(range, sema_unknown_import_alias_message(alias));
        this->report_lookup_suggestion(range, this->nearest_import_alias_name(alias));
        if (this->core_.import_alias_exists_outside_current_scope(alias)) {
            this->core_.report_help(
                range, SemanticDiagnosticKind::lookup, std::string(SEMA_IMPORTS_ARE_PART_LOCAL_HELP));
        }
    }
    return resolved;
}

const SemanticAnalyzerCore::ModuleIdList& ModuleVisibilityResolver::visible_modules(const syntax::ModuleId module) const
{
    static const SemanticAnalyzerCore::ModuleIdList empty;
    if (!syntax::is_valid(module)) {
        return empty;
    }
    const bool item_scoped = this->core_.uses_item_import_scope(module);
    if (item_scoped) {
        if (const auto found =
                this->core_.state_.modules.item_visible_modules_cache.find(this->core_.state_.flow.current_item.value);
            found != this->core_.state_.modules.item_visible_modules_cache.end()) {
            return found->second;
        }
    } else if (const auto found = this->core_.state_.modules.visible_modules_cache.find(module.value);
        found != this->core_.state_.modules.visible_modules_cache.end()) {
        return found->second;
    }
    if (module.value >= this->core_.ctx_.module.modules.size()) {
        SemanticAnalyzerCore::ModuleIdList result = this->make_module_id_list();
        result.reserve(1);
        result.push_back(module);
        const auto inserted = this->core_.state_.modules.visible_modules_cache.emplace(module.value, std::move(result));
        return inserted.first->second;
    }
    const std::span<const syntax::ResolvedImport> imports = this->core_.imports_for_scope(module);
    const base::usize import_count = imports.size();
    SemanticAnalyzerCore::ModuleIdList result = this->make_module_id_list();
    result.reserve(import_count + 1);
    result.push_back(module);
    std::unordered_set<base::u32> seen;
    seen.reserve(import_count + 1);
    seen.insert(module.value);
    for (const syntax::ResolvedImport& import : imports) {
        if (!syntax::is_valid(import.module)) {
            continue;
        }
        if (seen.insert(import.module.value).second) {
            result.push_back(import.module);
        }
        this->append_public_reexports(import.module, result, seen);
    }
    const auto inserted = item_scoped
        ? this->core_.state_.modules.item_visible_modules_cache.emplace(
              this->core_.state_.flow.current_item.value, std::move(result))
        : this->core_.state_.modules.visible_modules_cache.emplace(module.value, std::move(result));
    return inserted.first->second;
}

bool ModuleVisibilityResolver::module_alias_visible(const std::string_view name) const
{
    if (!syntax::is_valid(this->core_.state_.flow.current_module)
        || this->core_.state_.flow.current_module.value >= this->core_.ctx_.module.modules.size()) {
        return false;
    }
    for (const syntax::ResolvedImport& import : this->core_.imports_for_scope(this->core_.state_.flow.current_module)) {
        if (import.alias == name) {
            return true;
        }
    }
    return false;
}

bool ModuleVisibilityResolver::visible_root_module_name_exists(const std::string_view name) const
{
    if (name.empty()) {
        return false;
    }
    for (const syntax::ModuleId module : this->visible_modules(this->core_.state_.flow.current_module)) {
        if (!syntax::is_valid(module) || module.value >= this->core_.ctx_.module.modules.size()) {
            continue;
        }
        const syntax::ModulePath& path = this->core_.ctx_.module.modules[module.value].path;
        if (!path.parts.empty() && path.parts.front() == name) {
            return true;
        }
    }
    return false;
}

syntax::ModuleId ModuleVisibilityResolver::find_visible_module_path(const std::vector<std::string_view>& parts) const
{
    if (parts.empty()) {
        return syntax::INVALID_MODULE_ID;
    }
    for (const syntax::ModuleId module : this->visible_modules(this->core_.state_.flow.current_module)) {
        if (!syntax::is_valid(module) || module.value >= this->core_.ctx_.module.modules.size()) {
            continue;
        }
        if (module_path_matches_parts(this->core_.ctx_.module.modules[module.value].path, parts)) {
            return module;
        }
    }
    return syntax::INVALID_MODULE_ID;
}

bool ModuleVisibilityResolver::visible_module_path_prefix_exists(const std::vector<std::string_view>& parts) const
{
    if (parts.size() < 2) {
        return false;
    }
    for (base::usize prefix_size = 1; prefix_size < parts.size(); ++prefix_size) {
        for (const syntax::ModuleId module : this->visible_modules(this->core_.state_.flow.current_module)) {
            if (!syntax::is_valid(module) || module.value >= this->core_.ctx_.module.modules.size()) {
                continue;
            }
            if (module_path_matches_prefix(this->core_.ctx_.module.modules[module.value].path, parts, prefix_size)) {
                return true;
            }
        }
    }
    return false;
}

const SemanticAnalyzerCore::ModuleIdList& ModuleVisibilityResolver::module_export_modules(
    const syntax::ModuleId module) const
{
    static const SemanticAnalyzerCore::ModuleIdList empty;
    if (!syntax::is_valid(module)) {
        return empty;
    }
    if (const auto found = this->core_.state_.modules.export_modules_cache.find(module.value);
        found != this->core_.state_.modules.export_modules_cache.end()) {
        return found->second;
    }
    if (module.value >= this->core_.ctx_.module.modules.size()) {
        SemanticAnalyzerCore::ModuleIdList result = this->make_module_id_list();
        result.reserve(1);
        result.push_back(module);
        const auto inserted = this->core_.state_.modules.export_modules_cache.emplace(module.value, std::move(result));
        return inserted.first->second;
    }
    const base::usize import_count = this->core_.ctx_.module.modules[module.value].imports.size();
    SemanticAnalyzerCore::ModuleIdList result = this->make_module_id_list();
    result.reserve(import_count + 1);
    result.push_back(module);
    std::unordered_set<base::u32> seen;
    seen.reserve(import_count + 1);
    seen.insert(module.value);
    this->append_public_reexports(module, result, seen);
    const auto inserted = this->core_.state_.modules.export_modules_cache.emplace(module.value, std::move(result));
    return inserted.first->second;
}

void ModuleVisibilityResolver::append_public_reexports(const syntax::ModuleId module,
    SemanticAnalyzerCore::ModuleIdList& result, std::unordered_set<base::u32>& seen) const
{
    SemanticAnalyzerCore::ModuleIdList pending = this->make_module_id_list();
    pending.reserve(result.size());
    pending.push_back(module);
    while (!pending.empty()) {
        const syntax::ModuleId current = pending.back();
        pending.pop_back();
        if (!syntax::is_valid(current) || current.value >= this->core_.ctx_.module.modules.size()) {
            continue;
        }
        for (const syntax::ResolvedImport& import : this->core_.ctx_.module.modules[current.value].imports) {
            if (!syntax::visibility_is_public(import.visibility) || !syntax::is_valid(import.module)) {
                continue;
            }
            if (seen.insert(import.module.value).second) {
                result.push_back(import.module);
                pending.push_back(import.module);
            }
        }
    }
}

std::string ModuleVisibilityResolver::module_name(const syntax::ModuleId module) const
{
    if (!syntax::is_valid(module) || module.value >= this->core_.ctx_.module.modules.size()) {
        return std::string(SEMA_LOOKUP_UNKNOWN_MODULE_NAME);
    }
    return syntax::module_path_to_string(this->core_.ctx_.module.modules[module.value].path);
}

SemanticAnalyzerCore::ModuleIdList ModuleVisibilityResolver::make_module_id_list() const
{
    return this->core_.make_module_id_list();
}

void ModuleVisibilityResolver::report_lookup(const base::SourceRange& range, std::string message) const
{
    this->core_.report_lookup(range, std::move(message));
}

void ModuleVisibilityResolver::report_lookup_suggestion(
    const base::SourceRange& range, const std::string_view suggestion) const
{
    this->core_.report_lookup_suggestion(range, suggestion);
}

std::string_view ModuleVisibilityResolver::nearest_import_alias_name(const std::string_view name) const
{
    NameSuggestion best;
    for (const syntax::ResolvedImport& import : this->core_.imports_for_scope(this->core_.state_.flow.current_module)) {
        consider_suggestion(best, name, import.alias);
    }
    return best.name;
}

} // namespace aurex::sema
