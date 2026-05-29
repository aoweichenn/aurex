#include "schedule.hpp"

#include <algorithm>
#include <tuple>

namespace aurex::driver::incremental_cache_detail {

[[nodiscard]] base::usize query_kind_schedule_rank(const query::QueryKind kind) noexcept
{
    switch (kind) {
        case query::QueryKind::project_graph:
            return INCREMENTAL_CACHE_QUERY_SCHEDULE_PROJECT_GRAPH_RANK;
        case query::QueryKind::file_content:
            return INCREMENTAL_CACHE_QUERY_SCHEDULE_FILE_CONTENT_RANK;
        case query::QueryKind::lex_file:
            return INCREMENTAL_CACHE_QUERY_SCHEDULE_LEX_FILE_RANK;
        case query::QueryKind::parse_file:
            return INCREMENTAL_CACHE_QUERY_SCHEDULE_PARSE_FILE_RANK;
        case query::QueryKind::module_part:
            return INCREMENTAL_CACHE_QUERY_SCHEDULE_MODULE_PART_RANK;
        case query::QueryKind::module_graph:
            return INCREMENTAL_CACHE_QUERY_SCHEDULE_MODULE_GRAPH_RANK;
        case query::QueryKind::item_list:
            return INCREMENTAL_CACHE_QUERY_SCHEDULE_ITEM_LIST_RANK;
        case query::QueryKind::module_exports:
            return INCREMENTAL_CACHE_QUERY_SCHEDULE_MODULE_EXPORTS_RANK;
        case query::QueryKind::module_package_exports:
            return INCREMENTAL_CACHE_QUERY_SCHEDULE_MODULE_EXPORTS_RANK;
        case query::QueryKind::item_signature:
            return INCREMENTAL_CACHE_QUERY_SCHEDULE_ITEM_SIGNATURE_RANK;
        case query::QueryKind::generic_template_signature:
            return INCREMENTAL_CACHE_QUERY_SCHEDULE_GENERIC_TEMPLATE_SIGNATURE_RANK;
        case query::QueryKind::generic_instance_signature:
            return INCREMENTAL_CACHE_QUERY_SCHEDULE_GENERIC_INSTANCE_SIGNATURE_RANK;
        case query::QueryKind::function_body_syntax:
            return INCREMENTAL_CACHE_QUERY_SCHEDULE_FUNCTION_BODY_SYNTAX_RANK;
        case query::QueryKind::type_check_body:
            return INCREMENTAL_CACHE_QUERY_SCHEDULE_TYPE_CHECK_BODY_RANK;
        case query::QueryKind::generic_instance_body:
            return INCREMENTAL_CACHE_QUERY_SCHEDULE_GENERIC_INSTANCE_BODY_RANK;
        case query::QueryKind::lower_function_ir:
            return INCREMENTAL_CACHE_QUERY_SCHEDULE_LOWER_FUNCTION_IR_RANK;
        case query::QueryKind::diagnostics:
            return INCREMENTAL_CACHE_QUERY_SCHEDULE_DIAGNOSTICS_RANK;
        case query::QueryKind::invalid:
            return INCREMENTAL_CACHE_QUERY_SCHEDULE_INVALID_RANK;
    }
}

[[nodiscard]] bool query_dependency_edge_schedule_is_valid(const query::QueryDependencyEdge edge) noexcept
{
    if (edge.dependent == edge.dependency) {
        return false;
    }

    const base::usize dependent_rank = query_kind_schedule_rank(edge.dependent.kind);
    const base::usize dependency_rank = query_kind_schedule_rank(edge.dependency.kind);
    if (dependent_rank == INCREMENTAL_CACHE_QUERY_SCHEDULE_INVALID_RANK
        || dependency_rank == INCREMENTAL_CACHE_QUERY_SCHEDULE_INVALID_RANK) {
        return false;
    }
    return dependency_rank <= dependent_rank;
}

[[nodiscard]] bool query_key_less(const query::QueryKey lhs, const query::QueryKey rhs) noexcept
{
    return std::tie(
               lhs.kind, lhs.schema, lhs.global_id, lhs.payload.primary, lhs.payload.secondary, lhs.payload.byte_count)
        < std::tie(
            rhs.kind, rhs.schema, rhs.global_id, rhs.payload.primary, rhs.payload.secondary, rhs.payload.byte_count);
}

[[nodiscard]] bool query_record_key_less(const query::QueryRecord& lhs, const query::QueryRecord& rhs) noexcept
{
    return std::tie(lhs.key.kind, lhs.key.global_id, lhs.result.global_id, lhs.stable_key_bytes)
        < std::tie(rhs.key.kind, rhs.key.global_id, rhs.result.global_id, rhs.stable_key_bytes);
}

[[nodiscard]] bool query_subject_schedule_less(const QuerySubject& lhs, const QuerySubject& rhs) noexcept
{
    const base::usize lhs_rank = query_kind_schedule_rank(lhs.record.key.kind);
    const base::usize rhs_rank = query_kind_schedule_rank(rhs.record.key.kind);
    if (lhs_rank != rhs_rank) {
        return lhs_rank < rhs_rank;
    }
    return query_record_key_less(lhs.record, rhs.record);
}

[[nodiscard]] bool contains_query_key(const std::vector<query::QueryKey>& keys, const query::QueryKey key) noexcept
{
    return std::binary_search(keys.begin(), keys.end(), key, query_key_less);
}

} // namespace aurex::driver::incremental_cache_detail
