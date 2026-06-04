#include <algorithm>
#include <span>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <frontend/sema/internal/dropck/private/dropck_analysis.hpp>

namespace aurex::sema {
namespace {

struct DropCheckRegionEvent {
    base::u32 point = SEMA_BODY_FLOW_INVALID_INDEX;
    base::u32 region = SEMA_LIFETIME_INVALID_INDEX;
};

constexpr std::string_view SEMA_DROPCK_ORIGIN_KEY_SEPARATOR = " | ";
constexpr base::usize SEMA_DROPCK_ORIGIN_TYPE_STACK_INITIAL_CAPACITY = 32;

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

void append_origin_key_regions(const std::string_view key,
    const std::unordered_map<std::string_view, base::u32>& lifetime_region_by_origin_name,
    std::vector<base::u32>& regions)
{
    base::usize begin = 0;
    while (begin <= key.size()) {
        const base::usize separator = key.find(SEMA_DROPCK_ORIGIN_KEY_SEPARATOR, begin);
        const base::usize end = separator == std::string_view::npos ? key.size() : separator;
        const std::string_view name = key.substr(begin, end - begin);
        if (!name.empty()) {
            if (const auto found = lifetime_region_by_origin_name.find(name);
                found != lifetime_region_by_origin_name.end()) {
                regions.push_back(found->second);
            }
        }
        if (separator == std::string_view::npos) {
            break;
        }
        begin = separator + SEMA_DROPCK_ORIGIN_KEY_SEPARATOR.size();
    }
}

[[nodiscard]] bool drop_glue_step_observes_structural_borrows(const DropGlueStepKind kind) noexcept
{
    switch (kind) {
        case DropGlueStepKind::struct_field:
        case DropGlueStepKind::tuple_element:
        case DropGlueStepKind::array_element:
        case DropGlueStepKind::enum_payload:
            return true;
        case DropGlueStepKind::custom_destructor:
        case DropGlueStepKind::generic_value:
        case DropGlueStepKind::opaque_value:
            return false;
    }
    return false;
}

} // namespace

void SemanticAnalyzerCore::DropCheckAnalyzer::solve()
{
    const BodyFlowGraph* const graph = this->body_graph();
    if (graph != nullptr) {
        this->build_active_region_index(*graph);
    }
    this->build_lifetime_region_origin_index();

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
        this->enforce_destructor_observed_borrow_safety(action, glue.plan, active_regions);
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

void SemanticAnalyzerCore::DropCheckAnalyzer::build_lifetime_region_origin_index()
{
    this->lifetime_region_by_origin_name_.clear();
    const FunctionLifetimeFacts* const lifetime = this->lifetime_facts();
    if (lifetime == nullptr || lifetime->regions.empty()) {
        return;
    }
    this->lifetime_region_by_origin_name_.reserve(lifetime->regions.size());
    for (base::usize index = 0; index < lifetime->regions.size(); ++index) {
        const LifetimeRegion& region = lifetime->regions[index];
        if (region.kind == LifetimeRegionKind::static_ || region.name.empty()) {
            continue;
        }
        this->lifetime_region_by_origin_name_.emplace(region.name.view(), static_cast<base::u32>(index));
    }
    this->concrete_origin_regions_by_type_.clear();
}

std::span<const base::u32> SemanticAnalyzerCore::DropCheckAnalyzer::concrete_origin_regions_for_type(
    const TypeHandle type) const
{
    if (!this->valid_type(type) || this->lifetime_region_by_origin_name_.empty()) {
        return {};
    }
    if (const auto cached = this->concrete_origin_regions_by_type_.find(type.value);
        cached != this->concrete_origin_regions_by_type_.end()) {
        return std::span<const base::u32>(cached->second);
    }

    std::vector<base::u32> regions;
    std::vector<TypeFrame> pending;
    std::unordered_set<base::u32> visited;
    pending.reserve(SEMA_DROPCK_ORIGIN_TYPE_STACK_INITIAL_CAPACITY);
    visited.reserve(SEMA_DROPCK_ORIGIN_TYPE_STACK_INITIAL_CAPACITY);
    pending.push_back(TypeFrame{type});
    while (!pending.empty()) {
        const TypeFrame frame = pending.back();
        pending.pop_back();
        if (!this->valid_type(frame.type) || !visited.insert(frame.type.value).second) {
            continue;
        }
        const TypeInfo& info = this->core_.state_.checked.types.get(frame.type);
        switch (info.kind) {
            case TypeKind::reference:
                append_origin_key_regions(info.reference_origin_key.view(), this->lifetime_region_by_origin_name_,
                    regions);
                pending.push_back(TypeFrame{info.pointee});
                break;
            case TypeKind::array:
                pending.push_back(TypeFrame{info.array_element});
                break;
            case TypeKind::slice:
                pending.push_back(TypeFrame{info.slice_element});
                break;
            case TypeKind::tuple:
                for (const TypeHandle element : info.tuple_elements) {
                    pending.push_back(TypeFrame{element});
                }
                break;
            case TypeKind::function:
                for (const TypeHandle param : info.function_params) {
                    pending.push_back(TypeFrame{param});
                }
                pending.push_back(TypeFrame{info.function_return});
                break;
            case TypeKind::struct_:
                if (const StructInfo* const structure = this->core_.find_struct(frame.type); structure != nullptr) {
                    for (const StructFieldInfo& field : structure->fields) {
                        pending.push_back(TypeFrame{field.type});
                    }
                }
                break;
            case TypeKind::enum_:
                if (const EnumCaseList* const cases = this->core_.find_enum_cases_by_type(frame.type);
                    cases != nullptr) {
                    for (const EnumCaseInfo* const enum_case : *cases) {
                        if (enum_case == nullptr) {
                            continue;
                        }
                        for (const TypeHandle payload : enum_case->payload_types) {
                            pending.push_back(TypeFrame{payload});
                        }
                    }
                }
                break;
            case TypeKind::builtin:
            case TypeKind::pointer:
            case TypeKind::opaque_struct:
            case TypeKind::generic_param:
            case TypeKind::associated_projection:
                break;
        }
    }

    std::ranges::sort(regions);
    regions.erase(std::ranges::unique(regions).begin(), regions.end());
    const auto inserted = this->concrete_origin_regions_by_type_.emplace(type.value, std::move(regions));
    return std::span<const base::u32>(inserted.first->second);
}

void SemanticAnalyzerCore::DropCheckAnalyzer::enforce_destructor_observed_borrow_safety(
    const DropActionFact& action, const DropGluePlan& plan, const std::span<const base::u32> active_regions)
{
    for (const DropGlueStep& step : plan.steps) {
        if (!drop_glue_step_observes_structural_borrows(step.kind)) {
            continue;
        }
        for (const base::u32 region : this->concrete_origin_regions_for_type(step.value_type)) {
            if (!std::ranges::binary_search(active_regions, region)) {
                static_cast<void>(
                    this->append_violation(DropCheckViolationKind::borrowed_field_dangling, action, region));
            }
        }
    }
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
