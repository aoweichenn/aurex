#include <gtest/frontend/macro/early_item_expansion/early_item_expansion_contract_support.hpp>

#include <gtest/gtest.h>

namespace aurex::test {

using namespace early_item_expansion_support;

TEST(CoreUnit, EarlyItemExpansionValidationRejectsTokenMaterializationAdmissionDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.token_materialization_admissions.empty());
    ASSERT_FALSE(baseline.generated_token_buffers.empty());

    frontend::macro::EarlyItemExpansionResult missing_admission = baseline;
    missing_admission.token_materialization_admissions.clear();
    refresh_expansion_result(missing_admission);
    EXPECT_EQ(missing_admission.summary.token_materialization_admission_stub_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_admission));

    frontend::macro::EarlyItemExpansionResult missing_buffer = baseline;
    missing_buffer.generated_token_buffers.clear();
    refresh_expansion_result(missing_buffer);
    EXPECT_EQ(missing_buffer.summary.generated_token_buffer_stub_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_buffer));

    frontend::macro::EarlyItemExpansionResult empty_token_plan = baseline;
    empty_token_plan.token_materialization_admissions.front().token_plan_identity = {};
    refresh_expansion_result(empty_token_plan);
    EXPECT_FALSE(frontend::macro::is_valid(empty_token_plan));

    frontend::macro::EarlyItemExpansionResult empty_token_buffer_identity = baseline;
    empty_token_buffer_identity.token_materialization_admissions.front().token_buffer_identity = {};
    refresh_expansion_result(empty_token_buffer_identity);
    EXPECT_FALSE(frontend::macro::is_valid(empty_token_buffer_identity));

    frontend::macro::EarlyItemExpansionResult wrong_declaration_identity = baseline;
    wrong_declaration_identity.token_materialization_admissions.front().declaration_identity =
        query::stable_fingerprint("wrong admission declaration identity");
    refresh_expansion_result(wrong_declaration_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_declaration_identity));

    frontend::macro::EarlyItemExpansionResult wrong_generated_item_key = baseline;
    wrong_generated_item_key.token_materialization_admissions.front().generated_item_key =
        query::stable_fingerprint("wrong admission generated item key");
    refresh_expansion_result(wrong_generated_item_key);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_generated_item_key));

    frontend::macro::EarlyItemExpansionResult wrong_declared_name_identity = baseline;
    wrong_declared_name_identity.token_materialization_admissions.front().declared_name_identity =
        query::stable_fingerprint("wrong admission declared name identity");
    refresh_expansion_result(wrong_declared_name_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_declared_name_identity));

    frontend::macro::EarlyItemExpansionResult wrong_hygiene_mark = baseline;
    wrong_hygiene_mark.token_materialization_admissions.front().hygiene_mark =
        query::stable_fingerprint("wrong admission hygiene mark");
    refresh_expansion_result(wrong_hygiene_mark);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_hygiene_mark));

    frontend::macro::EarlyItemExpansionResult wrong_source_map = baseline;
    wrong_source_map.token_materialization_admissions.front().source_map_identity =
        query::stable_fingerprint("wrong admission source map");
    refresh_expansion_result(wrong_source_map);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_source_map));

    frontend::macro::EarlyItemExpansionResult wrong_trace_identity = baseline;
    wrong_trace_identity.token_materialization_admissions.front().trace_identity =
        query::stable_fingerprint("wrong admission trace identity");
    refresh_expansion_result(wrong_trace_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_trace_identity));

    frontend::macro::EarlyItemExpansionResult wrong_policy = baseline;
    wrong_policy.token_materialization_admissions.front().admission_policy = "wrong_admission_policy";
    refresh_expansion_result(wrong_policy);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_policy));

    frontend::macro::EarlyItemExpansionResult wrong_stream_name = baseline;
    wrong_stream_name.token_materialization_admissions.front().token_stream_name =
        "m21h-token-stream:wrong";
    refresh_expansion_result(wrong_stream_name);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_stream_name));

    frontend::macro::EarlyItemExpansionResult wrong_blocker = baseline;
    wrong_blocker.token_materialization_admissions.front().blocker_reason = "wrong blocker";
    refresh_expansion_result(wrong_blocker);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_blocker));

    frontend::macro::EarlyItemExpansionResult not_compiler_owned = baseline;
    not_compiler_owned.token_materialization_admissions.front().compiler_owned = false;
    refresh_expansion_result(not_compiler_owned);
    EXPECT_EQ(not_compiler_owned.summary.compiler_owned_admission_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(not_compiler_owned));

    frontend::macro::EarlyItemExpansionResult not_admitted = baseline;
    not_admitted.token_materialization_admissions.front().admitted = false;
    refresh_expansion_result(not_admitted);
    EXPECT_EQ(not_admitted.summary.admitted_token_materialization_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(not_admitted));

    frontend::macro::EarlyItemExpansionResult materialized_admission = baseline;
    materialized_admission.token_materialization_admissions.front().materialized_tokens = true;
    refresh_expansion_result(materialized_admission);
    EXPECT_EQ(materialized_admission.summary.materialized_token_admission_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(materialized_admission));

    frontend::macro::EarlyItemExpansionResult generated_text = baseline;
    generated_text.token_materialization_admissions.front().generated_source_text = true;
    refresh_expansion_result(generated_text);
    EXPECT_EQ(generated_text.summary.generated_source_text_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(generated_text));

    frontend::macro::EarlyItemExpansionResult parse_ready = baseline;
    parse_ready.token_materialization_admissions.front().parse_ready = true;
    refresh_expansion_result(parse_ready);
    EXPECT_EQ(parse_ready.summary.parse_ready_token_buffer_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(parse_ready));

    frontend::macro::EarlyItemExpansionResult external_process = baseline;
    external_process.token_materialization_admissions.front().external_process_required = true;
    refresh_expansion_result(external_process);
    EXPECT_EQ(external_process.summary.external_process_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(external_process));

    frontend::macro::EarlyItemExpansionResult standard_library = baseline;
    standard_library.token_materialization_admissions.front().standard_library_required = true;
    refresh_expansion_result(standard_library);
    EXPECT_EQ(standard_library.summary.standard_library_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(standard_library));

    frontend::macro::EarlyItemExpansionResult runtime = baseline;
    runtime.token_materialization_admissions.front().runtime_required = true;
    refresh_expansion_result(runtime);
    EXPECT_EQ(runtime.summary.runtime_required_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(runtime));

    frontend::macro::EarlyItemExpansionResult user_code = baseline;
    user_code.token_materialization_admissions.front().produced_user_generated_code = true;
    refresh_expansion_result(user_code);
    EXPECT_EQ(user_code.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(user_code));
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsGeneratedTokenBufferDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.generated_token_buffers.empty());

    frontend::macro::EarlyItemExpansionResult empty_token_plan = baseline;
    empty_token_plan.generated_token_buffers.front().token_plan_identity = {};
    refresh_expansion_result(empty_token_plan);
    EXPECT_FALSE(frontend::macro::is_valid(empty_token_plan));

    frontend::macro::EarlyItemExpansionResult empty_token_buffer_identity = baseline;
    empty_token_buffer_identity.generated_token_buffers.front().token_buffer_identity = {};
    refresh_expansion_result(empty_token_buffer_identity);
    EXPECT_FALSE(frontend::macro::is_valid(empty_token_buffer_identity));

    frontend::macro::EarlyItemExpansionResult wrong_source_map = baseline;
    wrong_source_map.generated_token_buffers.front().source_map_identity =
        query::stable_fingerprint("wrong token buffer source map");
    refresh_expansion_result(wrong_source_map);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_source_map));

    frontend::macro::EarlyItemExpansionResult wrong_hygiene_mark = baseline;
    wrong_hygiene_mark.generated_token_buffers.front().hygiene_mark =
        query::stable_fingerprint("wrong token buffer hygiene mark");
    refresh_expansion_result(wrong_hygiene_mark);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_hygiene_mark));

    frontend::macro::EarlyItemExpansionResult wrong_stream_name = baseline;
    wrong_stream_name.generated_token_buffers.front().token_stream_name =
        "m21h-token-stream:wrong";
    refresh_expansion_result(wrong_stream_name);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_stream_name));

    frontend::macro::EarlyItemExpansionResult wrong_kind = baseline;
    wrong_kind.generated_token_buffers.front().token_buffer_kind = "wrong_token_buffer_kind";
    refresh_expansion_result(wrong_kind);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_kind));

    frontend::macro::EarlyItemExpansionResult wrong_producer_policy = baseline;
    wrong_producer_policy.generated_token_buffers.front().token_producer_policy =
        "wrong_token_producer_policy";
    refresh_expansion_result(wrong_producer_policy);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_producer_policy));

    frontend::macro::EarlyItemExpansionResult wrong_blocker = baseline;
    wrong_blocker.generated_token_buffers.front().blocker_reason = "wrong blocker";
    refresh_expansion_result(wrong_blocker);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_blocker));

    frontend::macro::EarlyItemExpansionResult empty_materialization_identity = baseline;
    empty_materialization_identity.generated_token_buffers.front().materialization_identity = {};
    refresh_expansion_result(empty_materialization_identity);
    EXPECT_FALSE(frontend::macro::is_valid(empty_materialization_identity));

    frontend::macro::EarlyItemExpansionResult non_empty_token_count = baseline;
    non_empty_token_count.generated_token_buffers.front().token_count = 1U;
    refresh_expansion_result(non_empty_token_count);
    EXPECT_FALSE(frontend::macro::is_valid(non_empty_token_count));

    frontend::macro::EarlyItemExpansionResult non_empty_buffer = baseline;
    non_empty_buffer.generated_token_buffers.front().empty = false;
    refresh_expansion_result(non_empty_buffer);
    EXPECT_EQ(non_empty_buffer.summary.empty_generated_token_buffer_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(non_empty_buffer));

    frontend::macro::EarlyItemExpansionResult materialized_tokens = baseline;
    materialized_tokens.generated_token_buffers.front().materialized_tokens = true;
    refresh_expansion_result(materialized_tokens);
    EXPECT_EQ(materialized_tokens.summary.materialized_token_buffer_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(materialized_tokens));

    frontend::macro::EarlyItemExpansionResult generated_text = baseline;
    generated_text.generated_token_buffers.front().generated_source_text = true;
    refresh_expansion_result(generated_text);
    EXPECT_EQ(generated_text.summary.generated_source_text_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(generated_text));

    frontend::macro::EarlyItemExpansionResult parser_consumable = baseline;
    parser_consumable.generated_token_buffers.front().parser_consumable = true;
    refresh_expansion_result(parser_consumable);
    EXPECT_EQ(parser_consumable.summary.parse_ready_token_buffer_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(parser_consumable));

    frontend::macro::EarlyItemExpansionResult user_code = baseline;
    user_code.generated_token_buffers.front().produced_user_generated_code = true;
    refresh_expansion_result(user_code);
    EXPECT_EQ(user_code.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(user_code));
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsDeriveGeneratedTokenPrototypeDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[derive(Copy, Eq)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_EQ(baseline.inputs.size(), 1U);
    ASSERT_EQ(baseline.token_materialization_admissions.size(), 1U);
    ASSERT_EQ(baseline.generated_token_buffers.size(), 1U);
    ASSERT_FALSE(baseline.generated_token_records.empty());
    EXPECT_TRUE(baseline.token_materialization_admissions.front().materialized_tokens);
    EXPECT_TRUE(baseline.generated_token_buffers.front().materialized_tokens);
    EXPECT_FALSE(baseline.generated_token_buffers.front().empty);
    EXPECT_EQ(baseline.summary.generated_token_record_count,
        baseline.inputs.front().token_count + EARLY_ITEM_EXPANSION_TEST_DERIVE_SENTINEL_TOKEN_COUNT);

    frontend::macro::EarlyItemExpansionResult non_materialized_admission = baseline;
    non_materialized_admission.token_materialization_admissions.front().materialized_tokens = false;
    refresh_expansion_result(non_materialized_admission);
    EXPECT_EQ(non_materialized_admission.summary.materialized_token_admission_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(non_materialized_admission));

    frontend::macro::EarlyItemExpansionResult empty_derive_buffer = baseline;
    empty_derive_buffer.generated_token_buffers.front().empty = true;
    refresh_expansion_result(empty_derive_buffer);
    EXPECT_EQ(empty_derive_buffer.summary.empty_generated_token_buffer_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(empty_derive_buffer));

    frontend::macro::EarlyItemExpansionResult non_materialized_buffer = baseline;
    non_materialized_buffer.generated_token_buffers.front().materialized_tokens = false;
    refresh_expansion_result(non_materialized_buffer);
    EXPECT_EQ(non_materialized_buffer.summary.materialized_token_buffer_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(non_materialized_buffer));

    frontend::macro::EarlyItemExpansionResult wrong_token_count = baseline;
    wrong_token_count.generated_token_buffers.front().token_count = 0U;
    refresh_expansion_result(wrong_token_count);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_token_count));

    frontend::macro::EarlyItemExpansionResult wrong_derive_policy = baseline;
    wrong_derive_policy.generated_token_buffers.front().token_producer_policy =
        "compiler_owned_blocked_empty_token_producer_v1";
    refresh_expansion_result(wrong_derive_policy);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_derive_policy));

    frontend::macro::EarlyItemExpansionResult wrong_materialization_identity = baseline;
    wrong_materialization_identity.generated_token_buffers.front().materialization_identity =
        query::stable_fingerprint("wrong derive materialization identity");
    refresh_expansion_result(wrong_materialization_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_materialization_identity));
}

