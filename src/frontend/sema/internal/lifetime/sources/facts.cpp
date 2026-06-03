#include <aurex/frontend/sema/checked_module.hpp>
#include <aurex/infrastructure/query/stable_hash.hpp>

#include <sstream>

namespace aurex::sema {
namespace {

constexpr std::string_view SEMA_LIFETIME_FACTS_FINGERPRINT_MARKER = "sema.lifetime_facts.v1";
constexpr std::string_view SEMA_TYPE_LIFETIME_INFO_FINGERPRINT_MARKER = "sema.type_lifetime_info.v1";
constexpr std::string_view SEMA_GENERIC_LIFETIME_PREDICATE_FINGERPRINT_MARKER =
    "sema.generic_lifetime_predicate.v1";

void mix_function_key(query::StableHashBuilder& builder, const FunctionLookupKey key) noexcept
{
    builder.mix_u32(key.module);
    builder.mix_u32(key.owner_type);
    builder.mix_u32(key.name.value);
}

void append_optional_name_id(std::ostringstream& stream, const IdentId name)
{
    if (syntax::is_valid(name)) {
        stream << '#' << name.value;
        return;
    }
    stream << '-';
}

void append_optional_expr_id(std::ostringstream& stream, const syntax::ExprId expr)
{
    if (syntax::is_valid(expr)) {
        stream << 'e' << expr.value;
        return;
    }
    stream << '-';
}

void append_range(std::ostringstream& stream, const base::SourceRange& range)
{
    stream << range.source.value << ':' << range.begin << ':' << range.end;
}

} // namespace

std::string_view lifetime_region_kind_name(const LifetimeRegionKind kind) noexcept
{
    switch (kind) {
        case LifetimeRegionKind::parameter:
            return "parameter";
        case LifetimeRegionKind::self:
            return "self";
        case LifetimeRegionKind::static_:
            return "static";
        case LifetimeRegionKind::explicit_origin:
            return "explicit_origin";
        case LifetimeRegionKind::inferred:
            return "inferred";
        case LifetimeRegionKind::local:
            return "local";
        case LifetimeRegionKind::temporary:
            return "temporary";
        case LifetimeRegionKind::unknown:
            return "unknown";
    }
    return "<invalid>";
}

std::string_view lifetime_constraint_reason_name(const LifetimeConstraintReason reason) noexcept
{
    switch (reason) {
        case LifetimeConstraintReason::declared_origin:
            return "declared_origin";
        case LifetimeConstraintReason::reference_type:
            return "reference_type";
        case LifetimeConstraintReason::return_contract:
            return "return_contract";
        case LifetimeConstraintReason::return_type:
            return "return_type";
        case LifetimeConstraintReason::call:
            return "call";
        case LifetimeConstraintReason::reborrow:
            return "reborrow";
        case LifetimeConstraintReason::dropck:
            return "dropck";
    }
    return "<invalid>";
}

std::string_view lifetime_violation_kind_name(const LifetimeViolationKind kind) noexcept
{
    switch (kind) {
        case LifetimeViolationKind::unknown_origin:
            return "unknown_origin";
        case LifetimeViolationKind::ambiguous_elision:
            return "ambiguous_elision";
        case LifetimeViolationKind::return_origin_outside_type:
            return "return_origin_outside_type";
        case LifetimeViolationKind::local_escape:
            return "local_escape";
        case LifetimeViolationKind::unknown_escape:
            return "unknown_escape";
        case LifetimeViolationKind::type_outlives:
            return "type_outlives";
    }
    return "<invalid>";
}

std::string_view generic_lifetime_predicate_source_name(const GenericLifetimePredicateSource source) noexcept
{
    switch (source) {
        case GenericLifetimePredicateSource::explicit_origin:
            return "explicit_origin";
        case GenericLifetimePredicateSource::inferred_reference:
            return "inferred_reference";
        case GenericLifetimePredicateSource::associated_projection:
            return "associated_projection";
    }
    return "<invalid>";
}

query::StableFingerprint128 type_lifetime_info_fingerprint(const TypeLifetimeInfo& info) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(SEMA_TYPE_LIFETIME_INFO_FINGERPRINT_MARKER);
    builder.mix_u32(info.type.value);
    builder.mix_bool(info.can_contain_borrow);
    builder.mix_bool(info.has_concrete_borrow_surface);
    builder.mix_u32(info.part_index);
    builder.mix_u64(static_cast<base::u64>(info.origin_names.size()));
    for (const InternedText origin : info.origin_names) {
        builder.mix_string(origin.view());
    }
    return builder.finish();
}

query::StableFingerprint128 generic_lifetime_predicate_fingerprint(
    const GenericLifetimePredicate& predicate) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(SEMA_GENERIC_LIFETIME_PREDICATE_FINGERPRINT_MARKER);
    builder.mix_u32(predicate.subject_type.value);
    builder.mix_string(predicate.origin_name.view());
    builder.mix_u32(predicate.origin_name_id.value);
    builder.mix_u8(static_cast<base::u8>(predicate.source));
    builder.mix_u32(predicate.part_index);
    return builder.finish();
}

