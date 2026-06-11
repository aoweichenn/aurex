#include <aurex/infrastructure/query/owned_dyn_drop_allocator_identity_gate.hpp>

#include <string>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

constexpr base::u8 QUERY_TEST_INVALID_ENUM_VALUE = 0xffU;

[[nodiscard]] bool contains_text(const std::string& value, const std::string_view text)
{
    return value.find(text) != std::string::npos;
}

[[nodiscard]] query::OwnedDynDropAllocatorIdentityGate baseline()
{
    return query::m20c_owned_dyn_drop_allocator_identity_gate_baseline();
}

void refresh(query::OwnedDynDropAllocatorIdentityGate& gate)
{
    gate.summary = query::summarize_owned_dyn_drop_allocator_identity_gate_counts(gate);
    gate.fingerprint = query::owned_dyn_drop_allocator_identity_gate_fingerprint(gate);
}

} // namespace

TEST(QueryUnit, OwnedDynDropAllocatorIdentityGateExposesEnumNamesAndInvalidFallbacks)
{
    EXPECT_EQ(query::owned_dyn_drop_allocator_identity_fact_kind_name(
                  query::OwnedDynDropAllocatorIdentityFactKind::erased_drop_identity),
        "erased_drop_identity");
    EXPECT_EQ(query::owned_dyn_drop_allocator_identity_fact_kind_name(
                  query::OwnedDynDropAllocatorIdentityFactKind::allocator_identity),
        "allocator_identity");
    EXPECT_EQ(query::owned_dyn_drop_allocator_identity_fact_kind_name(
                  query::OwnedDynDropAllocatorIdentityFactKind::cleanup_dropck_bridge),
        "cleanup_dropck_bridge");
    EXPECT_EQ(query::owned_dyn_drop_allocator_identity_fact_kind_name(
                  query::OwnedDynDropAllocatorIdentityFactKind::owned_handle_identity_binding),
        "owned_handle_identity_binding");
    EXPECT_EQ(query::owned_dyn_drop_allocator_identity_fact_kind_name(
                  query::OwnedDynDropAllocatorIdentityFactKind::runtime_lowering_blocker),
        "runtime_lowering_blocker");

    EXPECT_EQ(query::owned_dyn_drop_allocator_identity_stage_name(
                  query::OwnedDynDropAllocatorIdentityStage::identity_prerequisite),
        "identity_prerequisite");
    EXPECT_EQ(query::owned_dyn_drop_allocator_identity_stage_name(
                  query::OwnedDynDropAllocatorIdentityStage::verifier_identity_guard),
        "verifier_identity_guard");
    EXPECT_EQ(query::owned_dyn_drop_allocator_identity_stage_name(
                  query::OwnedDynDropAllocatorIdentityStage::blocked_future_runtime),
        "blocked_future_runtime");

    EXPECT_EQ(query::owned_dyn_drop_allocator_identity_policy_name(
                  query::OwnedDynDropAllocatorIdentityPolicy::compiler_owned_erased_drop_identity_v1),
        "compiler_owned_erased_drop_identity_v1");
    EXPECT_EQ(query::owned_dyn_drop_allocator_identity_policy_name(
                  query::OwnedDynDropAllocatorIdentityPolicy::compiler_owned_allocator_identity_v1),
        "compiler_owned_allocator_identity_v1");
    EXPECT_EQ(query::owned_dyn_drop_allocator_identity_policy_name(
                  query::OwnedDynDropAllocatorIdentityPolicy::cleanup_dropck_static_bridge_v1),
        "cleanup_dropck_static_bridge_v1");
    EXPECT_EQ(query::owned_dyn_drop_allocator_identity_policy_name(
                  query::OwnedDynDropAllocatorIdentityPolicy::owned_handle_identity_binding_v1),
        "owned_handle_identity_binding_v1");
    EXPECT_EQ(query::owned_dyn_drop_allocator_identity_policy_name(
                  query::OwnedDynDropAllocatorIdentityPolicy::runtime_lowering_not_implemented_v1),
        "runtime_lowering_not_implemented_v1");

    EXPECT_EQ(query::owned_dyn_drop_allocator_identity_fact_kind_name(
                  static_cast<query::OwnedDynDropAllocatorIdentityFactKind>(QUERY_TEST_INVALID_ENUM_VALUE)),
        "invalid");
    EXPECT_EQ(query::owned_dyn_drop_allocator_identity_stage_name(
                  static_cast<query::OwnedDynDropAllocatorIdentityStage>(QUERY_TEST_INVALID_ENUM_VALUE)),
        "invalid");
    EXPECT_EQ(query::owned_dyn_drop_allocator_identity_policy_name(
                  static_cast<query::OwnedDynDropAllocatorIdentityPolicy>(QUERY_TEST_INVALID_ENUM_VALUE)),
        "invalid");
    EXPECT_FALSE(
        query::is_valid(static_cast<query::OwnedDynDropAllocatorIdentityFactKind>(QUERY_TEST_INVALID_ENUM_VALUE)));
    EXPECT_FALSE(
        query::is_valid(static_cast<query::OwnedDynDropAllocatorIdentityStage>(QUERY_TEST_INVALID_ENUM_VALUE)));
    EXPECT_FALSE(
        query::is_valid(static_cast<query::OwnedDynDropAllocatorIdentityPolicy>(QUERY_TEST_INVALID_ENUM_VALUE)));
}

