#include <aurex/frontend/lex/lexer.hpp>
#include <aurex/frontend/parse/parser.hpp>
#include <aurex/frontend/sema/checked_module.hpp>
#include <aurex/frontend/sema/sema.hpp>
#include <aurex/frontend/sema/sema_messages.hpp>
#include <aurex/infrastructure/base/diagnostic.hpp>
#include <aurex/infrastructure/query/type_check_body_query.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <frontend/sema/internal/core/private/sema_core.hpp>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

constexpr base::SourceId LIFETIME_TEST_SOURCE_ID{707};
constexpr syntax::ModuleId LIFETIME_TEST_MODULE_ID{0U};
constexpr syntax::ItemId LIFETIME_TEST_ITEM_ID{0U};
constexpr sema::IdentId LIFETIME_TEST_FUNCTION_NAME_ID{41U};
constexpr sema::IdentId LIFETIME_TEST_VALUE_NAME_ID{42U};
constexpr base::u32 LIFETIME_TEST_OUT_OF_RANGE_INDEX = 99U;

struct LifetimeWhiteboxHarness {
    syntax::AstModule module;
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzerCore analyzer;
    syntax::ItemNode function;
    sema::FunctionSignature signature;
    sema::FunctionLookupKey key;

    LifetimeWhiteboxHarness() noexcept : analyzer(module, diagnostics)
    {
    }

    LifetimeWhiteboxHarness(const LifetimeWhiteboxHarness&) = delete;
    LifetimeWhiteboxHarness& operator=(const LifetimeWhiteboxHarness&) = delete;
};

[[nodiscard]] syntax::AstModule parse_lifetime_source(const std::string_view source)
{
    base::DiagnosticSink diagnostics;
    lex::Lexer lexer(LIFETIME_TEST_SOURCE_ID, source, diagnostics);
    auto tokens = lexer.tokenize();
    if (!tokens) {
        ADD_FAILURE() << tokens.error().message;
        return {};
    }

    parse::Parser parser(tokens.value(), diagnostics);
    auto parsed = parser.parse_module();
    if (!parsed) {
        ADD_FAILURE() << parsed.error().message;
        return {};
    }
    if (diagnostics.has_error()) {
        for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
            ADD_FAILURE() << diagnostic.message;
        }
        return {};
    }
    return parsed.take_value();
}

[[nodiscard]] sema::CheckedModule analyze_lifetime_source(const std::string_view source)
{
    syntax::AstModule module = parse_lifetime_source(source);
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(std::move(module), diagnostics);
    auto result = analyzer.analyze();
    if (!result) {
        for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
            ADD_FAILURE() << diagnostic.message;
        }
        ADD_FAILURE() << result.error().message;
        return {};
    }
    return result.take_value();
}

[[nodiscard]] std::string analyze_lifetime_source_failure(const std::string_view source)
{
    syntax::AstModule module = parse_lifetime_source(source);
    base::DiagnosticSink diagnostics;
    sema::SemanticAnalyzer analyzer(std::move(module), diagnostics);
    auto result = analyzer.analyze();

    std::string output;
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        output += diagnostic.message;
        output += '\n';
    }
    if (result) {
        ADD_FAILURE() << "expected semantic analysis to fail";
    } else {
        output += result.error().message;
        output += '\n';
    }
    return output;
}

[[nodiscard]] const sema::FunctionLifetimeFacts* find_lifetime_facts(
    const sema::CheckedModule& checked, const std::string_view function_name)
{
    for (const auto& entry : checked.functions) {
        if (entry.second.name.view() != function_name) {
            continue;
        }
        const auto facts = checked.lifetime_facts.find(entry.first);
        if (facts != checked.lifetime_facts.end()) {
            return &facts->second;
        }
    }
    return nullptr;
}

[[nodiscard]] bool lifetime_facts_has_region_kind(
    const sema::FunctionLifetimeFacts& facts, const sema::LifetimeRegionKind kind) noexcept
{
    return std::ranges::any_of(facts.regions, [kind](const sema::LifetimeRegion& region) {
        return region.kind == kind;
    });
}

void expect_lifetime_diagnostic(const std::string_view source, const std::string_view diagnostic)
{
    const std::string output = analyze_lifetime_source_failure(source);
    EXPECT_NE(output.find(diagnostic), std::string::npos) << output;
}

void configure_lifetime_whitebox_harness(LifetimeWhiteboxHarness& harness, const std::string_view name)
{
    harness.function = {};
    harness.signature = {};
    harness.key = sema::FunctionLookupKey{
        LIFETIME_TEST_MODULE_ID.value,
        sema::SEMA_LOOKUP_INVALID_KEY_PART,
        LIFETIME_TEST_FUNCTION_NAME_ID,
    };

    const sema::TypeHandle i32 = harness.analyzer.state_.checked.types.builtin(sema::BuiltinType::i32);
    const sema::TypeHandle ref_i32 =
        harness.analyzer.state_.checked.types.reference(sema::PointerMutability::const_, i32);

    harness.function.kind = syntax::ItemKind::fn_decl;
    harness.function.name = name;
    harness.function.name_id = LIFETIME_TEST_FUNCTION_NAME_ID;
    harness.function.range = base::SourceRange{LIFETIME_TEST_SOURCE_ID, 1U, 20U};
    harness.function.visibility = syntax::Visibility::private_;
    harness.function.params.push_back(syntax::ParamDecl{
        .name = "value",
        .type = syntax::INVALID_TYPE_ID,
        .range = base::SourceRange{LIFETIME_TEST_SOURCE_ID, 21U, 26U},
        .name_id = LIFETIME_TEST_VALUE_NAME_ID,
    });

    harness.signature.name = harness.analyzer.state_.checked.intern_text(name);
    harness.signature.name_id = LIFETIME_TEST_FUNCTION_NAME_ID;
    harness.signature.semantic_key = harness.key;
    harness.signature.module = LIFETIME_TEST_MODULE_ID;
    harness.signature.return_type = ref_i32;
    harness.signature.param_types.push_back(ref_i32);
    harness.signature.range = harness.function.range;
    harness.signature.visibility = syntax::Visibility::private_;
    harness.signature.has_definition = true;
    harness.signature.definition_item = LIFETIME_TEST_ITEM_ID;
}

[[nodiscard]] bool lifetime_facts_has_violation_kind(
    const sema::FunctionLifetimeFacts& facts, const sema::LifetimeViolationKind kind) noexcept
{
    return std::ranges::any_of(facts.violations, [kind](const sema::LifetimeViolation& violation) {
        return violation.kind == kind;
    });
}

