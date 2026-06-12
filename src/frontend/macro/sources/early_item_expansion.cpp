#include <aurex/frontend/macro/early_item_expansion.hpp>
#include <aurex/infrastructure/base/integer.hpp>

#include <algorithm>
#include <limits>
#include <sstream>
#include <utility>

namespace aurex::frontend::macro {
namespace {

constexpr std::string_view FRONTEND_MACRO_M21L_EXPANSION_NAME =
    "M21l Parser Admission Diagnostic Report Projection";
constexpr std::string_view FRONTEND_MACRO_M21L_EXPANSION_FINGERPRINT_MARKER =
    "frontend.macro.m21l.parser_admission_diagnostic_report_projection.v1";
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
constexpr std::string_view FRONTEND_MACRO_M21G_GENERATED_ITEM_DECLARATION_MARKER =
    "frontend.macro.m21g.generated_item_declaration_stub.v1";
constexpr std::string_view FRONTEND_MACRO_M21G_DECLARED_NAME_STUB_MARKER =
    "frontend.macro.m21g.declared_generated_name_stub.v1";
constexpr std::string_view FRONTEND_MACRO_M21G_GENERATED_ITEM_KEY_MARKER =
    "frontend.macro.m21g.generated_item_key.v1";
constexpr std::string_view FRONTEND_MACRO_M21G_DECLARATION_IDENTITY_MARKER =
    "frontend.macro.m21g.declaration_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M21G_DECLARED_NAME_IDENTITY_MARKER =
    "frontend.macro.m21g.declared_name_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M21G_DECLARATION_ROLE =
    "attached_item_codegen_declared_names_v1";
constexpr std::string_view FRONTEND_MACRO_M21G_DECLARED_NAME_NAMESPACE = "item";
constexpr std::string_view FRONTEND_MACRO_M21G_DECLARATION_BLOCKER =
    "generated item declaration materialization is blocked in M21g";
constexpr std::string_view FRONTEND_MACRO_M21G_DECLARED_NAME_BLOCKER =
    "declared generated name lookup is blocked in M21g";
constexpr std::string_view FRONTEND_MACRO_M21G_GENERATED_NAME_PREFIX =
    "__aurex_macro_declared:";
constexpr std::string_view FRONTEND_MACRO_M21G_MISSING_GENERATED_PART =
    "early item macro expansion missing generated module part placeholder";
constexpr std::string_view FRONTEND_MACRO_M21H_ADMISSION_STUB_MARKER =
    "frontend.macro.m21h.token_materialization_admission_stub.v1";
constexpr std::string_view FRONTEND_MACRO_M21H_TOKEN_BUFFER_STUB_MARKER =
    "frontend.macro.m21h.generated_token_buffer_stub.v1";
constexpr std::string_view FRONTEND_MACRO_M21I_GENERATED_TOKEN_RECORD_MARKER =
    "frontend.macro.m21i.generated_token_record.v1";
constexpr std::string_view FRONTEND_MACRO_M21I_TOKEN_MATERIALIZATION_IDENTITY_MARKER =
    "frontend.macro.m21i.token_materialization_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M21J_PARSER_ADMISSION_GATE_MARKER =
    "frontend.macro.m21j.generated_token_parser_admission_gate_stub.v1";
constexpr std::string_view FRONTEND_MACRO_M21J_PARSE_GATE_IDENTITY_MARKER =
    "frontend.macro.m21j.parse_gate_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M21K_DIAGNOSTIC_PROJECTION_MARKER =
    "frontend.macro.m21k.parser_admission_diagnostic_projection_stub.v1";
constexpr std::string_view FRONTEND_MACRO_M21K_DIAGNOSTIC_IDENTITY_MARKER =
    "frontend.macro.m21k.parser_admission_diagnostic_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M21K_DIAGNOSTIC_ANCHOR_MARKER =
    "frontend.macro.m21k.parser_admission_diagnostic_anchor.v1";
constexpr std::string_view FRONTEND_MACRO_M21L_REPORT_ENTRY_MARKER =
    "frontend.macro.m21l.parser_admission_report_entry.v1";
constexpr std::string_view FRONTEND_MACRO_M21L_REPORT_ENTRY_IDENTITY_MARKER =
    "frontend.macro.m21l.parser_admission_report_entry_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M21L_REPORT_MARKER =
    "frontend.macro.m21l.parser_admission_report.v1";
constexpr std::string_view FRONTEND_MACRO_M21L_REPORT_IDENTITY_MARKER =
    "frontend.macro.m21l.parser_admission_report_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M21L_REPORT_ANCHOR_IDENTITY_MARKER =
    "frontend.macro.m21l.parser_admission_report_anchor_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M21L_REPORT_GROUPING_IDENTITY_MARKER =
    "frontend.macro.m21l.parser_admission_report_grouping_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M21H_TOKEN_PLAN_IDENTITY_MARKER =
    "frontend.macro.m21h.token_plan_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M21H_TOKEN_BUFFER_IDENTITY_MARKER =
    "frontend.macro.m21h.token_buffer_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M21H_ADMISSION_POLICY =
    "compiler_owned_attached_item_token_materialization_admission_v1";
constexpr std::string_view FRONTEND_MACRO_M21H_TOKEN_BUFFER_KIND =
    "compiler_owned_empty_token_stream";
constexpr std::string_view FRONTEND_MACRO_M21I_DERIVE_TOKEN_BUFFER_KIND =
    "compiler_owned_builtin_derive_token_stream_prototype";
constexpr std::string_view FRONTEND_MACRO_M21H_TOKEN_STREAM_PREFIX =
    "m21h-token-stream:";
constexpr std::string_view FRONTEND_MACRO_M21I_DERIVE_TOKEN_PRODUCER_POLICY =
    "compiler_owned_builtin_derive_token_producer_prototype_v1";
constexpr std::string_view FRONTEND_MACRO_M21I_EMPTY_TOKEN_PRODUCER_POLICY =
    "compiler_owned_blocked_empty_token_producer_v1";
constexpr std::string_view FRONTEND_MACRO_M21I_DERIVE_ADMISSION_BLOCKER =
    "compiler-owned derive token prototype remains parser-blocked in M21i";
constexpr std::string_view FRONTEND_MACRO_M21I_EMPTY_ADMISSION_BLOCKER =
    "non-derive item attribute token materialization remains blocked in M21i";
constexpr std::string_view FRONTEND_MACRO_M21I_DERIVE_TOKEN_BUFFER_BLOCKER =
    "compiler-owned generated token buffer remains parser-blocked in M21i";
constexpr std::string_view FRONTEND_MACRO_M21I_EMPTY_TOKEN_BUFFER_BLOCKER =
    "non-derive generated token buffer remains empty and parser-blocked in M21i";
constexpr std::string_view FRONTEND_MACRO_M21J_PARSER_GATE_POLICY =
    "compiler_owned_generated_token_parser_admission_gate_v1";
constexpr std::string_view FRONTEND_MACRO_M21J_DERIVE_PARSE_BLOCKER =
    "compiler-owned derive generated token buffer parser admission remains blocked in M21j";
constexpr std::string_view FRONTEND_MACRO_M21J_EMPTY_PARSE_BLOCKER =
    "empty or non-derive generated token buffer parser admission remains blocked in M21j";
constexpr std::string_view FRONTEND_MACRO_M21J_MISSING_PARSE_MERGE_STUB =
    "early item macro expansion missing generated module part parse merge stub";
constexpr std::string_view FRONTEND_MACRO_M21K_DIAGNOSTIC_POLICY =
    "parser_admission_blocked_diagnostic_projection_v1";
constexpr std::string_view FRONTEND_MACRO_M21K_DERIVE_BLOCKER_CATEGORY =
    "derive_token_buffer_parser_admission_blocked";
constexpr std::string_view FRONTEND_MACRO_M21K_EMPTY_BLOCKER_CATEGORY =
    "empty_token_buffer_parser_admission_blocked";
constexpr std::string_view FRONTEND_MACRO_M21K_GENERATED_PART_PARSE_BLOCKER =
    "generated module part parse remains blocked before parser admission diagnostics in M21k";
constexpr std::string_view FRONTEND_MACRO_M21K_DERIVE_USER_MESSAGE =
    "generated derive token buffer is compiler-owned but parser admission remains blocked in M21k";
constexpr std::string_view FRONTEND_MACRO_M21K_EMPTY_USER_MESSAGE =
    "generated token buffer is empty and parser admission remains blocked in M21k";
constexpr std::string_view FRONTEND_MACRO_M21K_DEBUG_PROJECTION_PREFIX =
    "m21k-parser-admission:";
constexpr std::string_view FRONTEND_MACRO_M21L_REPORT_POLICY =
    "parser_admission_blocked_report_query_projection_v1";
constexpr std::string_view FRONTEND_MACRO_M21L_QUERY_NAME_PREFIX =
    "m21l-parser-admission-report:";
constexpr std::string_view FRONTEND_MACRO_M21L_REPORT_BLOCKER =
    "parser admission diagnostic report remains parser-blocked in M21l";
constexpr std::string_view FRONTEND_MACRO_M21I_DERIVE_BEGIN_TOKEN_TEXT =
    "__aurex_builtin_derive_begin";
constexpr std::string_view FRONTEND_MACRO_M21I_DERIVE_END_TOKEN_TEXT =
    "__aurex_builtin_derive_end";
constexpr std::string_view FRONTEND_MACRO_M21I_DERIVE_BEGIN_TOKEN_ROLE = "derive_codegen_begin";
constexpr std::string_view FRONTEND_MACRO_M21I_DERIVE_SOURCE_TOKEN_ROLE = "derive_source_token_placeholder";
constexpr std::string_view FRONTEND_MACRO_M21I_DERIVE_END_TOKEN_ROLE = "derive_codegen_end";
constexpr base::u64 FRONTEND_MACRO_M21I_DERIVE_SENTINEL_TOKEN_COUNT = 2U;
constexpr std::string_view FRONTEND_MACRO_M21I_DERIVE_SOURCE_TOKEN_PREFIX =
    "__aurex_builtin_derive_source_token_";
constexpr std::string_view FRONTEND_MACRO_M21I_GENERATED_TOKEN_RESERVE_CONTEXT =
    "compiler-owned generated token reserve";
constexpr base::u64 FRONTEND_MACRO_M21I_MAX_GENERATED_TOKEN_INDEX =
    static_cast<base::u64>(std::numeric_limits<base::u32>::max());

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

[[nodiscard]] base::usize count_item_attributes(const syntax::AstModule& ast) noexcept
{
    base::usize count = 0;
    for (base::usize item_index = 0; item_index < ast.items.size(); ++item_index) {
        count += ast.items[item_index].attributes.size();
    }
    return count;
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

[[nodiscard]] std::string generated_item_name_for_input(const EarlyItemMacroInput& input)
{
    std::string name(FRONTEND_MACRO_M21G_GENERATED_NAME_PREFIX);
    name += std::to_string(input.module.value);
    name.push_back(':');
    name += std::to_string(input.part_index);
    name.push_back(':');
    name += std::to_string(input.item.value);
    name.push_back(':');
    name += std::to_string(input.attribute_index);
    name.push_back(':');
    name += input.attribute_name;
    return name;
}

[[nodiscard]] std::string token_stream_name_for_input(const EarlyItemMacroInput& input)
{
    std::string name(FRONTEND_MACRO_M21H_TOKEN_STREAM_PREFIX);
    name += std::to_string(input.module.value);
    name.push_back(':');
    name += std::to_string(input.part_index);
    name.push_back(':');
    name += std::to_string(input.item.value);
    name.push_back(':');
    name += std::to_string(input.attribute_index);
    name.push_back(':');
    name += input.attribute_name;
    return name;
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

[[nodiscard]] query::StableFingerprint128 generated_item_stub_fingerprint(
    const std::string_view marker,
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const query::StableFingerprint128 declared_name_set,
    const std::string_view generated_item_name) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(marker);
    mix_macro_input_identity(builder, input);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    builder.mix_fingerprint(placeholder.output_fingerprint);
    builder.mix_fingerprint(declared_name_set);
    builder.mix_string(generated_item_name);
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 token_plan_identity(
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const ExpansionHygieneStub& hygiene,
    const ExpansionTraceStub& trace,
    const GeneratedItemDeclarationStub& declaration,
    const DeclaredGeneratedNameStub& declared_name,
    const std::string_view token_stream_name) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21H_TOKEN_PLAN_IDENTITY_MARKER);
    mix_macro_input_identity(builder, input);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    builder.mix_fingerprint(placeholder.output_fingerprint);
    builder.mix_fingerprint(declaration.declaration_identity);
    builder.mix_fingerprint(declaration.generated_item_key);
    builder.mix_fingerprint(hygiene.declared_name_set);
    builder.mix_fingerprint(declared_name.declared_name_identity);
    builder.mix_fingerprint(declared_name.hygiene_mark);
    builder.mix_fingerprint(trace.trace_identity);
    builder.mix_fingerprint(trace.generated_source_map_identity);
    builder.mix_string(token_stream_name);
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 token_buffer_identity(
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const ExpansionHygieneStub& hygiene,
    const ExpansionTraceStub& trace,
    const query::StableFingerprint128 token_plan,
    const std::string_view token_buffer_kind,
    const std::string_view token_stream_name) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21H_TOKEN_BUFFER_IDENTITY_MARKER);
    mix_macro_input_identity(builder, input);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    builder.mix_fingerprint(placeholder.output_fingerprint);
    builder.mix_fingerprint(token_plan);
    builder.mix_fingerprint(hygiene.generated_fresh_mark);
    builder.mix_fingerprint(trace.generated_source_map_identity);
    builder.mix_string(token_buffer_kind);
    builder.mix_string(token_stream_name);
    return builder.finish();
}

[[nodiscard]] bool compiler_owned_token_prototype_enabled(const EarlyItemMacroInput& input) noexcept
{
    return input.disposition == EarlyItemExpansionDisposition::builtin_derive_passthrough;
}

[[nodiscard]] std::string_view token_buffer_kind_for_input(const EarlyItemMacroInput& input) noexcept
{
    return compiler_owned_token_prototype_enabled(input) ? FRONTEND_MACRO_M21I_DERIVE_TOKEN_BUFFER_KIND
                                                        : FRONTEND_MACRO_M21H_TOKEN_BUFFER_KIND;
}

[[nodiscard]] std::string_view token_producer_policy_for_input(const EarlyItemMacroInput& input) noexcept
{
    return compiler_owned_token_prototype_enabled(input) ? FRONTEND_MACRO_M21I_DERIVE_TOKEN_PRODUCER_POLICY
                                                        : FRONTEND_MACRO_M21I_EMPTY_TOKEN_PRODUCER_POLICY;
}

[[nodiscard]] std::string_view token_admission_blocker_for_input(const EarlyItemMacroInput& input) noexcept
{
    return compiler_owned_token_prototype_enabled(input) ? FRONTEND_MACRO_M21I_DERIVE_ADMISSION_BLOCKER
                                                        : FRONTEND_MACRO_M21I_EMPTY_ADMISSION_BLOCKER;
}

[[nodiscard]] std::string_view token_buffer_blocker_for_input(const EarlyItemMacroInput& input) noexcept
{
    return compiler_owned_token_prototype_enabled(input) ? FRONTEND_MACRO_M21I_DERIVE_TOKEN_BUFFER_BLOCKER
                                                        : FRONTEND_MACRO_M21I_EMPTY_TOKEN_BUFFER_BLOCKER;
}

[[nodiscard]] std::string_view parser_admission_blocker_for_input(const EarlyItemMacroInput& input) noexcept
{
    return compiler_owned_token_prototype_enabled(input) ? FRONTEND_MACRO_M21J_DERIVE_PARSE_BLOCKER
                                                        : FRONTEND_MACRO_M21J_EMPTY_PARSE_BLOCKER;
}

[[nodiscard]] std::string_view parser_admission_diagnostic_category_for_input(
    const EarlyItemMacroInput& input) noexcept
{
    return compiler_owned_token_prototype_enabled(input) ? FRONTEND_MACRO_M21K_DERIVE_BLOCKER_CATEGORY
                                                        : FRONTEND_MACRO_M21K_EMPTY_BLOCKER_CATEGORY;
}

[[nodiscard]] std::string_view parser_admission_diagnostic_message_for_input(
    const EarlyItemMacroInput& input) noexcept
{
    return compiler_owned_token_prototype_enabled(input) ? FRONTEND_MACRO_M21K_DERIVE_USER_MESSAGE
                                                        : FRONTEND_MACRO_M21K_EMPTY_USER_MESSAGE;
}

[[nodiscard]] std::string parser_admission_debug_projection_name(const EarlyItemMacroInput& input)
{
    std::string name(FRONTEND_MACRO_M21K_DEBUG_PROJECTION_PREFIX);
    name += std::to_string(input.module.value);
    name.push_back(':');
    name += std::to_string(input.part_index);
    name.push_back(':');
    name += std::to_string(input.item.value);
    name.push_back(':');
    name += std::to_string(input.attribute_index);
    name.push_back(':');
    name += input.attribute_name;
    return name;
}

[[nodiscard]] std::string parser_admission_report_query_name(
    const syntax::ModuleId module, const base::u32 source_part_index)
{
    std::string name(FRONTEND_MACRO_M21L_QUERY_NAME_PREFIX);
    name += std::to_string(module.value);
    name.push_back(':');
    name += std::to_string(source_part_index);
    return name;
}

[[nodiscard]] bool token_buffer_kind_is_compiler_owned(const std::string_view token_buffer_kind) noexcept
{
    return token_buffer_kind == FRONTEND_MACRO_M21I_DERIVE_TOKEN_BUFFER_KIND
        || token_buffer_kind == FRONTEND_MACRO_M21H_TOKEN_BUFFER_KIND;
}

[[nodiscard]] bool token_producer_policy_is_compiler_owned(const std::string_view token_producer_policy) noexcept
{
    return token_producer_policy == FRONTEND_MACRO_M21I_DERIVE_TOKEN_PRODUCER_POLICY
        || token_producer_policy == FRONTEND_MACRO_M21I_EMPTY_TOKEN_PRODUCER_POLICY;
}

[[nodiscard]] bool token_materialization_admission_state_is_valid(
    const TokenMaterializationAdmissionStub& stub) noexcept
{
    if (stub.blocker_reason == FRONTEND_MACRO_M21I_DERIVE_ADMISSION_BLOCKER) {
        return stub.materialized_tokens;
    }
    if (stub.blocker_reason == FRONTEND_MACRO_M21I_EMPTY_ADMISSION_BLOCKER) {
        return !stub.materialized_tokens;
    }
    return false;
}

[[nodiscard]] bool generated_token_buffer_state_is_valid(const GeneratedTokenBufferStub& stub) noexcept
{
    if (stub.token_buffer_kind == FRONTEND_MACRO_M21I_DERIVE_TOKEN_BUFFER_KIND) {
        return stub.token_producer_policy == FRONTEND_MACRO_M21I_DERIVE_TOKEN_PRODUCER_POLICY
            && stub.blocker_reason == FRONTEND_MACRO_M21I_DERIVE_TOKEN_BUFFER_BLOCKER
            && stub.token_count >= FRONTEND_MACRO_M21I_DERIVE_SENTINEL_TOKEN_COUNT
            && !stub.empty
            && stub.materialized_tokens;
    }
    if (stub.token_buffer_kind == FRONTEND_MACRO_M21H_TOKEN_BUFFER_KIND) {
        return stub.token_producer_policy == FRONTEND_MACRO_M21I_EMPTY_TOKEN_PRODUCER_POLICY
            && stub.blocker_reason == FRONTEND_MACRO_M21I_EMPTY_TOKEN_BUFFER_BLOCKER
            && stub.token_count == 0
            && stub.empty
            && !stub.materialized_tokens;
    }
    return false;
}

[[nodiscard]] bool parser_admission_diagnostic_category_is_valid(
    const ParserAdmissionDiagnosticProjectionStub& stub) noexcept
{
    if (stub.blocker_category == FRONTEND_MACRO_M21K_DERIVE_BLOCKER_CATEGORY) {
        return stub.token_buffer_blocker == FRONTEND_MACRO_M21J_DERIVE_PARSE_BLOCKER
            && stub.user_message == FRONTEND_MACRO_M21K_DERIVE_USER_MESSAGE
            && stub.token_count > 0
            && stub.token_buffer_materialized
            && stub.token_records_available;
    }
    if (stub.blocker_category == FRONTEND_MACRO_M21K_EMPTY_BLOCKER_CATEGORY) {
        return stub.token_buffer_blocker == FRONTEND_MACRO_M21J_EMPTY_PARSE_BLOCKER
            && stub.user_message == FRONTEND_MACRO_M21K_EMPTY_USER_MESSAGE
            && stub.token_count == 0
            && !stub.token_buffer_materialized
            && !stub.token_records_available;
    }
    return false;
}

[[nodiscard]] bool parser_admission_report_entry_category_is_valid(
    const ParserAdmissionDiagnosticReportEntry& entry) noexcept
{
    if (entry.blocker_category == FRONTEND_MACRO_M21K_DERIVE_BLOCKER_CATEGORY) {
        return entry.token_count > 0 && entry.token_records_available;
    }
    if (entry.blocker_category == FRONTEND_MACRO_M21K_EMPTY_BLOCKER_CATEGORY) {
        return entry.token_count == 0 && !entry.token_records_available;
    }
    return false;
}

[[nodiscard]] base::u64 generated_token_count_for_attribute(
    const EarlyItemMacroInput& input) noexcept
{
    if (!compiler_owned_token_prototype_enabled(input)) {
        return 0;
    }
    return input.token_count + FRONTEND_MACRO_M21I_DERIVE_SENTINEL_TOKEN_COUNT;
}

[[nodiscard]] std::string generated_source_token_text(const base::u32 token_index)
{
    std::string token_text(FRONTEND_MACRO_M21I_DERIVE_SOURCE_TOKEN_PREFIX);
    token_text += std::to_string(token_index);
    return token_text;
}

[[nodiscard]] query::StableFingerprint128 generated_token_materialization_identity(
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const TokenMaterializationAdmissionStub& admission,
    const std::string_view token_buffer_kind,
    const std::string_view token_producer_policy,
    const base::u64 token_count) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21I_TOKEN_MATERIALIZATION_IDENTITY_MARKER);
    mix_macro_input_identity(builder, input);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    builder.mix_fingerprint(placeholder.output_fingerprint);
    builder.mix_fingerprint(admission.token_plan_identity);
    builder.mix_fingerprint(admission.token_buffer_identity);
    builder.mix_fingerprint(admission.source_map_identity);
    builder.mix_fingerprint(admission.hygiene_mark);
    builder.mix_string(token_buffer_kind);
    builder.mix_string(token_producer_policy);
    builder.mix_u64(token_count);
    builder.mix_bool(compiler_owned_token_prototype_enabled(input));
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 generated_token_record_identity(
    const EarlyItemMacroInput& input,
    const GeneratedTokenBufferStub& buffer,
    const base::u32 token_index,
    const syntax::TokenKind token_kind,
    const std::string_view token_text,
    const std::string_view token_role,
    const base::SourceRange& anchor_range) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21I_GENERATED_TOKEN_RECORD_MARKER);
    mix_macro_input_identity(builder, input);
    builder.mix_fingerprint(buffer.token_buffer_identity);
    builder.mix_fingerprint(buffer.materialization_identity);
    builder.mix_fingerprint(buffer.source_map_identity);
    builder.mix_fingerprint(buffer.hygiene_mark);
    builder.mix_u32(token_index);
    builder.mix_u8(static_cast<base::u8>(token_kind));
    builder.mix_string(token_text);
    builder.mix_string(token_role);
    builder.mix_u64(static_cast<base::u64>(anchor_range.source.value));
    builder.mix_u64(static_cast<base::u64>(anchor_range.begin));
    builder.mix_u64(static_cast<base::u64>(anchor_range.end));
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 generated_token_parser_admission_gate_identity(
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const GeneratedModulePartParseMergeStub& parse_merge_stub,
    const GeneratedTokenBufferStub& buffer,
    const bool token_records_available) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21J_PARSE_GATE_IDENTITY_MARKER);
    mix_macro_input_identity(builder, input);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    builder.mix_fingerprint(placeholder.output_fingerprint);
    builder.mix_fingerprint(parse_merge_stub.generated_buffer_identity);
    builder.mix_fingerprint(parse_merge_stub.parse_config_fingerprint);
    builder.mix_fingerprint(parse_merge_stub.expansion_origin);
    builder.mix_fingerprint(buffer.token_plan_identity);
    builder.mix_fingerprint(buffer.token_buffer_identity);
    builder.mix_fingerprint(buffer.materialization_identity);
    builder.mix_fingerprint(buffer.source_map_identity);
    builder.mix_fingerprint(buffer.hygiene_mark);
    builder.mix_string(buffer.token_stream_name);
    builder.mix_string(FRONTEND_MACRO_M21J_PARSER_GATE_POLICY);
    builder.mix_string(parser_admission_blocker_for_input(input));
    builder.mix_u64(buffer.token_count);
    builder.mix_bool(buffer.materialized_tokens);
    builder.mix_bool(token_records_available);
    builder.mix_bool(compiler_owned_token_prototype_enabled(input));
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 parser_admission_diagnostic_anchor_identity(
    const EarlyItemMacroInput& input,
    const GeneratedTokenParserAdmissionGateStub& gate,
    const ExpansionTraceStub& trace) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21K_DIAGNOSTIC_ANCHOR_MARKER);
    mix_macro_input_identity(builder, input);
    builder.mix_u64(static_cast<base::u64>(input.attribute_range.source.value));
    builder.mix_u64(static_cast<base::u64>(input.attribute_range.begin));
    builder.mix_u64(static_cast<base::u64>(input.attribute_range.end));
    builder.mix_u64(static_cast<base::u64>(input.token_tree_range.source.value));
    builder.mix_u64(static_cast<base::u64>(input.token_tree_range.begin));
    builder.mix_u64(static_cast<base::u64>(input.token_tree_range.end));
    builder.mix_fingerprint(gate.parse_gate_identity);
    builder.mix_fingerprint(trace.trace_identity);
    builder.mix_fingerprint(trace.diagnostic_anchor);
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 parser_admission_diagnostic_identity(
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const GeneratedModulePartParseMergeStub& parse_merge_stub,
    const GeneratedTokenBufferStub& buffer,
    const GeneratedTokenParserAdmissionGateStub& gate,
    const ExpansionTraceStub& trace,
    const query::StableFingerprint128 diagnostic_anchor,
    const std::string_view debug_projection_name) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21K_DIAGNOSTIC_IDENTITY_MARKER);
    mix_macro_input_identity(builder, input);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    builder.mix_fingerprint(placeholder.output_fingerprint);
    builder.mix_fingerprint(gate.parse_gate_identity);
    builder.mix_fingerprint(diagnostic_anchor);
    builder.mix_fingerprint(buffer.token_plan_identity);
    builder.mix_fingerprint(buffer.token_buffer_identity);
    builder.mix_fingerprint(buffer.materialization_identity);
    builder.mix_fingerprint(parse_merge_stub.generated_buffer_identity);
    builder.mix_fingerprint(parse_merge_stub.parse_config_fingerprint);
    builder.mix_fingerprint(buffer.source_map_identity);
    builder.mix_fingerprint(buffer.hygiene_mark);
    builder.mix_fingerprint(trace.trace_identity);
    builder.mix_fingerprint(trace.generated_source_map_identity);
    builder.mix_string(FRONTEND_MACRO_M21K_DIAGNOSTIC_POLICY);
    builder.mix_string(parser_admission_diagnostic_category_for_input(input));
    builder.mix_string(parser_admission_blocker_for_input(input));
    builder.mix_string(FRONTEND_MACRO_M21K_GENERATED_PART_PARSE_BLOCKER);
    builder.mix_string(parser_admission_diagnostic_message_for_input(input));
    builder.mix_string(debug_projection_name);
    builder.mix_u64(buffer.token_count);
    builder.mix_bool(buffer.materialized_tokens);
    builder.mix_bool(buffer.token_count > 0);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
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

