#include <aurex/application/tooling/session.hpp>
#include <aurex/infrastructure/query/query_context.hpp>

#include <algorithm>
#include <tuple>

namespace aurex::tooling {
namespace {

constexpr std::string_view TOOLING_REUSE_STATUS_UNCHANGED = "unchanged";
constexpr std::string_view TOOLING_REUSE_STATUS_RECOMPUTED = "recomputed";
constexpr std::string_view TOOLING_REUSE_STATUS_INVALIDATED = "invalidated";
constexpr std::string_view TOOLING_REUSE_STATUS_MALFORMED = "malformed";
constexpr std::string_view TOOLING_REUSE_KIND_ITEM_SIGNATURE = "item_signature";
constexpr std::string_view TOOLING_REUSE_KIND_GENERIC_TEMPLATE_SIGNATURE = "generic_template_signature";
constexpr std::string_view TOOLING_REUSE_KIND_FUNCTION_BODY_SYNTAX = "function_body_syntax";
constexpr std::string_view TOOLING_REUSE_KIND_TYPE_CHECK_BODY = "type_check_body";
constexpr std::string_view TOOLING_REUSE_KIND_BORROW_SUMMARY = "borrow_summary";
constexpr std::string_view TOOLING_REUSE_KIND_BORROW_CONTRACT = "borrow_contract";
constexpr std::string_view TOOLING_REUSE_KIND_LIFETIME_FACTS = "lifetime_facts";
constexpr std::string_view TOOLING_REUSE_KIND_DROPCK_FACTS = "dropck_facts";
constexpr std::string_view TOOLING_REUSE_KIND_BODY_LOAN_CHECK = "body_loan_check";
constexpr std::string_view TOOLING_REUSE_REASON_BODY_LOCAL = "body-local edit";
constexpr std::string_view TOOLING_REUSE_REASON_SIGNATURE = "signature edit";

[[nodiscard]] bool tooling_query_key_less(const query::QueryKey lhs, const query::QueryKey rhs) noexcept
{
    return std::tie(
               lhs.kind, lhs.schema, lhs.global_id, lhs.payload.primary, lhs.payload.secondary, lhs.payload.byte_count)
        < std::tie(
            rhs.kind, rhs.schema, rhs.global_id, rhs.payload.primary, rhs.payload.secondary, rhs.payload.byte_count);
}

[[nodiscard]] bool tooling_dependency_edge_less(
    const query::QueryDependencyEdge lhs, const query::QueryDependencyEdge rhs) noexcept
{
    if (lhs.dependent == rhs.dependent) {
        return tooling_query_key_less(lhs.dependency, rhs.dependency);
    }
    return tooling_query_key_less(lhs.dependent, rhs.dependent);
}

[[nodiscard]] bool tooling_ranges_intersect(const base::SourceRange lhs, const base::SourceRange rhs) noexcept
{
    if (lhs.source.value != rhs.source.value) {
        return false;
    }
    return lhs.begin < rhs.end && rhs.begin < lhs.end;
}

[[nodiscard]] const base::SourceFile* tooling_source_file_for_range(
    const IdeSnapshot& snapshot, const base::SourceRange range) noexcept
{
    if (!range.well_formed()) {
        return nullptr;
    }
    return snapshot.sources.try_get(range.source);
}

[[nodiscard]] ToolingTextRange tooling_range_for_snapshot(const IdeSnapshot& snapshot, const base::SourceRange range)
{
    ToolingTextRange result;
    result.range = range;
    const base::SourceFile* const file = tooling_source_file_for_range(snapshot, range);
    if (file == nullptr) {
        return result;
    }
    result.path = std::string(file->path());
    result.start = file->line_column(range.begin);
    result.end = file->line_column(range.end);
    return result;
}

[[nodiscard]] std::string tooling_stable_key_or_empty(const query::QueryKey key)
{
    return query::is_valid(key) ? query::debug_string(key) : std::string{};
}

[[nodiscard]] std::string tooling_stable_key_or_empty(const query::DefKey key)
{
    return query::is_valid(key) ? query::debug_string(key) : std::string{};
}

[[nodiscard]] std::string tooling_stable_key_or_empty(const query::MemberKey key)
{
    return query::is_valid(key) ? query::debug_string(key) : std::string{};
}

[[nodiscard]] std::string tooling_stable_key_or_empty(const query::BodyKey key)
{
    return query::is_valid(key) ? query::debug_string(key) : std::string{};
}

[[nodiscard]] std::string tooling_stable_key_or_empty(const query::GenericInstanceKey& key)
{
    return query::is_valid(key) ? query::debug_string(key) : std::string{};
}

[[nodiscard]] std::string_view tooling_semantic_fact_kind_name(const IdeSemanticFactKind kind) noexcept
{
    switch (kind) {
        case IdeSemanticFactKind::item_signature:
            return TOOLING_REUSE_KIND_ITEM_SIGNATURE;
        case IdeSemanticFactKind::generic_template_signature:
            return TOOLING_REUSE_KIND_GENERIC_TEMPLATE_SIGNATURE;
        case IdeSemanticFactKind::function_body_syntax:
            return TOOLING_REUSE_KIND_FUNCTION_BODY_SYNTAX;
        case IdeSemanticFactKind::type_check_body:
            return TOOLING_REUSE_KIND_TYPE_CHECK_BODY;
        case IdeSemanticFactKind::borrow_summary:
            return TOOLING_REUSE_KIND_BORROW_SUMMARY;
        case IdeSemanticFactKind::borrow_contract:
            return TOOLING_REUSE_KIND_BORROW_CONTRACT;
        case IdeSemanticFactKind::lifetime_facts:
            return TOOLING_REUSE_KIND_LIFETIME_FACTS;
        case IdeSemanticFactKind::dropck_facts:
            return TOOLING_REUSE_KIND_DROPCK_FACTS;
        case IdeSemanticFactKind::body_loan_check:
            return TOOLING_REUSE_KIND_BODY_LOAN_CHECK;
    }
    return TOOLING_REUSE_KIND_ITEM_SIGNATURE;
}

[[nodiscard]] bool tooling_semantic_fact_is_body_local(const IdeSemanticFact& fact) noexcept
{
    return fact.kind == IdeSemanticFactKind::function_body_syntax || fact.kind == IdeSemanticFactKind::type_check_body
        || fact.kind == IdeSemanticFactKind::borrow_summary || fact.kind == IdeSemanticFactKind::lifetime_facts
        || fact.kind == IdeSemanticFactKind::dropck_facts || fact.kind == IdeSemanticFactKind::body_loan_check;
}

[[nodiscard]] std::string_view tooling_invalidation_reason(const IdeSemanticFact& fact) noexcept
{
    return tooling_semantic_fact_is_body_local(fact) ? TOOLING_REUSE_REASON_BODY_LOCAL : TOOLING_REUSE_REASON_SIGNATURE;
}

[[nodiscard]] std::vector<query::QueryKey> tooling_dependencies_for_record(
    const query::QueryRecord& record, const std::vector<query::QueryDependencyEdge>& edges)
{
    std::vector<query::QueryKey> dependencies;
    for (const query::QueryDependencyEdge& edge : edges) {
        if (edge.dependent == record.key) {
            dependencies.push_back(edge.dependency);
        }
    }
    return dependencies;
}

[[nodiscard]] query::QueryContext tooling_seed_query_context(const IdeQuerySnapshot& snapshot)
{
    query::QueryContext context;
    for (const query::QueryRecord& record : snapshot.records) {
        static_cast<void>(
            context.seed_completed_record(record, tooling_dependencies_for_record(record, snapshot.dependencies)));
    }
    return context;
}

[[nodiscard]] const query::QueryReuseDecision* tooling_decision_for_key(
    const query::QueryReusePlan& plan, const query::QueryKey key) noexcept
{
    for (const query::QueryReuseDecision& decision : plan.decisions) {
        if (decision.key == key) {
            return &decision;
        }
    }
    return nullptr;
}

[[nodiscard]] bool tooling_has_matching_fact(
    const std::vector<IdeSemanticFact>& facts, const IdeSemanticFact& expected) noexcept
{
    for (const IdeSemanticFact& fact : facts) {
        if (fact.query == expected.query && fact.kind == expected.kind && fact.name == expected.name) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] ToolingReuseFactStatus tooling_status_for_decision(
    const query::QueryReuseDecision* const decision) noexcept
{
    if (decision == nullptr) {
        return ToolingReuseFactStatus::malformed;
    }
    return decision->disposition == query::QueryReuseDisposition::reuse ? ToolingReuseFactStatus::unchanged
                                                                        : ToolingReuseFactStatus::recomputed;
}

[[nodiscard]] ToolingReuseFact tooling_reuse_fact_from_semantic_fact(const IdeSnapshot& snapshot,
    const IdeSemanticFact& fact, const ToolingReuseFactStatus status,
    const query::QueryRecordChangeStatus change_status)
{
    return ToolingReuseFact{
        status,
        change_status,
        fact.query,
        fact.definition,
        fact.member,
        fact.body,
        fact.generic_instance,
        tooling_range_for_snapshot(snapshot, fact.range),
        fact.name,
        std::string(tooling_semantic_fact_kind_name(fact.kind)),
        fact.detail,
        tooling_stable_key_or_empty(fact.query),
        tooling_stable_key_or_empty(fact.definition),
        tooling_stable_key_or_empty(fact.member),
        tooling_stable_key_or_empty(fact.body),
        tooling_stable_key_or_empty(fact.generic_instance),
        fact.part_index,
    };
}

void tooling_count_reuse_fact(ToolingReuseSummary& summary, const ToolingReuseFactStatus status) noexcept
{
    summary.total_facts += 1;
    switch (status) {
        case ToolingReuseFactStatus::unchanged:
            summary.unchanged_facts += 1;
            break;
        case ToolingReuseFactStatus::recomputed:
            summary.recomputed_facts += 1;
            break;
        case ToolingReuseFactStatus::invalidated:
            summary.invalidated_facts += 1;
            break;
        case ToolingReuseFactStatus::malformed:
            summary.malformed_facts += 1;
            break;
    }
}

void tooling_append_reuse_fact(ToolingReusePlan& plan, ToolingReuseFact fact)
{
    tooling_count_reuse_fact(plan.summary, fact.status);
    plan.facts.push_back(std::move(fact));
}

[[nodiscard]] ToolingDependencyDiff tooling_dependency_diff(
    std::vector<query::QueryDependencyEdge> before, std::vector<query::QueryDependencyEdge> after)
{
    std::sort(before.begin(), before.end(), tooling_dependency_edge_less);
    std::sort(after.begin(), after.end(), tooling_dependency_edge_less);
    ToolingDependencyDiff diff;
    base::usize before_index = 0;
    base::usize after_index = 0;
    while (before_index < before.size() || after_index < after.size()) {
        if (before_index >= before.size()) {
            diff.added += after.size() - after_index;
            break;
        }
        if (after_index >= after.size()) {
            diff.removed += before.size() - before_index;
            break;
        }
        const query::QueryDependencyEdge before_edge = before[before_index];
        const query::QueryDependencyEdge after_edge = after[after_index];
        if (before_edge == after_edge) {
            ++diff.unchanged;
            ++before_index;
            ++after_index;
            continue;
        }
        if (tooling_dependency_edge_less(before_edge, after_edge)) {
            ++diff.removed;
            ++before_index;
            continue;
        }
        ++diff.added;
        ++after_index;
    }
    return diff;
}

void tooling_append_invalidation_roots(ToolingReusePlan& plan, const IdeSnapshot& before, const IdeEditImpact& impact)
{
    if (!impact.valid) {
        return;
    }
    for (const IdeSemanticFact& fact : before.query.semantic_facts) {
        if (!query::is_valid(fact.query) || !tooling_ranges_intersect(fact.range, impact.range)) {
            continue;
        }
        plan.invalidation_roots.push_back(ToolingInvalidationRoot{
            fact.query,
            tooling_range_for_snapshot(before, fact.range),
            fact.name,
            std::string(tooling_semantic_fact_kind_name(fact.kind)),
            std::string(tooling_invalidation_reason(fact)),
            tooling_stable_key_or_empty(fact.query),
        });
    }
}

void tooling_append_current_facts(ToolingReusePlan& plan, const IdeSnapshot& after)
{
    for (const IdeSemanticFact& fact : after.query.semantic_facts) {
        const query::QueryReuseDecision* const decision = tooling_decision_for_key(plan.query_plan, fact.query);
        const ToolingReuseFactStatus status = tooling_status_for_decision(decision);
        const query::QueryRecordChangeStatus change_status =
            decision == nullptr ? query::QueryRecordChangeStatus::malformed : decision->change_status;
        tooling_append_reuse_fact(plan, tooling_reuse_fact_from_semantic_fact(after, fact, status, change_status));
    }
}

void tooling_append_invalidated_facts(ToolingReusePlan& plan, const IdeSnapshot& before, const IdeSnapshot& after)
{
    for (const IdeSemanticFact& fact : before.query.semantic_facts) {
        if (tooling_has_matching_fact(after.query.semantic_facts, fact)) {
            continue;
        }
        tooling_append_reuse_fact(plan,
            tooling_reuse_fact_from_semantic_fact(
                before, fact, ToolingReuseFactStatus::invalidated, query::QueryRecordChangeStatus::missing));
    }
}

[[nodiscard]] bool tooling_roots_are_body_local(const std::vector<ToolingInvalidationRoot>& roots) noexcept
{
    if (roots.empty()) {
        return false;
    }
    for (const ToolingInvalidationRoot& root : roots) {
        if (root.kind != TOOLING_REUSE_KIND_FUNCTION_BODY_SYNTAX && root.kind != TOOLING_REUSE_KIND_TYPE_CHECK_BODY
            && root.kind != TOOLING_REUSE_KIND_BORROW_SUMMARY && root.kind != TOOLING_REUSE_KIND_LIFETIME_FACTS
            && root.kind != TOOLING_REUSE_KIND_DROPCK_FACTS && root.kind != TOOLING_REUSE_KIND_BODY_LOAN_CHECK) {
            return false;
        }
    }
    return true;
}

void tooling_sort_reuse_facts(std::vector<ToolingReuseFact>& facts)
{
    std::sort(facts.begin(), facts.end(), [](const ToolingReuseFact& lhs, const ToolingReuseFact& rhs) {
        return std::tie(lhs.range.path, lhs.range.range.begin, lhs.range.range.end, lhs.kind, lhs.name)
            < std::tie(rhs.range.path, rhs.range.range.begin, rhs.range.range.end, rhs.kind, rhs.name);
    });
}

void tooling_sort_invalidation_roots(std::vector<ToolingInvalidationRoot>& roots)
{
    std::sort(roots.begin(), roots.end(), [](const ToolingInvalidationRoot& lhs, const ToolingInvalidationRoot& rhs) {
        return std::tie(lhs.range.path, lhs.range.range.begin, lhs.range.range.end, lhs.kind, lhs.name)
            < std::tie(rhs.range.path, rhs.range.range.begin, rhs.range.range.end, rhs.kind, rhs.name);
    });
}

} // namespace

std::string_view tooling_reuse_fact_status_name(const ToolingReuseFactStatus status) noexcept
{
    switch (status) {
        case ToolingReuseFactStatus::unchanged:
            return TOOLING_REUSE_STATUS_UNCHANGED;
        case ToolingReuseFactStatus::recomputed:
            return TOOLING_REUSE_STATUS_RECOMPUTED;
        case ToolingReuseFactStatus::invalidated:
            return TOOLING_REUSE_STATUS_INVALIDATED;
        case ToolingReuseFactStatus::malformed:
            return TOOLING_REUSE_STATUS_MALFORMED;
    }
    return TOOLING_REUSE_STATUS_MALFORMED;
}

ToolingReusePlan tooling_plan_reuse(const IdeSnapshot& before, const IdeSnapshot& after, const IdeEditImpact& impact)
{
    ToolingReusePlan plan;
    plan.valid = before.query.records.empty() || after.query.records.empty() ? false : impact.valid;
    plan.impact = impact;
    plan.impact_range = tooling_range_for_snapshot(before, impact.range);
    query::QueryContext cached_context = tooling_seed_query_context(before.query);
    plan.query_plan = query::build_query_reuse_plan(cached_context, after.query.records);
    plan.dependencies = tooling_dependency_diff(before.query.dependencies, after.query.dependencies);
    tooling_append_invalidation_roots(plan, before, impact);
    tooling_append_current_facts(plan, after);
    tooling_append_invalidated_facts(plan, before, after);
    plan.summary.body_local = tooling_roots_are_body_local(plan.invalidation_roots);
    tooling_sort_invalidation_roots(plan.invalidation_roots);
    tooling_sort_reuse_facts(plan.facts);
    return plan;
}

} // namespace aurex::tooling
