#include <aurex/infrastructure/query/dyn_ownership_runtime_ir_verifier_facts.hpp>

#include <string>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

constexpr base::u8 QUERY_TEST_INVALID_ENUM_VALUE = 0xffU;

[[nodiscard]] bool contains_text(const std::string& value, const std::string_view text)
{
    return value.find(text) != std::string::npos;
}

[[nodiscard]] query::FunctionDynOwnershipRuntimeIrVerifierFacts baseline()
{
    return query::m19_dyn_ownership_runtime_ir_verifier_baseline();
}

void refresh(query::FunctionDynOwnershipRuntimeIrVerifierFacts& facts)
{
    facts.summary = query::summarize_dyn_ownership_runtime_ir_verifier_counts(facts);
    facts.fingerprint = query::dyn_ownership_runtime_ir_verifier_facts_fingerprint(facts);
}

} // namespace

TEST(QueryUnit, DynOwnershipRuntimeIrVerifierFactsExposeEnumNamesAndInvalidFallbacks)
{
    EXPECT_EQ(query::dyn_ownership_runtime_ir_verifier_fact_kind_name(
                  query::DynOwnershipRuntimeIrVerifierFactKind::borrowed_vtable_destructor_free),
        "borrowed_vtable_destructor_free");
    EXPECT_EQ(query::dyn_ownership_runtime_ir_verifier_fact_kind_name(
                  query::DynOwnershipRuntimeIrVerifierFactKind::static_cleanup_only),
        "static_cleanup_only");
    EXPECT_EQ(query::dyn_ownership_runtime_ir_verifier_fact_kind_name(
                  query::DynOwnershipRuntimeIrVerifierFactKind::erased_drop_identity_required),
        "erased_drop_identity_required");
    EXPECT_EQ(query::dyn_ownership_runtime_ir_verifier_fact_kind_name(
                  query::DynOwnershipRuntimeIrVerifierFactKind::allocator_identity_required),
        "allocator_identity_required");
    EXPECT_EQ(query::dyn_ownership_runtime_ir_verifier_fact_kind_name(
                  query::DynOwnershipRuntimeIrVerifierFactKind::owned_dyn_object_placeholder_blocked),
        "owned_dyn_object_placeholder_blocked");
    EXPECT_EQ(query::dyn_ownership_runtime_ir_verifier_fact_kind_name(
                  query::DynOwnershipRuntimeIrVerifierFactKind::runtime_lowering_blocked_without_stdlib),
        "runtime_lowering_blocked_without_stdlib");

    EXPECT_EQ(query::dyn_ownership_runtime_ir_verifier_stage_name(
                  query::DynOwnershipRuntimeIrVerifierStage::ir_verifier_preparation),
        "ir_verifier_preparation");
    EXPECT_EQ(query::dyn_ownership_runtime_ir_verifier_stage_name(
                  query::DynOwnershipRuntimeIrVerifierStage::verifier_negative_matrix),
        "verifier_negative_matrix");
    EXPECT_EQ(query::dyn_ownership_runtime_ir_verifier_stage_name(
                  query::DynOwnershipRuntimeIrVerifierStage::blocked_future_runtime),
        "blocked_future_runtime");

    EXPECT_EQ(query::dyn_ownership_runtime_ir_verifier_policy_name(
                  query::DynOwnershipRuntimeIrVerifierPolicy::borrowed_vtable_methods_only_v1),
        "borrowed_vtable_methods_only_v1");
    EXPECT_EQ(query::dyn_ownership_runtime_ir_verifier_policy_name(
                  query::DynOwnershipRuntimeIrVerifierPolicy::static_cleanup_marker_only_v1),
        "static_cleanup_marker_only_v1");
    EXPECT_EQ(query::dyn_ownership_runtime_ir_verifier_policy_name(
                  query::DynOwnershipRuntimeIrVerifierPolicy::erased_drop_identity_prerequisite_v1),
        "erased_drop_identity_prerequisite_v1");
    EXPECT_EQ(query::dyn_ownership_runtime_ir_verifier_policy_name(
                  query::DynOwnershipRuntimeIrVerifierPolicy::allocator_identity_prerequisite_v1),
        "allocator_identity_prerequisite_v1");
    EXPECT_EQ(query::dyn_ownership_runtime_ir_verifier_policy_name(
                  query::DynOwnershipRuntimeIrVerifierPolicy::owned_dyn_object_placeholder_not_lowered_v1),
        "owned_dyn_object_placeholder_not_lowered_v1");
    EXPECT_EQ(query::dyn_ownership_runtime_ir_verifier_policy_name(
                  query::DynOwnershipRuntimeIrVerifierPolicy::runtime_lowering_not_implemented_v1),
        "runtime_lowering_not_implemented_v1");

    EXPECT_EQ(query::dyn_ownership_runtime_ir_verifier_fact_kind_name(
                  static_cast<query::DynOwnershipRuntimeIrVerifierFactKind>(QUERY_TEST_INVALID_ENUM_VALUE)),
        "invalid");
    EXPECT_EQ(query::dyn_ownership_runtime_ir_verifier_stage_name(
                  static_cast<query::DynOwnershipRuntimeIrVerifierStage>(QUERY_TEST_INVALID_ENUM_VALUE)),
        "invalid");
    EXPECT_EQ(query::dyn_ownership_runtime_ir_verifier_policy_name(
                  static_cast<query::DynOwnershipRuntimeIrVerifierPolicy>(QUERY_TEST_INVALID_ENUM_VALUE)),
        "invalid");
    EXPECT_FALSE(query::is_valid(
        static_cast<query::DynOwnershipRuntimeIrVerifierFactKind>(QUERY_TEST_INVALID_ENUM_VALUE)));
    EXPECT_FALSE(query::is_valid(
        static_cast<query::DynOwnershipRuntimeIrVerifierStage>(QUERY_TEST_INVALID_ENUM_VALUE)));
    EXPECT_FALSE(query::is_valid(
        static_cast<query::DynOwnershipRuntimeIrVerifierPolicy>(QUERY_TEST_INVALID_ENUM_VALUE)));
}