[[nodiscard]] GeneratedItemDeclarationStub make_generated_item_declaration_stub(
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const ExpansionHygieneStub& hygiene)
{
    const std::string generated_item_name = generated_item_name_for_input(input);
    return GeneratedItemDeclarationStub{
        input.item,
        input.module,
        input.part_index,
        input.attribute_index,
        input.attached_part,
        placeholder.generated_part,
        input.query_key_fingerprint,
        generated_item_stub_fingerprint(FRONTEND_MACRO_M21G_DECLARATION_IDENTITY_MARKER,
            input, placeholder, hygiene.declared_name_set, generated_item_name),
        hygiene.declared_name_set,
        generated_item_stub_fingerprint(FRONTEND_MACRO_M21G_GENERATED_ITEM_KEY_MARKER,
            input, placeholder, hygiene.declared_name_set, generated_item_name),
        std::string(FRONTEND_MACRO_M21G_DECLARATION_ROLE),
        generated_item_name,
        std::string(FRONTEND_MACRO_M21G_DECLARATION_BLOCKER),
        true,
        false,
        false,
        false,
        false,
        false,
    };
}

[[nodiscard]] DeclaredGeneratedNameStub make_declared_generated_name_stub(
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const ExpansionHygieneStub& hygiene,
    const GeneratedItemDeclarationStub& declaration)
{
    return DeclaredGeneratedNameStub{
        input.item,
        input.module,
        input.part_index,
        input.attribute_index,
        input.attached_part,
        placeholder.generated_part,
        input.query_key_fingerprint,
        hygiene.declared_name_set,
        generated_item_stub_fingerprint(FRONTEND_MACRO_M21G_DECLARED_NAME_IDENTITY_MARKER,
            input, placeholder, hygiene.declared_name_set, declaration.generated_item_name),
        hygiene.generated_fresh_mark,
        declaration.generated_item_name,
        std::string(FRONTEND_MACRO_M21G_DECLARED_NAME_NAMESPACE),
        std::string(FRONTEND_MACRO_M21G_DECLARED_NAME_BLOCKER),
        false,
        false,
        false,
        false,
    };
}

[[nodiscard]] TokenMaterializationAdmissionStub make_token_materialization_admission_stub(
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const ExpansionHygieneStub& hygiene,
    const ExpansionTraceStub& trace,
    const GeneratedItemDeclarationStub& declaration,
    const DeclaredGeneratedNameStub& declared_name)
{
    const std::string token_stream_name = token_stream_name_for_input(input);
    const std::string_view token_buffer_kind = token_buffer_kind_for_input(input);
    const query::StableFingerprint128 token_plan = token_plan_identity(
        input, placeholder, hygiene, trace, declaration, declared_name, token_stream_name);
    return TokenMaterializationAdmissionStub{
        input.item,
        input.module,
        input.part_index,
        input.attribute_index,
        input.attached_part,
        placeholder.generated_part,
        input.query_key_fingerprint,
        declaration.declaration_identity,
        declaration.generated_item_key,
        hygiene.declared_name_set,
        declared_name.declared_name_identity,
        declared_name.hygiene_mark,
        trace.generated_source_map_identity,
        trace.trace_identity,
        token_plan,
        token_buffer_identity(input, placeholder, hygiene, trace, token_plan, token_buffer_kind, token_stream_name),
        std::string(FRONTEND_MACRO_M21H_ADMISSION_POLICY),
        token_stream_name,
        std::string(token_admission_blocker_for_input(input)),
        true,
        true,
        compiler_owned_token_prototype_enabled(input),
        false,
        false,
        false,
        false,
        false,
        false,
    };
}

[[nodiscard]] GeneratedTokenBufferStub make_generated_token_buffer_stub(
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const ExpansionHygieneStub& hygiene,
    const ExpansionTraceStub& trace,
    const TokenMaterializationAdmissionStub& admission)
{
    const base::u64 token_count = generated_token_count_for_attribute(input);
    const bool materialized_tokens = token_count > 0;
    const std::string_view token_buffer_kind = token_buffer_kind_for_input(input);
    const std::string_view token_producer_policy = token_producer_policy_for_input(input);
    return GeneratedTokenBufferStub{
        input.item,
        input.module,
        input.part_index,
        input.attribute_index,
        input.attached_part,
        placeholder.generated_part,
        admission.token_plan_identity,
        admission.token_buffer_identity,
        generated_token_materialization_identity(
            input, placeholder, admission, token_buffer_kind, token_producer_policy, token_count),
        trace.generated_source_map_identity,
        hygiene.generated_fresh_mark,
        admission.token_stream_name,
        std::string(token_buffer_kind),
        std::string(token_producer_policy),
        std::string(token_buffer_blocker_for_input(input)),
        token_count,
        !materialized_tokens,
        materialized_tokens,
        false,
        false,
        false,
    };
}

void push_generated_token_record(std::vector<GeneratedTokenRecord>& records,
    const EarlyItemMacroInput& input,
    const GeneratedTokenBufferStub& buffer,
    const base::u32 token_index,
    const syntax::TokenKind token_kind,
    const std::string_view token_text,
    const std::string_view token_role,
    const base::SourceRange anchor_range)
{
    records.push_back(GeneratedTokenRecord{
        input.item,
        input.module,
        input.part_index,
        input.attribute_index,
        token_index,
        buffer.token_buffer_identity,
        generated_token_record_identity(input, buffer, token_index, token_kind, token_text, token_role, anchor_range),
        buffer.source_map_identity,
        buffer.hygiene_mark,
        token_kind,
        std::string(token_text),
        std::string(token_role),
        anchor_range,
        true,
        false,
        false,
    });
}

void append_generated_token_records_for_attribute(std::vector<GeneratedTokenRecord>& records,
    const EarlyItemMacroInput& input,
    const syntax::AttributeDecl& attribute,
    const GeneratedTokenBufferStub& buffer)
{
    if (!compiler_owned_token_prototype_enabled(input)) {
        return;
    }
    base::u32 token_index = 0;
    push_generated_token_record(records, input, buffer, token_index, syntax::TokenKind::identifier,
        FRONTEND_MACRO_M21I_DERIVE_BEGIN_TOKEN_TEXT, FRONTEND_MACRO_M21I_DERIVE_BEGIN_TOKEN_ROLE,
        input.attribute_range);
    ++token_index;
    for (const syntax::AttributeTokenDecl& source_token : attribute.token_tree) {
        const std::string token_text = generated_source_token_text(token_index);
        push_generated_token_record(records, input, buffer, token_index, source_token.kind, token_text,
            FRONTEND_MACRO_M21I_DERIVE_SOURCE_TOKEN_ROLE, source_token.range);
        ++token_index;
    }
    push_generated_token_record(records, input, buffer, token_index, syntax::TokenKind::identifier,
        FRONTEND_MACRO_M21I_DERIVE_END_TOKEN_TEXT, FRONTEND_MACRO_M21I_DERIVE_END_TOKEN_ROLE,
        input.attribute_range);
}

