#include <gtest/frontend/macro/early_item_expansion/early_item_expansion_builtin_derive_support.hpp>
#include <gtest/frontend/macro/early_item_expansion/early_item_expansion_contract_support.hpp>

#include <gtest/gtest.h>

namespace aurex::test {

using namespace early_item_expansion_support;

TEST(CoreUnit, EarlyItemExpansionValidationRejectsBuiltinDeriveM22Drift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "#[derive(Copy, Eq, Hash)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.builtin_derive_expansion_admissions.size(), 2U);
    ASSERT_EQ(baseline.builtin_derive_semantic_plans.size(), 2U);
    ASSERT_EQ(baseline.builtin_derive_parser_release_gates.size(), 1U);

    frontend::macro::EarlyItemExpansionResult missing_admissions = baseline;
    missing_admissions.builtin_derive_expansion_admissions.clear();
    refresh_expansion_result(missing_admissions);
    EXPECT_EQ(missing_admissions.summary.builtin_derive_expansion_admission_gate_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_admissions));

    frontend::macro::EarlyItemExpansionResult wrong_admission_identity = baseline;
    wrong_admission_identity.builtin_derive_expansion_admissions.back().admission_identity =
        query::stable_fingerprint("wrong builtin derive admission identity");
    refresh_expansion_result(wrong_admission_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_admission_identity));

    frontend::macro::EarlyItemExpansionResult wrong_admission_kind = baseline;
    wrong_admission_kind.builtin_derive_expansion_admissions.back().admission_kind =
        "non_derive_attribute_expansion_blocked";
    refresh_expansion_result(wrong_admission_kind);
    EXPECT_EQ(wrong_admission_kind.summary.builtin_derive_expansion_derive_admission_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_admission_kind));

    frontend::macro::EarlyItemExpansionResult wrong_admission_query = baseline;
    wrong_admission_query.builtin_derive_expansion_admissions.back().query_name =
        "m22a-builtin-derive-admission:wrong";
    refresh_expansion_result(wrong_admission_query);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_admission_query));

    frontend::macro::EarlyItemExpansionResult wrong_candidate_count = baseline;
    wrong_candidate_count.builtin_derive_expansion_admissions.back().capability_candidate_count = 1U;
    refresh_expansion_result(wrong_candidate_count);
    EXPECT_NE(wrong_candidate_count.summary.builtin_derive_expansion_capability_candidate_count,
        baseline.summary.builtin_derive_expansion_capability_candidate_count);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_candidate_count));

    frontend::macro::EarlyItemExpansionResult admission_parser_enabled = baseline;
    admission_parser_enabled.builtin_derive_expansion_admissions.back().parser_consumption_enabled = true;
    refresh_expansion_result(admission_parser_enabled);
    EXPECT_EQ(admission_parser_enabled.summary.parse_ready_token_buffer_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(admission_parser_enabled));

    frontend::macro::EarlyItemExpansionResult admission_standard_library = baseline;
    admission_standard_library.builtin_derive_expansion_admissions.back().standard_library_required = true;
    refresh_expansion_result(admission_standard_library);
    EXPECT_EQ(admission_standard_library.summary.standard_library_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(admission_standard_library));

    frontend::macro::EarlyItemExpansionResult missing_plans = baseline;
    missing_plans.builtin_derive_semantic_plans.clear();
    refresh_expansion_result(missing_plans);
    EXPECT_EQ(missing_plans.summary.builtin_derive_semantic_plan_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_plans));

    frontend::macro::EarlyItemExpansionResult wrong_semantic_identity = baseline;
    wrong_semantic_identity.builtin_derive_semantic_plans.back().semantic_plan_identity =
        query::stable_fingerprint("wrong builtin derive semantic plan");
    refresh_expansion_result(wrong_semantic_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_semantic_identity));

    frontend::macro::EarlyItemExpansionResult wrong_target_kind = baseline;
    wrong_target_kind.builtin_derive_semantic_plans.back().target_kind = "enum";
    refresh_expansion_result(wrong_target_kind);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_target_kind));

    frontend::macro::EarlyItemExpansionResult wrong_capability_total = baseline;
    wrong_capability_total.builtin_derive_semantic_plans.back().capability_count = 2U;
    refresh_expansion_result(wrong_capability_total);
    EXPECT_EQ(wrong_capability_total.summary.builtin_derive_semantic_capability_count, 2U);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_capability_total));

    frontend::macro::EarlyItemExpansionResult semantic_requires_generated_items = baseline;
    semantic_requires_generated_items.builtin_derive_semantic_plans.back().requires_generated_items = true;
    refresh_expansion_result(semantic_requires_generated_items);
    EXPECT_FALSE(frontend::macro::is_valid(semantic_requires_generated_items));

    frontend::macro::EarlyItemExpansionResult semantic_runtime = baseline;
    semantic_runtime.builtin_derive_semantic_plans.back().requires_runtime = true;
    refresh_expansion_result(semantic_runtime);
    EXPECT_EQ(semantic_runtime.summary.runtime_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(semantic_runtime));

    frontend::macro::EarlyItemExpansionResult missing_release = baseline;
    missing_release.builtin_derive_parser_release_gates.clear();
    refresh_expansion_result(missing_release);
    EXPECT_EQ(missing_release.summary.builtin_derive_parser_release_gate_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_release));

    frontend::macro::EarlyItemExpansionResult wrong_release_identity = baseline;
    wrong_release_identity.builtin_derive_parser_release_gates.front().release_gate_identity =
        query::stable_fingerprint("wrong builtin derive parser release gate");
    refresh_expansion_result(wrong_release_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_release_identity));

    frontend::macro::EarlyItemExpansionResult wrong_release_counts = baseline;
    wrong_release_counts.builtin_derive_parser_release_gates.front().derive_admission_count = 0U;
    refresh_expansion_result(wrong_release_counts);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_release_counts));

    frontend::macro::EarlyItemExpansionResult release_parser_enabled = baseline;
    release_parser_enabled.builtin_derive_parser_release_gates.front().parser_consumption_enabled = true;
    refresh_expansion_result(release_parser_enabled);
    EXPECT_EQ(release_parser_enabled.summary.builtin_derive_parser_release_parser_consumable_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(release_parser_enabled));

    frontend::macro::EarlyItemExpansionResult release_debug_trace = baseline;
    release_debug_trace.builtin_derive_parser_release_gates.front().debug_trace_available = true;
    refresh_expansion_result(release_debug_trace);
    EXPECT_EQ(release_debug_trace.summary.parser_admission_debug_trace_projection_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(release_debug_trace));

    frontend::macro::EarlyItemExpansionResult release_external_process = baseline;
    release_external_process.builtin_derive_parser_release_gates.front().external_process_required = true;
    refresh_expansion_result(release_external_process);
    EXPECT_EQ(release_external_process.summary.external_process_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(release_external_process));
}

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksBuiltinDeriveM22Facts)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy, Eq, Hash)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.builtin_derive_expansion_admissions.empty());
    ASSERT_FALSE(baseline.builtin_derive_semantic_plans.empty());
    ASSERT_FALSE(baseline.builtin_derive_parser_release_gates.empty());

    frontend::macro::EarlyItemExpansionResult admission_identity = baseline;
    admission_identity.builtin_derive_expansion_admissions.front().admission_identity =
        query::stable_fingerprint("different builtin derive admission identity");
    refresh_expansion_result(admission_identity);
    EXPECT_NE(admission_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(admission_identity));

    frontend::macro::EarlyItemExpansionResult admission_query = baseline;
    admission_query.builtin_derive_expansion_admissions.front().query_name =
        "m22a-builtin-derive-admission:different";
    refresh_expansion_result(admission_query);
    EXPECT_NE(admission_query.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(admission_query));

    frontend::macro::EarlyItemExpansionResult semantic_identity = baseline;
    semantic_identity.builtin_derive_semantic_plans.front().semantic_plan_identity =
        query::stable_fingerprint("different builtin derive semantic plan identity");
    refresh_expansion_result(semantic_identity);
    EXPECT_NE(semantic_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(semantic_identity));

    frontend::macro::EarlyItemExpansionResult semantic_capabilities = baseline;
    semantic_capabilities.builtin_derive_semantic_plans.front().hash_capability_count = 0U;
    refresh_expansion_result(semantic_capabilities);
    EXPECT_NE(semantic_capabilities.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(semantic_capabilities));

    frontend::macro::EarlyItemExpansionResult release_identity = baseline;
    release_identity.builtin_derive_parser_release_gates.front().release_gate_identity =
        query::stable_fingerprint("different builtin derive parser release identity");
    refresh_expansion_result(release_identity);
    EXPECT_NE(release_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(release_identity));
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsBuiltinDeriveM22ReleaseHardeningDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "#[derive(Copy, Eq, Hash)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.builtin_derive_release_hardening_matrices.size(), 1U);
    ASSERT_EQ(baseline.builtin_derive_debug_dump_contracts.size(), 1U);
    ASSERT_EQ(baseline.builtin_derive_rollback_diagnostic_gates.size(), 1U);

    frontend::macro::EarlyItemExpansionResult missing_matrix = baseline;
    missing_matrix.builtin_derive_release_hardening_matrices.clear();
    refresh_expansion_result(missing_matrix);
    EXPECT_EQ(missing_matrix.summary.builtin_derive_release_hardening_matrix_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_matrix));

    frontend::macro::EarlyItemExpansionResult wrong_matrix_identity = baseline;
    wrong_matrix_identity.builtin_derive_release_hardening_matrices.front().hardening_matrix_identity =
        query::stable_fingerprint("wrong builtin derive release hardening matrix");
    refresh_expansion_result(wrong_matrix_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_matrix_identity));

    frontend::macro::EarlyItemExpansionResult wrong_matrix_query = baseline;
    wrong_matrix_query.builtin_derive_release_hardening_matrices.front().hardening_query_name =
        "m22d-builtin-derive-release-hardening:wrong";
    refresh_expansion_result(wrong_matrix_query);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_matrix_query));

    frontend::macro::EarlyItemExpansionResult wrong_cross_part_count = baseline;
    wrong_cross_part_count.builtin_derive_release_hardening_matrices.front().cross_part_admission_count = 1U;
    refresh_expansion_result(wrong_cross_part_count);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_cross_part_count));

    frontend::macro::EarlyItemExpansionResult matrix_not_complete = baseline;
    matrix_not_complete.builtin_derive_release_hardening_matrices.front().negative_matrix_complete = false;
    refresh_expansion_result(matrix_not_complete);
    EXPECT_EQ(matrix_not_complete.summary.builtin_derive_release_hardening_negative_matrix_complete_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(matrix_not_complete));

    frontend::macro::EarlyItemExpansionResult matrix_parser_enabled = baseline;
    matrix_parser_enabled.builtin_derive_release_hardening_matrices.front().parser_consumption_enabled = true;
    refresh_expansion_result(matrix_parser_enabled);
    EXPECT_EQ(matrix_parser_enabled.summary.builtin_derive_release_hardening_parser_consumable_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(matrix_parser_enabled));

    frontend::macro::EarlyItemExpansionResult matrix_standard_library = baseline;
    matrix_standard_library.builtin_derive_release_hardening_matrices.front().standard_library_required = true;
    refresh_expansion_result(matrix_standard_library);
    EXPECT_EQ(matrix_standard_library.summary.standard_library_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(matrix_standard_library));

    frontend::macro::EarlyItemExpansionResult missing_debug_contract = baseline;
    missing_debug_contract.builtin_derive_debug_dump_contracts.clear();
    refresh_expansion_result(missing_debug_contract);
    EXPECT_EQ(missing_debug_contract.summary.builtin_derive_debug_dump_contract_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_debug_contract));

    frontend::macro::EarlyItemExpansionResult wrong_debug_identity = baseline;
    wrong_debug_identity.builtin_derive_debug_dump_contracts.front().debug_dump_contract_identity =
        query::stable_fingerprint("wrong builtin derive debug dump contract");
    refresh_expansion_result(wrong_debug_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_debug_identity));

    frontend::macro::EarlyItemExpansionResult wrong_debug_query = baseline;
    wrong_debug_query.builtin_derive_debug_dump_contracts.front().debug_dump_query_name =
        "m22e-builtin-derive-debug-dump:wrong";
    refresh_expansion_result(wrong_debug_query);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_debug_query));

    frontend::macro::EarlyItemExpansionResult debug_unstable_order = baseline;
    debug_unstable_order.builtin_derive_debug_dump_contracts.front().stable_ordering_available = false;
    refresh_expansion_result(debug_unstable_order);
    EXPECT_FALSE(frontend::macro::is_valid(debug_unstable_order));

    frontend::macro::EarlyItemExpansionResult debug_incomplete = baseline;
    debug_incomplete.builtin_derive_debug_dump_contracts.front().debug_dump_contract_complete = false;
    refresh_expansion_result(debug_incomplete);
    EXPECT_EQ(debug_incomplete.summary.builtin_derive_debug_dump_complete_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(debug_incomplete));

    frontend::macro::EarlyItemExpansionResult debug_parser_enabled = baseline;
    debug_parser_enabled.builtin_derive_debug_dump_contracts.front().parser_consumption_enabled = true;
    refresh_expansion_result(debug_parser_enabled);
    EXPECT_EQ(debug_parser_enabled.summary.builtin_derive_debug_dump_parser_consumable_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(debug_parser_enabled));

    frontend::macro::EarlyItemExpansionResult debug_runtime = baseline;
    debug_runtime.builtin_derive_debug_dump_contracts.front().runtime_required = true;
    refresh_expansion_result(debug_runtime);
    EXPECT_EQ(debug_runtime.summary.runtime_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(debug_runtime));

    frontend::macro::EarlyItemExpansionResult missing_rollback = baseline;
    missing_rollback.builtin_derive_rollback_diagnostic_gates.clear();
    refresh_expansion_result(missing_rollback);
    EXPECT_EQ(missing_rollback.summary.builtin_derive_rollback_diagnostic_gate_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_rollback));

    frontend::macro::EarlyItemExpansionResult wrong_rollback_identity = baseline;
    wrong_rollback_identity.builtin_derive_rollback_diagnostic_gates.front().rollback_gate_identity =
        query::stable_fingerprint("wrong builtin derive rollback diagnostic gate");
    refresh_expansion_result(wrong_rollback_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_rollback_identity));

    frontend::macro::EarlyItemExpansionResult wrong_rollback_query = baseline;
    wrong_rollback_query.builtin_derive_rollback_diagnostic_gates.front().rollback_query_name =
        "m22f-builtin-derive-rollback-diagnostic:wrong";
    refresh_expansion_result(wrong_rollback_query);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_rollback_query));

    frontend::macro::EarlyItemExpansionResult wrong_diagnostic_count = baseline;
    wrong_diagnostic_count.builtin_derive_rollback_diagnostic_gates.front().derive_diagnostic_count = 0U;
    refresh_expansion_result(wrong_diagnostic_count);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_diagnostic_count));

    frontend::macro::EarlyItemExpansionResult rollback_prerequisite_missing = baseline;
    rollback_prerequisite_missing.builtin_derive_rollback_diagnostic_gates.front()
        .release_rollback_plan_complete = false;
    refresh_expansion_result(rollback_prerequisite_missing);
    EXPECT_EQ(rollback_prerequisite_missing.summary.builtin_derive_rollback_diagnostic_design_complete_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(rollback_prerequisite_missing));

    frontend::macro::EarlyItemExpansionResult rollback_execution_enabled = baseline;
    rollback_execution_enabled.builtin_derive_rollback_diagnostic_gates.front().rollback_execution_enabled = true;
    refresh_expansion_result(rollback_execution_enabled);
    EXPECT_EQ(rollback_execution_enabled.summary.builtin_derive_rollback_diagnostic_parser_consumable_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(rollback_execution_enabled));

    frontend::macro::EarlyItemExpansionResult rollback_parser_enabled = baseline;
    rollback_parser_enabled.builtin_derive_rollback_diagnostic_gates.front().parser_consumption_enabled = true;
    refresh_expansion_result(rollback_parser_enabled);
    EXPECT_EQ(rollback_parser_enabled.summary.builtin_derive_rollback_diagnostic_parser_consumable_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(rollback_parser_enabled));

    frontend::macro::EarlyItemExpansionResult rollback_external = baseline;
    rollback_external.builtin_derive_rollback_diagnostic_gates.front().external_process_required = true;
    refresh_expansion_result(rollback_external);
    EXPECT_EQ(rollback_external.summary.external_process_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(rollback_external));

    frontend::macro::EarlyItemExpansionResult rollback_user_code = baseline;
    rollback_user_code.builtin_derive_rollback_diagnostic_gates.front().produced_user_generated_code = true;
    refresh_expansion_result(rollback_user_code);
    EXPECT_EQ(rollback_user_code.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(rollback_user_code));
}

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksBuiltinDeriveM22ReleaseHardeningFacts)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy, Eq, Hash)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.builtin_derive_release_hardening_matrices.empty());
    ASSERT_FALSE(baseline.builtin_derive_debug_dump_contracts.empty());
    ASSERT_FALSE(baseline.builtin_derive_rollback_diagnostic_gates.empty());

    frontend::macro::EarlyItemExpansionResult matrix_identity = baseline;
    matrix_identity.builtin_derive_release_hardening_matrices.front().hardening_matrix_identity =
        query::stable_fingerprint("different builtin derive release hardening matrix identity");
    refresh_expansion_result(matrix_identity);
    EXPECT_NE(matrix_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(matrix_identity));

    frontend::macro::EarlyItemExpansionResult matrix_counts = baseline;
    matrix_counts.builtin_derive_release_hardening_matrices.front().part_local_derive_admission_count = 0U;
    refresh_expansion_result(matrix_counts);
    EXPECT_NE(matrix_counts.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(matrix_counts));

    frontend::macro::EarlyItemExpansionResult debug_identity = baseline;
    debug_identity.builtin_derive_debug_dump_contracts.front().debug_dump_contract_identity =
        query::stable_fingerprint("different builtin derive debug dump identity");
    refresh_expansion_result(debug_identity);
    EXPECT_NE(debug_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(debug_identity));

    frontend::macro::EarlyItemExpansionResult debug_complete = baseline;
    debug_complete.builtin_derive_debug_dump_contracts.front().debug_dump_contract_complete = false;
    refresh_expansion_result(debug_complete);
    EXPECT_NE(debug_complete.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(debug_complete));

    frontend::macro::EarlyItemExpansionResult rollback_identity = baseline;
    rollback_identity.builtin_derive_rollback_diagnostic_gates.front().rollback_gate_identity =
        query::stable_fingerprint("different builtin derive rollback diagnostic identity");
    refresh_expansion_result(rollback_identity);
    EXPECT_NE(rollback_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(rollback_identity));

    frontend::macro::EarlyItemExpansionResult rollback_design = baseline;
    rollback_design.builtin_derive_rollback_diagnostic_gates.front()
        .diagnostic_grouping_available = false;
    refresh_expansion_result(rollback_design);
    EXPECT_NE(rollback_design.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(rollback_design));
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsBuiltinDeriveM23ParserConsumptionAdmissionDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "#[derive(Copy, Eq, Hash)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.builtin_derive_parser_consumption_admission_protocols.size(), 1U);
    ASSERT_EQ(baseline.builtin_derive_checkpoint_rollback_protocols.size(), 1U);
    ASSERT_EQ(baseline.builtin_derive_preconsumption_verification_closures.size(), 1U);

    frontend::macro::EarlyItemExpansionResult missing_admission = baseline;
    missing_admission.builtin_derive_parser_consumption_admission_protocols.clear();
    refresh_expansion_result(missing_admission);
    EXPECT_EQ(missing_admission.summary.builtin_derive_parser_consumption_admission_protocol_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_admission));

    frontend::macro::EarlyItemExpansionResult wrong_admission_identity = baseline;
    wrong_admission_identity.builtin_derive_parser_consumption_admission_protocols.front()
        .admission_protocol_identity = query::stable_fingerprint("wrong m23a admission protocol identity");
    refresh_expansion_result(wrong_admission_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_admission_identity));

    frontend::macro::EarlyItemExpansionResult wrong_admission_query = baseline;
    wrong_admission_query.builtin_derive_parser_consumption_admission_protocols.front()
        .admission_query_name = "m23a-builtin-derive-parser-consumption-admission:wrong";
    refresh_expansion_result(wrong_admission_query);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_admission_query));

    frontend::macro::EarlyItemExpansionResult admission_parser_admitted = baseline;
    admission_parser_admitted.builtin_derive_parser_consumption_admission_protocols.front()
        .parser_admitted = true;
    refresh_expansion_result(admission_parser_admitted);
    EXPECT_EQ(admission_parser_admitted.summary.builtin_derive_parser_consumption_admission_parser_consumable_count,
        1U);
    EXPECT_FALSE(frontend::macro::is_valid(admission_parser_admitted));

    frontend::macro::EarlyItemExpansionResult admission_parser_enabled = baseline;
    admission_parser_enabled.builtin_derive_parser_consumption_admission_protocols.front()
        .parser_consumption_enabled = true;
    refresh_expansion_result(admission_parser_enabled);
    EXPECT_EQ(admission_parser_enabled.summary.builtin_derive_parser_consumption_admission_parser_consumable_count,
        1U);
    EXPECT_FALSE(frontend::macro::is_valid(admission_parser_enabled));

    frontend::macro::EarlyItemExpansionResult admission_standard_library = baseline;
    admission_standard_library.builtin_derive_parser_consumption_admission_protocols.front()
        .standard_library_required = true;
    refresh_expansion_result(admission_standard_library);
    EXPECT_EQ(admission_standard_library.summary.standard_library_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(admission_standard_library));

    frontend::macro::EarlyItemExpansionResult admission_runtime = baseline;
    admission_runtime.builtin_derive_parser_consumption_admission_protocols.front().runtime_required = true;
    refresh_expansion_result(admission_runtime);
    EXPECT_EQ(admission_runtime.summary.runtime_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(admission_runtime));

    frontend::macro::EarlyItemExpansionResult admission_external = baseline;
    admission_external.builtin_derive_parser_consumption_admission_protocols.front()
        .external_process_required = true;
    refresh_expansion_result(admission_external);
    EXPECT_EQ(admission_external.summary.external_process_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(admission_external));

    frontend::macro::EarlyItemExpansionResult admission_user_code = baseline;
    admission_user_code.builtin_derive_parser_consumption_admission_protocols.front()
        .produced_user_generated_code = true;
    refresh_expansion_result(admission_user_code);
    EXPECT_EQ(admission_user_code.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(admission_user_code));

    frontend::macro::EarlyItemExpansionResult missing_checkpoint = baseline;
    missing_checkpoint.builtin_derive_checkpoint_rollback_protocols.clear();
    refresh_expansion_result(missing_checkpoint);
    EXPECT_EQ(missing_checkpoint.summary.builtin_derive_checkpoint_rollback_protocol_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_checkpoint));

    frontend::macro::EarlyItemExpansionResult wrong_checkpoint_identity = baseline;
    wrong_checkpoint_identity.builtin_derive_checkpoint_rollback_protocols.front()
        .checkpoint_protocol_identity = query::stable_fingerprint("wrong m23b checkpoint protocol identity");
    refresh_expansion_result(wrong_checkpoint_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_checkpoint_identity));

    frontend::macro::EarlyItemExpansionResult wrong_checkpoint_query = baseline;
    wrong_checkpoint_query.builtin_derive_checkpoint_rollback_protocols.front()
        .checkpoint_query_name = "m23b-builtin-derive-checkpoint-rollback:wrong";
    refresh_expansion_result(wrong_checkpoint_query);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_checkpoint_query));

    frontend::macro::EarlyItemExpansionResult wrong_checkpoint_count = baseline;
    wrong_checkpoint_count.builtin_derive_checkpoint_rollback_protocols.front().checkpoint_count = 2U;
    refresh_expansion_result(wrong_checkpoint_count);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_checkpoint_count));

    frontend::macro::EarlyItemExpansionResult checkpoint_replay_missing = baseline;
    checkpoint_replay_missing.builtin_derive_checkpoint_rollback_protocols.front()
        .diagnostic_replay_available = false;
    refresh_expansion_result(checkpoint_replay_missing);
    EXPECT_FALSE(frontend::macro::is_valid(checkpoint_replay_missing));

    frontend::macro::EarlyItemExpansionResult checkpoint_rollback_enabled = baseline;
    checkpoint_rollback_enabled.builtin_derive_checkpoint_rollback_protocols.front()
        .rollback_execution_enabled = true;
    refresh_expansion_result(checkpoint_rollback_enabled);
    EXPECT_EQ(checkpoint_rollback_enabled.summary.builtin_derive_checkpoint_rollback_parser_consumable_count,
        1U);
    EXPECT_FALSE(frontend::macro::is_valid(checkpoint_rollback_enabled));

    frontend::macro::EarlyItemExpansionResult checkpoint_parser_enabled = baseline;
    checkpoint_parser_enabled.builtin_derive_checkpoint_rollback_protocols.front()
        .parser_consumption_enabled = true;
    refresh_expansion_result(checkpoint_parser_enabled);
    EXPECT_EQ(checkpoint_parser_enabled.summary.builtin_derive_checkpoint_rollback_parser_consumable_count,
        1U);
    EXPECT_FALSE(frontend::macro::is_valid(checkpoint_parser_enabled));

    frontend::macro::EarlyItemExpansionResult missing_closure = baseline;
    missing_closure.builtin_derive_preconsumption_verification_closures.clear();
    refresh_expansion_result(missing_closure);
    EXPECT_EQ(missing_closure.summary.builtin_derive_preconsumption_verification_closure_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_closure));

    frontend::macro::EarlyItemExpansionResult wrong_closure_identity = baseline;
    wrong_closure_identity.builtin_derive_preconsumption_verification_closures.front()
        .verification_closure_identity = query::stable_fingerprint("wrong m23c verification closure identity");
    refresh_expansion_result(wrong_closure_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_closure_identity));

    frontend::macro::EarlyItemExpansionResult wrong_closure_query = baseline;
    wrong_closure_query.builtin_derive_preconsumption_verification_closures.front()
        .verification_query_name = "m23c-builtin-derive-preconsumption-verification:wrong";
    refresh_expansion_result(wrong_closure_query);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_closure_query));

    frontend::macro::EarlyItemExpansionResult closure_incomplete = baseline;
    closure_incomplete.builtin_derive_preconsumption_verification_closures.front()
        .verification_closure_complete = false;
    refresh_expansion_result(closure_incomplete);
    EXPECT_EQ(closure_incomplete.summary.builtin_derive_preconsumption_verification_complete_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(closure_incomplete));

    frontend::macro::EarlyItemExpansionResult closure_parser_enabled = baseline;
    closure_parser_enabled.builtin_derive_preconsumption_verification_closures.front()
        .parser_consumption_enabled = true;
    refresh_expansion_result(closure_parser_enabled);
    EXPECT_EQ(closure_parser_enabled.summary.builtin_derive_preconsumption_verification_parser_consumable_count,
        1U);
    EXPECT_FALSE(frontend::macro::is_valid(closure_parser_enabled));

    frontend::macro::EarlyItemExpansionResult closure_sema_visible = baseline;
    closure_sema_visible.builtin_derive_preconsumption_verification_closures.front().sema_visible = true;
    refresh_expansion_result(closure_sema_visible);
    EXPECT_EQ(closure_sema_visible.summary.sema_visible_generated_part_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(closure_sema_visible));

    frontend::macro::EarlyItemExpansionResult closure_part_parsed = baseline;
    closure_part_parsed.builtin_derive_preconsumption_verification_closures.front().generated_part_parsed =
        true;
    refresh_expansion_result(closure_part_parsed);
    EXPECT_EQ(closure_part_parsed.summary.parsed_generated_part_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(closure_part_parsed));

    frontend::macro::EarlyItemExpansionResult closure_standard_library = baseline;
    closure_standard_library.builtin_derive_preconsumption_verification_closures.front()
        .standard_library_required = true;
    refresh_expansion_result(closure_standard_library);
    EXPECT_EQ(closure_standard_library.summary.standard_library_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(closure_standard_library));

    frontend::macro::EarlyItemExpansionResult closure_runtime = baseline;
    closure_runtime.builtin_derive_preconsumption_verification_closures.front().runtime_required = true;
    refresh_expansion_result(closure_runtime);
    EXPECT_EQ(closure_runtime.summary.runtime_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(closure_runtime));

    frontend::macro::EarlyItemExpansionResult closure_external = baseline;
    closure_external.builtin_derive_preconsumption_verification_closures.front()
        .external_process_required = true;
    refresh_expansion_result(closure_external);
    EXPECT_EQ(closure_external.summary.external_process_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(closure_external));

    frontend::macro::EarlyItemExpansionResult closure_user_code = baseline;
    closure_user_code.builtin_derive_preconsumption_verification_closures.front()
        .produced_user_generated_code = true;
    refresh_expansion_result(closure_user_code);
    EXPECT_EQ(closure_user_code.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(closure_user_code));
}

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksBuiltinDeriveM23ParserConsumptionAdmissionFacts)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy, Eq, Hash)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.builtin_derive_parser_consumption_admission_protocols.empty());
    ASSERT_FALSE(baseline.builtin_derive_checkpoint_rollback_protocols.empty());
    ASSERT_FALSE(baseline.builtin_derive_preconsumption_verification_closures.empty());

    frontend::macro::EarlyItemExpansionResult admission_identity = baseline;
    admission_identity.builtin_derive_parser_consumption_admission_protocols.front()
        .admission_protocol_identity =
        query::stable_fingerprint("different m23a admission protocol identity");
    refresh_expansion_result(admission_identity);
    EXPECT_NE(admission_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(admission_identity));

    frontend::macro::EarlyItemExpansionResult admission_counts = baseline;
    admission_counts.builtin_derive_parser_consumption_admission_protocols.front().derive_candidate_count =
        0U;
    refresh_expansion_result(admission_counts);
    EXPECT_NE(admission_counts.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(admission_counts));

    frontend::macro::EarlyItemExpansionResult checkpoint_identity = baseline;
    checkpoint_identity.builtin_derive_checkpoint_rollback_protocols.front()
        .checkpoint_protocol_identity =
        query::stable_fingerprint("different m23b checkpoint rollback identity");
    refresh_expansion_result(checkpoint_identity);
    EXPECT_NE(checkpoint_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(checkpoint_identity));

    frontend::macro::EarlyItemExpansionResult checkpoint_count = baseline;
    checkpoint_count.builtin_derive_checkpoint_rollback_protocols.front().rollback_plan_count = 2U;
    refresh_expansion_result(checkpoint_count);
    EXPECT_NE(checkpoint_count.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(checkpoint_count));

    frontend::macro::EarlyItemExpansionResult closure_identity = baseline;
    closure_identity.builtin_derive_preconsumption_verification_closures.front()
        .verification_closure_identity =
        query::stable_fingerprint("different m23c verification closure identity");
    refresh_expansion_result(closure_identity);
    EXPECT_NE(closure_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(closure_identity));

    frontend::macro::EarlyItemExpansionResult closure_query = baseline;
    closure_query.builtin_derive_preconsumption_verification_closures.front()
        .verification_query_name = "m23c-builtin-derive-preconsumption-verification:different";
    refresh_expansion_result(closure_query);
    EXPECT_NE(closure_query.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(closure_query));
}
} // namespace aurex::test
