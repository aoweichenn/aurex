#include <aurex/frontend/macro/early_item_expansion.hpp>
#include <aurex/infrastructure/base/integer.hpp>

#include <algorithm>
#include <sstream>
#include <utility>

namespace aurex::frontend::macro {
namespace {

constexpr std::string_view FRONTEND_MACRO_M21F_EXPANSION_NAME =
    "M21f Hygiene Source Map Debug Trace Stub Contract";
constexpr std::string_view FRONTEND_MACRO_M21F_EXPANSION_FINGERPRINT_MARKER =
    "frontend.macro.m21f.hygiene_source_map_debug_trace_stub_contract.v1";
constexpr std::string_view FRONTEND_MACRO_M21D_TOKEN_TREE_FINGERPRINT_MARKER =
    "frontend.macro.m21d.attribute_token_tree.v1";
constexpr std::string_view FRONTEND_MACRO_M21D_QUERY_KEY_FINGERPRINT_MARKER =
    "frontend.macro.m21d.early_item_query_key.v1";
constexpr std::string_view FRONTEND_MACRO_M21D_GENERATED_PART_NAME_PREFIX = "#macro-generated:";
constexpr std::string_view FRONTEND_MACRO_M21D_ITEM_MODULES_MISMATCH =
    "early item macro expansion requires one module owner per item";
constexpr std::string_view FRONTEND_MACRO_M21D_ITEM_PARTS_MISMATCH =
    "early item macro expansion requires one module part index per item";
constexpr std::string_view FRONTEND_MACRO_M21D_INVALID_PLAN =
    "early item macro expansion requires a valid M21c macro expansion plan";
constexpr std::string_view FRONTEND_MACRO_M21D_MISSING_MODULE_PART_KEY =
    "early item macro expansion missing module part key for attached item";
constexpr std::string_view FRONTEND_MACRO_M21D_GENERATED_PART_PLACEHOLDER_MARKER =
    "frontend.macro.m21d.generated_part_placeholder.v1";
constexpr base::u32 FRONTEND_MACRO_M21D_GENERATED_PART_INDEX_OFFSET = 100'000U;
constexpr std::string_view FRONTEND_MACRO_M21E_PARSE_MERGE_STUB_MARKER =
    "frontend.macro.m21e.generated_part_parse_merge_stub.v1";
constexpr std::string_view FRONTEND_MACRO_M21E_GENERATED_BUFFER_IDENTITY_MARKER =
    "frontend.macro.m21e.generated_part_buffer_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M21E_PARSE_CONFIG_MARKER =
    "frontend.macro.m21e.generated_part_parse_config.v1";
constexpr std::string_view FRONTEND_MACRO_M21E_MERGE_ORDERING_MARKER =
    "frontend.macro.m21e.generated_part_merge_ordering.v1";
constexpr std::string_view FRONTEND_MACRO_M21E_GENERATED_BUFFER_PREFIX =
    "m21e-noop-generated-buffer:";
constexpr std::string_view FRONTEND_MACRO_M21E_PARSE_MERGE_BLOCKER =
    "generated module part parse and merge are blocked in M21e";
constexpr std::string_view FRONTEND_MACRO_M21F_HYGIENE_STUB_MARKER =
    "frontend.macro.m21f.hygiene_stub.v1";
constexpr std::string_view FRONTEND_MACRO_M21F_CALL_SITE_MARK_MARKER =
    "frontend.macro.m21f.call_site_mark.v1";
constexpr std::string_view FRONTEND_MACRO_M21F_DEFINITION_SITE_MARK_MARKER =
    "frontend.macro.m21f.definition_site_mark.v1";
constexpr std::string_view FRONTEND_MACRO_M21F_GENERATED_FRESH_MARK_MARKER =
    "frontend.macro.m21f.generated_fresh_mark.v1";
constexpr std::string_view FRONTEND_MACRO_M21F_DECLARED_NAME_SET_MARKER =
    "frontend.macro.m21f.declared_name_set.v1";
constexpr std::string_view FRONTEND_MACRO_M21F_TRACE_STUB_MARKER =
    "frontend.macro.m21f.trace_stub.v1";
constexpr std::string_view FRONTEND_MACRO_M21F_TRACE_IDENTITY_MARKER =
    "frontend.macro.m21f.trace_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M21F_GENERATED_SOURCE_MAP_MARKER =
    "frontend.macro.m21f.generated_source_map_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M21F_DIAGNOSTIC_ANCHOR_MARKER =
    "frontend.macro.m21f.diagnostic_anchor.v1";
constexpr std::string_view FRONTEND_MACRO_M21F_HYGIENE_POLICY = "origin_mark_hygiene_v1";
constexpr std::string_view FRONTEND_MACRO_M21F_TRACE_POLICY = "expansion_source_map_debug_trace_v1";
constexpr std::string_view FRONTEND_MACRO_M21F_TRACE_BLOCKER =
    "real macro source map and debug trace are blocked in M21f";

[[nodiscard]] base::Error internal_error(const std::string_view message)
{
    return base::Error{base::ErrorCode::internal_error, std::string(message)};
}

[[nodiscard]] bool source_range_is_well_formed(const base::SourceRange& range) noexcept
{
    return range.well_formed();
}

[[nodiscard]] bool source_ranges_equal(
    const base::SourceRange& lhs, const base::SourceRange& rhs) noexcept
{
    return lhs.source.value == rhs.source.value && lhs.begin == rhs.begin && lhs.end == rhs.end;
}

[[nodiscard]] bool is_nonzero_fingerprint(const query::StableFingerprint128 fingerprint) noexcept
{
    return fingerprint != query::StableFingerprint128{};
}

[[nodiscard]] bool item_id_in_range(const syntax::AstModule& ast, const syntax::ItemId item) noexcept
{
    return syntax::is_valid(item) && item.value < ast.items.size();
}

[[nodiscard]] bool module_id_in_range(const syntax::AstModule& ast, const syntax::ModuleId module) noexcept
{
    return syntax::is_valid(module) && module.value < ast.modules.size();
}

[[nodiscard]] std::string module_part_generated_name(
    const syntax::ModuleId module, const base::u32 part_index)
{
    std::string name(FRONTEND_MACRO_M21D_GENERATED_PART_NAME_PREFIX);
    name += std::to_string(module.value);
    name.push_back(':');
    name += std::to_string(part_index);
    return name;
}

[[nodiscard]] std::string module_part_generated_virtual_buffer(
    const syntax::ModuleId module, const base::u32 part_index)
{
    std::string buffer(FRONTEND_MACRO_M21E_GENERATED_BUFFER_PREFIX);
    buffer += std::to_string(module.value);
    buffer.push_back(':');
    buffer += std::to_string(part_index);
    return buffer;
}

[[nodiscard]] query::StableFingerprint128 generated_buffer_identity(
    const GeneratedModulePartPlaceholder& placeholder, const std::string_view generated_buffer_name) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21E_GENERATED_BUFFER_IDENTITY_MARKER);
    builder.mix_u32(placeholder.module.value);
    builder.mix_u32(placeholder.source_part_index);
    builder.mix_u32(placeholder.generated_stable_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.source_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    builder.mix_string(generated_buffer_name);
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 parse_config_fingerprint(
    const GeneratedModulePartPlaceholder& placeholder) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21E_PARSE_CONFIG_MARKER);
    builder.mix_fingerprint(query::stable_key_fingerprint(query::parser_config_key()));
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 merge_ordering_key(
    const GeneratedModulePartPlaceholder& placeholder) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21E_MERGE_ORDERING_MARKER);
    builder.mix_u32(placeholder.module.value);
    builder.mix_u32(placeholder.source_part_index);
    builder.mix_u32(placeholder.generated_stable_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.source_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    return builder.finish();
}