[[nodiscard]] bool lifetime_facts_has_emitted_violation_kind(
    const sema::FunctionLifetimeFacts& facts, const sema::LifetimeViolationKind kind) noexcept
{
    return std::ranges::any_of(facts.violations, [kind](const sema::LifetimeViolation& violation) {
        return violation.kind == kind && violation.diagnostic_emitted;
    });
}

} // namespace

TEST(CoreUnit, LifetimeFactsNamesFingerprintsAndDumpsAreStable)
{
    sema::CheckedModule checked;
    sema::FunctionLifetimeFacts facts;
    facts.function = sema::FunctionLookupKey{1U, sema::SEMA_LOOKUP_INVALID_KEY_PART, sema::IdentId{2U}};
    facts.return_type = sema::TypeHandle{3U};
    facts.solved = true;
    facts.diagnostic_mode_enforced = true;
    facts.regions.push_back(sema::LifetimeRegion{
        .kind = sema::LifetimeRegionKind::explicit_origin,
        .name_id = sema::IdentId{4U},
        .name = checked.intern_text("data"),
        .param_index = sema::SEMA_LIFETIME_INVALID_INDEX,
        .range = base::SourceRange{LIFETIME_TEST_SOURCE_ID, 10U, 14U},
    });
    facts.regions.push_back(sema::LifetimeRegion{
        .kind = sema::LifetimeRegionKind::inferred,
        .name_id = sema::IdentId{5U},
        .name = {},
        .param_index = 0U,
        .range = base::SourceRange{LIFETIME_TEST_SOURCE_ID, 15U, 18U},
    });
    facts.regions.push_back(sema::LifetimeRegion{
        .kind = sema::LifetimeRegionKind::local,
        .name = {},
        .param_index = sema::SEMA_LIFETIME_INVALID_INDEX,
        .range = base::SourceRange{LIFETIME_TEST_SOURCE_ID, 19U, 21U},
    });
    facts.outlives_constraints.push_back(sema::LifetimeOutlivesConstraint{
        .longer_region = 0U,
        .shorter_region = 1U,
        .reason = sema::LifetimeConstraintReason::return_contract,
        .range = base::SourceRange{LIFETIME_TEST_SOURCE_ID, 22U, 26U},
    });
    facts.type_outlives_constraints.push_back(sema::LifetimeTypeOutlivesConstraint{
        .type = sema::TypeHandle{8U},
        .region = 1U,
        .reason = sema::LifetimeConstraintReason::dropck,
        .range = base::SourceRange{LIFETIME_TEST_SOURCE_ID, 27U, 31U},
    });
    facts.live_ranges.push_back(sema::LifetimeRegionLiveRange{
        .region = 1U,
        .first_point = 2U,
        .last_point = 7U,
        .point_count = 4U,
        .range = base::SourceRange{LIFETIME_TEST_SOURCE_ID, 31U, 32U},
    });
    facts.return_regions.push_back(sema::LifetimeReturnRegion{
        .region = 1U,
        .return_expr = syntax::ExprId{9U},
        .range = base::SourceRange{LIFETIME_TEST_SOURCE_ID, 32U, 36U},
    });
    facts.violations.push_back(sema::LifetimeViolation{
        .kind = sema::LifetimeViolationKind::unknown_origin,
        .region = 0U,
        .related_region = sema::SEMA_LIFETIME_INVALID_INDEX,
        .type = sema::INVALID_TYPE_HANDLE,
        .expr = syntax::INVALID_EXPR_ID,
        .diagnostic_emitted = true,
        .range = base::SourceRange{LIFETIME_TEST_SOURCE_ID, 20U, 24U},
    });
    facts.violations.push_back(sema::LifetimeViolation{
        .kind = sema::LifetimeViolationKind::type_outlives,
        .region = 1U,
        .related_region = 0U,
        .type = sema::TypeHandle{8U},
        .expr = syntax::ExprId{10U},
        .diagnostic_emitted = false,
        .range = base::SourceRange{LIFETIME_TEST_SOURCE_ID, 40U, 44U},
    });

    EXPECT_EQ(sema::lifetime_region_kind_name(sema::LifetimeRegionKind::parameter), "parameter");
    EXPECT_EQ(sema::lifetime_region_kind_name(sema::LifetimeRegionKind::self), "self");
    EXPECT_EQ(sema::lifetime_region_kind_name(sema::LifetimeRegionKind::static_), "static");
    EXPECT_EQ(sema::lifetime_region_kind_name(sema::LifetimeRegionKind::explicit_origin), "explicit_origin");
    EXPECT_EQ(sema::lifetime_region_kind_name(sema::LifetimeRegionKind::inferred), "inferred");
    EXPECT_EQ(sema::lifetime_region_kind_name(sema::LifetimeRegionKind::local), "local");
    EXPECT_EQ(sema::lifetime_region_kind_name(sema::LifetimeRegionKind::temporary), "temporary");
    EXPECT_EQ(sema::lifetime_region_kind_name(sema::LifetimeRegionKind::unknown), "unknown");
    EXPECT_EQ(sema::lifetime_constraint_reason_name(sema::LifetimeConstraintReason::declared_origin),
        "declared_origin");
    EXPECT_EQ(sema::lifetime_constraint_reason_name(sema::LifetimeConstraintReason::reference_type), "reference_type");
    EXPECT_EQ(sema::lifetime_constraint_reason_name(sema::LifetimeConstraintReason::return_contract),
        "return_contract");
    EXPECT_EQ(sema::lifetime_constraint_reason_name(sema::LifetimeConstraintReason::return_type), "return_type");
    EXPECT_EQ(sema::lifetime_constraint_reason_name(sema::LifetimeConstraintReason::call), "call");
    EXPECT_EQ(sema::lifetime_constraint_reason_name(sema::LifetimeConstraintReason::reborrow), "reborrow");
    EXPECT_EQ(sema::lifetime_constraint_reason_name(sema::LifetimeConstraintReason::dropck), "dropck");
    EXPECT_EQ(sema::lifetime_violation_kind_name(sema::LifetimeViolationKind::unknown_origin), "unknown_origin");
    EXPECT_EQ(sema::lifetime_violation_kind_name(sema::LifetimeViolationKind::ambiguous_elision),
        "ambiguous_elision");
    EXPECT_EQ(sema::lifetime_violation_kind_name(sema::LifetimeViolationKind::return_origin_outside_type),
        "return_origin_outside_type");
    EXPECT_EQ(sema::lifetime_violation_kind_name(sema::LifetimeViolationKind::local_escape), "local_escape");
    EXPECT_EQ(sema::lifetime_violation_kind_name(sema::LifetimeViolationKind::unknown_escape), "unknown_escape");
    EXPECT_EQ(sema::lifetime_violation_kind_name(sema::LifetimeViolationKind::type_outlives), "type_outlives");
    EXPECT_EQ(sema::generic_lifetime_predicate_source_name(sema::GenericLifetimePredicateSource::explicit_origin),
        "explicit_origin");
    EXPECT_EQ(sema::generic_lifetime_predicate_source_name(sema::GenericLifetimePredicateSource::inferred_reference),
        "inferred_reference");
    EXPECT_EQ(
        sema::generic_lifetime_predicate_source_name(sema::GenericLifetimePredicateSource::associated_projection),
        "associated_projection");
    EXPECT_EQ(sema::lifetime_region_kind_name(static_cast<sema::LifetimeRegionKind>(255U)), "<invalid>");
    EXPECT_EQ(sema::lifetime_constraint_reason_name(static_cast<sema::LifetimeConstraintReason>(255U)), "<invalid>");
    EXPECT_EQ(sema::lifetime_violation_kind_name(static_cast<sema::LifetimeViolationKind>(255U)), "<invalid>");
    EXPECT_EQ(sema::generic_lifetime_predicate_source_name(
                  static_cast<sema::GenericLifetimePredicateSource>(255U)),
        "<invalid>");

    const query::StableFingerprint128 original = sema::function_lifetime_facts_fingerprint(facts);
    sema::FunctionLifetimeFacts range_changed = facts;
    range_changed.regions.front().range = base::SourceRange{LIFETIME_TEST_SOURCE_ID, 30U, 34U};
    range_changed.violations.front().range = base::SourceRange{LIFETIME_TEST_SOURCE_ID, 40U, 44U};
    EXPECT_EQ(sema::function_lifetime_facts_fingerprint(range_changed), original);

    sema::FunctionLifetimeFacts violation_changed = facts;
    violation_changed.violations.front().kind = sema::LifetimeViolationKind::ambiguous_elision;
    EXPECT_NE(sema::function_lifetime_facts_fingerprint(violation_changed), original);
    sema::FunctionLifetimeFacts live_range_changed = facts;
    live_range_changed.live_ranges.front().last_point = 8U;
    EXPECT_NE(sema::function_lifetime_facts_fingerprint(live_range_changed), original);

    sema::TypeLifetimeInfo type_lifetime;
    type_lifetime.type = sema::TypeHandle{15U};
    type_lifetime.origin_names.push_back(checked.intern_text("data"));
    type_lifetime.can_contain_borrow = true;
    type_lifetime.has_concrete_borrow_surface = true;
    type_lifetime.part_index = 3U;
    const query::StableFingerprint128 type_original = sema::type_lifetime_info_fingerprint(type_lifetime);
    sema::TypeLifetimeInfo type_changed = type_lifetime;
    type_changed.origin_names.front() = checked.intern_text("other");
    EXPECT_NE(sema::type_lifetime_info_fingerprint(type_changed), type_original);

    sema::GenericLifetimePredicate predicate;
    predicate.subject_type = sema::TypeHandle{16U};
    predicate.origin_name = checked.intern_text("data");
    predicate.origin_name_id = sema::IdentId{17U};
    predicate.source = sema::GenericLifetimePredicateSource::explicit_origin;
    predicate.part_index = 4U;
    const query::StableFingerprint128 predicate_original =
        sema::generic_lifetime_predicate_fingerprint(predicate);
    sema::GenericLifetimePredicate predicate_changed = predicate;
    predicate_changed.source = sema::GenericLifetimePredicateSource::associated_projection;
    EXPECT_NE(sema::generic_lifetime_predicate_fingerprint(predicate_changed), predicate_original);

    const std::string dump = sema::dump_function_lifetime_facts(facts);
    EXPECT_NE(dump.find("lifetime_facts"), std::string::npos) << dump;
    EXPECT_NE(dump.find("regions=3"), std::string::npos) << dump;
    EXPECT_NE(dump.find("outlives=1"), std::string::npos) << dump;
    EXPECT_NE(dump.find("type_outlives=1"), std::string::npos) << dump;
    EXPECT_NE(dump.find("live_ranges=1"), std::string::npos) << dump;
    EXPECT_NE(dump.find("returns=1"), std::string::npos) << dump;
    EXPECT_NE(dump.find("violations=2"), std::string::npos) << dump;
    EXPECT_NE(dump.find("name=#5"), std::string::npos) << dump;
    EXPECT_NE(dump.find("name=-"), std::string::npos) << dump;
    EXPECT_NE(dump.find("reason=return_contract"), std::string::npos) << dump;
    EXPECT_NE(dump.find("reason=dropck"), std::string::npos) << dump;
    EXPECT_NE(dump.find("first=p2"), std::string::npos) << dump;
    EXPECT_NE(dump.find("expr=e9"), std::string::npos) << dump;
    EXPECT_NE(dump.find("fingerprint="), std::string::npos) << dump;

    sema::FunctionLifetimeFacts inactive = facts;
    inactive.solved = false;
    inactive.diagnostic_mode_enforced = false;
    const std::string inactive_dump = sema::dump_function_lifetime_facts(inactive);
    EXPECT_NE(inactive_dump.find("solved=false"), std::string::npos) << inactive_dump;
    EXPECT_NE(inactive_dump.find("enforced=false"), std::string::npos) << inactive_dump;
}