TEST(QueryUnit, DynOwnershipRuntimeIrVerifierFactsM19BaselineRecordsVerifierBoundary)
{
    const query::FunctionDynOwnershipRuntimeIrVerifierFacts facts = baseline();
    ASSERT_TRUE(query::is_valid(facts));
    EXPECT_TRUE(query::is_valid_m19_dyn_ownership_runtime_ir_verifier_baseline(facts));
    EXPECT_EQ(facts.symbol, "M19 Dyn Ownership Runtime IR Verifier Preparation");
    EXPECT_EQ(facts.fingerprint, query::dyn_ownership_runtime_ir_verifier_facts_fingerprint(facts));
    EXPECT_EQ(facts.summary.fact_count, 6U);
    EXPECT_EQ(facts.summary.borrowed_vtable_count, 1U);
    EXPECT_EQ(facts.summary.destructor_free_vtable_count, 1U);
    EXPECT_EQ(facts.summary.static_cleanup_only_count, 1U);
    EXPECT_EQ(facts.summary.erased_drop_identity_required_count, 1U);
    EXPECT_EQ(facts.summary.allocator_identity_required_count, 1U);
    EXPECT_EQ(facts.summary.owned_dyn_object_placeholder_blocked_count, 6U);
    EXPECT_EQ(facts.summary.runtime_lowering_blocked_count, 6U);
    EXPECT_EQ(facts.summary.dynamic_drop_runtime_blocked_count, 6U);
    EXPECT_EQ(facts.summary.standard_library_blocked_count, 6U);
    EXPECT_EQ(facts.summary.lowering_runtime_implemented_count, 0U);
}

