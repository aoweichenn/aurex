#include <gtest/frontend/macro/early_item_expansion/early_item_expansion_noop_fixture_support.hpp>

#include <gtest/gtest.h>

namespace aurex::test {

using namespace early_item_expansion_support;

TEST(CoreUnit, EarlyItemExpansionNoopCollectsInputAndGeneratedTokenStubs)
{
    const NoopAttributeExpansionFixture fixture = make_noop_attribute_expansion_fixture();
    const frontend::macro::EarlyItemExpansionResult& result = fixture.result;
    const syntax::ItemNode* const config = find_item(fixture.module, "Config");
    ASSERT_NE(config, nullptr);
    ASSERT_EQ(config->attributes.size(), 2U);
    const auto& part_keys = fixture.part_keys;

    const frontend::macro::EarlyItemMacroInput* const builder = input_by_attribute(result, "builder");
    ASSERT_NE(builder, nullptr);
    EXPECT_EQ(builder->attribute_index, 0U);
    EXPECT_EQ(builder->part_index, 0U);
    EXPECT_EQ(builder->token_count, config->attributes[0].token_tree.size());
    EXPECT_EQ(builder->disposition,
        frontend::macro::EarlyItemExpansionDisposition::blocked_unimplemented_attribute);
    EXPECT_EQ(builder->attached_part, part_keys[0][0]);
    EXPECT_GT(builder->token_tree_fingerprint.byte_count, 0U);
    EXPECT_GT(builder->query_key_fingerprint.byte_count, 0U);

    const frontend::macro::EarlyItemMacroInput* const derive = input_by_attribute(result, "derive");
    ASSERT_NE(derive, nullptr);
    EXPECT_EQ(derive->attribute_index, 1U);
    EXPECT_EQ(derive->disposition,
        frontend::macro::EarlyItemExpansionDisposition::builtin_derive_passthrough);
    EXPECT_EQ(derive->token_count, config->attributes[1].token_tree.size());
    EXPECT_NE(derive->query_key_fingerprint, builder->query_key_fingerprint);

    ASSERT_EQ(result.generated_parts.size(), 1U);
    const frontend::macro::GeneratedModulePartPlaceholder& generated = result.generated_parts.front();
    EXPECT_EQ(generated.module.value, 0U);
    EXPECT_EQ(generated.source_part_index, 0U);
    EXPECT_EQ(generated.generated_stable_index, EARLY_ITEM_EXPANSION_TEST_GENERATED_PART_INDEX_OFFSET);
    EXPECT_EQ(generated.source_role, query::SourceRole::generated);
    EXPECT_EQ(generated.part_kind, query::ModulePartKind::generated);
    EXPECT_EQ(generated.source_part, part_keys[0][0]);
    EXPECT_EQ(generated.generated_part.kind, query::ModulePartKind::generated);
    EXPECT_EQ(generated.generated_part.file.role, query::SourceRole::generated);
    EXPECT_NE(generated.generated_part, generated.source_part);
    EXPECT_FALSE(generated.parsed);
    EXPECT_FALSE(generated.merged);
    EXPECT_FALSE(generated.produced_user_generated_code);

    ASSERT_EQ(result.generated_part_stubs.size(), 1U);
    const frontend::macro::GeneratedModulePartParseMergeStub& stub =
        result.generated_part_stubs.front();
    EXPECT_TRUE(frontend::macro::is_valid(stub));
    EXPECT_EQ(stub.module.value, generated.module.value);
    EXPECT_EQ(stub.source_part_index, generated.source_part_index);
    EXPECT_EQ(stub.generated_stable_index, generated.generated_stable_index);
    EXPECT_EQ(stub.source_part, generated.source_part);
    EXPECT_EQ(stub.generated_part, generated.generated_part);
    EXPECT_GT(stub.generated_buffer_identity.byte_count, 0U);
    EXPECT_GT(stub.parse_config_fingerprint.byte_count, 0U);
    EXPECT_GT(stub.merge_ordering_key.byte_count, 0U);
    EXPECT_EQ(stub.expansion_origin, generated.output_fingerprint);
    expect_contains(stub.generated_buffer_name, "m21e-noop-generated-buffer:");
    expect_contains(stub.blocker_reason, "parse and merge are blocked in M21e");
    EXPECT_EQ(stub.lifecycle_state,
        frontend::macro::GeneratedModulePartLifecycleState::merge_blocked);
    EXPECT_TRUE(stub.materialized_buffer);
    EXPECT_FALSE(stub.parsed);
    EXPECT_FALSE(stub.merged);
    EXPECT_FALSE(stub.sema_visible);
    EXPECT_FALSE(stub.produced_user_generated_code);

    ASSERT_EQ(result.source_maps.size(), 2U);
    for (const frontend::macro::ExpansionSourceMapPlaceholder& source_map : result.source_maps) {
        EXPECT_FALSE(source_map.real_source_map);
        EXPECT_FALSE(source_map.debug_trace_available);
        EXPECT_GT(source_map.expansion_origin.byte_count, 0U);
    }
    EXPECT_EQ(result.source_maps[0].expansion_origin, builder->query_key_fingerprint);
    EXPECT_EQ(result.source_maps[1].expansion_origin, derive->query_key_fingerprint);

    ASSERT_EQ(result.hygiene_stubs.size(), 2U);
    const frontend::macro::ExpansionHygieneStub* const builder_hygiene =
        hygiene_stub_for_input(result, *builder);
    ASSERT_NE(builder_hygiene, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*builder_hygiene));
    EXPECT_EQ(builder_hygiene->part_index, builder->part_index);
    EXPECT_EQ(builder_hygiene->attribute_index, builder->attribute_index);
    EXPECT_EQ(builder_hygiene->attached_part, builder->attached_part);
    EXPECT_EQ(builder_hygiene->expansion_origin, builder->query_key_fingerprint);
    EXPECT_GT(builder_hygiene->call_site_mark.byte_count, 0U);
    EXPECT_GT(builder_hygiene->definition_site_mark.byte_count, 0U);
    EXPECT_GT(builder_hygiene->generated_fresh_mark.byte_count, 0U);
    EXPECT_GT(builder_hygiene->declared_name_set.byte_count, 0U);
    EXPECT_NE(builder_hygiene->call_site_mark, builder_hygiene->definition_site_mark);
    EXPECT_NE(builder_hygiene->definition_site_mark, builder_hygiene->generated_fresh_mark);
    EXPECT_EQ(builder_hygiene->policy, "origin_mark_hygiene_v1");
    EXPECT_FALSE(builder_hygiene->resolved);
    EXPECT_FALSE(builder_hygiene->declared_names_visible);
    EXPECT_FALSE(builder_hygiene->captures_call_site_locals);

