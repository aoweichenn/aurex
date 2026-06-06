#include <aurex/infrastructure/query/cleanup_marker_facts.hpp>

#include <sstream>
#include <utility>

namespace aurex::query {
namespace {

constexpr std::string_view QUERY_CLEANUP_MARKER_FACTS_FINGERPRINT_MARKER =
    "query.cleanup_marker_facts.v1";
constexpr base::u32 QUERY_CLEANUP_MARKER_INVALID_POLICY_VALUE = 255U;

void increment_policy_count(CleanupMarkerSummary& summary, const CleanupMarkerPolicy policy) noexcept
{
    switch (policy) {
        case CleanupMarkerPolicy::none:
            ++summary.none_count;
            break;
        case CleanupMarkerPolicy::structural_static:
            ++summary.structural_static_count;
            break;
        case CleanupMarkerPolicy::generic_marker_only:
            ++summary.generic_marker_only_count;
            break;
        case CleanupMarkerPolicy::associated_projection_marker_only:
            ++summary.associated_projection_marker_only_count;
            break;
        case CleanupMarkerPolicy::opaque_marker_only:
            ++summary.opaque_marker_only_count;
            break;
        case CleanupMarkerPolicy::unknown_marker_only:
            ++summary.unknown_marker_only_count;
            break;
        case CleanupMarkerPolicy::static_custom_destructor:
            ++summary.static_custom_destructor_count;
            break;
    }
}

[[nodiscard]] base::u8 stable_policy_value(const CleanupMarkerPolicy policy) noexcept
{
    return is_valid(policy) ? static_cast<base::u8>(policy)
                            : static_cast<base::u8>(QUERY_CLEANUP_MARKER_INVALID_POLICY_VALUE);
}

} // namespace

std::string_view cleanup_marker_kind_name(const CleanupMarkerKind kind) noexcept
{
    switch (kind) {
        case CleanupMarkerKind::drop:
            return "drop";
        case CleanupMarkerKind::drop_if:
            return "drop_if";
    }
    return "invalid";
}

std::string_view cleanup_marker_policy_name(const CleanupMarkerPolicy policy) noexcept
{
    switch (policy) {
        case CleanupMarkerPolicy::none:
            return "none";
        case CleanupMarkerPolicy::structural_static:
            return "structural_static";
        case CleanupMarkerPolicy::generic_marker_only:
            return "generic_marker_only";
        case CleanupMarkerPolicy::associated_projection_marker_only:
            return "associated_projection_marker_only";
        case CleanupMarkerPolicy::opaque_marker_only:
            return "opaque_marker_only";
        case CleanupMarkerPolicy::unknown_marker_only:
            return "unknown_marker_only";
        case CleanupMarkerPolicy::static_custom_destructor:
            return "static_custom_destructor";
    }
    return "invalid";
}

bool is_valid(const CleanupMarkerPolicy policy) noexcept
{
    switch (policy) {
        case CleanupMarkerPolicy::none:
        case CleanupMarkerPolicy::structural_static:
        case CleanupMarkerPolicy::generic_marker_only:
        case CleanupMarkerPolicy::associated_projection_marker_only:
        case CleanupMarkerPolicy::opaque_marker_only:
        case CleanupMarkerPolicy::unknown_marker_only:
        case CleanupMarkerPolicy::static_custom_destructor:
            return true;
    }
    return false;
}

void record_cleanup_marker_fact(FunctionCleanupMarkerFacts& facts, CleanupMarkerFact fact)
{
    switch (fact.kind) {
        case CleanupMarkerKind::drop:
            ++facts.summary.drop_count;
            break;
        case CleanupMarkerKind::drop_if:
            ++facts.summary.drop_if_count;
            break;
    }
    increment_policy_count(facts.summary, fact.policy);
    facts.markers.push_back(std::move(fact));
}

StableFingerprint128 function_cleanup_marker_facts_fingerprint(
    const FunctionCleanupMarkerFacts& facts) noexcept
{
    StableHashBuilder builder;
    builder.mix_string(QUERY_CLEANUP_MARKER_FACTS_FINGERPRINT_MARKER);
    builder.mix_string(facts.symbol);
    builder.mix_u64(facts.markers.size());
    builder.mix_u64(facts.summary.drop_count);
    builder.mix_u64(facts.summary.drop_if_count);
    builder.mix_u64(facts.summary.none_count);
    builder.mix_u64(facts.summary.structural_static_count);
    builder.mix_u64(facts.summary.generic_marker_only_count);
    builder.mix_u64(facts.summary.associated_projection_marker_only_count);
    builder.mix_u64(facts.summary.opaque_marker_only_count);
    builder.mix_u64(facts.summary.unknown_marker_only_count);
    builder.mix_u64(facts.summary.static_custom_destructor_count);
    for (const CleanupMarkerFact& marker : facts.markers) {
        builder.mix_u8(static_cast<base::u8>(marker.kind));
        builder.mix_u8(stable_policy_value(marker.policy));
        builder.mix_u32(marker.value_id);
        builder.mix_u32(marker.object_value_id);
        builder.mix_u32(marker.condition_value_id);
        builder.mix_u32(marker.target_type_id);
        builder.mix_string(marker.target_type);
    }
    return builder.finish();
}

std::string summarize_function_cleanup_marker_facts(const FunctionCleanupMarkerFacts& facts)
{
    std::ostringstream label;
    label << "cleanup_marker_facts markers=" << facts.markers.size()
          << " drop=" << facts.summary.drop_count
          << " drop_if=" << facts.summary.drop_if_count
          << " structural_static=" << facts.summary.structural_static_count
          << " generic_marker_only=" << facts.summary.generic_marker_only_count
          << " associated_projection_marker_only=" << facts.summary.associated_projection_marker_only_count
          << " opaque_marker_only=" << facts.summary.opaque_marker_only_count
          << " unknown_marker_only=" << facts.summary.unknown_marker_only_count
          << " static_custom_destructor=" << facts.summary.static_custom_destructor_count;
    if (!facts.markers.empty()) {
        label << " first_policy=" << cleanup_marker_policy_name(facts.markers.front().policy);
    }
    label << " fingerprint=" << debug_string(function_cleanup_marker_facts_fingerprint(facts));
    return label.str();
}

std::string dump_function_cleanup_marker_facts(const FunctionCleanupMarkerFacts& facts)
{
    std::ostringstream stream;
    stream << "cleanup_marker_facts function=" << (facts.symbol.empty() ? "<anonymous>" : facts.symbol)
           << " markers=" << facts.markers.size()
           << " fingerprint=" << debug_string(function_cleanup_marker_facts_fingerprint(facts)) << '\n';
    for (base::usize index = 0; index < facts.markers.size(); ++index) {
        const CleanupMarkerFact& marker = facts.markers[index];
        stream << "  c" << index << ' ' << cleanup_marker_kind_name(marker.kind)
               << " value=v" << marker.value_id
               << " object=v" << marker.object_value_id
               << " condition=v" << marker.condition_value_id
               << " target_type=" << marker.target_type_id;
        if (!marker.target_type.empty()) {
            stream << '/' << marker.target_type;
        }
        stream << " policy=" << cleanup_marker_policy_name(marker.policy) << '\n';
    }
    return stream.str();
}

} // namespace aurex::query
