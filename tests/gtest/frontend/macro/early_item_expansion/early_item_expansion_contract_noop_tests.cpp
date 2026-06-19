#include <gtest/frontend/macro/early_item_expansion/early_item_expansion_contract_support.hpp>

#include <gtest/gtest.h>

namespace aurex::test {

using namespace early_item_expansion_support;

TEST(CoreUnit, EarlyItemExpansionValidationRejectsNoopBoundaryDrift)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));

    frontend::macro::EarlyItemExpansionResult generated_code = baseline;
    ASSERT_FALSE(generated_code.generated_parts.empty());
    generated_code.generated_parts.front().produced_user_generated_code = true;
    refresh_expansion_result(generated_code);
    EXPECT_FALSE(frontend::macro::is_valid(generated_code));

    frontend::macro::EarlyItemExpansionResult parsed_part = baseline;
    ASSERT_FALSE(parsed_part.generated_parts.empty());
    parsed_part.generated_parts.front().parsed = true;
    refresh_expansion_result(parsed_part);
    EXPECT_FALSE(frontend::macro::is_valid(parsed_part));

    frontend::macro::EarlyItemExpansionResult merged_part = baseline;
    ASSERT_FALSE(merged_part.generated_parts.empty());
    merged_part.generated_parts.front().merged = true;
    refresh_expansion_result(merged_part);
    EXPECT_FALSE(frontend::macro::is_valid(merged_part));

    frontend::macro::EarlyItemExpansionResult missing_stub = baseline;
    ASSERT_FALSE(missing_stub.generated_part_stubs.empty());
    missing_stub.generated_part_stubs.clear();
    refresh_expansion_result(missing_stub);
    EXPECT_FALSE(frontend::macro::is_valid(missing_stub));

    frontend::macro::EarlyItemExpansionResult parse_blocked_stub = baseline;
    ASSERT_FALSE(parse_blocked_stub.generated_part_stubs.empty());
    parse_blocked_stub.generated_part_stubs.front().lifecycle_state =
        frontend::macro::GeneratedModulePartLifecycleState::parse_blocked;
    refresh_expansion_result(parse_blocked_stub);
    EXPECT_EQ(parse_blocked_stub.summary.parse_blocked_count, 1U);
    EXPECT_EQ(parse_blocked_stub.summary.merge_blocked_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(parse_blocked_stub));

    frontend::macro::EarlyItemExpansionResult invalid_lifecycle_stub = baseline;
    ASSERT_FALSE(invalid_lifecycle_stub.generated_part_stubs.empty());
    invalid_lifecycle_stub.generated_part_stubs.front().lifecycle_state =
        static_cast<frontend::macro::GeneratedModulePartLifecycleState>(
            EARLY_ITEM_EXPANSION_TEST_INVALID_LIFECYCLE_STATE);
    refresh_expansion_result(invalid_lifecycle_stub);
    EXPECT_FALSE(frontend::macro::is_valid(invalid_lifecycle_stub));

    frontend::macro::EarlyItemExpansionResult non_materialized_stub = baseline;
    ASSERT_FALSE(non_materialized_stub.generated_part_stubs.empty());
    non_materialized_stub.generated_part_stubs.front().materialized_buffer = false;
    refresh_expansion_result(non_materialized_stub);
    EXPECT_EQ(non_materialized_stub.summary.materialized_buffer_stub_count, 0U);
    EXPECT_FALSE(frontend::macro::is_valid(non_materialized_stub));

    frontend::macro::EarlyItemExpansionResult parsed_stub = baseline;
    ASSERT_FALSE(parsed_stub.generated_part_stubs.empty());
    parsed_stub.generated_part_stubs.front().parsed = true;
    refresh_expansion_result(parsed_stub);
    EXPECT_EQ(parsed_stub.summary.parsed_generated_part_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(parsed_stub));

    frontend::macro::EarlyItemExpansionResult merged_stub = baseline;
    ASSERT_FALSE(merged_stub.generated_part_stubs.empty());
    merged_stub.generated_part_stubs.front().merged = true;
    refresh_expansion_result(merged_stub);
    EXPECT_EQ(merged_stub.summary.merged_generated_part_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(merged_stub));

    frontend::macro::EarlyItemExpansionResult sema_visible_stub = baseline;
    ASSERT_FALSE(sema_visible_stub.generated_part_stubs.empty());
    sema_visible_stub.generated_part_stubs.front().sema_visible = true;
    refresh_expansion_result(sema_visible_stub);
    EXPECT_EQ(sema_visible_stub.summary.sema_visible_generated_part_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(sema_visible_stub));

    frontend::macro::EarlyItemExpansionResult generated_code_stub = baseline;
    ASSERT_FALSE(generated_code_stub.generated_part_stubs.empty());
    generated_code_stub.generated_part_stubs.front().produced_user_generated_code = true;
    refresh_expansion_result(generated_code_stub);
    EXPECT_EQ(generated_code_stub.summary.user_generated_code_count, 1U);
    EXPECT_FALSE(frontend::macro::is_valid(generated_code_stub));

    frontend::macro::EarlyItemExpansionResult empty_buffer_identity = baseline;
    ASSERT_FALSE(empty_buffer_identity.generated_part_stubs.empty());
    empty_buffer_identity.generated_part_stubs.front().generated_buffer_identity = {};
    refresh_expansion_result(empty_buffer_identity);
    EXPECT_FALSE(frontend::macro::is_valid(empty_buffer_identity));

    frontend::macro::EarlyItemExpansionResult empty_parse_config = baseline;
    ASSERT_FALSE(empty_parse_config.generated_part_stubs.empty());
    empty_parse_config.generated_part_stubs.front().parse_config_fingerprint = {};
    refresh_expansion_result(empty_parse_config);
    EXPECT_FALSE(frontend::macro::is_valid(empty_parse_config));

    frontend::macro::EarlyItemExpansionResult empty_merge_ordering = baseline;
    ASSERT_FALSE(empty_merge_ordering.generated_part_stubs.empty());
    empty_merge_ordering.generated_part_stubs.front().merge_ordering_key = {};
    refresh_expansion_result(empty_merge_ordering);
    EXPECT_FALSE(frontend::macro::is_valid(empty_merge_ordering));

    frontend::macro::EarlyItemExpansionResult empty_origin = baseline;
    ASSERT_FALSE(empty_origin.generated_part_stubs.empty());
    empty_origin.generated_part_stubs.front().expansion_origin = {};
    refresh_expansion_result(empty_origin);
    EXPECT_FALSE(frontend::macro::is_valid(empty_origin));

    frontend::macro::EarlyItemExpansionResult empty_buffer_name = baseline;
    ASSERT_FALSE(empty_buffer_name.generated_part_stubs.empty());
    empty_buffer_name.generated_part_stubs.front().generated_buffer_name.clear();
    refresh_expansion_result(empty_buffer_name);
    EXPECT_FALSE(frontend::macro::is_valid(empty_buffer_name));

    frontend::macro::EarlyItemExpansionResult empty_blocker = baseline;
    ASSERT_FALSE(empty_blocker.generated_part_stubs.empty());
    empty_blocker.generated_part_stubs.front().blocker_reason.clear();
    refresh_expansion_result(empty_blocker);
    EXPECT_FALSE(frontend::macro::is_valid(empty_blocker));

    frontend::macro::EarlyItemExpansionResult wrong_source_part = baseline;
    ASSERT_FALSE(wrong_source_part.generated_part_stubs.empty());
    wrong_source_part.generated_part_stubs.front().source_part =
        wrong_source_part.generated_part_stubs.front().generated_part;
    refresh_expansion_result(wrong_source_part);
    EXPECT_FALSE(frontend::macro::is_valid(wrong_source_part));

    frontend::macro::EarlyItemExpansionResult real_source_map = baseline;
    ASSERT_FALSE(real_source_map.source_maps.empty());
    real_source_map.source_maps.front().real_source_map = true;
    refresh_expansion_result(real_source_map);
    EXPECT_FALSE(frontend::macro::is_valid(real_source_map));

    frontend::macro::EarlyItemExpansionResult source_map_debug_trace = baseline;
    ASSERT_FALSE(source_map_debug_trace.source_maps.empty());
    source_map_debug_trace.source_maps.front().debug_trace_available = true;
    refresh_expansion_result(source_map_debug_trace);
    EXPECT_FALSE(frontend::macro::is_valid(source_map_debug_trace));

    frontend::macro::EarlyItemExpansionResult stale_summary = baseline;
    stale_summary.summary.macro_input_count += 1U;
    EXPECT_FALSE(frontend::macro::is_valid(stale_summary));

    frontend::macro::EarlyItemExpansionResult stale_fingerprint = baseline;
    stale_fingerprint.fingerprint = query::stable_fingerprint("stale early item expansion");
    EXPECT_FALSE(frontend::macro::is_valid(stale_fingerprint));
}