TEST(CoreUnit, EarlyItemExpansionValidationRejectsGeneratedTokenRecordDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "#[derive(Copy, Eq)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    const base::usize derive_record_index = first_record_index_for_attribute(baseline, "derive");
    ASSERT_LT(derive_record_index, baseline.generated_token_records.size());

    frontend::macro::EarlyItemExpansionResult missing_records = baseline;
    missing_records.generated_token_records.clear();
    refresh_expansion_result(missing_records);
    EXPECT_EQ(missing_records.summary.generated_token_record_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(missing_records));

    frontend::macro::EarlyItemExpansionResult parser_visible = baseline;
    parser_visible.generated_token_records[derive_record_index].parser_visible = true;
    refresh_expansion_result(parser_visible);
    EXPECT_EQ(parser_visible.summary.parser_visible_generated_token_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(parser_visible));

    frontend::macro::EarlyItemExpansionResult user_generated_code = baseline;
    user_generated_code.generated_token_records[derive_record_index].produced_user_generated_code = true;
    refresh_expansion_result(user_generated_code);
    EXPECT_EQ(user_generated_code.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(user_generated_code));

    frontend::macro::EarlyItemExpansionResult wrong_token_identity = baseline;
    wrong_token_identity.generated_token_records[derive_record_index].token_identity =
        query::stable_fingerprint("wrong generated token identity");
    refresh_expansion_result(wrong_token_identity);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_token_identity));

    frontend::macro::EarlyItemExpansionResult wrong_begin_text = baseline;
    wrong_begin_text.generated_token_records[derive_record_index].text =
        "__aurex_builtin_derive_wrong_begin";
    refresh_expansion_result(wrong_begin_text);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_begin_text));

    frontend::macro::EarlyItemExpansionResult wrong_begin_role = baseline;
    wrong_begin_role.generated_token_records[derive_record_index].token_role =
        "wrong_generated_token_role";
    refresh_expansion_result(wrong_begin_role);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_begin_role));

    frontend::macro::EarlyItemExpansionResult wrong_begin_kind = baseline;
    wrong_begin_kind.generated_token_records[derive_record_index].kind = syntax::TokenKind::integer_literal;
    refresh_expansion_result(wrong_begin_kind);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_begin_kind));

    frontend::macro::EarlyItemExpansionResult invalid_record_kind = baseline;
    invalid_record_kind.generated_token_records[derive_record_index].kind = syntax::TokenKind::invalid;
    refresh_expansion_result(invalid_record_kind);
    EXPECT_FALSE(frontend::macro::is_valid(invalid_record_kind));

    frontend::macro::EarlyItemExpansionResult wrong_record_buffer = baseline;
    wrong_record_buffer.generated_token_records[derive_record_index].token_buffer_identity =
        query::stable_fingerprint("wrong generated token buffer identity");
    refresh_expansion_result(wrong_record_buffer);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_record_buffer));

    frontend::macro::EarlyItemExpansionResult stale_record_fingerprint = baseline;
    stale_record_fingerprint.generated_token_records[derive_record_index].text =
        "__aurex_builtin_derive_wrong_begin";
    refresh_expansion_result(stale_record_fingerprint);
    EXPECT_NE(stale_record_fingerprint.fingerprint, baseline.fingerprint);
}


