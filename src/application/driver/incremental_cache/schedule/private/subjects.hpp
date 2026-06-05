#pragma once

#include <aurex/frontend/sema/checked_module.hpp>
#include <aurex/frontend/syntax/core/ast.hpp>
#include <aurex/infrastructure/base/source.hpp>
#include <aurex/infrastructure/project/project_model.hpp>
#include <aurex/infrastructure/query/source_file_query.hpp>
#include <aurex/midend/ir/ir.hpp>

#include <filesystem>
#include <optional>
#include <span>
#include <string>

#include <application/driver/incremental_cache/core/private/types.hpp>

namespace aurex::driver::incremental_cache_detail {

[[nodiscard]] std::optional<SourceStageQueryRecords> source_stage_query_records_for_file(
    const std::filesystem::path& path, query::PackageKey package,
    query::QuerySourceStageMode mode = query::QuerySourceStageMode::semantic);
[[nodiscard]] std::vector<DefinitionRecord> collect_definitions(const sema::CheckedModule& checked);
[[nodiscard]] QuerySubjectCollection collect_query_subjects(std::span<const ModuleRecord> modules,
    const sema::CheckedModule& checked, const base::SourceManager& sources, const syntax::AstModule* ast,
    const project::ProjectModel& project_model, const ir::Module* lowered_ir, bool include_lowering_subjects);
void evaluate_query_subject(
    query::QueryContext& context, const QuerySubjectCollection& collection, const QuerySubject& subject);

} // namespace aurex::driver::incremental_cache_detail
