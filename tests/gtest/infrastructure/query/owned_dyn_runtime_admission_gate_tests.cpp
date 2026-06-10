#include <aurex/infrastructure/query/owned_dyn_runtime_admission_gate.hpp>

#include <string>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

constexpr base::u8 QUERY_TEST_INVALID_ENUM_VALUE = 0xffU;

[[nodiscard]] bool contains_text(const std::string& value, const std::string_view text)
{
    return value.find(text) != std::string::npos;
}

[[nodiscard]] query::OwnedDynRuntimeAdmissionGate baseline()
{
    return query::m20_owned_dyn_runtime_admission_gate_baseline();
}

void refresh(query::OwnedDynRuntimeAdmissionGate& gate)
{
    gate.summary = query::summarize_owned_dyn_runtime_admission_gate_counts(gate);
    gate.fingerprint = query::owned_dyn_runtime_admission_gate_fingerprint(gate);
}

} // namespace

TEST(QueryUnit, OwnedDynRuntimeAdmissionGateExposesEnumNamesAndInvalidFallbacks)
{
    EXPECT_EQ(query::owned_dyn_runtime_admission_capability_name(
                  query::OwnedDynRuntimeAdmissionCapability::owned_object_layout),
        "owned_object_layout");
    EXPECT_EQ(query::owned_dyn_runtime_admission_capability_name(
                  query::OwnedDynRuntimeAdmissionCapability::erased_drop_identity),
        "erased_drop_identity");
    EXPECT_EQ(query::owned_dyn_runtime_admission_capability_name(
                  query::OwnedDynRuntimeAdmissionCapability::allocator_identity),
        "allocator_identity");
    EXPECT_EQ(query::owned_dyn_runtime_admission_capability_name(
                  query::OwnedDynRuntimeAdmissionCapability::runtime_lowering_abi),
        "runtime_lowering_abi");
    EXPECT_EQ(query::owned_dyn_runtime_admission_capability_name(
                  query::OwnedDynRuntimeAdmissionCapability::box_dyn_surface),
        "box_dyn_surface");
    EXPECT_EQ(query::owned_dyn_runtime_admission_capability_name(
                  query::OwnedDynRuntimeAdmissionCapability::borrowed_dyn_abi_separation),
        "borrowed_dyn_abi_separation");

    EXPECT_EQ(query::owned_dyn_runtime_admission_stage_name(
                  query::OwnedDynRuntimeAdmissionStage::admission_design_gate),
        "admission_design_gate");
    EXPECT_EQ(query::owned_dyn_runtime_admission_stage_name(
                  query::OwnedDynRuntimeAdmissionStage::ir_shape_prerequisite),
        "ir_shape_prerequisite");
    EXPECT_EQ(query::owned_dyn_runtime_admission_stage_name(
                  query::OwnedDynRuntimeAdmissionStage::runtime_identity_prerequisite),
        "runtime_identity_prerequisite");
    EXPECT_EQ(query::owned_dyn_runtime_admission_stage_name(
                  query::OwnedDynRuntimeAdmissionStage::standard_library_api_prerequisite),
        "standard_library_api_prerequisite");
    EXPECT_EQ(query::owned_dyn_runtime_admission_stage_name(
                  query::OwnedDynRuntimeAdmissionStage::blocked_future_implementation),
        "blocked_future_implementation");

    EXPECT_EQ(query::owned_dyn_runtime_admission_policy_name(
                  query::OwnedDynRuntimeAdmissionPolicy::owned_handle_metadata_v1),
        "owned_handle_metadata_v1");
    EXPECT_EQ(query::owned_dyn_runtime_admission_policy_name(
                  query::OwnedDynRuntimeAdmissionPolicy::erased_drop_identity_v1),
        "erased_drop_identity_v1");
    EXPECT_EQ(query::owned_dyn_runtime_admission_policy_name(
                  query::OwnedDynRuntimeAdmissionPolicy::allocator_identity_v1),
        "allocator_identity_v1");
    EXPECT_EQ(query::owned_dyn_runtime_admission_policy_name(
                  query::OwnedDynRuntimeAdmissionPolicy::runtime_lowering_abi_v1),
        "runtime_lowering_abi_v1");
    EXPECT_EQ(query::owned_dyn_runtime_admission_policy_name(
                  query::OwnedDynRuntimeAdmissionPolicy::box_dyn_surface_v1),
        "box_dyn_surface_v1");
    EXPECT_EQ(query::owned_dyn_runtime_admission_policy_name(
                  query::OwnedDynRuntimeAdmissionPolicy::borrowed_dyn_remains_destructor_free_v1),
        "borrowed_dyn_remains_destructor_free_v1");

    EXPECT_EQ(query::owned_dyn_runtime_admission_capability_name(
                  static_cast<query::OwnedDynRuntimeAdmissionCapability>(QUERY_TEST_INVALID_ENUM_VALUE)),
        "invalid");
    EXPECT_EQ(query::owned_dyn_runtime_admission_stage_name(
                  static_cast<query::OwnedDynRuntimeAdmissionStage>(QUERY_TEST_INVALID_ENUM_VALUE)),
        "invalid");
    EXPECT_EQ(query::owned_dyn_runtime_admission_policy_name(
                  static_cast<query::OwnedDynRuntimeAdmissionPolicy>(QUERY_TEST_INVALID_ENUM_VALUE)),
        "invalid");
    EXPECT_FALSE(query::is_valid(
        static_cast<query::OwnedDynRuntimeAdmissionCapability>(QUERY_TEST_INVALID_ENUM_VALUE)));
    EXPECT_FALSE(query::is_valid(
        static_cast<query::OwnedDynRuntimeAdmissionStage>(QUERY_TEST_INVALID_ENUM_VALUE)));
    EXPECT_FALSE(query::is_valid(
        static_cast<query::OwnedDynRuntimeAdmissionPolicy>(QUERY_TEST_INVALID_ENUM_VALUE)));
}

