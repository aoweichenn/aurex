#include <aurex/infrastructure/query/macro_expansion_facts.hpp>

#include <algorithm>
#include <sstream>
#include <utility>
#include <vector>

namespace aurex::query {
namespace {

constexpr std::string_view QUERY_MACRO_EXPANSION_PLAN_FINGERPRINT_MARKER =
    "query.macro_expansion_plan.v1";
constexpr std::string_view QUERY_MACRO_EXPANSION_M21C_PLAN_NAME =
    "M21c Early Item Macro Expansion Plan";
constexpr std::string_view QUERY_MACRO_EXPANSION_M27_PLAN_NAME =
    "M27 Aurex Macro Surface Admission Plan";
constexpr std::string_view QUERY_MACRO_EXPANSION_M27B_PLAN_NAME =
    "M27b Aurex Typed Matcher And Definition-Site Hygiene Admission Plan";
constexpr std::string_view QUERY_MACRO_EXPANSION_ATTRIBUTE_INPUT_FACT =
    "m21b_attribute_decl_token_tree_input";
constexpr std::string_view QUERY_MACRO_EXPANSION_DERIVE_PASSTHROUGH_FACT =
    "builtin_derive_attribute_passthrough";
constexpr std::string_view QUERY_MACRO_EXPANSION_QUERY_KEY_FACT =
    "early_item_macro_expansion_query_key";
constexpr std::string_view QUERY_MACRO_EXPANSION_GENERATED_PART_FACT =
    "generated_module_part_noop_plan";
constexpr std::string_view QUERY_MACRO_EXPANSION_SOURCE_MAP_FACT =
    "expansion_source_map_stub_plan";
constexpr std::string_view QUERY_MACRO_EXPANSION_UNIMPLEMENTED_BLOCKER_FACT =
    "unimplemented_item_attribute_macro_blocker";
constexpr std::string_view QUERY_MACRO_EXPANSION_EXTERNAL_BLOCKER_FACT =
    "external_procedural_macro_sandbox_future_blocker";
constexpr std::string_view QUERY_MACRO_EXPANSION_AUREX_DECLARATIVE_SURFACE_FACT =
    "aurex_declarative_macro_surface_admission";
constexpr std::string_view QUERY_MACRO_EXPANSION_AUREX_USER_DERIVE_SURFACE_FACT =
    "aurex_user_derive_macro_surface_admission";
constexpr std::string_view QUERY_MACRO_EXPANSION_AUREX_COMPILE_TIME_ADMISSION_FACT =
    "aurex_compile_time_macro_execution_admission";
constexpr std::string_view QUERY_MACRO_EXPANSION_AUREX_TYPED_MATCHER_ADMISSION_FACT =
    "aurex_macro_typed_matcher_admission";
constexpr std::string_view QUERY_MACRO_EXPANSION_AUREX_DEFINITION_SITE_HYGIENE_FACT =
    "aurex_macro_definition_site_hygiene_admission";
constexpr std::string_view QUERY_MACRO_EXPANSION_AUREX_DEBUGGABLE_DIAGNOSTIC_ANCHOR_FACT =
    "aurex_macro_debuggable_diagnostic_anchor";
constexpr std::string_view QUERY_MACRO_EXPANSION_UNIMPLEMENTED_PREFIX =
    "item attribute macros are parsed but macro expansion is not implemented yet: ";
constexpr base::u8 QUERY_MACRO_EXPANSION_INVALID_ENUM_VALUE = 255U;
constexpr base::usize QUERY_MACRO_EXPANSION_M21C_FACT_COUNT = 7U;
constexpr base::usize QUERY_MACRO_EXPANSION_M27_FACT_COUNT = 10U;
constexpr base::usize QUERY_MACRO_EXPANSION_M27B_FACT_COUNT = 13U;

[[nodiscard]] base::u8 stable_kind_value(const MacroExpansionFactKind kind) noexcept
{
    return is_valid(kind) ? static_cast<base::u8>(kind) : QUERY_MACRO_EXPANSION_INVALID_ENUM_VALUE;
}

[[nodiscard]] base::u8 stable_stage_value(const MacroExpansionStage stage) noexcept
{
    return is_valid(stage) ? static_cast<base::u8>(stage) : QUERY_MACRO_EXPANSION_INVALID_ENUM_VALUE;
}

[[nodiscard]] base::u8 stable_policy_value(const MacroExpansionPolicy policy) noexcept
{
    return is_valid(policy) ? static_cast<base::u8>(policy) : QUERY_MACRO_EXPANSION_INVALID_ENUM_VALUE;
}

[[nodiscard]] bool nonempty(const std::string_view value) noexcept
{
    return !value.empty();
}

[[nodiscard]] bool fact_payload_is_named(const MacroExpansionFact& fact) noexcept
{
    return nonempty(fact.fact_name)
        && nonempty(fact.input_fact)
        && nonempty(fact.output_fact)
        && nonempty(fact.blocker_fact);
}

[[nodiscard]] bool generated_identity_is_consistent(const MacroExpansionFact& fact) noexcept
{
    if (!fact.requires_generated_module_part) {
        return true;
    }
    return fact.uses_generated_source_role
        && fact.uses_generated_module_part_kind
        && fact.generated_source_role == SourceRole::generated
        && fact.generated_part_kind == ModulePartKind::generated;
}

[[nodiscard]] bool no_executable_surface_leaks(const MacroExpansionFact& fact) noexcept
{
    return !fact.produces_user_generated_code
        && !fact.standard_library_required
        && !fact.runtime_required;
}

[[nodiscard]] bool fact_matches_kind_stage_policy(const MacroExpansionFact& fact) noexcept
{
    switch (fact.kind) {
        case MacroExpansionFactKind::attribute_token_tree_input:
            return fact.stage == MacroExpansionStage::parsed_attribute_surface
                && fact.policy == MacroExpansionPolicy::attribute_token_tree_v1
                && fact.consumes_attribute_decl
                && fact.consumes_attribute_token_tree
                && !fact.preserves_builtin_derive
                && !fact.requires_query_key
                && !fact.requires_generated_module_part
                && !fact.requires_source_map
                && !fact.requires_hygiene
                && !fact.external_process_required
                && !fact.blocks_unimplemented_item_attribute;
        case MacroExpansionFactKind::builtin_derive_passthrough:
            return fact.stage == MacroExpansionStage::early_item_expansion
                && fact.policy == MacroExpansionPolicy::builtin_derive_passthrough_v1
                && fact.consumes_attribute_decl
                && fact.consumes_attribute_token_tree
                && fact.preserves_builtin_derive
                && fact.requires_query_key
                && !fact.requires_generated_module_part
                && !fact.requires_source_map
                && !fact.external_process_required
                && !fact.blocks_unimplemented_item_attribute;
        case MacroExpansionFactKind::early_item_expansion_query_key:
            return fact.stage == MacroExpansionStage::early_item_expansion
                && fact.policy == MacroExpansionPolicy::expansion_query_fingerprint_v1
                && fact.consumes_attribute_decl
                && fact.consumes_attribute_token_tree
                && !fact.preserves_builtin_derive
                && fact.requires_query_key
                && !fact.requires_generated_module_part
                && fact.requires_source_map
                && fact.requires_hygiene
                && !fact.external_process_required
                && !fact.blocks_unimplemented_item_attribute;
        case MacroExpansionFactKind::generated_module_part_noop:
            return fact.stage == MacroExpansionStage::generated_part_planning
                && fact.policy == MacroExpansionPolicy::generated_module_part_noop_v1
                && fact.consumes_attribute_decl
                && fact.consumes_attribute_token_tree
                && fact.requires_query_key
                && fact.requires_generated_module_part
                && fact.requires_source_map
                && fact.requires_hygiene
                && !fact.external_process_required
                && !fact.blocks_unimplemented_item_attribute;
        case MacroExpansionFactKind::expansion_source_map_stub:
            return fact.stage == MacroExpansionStage::generated_part_planning
                && fact.policy == MacroExpansionPolicy::source_map_trace_stub_v1
                && fact.consumes_attribute_decl
                && fact.consumes_attribute_token_tree
                && fact.requires_query_key
                && fact.requires_generated_module_part
                && fact.requires_source_map
                && fact.requires_hygiene
                && !fact.external_process_required
                && !fact.blocks_unimplemented_item_attribute;
        case MacroExpansionFactKind::unimplemented_item_attribute_blocker:
            return fact.stage == MacroExpansionStage::sema_blocker
                && fact.policy == MacroExpansionPolicy::unimplemented_item_attribute_blocker_v1
                && fact.consumes_attribute_decl
                && fact.consumes_attribute_token_tree
                && fact.requires_query_key
                && !fact.requires_generated_module_part
                && fact.requires_source_map
                && fact.requires_hygiene
                && !fact.external_process_required
                && fact.blocks_unimplemented_item_attribute;
        case MacroExpansionFactKind::external_procedural_macro_blocked:
            return fact.stage == MacroExpansionStage::future_stage
                && fact.policy == MacroExpansionPolicy::external_proc_macro_sandbox_future_v1
                && !fact.consumes_attribute_decl
                && !fact.consumes_attribute_token_tree
                && fact.requires_query_key
                && !fact.requires_generated_module_part
                && fact.requires_source_map
                && fact.requires_hygiene
                && fact.external_process_required
                && fact.blocks_unimplemented_item_attribute;
        case MacroExpansionFactKind::aurex_declarative_macro_surface:
            return fact.stage == MacroExpansionStage::early_item_expansion
                && fact.policy == MacroExpansionPolicy::aurex_declarative_macro_surface_v1
                && !fact.consumes_attribute_decl
                && fact.consumes_attribute_token_tree
                && !fact.preserves_builtin_derive
                && fact.requires_query_key
                && !fact.requires_generated_module_part
                && fact.requires_source_map
                && fact.requires_hygiene
                && !fact.external_process_required
                && fact.blocks_unimplemented_item_attribute;
        case MacroExpansionFactKind::aurex_user_derive_macro_surface:
            return fact.stage == MacroExpansionStage::early_item_expansion
                && fact.policy == MacroExpansionPolicy::aurex_user_derive_macro_surface_v1
                && !fact.consumes_attribute_decl
                && fact.consumes_attribute_token_tree
                && !fact.preserves_builtin_derive
                && fact.requires_query_key
                && !fact.requires_generated_module_part
                && fact.requires_source_map
                && fact.requires_hygiene
                && !fact.external_process_required
                && fact.blocks_unimplemented_item_attribute;
        case MacroExpansionFactKind::aurex_compile_time_macro_execution_admission:
            return fact.stage == MacroExpansionStage::early_item_expansion
                && fact.policy == MacroExpansionPolicy::aurex_compile_time_macro_execution_admission_v1
                && !fact.consumes_attribute_decl
                && fact.consumes_attribute_token_tree
                && !fact.preserves_builtin_derive
                && fact.requires_query_key
                && !fact.requires_generated_module_part
                && fact.requires_source_map
                && fact.requires_hygiene
                && !fact.external_process_required
                && fact.blocks_unimplemented_item_attribute;
        case MacroExpansionFactKind::aurex_macro_typed_matcher_admission:
            return fact.stage == MacroExpansionStage::early_item_expansion
                && fact.policy == MacroExpansionPolicy::aurex_macro_typed_matcher_admission_v1
                && !fact.consumes_attribute_decl
                && fact.consumes_attribute_token_tree
                && !fact.preserves_builtin_derive
                && fact.requires_query_key
                && !fact.requires_generated_module_part
                && fact.requires_source_map
                && fact.requires_hygiene
                && !fact.external_process_required
                && fact.blocks_unimplemented_item_attribute;
        case MacroExpansionFactKind::aurex_macro_definition_site_hygiene_admission:
            return fact.stage == MacroExpansionStage::early_item_expansion
                && fact.policy == MacroExpansionPolicy::aurex_macro_definition_site_hygiene_admission_v1
                && !fact.consumes_attribute_decl
                && fact.consumes_attribute_token_tree
                && !fact.preserves_builtin_derive
                && fact.requires_query_key
                && !fact.requires_generated_module_part
                && fact.requires_source_map
                && fact.requires_hygiene
                && !fact.external_process_required
                && fact.blocks_unimplemented_item_attribute;
        case MacroExpansionFactKind::aurex_macro_debuggable_diagnostic_anchor:
            return fact.stage == MacroExpansionStage::early_item_expansion
                && fact.policy == MacroExpansionPolicy::aurex_macro_debuggable_diagnostic_anchor_v1
                && !fact.consumes_attribute_decl
                && fact.consumes_attribute_token_tree
                && !fact.preserves_builtin_derive
                && fact.requires_query_key
                && !fact.requires_generated_module_part
                && fact.requires_source_map
                && fact.requires_hygiene
                && !fact.external_process_required
                && fact.blocks_unimplemented_item_attribute;
    }
    return false;
}

[[nodiscard]] bool plan_has_each_fact_kind_once(const MacroExpansionPlan& plan, const base::usize fact_count) noexcept
{
    if (plan.facts.size() != fact_count) {
        return false;
    }

    std::vector<bool> seen(fact_count, false);
    for (const MacroExpansionFact& fact : plan.facts) {
        if (!is_valid(fact.kind)) {
            return false;
        }
        const base::usize index = static_cast<base::usize>(fact.kind) - 1U;
        if (index >= seen.size() || seen[index]) {
            return false;
        }
        seen[index] = true;
    }
    return std::all_of(seen.begin(), seen.end(), [](const bool present) {
        return present;
    });
}

[[nodiscard]] bool summary_equals(const MacroExpansionSummary& lhs, const MacroExpansionSummary& rhs) noexcept
{
    return lhs.fact_count == rhs.fact_count
        && lhs.attribute_input_count == rhs.attribute_input_count
        && lhs.builtin_derive_passthrough_count == rhs.builtin_derive_passthrough_count
        && lhs.query_key_count == rhs.query_key_count
        && lhs.generated_part_count == rhs.generated_part_count
        && lhs.source_map_stub_count == rhs.source_map_stub_count
        && lhs.sema_blocker_count == rhs.sema_blocker_count
        && lhs.future_external_count == rhs.future_external_count
        && lhs.aurex_declarative_macro_surface_count == rhs.aurex_declarative_macro_surface_count
        && lhs.aurex_user_derive_macro_surface_count == rhs.aurex_user_derive_macro_surface_count
        && lhs.aurex_compile_time_macro_execution_admission_count
            == rhs.aurex_compile_time_macro_execution_admission_count
        && lhs.aurex_macro_typed_matcher_admission_count
            == rhs.aurex_macro_typed_matcher_admission_count
        && lhs.aurex_macro_definition_site_hygiene_admission_count
            == rhs.aurex_macro_definition_site_hygiene_admission_count
        && lhs.aurex_macro_debuggable_diagnostic_anchor_count
            == rhs.aurex_macro_debuggable_diagnostic_anchor_count
        && lhs.attribute_decl_input_count == rhs.attribute_decl_input_count
        && lhs.token_tree_input_count == rhs.token_tree_input_count
        && lhs.generated_source_role_count == rhs.generated_source_role_count
        && lhs.generated_module_part_kind_count == rhs.generated_module_part_kind_count
        && lhs.user_generated_code_count == rhs.user_generated_code_count
        && lhs.standard_library_required_count == rhs.standard_library_required_count
        && lhs.runtime_required_count == rhs.runtime_required_count
        && lhs.external_process_required_count == rhs.external_process_required_count
        && lhs.unimplemented_item_attribute_blocker_count
            == rhs.unimplemented_item_attribute_blocker_count;
}

void mix_summary(StableHashBuilder& builder, const MacroExpansionSummary& summary) noexcept
{
    builder.mix_u64(summary.fact_count);
    builder.mix_u64(summary.attribute_input_count);
    builder.mix_u64(summary.builtin_derive_passthrough_count);
    builder.mix_u64(summary.query_key_count);
    builder.mix_u64(summary.generated_part_count);
    builder.mix_u64(summary.source_map_stub_count);
    builder.mix_u64(summary.sema_blocker_count);
    builder.mix_u64(summary.future_external_count);
    builder.mix_u64(summary.aurex_declarative_macro_surface_count);
    builder.mix_u64(summary.aurex_user_derive_macro_surface_count);
    builder.mix_u64(summary.aurex_compile_time_macro_execution_admission_count);
    builder.mix_u64(summary.aurex_macro_typed_matcher_admission_count);
    builder.mix_u64(summary.aurex_macro_definition_site_hygiene_admission_count);
    builder.mix_u64(summary.aurex_macro_debuggable_diagnostic_anchor_count);
    builder.mix_u64(summary.attribute_decl_input_count);
    builder.mix_u64(summary.token_tree_input_count);
    builder.mix_u64(summary.generated_source_role_count);
    builder.mix_u64(summary.generated_module_part_kind_count);
    builder.mix_u64(summary.user_generated_code_count);
    builder.mix_u64(summary.standard_library_required_count);
    builder.mix_u64(summary.runtime_required_count);
    builder.mix_u64(summary.external_process_required_count);
    builder.mix_u64(summary.unimplemented_item_attribute_blocker_count);
}

void mix_fact(StableHashBuilder& builder, const MacroExpansionFact& fact) noexcept
{
    builder.mix_string(fact.fact_name);
    builder.mix_u8(stable_kind_value(fact.kind));
    builder.mix_u8(stable_stage_value(fact.stage));
    builder.mix_u8(stable_policy_value(fact.policy));
    builder.mix_bool(fact.consumes_attribute_decl);
    builder.mix_bool(fact.consumes_attribute_token_tree);
    builder.mix_bool(fact.preserves_builtin_derive);
    builder.mix_bool(fact.requires_query_key);
    builder.mix_bool(fact.requires_generated_module_part);
    builder.mix_bool(fact.uses_generated_source_role);
    builder.mix_bool(fact.uses_generated_module_part_kind);
    builder.mix_bool(fact.requires_source_map);
    builder.mix_bool(fact.requires_hygiene);
    builder.mix_bool(fact.produces_user_generated_code);
    builder.mix_bool(fact.standard_library_required);
    builder.mix_bool(fact.runtime_required);
    builder.mix_bool(fact.external_process_required);
    builder.mix_bool(fact.blocks_unimplemented_item_attribute);
    builder.mix_u8(static_cast<base::u8>(fact.generated_source_role));
    builder.mix_u8(static_cast<base::u8>(fact.generated_part_kind));
    builder.mix_string(fact.input_fact);
    builder.mix_string(fact.output_fact);
    builder.mix_string(fact.blocker_fact);
}

void append_fact_flags(std::ostringstream& stream, const MacroExpansionFact& fact)
{
    stream << " flags="
           << "attribute_decl:" << (fact.consumes_attribute_decl ? "yes" : "no")
           << ",token_tree:" << (fact.consumes_attribute_token_tree ? "yes" : "no")
           << ",builtin_derive:" << (fact.preserves_builtin_derive ? "yes" : "no")
           << ",query_key:" << (fact.requires_query_key ? "yes" : "no")
           << ",generated_part:" << (fact.requires_generated_module_part ? "yes" : "no")
           << ",generated_source_role:" << (fact.uses_generated_source_role ? "yes" : "no")
           << ",generated_part_kind:" << (fact.uses_generated_module_part_kind ? "yes" : "no")
           << ",source_map:" << (fact.requires_source_map ? "yes" : "no")
           << ",hygiene:" << (fact.requires_hygiene ? "yes" : "no")
           << ",user_generated_code:" << (fact.produces_user_generated_code ? "yes" : "no")
           << ",standard_library:" << (fact.standard_library_required ? "yes" : "no")
           << ",runtime:" << (fact.runtime_required ? "yes" : "no")
           << ",external_process:" << (fact.external_process_required ? "yes" : "no")
           << ",unimplemented_blocker:" << (fact.blocks_unimplemented_item_attribute ? "yes" : "no");
}

[[nodiscard]] MacroExpansionFact make_macro_expansion_fact(
    const std::string_view fact_name,
    const MacroExpansionFactKind kind,
    const MacroExpansionStage stage,
    const MacroExpansionPolicy policy,
    const std::string_view input_fact,
    const std::string_view output_fact,
    const std::string_view blocker_fact)
{
    MacroExpansionFact fact;
    fact.fact_name = std::string(fact_name);
    fact.kind = kind;
    fact.stage = stage;
    fact.policy = policy;
    fact.input_fact = std::string(input_fact);
    fact.output_fact = std::string(output_fact);
    fact.blocker_fact = std::string(blocker_fact);
    return fact;
}

[[nodiscard]] MacroExpansionFact make_attribute_input_fact()
{
    MacroExpansionFact fact = make_macro_expansion_fact(
        QUERY_MACRO_EXPANSION_ATTRIBUTE_INPUT_FACT,
        MacroExpansionFactKind::attribute_token_tree_input,
        MacroExpansionStage::parsed_attribute_surface,
        MacroExpansionPolicy::attribute_token_tree_v1,
        "ItemNode::attributes",
        "AttributeTokenDecl flat token tree",
        "parser_recovery_reports_malformed_attribute");
    fact.consumes_attribute_decl = true;
    fact.consumes_attribute_token_tree = true;
    return fact;
}

[[nodiscard]] MacroExpansionFact make_builtin_derive_fact()
{
    MacroExpansionFact fact = make_macro_expansion_fact(
        QUERY_MACRO_EXPANSION_DERIVE_PASSTHROUGH_FACT,
        MacroExpansionFactKind::builtin_derive_passthrough,
        MacroExpansionStage::early_item_expansion,
        MacroExpansionPolicy::builtin_derive_passthrough_v1,
        "AttributeDecl{name=derive}",
        "DeriveDecl compatibility path",
        "unsupported_derive_capability_diagnostic");
    fact.consumes_attribute_decl = true;
    fact.consumes_attribute_token_tree = true;
    fact.preserves_builtin_derive = true;
    fact.requires_query_key = true;
    return fact;
}

[[nodiscard]] MacroExpansionFact make_query_key_fact()
{
    MacroExpansionFact fact = make_macro_expansion_fact(
        QUERY_MACRO_EXPANSION_QUERY_KEY_FACT,
        MacroExpansionFactKind::early_item_expansion_query_key,
        MacroExpansionStage::early_item_expansion,
        MacroExpansionPolicy::expansion_query_fingerprint_v1,
        "macro definition identity + attached item stable key + token tree fingerprint",
        "macro expansion query key fingerprint",
        "attribute macro remains blocked until expansion query output exists");
    fact.consumes_attribute_decl = true;
    fact.consumes_attribute_token_tree = true;
    fact.requires_query_key = true;
    fact.requires_source_map = true;
    fact.requires_hygiene = true;
    return fact;
}

[[nodiscard]] MacroExpansionFact make_generated_part_fact()
{
    MacroExpansionFact fact = make_macro_expansion_fact(
        QUERY_MACRO_EXPANSION_GENERATED_PART_FACT,
        MacroExpansionFactKind::generated_module_part_noop,
        MacroExpansionStage::generated_part_planning,
        MacroExpansionPolicy::generated_module_part_noop_v1,
        "macro expansion query result",
        "SourceRole::generated + ModulePartKind::generated no-op module part",
        "generated user items are not admitted in M21c");
    fact.consumes_attribute_decl = true;
    fact.consumes_attribute_token_tree = true;
    fact.requires_query_key = true;
    fact.requires_generated_module_part = true;
    fact.uses_generated_source_role = true;
    fact.uses_generated_module_part_kind = true;
    fact.requires_source_map = true;
    fact.requires_hygiene = true;
    return fact;
}

[[nodiscard]] MacroExpansionFact make_source_map_fact()
{
    MacroExpansionFact fact = make_macro_expansion_fact(
        QUERY_MACRO_EXPANSION_SOURCE_MAP_FACT,
        MacroExpansionFactKind::expansion_source_map_stub,
        MacroExpansionStage::generated_part_planning,
        MacroExpansionPolicy::source_map_trace_stub_v1,
        "attached attribute source range",
        "macro expansion origin/debug trace stub",
        "--emit-expanded remains unavailable in M21c");
    fact.consumes_attribute_decl = true;
    fact.consumes_attribute_token_tree = true;
    fact.requires_query_key = true;
    fact.requires_generated_module_part = true;
    fact.uses_generated_source_role = true;
    fact.uses_generated_module_part_kind = true;
    fact.requires_source_map = true;
    fact.requires_hygiene = true;
    return fact;
}

[[nodiscard]] MacroExpansionFact make_unimplemented_blocker_fact()
{
    MacroExpansionFact fact = make_macro_expansion_fact(
        QUERY_MACRO_EXPANSION_UNIMPLEMENTED_BLOCKER_FACT,
        MacroExpansionFactKind::unimplemented_item_attribute_blocker,
        MacroExpansionStage::sema_blocker,
        MacroExpansionPolicy::unimplemented_item_attribute_blocker_v1,
        "non-derive AttributeDecl",
        QUERY_MACRO_EXPANSION_UNIMPLEMENTED_PREFIX,
        "macro expansion output missing");
    fact.consumes_attribute_decl = true;
    fact.consumes_attribute_token_tree = true;
    fact.requires_query_key = true;
    fact.requires_source_map = true;
    fact.requires_hygiene = true;
    fact.blocks_unimplemented_item_attribute = true;
    return fact;
}

[[nodiscard]] MacroExpansionFact make_external_blocker_fact()
{
    MacroExpansionFact fact = make_macro_expansion_fact(
        QUERY_MACRO_EXPANSION_EXTERNAL_BLOCKER_FACT,
        MacroExpansionFactKind::external_procedural_macro_blocked,
        MacroExpansionStage::future_stage,
        MacroExpansionPolicy::external_proc_macro_sandbox_future_v1,
        "external macro manifest",
        "sandboxed external procedural macro output",
        "external procedural macros are not executed in M21c");
    fact.requires_query_key = true;
    fact.requires_source_map = true;
    fact.requires_hygiene = true;
    fact.external_process_required = true;
    fact.blocks_unimplemented_item_attribute = true;
    return fact;
}

[[nodiscard]] MacroExpansionFact make_aurex_declarative_surface_fact()
{
    MacroExpansionFact fact = make_macro_expansion_fact(
        QUERY_MACRO_EXPANSION_AUREX_DECLARATIVE_SURFACE_FACT,
        MacroExpansionFactKind::aurex_declarative_macro_surface,
        MacroExpansionStage::early_item_expansion,
        MacroExpansionPolicy::aurex_declarative_macro_surface_v1,
        "ItemNode{kind=macro_decl, macro_kind=declarative}",
        "AurexMacroSurfaceAdmissionGate declarative surface",
        "declarative macro expansion remains blocked in M27a");
    fact.consumes_attribute_token_tree = true;
    fact.requires_query_key = true;
    fact.requires_source_map = true;
    fact.requires_hygiene = true;
    fact.blocks_unimplemented_item_attribute = true;
    return fact;
}

[[nodiscard]] MacroExpansionFact make_aurex_user_derive_surface_fact()
{
    MacroExpansionFact fact = make_macro_expansion_fact(
        QUERY_MACRO_EXPANSION_AUREX_USER_DERIVE_SURFACE_FACT,
        MacroExpansionFactKind::aurex_user_derive_macro_surface,
        MacroExpansionStage::early_item_expansion,
        MacroExpansionPolicy::aurex_user_derive_macro_surface_v1,
        "ItemNode{kind=macro_decl, macro_kind=derive}",
        "AurexMacroSurfaceAdmissionGate user derive surface",
        "user derive macro expansion remains admission-only in M27b");
    fact.consumes_attribute_token_tree = true;
    fact.requires_query_key = true;
    fact.requires_source_map = true;
    fact.requires_hygiene = true;
    fact.blocks_unimplemented_item_attribute = true;
    return fact;
}

[[nodiscard]] MacroExpansionFact make_aurex_compile_time_admission_fact()
{
    MacroExpansionFact fact = make_macro_expansion_fact(
        QUERY_MACRO_EXPANSION_AUREX_COMPILE_TIME_ADMISSION_FACT,
        MacroExpansionFactKind::aurex_compile_time_macro_execution_admission,
        MacroExpansionStage::early_item_expansion,
        MacroExpansionPolicy::aurex_compile_time_macro_execution_admission_v1,
        "ItemNode{kind=macro_decl, macro_kind=compile_time}",
        "AurexMacroSurfaceAdmissionGate compile-time execution admission",
        "compile-time macro execution remains check-only in M27c");
    fact.consumes_attribute_token_tree = true;
    fact.requires_query_key = true;
    fact.requires_source_map = true;
    fact.requires_hygiene = true;
    fact.blocks_unimplemented_item_attribute = true;
    return fact;
}

[[nodiscard]] MacroExpansionFact make_aurex_typed_matcher_admission_fact()
{
    MacroExpansionFact fact = make_macro_expansion_fact(
        QUERY_MACRO_EXPANSION_AUREX_TYPED_MATCHER_ADMISSION_FACT,
        MacroExpansionFactKind::aurex_macro_typed_matcher_admission,
        MacroExpansionStage::early_item_expansion,
        MacroExpansionPolicy::aurex_macro_typed_matcher_admission_v1,
        "macro body top-level match hints",
        "AurexMacroTypedMatcherAdmissionGate typed matcher facts",
        "typed matcher execution remains blocked in M27b");
    fact.consumes_attribute_token_tree = true;
    fact.requires_query_key = true;
    fact.requires_source_map = true;
    fact.requires_hygiene = true;
    fact.blocks_unimplemented_item_attribute = true;
    return fact;
}

[[nodiscard]] MacroExpansionFact make_aurex_definition_site_hygiene_fact()
{
    MacroExpansionFact fact = make_macro_expansion_fact(
        QUERY_MACRO_EXPANSION_AUREX_DEFINITION_SITE_HYGIENE_FACT,
        MacroExpansionFactKind::aurex_macro_definition_site_hygiene_admission,
        MacroExpansionStage::early_item_expansion,
        MacroExpansionPolicy::aurex_macro_definition_site_hygiene_admission_v1,
        "macro declaration definition site",
        "AurexMacroDefinitionSiteHygieneAdmissionGate",
        "definition-site hygiene resolution remains admission-only in M27b");
    fact.consumes_attribute_token_tree = true;
    fact.requires_query_key = true;
    fact.requires_source_map = true;
    fact.requires_hygiene = true;
    fact.blocks_unimplemented_item_attribute = true;
    return fact;
}

[[nodiscard]] MacroExpansionFact make_aurex_debuggable_diagnostic_anchor_fact()
{
    MacroExpansionFact fact = make_macro_expansion_fact(
        QUERY_MACRO_EXPANSION_AUREX_DEBUGGABLE_DIAGNOSTIC_ANCHOR_FACT,
        MacroExpansionFactKind::aurex_macro_debuggable_diagnostic_anchor,
        MacroExpansionStage::early_item_expansion,
        MacroExpansionPolicy::aurex_macro_debuggable_diagnostic_anchor_v1,
        "macro matcher source anchors",
        "stable macro matcher diagnostic anchors",
        "macro matcher diagnostics remain non-emitting in M27b");
    fact.consumes_attribute_token_tree = true;
    fact.requires_query_key = true;
    fact.requires_source_map = true;
    fact.requires_hygiene = true;
    fact.blocks_unimplemented_item_attribute = true;
    return fact;
}

} // namespace

std::string_view macro_expansion_fact_kind_name(const MacroExpansionFactKind kind) noexcept
{
    switch (kind) {
        case MacroExpansionFactKind::attribute_token_tree_input:
            return "attribute_token_tree_input";
        case MacroExpansionFactKind::builtin_derive_passthrough:
            return "builtin_derive_passthrough";
        case MacroExpansionFactKind::early_item_expansion_query_key:
            return "early_item_expansion_query_key";
        case MacroExpansionFactKind::generated_module_part_noop:
            return "generated_module_part_noop";
        case MacroExpansionFactKind::expansion_source_map_stub:
            return "expansion_source_map_stub";
        case MacroExpansionFactKind::unimplemented_item_attribute_blocker:
            return "unimplemented_item_attribute_blocker";
        case MacroExpansionFactKind::external_procedural_macro_blocked:
            return "external_procedural_macro_blocked";
        case MacroExpansionFactKind::aurex_declarative_macro_surface:
            return "aurex_declarative_macro_surface";
        case MacroExpansionFactKind::aurex_user_derive_macro_surface:
            return "aurex_user_derive_macro_surface";
        case MacroExpansionFactKind::aurex_compile_time_macro_execution_admission:
            return "aurex_compile_time_macro_execution_admission";
        case MacroExpansionFactKind::aurex_macro_typed_matcher_admission:
            return "aurex_macro_typed_matcher_admission";
        case MacroExpansionFactKind::aurex_macro_definition_site_hygiene_admission:
            return "aurex_macro_definition_site_hygiene_admission";
        case MacroExpansionFactKind::aurex_macro_debuggable_diagnostic_anchor:
            return "aurex_macro_debuggable_diagnostic_anchor";
    }
    return "invalid";
}

std::string_view macro_expansion_stage_name(const MacroExpansionStage stage) noexcept
{
    switch (stage) {
        case MacroExpansionStage::parsed_attribute_surface:
            return "parsed_attribute_surface";
        case MacroExpansionStage::early_item_expansion:
            return "early_item_expansion";
        case MacroExpansionStage::generated_part_planning:
            return "generated_part_planning";
        case MacroExpansionStage::sema_blocker:
            return "sema_blocker";
        case MacroExpansionStage::future_stage:
            return "future_stage";
    }
    return "invalid";
}

std::string_view macro_expansion_policy_name(const MacroExpansionPolicy policy) noexcept
{
    switch (policy) {
        case MacroExpansionPolicy::attribute_token_tree_v1:
            return "attribute_token_tree_v1";
        case MacroExpansionPolicy::builtin_derive_passthrough_v1:
            return "builtin_derive_passthrough_v1";
        case MacroExpansionPolicy::expansion_query_fingerprint_v1:
            return "expansion_query_fingerprint_v1";
        case MacroExpansionPolicy::generated_module_part_noop_v1:
            return "generated_module_part_noop_v1";
        case MacroExpansionPolicy::source_map_trace_stub_v1:
            return "source_map_trace_stub_v1";
        case MacroExpansionPolicy::unimplemented_item_attribute_blocker_v1:
            return "unimplemented_item_attribute_blocker_v1";
        case MacroExpansionPolicy::external_proc_macro_sandbox_future_v1:
            return "external_proc_macro_sandbox_future_v1";
        case MacroExpansionPolicy::aurex_declarative_macro_surface_v1:
            return "aurex_declarative_macro_surface_v1";
        case MacroExpansionPolicy::aurex_user_derive_macro_surface_v1:
            return "aurex_user_derive_macro_surface_v1";
        case MacroExpansionPolicy::aurex_compile_time_macro_execution_admission_v1:
            return "aurex_compile_time_macro_execution_admission_v1";
        case MacroExpansionPolicy::aurex_macro_typed_matcher_admission_v1:
            return "aurex_macro_typed_matcher_admission_v1";
        case MacroExpansionPolicy::aurex_macro_definition_site_hygiene_admission_v1:
            return "aurex_macro_definition_site_hygiene_admission_v1";
        case MacroExpansionPolicy::aurex_macro_debuggable_diagnostic_anchor_v1:
            return "aurex_macro_debuggable_diagnostic_anchor_v1";
    }
    return "invalid";
}

bool is_valid(const MacroExpansionFactKind kind) noexcept
{
    switch (kind) {
        case MacroExpansionFactKind::attribute_token_tree_input:
        case MacroExpansionFactKind::builtin_derive_passthrough:
        case MacroExpansionFactKind::early_item_expansion_query_key:
        case MacroExpansionFactKind::generated_module_part_noop:
        case MacroExpansionFactKind::expansion_source_map_stub:
        case MacroExpansionFactKind::unimplemented_item_attribute_blocker:
        case MacroExpansionFactKind::external_procedural_macro_blocked:
        case MacroExpansionFactKind::aurex_declarative_macro_surface:
        case MacroExpansionFactKind::aurex_user_derive_macro_surface:
        case MacroExpansionFactKind::aurex_compile_time_macro_execution_admission:
        case MacroExpansionFactKind::aurex_macro_typed_matcher_admission:
        case MacroExpansionFactKind::aurex_macro_definition_site_hygiene_admission:
        case MacroExpansionFactKind::aurex_macro_debuggable_diagnostic_anchor:
            return true;
    }
    return false;
}

bool is_valid(const MacroExpansionStage stage) noexcept
{
    switch (stage) {
        case MacroExpansionStage::parsed_attribute_surface:
        case MacroExpansionStage::early_item_expansion:
        case MacroExpansionStage::generated_part_planning:
        case MacroExpansionStage::sema_blocker:
        case MacroExpansionStage::future_stage:
            return true;
    }
    return false;
}

bool is_valid(const MacroExpansionPolicy policy) noexcept
{
    switch (policy) {
        case MacroExpansionPolicy::attribute_token_tree_v1:
        case MacroExpansionPolicy::builtin_derive_passthrough_v1:
        case MacroExpansionPolicy::expansion_query_fingerprint_v1:
        case MacroExpansionPolicy::generated_module_part_noop_v1:
        case MacroExpansionPolicy::source_map_trace_stub_v1:
        case MacroExpansionPolicy::unimplemented_item_attribute_blocker_v1:
        case MacroExpansionPolicy::external_proc_macro_sandbox_future_v1:
        case MacroExpansionPolicy::aurex_declarative_macro_surface_v1:
        case MacroExpansionPolicy::aurex_user_derive_macro_surface_v1:
        case MacroExpansionPolicy::aurex_compile_time_macro_execution_admission_v1:
        case MacroExpansionPolicy::aurex_macro_typed_matcher_admission_v1:
        case MacroExpansionPolicy::aurex_macro_definition_site_hygiene_admission_v1:
        case MacroExpansionPolicy::aurex_macro_debuggable_diagnostic_anchor_v1:
            return true;
    }
    return false;
}

bool is_valid(const MacroExpansionFact& fact) noexcept
{
    return is_valid(fact.kind)
        && is_valid(fact.stage)
        && is_valid(fact.policy)
        && fact_payload_is_named(fact)
        && generated_identity_is_consistent(fact)
        && no_executable_surface_leaks(fact)
        && fact_matches_kind_stage_policy(fact);
}

bool is_valid(const MacroExpansionSummary& summary, const MacroExpansionPlan& plan) noexcept
{
    return summary_equals(summary, summarize_macro_expansion_plan_counts(plan));
}

bool is_valid(const MacroExpansionPlan& plan) noexcept
{
    return is_valid_m21c_macro_expansion_plan(plan);
}

bool is_valid_m21c_macro_expansion_plan(const MacroExpansionPlan& plan) noexcept
{
    return std::string_view(plan.name) == QUERY_MACRO_EXPANSION_M21C_PLAN_NAME
        && plan_has_each_fact_kind_once(plan, QUERY_MACRO_EXPANSION_M21C_FACT_COUNT)
        && std::all_of(plan.facts.begin(), plan.facts.end(), [](const MacroExpansionFact& fact) {
               return static_cast<base::usize>(fact.kind) <= QUERY_MACRO_EXPANSION_M21C_FACT_COUNT;
           })
        && std::all_of(plan.facts.begin(), plan.facts.end(), [](const MacroExpansionFact& fact) {
               return is_valid(fact);
           })
        && is_valid(plan.summary, plan)
        && plan.fingerprint == macro_expansion_plan_fingerprint(plan);
}

bool is_valid_m27_macro_expansion_plan(const MacroExpansionPlan& plan) noexcept
{
    return std::string_view(plan.name) == QUERY_MACRO_EXPANSION_M27_PLAN_NAME
        && plan_has_each_fact_kind_once(plan, QUERY_MACRO_EXPANSION_M27_FACT_COUNT)
        && std::all_of(plan.facts.begin(), plan.facts.end(), [](const MacroExpansionFact& fact) {
               return is_valid(fact);
           })
        && is_valid(plan.summary, plan)
        && plan.fingerprint == macro_expansion_plan_fingerprint(plan);
}

bool is_valid_m27b_macro_expansion_plan(const MacroExpansionPlan& plan) noexcept
{
    return std::string_view(plan.name) == QUERY_MACRO_EXPANSION_M27B_PLAN_NAME
        && plan_has_each_fact_kind_once(plan, QUERY_MACRO_EXPANSION_M27B_FACT_COUNT)
        && std::all_of(plan.facts.begin(), plan.facts.end(), [](const MacroExpansionFact& fact) {
               return is_valid(fact);
           })
        && is_valid(plan.summary, plan)
        && plan.fingerprint == macro_expansion_plan_fingerprint(plan);
}

void record_macro_expansion_fact(MacroExpansionPlan& plan, MacroExpansionFact fact)
{
    plan.facts.push_back(std::move(fact));
}

MacroExpansionSummary summarize_macro_expansion_plan_counts(const MacroExpansionPlan& plan) noexcept
{
    MacroExpansionSummary summary;
    summary.fact_count = static_cast<base::u64>(plan.facts.size());
    for (const MacroExpansionFact& fact : plan.facts) {
        switch (fact.kind) {
            case MacroExpansionFactKind::attribute_token_tree_input:
                ++summary.attribute_input_count;
                break;
            case MacroExpansionFactKind::builtin_derive_passthrough:
                ++summary.builtin_derive_passthrough_count;
                break;
            case MacroExpansionFactKind::early_item_expansion_query_key:
                ++summary.query_key_count;
                break;
            case MacroExpansionFactKind::generated_module_part_noop:
                ++summary.generated_part_count;
                break;
            case MacroExpansionFactKind::expansion_source_map_stub:
                ++summary.source_map_stub_count;
                break;
            case MacroExpansionFactKind::unimplemented_item_attribute_blocker:
                ++summary.sema_blocker_count;
                break;
            case MacroExpansionFactKind::external_procedural_macro_blocked:
                ++summary.future_external_count;
                break;
            case MacroExpansionFactKind::aurex_declarative_macro_surface:
                ++summary.aurex_declarative_macro_surface_count;
                break;
            case MacroExpansionFactKind::aurex_user_derive_macro_surface:
                ++summary.aurex_user_derive_macro_surface_count;
                break;
            case MacroExpansionFactKind::aurex_compile_time_macro_execution_admission:
                ++summary.aurex_compile_time_macro_execution_admission_count;
                break;
            case MacroExpansionFactKind::aurex_macro_typed_matcher_admission:
                ++summary.aurex_macro_typed_matcher_admission_count;
                break;
            case MacroExpansionFactKind::aurex_macro_definition_site_hygiene_admission:
                ++summary.aurex_macro_definition_site_hygiene_admission_count;
                break;
            case MacroExpansionFactKind::aurex_macro_debuggable_diagnostic_anchor:
                ++summary.aurex_macro_debuggable_diagnostic_anchor_count;
                break;
        }
        if (fact.consumes_attribute_decl) {
            ++summary.attribute_decl_input_count;
        }
        if (fact.consumes_attribute_token_tree) {
            ++summary.token_tree_input_count;
        }
        if (fact.uses_generated_source_role) {
            ++summary.generated_source_role_count;
        }
        if (fact.uses_generated_module_part_kind) {
            ++summary.generated_module_part_kind_count;
        }
        if (fact.produces_user_generated_code) {
            ++summary.user_generated_code_count;
        }
        if (fact.standard_library_required) {
            ++summary.standard_library_required_count;
        }
        if (fact.runtime_required) {
            ++summary.runtime_required_count;
        }
        if (fact.external_process_required) {
            ++summary.external_process_required_count;
        }
        if (fact.blocks_unimplemented_item_attribute) {
            ++summary.unimplemented_item_attribute_blocker_count;
        }
    }
    return summary;
}

StableFingerprint128 macro_expansion_plan_fingerprint(const MacroExpansionPlan& plan) noexcept
{
    StableHashBuilder builder;
    builder.mix_string(QUERY_MACRO_EXPANSION_PLAN_FINGERPRINT_MARKER);
    builder.mix_string(plan.name);
    builder.mix_u64(plan.facts.size());
    for (const MacroExpansionFact& fact : plan.facts) {
        mix_fact(builder, fact);
    }
    mix_summary(builder, summarize_macro_expansion_plan_counts(plan));
    return builder.finish();
}

std::string summarize_macro_expansion_plan(const MacroExpansionPlan& plan)
{
    const MacroExpansionSummary summary = summarize_macro_expansion_plan_counts(plan);
    std::ostringstream stream;
    stream << "macro_expansion_plan name="
           << (plan.name.empty() ? "<anonymous>" : plan.name)
           << " facts=" << summary.fact_count
           << " attribute_inputs=" << summary.attribute_input_count
           << " builtin_derive_passthrough=" << summary.builtin_derive_passthrough_count
           << " query_keys=" << summary.query_key_count
           << " generated_parts=" << summary.generated_part_count
           << " source_map_stubs=" << summary.source_map_stub_count
           << " sema_blockers=" << summary.sema_blocker_count
           << " future_external=" << summary.future_external_count
           << " aurex_declarative_macro_surfaces="
           << summary.aurex_declarative_macro_surface_count
           << " aurex_user_derive_macro_surfaces="
           << summary.aurex_user_derive_macro_surface_count
           << " aurex_compile_time_macro_execution_admissions="
           << summary.aurex_compile_time_macro_execution_admission_count
           << " aurex_macro_typed_matcher_admissions="
           << summary.aurex_macro_typed_matcher_admission_count
           << " aurex_macro_definition_site_hygiene_admissions="
           << summary.aurex_macro_definition_site_hygiene_admission_count
           << " aurex_macro_debuggable_diagnostic_anchors="
           << summary.aurex_macro_debuggable_diagnostic_anchor_count
           << " user_generated_code=" << summary.user_generated_code_count
           << " standard_library_required=" << summary.standard_library_required_count
           << " runtime_required=" << summary.runtime_required_count
           << " external_process_required=" << summary.external_process_required_count
           << " fingerprint=" << debug_string(macro_expansion_plan_fingerprint(plan));
    return stream.str();
}

std::string dump_macro_expansion_plan(const MacroExpansionPlan& plan)
{
    std::ostringstream stream;
    stream << "macro_expansion_plan name="
           << (plan.name.empty() ? "<anonymous>" : plan.name)
           << " facts=" << plan.facts.size()
           << " fingerprint=" << debug_string(macro_expansion_plan_fingerprint(plan)) << '\n';
    for (base::usize index = 0; index < plan.facts.size(); ++index) {
        const MacroExpansionFact& fact = plan.facts[index];
        stream << "  fact #" << index
               << " name=" << fact.fact_name
               << " kind=" << macro_expansion_fact_kind_name(fact.kind)
               << " stage=" << macro_expansion_stage_name(fact.stage)
               << " policy=" << macro_expansion_policy_name(fact.policy);
        append_fact_flags(stream, fact);
        stream << '\n';
        stream << "    input_fact=" << fact.input_fact << '\n';
        stream << "    output_fact=" << fact.output_fact << '\n';
        stream << "    blocker_fact=" << fact.blocker_fact << '\n';
    }
    return stream.str();
}

MacroExpansionPlan m21c_macro_expansion_plan_baseline()
{
    MacroExpansionPlan plan;
    plan.name = std::string(QUERY_MACRO_EXPANSION_M21C_PLAN_NAME);
    record_macro_expansion_fact(plan, make_attribute_input_fact());
    record_macro_expansion_fact(plan, make_builtin_derive_fact());
    record_macro_expansion_fact(plan, make_query_key_fact());
    record_macro_expansion_fact(plan, make_generated_part_fact());
    record_macro_expansion_fact(plan, make_source_map_fact());
    record_macro_expansion_fact(plan, make_unimplemented_blocker_fact());
    record_macro_expansion_fact(plan, make_external_blocker_fact());
    plan.summary = summarize_macro_expansion_plan_counts(plan);
    plan.fingerprint = macro_expansion_plan_fingerprint(plan);
    return plan;
}

MacroExpansionPlan m27_macro_expansion_plan_baseline()
{
    MacroExpansionPlan plan;
    plan.name = std::string(QUERY_MACRO_EXPANSION_M27_PLAN_NAME);
    record_macro_expansion_fact(plan, make_attribute_input_fact());
    record_macro_expansion_fact(plan, make_builtin_derive_fact());
    record_macro_expansion_fact(plan, make_query_key_fact());
    record_macro_expansion_fact(plan, make_generated_part_fact());
    record_macro_expansion_fact(plan, make_source_map_fact());
    record_macro_expansion_fact(plan, make_unimplemented_blocker_fact());
    record_macro_expansion_fact(plan, make_external_blocker_fact());
    record_macro_expansion_fact(plan, make_aurex_declarative_surface_fact());
    record_macro_expansion_fact(plan, make_aurex_user_derive_surface_fact());
    record_macro_expansion_fact(plan, make_aurex_compile_time_admission_fact());
    plan.summary = summarize_macro_expansion_plan_counts(plan);
    plan.fingerprint = macro_expansion_plan_fingerprint(plan);
    return plan;
}

MacroExpansionPlan m27b_macro_expansion_plan_baseline()
{
    MacroExpansionPlan plan;
    plan.name = std::string(QUERY_MACRO_EXPANSION_M27B_PLAN_NAME);
    record_macro_expansion_fact(plan, make_attribute_input_fact());
    record_macro_expansion_fact(plan, make_builtin_derive_fact());
    record_macro_expansion_fact(plan, make_query_key_fact());
    record_macro_expansion_fact(plan, make_generated_part_fact());
    record_macro_expansion_fact(plan, make_source_map_fact());
    record_macro_expansion_fact(plan, make_unimplemented_blocker_fact());
    record_macro_expansion_fact(plan, make_external_blocker_fact());
    record_macro_expansion_fact(plan, make_aurex_declarative_surface_fact());
    record_macro_expansion_fact(plan, make_aurex_user_derive_surface_fact());
    record_macro_expansion_fact(plan, make_aurex_compile_time_admission_fact());
    record_macro_expansion_fact(plan, make_aurex_typed_matcher_admission_fact());
    record_macro_expansion_fact(plan, make_aurex_definition_site_hygiene_fact());
    record_macro_expansion_fact(plan, make_aurex_debuggable_diagnostic_anchor_fact());
    plan.summary = summarize_macro_expansion_plan_counts(plan);
    plan.fingerprint = macro_expansion_plan_fingerprint(plan);
    return plan;
}

std::string_view m21c_item_attribute_macro_unimplemented_prefix() noexcept
{
    return QUERY_MACRO_EXPANSION_UNIMPLEMENTED_PREFIX;
}

std::string m21c_item_attribute_macro_unimplemented_message(const std::string_view attribute_name)
{
    return std::string(QUERY_MACRO_EXPANSION_UNIMPLEMENTED_PREFIX) + std::string(attribute_name);
}

} // namespace aurex::query
