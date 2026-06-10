#include <aurex/infrastructure/query/owned_dyn_ir_shape_prototype_gate.hpp>

#include <string>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

constexpr base::u8 QUERY_TEST_INVALID_ENUM_VALUE = 0xffU;
constexpr base::u32 QUERY_TEST_WRONG_HANDLE_FIELD_COUNT = 3U;

[[nodiscard]] bool contains_text(const std::string& value, const std::string_view text)
{
    return value.find(text) != std::string::npos;
}

[[nodiscard]] query::OwnedDynIrShapePrototypeGate baseline()
{
    return query::m20b_owned_dyn_ir_shape_prototype_gate_baseline();
}

void refresh(query::OwnedDynIrShapePrototypeGate& gate)
{
    gate.summary = query::summarize_owned_dyn_ir_shape_prototype_gate_counts(gate);
    gate.fingerprint = query::owned_dyn_ir_shape_prototype_gate_fingerprint(gate);
}

} // namespace

TEST(QueryUnit, OwnedDynIrShapePrototypeGateExposesEnumNamesAndInvalidFallbacks)
{
    EXPECT_EQ(query::owned_dyn_ir_shape_prototype_fact_kind_name(
                  query::OwnedDynIrShapePrototypeFactKind::owned_handle_metadata),
        "owned_handle_metadata");
    EXPECT_EQ(query::owned_dyn_ir_shape_prototype_fact_kind_name(
                  query::OwnedDynIrShapePrototypeFactKind::erased_payload_pointer),
        "erased_payload_pointer");
    EXPECT_EQ(query::owned_dyn_ir_shape_prototype_fact_kind_name(
                  query::OwnedDynIrShapePrototypeFactKind::vtable_pointer_metadata),
        "vtable_pointer_metadata");
    EXPECT_EQ(query::owned_dyn_ir_shape_prototype_fact_kind_name(
                  query::OwnedDynIrShapePrototypeFactKind::drop_identity_placeholder),
        "drop_identity_placeholder");
    EXPECT_EQ(query::owned_dyn_ir_shape_prototype_fact_kind_name(
                  query::OwnedDynIrShapePrototypeFactKind::allocator_identity_placeholder),
        "allocator_identity_placeholder");
    EXPECT_EQ(query::owned_dyn_ir_shape_prototype_fact_kind_name(
                  query::OwnedDynIrShapePrototypeFactKind::runtime_lowering_blocker),
        "runtime_lowering_blocker");

    EXPECT_EQ(query::owned_dyn_ir_shape_prototype_stage_name(
                  query::OwnedDynIrShapePrototypeStage::ir_shape_prototype),
        "ir_shape_prototype");
    EXPECT_EQ(query::owned_dyn_ir_shape_prototype_stage_name(
                  query::OwnedDynIrShapePrototypeStage::verifier_shape_guard),
        "verifier_shape_guard");
    EXPECT_EQ(query::owned_dyn_ir_shape_prototype_stage_name(
                  query::OwnedDynIrShapePrototypeStage::blocked_future_runtime),
        "blocked_future_runtime");

    EXPECT_EQ(query::owned_dyn_ir_shape_prototype_policy_name(
                  query::OwnedDynIrShapePrototypePolicy::owned_handle_two_field_v1),
        "owned_handle_two_field_v1");
    EXPECT_EQ(query::owned_dyn_ir_shape_prototype_policy_name(
                  query::OwnedDynIrShapePrototypePolicy::erased_payload_pointer_v1),
        "erased_payload_pointer_v1");
    EXPECT_EQ(query::owned_dyn_ir_shape_prototype_policy_name(
                  query::OwnedDynIrShapePrototypePolicy::borrowed_vtable_pointer_unchanged_v1),
        "borrowed_vtable_pointer_unchanged_v1");
    EXPECT_EQ(query::owned_dyn_ir_shape_prototype_policy_name(
                  query::OwnedDynIrShapePrototypePolicy::drop_identity_not_lowered_v1),
        "drop_identity_not_lowered_v1");
    EXPECT_EQ(query::owned_dyn_ir_shape_prototype_policy_name(
                  query::OwnedDynIrShapePrototypePolicy::allocator_identity_not_lowered_v1),
        "allocator_identity_not_lowered_v1");
    EXPECT_EQ(query::owned_dyn_ir_shape_prototype_policy_name(
                  query::OwnedDynIrShapePrototypePolicy::runtime_lowering_not_implemented_v1),
        "runtime_lowering_not_implemented_v1");

    EXPECT_EQ(query::owned_dyn_ir_shape_prototype_fact_kind_name(
                  static_cast<query::OwnedDynIrShapePrototypeFactKind>(QUERY_TEST_INVALID_ENUM_VALUE)),
        "invalid");
    EXPECT_EQ(query::owned_dyn_ir_shape_prototype_stage_name(
                  static_cast<query::OwnedDynIrShapePrototypeStage>(QUERY_TEST_INVALID_ENUM_VALUE)),
        "invalid");
    EXPECT_EQ(query::owned_dyn_ir_shape_prototype_policy_name(
                  static_cast<query::OwnedDynIrShapePrototypePolicy>(QUERY_TEST_INVALID_ENUM_VALUE)),
        "invalid");
    EXPECT_FALSE(query::is_valid(
        static_cast<query::OwnedDynIrShapePrototypeFactKind>(QUERY_TEST_INVALID_ENUM_VALUE)));
    EXPECT_FALSE(query::is_valid(
        static_cast<query::OwnedDynIrShapePrototypeStage>(QUERY_TEST_INVALID_ENUM_VALUE)));
    EXPECT_FALSE(query::is_valid(
        static_cast<query::OwnedDynIrShapePrototypePolicy>(QUERY_TEST_INVALID_ENUM_VALUE)));
}

