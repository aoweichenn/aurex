#pragma once

#include <aurex/infrastructure/base/integer.hpp>
#include <aurex/infrastructure/query/stable_hash.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace aurex::query {

enum class CleanupMarkerKind : base::u8 {
    drop,
    drop_if,
};

enum class CleanupMarkerPolicy : base::u8 {
    none,
    structural_static,
    generic_marker_only,
    associated_projection_marker_only,
    opaque_marker_only,
    unknown_marker_only,
    static_custom_destructor,
    dynamic_erased_drop_blocked,
};

struct CleanupMarkerFact {
    CleanupMarkerKind kind = CleanupMarkerKind::drop;
    CleanupMarkerPolicy policy = CleanupMarkerPolicy::none;
    base::u32 value_id = 0;
    base::u32 object_value_id = 0;
    base::u32 condition_value_id = 0;
    base::u32 target_type_id = 0;
    std::string target_type;
};

struct CleanupMarkerSummary {
    base::u64 drop_count = 0;
    base::u64 drop_if_count = 0;
    base::u64 none_count = 0;
    base::u64 structural_static_count = 0;
    base::u64 generic_marker_only_count = 0;
    base::u64 associated_projection_marker_only_count = 0;
    base::u64 opaque_marker_only_count = 0;
    base::u64 unknown_marker_only_count = 0;
    base::u64 static_custom_destructor_count = 0;
    base::u64 dynamic_erased_drop_blocked_count = 0;
};

struct FunctionCleanupMarkerFacts {
    std::string symbol;
    std::vector<CleanupMarkerFact> markers;
    CleanupMarkerSummary summary;
    StableFingerprint128 fingerprint;
};

[[nodiscard]] std::string_view cleanup_marker_kind_name(CleanupMarkerKind kind) noexcept;
[[nodiscard]] std::string_view cleanup_marker_policy_name(CleanupMarkerPolicy policy) noexcept;
[[nodiscard]] bool is_valid(CleanupMarkerPolicy policy) noexcept;
void record_cleanup_marker_fact(FunctionCleanupMarkerFacts& facts, CleanupMarkerFact fact);
[[nodiscard]] StableFingerprint128 function_cleanup_marker_facts_fingerprint(
    const FunctionCleanupMarkerFacts& facts) noexcept;
[[nodiscard]] std::string summarize_function_cleanup_marker_facts(const FunctionCleanupMarkerFacts& facts);
[[nodiscard]] std::string dump_function_cleanup_marker_facts(const FunctionCleanupMarkerFacts& facts);

} // namespace aurex::query