[[nodiscard]] GeneratedTokenParserAdmissionGateStub make_generated_token_parser_admission_gate_stub(
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const GeneratedModulePartParseMergeStub& parse_merge_stub,
    const GeneratedTokenBufferStub& buffer)
{
    const bool token_records_available = buffer.token_count > 0;
    return GeneratedTokenParserAdmissionGateStub{
        input.item,
        input.module,
        input.part_index,
        input.attribute_index,
        input.attached_part,
        placeholder.generated_part,
        buffer.token_plan_identity,
        buffer.token_buffer_identity,
        buffer.materialization_identity,
        buffer.source_map_identity,
        buffer.hygiene_mark,
        parse_merge_stub.generated_buffer_identity,
        parse_merge_stub.parse_config_fingerprint,
        generated_token_parser_admission_gate_identity(
            input, placeholder, parse_merge_stub, buffer, token_records_available),
        buffer.token_stream_name,
        std::string(FRONTEND_MACRO_M21J_PARSER_GATE_POLICY),
        std::string(parser_admission_blocker_for_input(input)),
        buffer.token_count,
        true,
        buffer.materialized_tokens,
        token_records_available,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
    };
}

[[nodiscard]] ParserAdmissionDiagnosticProjectionStub make_parser_admission_diagnostic_projection_stub(
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const GeneratedModulePartParseMergeStub& parse_merge_stub,
    const ExpansionTraceStub& trace,
    const GeneratedTokenBufferStub& buffer,
    const GeneratedTokenParserAdmissionGateStub& gate)
{
    const std::string debug_projection_name = parser_admission_debug_projection_name(input);
    const query::StableFingerprint128 diagnostic_anchor =
        parser_admission_diagnostic_anchor_identity(input, gate, trace);
    return ParserAdmissionDiagnosticProjectionStub{
        input.item,
        input.module,
        input.part_index,
        input.attribute_index,
        input.attached_part,
        placeholder.generated_part,
        input.attribute_range,
        input.token_tree_range,
        gate.parse_gate_identity,
        parser_admission_diagnostic_identity(input, placeholder, parse_merge_stub, buffer, gate, trace,
            diagnostic_anchor, debug_projection_name),
        diagnostic_anchor,
        gate.token_plan_identity,
        gate.token_buffer_identity,
        gate.materialization_identity,
        parse_merge_stub.generated_buffer_identity,
        parse_merge_stub.parse_config_fingerprint,
        buffer.source_map_identity,
        buffer.hygiene_mark,
        trace.trace_identity,
        std::string(FRONTEND_MACRO_M21K_DIAGNOSTIC_POLICY),
        std::string(parser_admission_diagnostic_category_for_input(input)),
        std::string(parser_admission_blocker_for_input(input)),
        std::string(FRONTEND_MACRO_M21K_GENERATED_PART_PARSE_BLOCKER),
        std::string(parser_admission_diagnostic_message_for_input(input)),
        debug_projection_name,
        gate.token_count,
        gate.token_buffer_materialized,
        gate.token_records_available,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
    };
}

[[nodiscard]] query::StableFingerprint128 parser_admission_report_entry_identity(
    const ParserAdmissionDiagnosticProjectionStub& diagnostic,
    const base::u32 report_index,
    const std::string_view query_projection_name) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21L_REPORT_ENTRY_IDENTITY_MARKER);
    builder.mix_u32(diagnostic.item.value);
    builder.mix_u32(diagnostic.module.value);
    builder.mix_u32(diagnostic.part_index);
    builder.mix_u32(diagnostic.attribute_index);
    builder.mix_u32(report_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(diagnostic.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(diagnostic.generated_part));
    builder.mix_u64(static_cast<base::u64>(diagnostic.primary_anchor.source.value));
    builder.mix_u64(static_cast<base::u64>(diagnostic.primary_anchor.begin));
    builder.mix_u64(static_cast<base::u64>(diagnostic.primary_anchor.end));
    builder.mix_u64(static_cast<base::u64>(diagnostic.token_tree_anchor.source.value));
    builder.mix_u64(static_cast<base::u64>(diagnostic.token_tree_anchor.begin));
    builder.mix_u64(static_cast<base::u64>(diagnostic.token_tree_anchor.end));
    builder.mix_fingerprint(diagnostic.diagnostic_identity);
    builder.mix_fingerprint(diagnostic.diagnostic_anchor_identity);
    builder.mix_fingerprint(diagnostic.parse_gate_identity);
    builder.mix_string(diagnostic.blocker_category);
    builder.mix_string(diagnostic.debug_projection_name);
    builder.mix_string(query_projection_name);
    builder.mix_u64(diagnostic.token_count);
    builder.mix_bool(diagnostic.token_records_available);
    builder.mix_bool(false);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    return builder.finish();
}

[[nodiscard]] ParserAdmissionDiagnosticReportEntry make_parser_admission_report_entry(
    const ParserAdmissionDiagnosticProjectionStub& diagnostic,
    const base::u32 report_index)
{
    const std::string query_projection_name =
        parser_admission_report_query_name(diagnostic.module, diagnostic.part_index);
    return ParserAdmissionDiagnosticReportEntry{
        diagnostic.item,
        diagnostic.module,
        diagnostic.part_index,
        diagnostic.attribute_index,
        report_index,
        diagnostic.attached_part,
        diagnostic.generated_part,
        diagnostic.primary_anchor,
        diagnostic.token_tree_anchor,
        diagnostic.diagnostic_identity,
        diagnostic.diagnostic_anchor_identity,
        parser_admission_report_entry_identity(diagnostic, report_index, query_projection_name),
        diagnostic.parse_gate_identity,
        diagnostic.blocker_category,
        diagnostic.debug_projection_name,
        query_projection_name,
        diagnostic.token_count,
        diagnostic.token_records_available,
        false,
        true,
        true,
        false,
        false,
        false,
    };
}

[[nodiscard]] bool source_range_less_or_equal(
    const base::SourceRange& lhs, const base::SourceRange& rhs) noexcept
{
    if (lhs.source.value != rhs.source.value) {
        return lhs.source.value < rhs.source.value;
    }
    if (lhs.begin != rhs.begin) {
        return lhs.begin < rhs.begin;
    }
    return lhs.end <= rhs.end;
}

[[nodiscard]] bool parser_admission_report_entries_are_ordered(
    const std::vector<ParserAdmissionDiagnosticReportEntry>& entries,
    const syntax::ModuleId module,
    const base::u32 source_part_index) noexcept
{
    bool saw_entry = false;
    base::SourceRange previous{};
    for (const ParserAdmissionDiagnosticReportEntry& entry : entries) {
        if (entry.module.value != module.value || entry.part_index != source_part_index) {
            continue;
        }
        if (saw_entry && !source_range_less_or_equal(previous, entry.primary_anchor)) {
            return false;
        }
        previous = entry.primary_anchor;
        saw_entry = true;
    }
    return true;
}

[[nodiscard]] query::StableFingerprint128 parser_admission_report_grouping_identity(
    const GeneratedModulePartPlaceholder& placeholder,
    const std::vector<ParserAdmissionDiagnosticReportEntry>& entries) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21L_REPORT_GROUPING_IDENTITY_MARKER);
    builder.mix_u32(placeholder.module.value);
    builder.mix_u32(placeholder.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.source_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    base::u64 entry_count = 0;
    for (const ParserAdmissionDiagnosticReportEntry& entry : entries) {
        if (entry.module.value != placeholder.module.value
            || entry.part_index != placeholder.source_part_index) {
            continue;
        }
        ++entry_count;
        builder.mix_fingerprint(entry.report_entry_identity);
        builder.mix_fingerprint(entry.diagnostic_identity);
        builder.mix_fingerprint(entry.diagnostic_anchor_identity);
        builder.mix_u32(entry.report_index);
        builder.mix_string(entry.blocker_category);
    }
    builder.mix_u64(entry_count);
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 parser_admission_report_anchor_identity(
    const GeneratedModulePartPlaceholder& placeholder,
    const std::vector<ParserAdmissionDiagnosticReportEntry>& entries) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21L_REPORT_ANCHOR_IDENTITY_MARKER);
    builder.mix_u32(placeholder.module.value);
    builder.mix_u32(placeholder.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    for (const ParserAdmissionDiagnosticReportEntry& entry : entries) {
        if (entry.module.value != placeholder.module.value
            || entry.part_index != placeholder.source_part_index) {
            continue;
        }
        builder.mix_u32(entry.report_index);
        builder.mix_u64(static_cast<base::u64>(entry.primary_anchor.source.value));
        builder.mix_u64(static_cast<base::u64>(entry.primary_anchor.begin));
        builder.mix_u64(static_cast<base::u64>(entry.primary_anchor.end));
        builder.mix_fingerprint(entry.diagnostic_anchor_identity);
    }
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 parser_admission_report_identity(
    const GeneratedModulePartPlaceholder& placeholder,
    const GeneratedModulePartParseMergeStub& parse_merge_stub,
    const query::StableFingerprint128 grouping_identity,
    const query::StableFingerprint128 anchor_identity,
    const std::string_view report_query_name,
    const std::vector<ParserAdmissionDiagnosticReportEntry>& entries) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21L_REPORT_IDENTITY_MARKER);
    builder.mix_u32(placeholder.module.value);
    builder.mix_u32(placeholder.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.source_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    builder.mix_fingerprint(parse_merge_stub.generated_buffer_identity);
    builder.mix_fingerprint(parse_merge_stub.parse_config_fingerprint);
    builder.mix_fingerprint(grouping_identity);
    builder.mix_fingerprint(anchor_identity);
    builder.mix_string(FRONTEND_MACRO_M21L_REPORT_POLICY);
    builder.mix_string(report_query_name);
    builder.mix_string(FRONTEND_MACRO_M21L_REPORT_BLOCKER);
    for (const ParserAdmissionDiagnosticReportEntry& entry : entries) {
        if (entry.module.value != placeholder.module.value
            || entry.part_index != placeholder.source_part_index) {
            continue;
        }
        builder.mix_fingerprint(entry.report_entry_identity);
        builder.mix_bool(entry.token_records_available);
        builder.mix_bool(entry.parser_admitted);
    }
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(parser_admission_report_entries_are_ordered(
        entries, placeholder.module, placeholder.source_part_index));
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    return builder.finish();
}

[[nodiscard]] ParserAdmissionDiagnosticReport make_parser_admission_report(
    const GeneratedModulePartPlaceholder& placeholder,
    const GeneratedModulePartParseMergeStub& parse_merge_stub,
    const std::vector<ParserAdmissionDiagnosticReportEntry>& entries)
{
    const std::string report_query_name =
        parser_admission_report_query_name(placeholder.module, placeholder.source_part_index);
    const query::StableFingerprint128 grouping_identity =
        parser_admission_report_grouping_identity(placeholder, entries);
    const query::StableFingerprint128 anchor_identity =
        parser_admission_report_anchor_identity(placeholder, entries);
    ParserAdmissionDiagnosticReport report{
        placeholder.module,
        placeholder.source_part_index,
        placeholder.source_part,
        placeholder.generated_part,
        {},
        anchor_identity,
        grouping_identity,
        parse_merge_stub.parse_config_fingerprint,
        parse_merge_stub.generated_buffer_identity,
        std::string(FRONTEND_MACRO_M21L_REPORT_POLICY),
        report_query_name,
        std::string(FRONTEND_MACRO_M21L_REPORT_BLOCKER),
        0,
        0,
        0,
        0,
        0,
        true,
        true,
        parser_admission_report_entries_are_ordered(entries, placeholder.module, placeholder.source_part_index),
        false,
        false,
        false,
        false,
        false,
        false,
        false,
    };
    for (const ParserAdmissionDiagnosticReportEntry& entry : entries) {
        if (entry.module.value != placeholder.module.value
            || entry.part_index != placeholder.source_part_index) {
            continue;
        }
        ++report.entry_count;
        if (!entry.parser_admitted) {
            ++report.blocked_entry_count;
        }
        if (entry.blocker_category == FRONTEND_MACRO_M21K_DERIVE_BLOCKER_CATEGORY) {
            ++report.derive_entry_count;
        }
        if (entry.blocker_category == FRONTEND_MACRO_M21K_EMPTY_BLOCKER_CATEGORY) {
            ++report.empty_entry_count;
        }
        if (entry.token_records_available) {
            ++report.token_record_available_entry_count;
        }
    }
    report.report_identity = parser_admission_report_identity(
        placeholder, parse_merge_stub, report.report_grouping_identity, report.report_anchor_identity,
        report.report_query_name, entries);
    return report;
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

[[nodiscard]] base::usize count_compiler_owned_generated_token_records(const syntax::AstModule& ast)
{
    base::usize count = 0;
    for (base::usize item_index = 0; item_index < ast.items.size(); ++item_index) {
        const syntax::ItemNode& item = ast.items[item_index];
        for (base::usize attribute_index = 0; attribute_index < item.attributes.size(); ++attribute_index) {
            const syntax::AttributeDecl& attribute = item.attributes[attribute_index];
            if (disposition_for_attribute(attribute) != EarlyItemExpansionDisposition::builtin_derive_passthrough) {
                continue;
            }
            count = base::checked_add_usize(
                count, attribute.token_tree.size(), FRONTEND_MACRO_M21I_GENERATED_TOKEN_RESERVE_CONTEXT);
            count = base::checked_add_usize(count,
                static_cast<base::usize>(FRONTEND_MACRO_M21I_DERIVE_SENTINEL_TOKEN_COUNT),
                FRONTEND_MACRO_M21I_GENERATED_TOKEN_RESERVE_CONTEXT);
        }
    }
    return count;
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

void mix_generated_item_declaration_stub(
    query::StableHashBuilder& builder, const GeneratedItemDeclarationStub& stub) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M21G_GENERATED_ITEM_DECLARATION_MARKER);
    builder.mix_u32(stub.item.value);
    builder.mix_u32(stub.module.value);
    builder.mix_u32(stub.part_index);
    builder.mix_u32(stub.attribute_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(stub.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(stub.generated_part));
    builder.mix_fingerprint(stub.expansion_origin);
    builder.mix_fingerprint(stub.declaration_identity);
    builder.mix_fingerprint(stub.declared_name_set);
    builder.mix_fingerprint(stub.generated_item_key);
    builder.mix_string(stub.declaration_role);
    builder.mix_string(stub.generated_item_name);
    builder.mix_string(stub.blocker_reason);
    builder.mix_bool(stub.planned);
    builder.mix_bool(stub.materialized_tokens);
    builder.mix_bool(stub.parsed);
    builder.mix_bool(stub.merged);
    builder.mix_bool(stub.sema_visible);
    builder.mix_bool(stub.produced_user_generated_code);
}

void mix_declared_generated_name_stub(
    query::StableHashBuilder& builder, const DeclaredGeneratedNameStub& stub) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M21G_DECLARED_NAME_STUB_MARKER);
    builder.mix_u32(stub.item.value);
    builder.mix_u32(stub.module.value);
    builder.mix_u32(stub.part_index);
    builder.mix_u32(stub.attribute_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(stub.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(stub.generated_part));
    builder.mix_fingerprint(stub.expansion_origin);
    builder.mix_fingerprint(stub.declared_name_set);
    builder.mix_fingerprint(stub.declared_name_identity);
    builder.mix_fingerprint(stub.hygiene_mark);
    builder.mix_string(stub.declared_name);
    builder.mix_string(stub.namespace_kind);
    builder.mix_string(stub.blocker_reason);
    builder.mix_bool(stub.lookup_visible);
    builder.mix_bool(stub.export_visible);
    builder.mix_bool(stub.sema_visible);
    builder.mix_bool(stub.produced_user_generated_code);
}

void mix_token_materialization_admission_stub(
    query::StableHashBuilder& builder, const TokenMaterializationAdmissionStub& stub) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M21H_ADMISSION_STUB_MARKER);
    builder.mix_u32(stub.item.value);
    builder.mix_u32(stub.module.value);
    builder.mix_u32(stub.part_index);
    builder.mix_u32(stub.attribute_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(stub.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(stub.generated_part));
    builder.mix_fingerprint(stub.expansion_origin);
    builder.mix_fingerprint(stub.declaration_identity);
    builder.mix_fingerprint(stub.generated_item_key);
    builder.mix_fingerprint(stub.declared_name_set);
    builder.mix_fingerprint(stub.declared_name_identity);
    builder.mix_fingerprint(stub.hygiene_mark);
    builder.mix_fingerprint(stub.source_map_identity);
    builder.mix_fingerprint(stub.trace_identity);
    builder.mix_fingerprint(stub.token_plan_identity);
    builder.mix_fingerprint(stub.token_buffer_identity);
    builder.mix_string(stub.admission_policy);
    builder.mix_string(stub.token_stream_name);
    builder.mix_string(stub.blocker_reason);
    builder.mix_bool(stub.compiler_owned);
    builder.mix_bool(stub.admitted);
    builder.mix_bool(stub.materialized_tokens);
    builder.mix_bool(stub.generated_source_text);
    builder.mix_bool(stub.parse_ready);
    builder.mix_bool(stub.external_process_required);
    builder.mix_bool(stub.standard_library_required);
    builder.mix_bool(stub.runtime_required);
    builder.mix_bool(stub.produced_user_generated_code);
}

void mix_generated_token_buffer_stub(
    query::StableHashBuilder& builder, const GeneratedTokenBufferStub& stub) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M21H_TOKEN_BUFFER_STUB_MARKER);
    builder.mix_u32(stub.item.value);
    builder.mix_u32(stub.module.value);
    builder.mix_u32(stub.part_index);
    builder.mix_u32(stub.attribute_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(stub.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(stub.generated_part));
    builder.mix_fingerprint(stub.token_plan_identity);
    builder.mix_fingerprint(stub.token_buffer_identity);
    builder.mix_fingerprint(stub.materialization_identity);
    builder.mix_fingerprint(stub.source_map_identity);
    builder.mix_fingerprint(stub.hygiene_mark);
    builder.mix_string(stub.token_stream_name);
    builder.mix_string(stub.token_buffer_kind);
    builder.mix_string(stub.token_producer_policy);
    builder.mix_string(stub.blocker_reason);
    builder.mix_u64(stub.token_count);
    builder.mix_bool(stub.empty);
    builder.mix_bool(stub.materialized_tokens);
    builder.mix_bool(stub.generated_source_text);
    builder.mix_bool(stub.parser_consumable);
    builder.mix_bool(stub.produced_user_generated_code);
}