void mix_macro_input_identity(query::StableHashBuilder& builder, const EarlyItemMacroInput& input) noexcept
{
    builder.mix_u32(input.item.value);
    builder.mix_u32(input.module.value);
    builder.mix_u32(input.part_index);
    builder.mix_u32(input.attribute_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(input.attached_part));
    builder.mix_fingerprint(input.query_key_fingerprint);
}

[[nodiscard]] query::StableFingerprint128 hygiene_mark_fingerprint(
    const std::string_view marker, const EarlyItemMacroInput& input) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(marker);
    mix_macro_input_identity(builder, input);
    builder.mix_fingerprint(input.token_tree_fingerprint);
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 trace_stub_fingerprint(
    const std::string_view marker, const EarlyItemMacroInput& input) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(marker);
    mix_macro_input_identity(builder, input);
    builder.mix_u64(static_cast<base::u64>(input.attribute_range.source.value));
    builder.mix_u64(static_cast<base::u64>(input.attribute_range.begin));
    builder.mix_u64(static_cast<base::u64>(input.attribute_range.end));
    builder.mix_u64(static_cast<base::u64>(input.token_tree_range.source.value));
    builder.mix_u64(static_cast<base::u64>(input.token_tree_range.begin));
    builder.mix_u64(static_cast<base::u64>(input.token_tree_range.end));
    return builder.finish();
}

[[nodiscard]] GeneratedModulePartParseMergeStub make_parse_merge_stub(
    const GeneratedModulePartPlaceholder& placeholder)
{
    const std::string generated_buffer_name =
        module_part_generated_virtual_buffer(placeholder.module, placeholder.generated_stable_index);
    return GeneratedModulePartParseMergeStub{
        placeholder.module,
        placeholder.source_part_index,
        placeholder.generated_stable_index,
        placeholder.source_part,
        placeholder.generated_part,
        generated_buffer_identity(placeholder, generated_buffer_name),
        parse_config_fingerprint(placeholder),
        merge_ordering_key(placeholder),
        placeholder.output_fingerprint,
        generated_buffer_name,
        std::string(FRONTEND_MACRO_M21E_PARSE_MERGE_BLOCKER),
        GeneratedModulePartLifecycleState::merge_blocked,
        true,
        false,
        false,
        false,
        false,
    };
}

[[nodiscard]] ExpansionHygieneStub make_hygiene_stub(const EarlyItemMacroInput& input)
{
    return ExpansionHygieneStub{
        input.item,
        input.module,
        input.part_index,
        input.attribute_index,
        input.attached_part,
        input.query_key_fingerprint,
        hygiene_mark_fingerprint(FRONTEND_MACRO_M21F_CALL_SITE_MARK_MARKER, input),
        hygiene_mark_fingerprint(FRONTEND_MACRO_M21F_DEFINITION_SITE_MARK_MARKER, input),
        hygiene_mark_fingerprint(FRONTEND_MACRO_M21F_GENERATED_FRESH_MARK_MARKER, input),
        hygiene_mark_fingerprint(FRONTEND_MACRO_M21F_DECLARED_NAME_SET_MARKER, input),
        std::string(FRONTEND_MACRO_M21F_HYGIENE_POLICY),
        false,
        false,
        false,
    };
}

[[nodiscard]] ExpansionTraceStub make_trace_stub(const EarlyItemMacroInput& input)
{
    return ExpansionTraceStub{
        input.item,
        input.module,
        input.part_index,
        input.attribute_index,
        input.attached_part,
        input.attribute_range,
        input.token_tree_range,
        input.query_key_fingerprint,
        trace_stub_fingerprint(FRONTEND_MACRO_M21F_TRACE_IDENTITY_MARKER, input),
        trace_stub_fingerprint(FRONTEND_MACRO_M21F_GENERATED_SOURCE_MAP_MARKER, input),
        trace_stub_fingerprint(FRONTEND_MACRO_M21F_DIAGNOSTIC_ANCHOR_MARKER, input),
        std::string(FRONTEND_MACRO_M21F_TRACE_POLICY),
        std::string(FRONTEND_MACRO_M21F_TRACE_BLOCKER),
        false,
        false,
        false,
    };
}

[[nodiscard]] query::ModulePartKey generated_module_part_key(
    const query::ModulePartKey source_part, const syntax::ModuleId module, const base::u32 part_index)
{
    const std::string name = module_part_generated_name(module, part_index);
    const std::string virtual_buffer = module_part_generated_virtual_buffer(module, part_index);
    const query::FileKey generated_file = query::file_key(
        source_part.module.package, name, query::SourceRole::generated, virtual_buffer);
    return query::module_part_key(source_part.module, generated_file, query::ModulePartKind::generated, name,
        part_index);
}

[[nodiscard]] query::StableFingerprint128 fingerprint_attribute_token_tree(
    const syntax::AttributeDecl& attribute) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21D_TOKEN_TREE_FINGERPRINT_MARKER);
    builder.mix_string(attribute.name);
    builder.mix_bool(attribute.has_token_tree);
    builder.mix_u64(static_cast<base::u64>(attribute.token_tree.size()));
    for (const syntax::AttributeTokenDecl& token : attribute.token_tree) {
        builder.mix_u8(static_cast<base::u8>(token.kind));
        builder.mix_string(token.text);
        builder.mix_u64(static_cast<base::u64>(token.range.source.value));
        builder.mix_u64(static_cast<base::u64>(token.range.begin));
        builder.mix_u64(static_cast<base::u64>(token.range.end));
        builder.mix_u32(token.depth);
        builder.mix_u8(static_cast<base::u8>(token.group));
    }
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 fingerprint_early_item_query_key(
    const syntax::ItemId item,
    const syntax::ModuleId module,
    const base::u32 part_index,
    const base::u32 attribute_index,
    const syntax::AttributeDecl& attribute,
    const query::ModulePartKey attached_part,
    const query::StableFingerprint128 token_tree_fingerprint) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21D_QUERY_KEY_FINGERPRINT_MARKER);
    builder.mix_u32(item.value);
    builder.mix_u32(module.value);
    builder.mix_u32(part_index);
    builder.mix_u32(attribute_index);
    builder.mix_string(attribute.name);
    builder.mix_fingerprint(query::stable_key_fingerprint(attached_part));
    builder.mix_fingerprint(token_tree_fingerprint);
    return builder.finish();
}