TEST(QueryUnit, OwnedDynIrShapePrototypeGateM20bBaselineRecordsCompilerOwnedShape)
{
    const query::OwnedDynIrShapePrototypeGate gate = baseline();
    ASSERT_TRUE(query::is_valid(gate));
    EXPECT_TRUE(query::is_valid_m20b_owned_dyn_ir_shape_prototype_gate(gate));
    EXPECT_EQ(gate.subject, "M20b Owned Dyn IR Shape Prototype Gate");
    EXPECT_TRUE(query::is_valid_m20_owned_dyn_runtime_admission_gate(gate.admission_gate));
    EXPECT_EQ(gate.admission_gate_fingerprint, gate.admission_gate.fingerprint);
    EXPECT_EQ(gate.fingerprint, query::owned_dyn_ir_shape_prototype_gate_fingerprint(gate));

    EXPECT_EQ(gate.summary.fact_count, 6U);
    EXPECT_EQ(gate.summary.owned_handle_metadata_count, 1U);
    EXPECT_EQ(gate.summary.erased_payload_pointer_count, 1U);
    EXPECT_EQ(gate.summary.vtable_pointer_metadata_count, 1U);
    EXPECT_EQ(gate.summary.drop_identity_placeholder_count, 1U);
    EXPECT_EQ(gate.summary.allocator_identity_placeholder_count, 1U);
    EXPECT_EQ(gate.summary.runtime_lowering_blocker_count, 1U);
    EXPECT_EQ(gate.summary.m20a_reference_count, 6U);
    EXPECT_EQ(gate.summary.compiler_owned_ir_shape_count, 6U);
    EXPECT_EQ(gate.summary.owned_layout_prototype_visible_count, 6U);
    EXPECT_EQ(gate.summary.handle_metadata_visible_count, 1U);
    EXPECT_EQ(gate.summary.erased_payload_pointer_visible_count, 1U);
    EXPECT_EQ(gate.summary.vtable_pointer_visible_count, 1U);
    EXPECT_EQ(gate.summary.drop_identity_placeholder_visible_count, 1U);
    EXPECT_EQ(gate.summary.allocator_identity_placeholder_visible_count, 1U);
    EXPECT_EQ(gate.summary.borrowed_dyn_abi_unchanged_count, 6U);
    EXPECT_EQ(gate.summary.standard_library_api_blocked_count, 6U);
    EXPECT_EQ(gate.summary.box_dyn_surface_blocked_count, 6U);
    EXPECT_EQ(gate.summary.owning_dyn_user_value_blocked_count, 6U);
    EXPECT_EQ(gate.summary.allocator_api_blocked_count, 6U);
    EXPECT_EQ(gate.summary.runtime_lowering_blocked_count, 6U);
    EXPECT_EQ(gate.summary.dynamic_drop_runtime_blocked_count, 6U);
    EXPECT_EQ(gate.summary.backend_helper_blocked_count, 6U);
    EXPECT_EQ(gate.summary.executable_runtime_implemented_count, 0U);
    EXPECT_EQ(gate.summary.observed_layout_prototype_total, 1U);
}

