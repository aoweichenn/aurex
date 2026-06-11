#include <aurex/infrastructure/query/owned_dyn_runtime_lowering_abi_gate.hpp>

#include <string>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

constexpr base::u8 QUERY_TEST_INVALID_ENUM_VALUE = 0xffU;

[[nodiscard]] bool contains_text(const std::string& value, const std::string_view text)
{
    return value.find(text) != std::string::npos;
}

[[nodiscard]] query::OwnedDynRuntimeLoweringAbiGate baseline()
{
    return query::m20d_owned_dyn_runtime_lowering_abi_gate_baseline();
}

void refresh(query::OwnedDynRuntimeLoweringAbiGate& gate)
{
    gate.summary = query::summarize_owned_dyn_runtime_lowering_abi_gate_counts(gate);
    gate.fingerprint = query::owned_dyn_runtime_lowering_abi_gate_fingerprint(gate);
}

} // namespace

TEST(QueryUnit, OwnedDynRuntimeLoweringAbiGateExposesEnumNamesAndInvalidFallbacks)
{
    EXPECT_EQ(query::owned_dyn_runtime_lowering_abi_fact_kind_name(
                  query::OwnedDynRuntimeLoweringAbiFactKind::runtime_abi_descriptor),
        "runtime_abi_descriptor");
    EXPECT_EQ(query::owned_dyn_runtime_lowering_abi_fact_kind_name(
                  query::OwnedDynRuntimeLoweringAbiFactKind::lowering_transition_guard),
        "lowering_transition_guard");
    EXPECT_EQ(query::owned_dyn_runtime_lowering_abi_fact_kind_name(
                  query::OwnedDynRuntimeLoweringAbiFactKind::backend_helper_prerequisite),
        "backend_helper_prerequisite");
    EXPECT_EQ(query::owned_dyn_runtime_lowering_abi_fact_kind_name(
                  query::OwnedDynRuntimeLoweringAbiFactKind::drop_allocator_runtime_bridge),
        "drop_allocator_runtime_bridge");
    EXPECT_EQ(query::owned_dyn_runtime_lowering_abi_fact_kind_name(
                  query::OwnedDynRuntimeLoweringAbiFactKind::dynamic_drop_runtime_blocker),
        "dynamic_drop_runtime_blocker");

    EXPECT_EQ(query::owned_dyn_runtime_lowering_abi_stage_name(
                  query::OwnedDynRuntimeLoweringAbiStage::abi_design_closure),
        "abi_design_closure");
    EXPECT_EQ(query::owned_dyn_runtime_lowering_abi_stage_name(
                  query::OwnedDynRuntimeLoweringAbiStage::verifier_lowering_guard),
        "verifier_lowering_guard");
    EXPECT_EQ(query::owned_dyn_runtime_lowering_abi_stage_name(
                  query::OwnedDynRuntimeLoweringAbiStage::blocked_future_runtime),
        "blocked_future_runtime");

    EXPECT_EQ(query::owned_dyn_runtime_lowering_abi_policy_name(
                  query::OwnedDynRuntimeLoweringAbiPolicy::compiler_owned_runtime_abi_descriptor_v1),
        "compiler_owned_runtime_abi_descriptor_v1");
    EXPECT_EQ(query::owned_dyn_runtime_lowering_abi_policy_name(
                  query::OwnedDynRuntimeLoweringAbiPolicy::blocked_to_admitted_transition_check_v1),
        "blocked_to_admitted_transition_check_v1");
    EXPECT_EQ(query::owned_dyn_runtime_lowering_abi_policy_name(
                  query::OwnedDynRuntimeLoweringAbiPolicy::backend_helper_identity_prerequisite_v1),
        "backend_helper_identity_prerequisite_v1");
    EXPECT_EQ(query::owned_dyn_runtime_lowering_abi_policy_name(
                  query::OwnedDynRuntimeLoweringAbiPolicy::drop_allocator_runtime_bridge_v1),
        "drop_allocator_runtime_bridge_v1");
    EXPECT_EQ(query::owned_dyn_runtime_lowering_abi_policy_name(
                  query::OwnedDynRuntimeLoweringAbiPolicy::dynamic_drop_runtime_not_implemented_v1),
        "dynamic_drop_runtime_not_implemented_v1");

    EXPECT_EQ(query::owned_dyn_runtime_lowering_abi_fact_kind_name(
                  static_cast<query::OwnedDynRuntimeLoweringAbiFactKind>(QUERY_TEST_INVALID_ENUM_VALUE)),
        "invalid");
    EXPECT_EQ(query::owned_dyn_runtime_lowering_abi_stage_name(
                  static_cast<query::OwnedDynRuntimeLoweringAbiStage>(QUERY_TEST_INVALID_ENUM_VALUE)),
        "invalid");
    EXPECT_EQ(query::owned_dyn_runtime_lowering_abi_policy_name(
                  static_cast<query::OwnedDynRuntimeLoweringAbiPolicy>(QUERY_TEST_INVALID_ENUM_VALUE)),
        "invalid");
    EXPECT_FALSE(
        query::is_valid(static_cast<query::OwnedDynRuntimeLoweringAbiFactKind>(QUERY_TEST_INVALID_ENUM_VALUE)));
    EXPECT_FALSE(query::is_valid(
        static_cast<query::OwnedDynRuntimeLoweringAbiStage>(QUERY_TEST_INVALID_ENUM_VALUE)));
    EXPECT_FALSE(query::is_valid(
        static_cast<query::OwnedDynRuntimeLoweringAbiPolicy>(QUERY_TEST_INVALID_ENUM_VALUE)));
}