[[nodiscard]] EarlyItemExpansionDisposition disposition_for_attribute(
    const syntax::AttributeDecl& attribute) noexcept
{
    return attribute.name == "derive" ? EarlyItemExpansionDisposition::builtin_derive_passthrough
                                      : EarlyItemExpansionDisposition::blocked_unimplemented_attribute;
}

void mix_input(query::StableHashBuilder& builder, const EarlyItemMacroInput& input) noexcept
{
    builder.mix_u32(input.item.value);
    builder.mix_u32(input.module.value);
    builder.mix_u32(input.part_index);
    builder.mix_u32(input.attribute_index);
    builder.mix_string(input.attribute_name);
    builder.mix_u64(static_cast<base::u64>(input.attribute_range.source.value));
    builder.mix_u64(static_cast<base::u64>(input.attribute_range.begin));
    builder.mix_u64(static_cast<base::u64>(input.attribute_range.end));
    builder.mix_u64(static_cast<base::u64>(input.token_tree_range.source.value));
    builder.mix_u64(static_cast<base::u64>(input.token_tree_range.begin));
    builder.mix_u64(static_cast<base::u64>(input.token_tree_range.end));
    builder.mix_bool(input.has_token_tree);
    builder.mix_u64(input.token_count);
    builder.mix_fingerprint(query::stable_key_fingerprint(input.attached_part));
    builder.mix_fingerprint(input.token_tree_fingerprint);
    builder.mix_fingerprint(input.query_key_fingerprint);
    builder.mix_u8(static_cast<base::u8>(input.disposition));
}

void mix_generated_part(query::StableHashBuilder& builder, const GeneratedModulePartPlaceholder& part) noexcept
{
    builder.mix_u32(part.module.value);
    builder.mix_u32(part.source_part_index);
    builder.mix_u32(part.generated_stable_index);
    builder.mix_u8(static_cast<base::u8>(part.source_role));
    builder.mix_u8(static_cast<base::u8>(part.part_kind));
    builder.mix_fingerprint(query::stable_key_fingerprint(part.source_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(part.generated_part));
    builder.mix_fingerprint(part.output_fingerprint);
    builder.mix_bool(part.parsed);
    builder.mix_bool(part.merged);
    builder.mix_bool(part.produced_user_generated_code);
}

void mix_parse_merge_stub(query::StableHashBuilder& builder, const GeneratedModulePartParseMergeStub& stub) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M21E_PARSE_MERGE_STUB_MARKER);
    builder.mix_u32(stub.module.value);
    builder.mix_u32(stub.source_part_index);
    builder.mix_u32(stub.generated_stable_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(stub.source_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(stub.generated_part));
    builder.mix_fingerprint(stub.generated_buffer_identity);
    builder.mix_fingerprint(stub.parse_config_fingerprint);
    builder.mix_fingerprint(stub.merge_ordering_key);
    builder.mix_fingerprint(stub.expansion_origin);
    builder.mix_string(stub.generated_buffer_name);
    builder.mix_string(stub.blocker_reason);
    builder.mix_u8(static_cast<base::u8>(stub.lifecycle_state));
    builder.mix_bool(stub.materialized_buffer);
    builder.mix_bool(stub.parsed);
    builder.mix_bool(stub.merged);
    builder.mix_bool(stub.sema_visible);
    builder.mix_bool(stub.produced_user_generated_code);
}

void mix_source_map(query::StableHashBuilder& builder, const ExpansionSourceMapPlaceholder& source_map) noexcept
{
    builder.mix_u32(source_map.item.value);
    builder.mix_u32(source_map.module.value);
    builder.mix_u32(source_map.attribute_index);
    builder.mix_u64(static_cast<base::u64>(source_map.attribute_range.source.value));
    builder.mix_u64(static_cast<base::u64>(source_map.attribute_range.begin));
    builder.mix_u64(static_cast<base::u64>(source_map.attribute_range.end));
    builder.mix_u64(static_cast<base::u64>(source_map.token_tree_range.source.value));
    builder.mix_u64(static_cast<base::u64>(source_map.token_tree_range.begin));
    builder.mix_u64(static_cast<base::u64>(source_map.token_tree_range.end));
    builder.mix_fingerprint(source_map.expansion_origin);
    builder.mix_bool(source_map.real_source_map);
    builder.mix_bool(source_map.debug_trace_available);
}

void mix_hygiene_stub(query::StableHashBuilder& builder, const ExpansionHygieneStub& stub) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M21F_HYGIENE_STUB_MARKER);
    builder.mix_u32(stub.item.value);
    builder.mix_u32(stub.module.value);
    builder.mix_u32(stub.part_index);
    builder.mix_u32(stub.attribute_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(stub.attached_part));
    builder.mix_fingerprint(stub.expansion_origin);
    builder.mix_fingerprint(stub.call_site_mark);
    builder.mix_fingerprint(stub.definition_site_mark);
    builder.mix_fingerprint(stub.generated_fresh_mark);
    builder.mix_fingerprint(stub.declared_name_set);
    builder.mix_string(stub.policy);
    builder.mix_bool(stub.resolved);
    builder.mix_bool(stub.declared_names_visible);
    builder.mix_bool(stub.captures_call_site_locals);
}

void mix_trace_stub(query::StableHashBuilder& builder, const ExpansionTraceStub& stub) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M21F_TRACE_STUB_MARKER);
    builder.mix_u32(stub.item.value);
    builder.mix_u32(stub.module.value);
    builder.mix_u32(stub.part_index);
    builder.mix_u32(stub.attribute_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(stub.attached_part));
    builder.mix_u64(static_cast<base::u64>(stub.attribute_range.source.value));
    builder.mix_u64(static_cast<base::u64>(stub.attribute_range.begin));
    builder.mix_u64(static_cast<base::u64>(stub.attribute_range.end));
    builder.mix_u64(static_cast<base::u64>(stub.token_tree_range.source.value));
    builder.mix_u64(static_cast<base::u64>(stub.token_tree_range.begin));
    builder.mix_u64(static_cast<base::u64>(stub.token_tree_range.end));
    builder.mix_fingerprint(stub.expansion_origin);
    builder.mix_fingerprint(stub.trace_identity);
    builder.mix_fingerprint(stub.generated_source_map_identity);
    builder.mix_fingerprint(stub.diagnostic_anchor);
    builder.mix_string(stub.trace_policy);
    builder.mix_string(stub.blocker_reason);
    builder.mix_bool(stub.real_source_map);
    builder.mix_bool(stub.debug_trace_available);
    builder.mix_bool(stub.cli_emit_expanded_available);
}

