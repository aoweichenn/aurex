#include "frontend/macro/detail/output_contract/output_contract_admission_detail.hpp"

#include <string>

namespace aurex::frontend::macro::detail {

bool source_range_is_well_formed(const base::SourceRange& range) noexcept
{
    return range.well_formed();
}

bool is_nonzero_fingerprint(const query::StableFingerprint128 fingerprint) noexcept
{
    return fingerprint != query::StableFingerprint128{};
}

[[nodiscard]] std::string build_output_name(
    const std::string_view prefix,
    const syntax::ModuleId module,
    const base::u32 part_index,
    const syntax::ItemId consumer_item,
    const syntax::ItemId macro_item,
    const base::u32 output_index,
    const std::string_view macro_name)
{
    std::string name(prefix);
    name += std::to_string(module.value);
    name.push_back(':');
    name += std::to_string(part_index);
    name.push_back(':');
    name += std::to_string(consumer_item.value);
    name.push_back(':');
    name += std::to_string(macro_item.value);
    name.push_back(':');
    name += std::to_string(output_index);
    name.push_back(':');
    name += macro_name;
    return name;
}

std::string macro_output_contract_query_name(
    const syntax::ModuleId module,
    const base::u32 part_index,
    const syntax::ItemId consumer_item,
    const syntax::ItemId macro_item,
    const base::u32 output_index,
    const std::string_view macro_name)
{
    return build_output_name(FRONTEND_MACRO_M27D_OUTPUT_CONTRACT_QUERY_PREFIX, module, part_index,
        consumer_item, macro_item, output_index, macro_name);
}

std::string macro_output_declared_name_policy_query_name(
    const syntax::ModuleId module,
    const base::u32 part_index,
    const syntax::ItemId consumer_item,
    const syntax::ItemId macro_item,
    const base::u32 output_index,
    const std::string_view macro_name)
{
    return build_output_name(FRONTEND_MACRO_M27D_OUTPUT_DECLARED_NAME_QUERY_PREFIX, module,
        part_index, consumer_item, macro_item, output_index, macro_name);
}

std::string macro_output_diagnostic_projection_query_name(
    const syntax::ModuleId module,
    const base::u32 part_index,
    const syntax::ItemId consumer_item,
    const syntax::ItemId macro_item,
    const base::u32 output_index,
    const std::string_view macro_name)
{
    return build_output_name(FRONTEND_MACRO_M27D_OUTPUT_DIAGNOSTIC_QUERY_PREFIX, module, part_index,
        consumer_item, macro_item, output_index, macro_name);
}

std::string macro_output_token_buffer_name(
    const syntax::ModuleId module,
    const base::u32 part_index,
    const syntax::ItemId consumer_item,
    const syntax::ItemId macro_item,
    const base::u32 output_index,
    const std::string_view macro_name)
{
    return build_output_name(FRONTEND_MACRO_M27D_OUTPUT_TOKEN_BUFFER_PREFIX, module, part_index,
        consumer_item, macro_item, output_index, macro_name);
}

query::ModulePartKey macro_output_generated_module_part_key(
    const query::ModulePartKey source_part,
    const syntax::ModuleId module,
    const base::u32 part_index,
    const syntax::ItemId consumer_item,
    const syntax::ItemId macro_item,
    const base::u32 output_index,
    const std::string_view macro_name)
{
    const std::string name = macro_output_token_buffer_name(module, part_index, consumer_item,
        macro_item, output_index, macro_name);
    const query::FileKey generated_file = query::file_key(
        source_part.module.package, name, query::SourceRole::generated, name);
    return query::module_part_key(source_part.module, generated_file, query::ModulePartKind::generated,
        name, output_index);
}

query::StableFingerprint128 macro_output_source_map_identity(
    const syntax::ModuleId module,
    const base::u32 part_index,
    const syntax::ItemId consumer_item,
    const syntax::ItemId macro_item,
    const base::u32 output_index,
    const query::StableFingerprint128 source_admission_identity,
    const base::SourceRange& source_range) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M27D_OUTPUT_SOURCE_MAP_MARKER);
    builder.mix_u32(module.value);
    builder.mix_u32(part_index);
    builder.mix_u32(consumer_item.value);
    builder.mix_u32(macro_item.value);
    builder.mix_u32(output_index);
    builder.mix_fingerprint(source_admission_identity);
    builder.mix_u64(static_cast<base::u64>(source_range.source.value));
    builder.mix_u64(static_cast<base::u64>(source_range.begin));
    builder.mix_u64(static_cast<base::u64>(source_range.end));
    return builder.finish();
}