TEST(CoreUnit, LifetimeFactsPopulateTypeCheckBodyAuthorityFlags)
{
    sema::CheckedModule checked;
    const sema::FunctionLookupKey key{1U, sema::SEMA_LOOKUP_INVALID_KEY_PART, sema::IdentId{2U}};

    sema::FunctionBorrowSummary summary;
    summary.function = key;
    summary.return_type = sema::TypeHandle{3U};
    summary.origins.push_back(sema::BorrowSummaryOrigin{.kind = sema::BorrowSummaryOriginKind::local});
    summary.return_origins.push_back(sema::FunctionBorrowReturnOrigin{.origin_index = 0U});
    summary.has_unknown_return_origin = true;
    summary.has_local_return_escape = true;
    summary.fingerprint = query::stable_fingerprint("lifetime-test-borrow-summary");
    checked.borrow_summaries[key] = summary;

    sema::FunctionBorrowContract contract;
    contract.function = key;
    contract.return_type = sema::TypeHandle{3U};
    contract.return_selectors.push_back(sema::BorrowContractSelector{
        .kind = sema::BorrowContractSelectorKind::unknown,
        .param_index = sema::SEMA_BORROW_SUMMARY_INVALID_INDEX,
    });
    contract.unknown_return_allowed = true;
    contract.has_local_return_escape = true;
    contract.has_contract_mismatch = true;
    contract.fingerprint = sema::function_borrow_contract_fingerprint(contract);
    checked.borrow_contracts[key] = contract;

    sema::FunctionLifetimeFacts facts;
    facts.function = key;
    facts.return_type = sema::TypeHandle{3U};
    sema::LifetimeRegion unknown_region;
    unknown_region.kind = sema::LifetimeRegionKind::unknown;
    unknown_region.range = base::SourceRange{LIFETIME_TEST_SOURCE_ID, 1U, 2U};
    facts.regions.push_back(unknown_region);
    facts.outlives_constraints.push_back(sema::LifetimeOutlivesConstraint{
        .longer_region = 0U,
        .shorter_region = 0U,
        .reason = sema::LifetimeConstraintReason::declared_origin,
    });
    facts.type_outlives_constraints.push_back(sema::LifetimeTypeOutlivesConstraint{
        .type = sema::TypeHandle{4U},
        .region = 0U,
        .reason = sema::LifetimeConstraintReason::reference_type,
    });
    facts.live_ranges.push_back(sema::LifetimeRegionLiveRange{
        .region = 0U,
        .first_point = 1U,
        .last_point = 2U,
        .point_count = 3U,
    });
    facts.return_regions.push_back(sema::LifetimeReturnRegion{.region = 0U});
    facts.violations = {
        sema::LifetimeViolation{.kind = sema::LifetimeViolationKind::unknown_origin},
        sema::LifetimeViolation{.kind = sema::LifetimeViolationKind::ambiguous_elision},
        sema::LifetimeViolation{.kind = sema::LifetimeViolationKind::return_origin_outside_type},
        sema::LifetimeViolation{.kind = sema::LifetimeViolationKind::local_escape},
        sema::LifetimeViolation{.kind = sema::LifetimeViolationKind::unknown_escape},
    };
    facts.violations.front().diagnostic_emitted = true;
    facts.fingerprint = sema::function_lifetime_facts_fingerprint(facts);
    checked.lifetime_facts[key] = facts;

    sema::TypeLifetimeInfo type_lifetime;
    type_lifetime.type = sema::TypeHandle{3U};
    checked.type_lifetime_infos.push_back(type_lifetime);
    sema::GenericLifetimePredicate generic_lifetime;
    generic_lifetime.subject_type = sema::TypeHandle{3U};
    checked.generic_lifetime_predicates.push_back(generic_lifetime);

    sema::BodyLoanCheckResult loan;
    loan.function = key;
    loan.graph_missing = true;
    sema::BodyLoan direct_loan;
    direct_loan.parent_loan = sema::SEMA_BODY_LOAN_INVALID_INDEX;
    loan.loans.push_back(direct_loan);
    sema::BodyLoan reborrow;
    reborrow.parent_loan = 0U;
    loan.loans.push_back(reborrow);
    sema::BodyTwoPhaseBorrow two_phase;
    two_phase.diagnostic_emitted = true;
    loan.two_phase_borrows.push_back(two_phase);
    sema::BodyLoanConflict conflict;
    conflict.diagnostic_emitted = true;
    loan.conflicts.push_back(conflict);
    checked.body_loan_checks[key] = loan;

    query::TypeCheckBodyAuthority authority;
    authority.checked_body = query::query_result_fingerprint(query::stable_fingerprint("checked"));
    authority.body_syntax_result = query::query_result_fingerprint(query::stable_fingerprint("body"));
    authority.signature_result = query::query_result_fingerprint(query::stable_fingerprint("signature"));

    sema::populate_type_check_body_borrow_authority(authority, checked, key);

    EXPECT_TRUE(authority.has_borrow_summary);
    EXPECT_EQ(authority.borrow_summary_origin_count, 1U);
    EXPECT_EQ(authority.borrow_summary_dependency_count, 1U);
    EXPECT_TRUE(authority.borrow_summary_has_unknown_return_origin);
    EXPECT_TRUE(authority.borrow_summary_has_local_return_escape);
    EXPECT_TRUE(authority.has_borrow_contract);
    EXPECT_EQ(authority.borrow_contract_selector_count, 1U);
    EXPECT_TRUE(authority.borrow_contract_unknown_return_allowed);
    EXPECT_TRUE(authority.borrow_contract_has_local_return_escape);
    EXPECT_TRUE(authority.borrow_contract_has_mismatch);
    EXPECT_TRUE(authority.has_lifetime_facts);
    EXPECT_EQ(authority.lifetime_region_count, 1U);
    EXPECT_EQ(authority.lifetime_outlives_constraint_count, 1U);
    EXPECT_EQ(authority.lifetime_type_outlives_constraint_count, 1U);
    EXPECT_EQ(authority.lifetime_live_range_count, 1U);
    EXPECT_EQ(authority.lifetime_return_region_count, 1U);
    EXPECT_EQ(authority.lifetime_violation_count, 5U);
    EXPECT_EQ(authority.type_lifetime_info_count, 1U);
    EXPECT_EQ(authority.generic_lifetime_predicate_count, 1U);
    EXPECT_TRUE(authority.lifetime_has_emitted_diagnostics);
    EXPECT_TRUE(authority.lifetime_has_unknown_origin);
    EXPECT_TRUE(authority.lifetime_has_ambiguous_elision);
    EXPECT_TRUE(authority.lifetime_has_return_origin_mismatch);
    EXPECT_TRUE(authority.lifetime_has_local_escape);
    EXPECT_TRUE(authority.lifetime_has_unknown_escape);
    EXPECT_TRUE(authority.has_body_loan_check);
    EXPECT_EQ(authority.body_loan_count, 2U);
    EXPECT_EQ(authority.body_reborrow_count, 1U);
    EXPECT_EQ(authority.body_two_phase_borrow_count, 1U);
    EXPECT_EQ(authority.body_loan_conflict_count, 1U);
    EXPECT_TRUE(authority.body_loan_graph_missing);
    EXPECT_TRUE(authority.body_loan_has_emitted_diagnostics);
    EXPECT_TRUE(authority.body_two_phase_has_emitted_diagnostics);
    EXPECT_TRUE(query::is_valid(query::type_check_body_result_fingerprint(authority)));
}