TEST(QueryUnit, DynOwnershipRuntimeIrVerifierFactsRecordFunctionsUpdateSummary)
{
    const query::FunctionDynOwnershipRuntimeIrVerifierFacts source = baseline();
    query::FunctionDynOwnershipRuntimeIrVerifierFacts facts;
    facts.symbol = "manual M19 runtime IR verifier facts";

    query::record_dyn_ownership_runtime_ir_verifier_fact(facts, source.facts.front());
    EXPECT_EQ(facts.summary.fact_count, 1U);
    EXPECT_EQ(facts.summary.borrowed_vtable_count, 1U);
    EXPECT_EQ(facts.summary.destructor_free_vtable_count, 1U);
    EXPECT_TRUE(query::is_valid(facts));

    query::record_dyn_ownership_runtime_ir_verifier_fact(facts, source.facts[1]);
    EXPECT_EQ(facts.summary.fact_count, 2U);
    EXPECT_EQ(facts.summary.static_cleanup_only_count, 1U);
    EXPECT_TRUE(query::is_valid(facts));

    const query::StableFingerprint128 two_facts =
        query::dyn_ownership_runtime_ir_verifier_facts_fingerprint(facts);
    query::record_dyn_ownership_runtime_ir_verifier_fact(facts, source.facts[2]);
    facts.fingerprint = query::dyn_ownership_runtime_ir_verifier_facts_fingerprint(facts);
    EXPECT_NE(facts.fingerprint, two_facts);
    EXPECT_TRUE(query::is_valid(facts));
    EXPECT_FALSE(query::is_valid_m19_dyn_ownership_runtime_ir_verifier_baseline(facts));
}

TEST(QueryUnit, DynOwnershipRuntimeIrVerifierFactsValidationRejectsRuntimeDrift)
{
    const query::FunctionDynOwnershipRuntimeIrVerifierFacts facts = baseline();
    ASSERT_TRUE(query::is_valid_m19_dyn_ownership_runtime_ir_verifier_baseline(facts));

    const auto expect_invalid = [&facts](auto&& mutate) {
        query::FunctionDynOwnershipRuntimeIrVerifierFacts drift = facts;
        mutate(drift.facts.front());
        refresh(drift);
        EXPECT_FALSE(query::is_valid(drift));
    };
    expect_invalid([](query::DynOwnershipRuntimeIrVerifierFact& fact) {
        fact.fact_name.clear();
    });
    expect_invalid([](query::DynOwnershipRuntimeIrVerifierFact& fact) {
        fact.boundary_fact.clear();
    });
    expect_invalid([](query::DynOwnershipRuntimeIrVerifierFact& fact) {
        fact.kind = static_cast<query::DynOwnershipRuntimeIrVerifierFactKind>(QUERY_TEST_INVALID_ENUM_VALUE);
    });
    expect_invalid([](query::DynOwnershipRuntimeIrVerifierFact& fact) {
        fact.policy = query::DynOwnershipRuntimeIrVerifierPolicy::runtime_lowering_not_implemented_v1;
    });
    expect_invalid([](query::DynOwnershipRuntimeIrVerifierFact& fact) {
        fact.stage = query::DynOwnershipRuntimeIrVerifierStage::blocked_future_runtime;
    });
    expect_invalid([](query::DynOwnershipRuntimeIrVerifierFact& fact) {
        fact.verifier_visible = false;
    });
    expect_invalid([](query::DynOwnershipRuntimeIrVerifierFact& fact) {
        fact.borrowed_vtable_destructor_free = false;
    });
    expect_invalid([](query::DynOwnershipRuntimeIrVerifierFact& fact) {
        fact.owned_dyn_object_placeholder_blocked = false;
    });
    expect_invalid([](query::DynOwnershipRuntimeIrVerifierFact& fact) {
        fact.runtime_lowering_blocked = false;
    });
    expect_invalid([](query::DynOwnershipRuntimeIrVerifierFact& fact) {
        fact.dynamic_drop_runtime_blocked = false;
    });
    expect_invalid([](query::DynOwnershipRuntimeIrVerifierFact& fact) {
        fact.standard_library_blocked = false;
    });
    expect_invalid([](query::DynOwnershipRuntimeIrVerifierFact& fact) {
        fact.lowering_runtime_implemented = true;
    });

    query::FunctionDynOwnershipRuntimeIrVerifierFacts empty_symbol = facts;
    empty_symbol.symbol.clear();
    refresh(empty_symbol);
    EXPECT_FALSE(query::is_valid(empty_symbol));

    query::FunctionDynOwnershipRuntimeIrVerifierFacts stale_summary = facts;
    ++stale_summary.summary.fact_count;
    EXPECT_FALSE(query::is_valid(stale_summary));

    query::FunctionDynOwnershipRuntimeIrVerifierFacts stale_fingerprint = facts;
    stale_fingerprint.fingerprint = query::stable_fingerprint("stale M19 IR verifier facts");
    EXPECT_FALSE(query::is_valid(stale_fingerprint));

    query::FunctionDynOwnershipRuntimeIrVerifierFacts duplicate_kind = facts;
    duplicate_kind.facts[1] = duplicate_kind.facts.front();
    refresh(duplicate_kind);
    EXPECT_TRUE(query::is_valid(duplicate_kind));
    EXPECT_FALSE(query::is_valid_m19_dyn_ownership_runtime_ir_verifier_baseline(duplicate_kind));
}