query::StableFingerprint128 function_lifetime_facts_fingerprint(const FunctionLifetimeFacts& facts) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(SEMA_LIFETIME_FACTS_FINGERPRINT_MARKER);
    mix_function_key(builder, facts.function);
    builder.mix_u32(facts.return_type.value);
    builder.mix_bool(facts.solved);
    builder.mix_bool(facts.diagnostic_mode_enforced);
    builder.mix_u32(facts.part_index);
    builder.mix_u64(static_cast<base::u64>(facts.regions.size()));
    for (const LifetimeRegion& region : facts.regions) {
        builder.mix_u8(static_cast<base::u8>(region.kind));
        builder.mix_u32(region.name_id.value);
        builder.mix_string(region.name.view());
        builder.mix_u32(region.param_index);
    }
    builder.mix_u64(static_cast<base::u64>(facts.outlives_constraints.size()));
    for (const LifetimeOutlivesConstraint& constraint : facts.outlives_constraints) {
        builder.mix_u32(constraint.longer_region);
        builder.mix_u32(constraint.shorter_region);
        builder.mix_u8(static_cast<base::u8>(constraint.reason));
    }
    builder.mix_u64(static_cast<base::u64>(facts.type_outlives_constraints.size()));
    for (const LifetimeTypeOutlivesConstraint& constraint : facts.type_outlives_constraints) {
        builder.mix_u32(constraint.type.value);
        builder.mix_u32(constraint.region);
        builder.mix_u8(static_cast<base::u8>(constraint.reason));
    }
    builder.mix_u64(static_cast<base::u64>(facts.live_ranges.size()));
    for (const LifetimeRegionLiveRange& live_range : facts.live_ranges) {
        builder.mix_u32(live_range.region);
        builder.mix_u32(live_range.first_point);
        builder.mix_u32(live_range.last_point);
        builder.mix_u32(live_range.point_count);
    }
    builder.mix_u64(static_cast<base::u64>(facts.return_regions.size()));
    for (const LifetimeReturnRegion& returned : facts.return_regions) {
        builder.mix_u32(returned.region);
        builder.mix_u32(returned.return_expr.value);
    }
    builder.mix_u64(static_cast<base::u64>(facts.violations.size()));
    for (const LifetimeViolation& violation : facts.violations) {
        builder.mix_u8(static_cast<base::u8>(violation.kind));
        builder.mix_u32(violation.region);
        builder.mix_u32(violation.related_region);
        builder.mix_u32(violation.type.value);
        builder.mix_u32(violation.expr.value);
        builder.mix_bool(violation.diagnostic_emitted);
    }
    return builder.finish();
}

std::string dump_function_lifetime_facts(const FunctionLifetimeFacts& facts)
{
    std::ostringstream stream;
    stream << "lifetime_facts function=" << facts.function.module << ':' << facts.function.owner_type << ':';
    append_optional_name_id(stream, facts.function.name);
    stream << " return_type=" << facts.return_type.value << " regions=" << facts.regions.size()
           << " outlives=" << facts.outlives_constraints.size()
           << " type_outlives=" << facts.type_outlives_constraints.size()
           << " live_ranges=" << facts.live_ranges.size()
           << " returns=" << facts.return_regions.size() << " violations=" << facts.violations.size()
           << " solved=" << (facts.solved ? "true" : "false")
           << " enforced=" << (facts.diagnostic_mode_enforced ? "true" : "false")
           << " fingerprint=" << query::debug_string(function_lifetime_facts_fingerprint(facts)) << '\n';

    stream << "regions:\n";
    for (base::usize index = 0; index < facts.regions.size(); ++index) {
        const LifetimeRegion& region = facts.regions[index];
        stream << "  r" << index << ' ' << lifetime_region_kind_name(region.kind) << " param="
               << region.param_index << " name=";
        if (!region.name.empty()) {
            stream << region.name;
        } else {
            append_optional_name_id(stream, region.name_id);
        }
        stream << " range=";
        append_range(stream, region.range);
        stream << '\n';
    }

    stream << "outlives:\n";
    for (base::usize index = 0; index < facts.outlives_constraints.size(); ++index) {
        const LifetimeOutlivesConstraint& constraint = facts.outlives_constraints[index];
        stream << "  o" << index << " r" << constraint.longer_region << " : r" << constraint.shorter_region
               << " reason=" << lifetime_constraint_reason_name(constraint.reason) << " range=";
        append_range(stream, constraint.range);
        stream << '\n';
    }

    stream << "type_outlives:\n";
    for (base::usize index = 0; index < facts.type_outlives_constraints.size(); ++index) {
        const LifetimeTypeOutlivesConstraint& constraint = facts.type_outlives_constraints[index];
        stream << "  t" << index << " type=" << constraint.type.value << " : r" << constraint.region
               << " reason=" << lifetime_constraint_reason_name(constraint.reason) << " range=";
        append_range(stream, constraint.range);
        stream << '\n';
    }

    stream << "live_ranges:\n";
    for (base::usize index = 0; index < facts.live_ranges.size(); ++index) {
        const LifetimeRegionLiveRange& live_range = facts.live_ranges[index];
        stream << "  l" << index << " r" << live_range.region << " first=p" << live_range.first_point
               << " last=p" << live_range.last_point << " points=" << live_range.point_count << " range=";
        append_range(stream, live_range.range);
        stream << '\n';
    }

    stream << "returns:\n";
    for (base::usize index = 0; index < facts.return_regions.size(); ++index) {
        const LifetimeReturnRegion& returned = facts.return_regions[index];
        stream << "  ret" << index << " r" << returned.region << " expr=";
        append_optional_expr_id(stream, returned.return_expr);
        stream << " range=";
        append_range(stream, returned.range);
        stream << '\n';
    }

    stream << "violations:\n";
    for (base::usize index = 0; index < facts.violations.size(); ++index) {
        const LifetimeViolation& violation = facts.violations[index];
        stream << "  v" << index << ' ' << lifetime_violation_kind_name(violation.kind)
               << " region=r" << violation.region << " related=r" << violation.related_region << " type="
               << violation.type.value << " expr=";
        append_optional_expr_id(stream, violation.expr);
        stream << " emitted=" << (violation.diagnostic_emitted ? "true" : "false") << " range=";
        append_range(stream, violation.range);
        stream << '\n';
    }
    return stream.str();
}

} // namespace aurex::sema
