#include <gtest/frontend/macro/early_item_expansion/early_item_expansion_noop_fixture_support.hpp>

#include <gtest/gtest.h>

namespace aurex::test {

using namespace early_item_expansion_support;

TEST(CoreUnit, EarlyItemExpansionNoopCollectsGeneratedTokenRecords)
{
    const NoopAttributeExpansionFixture fixture = make_noop_attribute_expansion_fixture();
    const frontend::macro::EarlyItemExpansionResult& result = fixture.result;
    const NoopAttributeExpansionView view = inspect_noop_attribute_expansion(fixture);
    const syntax::ItemNode* const config = view.config;
    const auto* const builder = view.builder;
    const auto* const derive = view.derive;
    const auto* const derive_buffer = view.derive_buffer;
    ASSERT_NE(config, nullptr);
    ASSERT_EQ(config->attributes.size(), 2U);
    ASSERT_NE(builder, nullptr);
    ASSERT_NE(derive, nullptr);
    ASSERT_NE(derive_buffer, nullptr);

    const std::vector<const frontend::macro::GeneratedTokenRecord*> builder_records =
        token_records_for_input(result, *builder);
    EXPECT_TRUE(builder_records.empty());
    const std::vector<const frontend::macro::GeneratedTokenRecord*> derive_records =
        token_records_for_input(result, *derive);
    ASSERT_EQ(derive_records.size(), derive_buffer->token_count);
    const frontend::macro::GeneratedTokenRecord& begin_record = *derive_records.front();
    EXPECT_TRUE(frontend::macro::is_valid(begin_record));
    EXPECT_EQ(begin_record.token_buffer_identity, derive_buffer->token_buffer_identity);
    EXPECT_EQ(begin_record.token_index, 0U);
    EXPECT_EQ(begin_record.kind, syntax::TokenKind::identifier);
    EXPECT_EQ(begin_record.text, "__aurex_builtin_derive_begin");
    EXPECT_EQ(begin_record.token_role, "derive_codegen_begin");
    EXPECT_FALSE(begin_record.parser_visible);
    EXPECT_FALSE(begin_record.produced_user_generated_code);
    ASSERT_GE(derive_records.size(), 3U);
    const frontend::macro::GeneratedTokenRecord& first_source_record = *derive_records[1];
    EXPECT_EQ(first_source_record.kind, config->attributes[1].token_tree[0].kind);
    EXPECT_EQ(first_source_record.text, "__aurex_builtin_derive_source_token_1");
    EXPECT_EQ(first_source_record.token_role, "derive_source_token_placeholder");
    EXPECT_EQ(first_source_record.anchor_range.source.value,
        config->attributes[1].token_tree[0].range.source.value);
    EXPECT_EQ(first_source_record.anchor_range.begin, config->attributes[1].token_tree[0].range.begin);
    EXPECT_EQ(first_source_record.anchor_range.end, config->attributes[1].token_tree[0].range.end);
    const frontend::macro::GeneratedTokenRecord& end_record = *derive_records.back();
    EXPECT_EQ(end_record.token_index, derive_buffer->token_count - 1U);
    EXPECT_EQ(end_record.kind, syntax::TokenKind::identifier);
    EXPECT_EQ(end_record.text, "__aurex_builtin_derive_end");
    EXPECT_EQ(end_record.token_role, "derive_codegen_end");
    EXPECT_EQ(end_record.token_buffer_identity, derive_buffer->token_buffer_identity);
    EXPECT_NE(begin_record.token_identity, first_source_record.token_identity);
    EXPECT_NE(first_source_record.token_identity, end_record.token_identity);
}

} // namespace aurex::test