TEST(CoreUnit, LifetimeFactsAcceptValidExplicitOrigin)
{
    constexpr std::string_view source = "module lifetime.valid;\n"
                                        "fn id[origin data](value: &[data] i32) -> &[data] i32 {\n"
                                        "  return value;\n"
                                        "}\n"
                                        "fn main() -> void {}\n";
    const sema::CheckedModule checked = analyze_lifetime_source(source);
    const sema::FunctionLifetimeFacts* const facts = find_lifetime_facts(checked, "id");
    ASSERT_NE(facts, nullptr);
    EXPECT_TRUE(facts->solved);
    EXPECT_EQ(facts->violations.size(), 0U);
    EXPECT_EQ(facts->return_regions.size(), 1U);
    EXPECT_TRUE(lifetime_facts_has_region_kind(*facts, sema::LifetimeRegionKind::explicit_origin));
    EXPECT_GT(checked.type_lifetime_infos.size(), 0U);
    EXPECT_GT(checked.generic_lifetime_predicates.size(), 0U);

    const std::string checked_dump = sema::dump_checked_module(checked);
    EXPECT_NE(checked_dump.find("lifetime_facts"), std::string::npos) << checked_dump;
    EXPECT_NE(checked_dump.find("type_lifetime_infos"), std::string::npos) << checked_dump;
    EXPECT_NE(checked_dump.find("generic_lifetime_predicates"), std::string::npos) << checked_dump;
    EXPECT_NE(checked_dump.find("explicit_origin"), std::string::npos) << checked_dump;
    EXPECT_NE(checked_dump.find("violations=0"), std::string::npos) << checked_dump;

    sema::CheckedModule copied = checked;
    const sema::FunctionLifetimeFacts* const copied_facts = find_lifetime_facts(copied, "id");
    ASSERT_NE(copied_facts, nullptr);
    EXPECT_EQ(copied_facts->fingerprint, facts->fingerprint);
    const std::string copied_dump = sema::dump_checked_module(copied);
    EXPECT_NE(copied_dump.find("reference_origin_facts"), std::string::npos) << copied_dump;
    EXPECT_NE(copied_dump.find("lifetime_facts"), std::string::npos) << copied_dump;
    EXPECT_NE(copied_dump.find("name=data"), std::string::npos) << copied_dump;
}