void mix_generated_token_record(query::StableHashBuilder& builder, const GeneratedTokenRecord& record) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M21I_GENERATED_TOKEN_RECORD_MARKER);
    builder.mix_u32(record.item.value);
    builder.mix_u32(record.module.value);
    builder.mix_u32(record.part_index);
    builder.mix_u32(record.attribute_index);
    builder.mix_u32(record.token_index);
    builder.mix_fingerprint(record.token_buffer_identity);
    builder.mix_fingerprint(record.token_identity);
    builder.mix_fingerprint(record.source_map_identity);
    builder.mix_fingerprint(record.hygiene_mark);
    builder.mix_u8(static_cast<base::u8>(record.kind));
    builder.mix_string(record.text);
    builder.mix_string(record.token_role);
    builder.mix_u64(static_cast<base::u64>(record.anchor_range.source.value));
    builder.mix_u64(static_cast<base::u64>(record.anchor_range.begin));
    builder.mix_u64(static_cast<base::u64>(record.anchor_range.end));
    builder.mix_bool(record.compiler_owned);
    builder.mix_bool(record.parser_visible);
    builder.mix_bool(record.produced_user_generated_code);
}

void mix_generated_token_parser_admission_gate_stub(
    query::StableHashBuilder& builder, const GeneratedTokenParserAdmissionGateStub& stub) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M21J_PARSER_ADMISSION_GATE_MARKER);
    builder.mix_u32(stub.item.value);
    builder.mix_u32(stub.module.value);
    builder.mix_u32(stub.part_index);
    builder.mix_u32(stub.attribute_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(stub.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(stub.generated_part));
    builder.mix_fingerprint(stub.token_plan_identity);
    builder.mix_fingerprint(stub.token_buffer_identity);
    builder.mix_fingerprint(stub.materialization_identity);
    builder.mix_fingerprint(stub.source_map_identity);
    builder.mix_fingerprint(stub.hygiene_mark);
    builder.mix_fingerprint(stub.generated_buffer_identity);
    builder.mix_fingerprint(stub.parse_config_fingerprint);
    builder.mix_fingerprint(stub.parse_gate_identity);
    builder.mix_string(stub.token_stream_name);
    builder.mix_string(stub.parser_gate_policy);
    builder.mix_string(stub.blocker_reason);
    builder.mix_u64(stub.token_count);
    builder.mix_bool(stub.compiler_owned);
    builder.mix_bool(stub.token_buffer_materialized);
    builder.mix_bool(stub.token_records_available);
    builder.mix_bool(stub.parser_admitted);
    builder.mix_bool(stub.parse_ready);
    builder.mix_bool(stub.parser_consumable);
    builder.mix_bool(stub.generated_source_text);
    builder.mix_bool(stub.generated_part_parsed);
    builder.mix_bool(stub.generated_part_merged);
    builder.mix_bool(stub.sema_visible);
    builder.mix_bool(stub.produced_user_generated_code);
}

void mix_parser_admission_diagnostic_projection_stub(
    query::StableHashBuilder& builder, const ParserAdmissionDiagnosticProjectionStub& stub) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M21K_DIAGNOSTIC_PROJECTION_MARKER);
    builder.mix_u32(stub.item.value);
    builder.mix_u32(stub.module.value);
    builder.mix_u32(stub.part_index);
    builder.mix_u32(stub.attribute_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(stub.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(stub.generated_part));
    builder.mix_u64(static_cast<base::u64>(stub.primary_anchor.source.value));
    builder.mix_u64(static_cast<base::u64>(stub.primary_anchor.begin));
    builder.mix_u64(static_cast<base::u64>(stub.primary_anchor.end));
    builder.mix_u64(static_cast<base::u64>(stub.token_tree_anchor.source.value));
    builder.mix_u64(static_cast<base::u64>(stub.token_tree_anchor.begin));
    builder.mix_u64(static_cast<base::u64>(stub.token_tree_anchor.end));
    builder.mix_fingerprint(stub.parse_gate_identity);
    builder.mix_fingerprint(stub.diagnostic_identity);
    builder.mix_fingerprint(stub.diagnostic_anchor_identity);
    builder.mix_fingerprint(stub.token_plan_identity);
    builder.mix_fingerprint(stub.token_buffer_identity);
    builder.mix_fingerprint(stub.materialization_identity);
    builder.mix_fingerprint(stub.generated_buffer_identity);
    builder.mix_fingerprint(stub.parse_config_fingerprint);
    builder.mix_fingerprint(stub.source_map_identity);
    builder.mix_fingerprint(stub.hygiene_mark);
    builder.mix_fingerprint(stub.trace_identity);
    builder.mix_string(stub.diagnostic_policy);
    builder.mix_string(stub.blocker_category);
    builder.mix_string(stub.token_buffer_blocker);
    builder.mix_string(stub.generated_part_parse_blocker);
    builder.mix_string(stub.user_message);
    builder.mix_string(stub.debug_projection_name);
    builder.mix_u64(stub.token_count);
    builder.mix_bool(stub.token_buffer_materialized);
    builder.mix_bool(stub.token_records_available);
    builder.mix_bool(stub.parser_admitted);
    builder.mix_bool(stub.parse_ready);
    builder.mix_bool(stub.parser_consumable);
    builder.mix_bool(stub.generated_part_parsed);
    builder.mix_bool(stub.generated_part_merged);
    builder.mix_bool(stub.emit_expanded_available);
    builder.mix_bool(stub.debug_trace_available);
    builder.mix_bool(stub.source_map_available);
    builder.mix_bool(stub.produced_user_generated_code);
}

void mix_parser_admission_report_entry(
    query::StableHashBuilder& builder, const ParserAdmissionDiagnosticReportEntry& entry) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M21L_REPORT_ENTRY_MARKER);
    builder.mix_u32(entry.item.value);
    builder.mix_u32(entry.module.value);
    builder.mix_u32(entry.part_index);
    builder.mix_u32(entry.attribute_index);
    builder.mix_u32(entry.report_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(entry.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(entry.generated_part));
    builder.mix_u64(static_cast<base::u64>(entry.primary_anchor.source.value));
    builder.mix_u64(static_cast<base::u64>(entry.primary_anchor.begin));
    builder.mix_u64(static_cast<base::u64>(entry.primary_anchor.end));
    builder.mix_u64(static_cast<base::u64>(entry.token_tree_anchor.source.value));
    builder.mix_u64(static_cast<base::u64>(entry.token_tree_anchor.begin));
    builder.mix_u64(static_cast<base::u64>(entry.token_tree_anchor.end));
    builder.mix_fingerprint(entry.diagnostic_identity);
    builder.mix_fingerprint(entry.diagnostic_anchor_identity);
    builder.mix_fingerprint(entry.report_entry_identity);
    builder.mix_fingerprint(entry.parse_gate_identity);
    builder.mix_string(entry.blocker_category);
    builder.mix_string(entry.debug_projection_name);
    builder.mix_string(entry.query_projection_name);
    builder.mix_u64(entry.token_count);
    builder.mix_bool(entry.token_records_available);
    builder.mix_bool(entry.parser_admitted);
    builder.mix_bool(entry.report_visible);
    builder.mix_bool(entry.query_reusable);
    builder.mix_bool(entry.parser_consumable);
    builder.mix_bool(entry.emit_expanded_available);
    builder.mix_bool(entry.produced_user_generated_code);
}