    ASSERT_EQ(result.trace_stubs.size(), 2U);
    const frontend::macro::ExpansionTraceStub* const builder_trace =
        trace_stub_for_input(result, *builder);
    ASSERT_NE(builder_trace, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*builder_trace));
    EXPECT_EQ(builder_trace->part_index, builder->part_index);
    EXPECT_EQ(builder_trace->attribute_index, builder->attribute_index);
    EXPECT_EQ(builder_trace->attached_part, builder->attached_part);
    EXPECT_EQ(builder_trace->attribute_range.source.value, builder->attribute_range.source.value);
    EXPECT_EQ(builder_trace->attribute_range.begin, builder->attribute_range.begin);
    EXPECT_EQ(builder_trace->attribute_range.end, builder->attribute_range.end);
    EXPECT_EQ(builder_trace->token_tree_range.source.value, builder->token_tree_range.source.value);
    EXPECT_EQ(builder_trace->token_tree_range.begin, builder->token_tree_range.begin);
    EXPECT_EQ(builder_trace->token_tree_range.end, builder->token_tree_range.end);
    EXPECT_EQ(builder_trace->expansion_origin, builder->query_key_fingerprint);
    EXPECT_GT(builder_trace->trace_identity.byte_count, 0U);
    EXPECT_GT(builder_trace->generated_source_map_identity.byte_count, 0U);
    EXPECT_GT(builder_trace->diagnostic_anchor.byte_count, 0U);
    EXPECT_NE(builder_trace->trace_identity, builder_trace->generated_source_map_identity);
    EXPECT_EQ(builder_trace->trace_policy, "expansion_source_map_debug_trace_v1");
    expect_contains(builder_trace->blocker_reason, "blocked in M21f");
    EXPECT_FALSE(builder_trace->real_source_map);
    EXPECT_FALSE(builder_trace->debug_trace_available);
    EXPECT_FALSE(builder_trace->cli_emit_expanded_available);

    ASSERT_EQ(result.generated_item_declarations.size(), 2U);
    const frontend::macro::GeneratedItemDeclarationStub* const builder_declaration =
        generated_item_declaration_for_input(result, *builder);
    ASSERT_NE(builder_declaration, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*builder_declaration));
    EXPECT_EQ(builder_declaration->part_index, builder->part_index);
    EXPECT_EQ(builder_declaration->attribute_index, builder->attribute_index);
    EXPECT_EQ(builder_declaration->attached_part, builder->attached_part);
    EXPECT_EQ(builder_declaration->generated_part, generated.generated_part);
    EXPECT_EQ(builder_declaration->expansion_origin, builder->query_key_fingerprint);
    EXPECT_EQ(builder_declaration->declared_name_set, builder_hygiene->declared_name_set);
    EXPECT_GT(builder_declaration->declaration_identity.byte_count, 0U);
    EXPECT_GT(builder_declaration->generated_item_key.byte_count, 0U);
    EXPECT_NE(builder_declaration->declaration_identity, builder_declaration->generated_item_key);
    EXPECT_EQ(builder_declaration->declaration_role, "attached_item_codegen_declared_names_v1");
    expect_contains(builder_declaration->generated_item_name, "__aurex_macro_declared:0:0:0:0:builder");
    expect_contains(builder_declaration->blocker_reason, "blocked in M21g");
    EXPECT_TRUE(builder_declaration->planned);
    EXPECT_FALSE(builder_declaration->materialized_tokens);
    EXPECT_FALSE(builder_declaration->parsed);
    EXPECT_FALSE(builder_declaration->merged);
    EXPECT_FALSE(builder_declaration->sema_visible);
    EXPECT_FALSE(builder_declaration->produced_user_generated_code);

    const frontend::macro::GeneratedItemDeclarationStub* const derive_declaration =
        generated_item_declaration_for_input(result, *derive);
    ASSERT_NE(derive_declaration, nullptr);
    EXPECT_NE(derive_declaration->generated_item_name, builder_declaration->generated_item_name);
    expect_contains(derive_declaration->generated_item_name, "__aurex_macro_declared:0:0:0:1:derive");

    ASSERT_EQ(result.declared_generated_names.size(), 2U);
    const frontend::macro::DeclaredGeneratedNameStub* const builder_declared_name =
        declared_generated_name_for_input(result, *builder);
    ASSERT_NE(builder_declared_name, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*builder_declared_name));
    EXPECT_EQ(builder_declared_name->part_index, builder->part_index);
    EXPECT_EQ(builder_declared_name->attribute_index, builder->attribute_index);
    EXPECT_EQ(builder_declared_name->attached_part, builder->attached_part);
    EXPECT_EQ(builder_declared_name->generated_part, generated.generated_part);
    EXPECT_EQ(builder_declared_name->expansion_origin, builder->query_key_fingerprint);
    EXPECT_EQ(builder_declared_name->declared_name_set, builder_hygiene->declared_name_set);
    EXPECT_EQ(builder_declared_name->hygiene_mark, builder_hygiene->generated_fresh_mark);
    EXPECT_EQ(builder_declared_name->declared_name, builder_declaration->generated_item_name);
    EXPECT_EQ(builder_declared_name->namespace_kind, "item");
    EXPECT_GT(builder_declared_name->declared_name_identity.byte_count, 0U);
    expect_contains(builder_declared_name->blocker_reason, "lookup is blocked in M21g");
    EXPECT_FALSE(builder_declared_name->lookup_visible);
    EXPECT_FALSE(builder_declared_name->export_visible);
    EXPECT_FALSE(builder_declared_name->sema_visible);
    EXPECT_FALSE(builder_declared_name->produced_user_generated_code);

    ASSERT_EQ(result.token_materialization_admissions.size(), 2U);
    const frontend::macro::TokenMaterializationAdmissionStub* const builder_admission =
        token_admission_for_input(result, *builder);
    ASSERT_NE(builder_admission, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*builder_admission));
    EXPECT_EQ(builder_admission->part_index, builder->part_index);
    EXPECT_EQ(builder_admission->attribute_index, builder->attribute_index);
    EXPECT_EQ(builder_admission->attached_part, builder->attached_part);
    EXPECT_EQ(builder_admission->generated_part, generated.generated_part);
    EXPECT_EQ(builder_admission->expansion_origin, builder->query_key_fingerprint);
    EXPECT_EQ(builder_admission->declaration_identity, builder_declaration->declaration_identity);
    EXPECT_EQ(builder_admission->generated_item_key, builder_declaration->generated_item_key);
    EXPECT_EQ(builder_admission->declared_name_set, builder_hygiene->declared_name_set);
    EXPECT_EQ(builder_admission->declared_name_identity, builder_declared_name->declared_name_identity);
    EXPECT_EQ(builder_admission->hygiene_mark, builder_hygiene->generated_fresh_mark);
    EXPECT_EQ(builder_admission->source_map_identity, builder_trace->generated_source_map_identity);
    EXPECT_EQ(builder_admission->trace_identity, builder_trace->trace_identity);
    EXPECT_GT(builder_admission->token_plan_identity.byte_count, 0U);
    EXPECT_GT(builder_admission->token_buffer_identity.byte_count, 0U);
    EXPECT_NE(builder_admission->token_plan_identity, builder_admission->token_buffer_identity);
    EXPECT_EQ(builder_admission->admission_policy,
        "compiler_owned_attached_item_token_materialization_admission_v1");
    expect_contains(builder_admission->token_stream_name, "m21h-token-stream:0:0:0:0:builder");
    expect_contains(builder_admission->blocker_reason, "non-derive item attribute token materialization remains blocked in M21i");
    EXPECT_TRUE(builder_admission->compiler_owned);
    EXPECT_TRUE(builder_admission->admitted);
    EXPECT_FALSE(builder_admission->materialized_tokens);
    EXPECT_FALSE(builder_admission->generated_source_text);
    EXPECT_FALSE(builder_admission->parse_ready);
    EXPECT_FALSE(builder_admission->external_process_required);
    EXPECT_FALSE(builder_admission->standard_library_required);
    EXPECT_FALSE(builder_admission->runtime_required);
    EXPECT_FALSE(builder_admission->produced_user_generated_code);

    const frontend::macro::TokenMaterializationAdmissionStub* const derive_admission =
        token_admission_for_input(result, *derive);
    ASSERT_NE(derive_admission, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*derive_admission));
    EXPECT_NE(derive_admission->token_stream_name, builder_admission->token_stream_name);
    expect_contains(derive_admission->token_stream_name, "m21h-token-stream:0:0:0:1:derive");
    expect_contains(derive_admission->blocker_reason, "derive token prototype remains parser-blocked in M21i");
    EXPECT_TRUE(derive_admission->compiler_owned);
    EXPECT_TRUE(derive_admission->admitted);
    EXPECT_TRUE(derive_admission->materialized_tokens);
    EXPECT_FALSE(derive_admission->generated_source_text);
    EXPECT_FALSE(derive_admission->parse_ready);
    EXPECT_FALSE(derive_admission->external_process_required);
    EXPECT_FALSE(derive_admission->standard_library_required);
    EXPECT_FALSE(derive_admission->runtime_required);
    EXPECT_FALSE(derive_admission->produced_user_generated_code);

    ASSERT_EQ(result.generated_token_buffers.size(), 2U);
    const frontend::macro::GeneratedTokenBufferStub* const builder_buffer =
        token_buffer_for_input(result, *builder);
    ASSERT_NE(builder_buffer, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*builder_buffer));
    EXPECT_EQ(builder_buffer->part_index, builder->part_index);
    EXPECT_EQ(builder_buffer->attribute_index, builder->attribute_index);
    EXPECT_EQ(builder_buffer->attached_part, builder->attached_part);
    EXPECT_EQ(builder_buffer->generated_part, generated.generated_part);
    EXPECT_EQ(builder_buffer->token_plan_identity, builder_admission->token_plan_identity);
    EXPECT_EQ(builder_buffer->token_buffer_identity, builder_admission->token_buffer_identity);
    EXPECT_EQ(builder_buffer->source_map_identity, builder_trace->generated_source_map_identity);
    EXPECT_EQ(builder_buffer->hygiene_mark, builder_hygiene->generated_fresh_mark);
    EXPECT_EQ(builder_buffer->token_stream_name, builder_admission->token_stream_name);
    EXPECT_EQ(builder_buffer->token_buffer_kind, "compiler_owned_empty_token_stream");
    EXPECT_EQ(builder_buffer->token_producer_policy, "compiler_owned_blocked_empty_token_producer_v1");
    EXPECT_GT(builder_buffer->materialization_identity.byte_count, 0U);
    expect_contains(builder_buffer->blocker_reason, "empty and parser-blocked in M21i");
    EXPECT_EQ(builder_buffer->token_count, 0U);
    EXPECT_TRUE(builder_buffer->empty);
    EXPECT_FALSE(builder_buffer->materialized_tokens);
    EXPECT_FALSE(builder_buffer->generated_source_text);
    EXPECT_FALSE(builder_buffer->parser_consumable);
    EXPECT_FALSE(builder_buffer->produced_user_generated_code);

    const frontend::macro::GeneratedTokenBufferStub* const derive_buffer =
        token_buffer_for_input(result, *derive);
    ASSERT_NE(derive_buffer, nullptr);
    EXPECT_TRUE(frontend::macro::is_valid(*derive_buffer));
    EXPECT_EQ(derive_buffer->part_index, derive->part_index);
    EXPECT_EQ(derive_buffer->attribute_index, derive->attribute_index);
    EXPECT_EQ(derive_buffer->attached_part, derive->attached_part);
    EXPECT_EQ(derive_buffer->generated_part, generated.generated_part);
    EXPECT_EQ(derive_buffer->token_plan_identity, derive_admission->token_plan_identity);
    EXPECT_EQ(derive_buffer->token_buffer_identity, derive_admission->token_buffer_identity);
    EXPECT_GT(derive_buffer->materialization_identity.byte_count, 0U);
    EXPECT_EQ(derive_buffer->source_map_identity, result.trace_stubs[1].generated_source_map_identity);
    EXPECT_EQ(derive_buffer->token_stream_name, derive_admission->token_stream_name);
    EXPECT_EQ(derive_buffer->token_buffer_kind, "compiler_owned_builtin_derive_token_stream_prototype");
    EXPECT_EQ(derive_buffer->token_producer_policy,
        "compiler_owned_builtin_derive_token_producer_prototype_v1");
    expect_contains(derive_buffer->blocker_reason, "generated token buffer remains parser-blocked in M21i");
    EXPECT_EQ(derive_buffer->token_count,
        derive->token_count + EARLY_ITEM_EXPANSION_TEST_DERIVE_SENTINEL_TOKEN_COUNT);
    EXPECT_FALSE(derive_buffer->empty);
    EXPECT_TRUE(derive_buffer->materialized_tokens);
    EXPECT_FALSE(derive_buffer->generated_source_text);
    EXPECT_FALSE(derive_buffer->parser_consumable);
    EXPECT_FALSE(derive_buffer->produced_user_generated_code);
}

} // namespace aurex::test
