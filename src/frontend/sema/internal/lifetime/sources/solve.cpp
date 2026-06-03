#include <algorithm>
#include <tuple>

#include <frontend/sema/internal/lifetime/private/lifetime_analysis.hpp>

namespace aurex::sema {
namespace {

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
    this->initialize_outlives_matrix();
    this->close_outlives_matrix();
    this->facts_.solved = true;
}

void SemanticAnalyzerCore::LifetimeAnalyzer::initialize_outlives_matrix()
{
    const base::usize region_count = this->facts_.regions.size();
    this->outlives_.assign(region_count, std::vector<bool>(region_count, false));
    for (base::usize index = 0; index < region_count; ++index) {
        this->outlives_[index][index] = true;
    }
    for (const LifetimeOutlivesConstraint& constraint : this->facts_.outlives_constraints) {
        this->outlives_[constraint.longer_region][constraint.shorter_region] = true;
    }
}

void SemanticAnalyzerCore::LifetimeAnalyzer::close_outlives_matrix()
{
    const base::usize region_count = this->outlives_.size();
    for (base::usize middle = 0; middle < region_count; ++middle) {
        for (base::usize longer = 0; longer < region_count; ++longer) {
            if (!this->outlives_[longer][middle]) {
                continue;
            }
            for (base::usize shorter = 0; shorter < region_count; ++shorter) {
                this->outlives_[longer][shorter] = this->outlives_[longer][shorter]
                    || this->outlives_[middle][shorter];
            }
        }
    }
}

bool SemanticAnalyzerCore::LifetimeAnalyzer::region_outlives(
    const base::u32 longer, const base::u32 shorter) const noexcept
{
    return this->outlives_[longer][shorter];
}

} // namespace aurex::sema