TEST(QueryUnit, OwnedDynDropAllocatorIdentityGateM20cBaselineRecordsIdentities)
{
    const query::OwnedDynDropAllocatorIdentityGate gate = baseline();
    ASSERT_TRUE(query::is_valid(gate));
    EXPECT_TRUE(query::is_valid_m20c_owned_dyn_drop_allocator_identity_gate(gate));
    EXPECT_EQ(gate.subject, "M20c Drop / Allocator Identity Prerequisite Gate");
    EXPECT_TRUE(query::is_valid_m20b_owned_dyn_ir_shape_prototype_gate(gate.ir_shape_gate));
    EXPECT_EQ(gate.ir_shape_gate_fingerprint, gate.ir_shape_gate.fingerprint);
    EXPECT_EQ(gate.fingerprint, query::owned_dyn_drop_allocator_identity_gate_fingerprint(gate));

    EXPECT_EQ(gate.summary.fact_count, 5U);
    EXPECT_EQ(gate.summary.erased_drop_identity_count, 1U);
    EXPECT_EQ(gate.summary.allocator_identity_count, 1U);
    EXPECT_EQ(gate.summary.cleanup_dropck_bridge_count, 1U);
    EXPECT_EQ(gate.summary.owned_handle_identity_binding_count, 1U);
    EXPECT_EQ(gate.summary.runtime_lowering_blocker_count, 1U);
    EXPECT_EQ(gate.summary.m20b_reference_count, 5U);
    EXPECT_EQ(gate.summary.compiler_owned_identity_count, 5U);
    EXPECT_EQ(gate.summary.drop_identity_visible_count, 3U);
    EXPECT_EQ(gate.summary.allocator_identity_visible_count, 2U);
    EXPECT_EQ(gate.summary.cleanup_dropck_bridge_visible_count, 1U);
    EXPECT_EQ(gate.summary.owned_handle_binding_visible_count, 1U);
    EXPECT_EQ(gate.summary.standard_library_api_blocked_count, 5U);
    EXPECT_EQ(gate.summary.box_dyn_surface_blocked_count, 5U);
    EXPECT_EQ(gate.summary.owning_dyn_user_value_blocked_count, 5U);
    EXPECT_EQ(gate.summary.allocator_api_blocked_count, 5U);
    EXPECT_EQ(gate.summary.runtime_lowering_blocked_count, 5U);
    EXPECT_EQ(gate.summary.dynamic_drop_runtime_blocked_count, 5U);
    EXPECT_EQ(gate.summary.backend_helper_blocked_count, 5U);
    EXPECT_EQ(gate.summary.executable_runtime_implemented_count, 0U);
    EXPECT_EQ(gate.summary.observed_layout_prototype_total, 1U);
    ASSERT_FALSE(gate.facts.empty());
    EXPECT_NE(gate.facts.front().drop_identity_key, query::StableFingerprint128{});
    EXPECT_NE(gate.facts.front().allocator_identity_key, query::StableFingerprint128{});
    EXPECT_NE(gate.facts.front().prototype_identity_set_key, query::StableFingerprint128{});
    EXPECT_NE(gate.facts.front().drop_identity_key, gate.facts.front().allocator_identity_key);
}

