#include <algorithm>
#include <span>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include <frontend/sema/internal/dropck/private/dropck_analysis.hpp>

namespace aurex::sema {
namespace {

struct DropCheckRegionEvent {
    base::u32 point = SEMA_BODY_FLOW_INVALID_INDEX;
    base::u32 region = SEMA_LIFETIME_INVALID_INDEX;
};

[[nodiscard]] bool region_event_less(const DropCheckRegionEvent lhs, const DropCheckRegionEvent rhs) noexcept
{
    return std::tie(lhs.point, lhs.region) < std::tie(rhs.point, rhs.region);
}

[[nodiscard]] bool valid_live_range(const BodyFlowGraph& graph, const LifetimeRegionLiveRange& live_range) noexcept
{
    return live_range.region != SEMA_LIFETIME_INVALID_INDEX
        && live_range.first_point != SEMA_BODY_FLOW_INVALID_INDEX
        && live_range.last_point != SEMA_BODY_FLOW_INVALID_INDEX
        && live_range.first_point <= live_range.last_point
        && live_range.first_point < graph.points.size();
}

[[nodiscard]] bool conflict_is_dropck_action(const BodyLoanConflictKind kind) noexcept
{
    return kind == BodyLoanConflictKind::drop || kind == BodyLoanConflictKind::cleanup
        || kind == BodyLoanConflictKind::reinit;
}

} // namespace

void SemanticAnalyzerCore::DropCheckAnalyzer::solve()
{
    const BodyFlowGraph* const graph = this->body_graph();
    if (graph != nullptr) {
        this->build_active_region_index(*graph);
    }

    if (FunctionLifetimeFacts* const lifetime = this->mutable_lifetime_facts(); lifetime != nullptr) {
        for (const LifetimeTypeOutlivesConstraint& constraint : lifetime->type_outlives_constraints) {
            this->emitted_type_outlives_.insert(TypeOutlivesKey{
                .type = constraint.type.value,
                .region = constraint.region,
            });
        }
    }

    std::unordered_map<base::u32, base::usize> drop_action_by_flow_action;
    drop_action_by_flow_action.reserve(this->facts_.actions.size());
    for (base::usize index = 0; index < this->facts_.actions.size(); ++index) {
        drop_action_by_flow_action.emplace(this->facts_.actions[index].action, index);
    }

    if (const auto loan = this->core_.state_.checked.body_loan_checks.find(this->key_);
        loan != this->core_.state_.checked.body_loan_checks.end()) {
        for (const BodyLoanConflict& conflict : loan->second.conflicts) {
            if (!conflict_is_dropck_action(conflict.kind)) {
                continue;
            }
            const auto action_index = drop_action_by_flow_action.find(conflict.action);
            if (action_index == drop_action_by_flow_action.end()
                || action_index->second >= this->facts_.actions.size()) {
                continue;
            }
            DropActionFact action = this->facts_.actions[action_index->second];
            action.point = conflict.point;
            action.place = conflict.place;
            action.range = conflict.range;
            static_cast<void>(this->append_violation(
                DropCheckViolationKind::borrowed_drop, action, SEMA_LIFETIME_INVALID_INDEX,
                conflict.diagnostic_emitted));
        }
    }

    for (const DropActionFact& action : this->facts_.actions) {
        if (!this->valid_type(action.type)) {
            continue;
        }
        const auto fact_index = this->fact_by_type_.find(action.type.value);
        if (fact_index == this->fact_by_type_.end() || fact_index->second >= this->facts_.facts.size()) {
            continue;
        }
        const DropGlueCacheEntry& glue = this->cached_drop_glue(action.type);
        std::span<const base::u32> active_regions = this->active_regions_at_point(action.point);
        if (active_regions.empty()) {
            continue;
        }
        DropCheckFact& fact = this->facts_.facts[fact_index->second];
        if (this->type_can_contain_borrow(action.type)) {
            for (const base::u32 region : active_regions) {
                static_cast<void>(this->append_required_outlives(fact, action.type, region, action.range));
            }
        }
        for (const DropGlueStep& step : glue.plan.steps) {
            if (!this->type_can_contain_borrow(step.value_type)) {
                continue;
            }
            for (const base::u32 region : active_regions) {
                static_cast<void>(this->append_required_outlives(fact, step.value_type, region, action.range));
            }
        }
    }

    for (DropCheckFact& fact : this->facts_.facts) {
        fact.fingerprint = drop_check_fact_fingerprint(fact);
    }
    this->facts_.solved = true;
    this->facts_.fingerprint = function_drop_check_facts_fingerprint(this->facts_);

    if (FunctionLifetimeFacts* const lifetime = this->mutable_lifetime_facts(); lifetime != nullptr) {
        lifetime->fingerprint = function_lifetime_facts_fingerprint(*lifetime);
    }
}

bool SemanticAnalyzerCore::DropCheckAnalyzer::append_required_outlives(DropCheckFact& fact,
    const TypeHandle type, const base::u32 region, const base::SourceRange& range)
{
    if (!this->valid_type(type) || region == SEMA_LIFETIME_INVALID_INDEX) {
        return false;
    }
    if (!this->append_lifetime_type_outlives(type, region, range)) {
        return false;
    }
    fact.required_outlives.push_back(DropCheckRequiredOutlives{
        .type = type,
        .region = region,
        .reason = LifetimeConstraintReason::dropck,
        .range = range,
    });
    return true;
}

bool SemanticAnalyzerCore::DropCheckAnalyzer::append_violation(const DropCheckViolationKind kind,
    const DropActionFact& action, const base::u32 region, const bool diagnostic_emitted)
{
    const DropCheckViolationKey key{
        .kind = static_cast<base::u8>(kind),
        .action = action.action,
        .point = action.point,
        .place = action.place,
        .type = action.type.value,
        .region = region,
    };
    if (!this->violation_dedupe_.insert(key).second) {
        return false;
    }
    this->facts_.violations.push_back(DropCheckViolation{
        .kind = kind,
        .action = action.action,
        .point = action.point,
        .place = action.place,
        .type = action.type,
        .region = region,
        .diagnostic_emitted = diagnostic_emitted,
        .range = action.range,
    });
    return true;
}

bool SemanticAnalyzerCore::DropCheckAnalyzer::append_lifetime_type_outlives(
    const TypeHandle type, const base::u32 region, const base::SourceRange& range)
{
    if (!this->valid_type(type) || region == SEMA_LIFETIME_INVALID_INDEX) {
        return false;
    }
    const TypeOutlivesKey key{
        .type = type.value,
        .region = region,
    };
    if (!this->emitted_type_outlives_.insert(key).second) {
        return false;
    }
    FunctionLifetimeFacts* const lifetime = this->mutable_lifetime_facts();
    if (lifetime == nullptr) {
        return true;
    }
    lifetime->type_outlives_constraints.push_back(LifetimeTypeOutlivesConstraint{
        .type = type,
        .region = region,
        .reason = LifetimeConstraintReason::dropck,
        .range = range,
    });
    return true;
}

void SemanticAnalyzerCore::DropCheckAnalyzer::build_active_region_index(const BodyFlowGraph& graph)
{
    this->active_regions_by_point_.clear();
    if (graph.points.empty()) {
        return;
    }

    const FunctionLifetimeFacts* const lifetime = this->lifetime_facts();
    if (lifetime == nullptr || lifetime->live_ranges.empty()) {
        return;
    }

    std::vector<base::u32> query_points;
    query_points.reserve(this->facts_.actions.size());
    for (const DropActionFact& action : this->facts_.actions) {
        if (action.point != SEMA_BODY_FLOW_INVALID_INDEX && action.point < graph.points.size()) {
            query_points.push_back(action.point);
        }
    }
    if (query_points.empty()) {
        return;
    }
    std::ranges::sort(query_points);
    query_points.erase(std::unique(query_points.begin(), query_points.end()), query_points.end());

    std::vector<DropCheckRegionEvent> starts;
    std::vector<DropCheckRegionEvent> ends;
    starts.reserve(lifetime->live_ranges.size());
    ends.reserve(lifetime->live_ranges.size());
    for (const LifetimeRegionLiveRange& live_range : lifetime->live_ranges) {
        if (!valid_live_range(graph, live_range)) {
            continue;
        }
        starts.push_back(DropCheckRegionEvent{live_range.first_point, live_range.region});
        const base::u64 end_point = static_cast<base::u64>(live_range.last_point) + 1U;
        if (end_point < static_cast<base::u64>(graph.points.size())) {
            ends.push_back(DropCheckRegionEvent{static_cast<base::u32>(end_point), live_range.region});
        }
    }
    std::ranges::sort(starts, region_event_less);
    std::ranges::sort(ends, region_event_less);

    std::unordered_map<base::u32, base::u32> active_counts;
    active_counts.reserve(starts.size());
    base::usize start_index = 0;
    base::usize end_index = 0;
    for (const base::u32 point : query_points) {
        while (start_index < starts.size() && starts[start_index].point <= point) {
            active_counts[starts[start_index].region] += 1U;
            ++start_index;
        }
        while (end_index < ends.size() && ends[end_index].point <= point) {
            const base::u32 region = ends[end_index].region;
            base::u32& count = active_counts[region];
            if (count <= 1U) {
                active_counts.erase(region);
            } else {
                count -= 1U;
            }
            ++end_index;
        }
        if (active_counts.empty()) {
            continue;
        }
        std::vector<base::u32> active;
        active.reserve(active_counts.size());
        for (const auto& entry : active_counts) {
            active.push_back(entry.first);
        }
        std::ranges::sort(active);
        this->active_regions_by_point_.emplace(point, std::move(active));
    }
}

std::span<const base::u32> SemanticAnalyzerCore::DropCheckAnalyzer::active_regions_at_point(
    const base::u32 point) const noexcept
{
    if (point == SEMA_BODY_FLOW_INVALID_INDEX) {
        return {};
    }
    const auto found = this->active_regions_by_point_.find(point);
    if (found == this->active_regions_by_point_.end()) {
        return {};
    }
    return found->second;
}

} // namespace aurex::sema