TEST(QueryUnit, DynOwnershipRuntimeIrVerifierFactsSummaryDumpAndFingerprintAreStable)
{
    const query::FunctionDynOwnershipRuntimeIrVerifierFacts facts = baseline();
    ASSERT_TRUE(query::is_valid_m19_dyn_ownership_runtime_ir_verifier_baseline(facts));

    const std::string summary = query::summarize_dyn_ownership_runtime_ir_verifier_facts(facts);
    EXPECT_TRUE(contains_text(summary, "dyn_ownership_runtime_ir_verifier_facts")) << summary;
    EXPECT_TRUE(contains_text(summary, "facts=6")) << summary;
    EXPECT_TRUE(contains_text(summary, "borrowed_vtables=1")) << summary;
    EXPECT_TRUE(contains_text(summary, "runtime_lowering_blocked=6")) << summary;
    EXPECT_TRUE(contains_text(summary, "dynamic_drop_runtime_blocked=6")) << summary;
    EXPECT_TRUE(contains_text(summary, "standard_library_blocked=6")) << summary;
    EXPECT_TRUE(contains_text(summary, "lowering_runtime_implemented=0")) << summary;

    const std::string dump = query::dump_dyn_ownership_runtime_ir_verifier_facts(facts);
    EXPECT_TRUE(contains_text(dump, "borrowed_vtable_destructor_free_ir_guard")) << dump;
    EXPECT_TRUE(contains_text(dump, "static_cleanup_marker_only_ir_guard")) << dump;
    EXPECT_TRUE(contains_text(dump, "future_erased_drop_identity_required_by_verifier")) << dump;
    EXPECT_TRUE(contains_text(dump, "future_allocator_identity_required_by_verifier")) << dump;
    EXPECT_TRUE(contains_text(dump, "future_owned_dyn_object_placeholder_blocked_in_ir")) << dump;
    EXPECT_TRUE(contains_text(dump, "runtime_lowering_blocked_until_stdlib_runtime_stage")) << dump;
    EXPECT_TRUE(contains_text(dump, "dynamic_drop_runtime_not_in_m19")) << dump;
    EXPECT_TRUE(contains_text(dump, "allocator_api_not_in_m19")) << dump;
    EXPECT_TRUE(contains_text(dump, "runtime_abi_lowering_not_in_m19")) << dump;

    query::FunctionDynOwnershipRuntimeIrVerifierFacts changed = facts;
    changed.facts.front().verifier_guard_fact = "verifier_guard:v2";
    refresh(changed);
    EXPECT_TRUE(query::is_valid(changed));
    EXPECT_NE(changed.fingerprint, facts.fingerprint);
}

} // namespace aurex::test