TEST(QueryUnit, OwnedDynRuntimeLoweringAbiGateM20dBaselineRecordsClosedRuntimeAbiDesign)
{
    const query::OwnedDynRuntimeLoweringAbiGate gate = baseline();
    ASSERT_TRUE(query::is_valid(gate));
    EXPECT_TRUE(query::is_valid_m20d_owned_dyn_runtime_lowering_abi_gate(gate));
    EXPECT_EQ(gate.subject, "M20d Runtime Lowering ABI Design Closure");
    EXPECT_TRUE(query::is_valid_m20c_owned_dyn_drop_allocator_identity_gate(gate.drop_allocator_identity_gate));
    EXPECT_EQ(gate.drop_allocator_identity_gate_fingerprint, gate.drop_allocator_identity_gate.fingerprint);
    EXPECT_EQ(gate.fingerprint, query::owned_dyn_runtime_lowering_abi_gate_fingerprint(gate));

    EXPECT_EQ(gate.summary.fact_count, 5U);
    EXPECT_EQ(gate.summary.runtime_abi_descriptor_count, 1U);
    EXPECT_EQ(gate.summary.lowering_transition_guard_count, 1U);
    EXPECT_EQ(gate.summary.backend_helper_prerequisite_count, 1U);
    EXPECT_EQ(gate.summary.drop_allocator_runtime_bridge_count, 1U);
    EXPECT_EQ(gate.summary.dynamic_drop_runtime_blocker_count, 1U);
    EXPECT_EQ(gate.summary.m20c_reference_count, 5U);
    EXPECT_EQ(gate.summary.compiler_owned_runtime_abi_descriptor_count, 5U);
    EXPECT_EQ(gate.summary.runtime_abi_descriptor_visible_count, 4U);
    EXPECT_EQ(gate.summary.lowering_transition_guard_visible_count, 1U);
    EXPECT_EQ(gate.summary.backend_helper_prerequisite_visible_count, 1U);
    EXPECT_EQ(gate.summary.drop_allocator_runtime_bridge_visible_count, 1U);
    EXPECT_EQ(gate.summary.dynamic_drop_runtime_blocker_visible_count, 1U);
    EXPECT_EQ(gate.summary.standard_library_api_blocked_count, 5U);
    EXPECT_EQ(gate.summary.box_dyn_surface_blocked_count, 5U);
    EXPECT_EQ(gate.summary.owning_dyn_user_value_blocked_count, 5U);
    EXPECT_EQ(gate.summary.allocator_api_blocked_count, 5U);
    EXPECT_EQ(gate.summary.runtime_lowering_blocked_count, 5U);
    EXPECT_EQ(gate.summary.dynamic_drop_runtime_blocked_count, 5U);
    EXPECT_EQ(gate.summary.backend_helper_blocked_count, 5U);
    EXPECT_EQ(gate.summary.backend_helper_callable_count, 0U);
    EXPECT_EQ(gate.summary.executable_runtime_implemented_count, 0U);
    EXPECT_EQ(gate.summary.observed_layout_prototype_total, 1U);

    ASSERT_FALSE(gate.facts.empty());
    EXPECT_NE(gate.facts.front().runtime_abi_descriptor_key, query::StableFingerprint128{});
    EXPECT_NE(gate.facts.front().backend_helper_identity_key, query::StableFingerprint128{});
    EXPECT_NE(gate.facts.front().runtime_abi_descriptor_key, gate.facts.front().backend_helper_identity_key);
    EXPECT_NE(gate.facts.front().prototype_identity_set_key, query::StableFingerprint128{});
}