query::StableFingerprint128 macro_output_hygiene_mark_identity(
    const syntax::ModuleId module,
    const base::u32 part_index,
    const syntax::ItemId consumer_item,
    const syntax::ItemId macro_item,
    const base::u32 output_index,
    const query::StableFingerprint128 surface_admission_identity,
    const query::StableFingerprint128 source_admission_identity,
    const query::StableFingerprint128 matcher_identity,
    const query::StableFingerprint128 target_schema_identity) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M27D_OUTPUT_HYGIENE_MARK_MARKER);
    builder.mix_u32(module.value);
    builder.mix_u32(part_index);
    builder.mix_u32(consumer_item.value);
    builder.mix_u32(macro_item.value);
    builder.mix_u32(output_index);
    builder.mix_fingerprint(surface_admission_identity);
    builder.mix_fingerprint(source_admission_identity);
    builder.mix_fingerprint(matcher_identity);
    builder.mix_fingerprint(target_schema_identity);
    return builder.finish();
}

query::StableFingerprint128 macro_output_token_buffer_identity(
    const AurexMacroOutputContractAdmissionGate& gate) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M27D_OUTPUT_TOKEN_BUFFER_MARKER);
    builder.mix_u32(gate.consumer_item.value);
    builder.mix_u32(gate.macro_item.value);
    builder.mix_u32(gate.module.value);
    builder.mix_u32(gate.part_index);
    builder.mix_u32(gate.output_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(gate.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(gate.generated_part));
    builder.mix_fingerprint(gate.surface_admission_identity);
    builder.mix_fingerprint(gate.source_admission_identity);
    builder.mix_fingerprint(gate.hygiene_mark);
    builder.mix_fingerprint(gate.source_map_identity);
    builder.mix_string(gate.token_buffer_name);
    builder.mix_u64(gate.planned_token_count);
    builder.mix_bool(gate.compiler_owned_output);
    return builder.finish();
}

query::StableFingerprint128 macro_output_declared_name_set_fingerprint(
    const AurexMacroOutputContractAdmissionGate& gate) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M27D_OUTPUT_DECLARED_NAME_SET_MARKER);
    builder.mix_u32(gate.consumer_item.value);
    builder.mix_u32(gate.macro_item.value);
    builder.mix_u32(gate.module.value);
    builder.mix_u32(gate.part_index);
    builder.mix_u32(gate.output_index);
    builder.mix_fingerprint(gate.output_contract_identity);
    builder.mix_fingerprint(gate.hygiene_mark);
    builder.mix_u8(static_cast<base::u8>(gate.origin_kind));
    builder.mix_string(gate.macro_name);
    builder.mix_string(gate.consumer_name);
    return builder.finish();
}

query::StableFingerprint128 macro_output_declared_name_policy_identity(
    const AurexMacroOutputDeclaredNamePolicyAdmissionGate& gate) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M27D_OUTPUT_DECLARED_NAME_POLICY_MARKER);
    builder.mix_u32(gate.consumer_item.value);
    builder.mix_u32(gate.macro_item.value);
    builder.mix_u32(gate.module.value);
    builder.mix_u32(gate.part_index);
    builder.mix_u32(gate.output_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(gate.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(gate.generated_part));
    builder.mix_fingerprint(gate.output_contract_identity);
    builder.mix_fingerprint(gate.declared_name_set_fingerprint);
    builder.mix_fingerprint(gate.hygiene_mark);
    builder.mix_fingerprint(gate.diagnostic_anchor_identity);
    builder.mix_u8(static_cast<base::u8>(gate.origin_kind));
    builder.mix_u8(static_cast<base::u8>(gate.macro_kind));
    builder.mix_string(gate.macro_name);
    builder.mix_string(gate.consumer_name);
    builder.mix_string(gate.declared_name_policy);
    builder.mix_string(gate.declared_name_namespace);
    builder.mix_string(gate.query_name);
    builder.mix_u64(gate.planned_declared_name_count);
    builder.mix_bool(gate.declared_name_set_reserved);
    builder.mix_bool(gate.lookup_visible);
    builder.mix_bool(gate.export_visible);
    builder.mix_bool(gate.sema_visible);
    return builder.finish();
}