void mix_summary(query::StableHashBuilder& builder, const EarlyItemExpansionSummary& summary) noexcept
{
    builder.mix_u64(summary.macro_input_count);
    builder.mix_u64(summary.attribute_input_count);
    builder.mix_u64(summary.builtin_derive_passthrough_count);
    builder.mix_u64(summary.blocked_attribute_count);
    builder.mix_u64(summary.generated_part_placeholder_count);
    builder.mix_u64(summary.generated_part_stub_count);
    builder.mix_u64(summary.materialized_buffer_stub_count);
    builder.mix_u64(summary.parse_blocked_count);
    builder.mix_u64(summary.merge_blocked_count);
    builder.mix_u64(summary.sema_visible_generated_part_count);
    builder.mix_u64(summary.source_map_placeholder_count);
    builder.mix_u64(summary.hygiene_stub_count);
    builder.mix_u64(summary.unresolved_hygiene_stub_count);
    builder.mix_u64(summary.declared_name_stub_count);
    builder.mix_u64(summary.call_site_capture_count);
    builder.mix_u64(summary.trace_stub_count);
    builder.mix_u64(summary.real_source_map_count);
    builder.mix_u64(summary.debug_trace_available_count);
    builder.mix_u64(summary.cli_emit_expanded_available_count);
    builder.mix_u64(summary.parsed_generated_part_count);
    builder.mix_u64(summary.merged_generated_part_count);
    builder.mix_u64(summary.user_generated_code_count);
    builder.mix_u64(summary.standard_library_required_count);
    builder.mix_u64(summary.runtime_required_count);
    builder.mix_u64(summary.external_process_required_count);
}

[[nodiscard]] bool summary_equals(
    const EarlyItemExpansionSummary& lhs, const EarlyItemExpansionSummary& rhs) noexcept
{
    return lhs.macro_input_count == rhs.macro_input_count
        && lhs.attribute_input_count == rhs.attribute_input_count
        && lhs.builtin_derive_passthrough_count == rhs.builtin_derive_passthrough_count
        && lhs.blocked_attribute_count == rhs.blocked_attribute_count
        && lhs.generated_part_placeholder_count == rhs.generated_part_placeholder_count
        && lhs.generated_part_stub_count == rhs.generated_part_stub_count
        && lhs.materialized_buffer_stub_count == rhs.materialized_buffer_stub_count
        && lhs.parse_blocked_count == rhs.parse_blocked_count
        && lhs.merge_blocked_count == rhs.merge_blocked_count
        && lhs.sema_visible_generated_part_count == rhs.sema_visible_generated_part_count
        && lhs.source_map_placeholder_count == rhs.source_map_placeholder_count
        && lhs.hygiene_stub_count == rhs.hygiene_stub_count
        && lhs.unresolved_hygiene_stub_count == rhs.unresolved_hygiene_stub_count
        && lhs.declared_name_stub_count == rhs.declared_name_stub_count
        && lhs.call_site_capture_count == rhs.call_site_capture_count
        && lhs.trace_stub_count == rhs.trace_stub_count
        && lhs.real_source_map_count == rhs.real_source_map_count
        && lhs.debug_trace_available_count == rhs.debug_trace_available_count
        && lhs.cli_emit_expanded_available_count == rhs.cli_emit_expanded_available_count
        && lhs.parsed_generated_part_count == rhs.parsed_generated_part_count
        && lhs.merged_generated_part_count == rhs.merged_generated_part_count
        && lhs.user_generated_code_count == rhs.user_generated_code_count
        && lhs.standard_library_required_count == rhs.standard_library_required_count
        && lhs.runtime_required_count == rhs.runtime_required_count
        && lhs.external_process_required_count == rhs.external_process_required_count;
}

[[nodiscard]] bool generated_part_exists_for(
    const std::vector<GeneratedModulePartPlaceholder>& generated_parts,
    const syntax::ModuleId module,
    const base::u32 source_part_index) noexcept
{
    return std::any_of(generated_parts.begin(), generated_parts.end(),
        [module, source_part_index](const GeneratedModulePartPlaceholder& part) {
            return part.module.value == module.value && part.source_part_index == source_part_index;
        });
}

[[nodiscard]] bool stub_matches_placeholder(
    const GeneratedModulePartParseMergeStub& stub,
    const GeneratedModulePartPlaceholder& placeholder) noexcept
{
    const std::string expected_buffer_name =
        module_part_generated_virtual_buffer(placeholder.module, placeholder.generated_stable_index);
    return stub.module.value == placeholder.module.value
        && stub.source_part_index == placeholder.source_part_index
        && stub.generated_stable_index == placeholder.generated_stable_index
        && stub.source_part == placeholder.source_part
        && stub.generated_part == placeholder.generated_part
        && stub.generated_buffer_name == expected_buffer_name
        && stub.blocker_reason == FRONTEND_MACRO_M21E_PARSE_MERGE_BLOCKER
        && stub.generated_buffer_identity == generated_buffer_identity(placeholder, expected_buffer_name)
        && stub.parse_config_fingerprint == parse_config_fingerprint(placeholder)
        && stub.merge_ordering_key == merge_ordering_key(placeholder)
        && stub.expansion_origin == placeholder.output_fingerprint;
}

[[nodiscard]] bool source_map_matches_input(
    const ExpansionSourceMapPlaceholder& source_map, const EarlyItemMacroInput& input) noexcept
{
    return source_map.item.value == input.item.value
        && source_map.module.value == input.module.value
        && source_map.attribute_index == input.attribute_index
        && source_ranges_equal(source_map.attribute_range, input.attribute_range)
        && source_ranges_equal(source_map.token_tree_range, input.token_tree_range)
        && source_map.expansion_origin == input.query_key_fingerprint;
}

[[nodiscard]] bool hygiene_stub_matches_input(
    const ExpansionHygieneStub& stub, const EarlyItemMacroInput& input) noexcept
{
    return stub.item.value == input.item.value
        && stub.module.value == input.module.value
        && stub.part_index == input.part_index
        && stub.attribute_index == input.attribute_index
        && stub.attached_part == input.attached_part
        && stub.expansion_origin == input.query_key_fingerprint
        && stub.call_site_mark == hygiene_mark_fingerprint(FRONTEND_MACRO_M21F_CALL_SITE_MARK_MARKER, input)
        && stub.definition_site_mark == hygiene_mark_fingerprint(
               FRONTEND_MACRO_M21F_DEFINITION_SITE_MARK_MARKER, input)
        && stub.generated_fresh_mark == hygiene_mark_fingerprint(
               FRONTEND_MACRO_M21F_GENERATED_FRESH_MARK_MARKER, input)
        && stub.declared_name_set == hygiene_mark_fingerprint(
               FRONTEND_MACRO_M21F_DECLARED_NAME_SET_MARKER, input)
        && stub.policy == FRONTEND_MACRO_M21F_HYGIENE_POLICY;
}