TEST(QueryUnit, OwnedDynRuntimeAdmissionGateM20BaselineRecordsAdmissionPlan)
{
    const query::OwnedDynRuntimeAdmissionGate gate = baseline();
    ASSERT_TRUE(query::is_valid(gate));
    EXPECT_TRUE(query::is_valid_m20_owned_dyn_runtime_admission_gate(gate));
    EXPECT_EQ(gate.subject, "M20a Owned Dyn Runtime Admission Design Gate");
    EXPECT_TRUE(query::is_valid_m17_dyn_ownership_runtime_preparation_baseline(gate.runtime_facts));
    EXPECT_TRUE(query::is_valid_m18_dyn_ownership_runtime_boundary_gate(gate.boundary_gate));
    EXPECT_TRUE(query::is_valid_m19_dyn_ownership_runtime_ir_verifier_baseline(gate.ir_verifier_facts));
    EXPECT_EQ(gate.runtime_facts_fingerprint, gate.runtime_facts.fingerprint);
    EXPECT_EQ(gate.boundary_gate_fingerprint, gate.boundary_gate.fingerprint);
    EXPECT_EQ(gate.ir_verifier_facts_fingerprint, gate.ir_verifier_facts.fingerprint);
    EXPECT_EQ(gate.fingerprint, query::owned_dyn_runtime_admission_gate_fingerprint(gate));

    EXPECT_EQ(gate.summary.fact_count, 6U);
    EXPECT_EQ(gate.summary.owned_object_layout_count, 1U);
    EXPECT_EQ(gate.summary.erased_drop_identity_count, 1U);
    EXPECT_EQ(gate.summary.allocator_identity_count, 1U);
    EXPECT_EQ(gate.summary.runtime_lowering_abi_count, 1U);
    EXPECT_EQ(gate.summary.box_dyn_surface_count, 1U);
    EXPECT_EQ(gate.summary.borrowed_dyn_abi_separation_count, 1U);
    EXPECT_EQ(gate.summary.m17_reference_count, 6U);
    EXPECT_EQ(gate.summary.m18_reference_count, 6U);
    EXPECT_EQ(gate.summary.m19_reference_count, 6U);
    EXPECT_EQ(gate.summary.standard_library_api_blocked_count, 6U);
    EXPECT_EQ(gate.summary.box_dyn_surface_blocked_count, 6U);
    EXPECT_EQ(gate.summary.owning_dyn_user_value_blocked_count, 6U);
    EXPECT_EQ(gate.summary.allocator_api_blocked_count, 6U);
    EXPECT_EQ(gate.summary.runtime_lowering_blocked_count, 6U);
    EXPECT_EQ(gate.summary.dynamic_drop_runtime_blocked_count, 6U);
    EXPECT_EQ(gate.summary.borrowed_dyn_abi_unchanged_count, 6U);
    EXPECT_EQ(gate.summary.owned_layout_required_count, 5U);
    EXPECT_EQ(gate.summary.erased_drop_identity_required_count, 3U);
    EXPECT_EQ(gate.summary.allocator_identity_required_count, 3U);
    EXPECT_EQ(gate.summary.runtime_abi_required_count, 1U);
    EXPECT_EQ(gate.summary.backend_helper_required_count, 1U);
    EXPECT_EQ(gate.summary.executable_surface_implemented_count, 0U);
}