TEST(CoreUnit, LifetimeFactsCollectBodyLiveRangesForBorrowedReturn)
{
    constexpr std::string_view source = "module lifetime.live_ranges;\n"
                                        "fn id(value: &i32) -> &i32 {\n"
                                        "  return value;\n"
                                        "}\n"
                                        "fn main() -> void {}\n";
    const sema::CheckedModule checked = analyze_lifetime_source(source);
    const sema::FunctionLifetimeFacts* const facts = find_lifetime_facts(checked, "id");
    ASSERT_NE(facts, nullptr);
    EXPECT_TRUE(facts->solved);
    EXPECT_GT(facts->live_ranges.size(), 0U);

    const std::string checked_dump = sema::dump_checked_module(checked);
    EXPECT_NE(checked_dump.find("live_ranges="), std::string::npos) << checked_dump;
    EXPECT_NE(sema::dump_function_lifetime_facts(*facts).find("live_ranges:"), std::string::npos);
}

TEST(CoreUnit, LifetimeFactsCollectEnumOrPatternBorrowSummaryOrigins)
{
    constexpr std::string_view source = "module lifetime.pattern_summary;\n"
                                        "enum MaybeBorrow {\n"
                                        "  some(&i32),\n"
                                        "  other(&i32),\n"
                                        "  none,\n"
                                        "}\n"
                                        "fn choose(value: &i32, fallback: &i32, flag: bool) -> &i32 {\n"
                                        "  let .some(borrowed) | .other(borrowed) = if flag {\n"
                                        "    MaybeBorrow.some(value)\n"
                                        "  } else {\n"
                                        "    MaybeBorrow.other(fallback)\n"
                                        "  } else {\n"
                                        "    return fallback;\n"
                                        "  };\n"
                                        "  return borrowed;\n"
                                        "}\n"
                                        "fn main() -> void {}\n";
    const sema::CheckedModule checked = analyze_lifetime_source(source);
    const sema::FunctionLifetimeFacts* const facts = find_lifetime_facts(checked, "choose");
    ASSERT_NE(facts, nullptr);
    EXPECT_TRUE(facts->solved);
    EXPECT_TRUE(facts->violations.empty());
    EXPECT_TRUE(lifetime_facts_has_region_kind(*facts, sema::LifetimeRegionKind::parameter));
    std::vector<base::u32> returned_params;
    for (const sema::LifetimeReturnRegion& returned : facts->return_regions) {
        ASSERT_LT(returned.region, facts->regions.size());
        const sema::LifetimeRegion& region = facts->regions[returned.region];
        if (region.kind == sema::LifetimeRegionKind::parameter) {
            returned_params.push_back(region.param_index);
        }
    }
    std::ranges::sort(returned_params);
    returned_params.erase(std::ranges::unique(returned_params).begin(), returned_params.end());
    EXPECT_EQ(returned_params, (std::vector<base::u32>{0U, 1U}));
}

TEST(CoreUnit, LifetimeFactsAcceptExplicitOriginSet)
{
    constexpr std::string_view source = "module lifetime.origin_set;\n"
                                        "fn keep[origin left, origin right](value: &[left | right] i32)"
                                        " -> &[left | right] i32 {\n"
                                        "  return value;\n"
                                        "}\n"
                                        "fn main() -> void {}\n";
    const sema::CheckedModule checked = analyze_lifetime_source(source);
    const sema::FunctionLifetimeFacts* const facts = find_lifetime_facts(checked, "keep");
    ASSERT_NE(facts, nullptr);
    EXPECT_TRUE(facts->solved);
    EXPECT_TRUE(facts->violations.empty());
    EXPECT_EQ(facts->return_regions.size(), 2U);
    EXPECT_TRUE(lifetime_facts_has_region_kind(*facts, sema::LifetimeRegionKind::explicit_origin));

    const std::string dump = sema::dump_checked_module(checked);
    EXPECT_NE(dump.find("origins=left | right"), std::string::npos) << dump;
    EXPECT_NE(dump.find("returns=2"), std::string::npos) << dump;
}

TEST(CoreUnit, LifetimeFactsRejectUnknownExplicitOrigin)
{
    constexpr std::string_view source = "module lifetime.unknown;\n"
                                        "fn bad(value: &[missing] i32) -> &[missing] i32 {\n"
                                        "  return value;\n"
                                        "}\n"
                                        "fn main() -> void {}\n";
    expect_lifetime_diagnostic(source, sema::SEMA_LIFETIME_UNKNOWN_ORIGIN);
}