TEST(QueryUnit, OwnedDynDropAllocatorIdentityGateRecordFunctionUpdatesSummary)
{
    const query::OwnedDynDropAllocatorIdentityGate source = baseline();
    query::OwnedDynDropAllocatorIdentityGate gate;
    gate.subject = "manual M20c owned dyn identity gate";
    gate.ir_shape_gate = source.ir_shape_gate;
    gate.ir_shape_gate_fingerprint = source.ir_shape_gate_fingerprint;

    query::record_owned_dyn_drop_allocator_identity_fact(gate, source.facts.front());
    EXPECT_EQ(gate.summary.fact_count, 1U);
    EXPECT_EQ(gate.summary.erased_drop_identity_count, 1U);
    EXPECT_TRUE(query::is_valid(gate));

    const query::StableFingerprint128 one_fact = query::owned_dyn_drop_allocator_identity_gate_fingerprint(gate);
    query::record_owned_dyn_drop_allocator_identity_fact(gate, source.facts[1]);
    refresh(gate);
    EXPECT_EQ(gate.summary.fact_count, 2U);
    EXPECT_EQ(gate.summary.allocator_identity_count, 1U);
    EXPECT_TRUE(query::is_valid(gate));
    EXPECT_FALSE(query::is_valid_m20c_owned_dyn_drop_allocator_identity_gate(gate));
    EXPECT_NE(gate.fingerprint, one_fact);
}