TEST(QueryUnit, OwnedDynRuntimeAdmissionGateRecordFunctionUpdatesSummary)
{
    const query::OwnedDynRuntimeAdmissionGate source = baseline();
    query::OwnedDynRuntimeAdmissionGate gate;
    gate.subject = "manual M20a owned dyn runtime admission gate";
    gate.runtime_facts = source.runtime_facts;
    gate.runtime_facts_fingerprint = source.runtime_facts_fingerprint;
    gate.boundary_gate = source.boundary_gate;
    gate.boundary_gate_fingerprint = source.boundary_gate_fingerprint;
    gate.ir_verifier_facts = source.ir_verifier_facts;
    gate.ir_verifier_facts_fingerprint = source.ir_verifier_facts_fingerprint;

    query::record_owned_dyn_runtime_admission_fact(gate, source.admissions.front());
    EXPECT_EQ(gate.summary.fact_count, 1U);
    EXPECT_EQ(gate.summary.owned_object_layout_count, 1U);
    EXPECT_TRUE(query::is_valid(gate));

    const query::StableFingerprint128 one_fact =
        query::owned_dyn_runtime_admission_gate_fingerprint(gate);
    query::record_owned_dyn_runtime_admission_fact(gate, source.admissions[1]);
    refresh(gate);
    EXPECT_EQ(gate.summary.fact_count, 2U);
    EXPECT_EQ(gate.summary.erased_drop_identity_count, 1U);
    EXPECT_TRUE(query::is_valid(gate));
    EXPECT_FALSE(query::is_valid_m20_owned_dyn_runtime_admission_gate(gate));
    EXPECT_NE(gate.fingerprint, one_fact);
}