TEST(CoreUnit, LifetimeFactsRejectReturnOriginOutsideExplicitReturnType)
{
    constexpr std::string_view source = "module lifetime.return_mismatch;\n"
                                        "fn bad[origin left, origin right](left: &[left] i32, right: &[right] i32)"
                                        " -> &[left] i32 {\n"
                                        "  return right;\n"
                                        "}\n"
                                        "fn main() -> void {}\n";
    expect_lifetime_diagnostic(source, sema::SEMA_LIFETIME_RETURN_OUTSIDE_TYPE);
}

TEST(CoreUnit, LifetimeFactsDiagnoseReturnedLocalEscape)
{
    constexpr std::string_view source = "module lifetime.local_escape;\n"
                                        "fn bad() -> &i32 {\n"
                                        "  let value: i32 = 1;\n"
                                        "  return &value;\n"
                                        "}\n"
                                        "fn main() -> void {}\n";
    expect_lifetime_diagnostic(source, sema::SEMA_BORROWED_LOCAL_ESCAPE);
}

TEST(CoreUnit, LifetimeFactsDiagnoseRawDerivedLocalEscape)
{
    constexpr std::string_view source = "module lifetime.raw_escape;\n"
                                        "unsafe fn bad() -> str {\n"
                                        "  let bytes: [2]u8 = b\"ok\";\n"
                                        "  let ptr: *const u8 = sliceptr(bytes[:]);\n"
                                        "  return strraw(ptr, 2usize);\n"
                                        "}\n"
                                        "fn main() -> void {}\n";
    expect_lifetime_diagnostic(source, sema::SEMA_BORROWED_LOCAL_ESCAPE);
}

TEST(CoreUnit, LifetimeFactsDiagnoseRawDerivedSliceCallLocalEscape)
{
    constexpr std::string_view source = "module lifetime.raw_slice_call_escape;\n"
                                        "fn identity(values: []const u8) -> []const u8 {\n"
                                        "  return values;\n"
                                        "}\n"
                                        "unsafe fn bad() -> str {\n"
                                        "  let bytes: [2]u8 = b\"ok\";\n"
                                        "  return strraw(sliceptr(identity(bytes[:])), slicelen(bytes[:]));\n"
                                        "}\n"
                                        "fn main() -> void {}\n";
    expect_lifetime_diagnostic(source, sema::SEMA_BORROWED_LOCAL_ESCAPE);
}

TEST(CoreUnit, LifetimeFactsDiagnoseEnumPayloadLocalEscape)
{
    constexpr std::string_view source = "module lifetime.enum_escape;\n"
                                        "enum MaybeBorrow {\n"
                                        "  some(&i32),\n"
                                        "  none,\n"
                                        "}\n"
                                        "fn bad() -> MaybeBorrow {\n"
                                        "  let value: i32 = 1;\n"
                                        "  return MaybeBorrow.some(&value);\n"
                                        "}\n"
                                        "fn main() -> void {}\n";
    expect_lifetime_diagnostic(source, sema::SEMA_BORROWED_LOCAL_ESCAPE);
}

TEST(CoreUnit, LifetimeFactsDiagnoseMethodReceiverLocalEscape)
{
    constexpr std::string_view source = "module lifetime.method_escape;\n"
                                        "struct Box {\n"
                                        "  value: i32;\n"
                                        "}\n"
                                        "impl Box {\n"
                                        "  fn borrow(self: &Box) -> &i32 {\n"
                                        "    return &self.value;\n"
                                        "  }\n"
                                        "}\n"
                                        "fn bad() -> &i32 {\n"
                                        "  let box: Box = Box { value: 1 };\n"
                                        "  return box.borrow();\n"
                                        "}\n"
                                        "fn main() -> void {}\n";
    expect_lifetime_diagnostic(source, sema::SEMA_BORROWED_LOCAL_ESCAPE);
}

TEST(CoreUnit, LifetimeFactsDiagnoseParameterSlotLocalEscape)
{
    constexpr std::string_view source = "module lifetime.param_slot_escape;\n"
                                        "type Ref = &i32;\n"
                                        "fn bad(value: Ref) -> &Ref {\n"
                                        "  return &value;\n"
                                        "}\n"
                                        "fn main() -> void {}\n";
    expect_lifetime_diagnostic(source, sema::SEMA_BORROWED_LOCAL_ESCAPE);
}

TEST(CoreUnit, LifetimeFactsRejectAmbiguousPublicElision)
{
    constexpr std::string_view source = "module lifetime.ambiguous;\n"
                                        "pub fn choose(left: &i32, right: &i32, take_left: bool) -> &i32 {\n"
                                        "  if take_left { return left; }\n"
                                        "  return right;\n"
                                        "}\n"
                                        "fn main() -> void {}\n";
    expect_lifetime_diagnostic(source, sema::SEMA_LIFETIME_ELISION_AMBIGUOUS);
}

TEST(CoreUnit, LifetimeFactsRejectNestedReferenceTypeOutlivesMismatch)
{
    constexpr std::string_view source = "module lifetime.type_outlives;\n"
                                        "fn bad[origin outer, origin inner](value: &[outer] &[inner] i32)"
                                        " -> &[outer] &[inner] i32 {\n"
                                        "  return value;\n"
                                        "}\n"
                                        "fn main() -> void {}\n";
    expect_lifetime_diagnostic(source, sema::SEMA_LIFETIME_TYPE_OUTLIVES);
}

TEST(CoreUnit, LifetimeFactsCheckExternSignatures)
{
    constexpr std::string_view source = "module lifetime.externs;\n"
                                        "extern c {\n"
                                        "  fn ext(value: &[missing] i32) -> &[missing] i32;\n"
                                        "}\n"
                                        "fn main() -> void {}\n";
    expect_lifetime_diagnostic(source, sema::SEMA_LIFETIME_UNKNOWN_ORIGIN);
}

TEST(CoreUnit, LifetimeFactsRejectExternConcreteBorrowElisionWithoutBody)
{
    constexpr std::string_view source = "module lifetime.extern_elision;\n"
                                        "extern c {\n"
                                        "  fn choose(left: &i32, right: &i32) -> &i32;\n"
                                        "}\n"
                                        "fn main() -> void {}\n";
    expect_lifetime_diagnostic(source, sema::SEMA_LIFETIME_ELISION_AMBIGUOUS);
}