TEST(CoreUnit, EarlyItemExpansionFingerprintTracksTokenMaterializationAdmissionContract)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.token_materialization_admissions.empty());
    ASSERT_FALSE(baseline.generated_token_buffers.empty());

    frontend::macro::EarlyItemExpansionResult token_plan = baseline;
    token_plan.token_materialization_admissions.front().token_plan_identity =
        query::stable_fingerprint("different token plan identity");
    refresh_expansion_result(token_plan);
    EXPECT_NE(token_plan.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(token_plan));

    frontend::macro::EarlyItemExpansionResult token_buffer_identity = baseline;
    token_buffer_identity.token_materialization_admissions.front().token_buffer_identity =
        query::stable_fingerprint("different token buffer identity");
    refresh_expansion_result(token_buffer_identity);
    EXPECT_NE(token_buffer_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(token_buffer_identity));

    frontend::macro::EarlyItemExpansionResult buffer_stream = baseline;
    buffer_stream.generated_token_buffers.front().token_stream_name =
        "m21h-token-stream:different";
    refresh_expansion_result(buffer_stream);
    EXPECT_NE(buffer_stream.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(buffer_stream));

    frontend::macro::EarlyItemExpansionResult buffer_kind = baseline;
    buffer_kind.generated_token_buffers.front().token_buffer_kind = "different_buffer_kind";
    refresh_expansion_result(buffer_kind);
    EXPECT_NE(buffer_kind.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(buffer_kind));
}
} // namespace aurex::test
