#include <aurex/frontend/sema/checked_module.hpp>
#include <aurex/infrastructure/query/stable_hash.hpp>

#include <sstream>

#include <frontend/sema/internal/dropck/private/dropck_analysis.hpp>

namespace aurex::sema {
namespace {

constexpr std::string_view SEMA_DROP_CHECK_FACT_FINGERPRINT_MARKER = "sema.dropck.fact.v1";
constexpr std::string_view SEMA_DROP_CHECK_FACTS_FINGERPRINT_MARKER = "sema.dropck.facts.v1";

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

void append_range(std::ostringstream& stream, const base::SourceRange& range)
{
    stream << range.source.value << ':' << range.begin << ':' << range.end;
}

void mix_required_outlives(query::StableHashBuilder& builder, const DropCheckRequiredOutlives& required) noexcept
{
    builder.mix_u32(required.type.value);
    builder.mix_u32(required.region);
    builder.mix_u8(static_cast<base::u8>(required.reason));
}

void mix_drop_action(query::StableHashBuilder& builder, const DropActionFact& action) noexcept
{
    builder.mix_u8(static_cast<base::u8>(action.kind));
    builder.mix_u32(action.action);
    builder.mix_u32(action.point);
    builder.mix_u32(action.place);
    builder.mix_u32(action.type.value);
    builder.mix_fingerprint(action.destructor_key);
}

void mix_drop_violation(query::StableHashBuilder& builder, const DropCheckViolation& violation) noexcept
{
    builder.mix_u8(static_cast<base::u8>(violation.kind));
    builder.mix_u32(violation.action);
    builder.mix_u32(violation.point);
    builder.mix_u32(violation.place);
    builder.mix_u32(violation.type.value);
    builder.mix_u32(violation.region);
    builder.mix_bool(violation.diagnostic_emitted);
}

} // namespace

std::string_view drop_check_action_kind_name(const DropCheckActionKind kind) noexcept
{
    switch (kind) {
        case DropCheckActionKind::lexical_cleanup:
            return "lexical_cleanup";
        case DropCheckActionKind::overwrite:
            return "overwrite";
        case DropCheckActionKind::early_exit:
            return "early_exit";
        case DropCheckActionKind::explicit_drop:
            return "explicit_drop";
        case DropCheckActionKind::defer_cleanup:
            return "defer_cleanup";
    }
    return "<invalid>";
}

std::string_view drop_check_violation_kind_name(const DropCheckViolationKind kind) noexcept
{
    switch (kind) {
        case DropCheckViolationKind::borrowed_drop:
            return "borrowed_drop";
        case DropCheckViolationKind::borrowed_field_dangling:
            return "borrowed_field_dangling";
        case DropCheckViolationKind::generic_type_outlives:
            return "generic_type_outlives";
        case DropCheckViolationKind::destructor_escape:
            return "destructor_escape";
        case DropCheckViolationKind::drop_glue_missing:
            return "drop_glue_missing";
    }
    return "<invalid>";
}

query::StableFingerprint128 drop_check_fact_fingerprint(const DropCheckFact& fact) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(SEMA_DROP_CHECK_FACT_FINGERPRINT_MARKER);
    builder.mix_u32(fact.type.value);
    mix_function_key(builder, fact.destructor_function);
    builder.mix_fingerprint(fact.drop_glue_fingerprint);
    builder.mix_bool(fact.may_observe_fields);
    builder.mix_bool(fact.may_move_fields);
    builder.mix_u64(static_cast<base::u64>(fact.required_outlives.size()));
    for (const DropCheckRequiredOutlives& required : fact.required_outlives) {
        mix_required_outlives(builder, required);
    }
    return builder.finish();
}

query::StableFingerprint128 function_drop_check_facts_fingerprint(const FunctionDropCheckFacts& facts) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(SEMA_DROP_CHECK_FACTS_FINGERPRINT_MARKER);
    mix_function_key(builder, facts.function);
    builder.mix_bool(facts.solved);
    builder.mix_bool(facts.diagnostic_mode_enforced);
    builder.mix_bool(facts.graph_missing);
    builder.mix_u32(facts.part_index);
    builder.mix_u64(static_cast<base::u64>(facts.facts.size()));
    for (const DropCheckFact& fact : facts.facts) {
        builder.mix_fingerprint(drop_check_fact_fingerprint(fact));
    }
    builder.mix_u64(static_cast<base::u64>(facts.actions.size()));
    for (const DropActionFact& action : facts.actions) {
        mix_drop_action(builder, action);
    }
    builder.mix_u64(static_cast<base::u64>(facts.violations.size()));
    for (const DropCheckViolation& violation : facts.violations) {
        mix_drop_violation(builder, violation);
    }
    return builder.finish();
}

std::string dump_function_drop_check_facts(const FunctionDropCheckFacts& facts)
{
    std::ostringstream stream;
    stream << "dropck_facts function=" << facts.function.module << ':' << facts.function.owner_type << ':';
    append_optional_name_id(stream, facts.function.name);
    stream << " facts=" << facts.facts.size() << " actions=" << facts.actions.size()
           << " violations=" << facts.violations.size() << " solved=" << (facts.solved ? "true" : "false")
           << " enforced=" << (facts.diagnostic_mode_enforced ? "true" : "false")
           << " graph_missing=" << (facts.graph_missing ? "true" : "false")
           << " fingerprint=" << query::debug_string(function_drop_check_facts_fingerprint(facts)) << '\n';

    stream << "facts:\n";
    for (base::usize index = 0; index < facts.facts.size(); ++index) {
        const DropCheckFact& fact = facts.facts[index];
        stream << "  f" << index << " type=" << fact.type.value << " required="
               << fact.required_outlives.size() << " observe=" << (fact.may_observe_fields ? "true" : "false")
               << " move=" << (fact.may_move_fields ? "true" : "false")
               << " drop_glue=" << query::debug_string(fact.drop_glue_fingerprint)
               << " fingerprint=" << query::debug_string(drop_check_fact_fingerprint(fact)) << '\n';
        for (base::usize required_index = 0; required_index < fact.required_outlives.size(); ++required_index) {
            const DropCheckRequiredOutlives& required = fact.required_outlives[required_index];
            stream << "    req" << required_index << " type=" << required.type.value << " : r" << required.region
                   << " reason=" << lifetime_constraint_reason_name(required.reason) << " range=";
            append_range(stream, required.range);
            stream << '\n';
        }
    }

    stream << "actions:\n";
    for (base::usize index = 0; index < facts.actions.size(); ++index) {
        const DropActionFact& action = facts.actions[index];
        stream << "  a" << index << ' ' << drop_check_action_kind_name(action.kind) << " flow=a"
               << action.action << "/p" << action.point << " place=" << action.place << " type="
               << action.type.value << " destructor=" << query::debug_string(action.destructor_key) << " range=";
        append_range(stream, action.range);
        stream << '\n';
    }

    stream << "violations:\n";
    for (base::usize index = 0; index < facts.violations.size(); ++index) {
        const DropCheckViolation& violation = facts.violations[index];
        stream << "  v" << index << ' ' << drop_check_violation_kind_name(violation.kind) << " action=a"
               << violation.action << " point=p" << violation.point << " place=" << violation.place << " type="
               << violation.type.value << " region=r" << violation.region
               << " emitted=" << (violation.diagnostic_emitted ? "true" : "false") << " range=";
        append_range(stream, violation.range);
        stream << '\n';
    }
    return stream.str();
}

} // namespace aurex::sema