TEST(CoreUnit, LifetimeFactsRejectExternConcreteBorrowSurfaceElisionWithoutBody)
{
    constexpr std::string_view source = "module lifetime.extern_concrete_surfaces;\n"
                                        "extern c {\n"
                                        "  fn choose_text(left: str, right: str) -> str;\n"
                                        "  fn choose_slice(left: []const u8, right: []const u8) -> []const u8;\n"
                                        "  fn choose_array(left: &i32, right: &i32) -> [2]&i32;\n"
                                        "  fn choose_tuple_scalar(left: &i32, right: &i32) -> (&i32, i32);\n"
                                        "  fn choose_callback(left: &i32, right: &i32) -> fn() -> &i32;\n"
                                        "}\n"
                                        "fn main() -> void {}\n";
    expect_lifetime_diagnostic(source, sema::SEMA_LIFETIME_ELISION_AMBIGUOUS);
}

TEST(CoreUnit, LifetimeFactsRejectPrototypeAggregateBorrowElisionWithoutBodyFacts)
{
    constexpr std::string_view source = "module lifetime.prototype_elision;\n"
                                        "struct View {\n"
                                        "  value: &i32;\n"
                                        "}\n"
                                        "enum Choice {\n"
                                        "  some(&i32),\n"
                                        "  none,\n"
                                        "}\n"
                                        "fn choose_view(left: &i32, right: &i32) -> View;\n"
                                        "fn choose_choice(left: &i32, right: &i32) -> Choice;\n"
                                        "fn choose_tuple(left: &i32, right: &i32) -> (&i32, &i32);\n"
                                        "fn choose_view(left: &i32, right: &i32) -> View {\n"
                                        "  return View { value: left };\n"
                                        "}\n"
                                        "fn choose_choice(left: &i32, right: &i32) -> Choice {\n"
                                        "  return Choice.some(left);\n"
                                        "}\n"
                                        "fn choose_tuple(left: &i32, right: &i32) -> (&i32, &i32) {\n"
                                        "  return (left, right);\n"
                                        "}\n"
                                        "fn main() -> void {}\n";
    expect_lifetime_diagnostic(source, sema::SEMA_LIFETIME_ELISION_AMBIGUOUS);
}

TEST(CoreUnit, LifetimeFactsCollectBorrowSummaryOriginKinds)
{
    LifetimeWhiteboxHarness harness;
    configure_lifetime_whitebox_harness(harness, "summary_origins");
    sema::FunctionBorrowSummary summary;
    summary.function = harness.key;
    summary.return_type = harness.signature.return_type;
    summary.return_type_can_contain_borrow = true;
    summary.origins = {
        sema::BorrowSummaryOrigin{
            .kind = sema::BorrowSummaryOriginKind::parameter,
            .param_index = 0U,
            .range = base::SourceRange{LIFETIME_TEST_SOURCE_ID, 28U, 29U},
        },
        sema::BorrowSummaryOrigin{
            .kind = sema::BorrowSummaryOriginKind::static_,
            .range = base::SourceRange{LIFETIME_TEST_SOURCE_ID, 30U, 31U},
        },
        sema::BorrowSummaryOrigin{
            .kind = sema::BorrowSummaryOriginKind::local,
            .name_id = LIFETIME_TEST_VALUE_NAME_ID,
            .expr = syntax::ExprId{1U},
            .range = base::SourceRange{LIFETIME_TEST_SOURCE_ID, 31U, 32U},
        },
        sema::BorrowSummaryOrigin{
            .kind = sema::BorrowSummaryOriginKind::temporary,
            .expr = syntax::ExprId{2U},
            .range = base::SourceRange{LIFETIME_TEST_SOURCE_ID, 32U, 33U},
        },
        sema::BorrowSummaryOrigin{
            .kind = sema::BorrowSummaryOriginKind::unknown,
            .expr = syntax::ExprId{3U},
            .range = base::SourceRange{LIFETIME_TEST_SOURCE_ID, 34U, 35U},
        },
        sema::BorrowSummaryOrigin{
            .kind = sema::BorrowSummaryOriginKind::parameter,
            .param_index = LIFETIME_TEST_OUT_OF_RANGE_INDEX,
            .range = base::SourceRange{LIFETIME_TEST_SOURCE_ID, 35U, 36U},
        },
        sema::BorrowSummaryOrigin{
            .kind = sema::BorrowSummaryOriginKind::none,
            .range = base::SourceRange{LIFETIME_TEST_SOURCE_ID, 36U, 37U},
        },
    };
    summary.return_origins = {
        sema::FunctionBorrowReturnOrigin{.origin_index = 0U, .range = summary.origins[0].range},
        sema::FunctionBorrowReturnOrigin{.origin_index = 1U, .range = summary.origins[1].range},
        sema::FunctionBorrowReturnOrigin{.origin_index = 2U, .range = summary.origins[2].range},
        sema::FunctionBorrowReturnOrigin{.origin_index = 3U, .range = summary.origins[3].range},
        sema::FunctionBorrowReturnOrigin{.origin_index = 4U, .range = summary.origins[4].range},
        sema::FunctionBorrowReturnOrigin{.origin_index = 5U, .range = summary.origins[5].range},
        sema::FunctionBorrowReturnOrigin{.origin_index = 6U, .range = summary.origins[6].range},
        sema::FunctionBorrowReturnOrigin{
            .origin_index = LIFETIME_TEST_OUT_OF_RANGE_INDEX,
            .range = base::SourceRange{LIFETIME_TEST_SOURCE_ID, 38U, 39U},
        },
    };
    harness.analyzer.state_.checked.borrow_summaries[harness.key] = summary;
    sema::BodyFlowGraph graph;
    graph.function = harness.key;
    graph.points = {
        sema::BodyFlowPoint{
            .kind = sema::BodyFlowPointKind::entry,
            .range = base::SourceRange{LIFETIME_TEST_SOURCE_ID, 41U, 42U},
        },
        sema::BodyFlowPoint{
            .kind = sema::BodyFlowPointKind::sequence,
            .range = base::SourceRange{LIFETIME_TEST_SOURCE_ID, 42U, 43U},
        },
        sema::BodyFlowPoint{
            .kind = sema::BodyFlowPointKind::exit,
            .range = base::SourceRange{LIFETIME_TEST_SOURCE_ID, 43U, 44U},
        },
    };
    graph.actions.push_back(sema::BodyFlowAction{
        .kind = sema::BodyFlowActionKind::return_,
        .point = 1U,
        .expr = syntax::ExprId{2U},
        .range = base::SourceRange{LIFETIME_TEST_SOURCE_ID, 44U, 45U},
    });
    harness.analyzer.state_.checked.body_flow_graphs[harness.key] = std::move(graph);

    harness.analyzer.analyze_lifetimes(harness.function, harness.key, harness.signature);

    const auto facts = harness.analyzer.state_.checked.lifetime_facts.find(harness.key);
    ASSERT_NE(facts, harness.analyzer.state_.checked.lifetime_facts.end());
    EXPECT_EQ(facts->second.return_regions.size(), 6U);
    EXPECT_TRUE(lifetime_facts_has_region_kind(facts->second, sema::LifetimeRegionKind::parameter));
    EXPECT_TRUE(lifetime_facts_has_region_kind(facts->second, sema::LifetimeRegionKind::local));
    EXPECT_TRUE(lifetime_facts_has_region_kind(facts->second, sema::LifetimeRegionKind::temporary));
    EXPECT_TRUE(lifetime_facts_has_region_kind(facts->second, sema::LifetimeRegionKind::unknown));
    EXPECT_TRUE(lifetime_facts_has_violation_kind(facts->second, sema::LifetimeViolationKind::local_escape));
    EXPECT_TRUE(lifetime_facts_has_violation_kind(facts->second, sema::LifetimeViolationKind::unknown_escape));
    EXPECT_TRUE(
        lifetime_facts_has_emitted_violation_kind(facts->second, sema::LifetimeViolationKind::local_escape));
    EXPECT_GT(facts->second.live_ranges.size(), 0U);
    EXPECT_TRUE(std::ranges::any_of(facts->second.live_ranges, [&facts](const sema::LifetimeRegionLiveRange& range) {
        return range.region < facts->second.regions.size()
            && facts->second.regions[range.region].kind == sema::LifetimeRegionKind::temporary
            && range.first_point == 1U && range.last_point == 1U && range.point_count == 1U;
    }));
}

