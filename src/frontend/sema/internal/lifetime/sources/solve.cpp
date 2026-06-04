#include <algorithm>
#include <tuple>
#include <vector>

#include <frontend/sema/internal/lifetime/private/lifetime_analysis.hpp>

namespace aurex::sema {
namespace {

constexpr base::usize SEMA_LIFETIME_REACHABILITY_INITIAL_STACK_CAPACITY = 32;

[[nodiscard]] bool valid_body_flow_point(const BodyFlowGraph& graph, const base::u32 point) noexcept
{
    return point != SEMA_BODY_FLOW_INVALID_INDEX && point < graph.points.size();
}

void append_outlives_constraint(std::vector<LifetimeOutlivesConstraint>& constraints, const base::u32 longer,
    const base::u32 shorter, const LifetimeConstraintReason reason, const base::SourceRange& range)
{
    if (longer == shorter) {
        return;
    }
    constraints.push_back(LifetimeOutlivesConstraint{
        .longer_region = longer,
        .shorter_region = shorter,
        .reason = reason,
        .range = range,
    });
}

} // namespace

void SemanticAnalyzerCore::LifetimeAnalyzer::solve()
{
    for (base::u32 index = 0; index < this->facts_.regions.size(); ++index) {
        if (this->facts_.regions[index].kind != LifetimeRegionKind::static_) {
            continue;
        }
        for (base::u32 target = 0; target < this->facts_.regions.size(); ++target) {
            append_outlives_constraint(this->facts_.outlives_constraints, index, target,
                LifetimeConstraintReason::declared_origin, this->facts_.regions[index].range);
        }
    }
    std::ranges::sort(this->facts_.outlives_constraints,
        [](const LifetimeOutlivesConstraint& lhs, const LifetimeOutlivesConstraint& rhs) {
            return std::tie(lhs.longer_region, lhs.shorter_region, lhs.reason)
                < std::tie(rhs.longer_region, rhs.shorter_region, rhs.reason);
        });
    this->facts_.outlives_constraints.erase(
        std::ranges::unique(this->facts_.outlives_constraints,
            [](const LifetimeOutlivesConstraint& lhs, const LifetimeOutlivesConstraint& rhs) {
                return lhs.longer_region == rhs.longer_region && lhs.shorter_region == rhs.shorter_region
                    && lhs.reason == rhs.reason;
            }).begin(),
        this->facts_.outlives_constraints.end());
    this->initialize_outlives_graph();
    this->collect_body_live_ranges();
    this->facts_.solved = true;
}

void SemanticAnalyzerCore::LifetimeAnalyzer::initialize_outlives_graph()
{
    const base::usize region_count = this->facts_.regions.size();
    this->outlives_successors_.assign(region_count, {});
    this->outlives_reachability_cache_.assign(region_count, {});
    this->outlives_reachability_cached_.assign(region_count, false);
    for (const LifetimeOutlivesConstraint& constraint : this->facts_.outlives_constraints) {
        if (constraint.longer_region >= region_count || constraint.shorter_region >= region_count
            || constraint.longer_region == constraint.shorter_region) {
            continue;
        }
        this->outlives_successors_[constraint.longer_region].push_back(constraint.shorter_region);
    }
    for (std::vector<base::u32>& successors : this->outlives_successors_) {
        std::ranges::sort(successors);
        successors.erase(std::ranges::unique(successors).begin(), successors.end());
    }
}

void SemanticAnalyzerCore::LifetimeAnalyzer::collect_body_live_ranges()
{
    const auto found = this->core_.state_.checked.body_flow_graphs.find(this->key_);
    if (found == this->core_.state_.checked.body_flow_graphs.end() || found->second.points.empty()) {
        return;
    }

    const BodyFlowGraph& graph = found->second;
    const base::u32 first_point = this->first_body_flow_point(graph);
    const base::u32 last_point = this->last_body_flow_point(graph);
    const base::u32 point_count = static_cast<base::u32>(graph.points.size());
    for (base::u32 region = 0; region < this->facts_.regions.size(); ++region) {
        const LifetimeRegion& info = this->facts_.regions[region];
        switch (info.kind) {
            case LifetimeRegionKind::parameter:
            case LifetimeRegionKind::self:
            case LifetimeRegionKind::static_:
            case LifetimeRegionKind::explicit_origin:
                this->append_live_range(region, first_point, last_point, point_count, info.range);
                break;
            case LifetimeRegionKind::inferred:
            case LifetimeRegionKind::local:
            case LifetimeRegionKind::temporary:
            case LifetimeRegionKind::unknown:
                break;
        }
    }

    for (const LifetimeReturnRegion& returned : this->facts_.return_regions) {
        if (returned.region >= this->facts_.regions.size()) {
            continue;
        }
        const LifetimeRegionKind kind = this->facts_.regions[returned.region].kind;
        const base::u32 return_point = this->return_body_flow_point(graph, returned.return_expr);
        const base::u32 anchor = valid_body_flow_point(graph, return_point) ? return_point : last_point;
        if (kind == LifetimeRegionKind::local || kind == LifetimeRegionKind::temporary
            || kind == LifetimeRegionKind::unknown || kind == LifetimeRegionKind::inferred) {
            this->append_live_range(returned.region, anchor, anchor, 1U, returned.range);
        }
    }
}

base::u32 SemanticAnalyzerCore::LifetimeAnalyzer::first_body_flow_point(const BodyFlowGraph& graph) const noexcept
{
    for (base::u32 index = 0; index < graph.points.size(); ++index) {
        if (graph.points[index].kind == BodyFlowPointKind::entry) {
            return index;
        }
    }
    return graph.points.empty() ? SEMA_BODY_FLOW_INVALID_INDEX : 0U;
}

base::u32 SemanticAnalyzerCore::LifetimeAnalyzer::last_body_flow_point(const BodyFlowGraph& graph) const noexcept
{
    for (base::u32 index = 0; index < graph.points.size(); ++index) {
        if (graph.points[index].kind == BodyFlowPointKind::exit) {
            return index;
        }
    }
    return graph.points.empty()
        ? SEMA_BODY_FLOW_INVALID_INDEX
        : static_cast<base::u32>(graph.points.size() - 1U);
}

base::u32 SemanticAnalyzerCore::LifetimeAnalyzer::return_body_flow_point(
    const BodyFlowGraph& graph, const syntax::ExprId expr) const noexcept
{
    base::u32 fallback = SEMA_BODY_FLOW_INVALID_INDEX;
    for (const BodyFlowAction& action : graph.actions) {
        if (action.kind != BodyFlowActionKind::return_) {
            continue;
        }
        if (syntax::is_valid(expr) && action.expr.value == expr.value) {
            return action.point;
        }
        if (fallback == SEMA_BODY_FLOW_INVALID_INDEX) {
            fallback = action.point;
        }
    }
    return fallback;
}

void SemanticAnalyzerCore::LifetimeAnalyzer::append_live_range(const base::u32 region,
    const base::u32 first_point, const base::u32 last_point, const base::u32 point_count,
    const base::SourceRange& range)
{
    if (region >= this->facts_.regions.size()
        || (first_point == SEMA_BODY_FLOW_INVALID_INDEX && last_point == SEMA_BODY_FLOW_INVALID_INDEX)) {
        return;
    }
    this->facts_.live_ranges.push_back(LifetimeRegionLiveRange{
        .region = region,
        .first_point = first_point,
        .last_point = last_point,
        .point_count = point_count,
        .range = range,
    });
}

void SemanticAnalyzerCore::LifetimeAnalyzer::cache_outlives_reachability(const base::u32 longer)
{
    if (longer >= this->outlives_successors_.size() || longer >= this->outlives_reachability_cache_.size()
        || longer >= this->outlives_reachability_cached_.size()) {
        return;
    }
    std::vector<bool>& reachable = this->outlives_reachability_cache_[longer];
    reachable.assign(this->outlives_successors_.size(), false);
    std::vector<base::u32> pending;
    pending.reserve(SEMA_LIFETIME_REACHABILITY_INITIAL_STACK_CAPACITY);
    pending.push_back(longer);
    reachable[longer] = true;
    while (!pending.empty()) {
        const base::u32 current = pending.back();
        pending.pop_back();
        if (current >= this->outlives_successors_.size()) {
            continue;
        }
        for (const base::u32 successor : this->outlives_successors_[current]) {
            if (successor >= reachable.size() || reachable[successor]) {
                continue;
            }
            reachable[successor] = true;
            pending.push_back(successor);
        }
    }
    this->outlives_reachability_cached_[longer] = true;
}

bool SemanticAnalyzerCore::LifetimeAnalyzer::region_outlives(const base::u32 longer, const base::u32 shorter)
{
    if (longer == shorter) {
        return true;
    }
    if (longer >= this->outlives_successors_.size() || shorter >= this->outlives_successors_.size()) {
        return false;
    }
    if (longer >= this->outlives_reachability_cached_.size() || !this->outlives_reachability_cached_[longer]) {
        this->cache_outlives_reachability(longer);
    }
    return longer < this->outlives_reachability_cache_.size()
        && shorter < this->outlives_reachability_cache_[longer].size()
        && this->outlives_reachability_cache_[longer][shorter];
}

} // namespace aurex::sema
