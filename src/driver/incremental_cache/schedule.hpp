#pragma once

#include <vector>

#include "types.hpp"

namespace aurex::driver::incremental_cache_detail {

[[nodiscard]] bool query_dependency_edge_schedule_is_valid(query::QueryDependencyEdge edge) noexcept;
[[nodiscard]] bool contains_query_key(const std::vector<query::QueryKey>& keys, query::QueryKey key) noexcept;
[[nodiscard]] bool query_subject_schedule_less(const QuerySubject& lhs, const QuerySubject& rhs) noexcept;

} // namespace aurex::driver::incremental_cache_detail