[[nodiscard]] bool trace_stub_matches_input(
    const ExpansionTraceStub& stub, const EarlyItemMacroInput& input) noexcept
{
    return stub.item.value == input.item.value
        && stub.module.value == input.module.value
        && stub.part_index == input.part_index
        && stub.attribute_index == input.attribute_index
        && stub.attached_part == input.attached_part
        && source_ranges_equal(stub.attribute_range, input.attribute_range)
        && source_ranges_equal(stub.token_tree_range, input.token_tree_range)
        && stub.expansion_origin == input.query_key_fingerprint
        && stub.trace_identity == trace_stub_fingerprint(FRONTEND_MACRO_M21F_TRACE_IDENTITY_MARKER, input)
        && stub.generated_source_map_identity == trace_stub_fingerprint(
               FRONTEND_MACRO_M21F_GENERATED_SOURCE_MAP_MARKER, input)
        && stub.diagnostic_anchor == trace_stub_fingerprint(
               FRONTEND_MACRO_M21F_DIAGNOSTIC_ANCHOR_MARKER, input)
        && stub.trace_policy == FRONTEND_MACRO_M21F_TRACE_POLICY
        && stub.blocker_reason == FRONTEND_MACRO_M21F_TRACE_BLOCKER;
}

[[nodiscard]] bool generated_part_stubs_match_placeholders(
    const std::vector<GeneratedModulePartPlaceholder>& generated_parts,
    const std::vector<GeneratedModulePartParseMergeStub>& generated_part_stubs) noexcept
{
    if (generated_parts.size() != generated_part_stubs.size()) {
        return false;
    }
    for (base::usize index = 0; index < generated_parts.size(); ++index) {
        if (!stub_matches_placeholder(generated_part_stubs[index], generated_parts[index])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool per_input_stubs_match_inputs(const EarlyItemExpansionResult& result) noexcept
{
    if (result.inputs.size() != result.source_maps.size()
        || result.inputs.size() != result.hygiene_stubs.size()
        || result.inputs.size() != result.trace_stubs.size()) {
        return false;
    }
    for (base::usize index = 0; index < result.inputs.size(); ++index) {
        const EarlyItemMacroInput& input = result.inputs[index];
        if (!source_map_matches_input(result.source_maps[index], input)
            || !hygiene_stub_matches_input(result.hygiene_stubs[index], input)
            || !trace_stub_matches_input(result.trace_stubs[index], input)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] GeneratedModulePartPlaceholder make_generated_part_placeholder(
    const query::ModulePartKey source_part, const syntax::ModuleId module, const base::u32 source_part_index)
{
    const base::u32 generated_stable_index = base::checked_u32(
        base::checked_add_usize(source_part_index, FRONTEND_MACRO_M21D_GENERATED_PART_INDEX_OFFSET,
            "early item macro generated part stable index"),
        "early item macro generated part stable index");
    const query::ModulePartKey generated =
        generated_module_part_key(source_part, module, generated_stable_index);
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21D_GENERATED_PART_PLACEHOLDER_MARKER);
    builder.mix_fingerprint(query::stable_key_fingerprint(source_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(generated));
    builder.mix_u32(module.value);
    builder.mix_u32(source_part_index);
    return GeneratedModulePartPlaceholder{
        module,
        source_part_index,
        generated_stable_index,
        query::SourceRole::generated,
        query::ModulePartKind::generated,
        source_part,
        generated,
        builder.finish(),
        false,
        false,
        false,
    };
}

[[nodiscard]] ExpansionSourceMapPlaceholder make_source_map_placeholder(
    const EarlyItemMacroInput& input) noexcept
{
    return ExpansionSourceMapPlaceholder{
        input.item,
        input.module,
        input.attribute_index,
        input.attribute_range,
        input.token_tree_range,
        input.query_key_fingerprint,
        false,
        false,
    };
}

[[nodiscard]] base::Result<query::ModulePartKey> module_part_key_for_item(const syntax::AstModule& ast,
    const std::span<const std::vector<query::ModulePartKey>> module_part_keys, const syntax::ItemId item)
{
    if (!item_id_in_range(ast, item)) {
        return base::Result<query::ModulePartKey>::fail(internal_error(FRONTEND_MACRO_M21D_ITEM_MODULES_MISMATCH));
    }
    const syntax::ModuleId module = ast.item_modules[item.value];
    const base::u32 part_index = ast.item_part_indices[item.value];
    if (!module_id_in_range(ast, module) || module.value >= module_part_keys.size()
        || part_index >= module_part_keys[module.value].size()
        || !query::is_valid(module_part_keys[module.value][part_index])) {
        return base::Result<query::ModulePartKey>::fail(internal_error(FRONTEND_MACRO_M21D_MISSING_MODULE_PART_KEY));
    }
    return base::Result<query::ModulePartKey>::ok(module_part_keys[module.value][part_index]);
}

[[nodiscard]] EarlyItemMacroInput make_macro_input(const syntax::AstModule& ast,
    const syntax::ItemId item,
    const base::u32 attribute_index,
    const syntax::AttributeDecl& attribute,
    const query::ModulePartKey attached_part) noexcept
{
    const query::StableFingerprint128 token_tree_fingerprint = fingerprint_attribute_token_tree(attribute);
    const syntax::ModuleId module = ast.item_modules[item.value];
    const base::u32 part_index = ast.item_part_indices[item.value];
    const query::StableFingerprint128 query_key_fingerprint = fingerprint_early_item_query_key(
        item, module, part_index, attribute_index, attribute, attached_part, token_tree_fingerprint);
    return EarlyItemMacroInput{
        item,
        module,
        part_index,
        attribute_index,
        std::string(attribute.name),
        attribute.range,
        attribute.token_tree_range,
        attribute.has_token_tree,
        static_cast<base::u64>(attribute.token_tree.size()),
        attached_part,
        token_tree_fingerprint,
        query_key_fingerprint,
        disposition_for_attribute(attribute),
    };
}

} // namespace

std::string_view early_item_expansion_disposition_name(
    const EarlyItemExpansionDisposition disposition) noexcept
{
    switch (disposition) {
        case EarlyItemExpansionDisposition::builtin_derive_passthrough:
            return "builtin_derive_passthrough";
        case EarlyItemExpansionDisposition::blocked_unimplemented_attribute:
            return "blocked_unimplemented_attribute";
    }
    return "invalid";
}

std::string_view generated_module_part_lifecycle_state_name(
    const GeneratedModulePartLifecycleState state) noexcept
{
    switch (state) {
        case GeneratedModulePartLifecycleState::planned:
            return "planned";
        case GeneratedModulePartLifecycleState::materialized_buffer_stub:
            return "materialized_buffer_stub";
        case GeneratedModulePartLifecycleState::parse_blocked:
            return "parse_blocked";
        case GeneratedModulePartLifecycleState::merge_blocked:
            return "merge_blocked";
    }
    return "invalid";
}

bool is_valid(const EarlyItemExpansionDisposition disposition) noexcept
{
    switch (disposition) {
        case EarlyItemExpansionDisposition::builtin_derive_passthrough:
        case EarlyItemExpansionDisposition::blocked_unimplemented_attribute:
            return true;
    }
    return false;
}

bool is_valid(const GeneratedModulePartLifecycleState state) noexcept
{
    switch (state) {
        case GeneratedModulePartLifecycleState::planned:
        case GeneratedModulePartLifecycleState::materialized_buffer_stub:
        case GeneratedModulePartLifecycleState::parse_blocked:
        case GeneratedModulePartLifecycleState::merge_blocked:
            return true;
    }
    return false;
}

bool is_valid(const EarlyItemMacroInput& input) noexcept
{
    return syntax::is_valid(input.item)
        && syntax::is_valid(input.module)
        && !input.attribute_name.empty()
        && source_range_is_well_formed(input.attribute_range)
        && source_range_is_well_formed(input.token_tree_range)
        && query::is_valid(input.attached_part)
        && input.token_tree_fingerprint.byte_count > 0
        && input.query_key_fingerprint.byte_count > 0
        && is_valid(input.disposition);
}

bool is_valid(const GeneratedModulePartPlaceholder& placeholder) noexcept
{
    return syntax::is_valid(placeholder.module)
        && placeholder.source_role == query::SourceRole::generated
        && placeholder.part_kind == query::ModulePartKind::generated
        && query::is_valid(placeholder.source_part)
        && query::is_valid(placeholder.generated_part)
        && placeholder.generated_part.kind == query::ModulePartKind::generated
        && placeholder.generated_part.file.role == query::SourceRole::generated
        && placeholder.output_fingerprint.byte_count > 0
        && !placeholder.parsed
        && !placeholder.merged
        && !placeholder.produced_user_generated_code;
}

bool is_valid(const GeneratedModulePartParseMergeStub& stub) noexcept
{
    return syntax::is_valid(stub.module)
        && query::is_valid(stub.source_part)
        && query::is_valid(stub.generated_part)
        && stub.generated_part.kind == query::ModulePartKind::generated
        && stub.generated_part.file.role == query::SourceRole::generated
        && is_nonzero_fingerprint(stub.generated_buffer_identity)
        && is_nonzero_fingerprint(stub.parse_config_fingerprint)
        && is_nonzero_fingerprint(stub.merge_ordering_key)
        && is_nonzero_fingerprint(stub.expansion_origin)
        && !stub.generated_buffer_name.empty()
        && !stub.blocker_reason.empty()
        && is_valid(stub.lifecycle_state)
        && stub.lifecycle_state == GeneratedModulePartLifecycleState::merge_blocked
        && stub.materialized_buffer
        && !stub.parsed
        && !stub.merged
        && !stub.sema_visible
        && !stub.produced_user_generated_code;
}

bool is_valid(const ExpansionSourceMapPlaceholder& placeholder) noexcept
{
    return syntax::is_valid(placeholder.item)
        && syntax::is_valid(placeholder.module)
        && source_range_is_well_formed(placeholder.attribute_range)
        && source_range_is_well_formed(placeholder.token_tree_range)
        && placeholder.expansion_origin.byte_count > 0
        && !placeholder.real_source_map
        && !placeholder.debug_trace_available;
}

bool is_valid(const ExpansionHygieneStub& stub) noexcept
{
    return syntax::is_valid(stub.item)
        && syntax::is_valid(stub.module)
        && query::is_valid(stub.attached_part)
        && is_nonzero_fingerprint(stub.expansion_origin)
        && is_nonzero_fingerprint(stub.call_site_mark)
        && is_nonzero_fingerprint(stub.definition_site_mark)
        && is_nonzero_fingerprint(stub.generated_fresh_mark)
        && is_nonzero_fingerprint(stub.declared_name_set)
        && stub.policy == FRONTEND_MACRO_M21F_HYGIENE_POLICY
        && !stub.resolved
        && !stub.declared_names_visible
        && !stub.captures_call_site_locals;
}

bool is_valid(const ExpansionTraceStub& stub) noexcept
{
    return syntax::is_valid(stub.item)
        && syntax::is_valid(stub.module)
        && query::is_valid(stub.attached_part)
        && source_range_is_well_formed(stub.attribute_range)
        && source_range_is_well_formed(stub.token_tree_range)
        && is_nonzero_fingerprint(stub.expansion_origin)
        && is_nonzero_fingerprint(stub.trace_identity)
        && is_nonzero_fingerprint(stub.generated_source_map_identity)
        && is_nonzero_fingerprint(stub.diagnostic_anchor)
        && stub.trace_policy == FRONTEND_MACRO_M21F_TRACE_POLICY
        && stub.blocker_reason == FRONTEND_MACRO_M21F_TRACE_BLOCKER
        && !stub.real_source_map
        && !stub.debug_trace_available
        && !stub.cli_emit_expanded_available;
}

bool is_valid(const EarlyItemExpansionSummary& summary, const EarlyItemExpansionResult& result) noexcept
{
    return summary_equals(summary, summarize_early_item_expansion_counts(result));
}

bool is_valid(const EarlyItemExpansionResult& result) noexcept
{
    return std::string_view(result.name) == FRONTEND_MACRO_M21F_EXPANSION_NAME
        && query::is_valid_m21c_macro_expansion_plan(result.plan)
        && std::all_of(result.inputs.begin(), result.inputs.end(), [](const EarlyItemMacroInput& input) {
               return is_valid(input);
           })
        && std::all_of(result.generated_parts.begin(), result.generated_parts.end(),
               [](const GeneratedModulePartPlaceholder& placeholder) {
                   return is_valid(placeholder);
               })
        && std::all_of(result.generated_part_stubs.begin(), result.generated_part_stubs.end(),
               [](const GeneratedModulePartParseMergeStub& stub) {
                   return is_valid(stub);
               })
        && generated_part_stubs_match_placeholders(result.generated_parts, result.generated_part_stubs)
        && std::all_of(result.source_maps.begin(), result.source_maps.end(),
               [](const ExpansionSourceMapPlaceholder& placeholder) {
                   return is_valid(placeholder);
               })
        && std::all_of(result.hygiene_stubs.begin(), result.hygiene_stubs.end(),
               [](const ExpansionHygieneStub& stub) {
                   return is_valid(stub);
               })
        && std::all_of(result.trace_stubs.begin(), result.trace_stubs.end(),
               [](const ExpansionTraceStub& stub) {
                   return is_valid(stub);
               })
        && per_input_stubs_match_inputs(result)
        && is_valid(result.summary, result)
        && result.fingerprint == early_item_expansion_fingerprint(result);
}

EarlyItemExpansionSummary summarize_early_item_expansion_counts(
    const EarlyItemExpansionResult& result) noexcept
{
    EarlyItemExpansionSummary summary;
    summary.macro_input_count = static_cast<base::u64>(result.inputs.size());
    summary.attribute_input_count = static_cast<base::u64>(result.inputs.size());
    for (const EarlyItemMacroInput& input : result.inputs) {
        switch (input.disposition) {
            case EarlyItemExpansionDisposition::builtin_derive_passthrough:
                ++summary.builtin_derive_passthrough_count;
                break;
            case EarlyItemExpansionDisposition::blocked_unimplemented_attribute:
                ++summary.blocked_attribute_count;
                break;
        }
    }
    summary.generated_part_placeholder_count = static_cast<base::u64>(result.generated_parts.size());
    for (const GeneratedModulePartPlaceholder& part : result.generated_parts) {
        if (part.parsed) {
            ++summary.parsed_generated_part_count;
        }
        if (part.merged) {
            ++summary.merged_generated_part_count;
        }
        if (part.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.generated_part_stub_count = static_cast<base::u64>(result.generated_part_stubs.size());
    for (const GeneratedModulePartParseMergeStub& stub : result.generated_part_stubs) {
        if (stub.materialized_buffer) {
            ++summary.materialized_buffer_stub_count;
        }
        if (stub.lifecycle_state == GeneratedModulePartLifecycleState::parse_blocked
            || stub.lifecycle_state == GeneratedModulePartLifecycleState::merge_blocked) {
            ++summary.parse_blocked_count;
        }
        if (stub.lifecycle_state == GeneratedModulePartLifecycleState::merge_blocked) {
            ++summary.merge_blocked_count;
        }
        if (stub.sema_visible) {
            ++summary.sema_visible_generated_part_count;
        }
        if (stub.parsed) {
            ++summary.parsed_generated_part_count;
        }
        if (stub.merged) {
            ++summary.merged_generated_part_count;
        }
        if (stub.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.source_map_placeholder_count = static_cast<base::u64>(result.source_maps.size());
    summary.hygiene_stub_count = static_cast<base::u64>(result.hygiene_stubs.size());
    for (const ExpansionHygieneStub& stub : result.hygiene_stubs) {
        if (!stub.resolved) {
            ++summary.unresolved_hygiene_stub_count;
        }
        if (is_nonzero_fingerprint(stub.declared_name_set)) {
            ++summary.declared_name_stub_count;
        }
        if (stub.captures_call_site_locals) {
            ++summary.call_site_capture_count;
        }
    }
    summary.trace_stub_count = static_cast<base::u64>(result.trace_stubs.size());
    for (const ExpansionTraceStub& stub : result.trace_stubs) {
        if (stub.real_source_map) {
            ++summary.real_source_map_count;
        }
        if (stub.debug_trace_available) {
            ++summary.debug_trace_available_count;
        }
        if (stub.cli_emit_expanded_available) {
            ++summary.cli_emit_expanded_available_count;
        }
    }
    return summary;
}

query::StableFingerprint128 early_item_expansion_fingerprint(
    const EarlyItemExpansionResult& result) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21F_EXPANSION_FINGERPRINT_MARKER);
    builder.mix_string(result.name);
    builder.mix_fingerprint(query::macro_expansion_plan_fingerprint(result.plan));
    builder.mix_u64(static_cast<base::u64>(result.inputs.size()));
    for (const EarlyItemMacroInput& input : result.inputs) {
        mix_input(builder, input);
    }
    builder.mix_u64(static_cast<base::u64>(result.generated_parts.size()));
    for (const GeneratedModulePartPlaceholder& part : result.generated_parts) {
        mix_generated_part(builder, part);
    }
    builder.mix_u64(static_cast<base::u64>(result.generated_part_stubs.size()));
    for (const GeneratedModulePartParseMergeStub& stub : result.generated_part_stubs) {
        mix_parse_merge_stub(builder, stub);
    }
    builder.mix_u64(static_cast<base::u64>(result.source_maps.size()));
    for (const ExpansionSourceMapPlaceholder& source_map : result.source_maps) {
        mix_source_map(builder, source_map);
    }
    builder.mix_u64(static_cast<base::u64>(result.hygiene_stubs.size()));
    for (const ExpansionHygieneStub& stub : result.hygiene_stubs) {
        mix_hygiene_stub(builder, stub);
    }
    builder.mix_u64(static_cast<base::u64>(result.trace_stubs.size()));
    for (const ExpansionTraceStub& stub : result.trace_stubs) {
        mix_trace_stub(builder, stub);
    }
    mix_summary(builder, summarize_early_item_expansion_counts(result));
    return builder.finish();
}

std::string summarize_early_item_expansion(const EarlyItemExpansionResult& result)
{
    const EarlyItemExpansionSummary summary = summarize_early_item_expansion_counts(result);
    std::ostringstream stream;
    stream << "early_item_expansion name="
           << (result.name.empty() ? "<anonymous>" : result.name)
           << " attributes=" << summary.attribute_input_count
           << " builtin_derive_passthrough=" << summary.builtin_derive_passthrough_count
           << " blocked_attributes=" << summary.blocked_attribute_count
           << " generated_part_placeholders=" << summary.generated_part_placeholder_count
           << " generated_part_stubs=" << summary.generated_part_stub_count
           << " materialized_buffer_stubs=" << summary.materialized_buffer_stub_count
           << " parse_blocked=" << summary.parse_blocked_count
           << " merge_blocked=" << summary.merge_blocked_count
           << " sema_visible_generated_parts=" << summary.sema_visible_generated_part_count
           << " source_map_placeholders=" << summary.source_map_placeholder_count
           << " hygiene_stubs=" << summary.hygiene_stub_count
           << " unresolved_hygiene_stubs=" << summary.unresolved_hygiene_stub_count
           << " declared_name_stubs=" << summary.declared_name_stub_count
           << " trace_stubs=" << summary.trace_stub_count
           << " real_source_maps=" << summary.real_source_map_count
           << " debug_traces=" << summary.debug_trace_available_count
           << " cli_emit_expanded=" << summary.cli_emit_expanded_available_count
           << " user_generated_code=" << summary.user_generated_code_count
           << " standard_library_required=" << summary.standard_library_required_count
           << " runtime_required=" << summary.runtime_required_count
           << " external_process_required=" << summary.external_process_required_count
           << " fingerprint=" << query::debug_string(early_item_expansion_fingerprint(result));
    return stream.str();
}

std::string dump_early_item_expansion(const EarlyItemExpansionResult& result)
{
    std::ostringstream stream;
    stream << summarize_early_item_expansion(result) << '\n';
    for (base::usize index = 0; index < result.inputs.size(); ++index) {
        const EarlyItemMacroInput& input = result.inputs[index];
        stream << "  input #" << index
               << " item=" << input.item.value
               << " module=" << input.module.value
               << " part=" << input.part_index
               << " attr=" << input.attribute_name
               << " disposition=" << early_item_expansion_disposition_name(input.disposition)
               << " token_count=" << input.token_count
               << " query_key=" << query::debug_string(input.query_key_fingerprint) << '\n';
    }
    for (base::usize index = 0; index < result.generated_parts.size(); ++index) {
        const GeneratedModulePartPlaceholder& part = result.generated_parts[index];
        stream << "  generated_part #" << index
               << " module=" << part.module.value
               << " source_part=" << part.source_part_index
               << " stable_index=" << part.generated_stable_index
               << " source_role=generated"
               << " kind=generated"
               << " parsed=" << (part.parsed ? "yes" : "no")
               << " merged=" << (part.merged ? "yes" : "no")
               << " user_generated_code=" << (part.produced_user_generated_code ? "yes" : "no") << '\n';
    }
    for (base::usize index = 0; index < result.generated_part_stubs.size(); ++index) {
        const GeneratedModulePartParseMergeStub& stub = result.generated_part_stubs[index];
        stream << "  parse_merge_stub #" << index
               << " module=" << stub.module.value
               << " source_part=" << stub.source_part_index
               << " stable_index=" << stub.generated_stable_index
               << " buffer=" << stub.generated_buffer_name
               << " lifecycle=" << generated_module_part_lifecycle_state_name(stub.lifecycle_state)
               << " materialized_buffer=" << (stub.materialized_buffer ? "yes" : "no")
               << " parsed=" << (stub.parsed ? "yes" : "no")
               << " merged=" << (stub.merged ? "yes" : "no")
               << " sema_visible=" << (stub.sema_visible ? "yes" : "no")
               << " blocker=" << stub.blocker_reason
               << " buffer_identity=" << query::debug_string(stub.generated_buffer_identity)
               << " parse_config=" << query::debug_string(stub.parse_config_fingerprint)
               << " merge_ordering=" << query::debug_string(stub.merge_ordering_key) << '\n';
    }
    for (base::usize index = 0; index < result.source_maps.size(); ++index) {
        const ExpansionSourceMapPlaceholder& source_map = result.source_maps[index];
        stream << "  source_map #" << index
               << " item=" << source_map.item.value
               << " module=" << source_map.module.value
               << " attribute_index=" << source_map.attribute_index
               << " real_source_map=" << (source_map.real_source_map ? "yes" : "no")
               << " debug_trace=" << (source_map.debug_trace_available ? "yes" : "no") << '\n';
    }
    for (base::usize index = 0; index < result.hygiene_stubs.size(); ++index) {
        const ExpansionHygieneStub& stub = result.hygiene_stubs[index];
        stream << "  hygiene_stub #" << index
               << " item=" << stub.item.value
               << " module=" << stub.module.value
               << " part=" << stub.part_index
               << " attribute_index=" << stub.attribute_index
               << " policy=" << stub.policy
               << " resolved=" << (stub.resolved ? "yes" : "no")
               << " declared_names_visible=" << (stub.declared_names_visible ? "yes" : "no")
               << " captures_call_site_locals=" << (stub.captures_call_site_locals ? "yes" : "no")
               << " origin=" << query::debug_string(stub.expansion_origin)
               << " call_site_mark=" << query::debug_string(stub.call_site_mark)
               << " definition_site_mark=" << query::debug_string(stub.definition_site_mark)
               << " generated_fresh_mark=" << query::debug_string(stub.generated_fresh_mark)
               << " declared_name_set=" << query::debug_string(stub.declared_name_set) << '\n';
    }
    for (base::usize index = 0; index < result.trace_stubs.size(); ++index) {
        const ExpansionTraceStub& stub = result.trace_stubs[index];
        stream << "  trace_stub #" << index
               << " item=" << stub.item.value
               << " module=" << stub.module.value
               << " part=" << stub.part_index
               << " attribute_index=" << stub.attribute_index
               << " policy=" << stub.trace_policy
               << " real_source_map=" << (stub.real_source_map ? "yes" : "no")
               << " debug_trace=" << (stub.debug_trace_available ? "yes" : "no")
               << " cli_emit_expanded=" << (stub.cli_emit_expanded_available ? "yes" : "no")
               << " blocker=" << stub.blocker_reason
               << " origin=" << query::debug_string(stub.expansion_origin)
               << " trace_identity=" << query::debug_string(stub.trace_identity)
               << " generated_source_map="
               << query::debug_string(stub.generated_source_map_identity)
               << " diagnostic_anchor=" << query::debug_string(stub.diagnostic_anchor) << '\n';
    }
    return stream.str();
}

base::Result<EarlyItemExpansionResult> expand_early_item_macros_noop(const syntax::AstModule& ast,
    const std::span<const std::vector<query::ModulePartKey>> module_part_keys,
    const query::MacroExpansionPlan& plan)
{
    if (!query::is_valid_m21c_macro_expansion_plan(plan)) {
        return base::Result<EarlyItemExpansionResult>::fail(internal_error(FRONTEND_MACRO_M21D_INVALID_PLAN));
    }
    if (ast.item_modules.size() != ast.items.size()) {
        return base::Result<EarlyItemExpansionResult>::fail(
            internal_error(FRONTEND_MACRO_M21D_ITEM_MODULES_MISMATCH));
    }
    if (ast.item_part_indices.size() != ast.items.size()) {
        return base::Result<EarlyItemExpansionResult>::fail(
            internal_error(FRONTEND_MACRO_M21D_ITEM_PARTS_MISMATCH));
    }

    EarlyItemExpansionResult result;
    result.name = std::string(FRONTEND_MACRO_M21F_EXPANSION_NAME);
    result.plan = plan;
    result.inputs.reserve(ast.items.size());
    result.generated_parts.reserve(ast.items.size());
    result.generated_part_stubs.reserve(ast.items.size());
    result.source_maps.reserve(ast.items.size());
    result.hygiene_stubs.reserve(ast.items.size());
    result.trace_stubs.reserve(ast.items.size());

    for (base::usize item_index = 0; item_index < ast.items.size(); ++item_index) {
        const syntax::ItemId item_id{base::checked_u32(item_index, syntax::SYNTAX_ITEM_NODE_ID_CONTEXT)};
        const syntax::ItemNode& item = ast.items[item_index];
        if (item.attributes.empty()) {
            continue;
        }
        auto attached_part_result = module_part_key_for_item(ast, module_part_keys, item_id);
        if (!attached_part_result) {
            return base::Result<EarlyItemExpansionResult>::fail(attached_part_result.error());
        }
        const query::ModulePartKey attached_part = attached_part_result.value();
        const syntax::ModuleId module = ast.item_modules[item_index];
        const base::u32 part_index = ast.item_part_indices[item_index];
        if (!generated_part_exists_for(result.generated_parts, module, part_index)) {
            GeneratedModulePartPlaceholder placeholder =
                make_generated_part_placeholder(attached_part, module, part_index);
            result.generated_part_stubs.push_back(make_parse_merge_stub(placeholder));
            result.generated_parts.push_back(std::move(placeholder));
        }

        for (base::usize attribute_index = 0; attribute_index < item.attributes.size(); ++attribute_index) {
            const syntax::AttributeDecl& attribute = item.attributes[attribute_index];
            EarlyItemMacroInput input = make_macro_input(ast, item_id,
                base::checked_u32(attribute_index, "early item macro attribute index"), attribute, attached_part);
            result.source_maps.push_back(make_source_map_placeholder(input));
            result.hygiene_stubs.push_back(make_hygiene_stub(input));
            result.trace_stubs.push_back(make_trace_stub(input));
            result.inputs.push_back(std::move(input));
        }
    }

    result.summary = summarize_early_item_expansion_counts(result);
    result.fingerprint = early_item_expansion_fingerprint(result);
    return base::Result<EarlyItemExpansionResult>::ok(std::move(result));
}

} // namespace aurex::frontend::macro