TEST(CoreUnit, EarlyItemExpansionFingerprintTracksParseMergeStubContract)
{
    constexpr std::string_view source =
        "module macro.early_item_expansion;\n"
        "#[builder(flag)]\n"
        "struct Config { threads: i32; }\n";

    const frontend::macro::EarlyItemExpansionResult baseline = expand_source(source);
    ASSERT_TRUE(frontend::macro::is_valid(baseline));
    ASSERT_FALSE(baseline.generated_part_stubs.empty());

    frontend::macro::EarlyItemExpansionResult buffer_identity = baseline;
    buffer_identity.generated_part_stubs.front().generated_buffer_identity =
        query::stable_fingerprint("different generated buffer identity");
    refresh_expansion_result(buffer_identity);
    EXPECT_NE(buffer_identity.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(buffer_identity));

    frontend::macro::EarlyItemExpansionResult merge_order = baseline;
    merge_order.generated_part_stubs.front().merge_ordering_key =
        query::stable_fingerprint("different merge ordering");
    refresh_expansion_result(merge_order);
    EXPECT_NE(merge_order.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(merge_order));

    frontend::macro::EarlyItemExpansionResult blocker = baseline;
    blocker.generated_part_stubs.front().blocker_reason = "different blocker";
    refresh_expansion_result(blocker);
    EXPECT_NE(blocker.fingerprint, baseline.fingerprint);
    EXPECT_FALSE(frontend::macro::is_valid(blocker));
}
} // namespace aurex::test