void mix_parser_admission_report(
    query::StableHashBuilder& builder, const ParserAdmissionDiagnosticReport& report) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M21L_REPORT_MARKER);
    builder.mix_u32(report.module.value);
    builder.mix_u32(report.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(report.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(report.generated_part));
    builder.mix_fingerprint(report.report_identity);
    builder.mix_fingerprint(report.report_anchor_identity);
    builder.mix_fingerprint(report.report_grouping_identity);
    builder.mix_fingerprint(report.parse_config_fingerprint);
    builder.mix_fingerprint(report.generated_buffer_identity);
    builder.mix_string(report.report_policy);
    builder.mix_string(report.report_query_name);
    builder.mix_string(report.blocked_reason);
    builder.mix_u64(report.entry_count);
    builder.mix_u64(report.blocked_entry_count);
    builder.mix_u64(report.derive_entry_count);
    builder.mix_u64(report.empty_entry_count);
    builder.mix_u64(report.token_record_available_entry_count);
    builder.mix_bool(report.query_reusable);
    builder.mix_bool(report.report_visible);
    builder.mix_bool(report.source_anchor_ordered);
    builder.mix_bool(report.parser_admitted);
    builder.mix_bool(report.parse_ready);
    builder.mix_bool(report.parser_consumable);
    builder.mix_bool(report.emit_expanded_available);
    builder.mix_bool(report.debug_trace_available);
    builder.mix_bool(report.source_map_available);
    builder.mix_bool(report.produced_user_generated_code);
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
    builder.mix_u64(summary.generated_item_declaration_stub_count);
    builder.mix_u64(summary.planned_generated_item_declaration_count);
    builder.mix_u64(summary.materialized_generated_item_count);
    builder.mix_u64(summary.declared_generated_name_stub_count);
    builder.mix_u64(summary.lookup_visible_declared_name_count);
    builder.mix_u64(summary.export_visible_declared_name_count);
    builder.mix_u64(summary.token_materialization_admission_stub_count);
    builder.mix_u64(summary.compiler_owned_admission_count);
    builder.mix_u64(summary.admitted_token_materialization_count);
    builder.mix_u64(summary.materialized_token_admission_count);
    builder.mix_u64(summary.generated_token_buffer_stub_count);
    builder.mix_u64(summary.empty_generated_token_buffer_count);
    builder.mix_u64(summary.materialized_token_buffer_count);
    builder.mix_u64(summary.compiler_owned_token_buffer_count);
    builder.mix_u64(summary.generated_token_record_count);
    builder.mix_u64(summary.compiler_owned_generated_token_record_count);
    builder.mix_u64(summary.parser_visible_generated_token_count);
    builder.mix_u64(summary.parser_admission_gate_stub_count);
    builder.mix_u64(summary.compiler_owned_parser_admission_gate_count);
    builder.mix_u64(summary.token_record_available_gate_count);
    builder.mix_u64(summary.parser_blocked_token_buffer_count);
    builder.mix_u64(summary.parser_admitted_token_buffer_count);
    builder.mix_u64(summary.parser_admission_diagnostic_stub_count);
    builder.mix_u64(summary.parser_admission_diagnostic_blocked_count);
    builder.mix_u64(summary.derive_parser_admission_diagnostic_count);
    builder.mix_u64(summary.empty_parser_admission_diagnostic_count);
    builder.mix_u64(summary.emit_expanded_projection_available_count);
    builder.mix_u64(summary.parser_admission_debug_trace_projection_count);
    builder.mix_u64(summary.parser_admission_source_map_projection_count);
    builder.mix_u64(summary.parser_admission_report_entry_count);
    builder.mix_u64(summary.parser_admission_report_count);
    builder.mix_u64(summary.parser_admission_report_blocked_entry_count);
    builder.mix_u64(summary.parser_admission_report_derive_entry_count);
    builder.mix_u64(summary.parser_admission_report_empty_entry_count);
    builder.mix_u64(summary.parser_admission_report_token_record_available_entry_count);
    builder.mix_u64(summary.parser_admission_report_visible_count);
    builder.mix_u64(summary.parser_admission_report_query_reusable_count);
    builder.mix_u64(summary.parser_admission_report_unordered_anchor_count);
    builder.mix_u64(summary.parser_admission_report_parser_consumable_count);
    builder.mix_u64(summary.generated_source_text_count);
    builder.mix_u64(summary.parse_ready_token_buffer_count);
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
        && lhs.generated_item_declaration_stub_count == rhs.generated_item_declaration_stub_count
        && lhs.planned_generated_item_declaration_count == rhs.planned_generated_item_declaration_count
        && lhs.materialized_generated_item_count == rhs.materialized_generated_item_count
        && lhs.declared_generated_name_stub_count == rhs.declared_generated_name_stub_count
        && lhs.lookup_visible_declared_name_count == rhs.lookup_visible_declared_name_count
        && lhs.export_visible_declared_name_count == rhs.export_visible_declared_name_count
        && lhs.token_materialization_admission_stub_count == rhs.token_materialization_admission_stub_count
        && lhs.compiler_owned_admission_count == rhs.compiler_owned_admission_count
        && lhs.admitted_token_materialization_count == rhs.admitted_token_materialization_count
        && lhs.materialized_token_admission_count == rhs.materialized_token_admission_count
        && lhs.generated_token_buffer_stub_count == rhs.generated_token_buffer_stub_count
        && lhs.empty_generated_token_buffer_count == rhs.empty_generated_token_buffer_count
        && lhs.materialized_token_buffer_count == rhs.materialized_token_buffer_count
        && lhs.compiler_owned_token_buffer_count == rhs.compiler_owned_token_buffer_count
        && lhs.generated_token_record_count == rhs.generated_token_record_count
        && lhs.compiler_owned_generated_token_record_count == rhs.compiler_owned_generated_token_record_count
        && lhs.parser_visible_generated_token_count == rhs.parser_visible_generated_token_count
        && lhs.parser_admission_gate_stub_count == rhs.parser_admission_gate_stub_count
        && lhs.compiler_owned_parser_admission_gate_count == rhs.compiler_owned_parser_admission_gate_count
        && lhs.token_record_available_gate_count == rhs.token_record_available_gate_count
        && lhs.parser_blocked_token_buffer_count == rhs.parser_blocked_token_buffer_count
        && lhs.parser_admitted_token_buffer_count == rhs.parser_admitted_token_buffer_count
        && lhs.parser_admission_diagnostic_stub_count == rhs.parser_admission_diagnostic_stub_count
        && lhs.parser_admission_diagnostic_blocked_count == rhs.parser_admission_diagnostic_blocked_count
        && lhs.derive_parser_admission_diagnostic_count == rhs.derive_parser_admission_diagnostic_count
        && lhs.empty_parser_admission_diagnostic_count == rhs.empty_parser_admission_diagnostic_count
        && lhs.emit_expanded_projection_available_count == rhs.emit_expanded_projection_available_count
        && lhs.parser_admission_debug_trace_projection_count == rhs.parser_admission_debug_trace_projection_count
        && lhs.parser_admission_source_map_projection_count == rhs.parser_admission_source_map_projection_count
        && lhs.parser_admission_report_entry_count == rhs.parser_admission_report_entry_count
        && lhs.parser_admission_report_count == rhs.parser_admission_report_count
        && lhs.parser_admission_report_blocked_entry_count == rhs.parser_admission_report_blocked_entry_count
        && lhs.parser_admission_report_derive_entry_count == rhs.parser_admission_report_derive_entry_count
        && lhs.parser_admission_report_empty_entry_count == rhs.parser_admission_report_empty_entry_count
        && lhs.parser_admission_report_token_record_available_entry_count
            == rhs.parser_admission_report_token_record_available_entry_count
        && lhs.parser_admission_report_visible_count == rhs.parser_admission_report_visible_count
        && lhs.parser_admission_report_query_reusable_count == rhs.parser_admission_report_query_reusable_count
        && lhs.parser_admission_report_unordered_anchor_count == rhs.parser_admission_report_unordered_anchor_count
        && lhs.parser_admission_report_parser_consumable_count
            == rhs.parser_admission_report_parser_consumable_count
        && lhs.generated_source_text_count == rhs.generated_source_text_count
        && lhs.parse_ready_token_buffer_count == rhs.parse_ready_token_buffer_count
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

[[nodiscard]] const GeneratedModulePartPlaceholder* find_generated_part_for(
    const std::vector<GeneratedModulePartPlaceholder>& generated_parts,
    const syntax::ModuleId module,
    const base::u32 source_part_index) noexcept
{
    const auto found = std::find_if(generated_parts.begin(), generated_parts.end(),
        [module, source_part_index](const GeneratedModulePartPlaceholder& part) {
            return part.module.value == module.value && part.source_part_index == source_part_index;
        });
    return found == generated_parts.end() ? nullptr : &*found;
}

[[nodiscard]] const GeneratedModulePartParseMergeStub* find_parse_merge_stub_for(
    const std::vector<GeneratedModulePartParseMergeStub>& stubs,
    const syntax::ModuleId module,
    const base::u32 source_part_index) noexcept
{
    const auto found = std::find_if(stubs.begin(), stubs.end(),
        [module, source_part_index](const GeneratedModulePartParseMergeStub& stub) {
            return stub.module.value == module.value && stub.source_part_index == source_part_index;
        });
    return found == stubs.end() ? nullptr : &*found;
}

[[nodiscard]] const ParserAdmissionDiagnosticProjectionStub* find_parser_admission_diagnostic_for_entry(
    const std::vector<ParserAdmissionDiagnosticProjectionStub>& diagnostics,
    const ParserAdmissionDiagnosticReportEntry& entry) noexcept
{
    const auto found = std::find_if(diagnostics.begin(), diagnostics.end(),
        [&entry](const ParserAdmissionDiagnosticProjectionStub& diagnostic) {
            return diagnostic.item.value == entry.item.value
                && diagnostic.module.value == entry.module.value
                && diagnostic.part_index == entry.part_index
                && diagnostic.attribute_index == entry.attribute_index
                && diagnostic.diagnostic_identity == entry.diagnostic_identity;
        });
    return found == diagnostics.end() ? nullptr : &*found;
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

[[nodiscard]] bool generated_item_declaration_matches_input(
    const GeneratedItemDeclarationStub& declaration,
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const ExpansionHygieneStub& hygiene) noexcept
{
    const std::string expected_name = generated_item_name_for_input(input);
    return declaration.item.value == input.item.value
        && declaration.module.value == input.module.value
        && declaration.part_index == input.part_index
        && declaration.attribute_index == input.attribute_index
        && declaration.attached_part == input.attached_part
        && declaration.generated_part == placeholder.generated_part
        && declaration.expansion_origin == input.query_key_fingerprint
        && declaration.declared_name_set == hygiene.declared_name_set
        && declaration.declaration_identity == generated_item_stub_fingerprint(
               FRONTEND_MACRO_M21G_DECLARATION_IDENTITY_MARKER,
               input, placeholder, hygiene.declared_name_set, expected_name)
        && declaration.generated_item_key == generated_item_stub_fingerprint(
               FRONTEND_MACRO_M21G_GENERATED_ITEM_KEY_MARKER,
               input, placeholder, hygiene.declared_name_set, expected_name)
        && declaration.declaration_role == FRONTEND_MACRO_M21G_DECLARATION_ROLE
        && declaration.generated_item_name == expected_name
        && declaration.blocker_reason == FRONTEND_MACRO_M21G_DECLARATION_BLOCKER;
}

[[nodiscard]] bool declared_generated_name_matches_input(
    const DeclaredGeneratedNameStub& name,
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const ExpansionHygieneStub& hygiene,
    const GeneratedItemDeclarationStub& declaration) noexcept
{
    const std::string expected_name = generated_item_name_for_input(input);
    return name.item.value == input.item.value
        && name.module.value == input.module.value
        && name.part_index == input.part_index
        && name.attribute_index == input.attribute_index
        && name.attached_part == input.attached_part
        && name.generated_part == placeholder.generated_part
        && name.expansion_origin == input.query_key_fingerprint
        && name.declared_name_set == hygiene.declared_name_set
        && name.declared_name_identity == generated_item_stub_fingerprint(
               FRONTEND_MACRO_M21G_DECLARED_NAME_IDENTITY_MARKER,
               input, placeholder, hygiene.declared_name_set, expected_name)
        && name.hygiene_mark == hygiene.generated_fresh_mark
        && name.declared_name == declaration.generated_item_name
        && name.declared_name == expected_name
        && name.namespace_kind == FRONTEND_MACRO_M21G_DECLARED_NAME_NAMESPACE
        && name.blocker_reason == FRONTEND_MACRO_M21G_DECLARED_NAME_BLOCKER;
}

[[nodiscard]] bool token_materialization_admission_matches_input(
    const TokenMaterializationAdmissionStub& admission,
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const ExpansionHygieneStub& hygiene,
    const ExpansionTraceStub& trace,
    const GeneratedItemDeclarationStub& declaration,
    const DeclaredGeneratedNameStub& declared_name) noexcept
{
    const std::string expected_stream_name = token_stream_name_for_input(input);
    const query::StableFingerprint128 expected_token_plan = token_plan_identity(
        input, placeholder, hygiene, trace, declaration, declared_name, expected_stream_name);
    return admission.item.value == input.item.value
        && admission.module.value == input.module.value
        && admission.part_index == input.part_index
        && admission.attribute_index == input.attribute_index
        && admission.attached_part == input.attached_part
        && admission.generated_part == placeholder.generated_part
        && admission.expansion_origin == input.query_key_fingerprint
        && admission.declaration_identity == declaration.declaration_identity
        && admission.generated_item_key == declaration.generated_item_key
        && admission.declared_name_set == hygiene.declared_name_set
        && admission.declared_name_identity == declared_name.declared_name_identity
        && admission.hygiene_mark == declared_name.hygiene_mark
        && admission.hygiene_mark == hygiene.generated_fresh_mark
        && admission.source_map_identity == trace.generated_source_map_identity
        && admission.trace_identity == trace.trace_identity
        && admission.token_plan_identity == expected_token_plan
        && admission.token_buffer_identity == token_buffer_identity(
               input, placeholder, hygiene, trace, expected_token_plan, token_buffer_kind_for_input(input),
               expected_stream_name)
        && admission.admission_policy == FRONTEND_MACRO_M21H_ADMISSION_POLICY
        && admission.token_stream_name == expected_stream_name
        && admission.blocker_reason == token_admission_blocker_for_input(input);
}

[[nodiscard]] bool generated_token_buffer_matches_input(
    const GeneratedTokenBufferStub& buffer,
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const ExpansionHygieneStub& hygiene,
    const ExpansionTraceStub& trace,
    const TokenMaterializationAdmissionStub& admission) noexcept
{
    const base::u64 expected_token_count = generated_token_count_for_attribute(input);
    const bool materialized_tokens = expected_token_count > 0;
    return buffer.item.value == input.item.value
        && buffer.module.value == input.module.value
        && buffer.part_index == input.part_index
        && buffer.attribute_index == input.attribute_index
        && buffer.attached_part == input.attached_part
        && buffer.generated_part == placeholder.generated_part
        && buffer.token_plan_identity == admission.token_plan_identity
        && buffer.token_buffer_identity == admission.token_buffer_identity
        && buffer.materialization_identity == generated_token_materialization_identity(input, placeholder,
               admission, token_buffer_kind_for_input(input), token_producer_policy_for_input(input),
               expected_token_count)
        && buffer.source_map_identity == trace.generated_source_map_identity
        && buffer.hygiene_mark == hygiene.generated_fresh_mark
        && buffer.token_stream_name == admission.token_stream_name
        && buffer.token_buffer_kind == token_buffer_kind_for_input(input)
        && buffer.token_producer_policy == token_producer_policy_for_input(input)
        && buffer.blocker_reason == token_buffer_blocker_for_input(input)
        && buffer.token_count == expected_token_count
        && buffer.empty != materialized_tokens
        && buffer.materialized_tokens == materialized_tokens;
}

[[nodiscard]] bool generated_token_record_matches_buffer(
    const GeneratedTokenRecord& record,
    const EarlyItemMacroInput& input,
    const GeneratedTokenBufferStub& buffer,
    const base::u32 token_index) noexcept
{
    if (buffer.token_count == 0) {
        return false;
    }
    const bool is_begin_token = token_index == 0;
    const bool is_end_token = static_cast<base::u64>(token_index) + 1U == buffer.token_count;
    if (is_begin_token) {
        if (record.kind != syntax::TokenKind::identifier
            || record.text != FRONTEND_MACRO_M21I_DERIVE_BEGIN_TOKEN_TEXT
            || record.token_role != FRONTEND_MACRO_M21I_DERIVE_BEGIN_TOKEN_ROLE
            || !source_ranges_equal(record.anchor_range, input.attribute_range)) {
            return false;
        }
    } else if (is_end_token) {
        if (record.kind != syntax::TokenKind::identifier
            || record.text != FRONTEND_MACRO_M21I_DERIVE_END_TOKEN_TEXT
            || record.token_role != FRONTEND_MACRO_M21I_DERIVE_END_TOKEN_ROLE
            || !source_ranges_equal(record.anchor_range, input.attribute_range)) {
            return false;
        }
    } else if (record.text != generated_source_token_text(token_index)
        || record.token_role != FRONTEND_MACRO_M21I_DERIVE_SOURCE_TOKEN_ROLE) {
        return false;
    }
    return record.item.value == input.item.value
        && record.module.value == input.module.value
        && record.part_index == input.part_index
        && record.attribute_index == input.attribute_index
        && record.token_index == token_index
        && record.token_buffer_identity == buffer.token_buffer_identity
        && record.token_identity == generated_token_record_identity(input, buffer, token_index, record.kind,
               record.text, record.token_role, record.anchor_range)
        && record.source_map_identity == buffer.source_map_identity
        && record.hygiene_mark == buffer.hygiene_mark
        && record.compiler_owned
        && !record.parser_visible
        && !record.produced_user_generated_code;
}

[[nodiscard]] bool generated_token_records_match_buffers(const EarlyItemExpansionResult& result) noexcept
{
    base::usize record_cursor = 0;
    for (base::usize input_index = 0; input_index < result.inputs.size(); ++input_index) {
        const EarlyItemMacroInput& input = result.inputs[input_index];
        const GeneratedTokenBufferStub& buffer = result.generated_token_buffers[input_index];
        const base::usize remaining_records = result.generated_token_records.size() - record_cursor;
        if (buffer.token_count > static_cast<base::u64>(remaining_records)) {
            return false;
        }
        for (base::u64 token_offset = 0; token_offset < buffer.token_count; ++token_offset) {
            if (record_cursor >= result.generated_token_records.size()) {
                return false;
            }
            if (token_offset > FRONTEND_MACRO_M21I_MAX_GENERATED_TOKEN_INDEX) {
                return false;
            }
            const base::u32 token_index = static_cast<base::u32>(token_offset);
            if (!generated_token_record_matches_buffer(
                    result.generated_token_records[record_cursor], input, buffer, token_index)) {
                return false;
            }
            ++record_cursor;
        }
    }
    return record_cursor == result.generated_token_records.size();
}

[[nodiscard]] bool parser_admission_gate_matches_input(
    const GeneratedTokenParserAdmissionGateStub& gate,
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const GeneratedModulePartParseMergeStub& parse_merge_stub,
    const GeneratedTokenBufferStub& buffer) noexcept
{
    const bool token_records_available = buffer.token_count > 0;
    return gate.item.value == input.item.value
        && gate.module.value == input.module.value
        && gate.part_index == input.part_index
        && gate.attribute_index == input.attribute_index
        && gate.attached_part == input.attached_part
        && gate.generated_part == placeholder.generated_part
        && gate.token_plan_identity == buffer.token_plan_identity
        && gate.token_buffer_identity == buffer.token_buffer_identity
        && gate.materialization_identity == buffer.materialization_identity
        && gate.source_map_identity == buffer.source_map_identity
        && gate.hygiene_mark == buffer.hygiene_mark
        && gate.generated_buffer_identity == parse_merge_stub.generated_buffer_identity
        && gate.parse_config_fingerprint == parse_merge_stub.parse_config_fingerprint
        && gate.parse_gate_identity == generated_token_parser_admission_gate_identity(
               input, placeholder, parse_merge_stub, buffer, token_records_available)
        && gate.token_stream_name == buffer.token_stream_name
        && gate.parser_gate_policy == FRONTEND_MACRO_M21J_PARSER_GATE_POLICY
        && gate.blocker_reason == parser_admission_blocker_for_input(input)
        && gate.token_count == buffer.token_count
        && gate.compiler_owned
        && gate.token_buffer_materialized == buffer.materialized_tokens
        && gate.token_records_available == token_records_available
        && !gate.parser_admitted
        && !gate.parse_ready
        && !gate.parser_consumable
        && !gate.generated_source_text
        && !gate.generated_part_parsed
        && !gate.generated_part_merged
        && !gate.sema_visible
        && !gate.produced_user_generated_code;
}

[[nodiscard]] bool parser_admission_diagnostic_matches_input(
    const ParserAdmissionDiagnosticProjectionStub& diagnostic,
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const GeneratedModulePartParseMergeStub& parse_merge_stub,
    const ExpansionTraceStub& trace,
    const GeneratedTokenBufferStub& buffer,
    const GeneratedTokenParserAdmissionGateStub& gate) noexcept
{
    const std::string expected_debug_projection_name = parser_admission_debug_projection_name(input);
    const query::StableFingerprint128 expected_anchor =
        parser_admission_diagnostic_anchor_identity(input, gate, trace);
    return diagnostic.item.value == input.item.value
        && diagnostic.module.value == input.module.value
        && diagnostic.part_index == input.part_index
        && diagnostic.attribute_index == input.attribute_index
        && diagnostic.attached_part == input.attached_part
        && diagnostic.generated_part == placeholder.generated_part
        && source_ranges_equal(diagnostic.primary_anchor, input.attribute_range)
        && source_ranges_equal(diagnostic.token_tree_anchor, input.token_tree_range)
        && diagnostic.parse_gate_identity == gate.parse_gate_identity
        && diagnostic.diagnostic_anchor_identity == expected_anchor
        && diagnostic.diagnostic_identity == parser_admission_diagnostic_identity(input, placeholder,
               parse_merge_stub, buffer, gate, trace, expected_anchor, expected_debug_projection_name)
        && diagnostic.token_plan_identity == gate.token_plan_identity
        && diagnostic.token_buffer_identity == gate.token_buffer_identity
        && diagnostic.materialization_identity == gate.materialization_identity
        && diagnostic.generated_buffer_identity == parse_merge_stub.generated_buffer_identity
        && diagnostic.parse_config_fingerprint == parse_merge_stub.parse_config_fingerprint
        && diagnostic.source_map_identity == buffer.source_map_identity
        && diagnostic.hygiene_mark == buffer.hygiene_mark
        && diagnostic.trace_identity == trace.trace_identity
        && diagnostic.diagnostic_policy == FRONTEND_MACRO_M21K_DIAGNOSTIC_POLICY
        && diagnostic.blocker_category == parser_admission_diagnostic_category_for_input(input)
        && diagnostic.token_buffer_blocker == parser_admission_blocker_for_input(input)
        && diagnostic.generated_part_parse_blocker == FRONTEND_MACRO_M21K_GENERATED_PART_PARSE_BLOCKER
        && diagnostic.user_message == parser_admission_diagnostic_message_for_input(input)
        && diagnostic.debug_projection_name == expected_debug_projection_name
        && diagnostic.token_count == gate.token_count
        && diagnostic.token_buffer_materialized == gate.token_buffer_materialized
        && diagnostic.token_records_available == gate.token_records_available
        && !diagnostic.parser_admitted
        && !diagnostic.parse_ready
        && !diagnostic.parser_consumable
        && !diagnostic.generated_part_parsed
        && !diagnostic.generated_part_merged
        && !diagnostic.emit_expanded_available
        && !diagnostic.debug_trace_available
        && !diagnostic.source_map_available
        && !diagnostic.produced_user_generated_code;
}

[[nodiscard]] bool parser_admission_report_entry_matches_diagnostic(
    const ParserAdmissionDiagnosticReportEntry& entry,
    const ParserAdmissionDiagnosticProjectionStub& diagnostic,
    const base::u32 report_index) noexcept
{
    const std::string expected_query_projection_name =
        parser_admission_report_query_name(diagnostic.module, diagnostic.part_index);
    return entry.item.value == diagnostic.item.value
        && entry.module.value == diagnostic.module.value
        && entry.part_index == diagnostic.part_index
        && entry.attribute_index == diagnostic.attribute_index
        && entry.report_index == report_index
        && entry.attached_part == diagnostic.attached_part
        && entry.generated_part == diagnostic.generated_part
        && source_ranges_equal(entry.primary_anchor, diagnostic.primary_anchor)
        && source_ranges_equal(entry.token_tree_anchor, diagnostic.token_tree_anchor)
        && entry.diagnostic_identity == diagnostic.diagnostic_identity
        && entry.diagnostic_anchor_identity == diagnostic.diagnostic_anchor_identity
        && entry.report_entry_identity == parser_admission_report_entry_identity(
               diagnostic, report_index, expected_query_projection_name)
        && entry.parse_gate_identity == diagnostic.parse_gate_identity
        && entry.blocker_category == diagnostic.blocker_category
        && entry.debug_projection_name == diagnostic.debug_projection_name
        && entry.query_projection_name == expected_query_projection_name
        && entry.token_count == diagnostic.token_count
        && entry.token_records_available == diagnostic.token_records_available
        && !entry.parser_admitted
        && entry.report_visible
        && entry.query_reusable
        && !entry.parser_consumable
        && !entry.emit_expanded_available
        && !entry.produced_user_generated_code;
}

[[nodiscard]] bool parser_admission_report_matches_group(
    const ParserAdmissionDiagnosticReport& report,
    const GeneratedModulePartPlaceholder& placeholder,
    const GeneratedModulePartParseMergeStub& parse_merge_stub,
    const std::vector<ParserAdmissionDiagnosticReportEntry>& entries) noexcept
{
    const std::string expected_query_name =
        parser_admission_report_query_name(placeholder.module, placeholder.source_part_index);
    const query::StableFingerprint128 expected_grouping_identity =
        parser_admission_report_grouping_identity(placeholder, entries);
    const query::StableFingerprint128 expected_anchor_identity =
        parser_admission_report_anchor_identity(placeholder, entries);
    base::u64 entry_count = 0;
    base::u64 blocked_count = 0;
    base::u64 derive_count = 0;
    base::u64 empty_count = 0;
    base::u64 token_record_available_count = 0;
    for (const ParserAdmissionDiagnosticReportEntry& entry : entries) {
        if (entry.module.value != placeholder.module.value
            || entry.part_index != placeholder.source_part_index) {
            continue;
        }
        ++entry_count;
        if (!entry.parser_admitted) {
            ++blocked_count;
        }
        if (entry.blocker_category == FRONTEND_MACRO_M21K_DERIVE_BLOCKER_CATEGORY) {
            ++derive_count;
        }
        if (entry.blocker_category == FRONTEND_MACRO_M21K_EMPTY_BLOCKER_CATEGORY) {
            ++empty_count;
        }
        if (entry.token_records_available) {
            ++token_record_available_count;
        }
    }
    return report.module.value == placeholder.module.value
        && report.source_part_index == placeholder.source_part_index
        && report.attached_part == placeholder.source_part
        && report.generated_part == placeholder.generated_part
        && report.report_identity == parser_admission_report_identity(placeholder, parse_merge_stub,
               expected_grouping_identity, expected_anchor_identity, expected_query_name, entries)
        && report.report_anchor_identity == expected_anchor_identity
        && report.report_grouping_identity == expected_grouping_identity
        && report.parse_config_fingerprint == parse_merge_stub.parse_config_fingerprint
        && report.generated_buffer_identity == parse_merge_stub.generated_buffer_identity
        && report.report_policy == FRONTEND_MACRO_M21L_REPORT_POLICY
        && report.report_query_name == expected_query_name
        && report.blocked_reason == FRONTEND_MACRO_M21L_REPORT_BLOCKER
        && report.entry_count == entry_count
        && report.blocked_entry_count == blocked_count
        && report.derive_entry_count == derive_count
        && report.empty_entry_count == empty_count
        && report.token_record_available_entry_count == token_record_available_count
        && report.query_reusable
        && report.report_visible
        && report.source_anchor_ordered == parser_admission_report_entries_are_ordered(
               entries, placeholder.module, placeholder.source_part_index)
        && !report.parser_admitted
        && !report.parse_ready
        && !report.parser_consumable
        && !report.emit_expanded_available
        && !report.debug_trace_available
        && !report.source_map_available
        && !report.produced_user_generated_code;
}

[[nodiscard]] bool parser_admission_report_entries_match_diagnostics(
    const EarlyItemExpansionResult& result) noexcept
{
    if (result.parser_admission_report_entries.size() != result.parser_admission_diagnostics.size()) {
        return false;
    }
    for (base::usize index = 0; index < result.parser_admission_report_entries.size(); ++index) {
        if (index > std::numeric_limits<base::u32>::max()) {
            return false;
        }
        const ParserAdmissionDiagnosticReportEntry& entry = result.parser_admission_report_entries[index];
        const ParserAdmissionDiagnosticProjectionStub& diagnostic = result.parser_admission_diagnostics[index];
        const ParserAdmissionDiagnosticProjectionStub* const found_diagnostic =
            find_parser_admission_diagnostic_for_entry(result.parser_admission_diagnostics, entry);
        const base::u32 report_index = static_cast<base::u32>(index);
        if (found_diagnostic != &diagnostic
            || !parser_admission_report_entry_matches_diagnostic(entry, diagnostic, report_index)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool parser_admission_reports_match_groups(const EarlyItemExpansionResult& result) noexcept
{
    if (result.parser_admission_reports.size() != result.generated_parts.size()) {
        return false;
    }
    for (base::usize index = 0; index < result.parser_admission_reports.size(); ++index) {
        const GeneratedModulePartPlaceholder& placeholder = result.generated_parts[index];
        const GeneratedModulePartParseMergeStub* const parse_merge_stub =
            find_parse_merge_stub_for(result.generated_part_stubs, placeholder.module,
                placeholder.source_part_index);
        if (parse_merge_stub == nullptr
            || !parser_admission_report_matches_group(result.parser_admission_reports[index],
                placeholder, *parse_merge_stub, result.parser_admission_report_entries)) {
            return false;
        }
    }
    return true;
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
        || result.inputs.size() != result.trace_stubs.size()
        || result.inputs.size() != result.generated_item_declarations.size()
        || result.inputs.size() != result.declared_generated_names.size()
        || result.inputs.size() != result.token_materialization_admissions.size()
        || result.inputs.size() != result.generated_token_buffers.size()
        || result.inputs.size() != result.parser_admission_gates.size()
        || result.inputs.size() != result.parser_admission_diagnostics.size()) {
        return false;
    }
    for (base::usize index = 0; index < result.inputs.size(); ++index) {
        const EarlyItemMacroInput& input = result.inputs[index];
        const GeneratedModulePartPlaceholder* const placeholder =
            find_generated_part_for(result.generated_parts, input.module, input.part_index);
        if (placeholder == nullptr) {
            return false;
        }
        const GeneratedModulePartParseMergeStub* const parse_merge_stub =
            find_parse_merge_stub_for(result.generated_part_stubs, input.module, input.part_index);
        if (parse_merge_stub == nullptr) {
            return false;
        }
        if (!source_map_matches_input(result.source_maps[index], input)
            || !hygiene_stub_matches_input(result.hygiene_stubs[index], input)
            || !trace_stub_matches_input(result.trace_stubs[index], input)
            || !generated_item_declaration_matches_input(result.generated_item_declarations[index],
                input, *placeholder, result.hygiene_stubs[index])
            || !declared_generated_name_matches_input(result.declared_generated_names[index],
                input, *placeholder, result.hygiene_stubs[index], result.generated_item_declarations[index])
            || !token_materialization_admission_matches_input(result.token_materialization_admissions[index],
                input, *placeholder, result.hygiene_stubs[index], result.trace_stubs[index],
                result.generated_item_declarations[index], result.declared_generated_names[index])
            || !generated_token_buffer_matches_input(result.generated_token_buffers[index],
                input, *placeholder, result.hygiene_stubs[index], result.trace_stubs[index],
                result.token_materialization_admissions[index])
            || !parser_admission_gate_matches_input(result.parser_admission_gates[index],
                input, *placeholder, *parse_merge_stub, result.generated_token_buffers[index])
            || !parser_admission_diagnostic_matches_input(result.parser_admission_diagnostics[index],
                input, *placeholder, *parse_merge_stub, result.trace_stubs[index],
                result.generated_token_buffers[index], result.parser_admission_gates[index])) {
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

bool is_valid(const GeneratedItemDeclarationStub& stub) noexcept
{
    return syntax::is_valid(stub.item)
        && syntax::is_valid(stub.module)
        && query::is_valid(stub.attached_part)
        && query::is_valid(stub.generated_part)
        && stub.generated_part.kind == query::ModulePartKind::generated
        && stub.generated_part.file.role == query::SourceRole::generated
        && is_nonzero_fingerprint(stub.expansion_origin)
        && is_nonzero_fingerprint(stub.declaration_identity)
        && is_nonzero_fingerprint(stub.declared_name_set)
        && is_nonzero_fingerprint(stub.generated_item_key)
        && stub.declaration_identity != stub.generated_item_key
        && stub.declaration_role == FRONTEND_MACRO_M21G_DECLARATION_ROLE
        && !stub.generated_item_name.empty()
        && stub.blocker_reason == FRONTEND_MACRO_M21G_DECLARATION_BLOCKER
        && stub.planned
        && !stub.materialized_tokens
        && !stub.parsed
        && !stub.merged
        && !stub.sema_visible
        && !stub.produced_user_generated_code;
}

bool is_valid(const DeclaredGeneratedNameStub& stub) noexcept
{
    return syntax::is_valid(stub.item)
        && syntax::is_valid(stub.module)
        && query::is_valid(stub.attached_part)
        && query::is_valid(stub.generated_part)
        && stub.generated_part.kind == query::ModulePartKind::generated
        && stub.generated_part.file.role == query::SourceRole::generated
        && is_nonzero_fingerprint(stub.expansion_origin)
        && is_nonzero_fingerprint(stub.declared_name_set)
        && is_nonzero_fingerprint(stub.declared_name_identity)
        && is_nonzero_fingerprint(stub.hygiene_mark)
        && !stub.declared_name.empty()
        && stub.namespace_kind == FRONTEND_MACRO_M21G_DECLARED_NAME_NAMESPACE
        && stub.blocker_reason == FRONTEND_MACRO_M21G_DECLARED_NAME_BLOCKER
        && !stub.lookup_visible
        && !stub.export_visible
        && !stub.sema_visible
        && !stub.produced_user_generated_code;
}

bool is_valid(const TokenMaterializationAdmissionStub& stub) noexcept
{
    return syntax::is_valid(stub.item)
        && syntax::is_valid(stub.module)
        && query::is_valid(stub.attached_part)
        && query::is_valid(stub.generated_part)
        && stub.generated_part.kind == query::ModulePartKind::generated
        && stub.generated_part.file.role == query::SourceRole::generated
        && is_nonzero_fingerprint(stub.expansion_origin)
        && is_nonzero_fingerprint(stub.declaration_identity)
        && is_nonzero_fingerprint(stub.generated_item_key)
        && is_nonzero_fingerprint(stub.declared_name_set)
        && is_nonzero_fingerprint(stub.declared_name_identity)
        && is_nonzero_fingerprint(stub.hygiene_mark)
        && is_nonzero_fingerprint(stub.source_map_identity)
        && is_nonzero_fingerprint(stub.trace_identity)
        && is_nonzero_fingerprint(stub.token_plan_identity)
        && is_nonzero_fingerprint(stub.token_buffer_identity)
        && stub.token_plan_identity != stub.token_buffer_identity
        && stub.admission_policy == FRONTEND_MACRO_M21H_ADMISSION_POLICY
        && !stub.token_stream_name.empty()
        && token_materialization_admission_state_is_valid(stub)
        && stub.compiler_owned
        && stub.admitted
        && !stub.generated_source_text
        && !stub.parse_ready
        && !stub.external_process_required
        && !stub.standard_library_required
        && !stub.runtime_required
        && !stub.produced_user_generated_code;
}

bool is_valid(const GeneratedTokenBufferStub& stub) noexcept
{
    return syntax::is_valid(stub.item)
        && syntax::is_valid(stub.module)
        && query::is_valid(stub.attached_part)
        && query::is_valid(stub.generated_part)
        && stub.generated_part.kind == query::ModulePartKind::generated
        && stub.generated_part.file.role == query::SourceRole::generated
        && is_nonzero_fingerprint(stub.token_plan_identity)
        && is_nonzero_fingerprint(stub.token_buffer_identity)
        && is_nonzero_fingerprint(stub.materialization_identity)
        && is_nonzero_fingerprint(stub.source_map_identity)
        && is_nonzero_fingerprint(stub.hygiene_mark)
        && stub.token_plan_identity != stub.token_buffer_identity
        && stub.materialization_identity != stub.token_buffer_identity
        && !stub.token_stream_name.empty()
        && token_buffer_kind_is_compiler_owned(stub.token_buffer_kind)
        && token_producer_policy_is_compiler_owned(stub.token_producer_policy)
        && generated_token_buffer_state_is_valid(stub)
        && !stub.generated_source_text
        && !stub.parser_consumable
        && !stub.produced_user_generated_code;
}

bool is_valid(const GeneratedTokenRecord& record) noexcept
{
    return syntax::is_valid(record.item)
        && syntax::is_valid(record.module)
        && is_nonzero_fingerprint(record.token_buffer_identity)
        && is_nonzero_fingerprint(record.token_identity)
        && is_nonzero_fingerprint(record.source_map_identity)
        && is_nonzero_fingerprint(record.hygiene_mark)
        && record.kind != syntax::TokenKind::invalid
        && record.kind != syntax::TokenKind::eof
        && !record.text.empty()
        && !record.token_role.empty()
        && source_range_is_well_formed(record.anchor_range)
        && record.compiler_owned
        && !record.parser_visible
        && !record.produced_user_generated_code;
}

bool is_valid(const GeneratedTokenParserAdmissionGateStub& stub) noexcept
{
    return syntax::is_valid(stub.item)
        && syntax::is_valid(stub.module)
        && query::is_valid(stub.attached_part)
        && query::is_valid(stub.generated_part)
        && stub.generated_part.kind == query::ModulePartKind::generated
        && stub.generated_part.file.role == query::SourceRole::generated
        && is_nonzero_fingerprint(stub.token_plan_identity)
        && is_nonzero_fingerprint(stub.token_buffer_identity)
        && is_nonzero_fingerprint(stub.materialization_identity)
        && is_nonzero_fingerprint(stub.source_map_identity)
        && is_nonzero_fingerprint(stub.hygiene_mark)
        && is_nonzero_fingerprint(stub.generated_buffer_identity)
        && is_nonzero_fingerprint(stub.parse_config_fingerprint)
        && is_nonzero_fingerprint(stub.parse_gate_identity)
        && stub.token_plan_identity != stub.token_buffer_identity
        && stub.materialization_identity != stub.token_buffer_identity
        && stub.parse_gate_identity != stub.token_buffer_identity
        && !stub.token_stream_name.empty()
        && stub.parser_gate_policy == FRONTEND_MACRO_M21J_PARSER_GATE_POLICY
        && (stub.blocker_reason == FRONTEND_MACRO_M21J_DERIVE_PARSE_BLOCKER
            || stub.blocker_reason == FRONTEND_MACRO_M21J_EMPTY_PARSE_BLOCKER)
        && stub.compiler_owned
        && stub.token_buffer_materialized == (stub.token_count > 0)
        && stub.token_records_available == (stub.token_count > 0)
        && !stub.parser_admitted
        && !stub.parse_ready
        && !stub.parser_consumable
        && !stub.generated_source_text
        && !stub.generated_part_parsed
        && !stub.generated_part_merged
        && !stub.sema_visible
        && !stub.produced_user_generated_code;
}

bool is_valid(const ParserAdmissionDiagnosticProjectionStub& stub) noexcept
{
    return syntax::is_valid(stub.item)
        && syntax::is_valid(stub.module)
        && query::is_valid(stub.attached_part)
        && query::is_valid(stub.generated_part)
        && stub.generated_part.kind == query::ModulePartKind::generated
        && stub.generated_part.file.role == query::SourceRole::generated
        && source_range_is_well_formed(stub.primary_anchor)
        && source_range_is_well_formed(stub.token_tree_anchor)
        && is_nonzero_fingerprint(stub.parse_gate_identity)
        && is_nonzero_fingerprint(stub.diagnostic_identity)
        && is_nonzero_fingerprint(stub.diagnostic_anchor_identity)
        && is_nonzero_fingerprint(stub.token_plan_identity)
        && is_nonzero_fingerprint(stub.token_buffer_identity)
        && is_nonzero_fingerprint(stub.materialization_identity)
        && is_nonzero_fingerprint(stub.generated_buffer_identity)
        && is_nonzero_fingerprint(stub.parse_config_fingerprint)
        && is_nonzero_fingerprint(stub.source_map_identity)
        && is_nonzero_fingerprint(stub.hygiene_mark)
        && is_nonzero_fingerprint(stub.trace_identity)
        && stub.token_plan_identity != stub.token_buffer_identity
        && stub.materialization_identity != stub.token_buffer_identity
        && stub.parse_gate_identity != stub.token_buffer_identity
        && stub.diagnostic_identity != stub.parse_gate_identity
        && stub.diagnostic_anchor_identity != stub.diagnostic_identity
        && stub.diagnostic_policy == FRONTEND_MACRO_M21K_DIAGNOSTIC_POLICY
        && parser_admission_diagnostic_category_is_valid(stub)
        && stub.generated_part_parse_blocker == FRONTEND_MACRO_M21K_GENERATED_PART_PARSE_BLOCKER
        && !stub.debug_projection_name.empty()
        && !stub.parser_admitted
        && !stub.parse_ready
        && !stub.parser_consumable
        && !stub.generated_part_parsed
        && !stub.generated_part_merged
        && !stub.emit_expanded_available
        && !stub.debug_trace_available
        && !stub.source_map_available
        && !stub.produced_user_generated_code;
}

bool is_valid(const ParserAdmissionDiagnosticReportEntry& entry) noexcept
{
    return syntax::is_valid(entry.item)
        && syntax::is_valid(entry.module)
        && query::is_valid(entry.attached_part)
        && query::is_valid(entry.generated_part)
        && entry.generated_part.kind == query::ModulePartKind::generated
        && entry.generated_part.file.role == query::SourceRole::generated
        && source_range_is_well_formed(entry.primary_anchor)
        && source_range_is_well_formed(entry.token_tree_anchor)
        && is_nonzero_fingerprint(entry.diagnostic_identity)
        && is_nonzero_fingerprint(entry.diagnostic_anchor_identity)
        && is_nonzero_fingerprint(entry.report_entry_identity)
        && is_nonzero_fingerprint(entry.parse_gate_identity)
        && entry.diagnostic_identity != entry.parse_gate_identity
        && entry.report_entry_identity != entry.diagnostic_identity
        && entry.diagnostic_anchor_identity != entry.diagnostic_identity
        && parser_admission_report_entry_category_is_valid(entry)
        && !entry.debug_projection_name.empty()
        && !entry.query_projection_name.empty()
        && !entry.parser_admitted
        && entry.report_visible
        && entry.query_reusable
        && !entry.parser_consumable
        && !entry.emit_expanded_available
        && !entry.produced_user_generated_code;
}

bool is_valid(const ParserAdmissionDiagnosticReport& report) noexcept
{
    return syntax::is_valid(report.module)
        && query::is_valid(report.attached_part)
        && query::is_valid(report.generated_part)
        && report.generated_part.kind == query::ModulePartKind::generated
        && report.generated_part.file.role == query::SourceRole::generated
        && is_nonzero_fingerprint(report.report_identity)
        && is_nonzero_fingerprint(report.report_anchor_identity)
        && is_nonzero_fingerprint(report.report_grouping_identity)
        && is_nonzero_fingerprint(report.parse_config_fingerprint)
        && is_nonzero_fingerprint(report.generated_buffer_identity)
        && report.report_identity != report.report_anchor_identity
        && report.report_identity != report.report_grouping_identity
        && report.report_policy == FRONTEND_MACRO_M21L_REPORT_POLICY
        && !report.report_query_name.empty()
        && report.blocked_reason == FRONTEND_MACRO_M21L_REPORT_BLOCKER
        && report.entry_count == report.blocked_entry_count
        && report.entry_count == report.derive_entry_count + report.empty_entry_count
        && report.token_record_available_entry_count <= report.entry_count
        && report.query_reusable
        && report.report_visible
        && report.source_anchor_ordered
        && !report.parser_admitted
        && !report.parse_ready
        && !report.parser_consumable
        && !report.emit_expanded_available
        && !report.debug_trace_available
        && !report.source_map_available
        && !report.produced_user_generated_code;
}

bool is_valid(const EarlyItemExpansionSummary& summary, const EarlyItemExpansionResult& result) noexcept
{
    return summary_equals(summary, summarize_early_item_expansion_counts(result));
}

bool is_valid(const EarlyItemExpansionResult& result) noexcept
{
    return std::string_view(result.name) == FRONTEND_MACRO_M21L_EXPANSION_NAME
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
        && std::all_of(result.generated_item_declarations.begin(), result.generated_item_declarations.end(),
               [](const GeneratedItemDeclarationStub& stub) {
                   return is_valid(stub);
               })
        && std::all_of(result.declared_generated_names.begin(), result.declared_generated_names.end(),
               [](const DeclaredGeneratedNameStub& stub) {
                   return is_valid(stub);
               })
        && std::all_of(result.token_materialization_admissions.begin(),
               result.token_materialization_admissions.end(),
               [](const TokenMaterializationAdmissionStub& stub) {
                   return is_valid(stub);
               })
        && std::all_of(result.generated_token_buffers.begin(), result.generated_token_buffers.end(),
               [](const GeneratedTokenBufferStub& stub) {
                   return is_valid(stub);
               })
        && std::all_of(result.generated_token_records.begin(), result.generated_token_records.end(),
               [](const GeneratedTokenRecord& record) {
                   return is_valid(record);
               })
        && std::all_of(result.parser_admission_gates.begin(), result.parser_admission_gates.end(),
               [](const GeneratedTokenParserAdmissionGateStub& stub) {
                   return is_valid(stub);
               })
        && std::all_of(result.parser_admission_diagnostics.begin(),
               result.parser_admission_diagnostics.end(),
               [](const ParserAdmissionDiagnosticProjectionStub& stub) {
                   return is_valid(stub);
               })
        && std::all_of(result.parser_admission_report_entries.begin(),
               result.parser_admission_report_entries.end(),
               [](const ParserAdmissionDiagnosticReportEntry& entry) {
                   return is_valid(entry);
               })
        && std::all_of(result.parser_admission_reports.begin(), result.parser_admission_reports.end(),
               [](const ParserAdmissionDiagnosticReport& report) {
                   return is_valid(report);
               })
        && per_input_stubs_match_inputs(result)
        && generated_token_records_match_buffers(result)
        && parser_admission_report_entries_match_diagnostics(result)
        && parser_admission_reports_match_groups(result)
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
    summary.generated_item_declaration_stub_count =
        static_cast<base::u64>(result.generated_item_declarations.size());
    for (const GeneratedItemDeclarationStub& stub : result.generated_item_declarations) {
        if (stub.planned) {
            ++summary.planned_generated_item_declaration_count;
        }
        if (stub.materialized_tokens) {
            ++summary.materialized_generated_item_count;
        }
        if (stub.parsed) {
            ++summary.parsed_generated_part_count;
        }
        if (stub.merged) {
            ++summary.merged_generated_part_count;
        }
        if (stub.sema_visible) {
            ++summary.sema_visible_generated_part_count;
        }
        if (stub.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.declared_generated_name_stub_count =
        static_cast<base::u64>(result.declared_generated_names.size());
    for (const DeclaredGeneratedNameStub& stub : result.declared_generated_names) {
        if (stub.lookup_visible) {
            ++summary.lookup_visible_declared_name_count;
        }
        if (stub.export_visible) {
            ++summary.export_visible_declared_name_count;
        }
        if (stub.sema_visible) {
            ++summary.sema_visible_generated_part_count;
        }
        if (stub.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.token_materialization_admission_stub_count =
        static_cast<base::u64>(result.token_materialization_admissions.size());
    for (const TokenMaterializationAdmissionStub& stub : result.token_materialization_admissions) {
        if (stub.compiler_owned) {
            ++summary.compiler_owned_admission_count;
        }
        if (stub.admitted) {
            ++summary.admitted_token_materialization_count;
        }
        if (stub.materialized_tokens) {
            ++summary.materialized_token_admission_count;
        }
        if (stub.generated_source_text) {
            ++summary.generated_source_text_count;
        }
        if (stub.parse_ready) {
            ++summary.parse_ready_token_buffer_count;
        }
        if (stub.external_process_required) {
            ++summary.external_process_required_count;
        }
        if (stub.standard_library_required) {
            ++summary.standard_library_required_count;
        }
        if (stub.runtime_required) {
            ++summary.runtime_required_count;
        }
        if (stub.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.generated_token_buffer_stub_count =
        static_cast<base::u64>(result.generated_token_buffers.size());
    for (const GeneratedTokenBufferStub& stub : result.generated_token_buffers) {
        if (token_buffer_kind_is_compiler_owned(stub.token_buffer_kind)
            && token_producer_policy_is_compiler_owned(stub.token_producer_policy)) {
            ++summary.compiler_owned_token_buffer_count;
        }
        if (stub.empty) {
            ++summary.empty_generated_token_buffer_count;
        }
        if (stub.materialized_tokens) {
            ++summary.materialized_token_buffer_count;
        }
        if (stub.generated_source_text) {
            ++summary.generated_source_text_count;
        }
        if (stub.parser_consumable) {
            ++summary.parse_ready_token_buffer_count;
        }
        if (stub.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.generated_token_record_count = static_cast<base::u64>(result.generated_token_records.size());
    for (const GeneratedTokenRecord& record : result.generated_token_records) {
        if (record.compiler_owned) {
            ++summary.compiler_owned_generated_token_record_count;
        }
        if (record.parser_visible) {
            ++summary.parser_visible_generated_token_count;
        }
        if (record.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.parser_admission_gate_stub_count = static_cast<base::u64>(result.parser_admission_gates.size());
    for (const GeneratedTokenParserAdmissionGateStub& stub : result.parser_admission_gates) {
        if (stub.compiler_owned) {
            ++summary.compiler_owned_parser_admission_gate_count;
        }
        if (stub.token_records_available) {
            ++summary.token_record_available_gate_count;
        }
        if (!stub.parser_admitted) {
            ++summary.parser_blocked_token_buffer_count;
        }
        if (stub.parser_admitted) {
            ++summary.parser_admitted_token_buffer_count;
        }
        if (stub.generated_source_text) {
            ++summary.generated_source_text_count;
        }
        if (stub.parse_ready || stub.parser_consumable) {
            ++summary.parse_ready_token_buffer_count;
        }
        if (stub.generated_part_parsed) {
            ++summary.parsed_generated_part_count;
        }
        if (stub.generated_part_merged) {
            ++summary.merged_generated_part_count;
        }
        if (stub.sema_visible) {
            ++summary.sema_visible_generated_part_count;
        }
        if (stub.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.parser_admission_diagnostic_stub_count =
        static_cast<base::u64>(result.parser_admission_diagnostics.size());
    for (const ParserAdmissionDiagnosticProjectionStub& stub : result.parser_admission_diagnostics) {
        if (!stub.parser_admitted) {
            ++summary.parser_admission_diagnostic_blocked_count;
        }
        if (stub.blocker_category == FRONTEND_MACRO_M21K_DERIVE_BLOCKER_CATEGORY) {
            ++summary.derive_parser_admission_diagnostic_count;
        }
        if (stub.blocker_category == FRONTEND_MACRO_M21K_EMPTY_BLOCKER_CATEGORY) {
            ++summary.empty_parser_admission_diagnostic_count;
        }
        if (stub.emit_expanded_available) {
            ++summary.emit_expanded_projection_available_count;
        }
        if (stub.debug_trace_available) {
            ++summary.parser_admission_debug_trace_projection_count;
        }
        if (stub.source_map_available) {
            ++summary.parser_admission_source_map_projection_count;
        }
        if (stub.parse_ready || stub.parser_consumable) {
            ++summary.parse_ready_token_buffer_count;
        }
        if (stub.generated_part_parsed) {
            ++summary.parsed_generated_part_count;
        }
        if (stub.generated_part_merged) {
            ++summary.merged_generated_part_count;
        }
        if (stub.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.parser_admission_report_entry_count =
        static_cast<base::u64>(result.parser_admission_report_entries.size());
    for (const ParserAdmissionDiagnosticReportEntry& entry : result.parser_admission_report_entries) {
        if (!entry.parser_admitted) {
            ++summary.parser_admission_report_blocked_entry_count;
        }
        if (entry.blocker_category == FRONTEND_MACRO_M21K_DERIVE_BLOCKER_CATEGORY) {
            ++summary.parser_admission_report_derive_entry_count;
        }
        if (entry.blocker_category == FRONTEND_MACRO_M21K_EMPTY_BLOCKER_CATEGORY) {
            ++summary.parser_admission_report_empty_entry_count;
        }
        if (entry.token_records_available) {
            ++summary.parser_admission_report_token_record_available_entry_count;
        }
        if (entry.emit_expanded_available) {
            ++summary.emit_expanded_projection_available_count;
        }
        if (entry.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.parser_admission_report_count =
        static_cast<base::u64>(result.parser_admission_reports.size());
    for (const ParserAdmissionDiagnosticReport& report : result.parser_admission_reports) {
        if (report.report_visible) {
            ++summary.parser_admission_report_visible_count;
        }
        if (report.query_reusable) {
            ++summary.parser_admission_report_query_reusable_count;
        }
        if (!report.source_anchor_ordered) {
            ++summary.parser_admission_report_unordered_anchor_count;
        }
        if (report.parser_consumable) {
            ++summary.parser_admission_report_parser_consumable_count;
            ++summary.parse_ready_token_buffer_count;
        }
        if (report.parse_ready) {
            ++summary.parse_ready_token_buffer_count;
        }
        if (report.emit_expanded_available) {
            ++summary.emit_expanded_projection_available_count;
        }
        if (report.debug_trace_available) {
            ++summary.parser_admission_debug_trace_projection_count;
        }
        if (report.source_map_available) {
            ++summary.parser_admission_source_map_projection_count;
        }
        if (report.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    return summary;
}

query::StableFingerprint128 early_item_expansion_fingerprint(
    const EarlyItemExpansionResult& result) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21L_EXPANSION_FINGERPRINT_MARKER);
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
    builder.mix_u64(static_cast<base::u64>(result.generated_item_declarations.size()));
    for (const GeneratedItemDeclarationStub& stub : result.generated_item_declarations) {
        mix_generated_item_declaration_stub(builder, stub);
    }
    builder.mix_u64(static_cast<base::u64>(result.declared_generated_names.size()));
    for (const DeclaredGeneratedNameStub& stub : result.declared_generated_names) {
        mix_declared_generated_name_stub(builder, stub);
    }
    builder.mix_u64(static_cast<base::u64>(result.token_materialization_admissions.size()));
    for (const TokenMaterializationAdmissionStub& stub : result.token_materialization_admissions) {
        mix_token_materialization_admission_stub(builder, stub);
    }
    builder.mix_u64(static_cast<base::u64>(result.generated_token_buffers.size()));
    for (const GeneratedTokenBufferStub& stub : result.generated_token_buffers) {
        mix_generated_token_buffer_stub(builder, stub);
    }
    builder.mix_u64(static_cast<base::u64>(result.generated_token_records.size()));
    for (const GeneratedTokenRecord& record : result.generated_token_records) {
        mix_generated_token_record(builder, record);
    }
    builder.mix_u64(static_cast<base::u64>(result.parser_admission_gates.size()));
    for (const GeneratedTokenParserAdmissionGateStub& stub : result.parser_admission_gates) {
        mix_generated_token_parser_admission_gate_stub(builder, stub);
    }
    builder.mix_u64(static_cast<base::u64>(result.parser_admission_diagnostics.size()));
    for (const ParserAdmissionDiagnosticProjectionStub& stub : result.parser_admission_diagnostics) {
        mix_parser_admission_diagnostic_projection_stub(builder, stub);
    }
    builder.mix_u64(static_cast<base::u64>(result.parser_admission_report_entries.size()));
    for (const ParserAdmissionDiagnosticReportEntry& entry : result.parser_admission_report_entries) {
        mix_parser_admission_report_entry(builder, entry);
    }
    builder.mix_u64(static_cast<base::u64>(result.parser_admission_reports.size()));
    for (const ParserAdmissionDiagnosticReport& report : result.parser_admission_reports) {
        mix_parser_admission_report(builder, report);
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
           << " generated_item_declarations=" << summary.generated_item_declaration_stub_count
           << " planned_generated_item_declarations="
           << summary.planned_generated_item_declaration_count
           << " materialized_generated_items=" << summary.materialized_generated_item_count
           << " declared_generated_names=" << summary.declared_generated_name_stub_count
           << " lookup_visible_declared_names=" << summary.lookup_visible_declared_name_count
           << " export_visible_declared_names=" << summary.export_visible_declared_name_count
           << " token_materialization_admissions="
           << summary.token_materialization_admission_stub_count
           << " compiler_owned_admissions=" << summary.compiler_owned_admission_count
           << " admitted_token_materializations="
           << summary.admitted_token_materialization_count
           << " materialized_token_admissions="
           << summary.materialized_token_admission_count
           << " generated_token_buffers=" << summary.generated_token_buffer_stub_count
           << " empty_generated_token_buffers="
           << summary.empty_generated_token_buffer_count
           << " materialized_token_buffers=" << summary.materialized_token_buffer_count
           << " compiler_owned_token_buffers=" << summary.compiler_owned_token_buffer_count
           << " generated_token_records=" << summary.generated_token_record_count
           << " compiler_owned_generated_token_records="
           << summary.compiler_owned_generated_token_record_count
           << " parser_visible_generated_tokens=" << summary.parser_visible_generated_token_count
           << " parser_admission_gates=" << summary.parser_admission_gate_stub_count
           << " compiler_owned_parser_admission_gates="
           << summary.compiler_owned_parser_admission_gate_count
           << " token_record_available_gates=" << summary.token_record_available_gate_count
           << " parser_blocked_token_buffers=" << summary.parser_blocked_token_buffer_count
           << " parser_admitted_token_buffers=" << summary.parser_admitted_token_buffer_count
           << " parser_admission_diagnostics=" << summary.parser_admission_diagnostic_stub_count
           << " parser_admission_diagnostics_blocked="
           << summary.parser_admission_diagnostic_blocked_count
           << " derive_parser_admission_diagnostics="
           << summary.derive_parser_admission_diagnostic_count
           << " empty_parser_admission_diagnostics="
           << summary.empty_parser_admission_diagnostic_count
           << " emit_expanded_projections="
           << summary.emit_expanded_projection_available_count
           << " parser_admission_debug_trace_projections="
           << summary.parser_admission_debug_trace_projection_count
           << " parser_admission_source_map_projections="
           << summary.parser_admission_source_map_projection_count
           << " parser_admission_report_entries="
           << summary.parser_admission_report_entry_count
           << " parser_admission_reports=" << summary.parser_admission_report_count
           << " parser_admission_report_blocked_entries="
           << summary.parser_admission_report_blocked_entry_count
           << " derive_parser_admission_report_entries="
           << summary.parser_admission_report_derive_entry_count
           << " empty_parser_admission_report_entries="
           << summary.parser_admission_report_empty_entry_count
           << " parser_admission_report_token_record_available_entries="
           << summary.parser_admission_report_token_record_available_entry_count
           << " parser_admission_report_visible="
           << summary.parser_admission_report_visible_count
           << " parser_admission_report_query_reusable="
           << summary.parser_admission_report_query_reusable_count
           << " parser_admission_report_unordered_anchors="
           << summary.parser_admission_report_unordered_anchor_count
           << " parser_admission_report_parser_consumable="
           << summary.parser_admission_report_parser_consumable_count
           << " generated_source_text=" << summary.generated_source_text_count
           << " parse_ready_token_buffers=" << summary.parse_ready_token_buffer_count
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
    for (base::usize index = 0; index < result.generated_item_declarations.size(); ++index) {
        const GeneratedItemDeclarationStub& stub = result.generated_item_declarations[index];
        stream << "  generated_item_declaration_stub #" << index
               << " item=" << stub.item.value
               << " module=" << stub.module.value
               << " part=" << stub.part_index
               << " attribute_index=" << stub.attribute_index
               << " role=" << stub.declaration_role
               << " name=" << stub.generated_item_name
               << " planned=" << (stub.planned ? "yes" : "no")
               << " materialized_tokens=" << (stub.materialized_tokens ? "yes" : "no")
               << " parsed=" << (stub.parsed ? "yes" : "no")
               << " merged=" << (stub.merged ? "yes" : "no")
               << " sema_visible=" << (stub.sema_visible ? "yes" : "no")
               << " user_generated_code=" << (stub.produced_user_generated_code ? "yes" : "no")
               << " blocker=" << stub.blocker_reason
               << " origin=" << query::debug_string(stub.expansion_origin)
               << " declaration_identity=" << query::debug_string(stub.declaration_identity)
               << " declared_name_set=" << query::debug_string(stub.declared_name_set)
               << " generated_item_key=" << query::debug_string(stub.generated_item_key) << '\n';
    }
    for (base::usize index = 0; index < result.declared_generated_names.size(); ++index) {
        const DeclaredGeneratedNameStub& stub = result.declared_generated_names[index];
        stream << "  declared_generated_name_stub #" << index
               << " item=" << stub.item.value
               << " module=" << stub.module.value
               << " part=" << stub.part_index
               << " attribute_index=" << stub.attribute_index
               << " namespace=" << stub.namespace_kind
               << " name=" << stub.declared_name
               << " lookup_visible=" << (stub.lookup_visible ? "yes" : "no")
               << " export_visible=" << (stub.export_visible ? "yes" : "no")
               << " sema_visible=" << (stub.sema_visible ? "yes" : "no")
               << " user_generated_code=" << (stub.produced_user_generated_code ? "yes" : "no")
               << " blocker=" << stub.blocker_reason
               << " origin=" << query::debug_string(stub.expansion_origin)
               << " declared_name_set=" << query::debug_string(stub.declared_name_set)
               << " declared_name_identity=" << query::debug_string(stub.declared_name_identity)
               << " hygiene_mark=" << query::debug_string(stub.hygiene_mark) << '\n';
    }
    for (base::usize index = 0; index < result.token_materialization_admissions.size(); ++index) {
        const TokenMaterializationAdmissionStub& stub = result.token_materialization_admissions[index];
        stream << "  token_materialization_admission_stub #" << index
               << " item=" << stub.item.value
               << " module=" << stub.module.value
               << " part=" << stub.part_index
               << " attribute_index=" << stub.attribute_index
               << " policy=" << stub.admission_policy
               << " token_stream=" << stub.token_stream_name
               << " compiler_owned=" << (stub.compiler_owned ? "yes" : "no")
               << " admitted=" << (stub.admitted ? "yes" : "no")
               << " materialized_tokens=" << (stub.materialized_tokens ? "yes" : "no")
               << " generated_source_text=" << (stub.generated_source_text ? "yes" : "no")
               << " parse_ready=" << (stub.parse_ready ? "yes" : "no")
               << " external_process_required=" << (stub.external_process_required ? "yes" : "no")
               << " standard_library_required=" << (stub.standard_library_required ? "yes" : "no")
               << " runtime_required=" << (stub.runtime_required ? "yes" : "no")
               << " user_generated_code=" << (stub.produced_user_generated_code ? "yes" : "no")
               << " blocker=" << stub.blocker_reason
               << " origin=" << query::debug_string(stub.expansion_origin)
               << " declaration_identity=" << query::debug_string(stub.declaration_identity)
               << " generated_item_key=" << query::debug_string(stub.generated_item_key)
               << " declared_name_set=" << query::debug_string(stub.declared_name_set)
               << " declared_name_identity=" << query::debug_string(stub.declared_name_identity)
               << " hygiene_mark=" << query::debug_string(stub.hygiene_mark)
               << " source_map_identity=" << query::debug_string(stub.source_map_identity)
               << " trace_identity=" << query::debug_string(stub.trace_identity)
               << " token_plan_identity=" << query::debug_string(stub.token_plan_identity)
               << " token_buffer_identity=" << query::debug_string(stub.token_buffer_identity) << '\n';
    }
    for (base::usize index = 0; index < result.generated_token_buffers.size(); ++index) {
        const GeneratedTokenBufferStub& stub = result.generated_token_buffers[index];
        stream << "  generated_token_buffer_stub #" << index
               << " item=" << stub.item.value
               << " module=" << stub.module.value
               << " part=" << stub.part_index
               << " attribute_index=" << stub.attribute_index
               << " token_stream=" << stub.token_stream_name
               << " kind=" << stub.token_buffer_kind
               << " producer=" << stub.token_producer_policy
               << " token_count=" << stub.token_count
               << " empty=" << (stub.empty ? "yes" : "no")
               << " materialized_tokens=" << (stub.materialized_tokens ? "yes" : "no")
               << " generated_source_text=" << (stub.generated_source_text ? "yes" : "no")
               << " parser_consumable=" << (stub.parser_consumable ? "yes" : "no")
               << " user_generated_code=" << (stub.produced_user_generated_code ? "yes" : "no")
               << " blocker=" << stub.blocker_reason
               << " token_plan_identity=" << query::debug_string(stub.token_plan_identity)
               << " token_buffer_identity=" << query::debug_string(stub.token_buffer_identity)
               << " materialization_identity=" << query::debug_string(stub.materialization_identity)
               << " source_map_identity=" << query::debug_string(stub.source_map_identity)
               << " hygiene_mark=" << query::debug_string(stub.hygiene_mark) << '\n';
    }
    for (base::usize index = 0; index < result.generated_token_records.size(); ++index) {
        const GeneratedTokenRecord& record = result.generated_token_records[index];
        stream << "  generated_token_record #" << index
               << " item=" << record.item.value
               << " module=" << record.module.value
               << " part=" << record.part_index
               << " attribute_index=" << record.attribute_index
               << " token_index=" << record.token_index
               << " kind=" << syntax::token_kind_name(record.kind)
               << " text=" << record.text
               << " role=" << record.token_role
               << " compiler_owned=" << (record.compiler_owned ? "yes" : "no")
               << " parser_visible=" << (record.parser_visible ? "yes" : "no")
               << " user_generated_code=" << (record.produced_user_generated_code ? "yes" : "no")
               << " token_identity=" << query::debug_string(record.token_identity)
               << " token_buffer_identity=" << query::debug_string(record.token_buffer_identity)
               << " source_map_identity=" << query::debug_string(record.source_map_identity)
               << " hygiene_mark=" << query::debug_string(record.hygiene_mark) << '\n';
    }
    for (base::usize index = 0; index < result.parser_admission_gates.size(); ++index) {
        const GeneratedTokenParserAdmissionGateStub& stub = result.parser_admission_gates[index];
        stream << "  generated_token_parser_admission_gate_stub #" << index
               << " item=" << stub.item.value
               << " module=" << stub.module.value
               << " part=" << stub.part_index
               << " attribute_index=" << stub.attribute_index
               << " token_stream=" << stub.token_stream_name
               << " policy=" << stub.parser_gate_policy
               << " token_count=" << stub.token_count
               << " compiler_owned=" << (stub.compiler_owned ? "yes" : "no")
               << " token_buffer_materialized=" << (stub.token_buffer_materialized ? "yes" : "no")
               << " token_records_available=" << (stub.token_records_available ? "yes" : "no")
               << " parser_admitted=" << (stub.parser_admitted ? "yes" : "no")
               << " parse_ready=" << (stub.parse_ready ? "yes" : "no")
               << " parser_consumable=" << (stub.parser_consumable ? "yes" : "no")
               << " generated_source_text=" << (stub.generated_source_text ? "yes" : "no")
               << " generated_part_parsed=" << (stub.generated_part_parsed ? "yes" : "no")
               << " generated_part_merged=" << (stub.generated_part_merged ? "yes" : "no")
               << " sema_visible=" << (stub.sema_visible ? "yes" : "no")
               << " user_generated_code=" << (stub.produced_user_generated_code ? "yes" : "no")
               << " blocker=" << stub.blocker_reason
               << " token_plan_identity=" << query::debug_string(stub.token_plan_identity)
               << " token_buffer_identity=" << query::debug_string(stub.token_buffer_identity)
               << " materialization_identity=" << query::debug_string(stub.materialization_identity)
               << " source_map_identity=" << query::debug_string(stub.source_map_identity)
               << " hygiene_mark=" << query::debug_string(stub.hygiene_mark)
               << " generated_buffer_identity=" << query::debug_string(stub.generated_buffer_identity)
               << " parse_config=" << query::debug_string(stub.parse_config_fingerprint)
               << " parse_gate_identity=" << query::debug_string(stub.parse_gate_identity) << '\n';
    }
    for (base::usize index = 0; index < result.parser_admission_diagnostics.size(); ++index) {
        const ParserAdmissionDiagnosticProjectionStub& stub = result.parser_admission_diagnostics[index];
        stream << "  parser_admission_diagnostic_projection_stub #" << index
               << " item=" << stub.item.value
               << " module=" << stub.module.value
               << " part=" << stub.part_index
               << " attribute_index=" << stub.attribute_index
               << " policy=" << stub.diagnostic_policy
               << " category=" << stub.blocker_category
               << " debug_projection=" << stub.debug_projection_name
               << " primary_anchor=" << stub.primary_anchor.source.value << ':'
               << stub.primary_anchor.begin << ':' << stub.primary_anchor.end
               << " token_tree_anchor=" << stub.token_tree_anchor.source.value << ':'
               << stub.token_tree_anchor.begin << ':' << stub.token_tree_anchor.end
               << " token_count=" << stub.token_count
               << " token_buffer_materialized="
               << (stub.token_buffer_materialized ? "yes" : "no")
               << " token_records_available="
               << (stub.token_records_available ? "yes" : "no")
               << " parser_admitted=" << (stub.parser_admitted ? "yes" : "no")
               << " parse_ready=" << (stub.parse_ready ? "yes" : "no")
               << " parser_consumable=" << (stub.parser_consumable ? "yes" : "no")
               << " generated_part_parsed="
               << (stub.generated_part_parsed ? "yes" : "no")
               << " generated_part_merged="
               << (stub.generated_part_merged ? "yes" : "no")
               << " emit_expanded_available="
               << (stub.emit_expanded_available ? "yes" : "no")
               << " debug_trace_available="
               << (stub.debug_trace_available ? "yes" : "no")
               << " source_map_available="
               << (stub.source_map_available ? "yes" : "no")
               << " user_generated_code="
               << (stub.produced_user_generated_code ? "yes" : "no")
               << " token_buffer_blocker=" << stub.token_buffer_blocker
               << " generated_part_parse_blocker=" << stub.generated_part_parse_blocker
               << " message=" << stub.user_message
               << " parse_gate_identity=" << query::debug_string(stub.parse_gate_identity)
               << " diagnostic_identity=" << query::debug_string(stub.diagnostic_identity)
               << " diagnostic_anchor="
               << query::debug_string(stub.diagnostic_anchor_identity)
               << " token_plan_identity=" << query::debug_string(stub.token_plan_identity)
               << " token_buffer_identity=" << query::debug_string(stub.token_buffer_identity)
               << " materialization_identity="
               << query::debug_string(stub.materialization_identity)
               << " generated_buffer_identity="
               << query::debug_string(stub.generated_buffer_identity)
               << " parse_config=" << query::debug_string(stub.parse_config_fingerprint)
               << " source_map_identity=" << query::debug_string(stub.source_map_identity)
               << " hygiene_mark=" << query::debug_string(stub.hygiene_mark)
               << " trace_identity=" << query::debug_string(stub.trace_identity) << '\n';
    }
    for (base::usize index = 0; index < result.parser_admission_report_entries.size(); ++index) {
        const ParserAdmissionDiagnosticReportEntry& entry = result.parser_admission_report_entries[index];
        stream << "  parser_admission_report_entry #" << index
               << " item=" << entry.item.value
               << " module=" << entry.module.value
               << " part=" << entry.part_index
               << " attribute_index=" << entry.attribute_index
               << " report_index=" << entry.report_index
               << " category=" << entry.blocker_category
               << " debug_projection=" << entry.debug_projection_name
               << " query_projection=" << entry.query_projection_name
               << " primary_anchor=" << entry.primary_anchor.source.value << ':'
               << entry.primary_anchor.begin << ':' << entry.primary_anchor.end
               << " token_tree_anchor=" << entry.token_tree_anchor.source.value << ':'
               << entry.token_tree_anchor.begin << ':' << entry.token_tree_anchor.end
               << " token_count=" << entry.token_count
               << " token_records_available="
               << (entry.token_records_available ? "yes" : "no")
               << " parser_admitted=" << (entry.parser_admitted ? "yes" : "no")
               << " report_visible=" << (entry.report_visible ? "yes" : "no")
               << " query_reusable=" << (entry.query_reusable ? "yes" : "no")
               << " parser_consumable=" << (entry.parser_consumable ? "yes" : "no")
               << " emit_expanded_available="
               << (entry.emit_expanded_available ? "yes" : "no")
               << " user_generated_code="
               << (entry.produced_user_generated_code ? "yes" : "no")
               << " parse_gate_identity=" << query::debug_string(entry.parse_gate_identity)
               << " diagnostic_identity=" << query::debug_string(entry.diagnostic_identity)
               << " diagnostic_anchor="
               << query::debug_string(entry.diagnostic_anchor_identity)
               << " report_entry_identity="
               << query::debug_string(entry.report_entry_identity) << '\n';
    }
    for (base::usize index = 0; index < result.parser_admission_reports.size(); ++index) {
        const ParserAdmissionDiagnosticReport& report = result.parser_admission_reports[index];
        stream << "  parser_admission_diagnostic_report #" << index
               << " module=" << report.module.value
               << " source_part=" << report.source_part_index
               << " policy=" << report.report_policy
               << " query=" << report.report_query_name
               << " entries=" << report.entry_count
               << " blocked_entries=" << report.blocked_entry_count
               << " derive_entries=" << report.derive_entry_count
               << " empty_entries=" << report.empty_entry_count
               << " token_record_available_entries="
               << report.token_record_available_entry_count
               << " query_reusable=" << (report.query_reusable ? "yes" : "no")
               << " report_visible=" << (report.report_visible ? "yes" : "no")
               << " source_anchor_ordered="
               << (report.source_anchor_ordered ? "yes" : "no")
               << " parser_admitted=" << (report.parser_admitted ? "yes" : "no")
               << " parse_ready=" << (report.parse_ready ? "yes" : "no")
               << " parser_consumable=" << (report.parser_consumable ? "yes" : "no")
               << " emit_expanded_available="
               << (report.emit_expanded_available ? "yes" : "no")
               << " debug_trace_available="
               << (report.debug_trace_available ? "yes" : "no")
               << " source_map_available="
               << (report.source_map_available ? "yes" : "no")
               << " user_generated_code="
               << (report.produced_user_generated_code ? "yes" : "no")
               << " blocker=" << report.blocked_reason
               << " report_identity=" << query::debug_string(report.report_identity)
               << " report_anchor_identity="
               << query::debug_string(report.report_anchor_identity)
               << " report_grouping_identity="
               << query::debug_string(report.report_grouping_identity)
               << " generated_buffer_identity="
               << query::debug_string(report.generated_buffer_identity)
               << " parse_config=" << query::debug_string(report.parse_config_fingerprint)
               << '\n';
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
    result.name = std::string(FRONTEND_MACRO_M21L_EXPANSION_NAME);
    result.plan = plan;
    const base::usize attribute_count = count_item_attributes(ast);
    result.inputs.reserve(attribute_count);
    result.generated_parts.reserve(ast.items.size());
    result.generated_part_stubs.reserve(ast.items.size());
    result.source_maps.reserve(attribute_count);
    result.hygiene_stubs.reserve(attribute_count);
    result.trace_stubs.reserve(attribute_count);
    result.generated_item_declarations.reserve(attribute_count);
    result.declared_generated_names.reserve(attribute_count);
    result.token_materialization_admissions.reserve(attribute_count);
    result.generated_token_buffers.reserve(attribute_count);
    result.generated_token_records.reserve(count_compiler_owned_generated_token_records(ast));
    result.parser_admission_gates.reserve(attribute_count);
    result.parser_admission_diagnostics.reserve(attribute_count);
    result.parser_admission_report_entries.reserve(attribute_count);
    result.parser_admission_reports.reserve(ast.items.size());

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
        const GeneratedModulePartPlaceholder* const generated_part =
            find_generated_part_for(result.generated_parts, module, part_index);
        if (generated_part == nullptr) {
            return base::Result<EarlyItemExpansionResult>::fail(
                internal_error(FRONTEND_MACRO_M21G_MISSING_GENERATED_PART));
        }
        const GeneratedModulePartParseMergeStub* const parse_merge_stub =
            find_parse_merge_stub_for(result.generated_part_stubs, module, part_index);
        if (parse_merge_stub == nullptr) {
            return base::Result<EarlyItemExpansionResult>::fail(
                internal_error(FRONTEND_MACRO_M21J_MISSING_PARSE_MERGE_STUB));
        }

        for (base::usize attribute_index = 0; attribute_index < item.attributes.size(); ++attribute_index) {
            const syntax::AttributeDecl& attribute = item.attributes[attribute_index];
            EarlyItemMacroInput input = make_macro_input(ast, item_id,
                base::checked_u32(attribute_index, "early item macro attribute index"), attribute, attached_part);
            ExpansionHygieneStub hygiene = make_hygiene_stub(input);
            ExpansionTraceStub trace = make_trace_stub(input);
            GeneratedItemDeclarationStub declaration =
                make_generated_item_declaration_stub(input, *generated_part, hygiene);
            DeclaredGeneratedNameStub declared_name =
                make_declared_generated_name_stub(input, *generated_part, hygiene, declaration);
            TokenMaterializationAdmissionStub admission = make_token_materialization_admission_stub(
                input, *generated_part, hygiene, trace, declaration, declared_name);
            GeneratedTokenBufferStub token_buffer = make_generated_token_buffer_stub(
                input, *generated_part, hygiene, trace, admission);
            GeneratedTokenParserAdmissionGateStub parser_admission_gate =
                make_generated_token_parser_admission_gate_stub(input, *generated_part, *parse_merge_stub,
                    token_buffer);
            ParserAdmissionDiagnosticProjectionStub parser_admission_diagnostic =
                make_parser_admission_diagnostic_projection_stub(input, *generated_part, *parse_merge_stub,
                    trace, token_buffer, parser_admission_gate);
            append_generated_token_records_for_attribute(result.generated_token_records, input, attribute,
                token_buffer);
            result.source_maps.push_back(make_source_map_placeholder(input));
            result.hygiene_stubs.push_back(std::move(hygiene));
            result.trace_stubs.push_back(std::move(trace));
            result.generated_item_declarations.push_back(std::move(declaration));
            result.declared_generated_names.push_back(std::move(declared_name));
            result.token_materialization_admissions.push_back(std::move(admission));
            result.generated_token_buffers.push_back(std::move(token_buffer));
            result.parser_admission_gates.push_back(std::move(parser_admission_gate));
            result.parser_admission_diagnostics.push_back(std::move(parser_admission_diagnostic));
            result.inputs.push_back(std::move(input));
        }
    }

    for (base::usize index = 0; index < result.parser_admission_diagnostics.size(); ++index) {
        result.parser_admission_report_entries.push_back(make_parser_admission_report_entry(
            result.parser_admission_diagnostics[index],
            base::checked_u32(index, "parser admission diagnostic report entry index")));
    }
    for (const GeneratedModulePartPlaceholder& placeholder : result.generated_parts) {
        const GeneratedModulePartParseMergeStub* const parse_merge_stub =
            find_parse_merge_stub_for(result.generated_part_stubs, placeholder.module,
                placeholder.source_part_index);
        if (parse_merge_stub == nullptr) {
            return base::Result<EarlyItemExpansionResult>::fail(
                internal_error(FRONTEND_MACRO_M21J_MISSING_PARSE_MERGE_STUB));
        }
        result.parser_admission_reports.push_back(
            make_parser_admission_report(placeholder, *parse_merge_stub,
                result.parser_admission_report_entries));
    }

    result.summary = summarize_early_item_expansion_counts(result);
    result.fingerprint = early_item_expansion_fingerprint(result);
    return base::Result<EarlyItemExpansionResult>::ok(std::move(result));
}

} // namespace aurex::frontend::macro
