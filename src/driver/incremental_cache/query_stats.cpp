#include "query_stats.hpp"

namespace aurex::driver::incremental_cache_detail {

void increment_query_kind_count(QueryKindExecutionCounts& counts, const QuerySubjectKind kind) noexcept
{
    counts.total += 1;
    switch (kind) {
        case QuerySubjectKind::file_content:
            counts.file_contents += 1;
            return;
        case QuerySubjectKind::lex_file:
            counts.lex_files += 1;
            return;
        case QuerySubjectKind::parse_file:
            counts.parse_files += 1;
            return;
        case QuerySubjectKind::module_graph:
            counts.module_graphs += 1;
            return;
        case QuerySubjectKind::module_exports:
        case QuerySubjectKind::module_package_exports:
            counts.module_exports += 1;
            return;
        case QuerySubjectKind::item_list:
            counts.item_lists += 1;
            return;
        case QuerySubjectKind::item_signature:
            counts.item_signatures += 1;
            return;
        case QuerySubjectKind::function_body_syntax:
            counts.function_body_syntaxes += 1;
            return;
        case QuerySubjectKind::type_check_body:
            counts.type_check_bodies += 1;
            return;
        case QuerySubjectKind::generic_template_signature:
            counts.generic_template_signatures += 1;
            return;
        case QuerySubjectKind::generic_instance_signature:
            counts.generic_instance_signatures += 1;
            return;
        case QuerySubjectKind::generic_instance_body:
            counts.generic_instance_bodies += 1;
            return;
        case QuerySubjectKind::lower_function_ir:
            counts.lower_function_irs += 1;
            return;
        case QuerySubjectKind::diagnostics:
            counts.diagnostics += 1;
            return;
    }
}

[[nodiscard]] base::usize total_query_execution_count(const QueryKindExecutionCounts& counts) noexcept
{
    return counts.total;
}

void increment_query_kind_count(QueryKindExecutionCounts& counts, const query::QueryKind kind) noexcept
{
    counts.total += 1;
    switch (kind) {
        case query::QueryKind::file_content:
            counts.file_contents += 1;
            return;
        case query::QueryKind::lex_file:
            counts.lex_files += 1;
            return;
        case query::QueryKind::parse_file:
            counts.parse_files += 1;
            return;
        case query::QueryKind::module_graph:
            counts.module_graphs += 1;
            return;
        case query::QueryKind::module_exports:
        case query::QueryKind::module_package_exports:
            counts.module_exports += 1;
            return;
        case query::QueryKind::item_list:
            counts.item_lists += 1;
            return;
        case query::QueryKind::item_signature:
            counts.item_signatures += 1;
            return;
        case query::QueryKind::function_body_syntax:
            counts.function_body_syntaxes += 1;
            return;
        case query::QueryKind::type_check_body:
            counts.type_check_bodies += 1;
            return;
        case query::QueryKind::generic_template_signature:
            counts.generic_template_signatures += 1;
            return;
        case query::QueryKind::generic_instance_signature:
            counts.generic_instance_signatures += 1;
            return;
        case query::QueryKind::generic_instance_body:
            counts.generic_instance_bodies += 1;
            return;
        case query::QueryKind::lower_function_ir:
            counts.lower_function_irs += 1;
            return;
        case query::QueryKind::diagnostics:
            counts.diagnostics += 1;
            return;
        case query::QueryKind::invalid:
            return;
    }
}

[[nodiscard]] QueryKindExecutionCounts query_record_counts_by_kind(
    const std::span<const query::QueryRecord> records) noexcept
{
    QueryKindExecutionCounts counts;
    for (const query::QueryRecord& record : records) {
        increment_query_kind_count(counts, record.key.kind);
    }
    return counts;
}

} // namespace aurex::driver::incremental_cache_detail
