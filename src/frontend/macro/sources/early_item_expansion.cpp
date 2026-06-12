#include <aurex/frontend/macro/early_item_expansion.hpp>
#include <aurex/infrastructure/base/integer.hpp>

#include <algorithm>
#include <sstream>
#include <utility>

namespace aurex::frontend::macro {
namespace {

constexpr std::string_view FRONTEND_MACRO_M21D_EXPANSION_NAME =
    "M21d No-op Early Item Macro Expansion Boundary";
constexpr std::string_view FRONTEND_MACRO_M21D_EXPANSION_FINGERPRINT_MARKER =
    "frontend.macro.m21d.noop_early_item_expansion.v1";
constexpr std::string_view FRONTEND_MACRO_M21D_TOKEN_TREE_FINGERPRINT_MARKER =
    "frontend.macro.m21d.attribute_token_tree.v1";
constexpr std::string_view FRONTEND_MACRO_M21D_QUERY_KEY_FINGERPRINT_MARKER =
    "frontend.macro.m21d.early_item_query_key.v1";
constexpr std::string_view FRONTEND_MACRO_M21D_GENERATED_PART_NAME_PREFIX = "#macro-generated:";
constexpr std::string_view FRONTEND_MACRO_M21D_GENERATED_VIRTUAL_BUFFER_PREFIX = "m21d-noop-generated:";
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

[[nodiscard]] base::Error internal_error(const std::string_view message)
{
    return base::Error{base::ErrorCode::internal_error, std::string(message)};
}

[[nodiscard]] bool source_range_is_well_formed(const base::SourceRange& range) noexcept
{
    return range.well_formed();
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
    std::string buffer(FRONTEND_MACRO_M21D_GENERATED_VIRTUAL_BUFFER_PREFIX);
    buffer += std::to_string(module.value);
    buffer.push_back(':');
    buffer += std::to_string(part_index);
    return buffer;
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

void mix_summary(query::StableHashBuilder& builder, const EarlyItemExpansionSummary& summary) noexcept
{
    builder.mix_u64(summary.macro_input_count);
    builder.mix_u64(summary.attribute_input_count);
    builder.mix_u64(summary.builtin_derive_passthrough_count);
    builder.mix_u64(summary.blocked_attribute_count);
    builder.mix_u64(summary.generated_part_placeholder_count);
    builder.mix_u64(summary.source_map_placeholder_count);
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
        && lhs.source_map_placeholder_count == rhs.source_map_placeholder_count
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

bool is_valid(const EarlyItemExpansionDisposition disposition) noexcept
{
    switch (disposition) {
        case EarlyItemExpansionDisposition::builtin_derive_passthrough:
        case EarlyItemExpansionDisposition::blocked_unimplemented_attribute:
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

bool is_valid(const EarlyItemExpansionSummary& summary, const EarlyItemExpansionResult& result) noexcept
{
    return summary_equals(summary, summarize_early_item_expansion_counts(result));
}

bool is_valid(const EarlyItemExpansionResult& result) noexcept
{
    return std::string_view(result.name) == FRONTEND_MACRO_M21D_EXPANSION_NAME
        && query::is_valid_m21c_macro_expansion_plan(result.plan)
        && std::all_of(result.inputs.begin(), result.inputs.end(), [](const EarlyItemMacroInput& input) {
               return is_valid(input);
           })
        && std::all_of(result.generated_parts.begin(), result.generated_parts.end(),
               [](const GeneratedModulePartPlaceholder& placeholder) {
                   return is_valid(placeholder);
               })
        && std::all_of(result.source_maps.begin(), result.source_maps.end(),
               [](const ExpansionSourceMapPlaceholder& placeholder) {
                   return is_valid(placeholder);
               })
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
    summary.source_map_placeholder_count = static_cast<base::u64>(result.source_maps.size());
    return summary;
}

query::StableFingerprint128 early_item_expansion_fingerprint(
    const EarlyItemExpansionResult& result) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21D_EXPANSION_FINGERPRINT_MARKER);
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
    builder.mix_u64(static_cast<base::u64>(result.source_maps.size()));
    for (const ExpansionSourceMapPlaceholder& source_map : result.source_maps) {
        mix_source_map(builder, source_map);
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
           << " source_map_placeholders=" << summary.source_map_placeholder_count
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
    for (base::usize index = 0; index < result.source_maps.size(); ++index) {
        const ExpansionSourceMapPlaceholder& source_map = result.source_maps[index];
        stream << "  source_map #" << index
               << " item=" << source_map.item.value
               << " module=" << source_map.module.value
               << " attribute_index=" << source_map.attribute_index
               << " real_source_map=" << (source_map.real_source_map ? "yes" : "no")
               << " debug_trace=" << (source_map.debug_trace_available ? "yes" : "no") << '\n';
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
    result.name = std::string(FRONTEND_MACRO_M21D_EXPANSION_NAME);
    result.plan = plan;
    result.inputs.reserve(ast.items.size());
    result.source_maps.reserve(ast.items.size());

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
            result.generated_parts.push_back(make_generated_part_placeholder(attached_part, module, part_index));
        }

        for (base::usize attribute_index = 0; attribute_index < item.attributes.size(); ++attribute_index) {
            const syntax::AttributeDecl& attribute = item.attributes[attribute_index];
            EarlyItemMacroInput input = make_macro_input(ast, item_id,
                base::checked_u32(attribute_index, "early item macro attribute index"), attribute, attached_part);
            result.source_maps.push_back(make_source_map_placeholder(input));
            result.inputs.push_back(std::move(input));
        }
    }

    result.summary = summarize_early_item_expansion_counts(result);
    result.fingerprint = early_item_expansion_fingerprint(result);
    return base::Result<EarlyItemExpansionResult>::ok(std::move(result));
}

} // namespace aurex::frontend::macro
