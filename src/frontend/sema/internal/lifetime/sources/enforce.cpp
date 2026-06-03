#include <aurex/frontend/sema/sema_messages.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include <frontend/sema/internal/lifetime/private/lifetime_analysis.hpp>

namespace aurex::sema {
namespace {

[[nodiscard]] bool valid_region_index(const FunctionLifetimeFacts& facts, const base::u32 region) noexcept
{
    return region != SEMA_LIFETIME_INVALID_INDEX && region < facts.regions.size();
}

[[nodiscard]] bool region_is_unknown(const FunctionLifetimeFacts& facts, const base::u32 region) noexcept
{
    return valid_region_index(facts, region) && facts.regions[region].kind == LifetimeRegionKind::unknown;
}

[[nodiscard]] bool region_set_contains(const std::vector<base::u32>& regions, const base::u32 candidate) noexcept
{
    return std::ranges::find(regions, candidate) != regions.end();
}

[[nodiscard]] bool region_set_has_unknown(
    const FunctionLifetimeFacts& facts, const std::vector<base::u32>& regions) noexcept
{
    return std::ranges::any_of(regions, [&](const base::u32 region) {
        return region_is_unknown(facts, region);
    });
}

[[nodiscard]] bool lifetime_violation_same_identity(
    const LifetimeViolation& lhs, const LifetimeViolationKind kind, const base::u32 region,
    const base::u32 related_region, const TypeHandle type, const syntax::ExprId expr) noexcept
{
    return lhs.kind == kind && lhs.region == region && lhs.related_region == related_region
        && lhs.type.value == type.value && lhs.expr.value == expr.value;
}

} // namespace

void SemanticAnalyzerCore::LifetimeAnalyzer::enforce_return_origin_subset()
{
    if (!this->type_can_contain_borrow(this->signature_->return_type) || this->facts_.return_regions.empty()) {
        return;
    }

    RegionSet declared_returns =
        this->regions_for_type_origin_key(this->signature_->return_type, this->signature_->range);
    if (declared_returns.regions.empty() || region_set_has_unknown(this->facts_, declared_returns.regions)) {
        return;
    }

    const base::u32 related_region = declared_returns.regions.front();
    for (const LifetimeReturnRegion& returned : this->facts_.return_regions) {
        if (region_is_unknown(this->facts_, returned.region)
            || region_set_contains(declared_returns.regions, returned.region)) {
            continue;
        }
        this->add_violation(LifetimeViolationKind::return_origin_outside_type, returned.region, related_region,
            this->signature_->return_type, returned.return_expr, false, returned.range);
    }
}

void SemanticAnalyzerCore::LifetimeAnalyzer::enforce_ambiguous_elision()
{
    if (this->has_declared_borrow_contract()
        || (!this->boundary_requires_explicit_contract() && !this->has_declared_origin_params())
        || !this->type_can_contain_borrow(this->signature_->return_type)) {
        return;
    }

    RegionSet return_origins =
        this->regions_for_type_origin_key(this->signature_->return_type, this->signature_->range);
    if (!return_origins.regions.empty()) {
        return;
    }

    std::vector<base::u32> candidate_regions;
    if (this->include_body_facts_) {
        candidate_regions.reserve(this->facts_.return_regions.size());
        for (const LifetimeReturnRegion& returned : this->facts_.return_regions) {
            if (valid_region_index(this->facts_, returned.region)
                && !region_is_unknown(this->facts_, returned.region)) {
                candidate_regions.push_back(returned.region);
            }
        }
    } else {
        if (!this->type_has_concrete_borrow_surface(this->signature_->return_type)) {
            return;
        }
        candidate_regions.reserve(this->signature_->param_types.size());
        for (base::usize index = 0; index < this->signature_->param_types.size(); ++index) {
            if (!this->type_can_contain_borrow(this->signature_->param_types[index])) {
                continue;
            }
            RegionSet param_regions = this->regions_for_param_type_origin(index);
            if (param_regions.regions.empty()) {
                param_regions.regions.push_back(this->parameter_region(index));
            }
            candidate_regions.insert(
                candidate_regions.end(), param_regions.regions.begin(), param_regions.regions.end());
        }
    }

    std::ranges::sort(candidate_regions);
    candidate_regions.erase(std::ranges::unique(candidate_regions).begin(), candidate_regions.end());
    if (candidate_regions.size() <= 1U) {
        return;
    }

    this->add_violation(LifetimeViolationKind::ambiguous_elision, candidate_regions[0], candidate_regions[1],
        this->signature_->return_type, syntax::INVALID_EXPR_ID, false, this->signature_->range);
}

void SemanticAnalyzerCore::LifetimeAnalyzer::enforce_type_outlives()
{
    for (const LifetimeTypeOutlivesConstraint& constraint : this->facts_.type_outlives_constraints) {
        if (!valid_region_index(this->facts_, constraint.region) || region_is_unknown(this->facts_, constraint.region)) {
            continue;
        }
        for (const OriginName& origin : this->origin_names_for_type(constraint.type)) {
            const base::u32 related = this->declared_or_unknown_origin(
                origin.name, origin.name_id, constraint.range, false);
            if (region_is_unknown(this->facts_, related) || this->region_outlives(related, constraint.region)) {
                continue;
            }
            this->add_violation(LifetimeViolationKind::type_outlives, constraint.region, related, constraint.type,
                syntax::INVALID_EXPR_ID, false, constraint.range);
        }
    }
}

void SemanticAnalyzerCore::LifetimeAnalyzer::enforce_diagnostics()
{
    for (LifetimeViolation& violation : this->facts_.violations) {
        if (violation.diagnostic_emitted) {
            continue;
        }

        switch (violation.kind) {
            case LifetimeViolationKind::unknown_origin:
                this->core_.report_general(violation.range, std::string(SEMA_LIFETIME_UNKNOWN_ORIGIN));
                violation.diagnostic_emitted = true;
                break;
            case LifetimeViolationKind::ambiguous_elision:
                this->core_.report_general(violation.range, std::string(SEMA_LIFETIME_ELISION_AMBIGUOUS));
                if (valid_region_index(this->facts_, violation.region)) {
                    this->core_.report_note(this->facts_.regions[violation.region].range,
                        SemanticDiagnosticKind::general, std::string(SEMA_LIFETIME_CONSTRAINT_NOTE));
                }
                if (valid_region_index(this->facts_, violation.related_region)) {
                    this->core_.report_note(this->facts_.regions[violation.related_region].range,
                        SemanticDiagnosticKind::general, std::string(SEMA_LIFETIME_CONSTRAINT_NOTE));
                }
                violation.diagnostic_emitted = true;
                break;
            case LifetimeViolationKind::return_origin_outside_type:
                this->core_.report_general(violation.range, std::string(SEMA_LIFETIME_RETURN_OUTSIDE_TYPE));
                if (valid_region_index(this->facts_, violation.region)) {
                    this->core_.report_note(this->facts_.regions[violation.region].range,
                        SemanticDiagnosticKind::general, std::string(SEMA_LIFETIME_CONSTRAINT_NOTE));
                }
                if (valid_region_index(this->facts_, violation.related_region)) {
                    this->core_.report_note(this->facts_.regions[violation.related_region].range,
                        SemanticDiagnosticKind::general, std::string(SEMA_LIFETIME_ORIGIN_DECLARED_HERE));
                }
                violation.diagnostic_emitted = true;
                break;
            case LifetimeViolationKind::type_outlives:
                this->core_.report_general(violation.range, std::string(SEMA_LIFETIME_TYPE_OUTLIVES));
                if (valid_region_index(this->facts_, violation.related_region)) {
                    this->core_.report_note(this->facts_.regions[violation.related_region].range,
                        SemanticDiagnosticKind::general, std::string(SEMA_LIFETIME_ORIGIN_DECLARED_HERE));
                }
                if (valid_region_index(this->facts_, violation.region)) {
                    this->core_.report_note(this->facts_.regions[violation.region].range,
                        SemanticDiagnosticKind::general, std::string(SEMA_LIFETIME_CONSTRAINT_NOTE));
                }
                violation.diagnostic_emitted = true;
                break;
            case LifetimeViolationKind::local_escape:
                this->core_.report_general(violation.range, std::string(SEMA_BORROWED_LOCAL_ESCAPE));
                if (valid_region_index(this->facts_, violation.region)) {
                    this->core_.report_note(this->facts_.regions[violation.region].range,
                        SemanticDiagnosticKind::general, std::string(SEMA_BORROWED_LOCAL_ORIGIN));
                }
                violation.diagnostic_emitted = true;
                break;
            case LifetimeViolationKind::unknown_escape:
                break;
        }
    }
}

void SemanticAnalyzerCore::LifetimeAnalyzer::add_violation(const LifetimeViolationKind kind, const base::u32 region,
    const base::u32 related_region, const TypeHandle type, const syntax::ExprId expr,
    const bool diagnostic_emitted, const base::SourceRange& range)
{
    const auto existing = std::ranges::find_if(this->facts_.violations, [&](const LifetimeViolation& violation) {
        return lifetime_violation_same_identity(violation, kind, region, related_region, type, expr);
    });
    if (existing != this->facts_.violations.end()) {
        existing->diagnostic_emitted = existing->diagnostic_emitted || diagnostic_emitted;
        return;
    }
    this->facts_.violations.push_back(LifetimeViolation{
        .kind = kind,
        .region = region,
        .related_region = related_region,
        .type = type,
        .expr = expr,
        .diagnostic_emitted = diagnostic_emitted,
        .range = range,
    });
}

void SemanticAnalyzerCore::analyze_signature_lifetimes(
    const syntax::ItemNode& function, const FunctionLookupKey& key, const FunctionSignature& signature)
{
    LifetimeAnalyzer(*this).analyze_signature(function, key, signature);
}

void SemanticAnalyzerCore::analyze_lifetimes(
    const syntax::ItemNode& function, const FunctionLookupKey& key, const FunctionSignature& signature)
{
    LifetimeAnalyzer(*this).analyze_body(function, key, signature);
}

} // namespace aurex::sema