TEST(QueryUnit, OwnedDynRuntimeAdmissionGateValidationRejectsAdmissionDrift)
{
    const query::OwnedDynRuntimeAdmissionGate gate = baseline();
    ASSERT_TRUE(query::is_valid_m20_owned_dyn_runtime_admission_gate(gate));

    const auto expect_invalid_first_fact = [&gate](auto&& mutate) {
        query::OwnedDynRuntimeAdmissionGate drift = gate;
        mutate(drift.admissions.front());
        refresh(drift);
        EXPECT_FALSE(query::is_valid(drift));
    };
    expect_invalid_first_fact([](query::OwnedDynRuntimeAdmissionFact& fact) {
        fact.fact_name.clear();
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeAdmissionFact& fact) {
        fact.admission_fact.clear();
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeAdmissionFact& fact) {
        fact.required_input_fact.clear();
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeAdmissionFact& fact) {
        fact.capability =
            static_cast<query::OwnedDynRuntimeAdmissionCapability>(QUERY_TEST_INVALID_ENUM_VALUE);
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeAdmissionFact& fact) {
        fact.stage = query::OwnedDynRuntimeAdmissionStage::blocked_future_implementation;
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeAdmissionFact& fact) {
        fact.policy = query::OwnedDynRuntimeAdmissionPolicy::box_dyn_surface_v1;
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeAdmissionFact& fact) {
        fact.references_m17_runtime_facts = false;
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeAdmissionFact& fact) {
        fact.references_m18_boundary_gate = false;
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeAdmissionFact& fact) {
        fact.references_m19_ir_verifier_facts = false;
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeAdmissionFact& fact) {
        fact.standard_library_api_blocked = false;
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeAdmissionFact& fact) {
        fact.box_dyn_surface_blocked = false;
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeAdmissionFact& fact) {
        fact.owning_dyn_user_value_blocked = false;
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeAdmissionFact& fact) {
        fact.allocator_api_blocked = false;
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeAdmissionFact& fact) {
        fact.runtime_lowering_blocked = false;
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeAdmissionFact& fact) {
        fact.dynamic_drop_runtime_blocked = false;
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeAdmissionFact& fact) {
        fact.borrowed_dyn_abi_unchanged = false;
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeAdmissionFact& fact) {
        fact.owned_layout_required = false;
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeAdmissionFact& fact) {
        fact.erased_drop_identity_required = true;
    });
    expect_invalid_first_fact([](query::OwnedDynRuntimeAdmissionFact& fact) {
        fact.executable_surface_implemented = true;
    });

    const auto expect_invalid_fact_at =
        [&gate](const base::usize index, auto&& mutate) {
            query::OwnedDynRuntimeAdmissionGate drift = gate;
            mutate(drift.admissions[index]);
            refresh(drift);
            EXPECT_FALSE(query::is_valid(drift));
        };
    expect_invalid_fact_at(1U, [](query::OwnedDynRuntimeAdmissionFact& fact) {
        fact.erased_drop_identity_required = false;
    });
    expect_invalid_fact_at(2U, [](query::OwnedDynRuntimeAdmissionFact& fact) {
        fact.allocator_identity_required = false;
    });
    expect_invalid_fact_at(3U, [](query::OwnedDynRuntimeAdmissionFact& fact) {
        fact.runtime_abi_required = false;
    });
    expect_invalid_fact_at(3U, [](query::OwnedDynRuntimeAdmissionFact& fact) {
        fact.backend_helper_required = false;
    });
    expect_invalid_fact_at(4U, [](query::OwnedDynRuntimeAdmissionFact& fact) {
        fact.runtime_abi_required = true;
    });
    expect_invalid_fact_at(5U, [](query::OwnedDynRuntimeAdmissionFact& fact) {
        fact.owned_layout_required = true;
    });

    query::OwnedDynRuntimeAdmissionGate empty_subject = gate;
    empty_subject.subject.clear();
    refresh(empty_subject);
    EXPECT_FALSE(query::is_valid(empty_subject));

    query::OwnedDynRuntimeAdmissionGate stale_runtime = gate;
    stale_runtime.runtime_facts_fingerprint = query::stable_fingerprint("stale M17");
    refresh(stale_runtime);
    EXPECT_FALSE(query::is_valid(stale_runtime));

    query::OwnedDynRuntimeAdmissionGate stale_boundary = gate;
    stale_boundary.boundary_gate_fingerprint = query::stable_fingerprint("stale M18");
    refresh(stale_boundary);
    EXPECT_FALSE(query::is_valid(stale_boundary));

    query::OwnedDynRuntimeAdmissionGate stale_ir = gate;
    stale_ir.ir_verifier_facts_fingerprint = query::stable_fingerprint("stale M19");
    refresh(stale_ir);
    EXPECT_FALSE(query::is_valid(stale_ir));

    query::OwnedDynRuntimeAdmissionGate stale_summary = gate;
    ++stale_summary.summary.fact_count;
    EXPECT_FALSE(query::is_valid(stale_summary));

    query::OwnedDynRuntimeAdmissionGate stale_fingerprint = gate;
    stale_fingerprint.fingerprint = query::stable_fingerprint("stale M20");
    EXPECT_FALSE(query::is_valid(stale_fingerprint));

    query::OwnedDynRuntimeAdmissionGate duplicate_capability = gate;
    duplicate_capability.admissions[1] = duplicate_capability.admissions.front();
    refresh(duplicate_capability);
    EXPECT_TRUE(query::is_valid(duplicate_capability));
    EXPECT_FALSE(query::is_valid_m20_owned_dyn_runtime_admission_gate(duplicate_capability));
}