TEST(QueryUnit, OwnedDynRuntimeLoweringAbiGateRecordFunctionUpdatesSummary)
{
    const query::OwnedDynRuntimeLoweringAbiGate source = baseline();
    query::OwnedDynRuntimeLoweringAbiGate gate;
    gate.subject = "manual M20d runtime ABI gate";
    gate.drop_allocator_identity_gate = source.drop_allocator_identity_gate;
    gate.drop_allocator_identity_gate_fingerprint = source.drop_allocator_identity_gate_fingerprint;

    query::record_owned_dyn_runtime_lowering_abi_fact(gate, source.facts.front());
    EXPECT_EQ(gate.summary.fact_count, 1U);
    EXPECT_EQ(gate.summary.runtime_abi_descriptor_count, 1U);
    EXPECT_TRUE(query::is_valid(gate));

    const query::StableFingerprint128 one_fact = query::owned_dyn_runtime_lowering_abi_gate_fingerprint(gate);
    query::record_owned_dyn_runtime_lowering_abi_fact(gate, source.facts[1]);
    refresh(gate);
    EXPECT_EQ(gate.summary.fact_count, 2U);
    EXPECT_EQ(gate.summary.lowering_transition_guard_count, 1U);
    EXPECT_TRUE(query::is_valid(gate));
    EXPECT_FALSE(query::is_valid_m20d_owned_dyn_runtime_lowering_abi_gate(gate));
    EXPECT_NE(gate.fingerprint, one_fact);
}

TEST(QueryUnit, OwnedDynRuntimeLoweringAbiGateValidationRejectsRuntimeAbiDrift)
{
    const query::OwnedDynRuntimeLoweringAbiGate gate = baseline();
    ASSERT_TRUE(query::is_valid_m20d_owned_dyn_runtime_lowering_abi_gate(gate));

    const auto expect_invalid_first_fact = [&gate](auto&& mutate) {
        query::OwnedDynRuntimeLoweringAbiGate drift = gate;
        mutate(drift.facts.front());
        refresh(drift);
        EXPECT_FALSE(query::is_valid(drift));
    };
    expect_invalid_first_fact([](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.fact_name.clear();
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.subject_symbol.clear();
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.m20c_gate_fact.clear();
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.verifier_guard_fact.clear();
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.blocked_surface_fact.clear();
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.kind = static_cast<query::OwnedDynRuntimeLoweringAbiFactKind>(QUERY_TEST_INVALID_ENUM_VALUE);
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.stage = query::OwnedDynRuntimeLoweringAbiStage::blocked_future_runtime;
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.policy = query::OwnedDynRuntimeLoweringAbiPolicy::dynamic_drop_runtime_not_implemented_v1;
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.references_m20c_drop_allocator_identity_gate = false;
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.compiler_owned_runtime_abi_descriptor = false;
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.runtime_abi_descriptor_visible = false;
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.backend_helper_prerequisite_visible = true;
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.borrowed_dyn_abi_unchanged = false;
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.standard_library_api_blocked = false;
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.box_dyn_surface_blocked = false;
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.owning_dyn_user_value_blocked = false;
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.allocator_api_blocked = false;
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.runtime_lowering_blocked = false;
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.dynamic_drop_runtime_blocked = false;
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.backend_helper_blocked = false;
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.backend_helper_callable = true;
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.executable_runtime_implemented = true;
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.runtime_abi_descriptor_key = query::StableFingerprint128{};
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.backend_helper_identity_key = query::StableFingerprint128{};
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.backend_helper_identity_key = fact.runtime_abi_descriptor_key;
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.prototype_identity_set_key = query::StableFingerprint128{};
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.drop_identity_key = query::StableFingerprint128{};
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.allocator_identity_key = fact.drop_identity_key;
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.layout_prototype_count = 0U;
    });

    const auto expect_invalid_fact_at = [&gate](const base::usize index, auto&& mutate) {
        query::OwnedDynRuntimeLoweringAbiGate drift = gate;
        mutate(drift.facts[index]);
        refresh(drift);
        EXPECT_FALSE(query::is_valid(drift));
    };
    expect_invalid_fact_at(1U, [](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.lowering_transition_guard_visible = false;
    });
    expect_invalid_fact_at(2U, [](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.backend_helper_prerequisite_visible = false;
    });
    expect_invalid_fact_at(3U, [](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.drop_allocator_runtime_bridge_visible = false;
    });
    expect_invalid_fact_at(4U, [](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.dynamic_drop_runtime_blocker_visible = false;
    });
    expect_invalid_fact_at(1U, [](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.runtime_abi_descriptor_key = query::stable_fingerprint("stale runtime ABI descriptor");
    });
    expect_invalid_fact_at(1U, [](query::OwnedDynRuntimeLoweringAbiFact& fact) {
        fact.backend_helper_identity_key = query::stable_fingerprint("stale backend helper");
    });

    query::OwnedDynRuntimeLoweringAbiGate empty_subject = gate;
    empty_subject.subject.clear();
    refresh(empty_subject);
    EXPECT_FALSE(query::is_valid(empty_subject));

    query::OwnedDynRuntimeLoweringAbiGate stale_m20c = gate;
    stale_m20c.drop_allocator_identity_gate_fingerprint = query::stable_fingerprint("stale M20c");
    refresh(stale_m20c);
    EXPECT_FALSE(query::is_valid(stale_m20c));

    query::OwnedDynRuntimeLoweringAbiGate stale_summary = gate;
    ++stale_summary.summary.fact_count;
    EXPECT_FALSE(query::is_valid(stale_summary));

    query::OwnedDynRuntimeLoweringAbiGate stale_fingerprint = gate;
    stale_fingerprint.fingerprint = query::stable_fingerprint("stale M20d");
    EXPECT_FALSE(query::is_valid(stale_fingerprint));

    query::OwnedDynRuntimeLoweringAbiGate duplicate_kind = gate;
    duplicate_kind.facts[1] = duplicate_kind.facts.front();
    refresh(duplicate_kind);
    EXPECT_TRUE(query::is_valid(duplicate_kind));
    EXPECT_FALSE(query::is_valid_m20d_owned_dyn_runtime_lowering_abi_gate(duplicate_kind));
}