TEST(QueryUnit, OwnedDynIrShapePrototypeGateRecordFunctionUpdatesSummary)
{
    const query::OwnedDynIrShapePrototypeGate source = baseline();
    query::OwnedDynIrShapePrototypeGate gate;
    gate.subject = "manual M20b owned dyn IR shape prototype gate";
    gate.admission_gate = source.admission_gate;
    gate.admission_gate_fingerprint = source.admission_gate_fingerprint;

    query::record_owned_dyn_ir_shape_prototype_fact(gate, source.facts.front());
    EXPECT_EQ(gate.summary.fact_count, 1U);
    EXPECT_EQ(gate.summary.owned_handle_metadata_count, 1U);
    EXPECT_TRUE(query::is_valid(gate));

    const query::StableFingerprint128 one_fact =
        query::owned_dyn_ir_shape_prototype_gate_fingerprint(gate);
    query::record_owned_dyn_ir_shape_prototype_fact(gate, source.facts[1]);
    refresh(gate);
    EXPECT_EQ(gate.summary.fact_count, 2U);
    EXPECT_EQ(gate.summary.erased_payload_pointer_count, 1U);
    EXPECT_TRUE(query::is_valid(gate));
    EXPECT_FALSE(query::is_valid_m20b_owned_dyn_ir_shape_prototype_gate(gate));
    EXPECT_NE(gate.fingerprint, one_fact);
}