query::StableFingerprint128 macro_output_contract_identity(
    const AurexMacroOutputContractAdmissionGate& gate) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M27D_OUTPUT_CONTRACT_MARKER);
    builder.mix_u32(gate.consumer_item.value);
    builder.mix_u32(gate.macro_item.value);
    builder.mix_u32(gate.module.value);
    builder.mix_u32(gate.part_index);
    builder.mix_u32(gate.output_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(gate.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(gate.generated_part));
    builder.mix_fingerprint(gate.surface_admission_identity);
    builder.mix_fingerprint(gate.source_admission_identity);
    builder.mix_fingerprint(gate.matcher_identity);
    builder.mix_fingerprint(gate.target_schema_identity);
    builder.mix_fingerprint(gate.hygiene_mark);
    builder.mix_fingerprint(gate.diagnostic_anchor_identity);
    builder.mix_fingerprint(gate.source_map_identity);
    builder.mix_fingerprint(gate.token_buffer_identity);
    builder.mix_u8(static_cast<base::u8>(gate.origin_kind));
    builder.mix_u8(static_cast<base::u8>(gate.macro_kind));
    builder.mix_string(gate.macro_name);
    builder.mix_string(gate.consumer_name);
    builder.mix_string(gate.output_policy);
    builder.mix_string(gate.query_name);
    builder.mix_string(gate.token_buffer_name);
    builder.mix_u64(gate.planned_token_count);
    builder.mix_bool(gate.compiler_owned_output);
    builder.mix_bool(gate.token_buffer_materialized);
    builder.mix_bool(gate.generated_source_text);
    builder.mix_bool(gate.parse_ready);
    builder.mix_bool(gate.parser_consumable);
    return builder.finish();
}

query::StableFingerprint128 macro_output_diagnostic_projection_identity(
    const AurexMacroOutputDiagnosticProjectionAdmissionGate& gate) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M27D_OUTPUT_DIAGNOSTIC_PROJECTION_MARKER);
    builder.mix_u32(gate.consumer_item.value);
    builder.mix_u32(gate.macro_item.value);
    builder.mix_u32(gate.module.value);
    builder.mix_u32(gate.part_index);
    builder.mix_u32(gate.output_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(gate.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(gate.generated_part));
    builder.mix_fingerprint(gate.output_contract_identity);
    builder.mix_fingerprint(gate.token_buffer_identity);
    builder.mix_fingerprint(gate.diagnostic_anchor_identity);
    builder.mix_fingerprint(gate.source_map_identity);
    builder.mix_fingerprint(gate.hygiene_mark);
    builder.mix_u8(static_cast<base::u8>(gate.origin_kind));
    builder.mix_u8(static_cast<base::u8>(gate.macro_kind));
    builder.mix_string(gate.macro_name);
    builder.mix_string(gate.consumer_name);
    builder.mix_string(gate.diagnostic_policy);
    builder.mix_string(gate.query_name);
    builder.mix_string(gate.blocker_category);
    builder.mix_string(gate.user_message);
    builder.mix_string(gate.blocker_reason);
    builder.mix_u64(static_cast<base::u64>(gate.primary_anchor.source.value));
    builder.mix_u64(static_cast<base::u64>(gate.primary_anchor.begin));
    builder.mix_u64(static_cast<base::u64>(gate.primary_anchor.end));
    builder.mix_u64(static_cast<base::u64>(gate.output_anchor.source.value));
    builder.mix_u64(static_cast<base::u64>(gate.output_anchor.begin));
    builder.mix_u64(static_cast<base::u64>(gate.output_anchor.end));
    builder.mix_bool(gate.diagnostic_emission_enabled);
    builder.mix_bool(gate.parser_consumable);
    return builder.finish();
}

} // namespace aurex::frontend::macro::detail