TEST(QueryUnit, OwnedDynRuntimeLoweringAbiGateSummaryDumpAndFingerprintAreStable)
{
    const query::OwnedDynRuntimeLoweringAbiGate gate = baseline();
    ASSERT_TRUE(query::is_valid_m20d_owned_dyn_runtime_lowering_abi_gate(gate));

    const std::string summary = query::summarize_owned_dyn_runtime_lowering_abi_gate(gate);
    EXPECT_TRUE(contains_text(summary, "owned_dyn_runtime_lowering_abi_gate")) << summary;
    EXPECT_TRUE(contains_text(summary, "facts=5")) << summary;
    EXPECT_TRUE(contains_text(summary, "runtime_abi_descriptor=1")) << summary;
    EXPECT_TRUE(contains_text(summary, "transition_guard=1")) << summary;
    EXPECT_TRUE(contains_text(summary, "backend_helper_prerequisite=1")) << summary;
    EXPECT_TRUE(contains_text(summary, "drop_allocator_bridge=1")) << summary;
    EXPECT_TRUE(contains_text(summary, "dynamic_drop_blocker=1")) << summary;
    EXPECT_TRUE(contains_text(summary, "backend_helper_callable=0")) << summary;
    EXPECT_TRUE(contains_text(summary, "executable_runtime_implemented=0")) << summary;

    const std::string dump = query::dump_owned_dyn_runtime_lowering_abi_gate(gate);
    EXPECT_TRUE(contains_text(dump, "owned_dyn_runtime_abi_descriptor_fact")) << dump;
    EXPECT_TRUE(contains_text(dump, "owned_dyn_blocked_to_admitted_transition_guard_fact")) << dump;
    EXPECT_TRUE(contains_text(dump, "owned_dyn_backend_helper_prerequisite_fact")) << dump;
    EXPECT_TRUE(contains_text(dump, "owned_dyn_drop_allocator_runtime_bridge_fact")) << dump;
    EXPECT_TRUE(contains_text(dump, "owned_dyn_dynamic_drop_runtime_blocker_fact")) << dump;
    EXPECT_TRUE(contains_text(dump, "requires_m20c_owned_dyn_drop_allocator_identity_gate")) << dump;
    EXPECT_TRUE(contains_text(dump, "runtime_abi_descriptor_key=")) << dump;
    EXPECT_TRUE(contains_text(dump, "backend_helper_identity_key=")) << dump;
    EXPECT_TRUE(contains_text(dump, "backend_helper_callable=no")) << dump;
    EXPECT_TRUE(contains_text(dump, "runtime_abi_lowering_not_executable_in_m20d")) << dump;

    query::OwnedDynRuntimeLoweringAbiGate changed = gate;
    changed.facts.front().runtime_abi_descriptor_key =
        query::stable_fingerprint("changed runtime ABI descriptor");
    refresh(changed);
    EXPECT_NE(changed.fingerprint, gate.fingerprint);
}

} // namespace aurex::test