TEST(QueryUnit, OwnedDynIrShapePrototypeGateValidationRejectsShapeDrift)
{
    const query::OwnedDynIrShapePrototypeGate gate = baseline();
    ASSERT_TRUE(query::is_valid_m20b_owned_dyn_ir_shape_prototype_gate(gate));

    const auto expect_invalid_first_fact = [&gate](auto&& mutate) {
        query::OwnedDynIrShapePrototypeGate drift = gate;
        mutate(drift.facts.front());
        refresh(drift);
        EXPECT_FALSE(query::is_valid(drift));
    };
    expect_invalid_first_fact([](query::OwnedDynIrShapePrototypeFact& fact) {
        fact.fact_name.clear();
    });
    expect_invalid_first_fact([](query::OwnedDynIrShapePrototypeFact& fact) {
        fact.subject_symbol.clear();
    });
    expect_invalid_first_fact([](query::OwnedDynIrShapePrototypeFact& fact) {
        fact.m20a_gate_fact.clear();
    });
    expect_invalid_first_fact([](query::OwnedDynIrShapePrototypeFact& fact) {
        fact.verifier_guard_fact.clear();
    });
    expect_invalid_first_fact([](query::OwnedDynIrShapePrototypeFact& fact) {
        fact.blocked_surface_fact.clear();
    });
    expect_invalid_first_fact([](query::OwnedDynIrShapePrototypeFact& fact) {
        fact.kind =
            static_cast<query::OwnedDynIrShapePrototypeFactKind>(QUERY_TEST_INVALID_ENUM_VALUE);
    });
    expect_invalid_first_fact([](query::OwnedDynIrShapePrototypeFact& fact) {
        fact.stage = query::OwnedDynIrShapePrototypeStage::blocked_future_runtime;
    });
    expect_invalid_first_fact([](query::OwnedDynIrShapePrototypeFact& fact) {
        fact.policy = query::OwnedDynIrShapePrototypePolicy::runtime_lowering_not_implemented_v1;
    });
    expect_invalid_first_fact([](query::OwnedDynIrShapePrototypeFact& fact) {
        fact.references_m20a_admission_gate = false;
    });
    expect_invalid_first_fact([](query::OwnedDynIrShapePrototypeFact& fact) {
        fact.compiler_owned_ir_shape = false;
    });
    expect_invalid_first_fact([](query::OwnedDynIrShapePrototypeFact& fact) {
        fact.owned_layout_prototype_visible = false;
    });
    expect_invalid_first_fact([](query::OwnedDynIrShapePrototypeFact& fact) {
        fact.handle_metadata_visible = false;
    });
    expect_invalid_first_fact([](query::OwnedDynIrShapePrototypeFact& fact) {
        fact.erased_payload_pointer_visible = true;
    });
    expect_invalid_first_fact([](query::OwnedDynIrShapePrototypeFact& fact) {
        fact.borrowed_dyn_abi_unchanged = false;
    });
    expect_invalid_first_fact([](query::OwnedDynIrShapePrototypeFact& fact) {
        fact.standard_library_api_blocked = false;
    });
    expect_invalid_first_fact([](query::OwnedDynIrShapePrototypeFact& fact) {
        fact.box_dyn_surface_blocked = false;
    });
    expect_invalid_first_fact([](query::OwnedDynIrShapePrototypeFact& fact) {
        fact.owning_dyn_user_value_blocked = false;
    });
    expect_invalid_first_fact([](query::OwnedDynIrShapePrototypeFact& fact) {
        fact.allocator_api_blocked = false;
    });
    expect_invalid_first_fact([](query::OwnedDynIrShapePrototypeFact& fact) {
        fact.runtime_lowering_blocked = false;
    });
    expect_invalid_first_fact([](query::OwnedDynIrShapePrototypeFact& fact) {
        fact.dynamic_drop_runtime_blocked = false;
    });
    expect_invalid_first_fact([](query::OwnedDynIrShapePrototypeFact& fact) {
        fact.backend_helper_blocked = false;
    });
    expect_invalid_first_fact([](query::OwnedDynIrShapePrototypeFact& fact) {
        fact.executable_runtime_implemented = true;
    });
    expect_invalid_first_fact([](query::OwnedDynIrShapePrototypeFact& fact) {
        fact.layout_prototype_count = 0U;
    });
    expect_invalid_first_fact([](query::OwnedDynIrShapePrototypeFact& fact) {
        fact.handle_field_count = QUERY_TEST_WRONG_HANDLE_FIELD_COUNT;
    });

    const auto expect_invalid_fact_at =
        [&gate](const base::usize index, auto&& mutate) {
            query::OwnedDynIrShapePrototypeGate drift = gate;
            mutate(drift.facts[index]);
            refresh(drift);
            EXPECT_FALSE(query::is_valid(drift));
        };
    expect_invalid_fact_at(1U, [](query::OwnedDynIrShapePrototypeFact& fact) {
        fact.erased_payload_pointer_visible = false;
    });
    expect_invalid_fact_at(2U, [](query::OwnedDynIrShapePrototypeFact& fact) {
        fact.vtable_pointer_visible = false;
    });
    expect_invalid_fact_at(3U, [](query::OwnedDynIrShapePrototypeFact& fact) {
        fact.drop_identity_placeholder_visible = false;
    });
    expect_invalid_fact_at(4U, [](query::OwnedDynIrShapePrototypeFact& fact) {
        fact.allocator_identity_placeholder_visible = false;
    });
    expect_invalid_fact_at(5U, [](query::OwnedDynIrShapePrototypeFact& fact) {
        fact.vtable_pointer_visible = true;
    });
    expect_invalid_fact_at(1U, [](query::OwnedDynIrShapePrototypeFact& fact) {
        fact.layout_prototype_count = 2U;
    });

    query::OwnedDynIrShapePrototypeGate empty_subject = gate;
    empty_subject.subject.clear();
    refresh(empty_subject);
    EXPECT_FALSE(query::is_valid(empty_subject));

    query::OwnedDynIrShapePrototypeGate stale_admission = gate;
    stale_admission.admission_gate_fingerprint = query::stable_fingerprint("stale M20a");
    refresh(stale_admission);
    EXPECT_FALSE(query::is_valid(stale_admission));

    query::OwnedDynIrShapePrototypeGate stale_summary = gate;
    ++stale_summary.summary.fact_count;
    EXPECT_FALSE(query::is_valid(stale_summary));

    query::OwnedDynIrShapePrototypeGate stale_fingerprint = gate;
    stale_fingerprint.fingerprint = query::stable_fingerprint("stale M20b");
    EXPECT_FALSE(query::is_valid(stale_fingerprint));

    query::OwnedDynIrShapePrototypeGate duplicate_kind = gate;
    duplicate_kind.facts[1] = duplicate_kind.facts.front();
    refresh(duplicate_kind);
    EXPECT_TRUE(query::is_valid(duplicate_kind));
    EXPECT_FALSE(query::is_valid_m20b_owned_dyn_ir_shape_prototype_gate(duplicate_kind));
}

