#pragma once

#include <aurex/base/source.hpp>
#include <aurex/project/project_model.hpp>
#include <aurex/sema/checked_module.hpp>
#include <aurex/syntax/ast.hpp>

#include <filesystem>
#include <optional>
#include <span>
#include <string>

#include "types.hpp"

namespace aurex::driver::incremental_cache_detail {

[[nodiscard]] std::optional<SourceStageQueryRecords> source_stage_query_records_for_file(
    const std::filesystem::path& path, query::PackageKey package);
[[nodiscard]] std::vector<DefinitionRecord> collect_definitions(const sema::CheckedModule& checked);
[[nodiscard]] QuerySubjectCollection collect_query_subjects(std::span<const ModuleRecord> modules,
    const sema::CheckedModule& checked, const base::SourceManager& sources, const syntax::AstModule* ast,
    const project::ProjectModel& project_model, bool include_lowering_subjects);
void evaluate_query_subject(
    query::QueryContext& context, const QuerySubjectCollection& collection, const QuerySubject& subject);

} // namespace aurex::driver::incremental_cache_detail
