#include <gtest/frontend/macro/early_item_expansion/early_item_expansion_common_support.hpp>

#include <gtest/gtest.h>

namespace aurex::test {

using namespace early_item_expansion_support;

TEST(CoreUnit, EarlyItemExpansionDispositionNamesExposeInvalidFallback)
{
    using frontend::macro::EarlyItemExpansionDisposition;

    EXPECT_EQ(frontend::macro::early_item_expansion_disposition_name(
                  EarlyItemExpansionDisposition::builtin_derive_passthrough),
        "builtin_derive_passthrough");
    EXPECT_EQ(frontend::macro::early_item_expansion_disposition_name(
                  EarlyItemExpansionDisposition::blocked_unimplemented_attribute),
        "blocked_unimplemented_attribute");
    EXPECT_EQ(frontend::macro::early_item_expansion_disposition_name(
                  static_cast<EarlyItemExpansionDisposition>(EARLY_ITEM_EXPANSION_TEST_INVALID_DISPOSITION)),
        "invalid");
    EXPECT_TRUE(frontend::macro::is_valid(EarlyItemExpansionDisposition::builtin_derive_passthrough));
    EXPECT_FALSE(frontend::macro::is_valid(
        static_cast<EarlyItemExpansionDisposition>(EARLY_ITEM_EXPANSION_TEST_INVALID_DISPOSITION)));
}

TEST(CoreUnit, EarlyItemExpansionLifecycleNamesExposeInvalidFallback)
{
    using frontend::macro::GeneratedModulePartLifecycleState;

    EXPECT_EQ(frontend::macro::generated_module_part_lifecycle_state_name(
                  GeneratedModulePartLifecycleState::planned),
        "planned");
    EXPECT_EQ(frontend::macro::generated_module_part_lifecycle_state_name(
                  GeneratedModulePartLifecycleState::materialized_buffer_stub),
        "materialized_buffer_stub");
    EXPECT_EQ(frontend::macro::generated_module_part_lifecycle_state_name(
                  GeneratedModulePartLifecycleState::parse_blocked),
        "parse_blocked");
    EXPECT_EQ(frontend::macro::generated_module_part_lifecycle_state_name(
                  GeneratedModulePartLifecycleState::merge_blocked),
        "merge_blocked");
    EXPECT_EQ(frontend::macro::generated_module_part_lifecycle_state_name(
                  static_cast<GeneratedModulePartLifecycleState>(
                      EARLY_ITEM_EXPANSION_TEST_INVALID_LIFECYCLE_STATE)),
        "invalid");
    EXPECT_TRUE(frontend::macro::is_valid(GeneratedModulePartLifecycleState::planned));
    EXPECT_TRUE(frontend::macro::is_valid(GeneratedModulePartLifecycleState::merge_blocked));
    EXPECT_FALSE(frontend::macro::is_valid(
        static_cast<GeneratedModulePartLifecycleState>(EARLY_ITEM_EXPANSION_TEST_INVALID_LIFECYCLE_STATE)));
}
} // namespace aurex::test