TEST(QueryUnit, OwnedDynIrShapePrototypeGateSummaryDumpAndFingerprintAreStable)
{
    const query::OwnedDynIrShapePrototypeGate gate = baseline();
    ASSERT_TRUE(query::is_valid_m20b_owned_dyn_ir_shape_prototype_gate(gate));

    const std::string summary = query::summarize_owned_dyn_ir_shape_prototype_gate(gate);
    EXPECT_TRUE(contains_text(summary, "owned_dyn_ir_shape_prototype_gate")) << summary;
    EXPECT_TRUE(contains_text(summary, "facts=6")) << summary;
    EXPECT_TRUE(contains_text(summary, "owned_handle=1")) << summary;
    EXPECT_TRUE(contains_text(summary, "erased_payload_pointer=1")) << summary;
    EXPECT_TRUE(contains_text(summary, "vtable_pointer=1")) << summary;
    EXPECT_TRUE(contains_text(summary, "runtime_lowering_blocker=1")) << summary;
    EXPECT_TRUE(contains_text(summary, "standard_library_api_blocked=6")) << summary;
    EXPECT_TRUE(contains_text(summary, "dynamic_drop_runtime_blocked=6")) << summary;
    EXPECT_TRUE(contains_text(summary, "backend_helper_blocked=6")) << summary;
    EXPECT_TRUE(contains_text(summary, "executable_runtime_implemented=0")) << summary;

    const std::string dump = query::dump_owned_dyn_ir_shape_prototype_gate(gate);
    EXPECT_TRUE(contains_text(dump, "owned_dyn_handle_metadata_ir_shape_fact")) << dump;
    EXPECT_TRUE(contains_text(dump, "owned_dyn_erased_payload_pointer_ir_shape_fact")) << dump;
    EXPECT_TRUE(contains_text(dump, "owned_dyn_vtable_pointer_ir_shape_fact")) << dump;
    EXPECT_TRUE(contains_text(dump, "owned_dyn_drop_identity_placeholder_ir_shape_fact")) << dump;
    EXPECT_TRUE(contains_text(dump, "owned_dyn_allocator_identity_placeholder_ir_shape_fact")) << dump;
    EXPECT_TRUE(contains_text(dump, "owned_dyn_runtime_lowering_blocker_ir_shape_fact")) << dump;
    EXPECT_TRUE(contains_text(dump, "requires_m20a_owned_dyn_runtime_admission_gate")) << dump;
    EXPECT_TRUE(contains_text(dump, "box_dyn_trait_not_in_m20b")) << dump;
    EXPECT_TRUE(contains_text(dump, "owning_dyn_user_value_not_in_m20b")) << dump;
    EXPECT_TRUE(contains_text(dump, "allocator_api_not_in_m20b")) << dump;
    EXPECT_TRUE(contains_text(dump, "runtime_abi_lowering_not_in_m20b")) << dump;
    EXPECT_TRUE(contains_text(dump, "dynamic_drop_runtime_not_in_m20b")) << dump;
    EXPECT_TRUE(contains_text(dump, "backend_runtime_helper_not_in_m20b")) << dump;

    query::OwnedDynIrShapePrototypeGate changed = gate;
    changed.facts.front().verifier_guard_fact = "verifier_requires_owned_dyn_handle_v2";
    refresh(changed);
    EXPECT_TRUE(query::is_valid(changed));
    EXPECT_NE(changed.fingerprint, gate.fingerprint);
}

} // namespace aurex::test