TEST(CoreUnit, LifetimeFactsCollectUnknownSummaryFlagWithKnownReturnOrigin)
{
    LifetimeWhiteboxHarness harness;
    configure_lifetime_whitebox_harness(harness, "summary_unknown_flag");
    sema::FunctionBorrowSummary summary;
    summary.function = harness.key;
    summary.return_type = harness.signature.return_type;
    summary.return_type_can_contain_borrow = true;
    summary.has_unknown_return_origin = true;
    summary.origins = {
        sema::BorrowSummaryOrigin{
            .kind = sema::BorrowSummaryOriginKind::parameter,
            .param_index = 0U,
            .range = base::SourceRange{LIFETIME_TEST_SOURCE_ID, 39U, 40U},
        },
    };
    summary.return_origins = {
        sema::FunctionBorrowReturnOrigin{.origin_index = 0U, .range = summary.origins[0].range},
    };
    harness.analyzer.state_.checked.borrow_summaries[harness.key] = summary;

    harness.analyzer.analyze_lifetimes(harness.function, harness.key, harness.signature);

    const auto facts = harness.analyzer.state_.checked.lifetime_facts.find(harness.key);
    ASSERT_NE(facts, harness.analyzer.state_.checked.lifetime_facts.end());
    EXPECT_EQ(facts->second.return_regions.size(), 2U);
    EXPECT_TRUE(lifetime_facts_has_region_kind(facts->second, sema::LifetimeRegionKind::parameter));
    EXPECT_TRUE(lifetime_facts_has_region_kind(facts->second, sema::LifetimeRegionKind::unknown));
    EXPECT_TRUE(lifetime_facts_has_violation_kind(facts->second, sema::LifetimeViolationKind::unknown_escape));
}

TEST(CoreUnit, LifetimeFactsCollectEmptyUnknownBorrowContract)
{
    LifetimeWhiteboxHarness harness;
    configure_lifetime_whitebox_harness(harness, "unknown_contract");
    sema::FunctionBorrowContract contract;
    contract.function = harness.key;
    contract.return_type = harness.signature.return_type;
    contract.return_type_can_contain_borrow = true;
    contract.unknown_return_allowed = true;
    contract.source = sema::FunctionBorrowContractSource::conservative_unknown;
    contract.range = base::SourceRange{LIFETIME_TEST_SOURCE_ID, 40U, 45U};
    harness.analyzer.state_.checked.borrow_contracts[harness.key] = contract;

    harness.analyzer.analyze_signature_lifetimes(harness.function, harness.key, harness.signature);

    const auto facts = harness.analyzer.state_.checked.lifetime_facts.find(harness.key);
    ASSERT_NE(facts, harness.analyzer.state_.checked.lifetime_facts.end());
    ASSERT_EQ(facts->second.return_regions.size(), 1U);
    EXPECT_TRUE(lifetime_facts_has_region_kind(facts->second, sema::LifetimeRegionKind::unknown));
}

TEST(CoreUnit, LifetimeFactsIgnoreMalformedReferenceOriginFacts)
{
    LifetimeWhiteboxHarness harness;
    configure_lifetime_whitebox_harness(harness, "malformed_origin_facts");
    const sema::TypeHandle i32 = harness.analyzer.state_.checked.types.builtin(sema::BuiltinType::i32);

    sema::ReferenceOriginFact invalid_type_fact;
    invalid_type_fact.module = LIFETIME_TEST_MODULE_ID;
    invalid_type_fact.item = LIFETIME_TEST_ITEM_ID;
    invalid_type_fact.semantic_type = sema::INVALID_TYPE_HANDLE;
    invalid_type_fact.origin_names.push_back(harness.analyzer.state_.checked.intern_text("invalid_type"));
    invalid_type_fact.range = base::SourceRange{LIFETIME_TEST_SOURCE_ID, 50U, 52U};
    harness.analyzer.state_.checked.reference_origin_facts.push_back(std::move(invalid_type_fact));

    sema::ReferenceOriginFact non_reference_fact;
    non_reference_fact.module = LIFETIME_TEST_MODULE_ID;
    non_reference_fact.item = LIFETIME_TEST_ITEM_ID;
    non_reference_fact.semantic_type = i32;
    non_reference_fact.origin_names.push_back(harness.analyzer.state_.checked.intern_text("non_reference"));
    non_reference_fact.origin_name_ids.push_back(LIFETIME_TEST_VALUE_NAME_ID);
    non_reference_fact.range = base::SourceRange{LIFETIME_TEST_SOURCE_ID, 53U, 55U};
    harness.analyzer.state_.checked.reference_origin_facts.push_back(std::move(non_reference_fact));

    harness.analyzer.analyze_lifetimes(harness.function, harness.key, harness.signature);

    const auto facts = harness.analyzer.state_.checked.lifetime_facts.find(harness.key);
    ASSERT_NE(facts, harness.analyzer.state_.checked.lifetime_facts.end());
    EXPECT_TRUE(facts->second.type_outlives_constraints.empty());
    EXPECT_TRUE(lifetime_facts_has_violation_kind(facts->second, sema::LifetimeViolationKind::unknown_origin));
}

} // namespace aurex::test