TEST(QueryUnit, OwnedDynDropAllocatorIdentityGateValidationRejectsIdentityDrift)
{
    const query::OwnedDynDropAllocatorIdentityGate gate = baseline();
    ASSERT_TRUE(query::is_valid_m20c_owned_dyn_drop_allocator_identity_gate(gate));

    const auto expect_invalid_first_fact = [&gate](auto&& mutate) {
        query::OwnedDynDropAllocatorIdentityGate drift = gate;
        mutate(drift.facts.front());
        refresh(drift);
        EXPECT_FALSE(query::is_valid(drift));
    };
    expect_invalid_first_fact([](query::OwnedDynDropAllocatorIdentityFact& fact) {
        fact.fact_name.clear();
    });
    expect_invalid_first_fact([](query::OwnedDynDropAllocatorIdentityFact& fact) {
        fact.subject_symbol.clear();
    });
    expect_invalid_first_fact([](query::OwnedDynDropAllocatorIdentityFact& fact) {
        fact.m20b_gate_fact.clear();
    });
    expect_invalid_first_fact([](query::OwnedDynDropAllocatorIdentityFact& fact) {
        fact.verifier_guard_fact.clear();
    });
    expect_invalid_first_fact([](query::OwnedDynDropAllocatorIdentityFact& fact) {
        fact.blocked_surface_fact.clear();
    });
    expect_invalid_first_fact([](query::OwnedDynDropAllocatorIdentityFact& fact) {
        fact.kind = static_cast<query::OwnedDynDropAllocatorIdentityFactKind>(QUERY_TEST_INVALID_ENUM_VALUE);
    });
    expect_invalid_first_fact([](query::OwnedDynDropAllocatorIdentityFact& fact) {
        fact.stage = query::OwnedDynDropAllocatorIdentityStage::blocked_future_runtime;
    });
    expect_invalid_first_fact([](query::OwnedDynDropAllocatorIdentityFact& fact) {
        fact.policy = query::OwnedDynDropAllocatorIdentityPolicy::runtime_lowering_not_implemented_v1;
    });
    expect_invalid_first_fact([](query::OwnedDynDropAllocatorIdentityFact& fact) {
        fact.references_m20b_ir_shape_gate = false;
    });
    expect_invalid_first_fact([](query::OwnedDynDropAllocatorIdentityFact& fact) {
        fact.compiler_owned_identity = false;
    });
    expect_invalid_first_fact([](query::OwnedDynDropAllocatorIdentityFact& fact) {
        fact.drop_identity_visible = false;
    });
    expect_invalid_first_fact([](query::OwnedDynDropAllocatorIdentityFact& fact) {
        fact.allocator_identity_visible = true;
    });
    expect_invalid_first_fact([](query::OwnedDynDropAllocatorIdentityFact& fact) {
        fact.borrowed_dyn_abi_unchanged = false;
    });
    expect_invalid_first_fact([](query::OwnedDynDropAllocatorIdentityFact& fact) {
        fact.standard_library_api_blocked = false;
    });
    expect_invalid_first_fact([](query::OwnedDynDropAllocatorIdentityFact& fact) {
        fact.box_dyn_surface_blocked = false;
    });
    expect_invalid_first_fact([](query::OwnedDynDropAllocatorIdentityFact& fact) {
        fact.owning_dyn_user_value_blocked = false;
    });
    expect_invalid_first_fact([](query::OwnedDynDropAllocatorIdentityFact& fact) {
        fact.allocator_api_blocked = false;
    });
    expect_invalid_first_fact([](query::OwnedDynDropAllocatorIdentityFact& fact) {
        fact.runtime_lowering_blocked = false;
    });
    expect_invalid_first_fact([](query::OwnedDynDropAllocatorIdentityFact& fact) {
        fact.dynamic_drop_runtime_blocked = false;
    });
    expect_invalid_first_fact([](query::OwnedDynDropAllocatorIdentityFact& fact) {
        fact.backend_helper_blocked = false;
    });
    expect_invalid_first_fact([](query::OwnedDynDropAllocatorIdentityFact& fact) {
        fact.executable_runtime_implemented = true;
    });
    expect_invalid_first_fact([](query::OwnedDynDropAllocatorIdentityFact& fact) {
        fact.drop_identity_key = query::StableFingerprint128{};
    });
    expect_invalid_first_fact([](query::OwnedDynDropAllocatorIdentityFact& fact) {
        fact.allocator_identity_key = fact.drop_identity_key;
    });
    expect_invalid_first_fact([](query::OwnedDynDropAllocatorIdentityFact& fact) {
        fact.prototype_identity_set_key = query::StableFingerprint128{};
    });
    expect_invalid_first_fact([](query::OwnedDynDropAllocatorIdentityFact& fact) {
        fact.layout_prototype_count = 0U;
    });

    const auto expect_invalid_fact_at = [&gate](const base::usize index, auto&& mutate) {
        query::OwnedDynDropAllocatorIdentityGate drift = gate;
        mutate(drift.facts[index]);
        refresh(drift);
        EXPECT_FALSE(query::is_valid(drift));
    };
    expect_invalid_fact_at(1U, [](query::OwnedDynDropAllocatorIdentityFact& fact) {
        fact.allocator_identity_visible = false;
    });
    expect_invalid_fact_at(2U, [](query::OwnedDynDropAllocatorIdentityFact& fact) {
        fact.cleanup_dropck_bridge_visible = false;
    });
    expect_invalid_fact_at(3U, [](query::OwnedDynDropAllocatorIdentityFact& fact) {
        fact.owned_handle_binding_visible = false;
    });
    expect_invalid_fact_at(4U, [](query::OwnedDynDropAllocatorIdentityFact& fact) {
        fact.drop_identity_visible = true;
    });
    expect_invalid_fact_at(1U, [](query::OwnedDynDropAllocatorIdentityFact& fact) {
        fact.layout_prototype_count = 2U;
    });
    expect_invalid_fact_at(1U, [](query::OwnedDynDropAllocatorIdentityFact& fact) {
        fact.prototype_identity_set_key = query::stable_fingerprint("stale identity set");
    });

    query::OwnedDynDropAllocatorIdentityGate empty_subject = gate;
    empty_subject.subject.clear();
    refresh(empty_subject);
    EXPECT_FALSE(query::is_valid(empty_subject));

    query::OwnedDynDropAllocatorIdentityGate stale_shape = gate;
    stale_shape.ir_shape_gate_fingerprint = query::stable_fingerprint("stale M20b");
    refresh(stale_shape);
    EXPECT_FALSE(query::is_valid(stale_shape));

    query::OwnedDynDropAllocatorIdentityGate stale_summary = gate;
    ++stale_summary.summary.fact_count;
    EXPECT_FALSE(query::is_valid(stale_summary));

    query::OwnedDynDropAllocatorIdentityGate stale_fingerprint = gate;
    stale_fingerprint.fingerprint = query::stable_fingerprint("stale M20c");
    EXPECT_FALSE(query::is_valid(stale_fingerprint));

    query::OwnedDynDropAllocatorIdentityGate duplicate_kind = gate;
    duplicate_kind.facts[1] = duplicate_kind.facts.front();
    refresh(duplicate_kind);
    EXPECT_TRUE(query::is_valid(duplicate_kind));
    EXPECT_FALSE(query::is_valid_m20c_owned_dyn_drop_allocator_identity_gate(duplicate_kind));
}