TEST(QueryUnit, OwnedDynRuntimeAdmissionGateSummaryDumpAndFingerprintAreStable)
{
    const query::OwnedDynRuntimeAdmissionGate gate = baseline();
    ASSERT_TRUE(query::is_valid_m20_owned_dyn_runtime_admission_gate(gate));

    const std::string summary = query::summarize_owned_dyn_runtime_admission_gate(gate);
    EXPECT_TRUE(contains_text(summary, "owned_dyn_runtime_admission_gate")) << summary;
    EXPECT_TRUE(contains_text(summary, "facts=6")) << summary;
    EXPECT_TRUE(contains_text(summary, "owned_layout=1")) << summary;
    EXPECT_TRUE(contains_text(summary, "runtime_lowering_abi=1")) << summary;
    EXPECT_TRUE(contains_text(summary, "box_dyn_surface=1")) << summary;
    EXPECT_TRUE(contains_text(summary, "standard_library_api_blocked=6")) << summary;
    EXPECT_TRUE(contains_text(summary, "dynamic_drop_runtime_blocked=6")) << summary;
    EXPECT_TRUE(contains_text(summary, "executable_surface_implemented=0")) << summary;

    const std::string dump = query::dump_owned_dyn_runtime_admission_gate(gate);
    EXPECT_TRUE(contains_text(dump, "owned_dyn_object_layout_admission_fact")) << dump;
    EXPECT_TRUE(contains_text(dump, "erased_drop_identity_admission_fact")) << dump;
    EXPECT_TRUE(contains_text(dump, "allocator_identity_admission_fact")) << dump;
    EXPECT_TRUE(contains_text(dump, "runtime_lowering_abi_admission_fact")) << dump;
    EXPECT_TRUE(contains_text(dump, "box_dyn_surface_admission_fact")) << dump;
    EXPECT_TRUE(contains_text(dump, "borrowed_dyn_abi_separation_fact")) << dump;
    EXPECT_TRUE(contains_text(dump, "requires_m17_dyn_ownership_runtime_facts")) << dump;
    EXPECT_TRUE(contains_text(dump, "requires_m18_dyn_ownership_runtime_boundary_gate")) << dump;
    EXPECT_TRUE(contains_text(dump, "requires_m19_dyn_ownership_runtime_ir_verifier_facts")) << dump;
    EXPECT_TRUE(contains_text(dump, "standard_library_api_not_in_m20a")) << dump;
    EXPECT_TRUE(contains_text(dump, "runtime_abi_lowering_not_in_m20a")) << dump;
    EXPECT_TRUE(contains_text(dump, "borrowed_dyn_vtable_remains_destructor_free")) << dump;

    query::OwnedDynRuntimeAdmissionGate changed = gate;
    changed.admissions.front().next_stage_fact = "m20b:owned-layout-v2";
    refresh(changed);
    EXPECT_TRUE(query::is_valid(changed));
    EXPECT_NE(changed.fingerprint, gate.fingerprint);
}

} // namespace aurex::test