TEST(QueryUnit, OwnedDynDropAllocatorIdentityGateSummaryDumpAndFingerprintAreStable)
{
    const query::OwnedDynDropAllocatorIdentityGate gate = baseline();
    ASSERT_TRUE(query::is_valid_m20c_owned_dyn_drop_allocator_identity_gate(gate));

    const std::string summary = query::summarize_owned_dyn_drop_allocator_identity_gate(gate);
    EXPECT_TRUE(contains_text(summary, "owned_dyn_drop_allocator_identity_gate")) << summary;
    EXPECT_TRUE(contains_text(summary, "facts=5")) << summary;
    EXPECT_TRUE(contains_text(summary, "drop_identity=1")) << summary;
    EXPECT_TRUE(contains_text(summary, "allocator_identity=1")) << summary;
    EXPECT_TRUE(contains_text(summary, "cleanup_dropck_bridge=1")) << summary;
    EXPECT_TRUE(contains_text(summary, "handle_binding=1")) << summary;
    EXPECT_TRUE(contains_text(summary, "runtime_lowering_blocker=1")) << summary;
    EXPECT_TRUE(contains_text(summary, "allocator_api_blocked=5")) << summary;
    EXPECT_TRUE(contains_text(summary, "dynamic_drop_runtime_blocked=5")) << summary;
    EXPECT_TRUE(contains_text(summary, "executable_runtime_implemented=0")) << summary;

    const std::string dump = query::dump_owned_dyn_drop_allocator_identity_gate(gate);
    EXPECT_TRUE(contains_text(dump, "owned_dyn_erased_drop_identity_prerequisite_fact")) << dump;
    EXPECT_TRUE(contains_text(dump, "owned_dyn_allocator_identity_prerequisite_fact")) << dump;
    EXPECT_TRUE(contains_text(dump, "owned_dyn_cleanup_dropck_bridge_identity_fact")) << dump;
    EXPECT_TRUE(contains_text(dump, "owned_dyn_handle_identity_binding_fact")) << dump;
    EXPECT_TRUE(contains_text(dump, "owned_dyn_drop_allocator_runtime_lowering_blocker_fact")) << dump;
    EXPECT_TRUE(contains_text(dump, "requires_m20b_owned_dyn_ir_shape_prototype_gate")) << dump;
    EXPECT_TRUE(contains_text(dump, "dynamic_drop_runtime_not_in_m20c")) << dump;
    EXPECT_TRUE(contains_text(dump, "allocator_api_not_in_m20c")) << dump;
    EXPECT_TRUE(contains_text(dump, "drop_key=")) << dump;
    EXPECT_TRUE(contains_text(dump, "allocator_key=")) << dump;
    EXPECT_TRUE(contains_text(dump, "identity_set_key=")) << dump;

    query::OwnedDynDropAllocatorIdentityGate changed = gate;
    changed.facts.front().drop_identity_key = query::stable_fingerprint("changed drop identity");
    refresh(changed);
    EXPECT_NE(gate.fingerprint, changed.fingerprint);
}

} // namespace aurex::test
