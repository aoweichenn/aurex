#include <aurex/infrastructure/query/macro_design_gate.hpp>

#include <algorithm>
#include <array>
#include <sstream>
#include <utility>

namespace aurex::query {
namespace {

constexpr std::string_view QUERY_MACRO_DESIGN_GATE_FINGERPRINT_MARKER = "query.macro_design_gate.v1";
constexpr std::string_view QUERY_MACRO_DESIGN_M21A_GATE_NAME = "M21a Macro System Design Gate";
constexpr std::string_view QUERY_MACRO_DESIGN_NO_STDLIB_NON_GOAL = "standard_library_not_in_m21a";
constexpr std::string_view QUERY_MACRO_DESIGN_NO_RUNTIME_NON_GOAL = "runtime_not_in_m21a";
constexpr std::string_view QUERY_MACRO_DESIGN_NO_TEXTUAL_MACRO_NON_GOAL = "do_not_support_textual_macros";
constexpr std::string_view QUERY_MACRO_DESIGN_TOKEN_TREE_POLICY = "token_tree_attribute_surface_v1";
constexpr std::string_view QUERY_MACRO_DESIGN_HYGIENE_POLICY = "origin_mark_hygiene_v1";
constexpr std::string_view QUERY_MACRO_DESIGN_TRACE_POLICY = "expansion_source_map_debug_trace_v1";
constexpr std::string_view QUERY_MACRO_DESIGN_QUERY_POLICY = "macro_expansion_query_fingerprint_v1";
constexpr std::string_view QUERY_MACRO_DESIGN_ATTACHED_POLICY = "attached_item_codegen_declared_names_v1";
constexpr std::string_view QUERY_MACRO_DESIGN_TYPED_EXPR_POLICY = "typed_expression_macro_boundary_v1";
constexpr std::string_view QUERY_MACRO_DESIGN_EXTERNAL_POLICY = "external_proc_macro_sandbox_boundary_v1";
constexpr base::usize QUERY_MACRO_DESIGN_M21A_CAPABILITY_COUNT = 7U;
constexpr base::u8 QUERY_MACRO_DESIGN_INVALID_CAPABILITY_VALUE = 255U;
constexpr base::u8 QUERY_MACRO_DESIGN_INVALID_STAGE_VALUE = 255U;
constexpr base::u8 QUERY_MACRO_DESIGN_INVALID_DECISION_VALUE = 255U;

[[nodiscard]] base::u8 stable_capability_value(const MacroDesignCapability capability) noexcept
{
    return is_valid(capability) ? static_cast<base::u8>(capability)
                                : QUERY_MACRO_DESIGN_INVALID_CAPABILITY_VALUE;
}

[[nodiscard]] base::u8 stable_stage_value(const MacroDesignGateStage stage) noexcept
{
    return is_valid(stage) ? static_cast<base::u8>(stage) : QUERY_MACRO_DESIGN_INVALID_STAGE_VALUE;
}

[[nodiscard]] base::u8 stable_decision_value(const MacroDesignPolicyDecision decision) noexcept
{
    return is_valid(decision) ? static_cast<base::u8>(decision) : QUERY_MACRO_DESIGN_INVALID_DECISION_VALUE;
}

[[nodiscard]] bool contains_text(
    const std::vector<std::string>& values, const std::string_view expected) noexcept
{
    return std::any_of(values.begin(), values.end(), [expected](const std::string& value) {
        return std::string_view(value) == expected;
    });
}

[[nodiscard]] bool has_required_detail_vectors(const MacroDesignCandidate& candidate) noexcept
{
    return !candidate.blockers.empty() && !candidate.required_facts.empty() && !candidate.non_goals.empty();
}

[[nodiscard]] bool decision_matches_impact(const MacroDesignCandidate& candidate) noexcept
{
    if ((candidate.impact.standard_library_required || candidate.impact.runtime_required)
        && candidate.decision != MacroDesignPolicyDecision::requires_later_language_surface) {
        return false;
    }
    if (candidate.impact.external_process_required
        && candidate.decision != MacroDesignPolicyDecision::requires_process_sandbox) {
        return false;
    }
    return true;
}

[[nodiscard]] bool candidate_shape_is_valid(const MacroDesignCandidate& candidate) noexcept
{
    return is_valid(candidate.capability) && is_valid(candidate.stage) && is_valid(candidate.decision)
        && !candidate.selected_policy.empty() && has_required_detail_vectors(candidate)
        && decision_matches_impact(candidate)
        && contains_text(candidate.non_goals, QUERY_MACRO_DESIGN_NO_STDLIB_NON_GOAL)
        && contains_text(candidate.non_goals, QUERY_MACRO_DESIGN_NO_RUNTIME_NON_GOAL)
        && contains_text(candidate.non_goals, QUERY_MACRO_DESIGN_NO_TEXTUAL_MACRO_NON_GOAL);
}

[[nodiscard]] bool gate_has_each_macro_capability_once(const MacroDesignGate& gate) noexcept
{
    if (gate.candidates.size() != QUERY_MACRO_DESIGN_M21A_CAPABILITY_COUNT) {
        return false;
    }

    std::array<bool, QUERY_MACRO_DESIGN_M21A_CAPABILITY_COUNT> seen{};
    for (const MacroDesignCandidate& candidate : gate.candidates) {
        if (!is_valid(candidate.capability)) {
            return false;
        }
        const base::usize index = static_cast<base::usize>(candidate.capability) - 1U;
        if (index >= seen.size() || seen[index]) {
            return false;
        }
        seen[index] = true;
    }
    return std::all_of(seen.begin(), seen.end(), [](const bool present) {
        return present;
    });
}

[[nodiscard]] bool m21a_capability_gate_is_valid(const MacroDesignCandidate& candidate) noexcept
{
    switch (candidate.capability) {
        case MacroDesignCapability::token_tree_and_attribute_surface:
            return candidate.stage == MacroDesignGateStage::ready_for_implementation
                && candidate.decision == MacroDesignPolicyDecision::selected_m21_frontend_query_path
                && std::string_view(candidate.selected_policy) == QUERY_MACRO_DESIGN_TOKEN_TREE_POLICY
                && candidate.impact.lexer_parser_impact && candidate.impact.ast_model_impact
                && !candidate.impact.sema_name_resolution_impact && candidate.impact.query_key_impact
                && candidate.impact.incremental_cache_impact && candidate.impact.tooling_debug_impact
                && candidate.impact.source_map_impact && !candidate.impact.hygiene_required
                && !candidate.impact.standard_library_required && !candidate.impact.runtime_required
                && !candidate.impact.external_process_required;
        case MacroDesignCapability::hygienic_name_resolution:
            return candidate.stage == MacroDesignGateStage::ready_for_implementation
                && candidate.decision == MacroDesignPolicyDecision::selected_m21_frontend_query_path
                && std::string_view(candidate.selected_policy) == QUERY_MACRO_DESIGN_HYGIENE_POLICY
                && !candidate.impact.lexer_parser_impact && candidate.impact.ast_model_impact
                && candidate.impact.sema_name_resolution_impact && candidate.impact.query_key_impact
                && candidate.impact.incremental_cache_impact && candidate.impact.tooling_debug_impact
                && candidate.impact.source_map_impact && candidate.impact.hygiene_required
                && !candidate.impact.standard_library_required && !candidate.impact.runtime_required
                && !candidate.impact.external_process_required;
        case MacroDesignCapability::expansion_source_map_and_debug_trace:
            return candidate.stage == MacroDesignGateStage::ready_for_implementation
                && candidate.decision == MacroDesignPolicyDecision::selected_m21_frontend_query_path
                && std::string_view(candidate.selected_policy) == QUERY_MACRO_DESIGN_TRACE_POLICY
                && !candidate.impact.lexer_parser_impact && candidate.impact.ast_model_impact
                && candidate.impact.sema_name_resolution_impact && candidate.impact.query_key_impact
                && candidate.impact.incremental_cache_impact && candidate.impact.tooling_debug_impact
                && candidate.impact.source_map_impact && candidate.impact.hygiene_required
                && !candidate.impact.standard_library_required && !candidate.impact.runtime_required
                && !candidate.impact.external_process_required;
        case MacroDesignCapability::query_backed_incremental_expansion:
            return candidate.stage == MacroDesignGateStage::ready_for_implementation
                && candidate.decision == MacroDesignPolicyDecision::selected_m21_frontend_query_path
                && std::string_view(candidate.selected_policy) == QUERY_MACRO_DESIGN_QUERY_POLICY
                && candidate.impact.lexer_parser_impact && candidate.impact.ast_model_impact
                && candidate.impact.sema_name_resolution_impact && candidate.impact.query_key_impact
                && candidate.impact.incremental_cache_impact && candidate.impact.tooling_debug_impact
                && candidate.impact.source_map_impact && candidate.impact.hygiene_required
                && !candidate.impact.standard_library_required && !candidate.impact.runtime_required
                && !candidate.impact.external_process_required;
        case MacroDesignCapability::attached_item_codegen_surface:
            return candidate.stage == MacroDesignGateStage::ready_for_implementation
                && candidate.decision == MacroDesignPolicyDecision::selected_m21_frontend_query_path
                && std::string_view(candidate.selected_policy) == QUERY_MACRO_DESIGN_ATTACHED_POLICY
                && candidate.impact.lexer_parser_impact && candidate.impact.ast_model_impact
                && candidate.impact.sema_name_resolution_impact && candidate.impact.query_key_impact
                && candidate.impact.incremental_cache_impact && candidate.impact.tooling_debug_impact
                && candidate.impact.source_map_impact && candidate.impact.hygiene_required
                && !candidate.impact.standard_library_required && !candidate.impact.runtime_required
                && !candidate.impact.external_process_required;
        case MacroDesignCapability::typed_expression_macro_boundary:
            return candidate.stage == MacroDesignGateStage::future_stage
                && candidate.decision == MacroDesignPolicyDecision::requires_typed_macro_engine
                && std::string_view(candidate.selected_policy) == QUERY_MACRO_DESIGN_TYPED_EXPR_POLICY
                && candidate.impact.lexer_parser_impact && candidate.impact.ast_model_impact
                && candidate.impact.sema_name_resolution_impact && candidate.impact.query_key_impact
                && candidate.impact.incremental_cache_impact && candidate.impact.tooling_debug_impact
                && candidate.impact.source_map_impact && candidate.impact.hygiene_required
                && !candidate.impact.standard_library_required && !candidate.impact.runtime_required
                && !candidate.impact.external_process_required;
        case MacroDesignCapability::external_procedural_macro_sandbox:
            return candidate.stage == MacroDesignGateStage::blocked_by_dependency
                && candidate.decision == MacroDesignPolicyDecision::requires_process_sandbox
                && std::string_view(candidate.selected_policy) == QUERY_MACRO_DESIGN_EXTERNAL_POLICY
                && !candidate.impact.lexer_parser_impact && !candidate.impact.ast_model_impact
                && !candidate.impact.sema_name_resolution_impact && candidate.impact.query_key_impact
                && candidate.impact.incremental_cache_impact && candidate.impact.tooling_debug_impact
                && candidate.impact.source_map_impact && candidate.impact.hygiene_required
                && !candidate.impact.standard_library_required && !candidate.impact.runtime_required
                && candidate.impact.external_process_required;
    }
    return false;
}

void mix_string_vector(StableHashBuilder& builder, const std::vector<std::string>& values) noexcept
{
    builder.mix_u64(static_cast<base::u64>(values.size()));
    for (const std::string& value : values) {
        builder.mix_string(value);
    }
}

void mix_impact(StableHashBuilder& builder, const MacroDesignImpactSummary& impact) noexcept
{
    builder.mix_bool(impact.lexer_parser_impact);
    builder.mix_bool(impact.ast_model_impact);
    builder.mix_bool(impact.sema_name_resolution_impact);
    builder.mix_bool(impact.query_key_impact);
    builder.mix_bool(impact.incremental_cache_impact);
    builder.mix_bool(impact.tooling_debug_impact);
    builder.mix_bool(impact.source_map_impact);
    builder.mix_bool(impact.hygiene_required);
    builder.mix_bool(impact.standard_library_required);
    builder.mix_bool(impact.runtime_required);
    builder.mix_bool(impact.external_process_required);
}

void mix_candidate(StableHashBuilder& builder, const MacroDesignCandidate& candidate) noexcept
{
    builder.mix_u8(stable_capability_value(candidate.capability));
    builder.mix_u8(stable_stage_value(candidate.stage));
    builder.mix_u8(stable_decision_value(candidate.decision));
    builder.mix_string(candidate.selected_policy);
    mix_impact(builder, candidate.impact);
    mix_string_vector(builder, candidate.blockers);
    mix_string_vector(builder, candidate.required_facts);
    mix_string_vector(builder, candidate.non_goals);
}

void append_impact_summary(std::ostringstream& stream, const MacroDesignImpactSummary& impact)
{
    stream << " impact="
           << "lexer_parser:" << (impact.lexer_parser_impact ? "yes" : "no")
           << ",ast_model:" << (impact.ast_model_impact ? "yes" : "no")
           << ",sema_name_resolution:" << (impact.sema_name_resolution_impact ? "yes" : "no")
           << ",query_key:" << (impact.query_key_impact ? "yes" : "no")
           << ",incremental_cache:" << (impact.incremental_cache_impact ? "yes" : "no")
           << ",tooling_debug:" << (impact.tooling_debug_impact ? "yes" : "no")
           << ",source_map:" << (impact.source_map_impact ? "yes" : "no")
           << ",hygiene:" << (impact.hygiene_required ? "yes" : "no")
           << ",standard_library:" << (impact.standard_library_required ? "yes" : "no")
           << ",runtime:" << (impact.runtime_required ? "yes" : "no")
           << ",external_process:" << (impact.external_process_required ? "yes" : "no");
}

[[nodiscard]] std::vector<std::string> m21a_common_non_goals()
{
    return {
        std::string(QUERY_MACRO_DESIGN_NO_STDLIB_NON_GOAL),
        std::string(QUERY_MACRO_DESIGN_NO_RUNTIME_NON_GOAL),
        std::string(QUERY_MACRO_DESIGN_NO_TEXTUAL_MACRO_NON_GOAL),
        "do_not_expand_external_proc_macros_in_m21a",
        "do_not_allow_macros_to_bypass_sema_or_borrow_checking",
    };
}

[[nodiscard]] MacroDesignCandidate make_token_tree_candidate()
{
    return MacroDesignCandidate{
        MacroDesignCapability::token_tree_and_attribute_surface,
        MacroDesignGateStage::ready_for_implementation,
        MacroDesignPolicyDecision::selected_m21_frontend_query_path,
        std::string(QUERY_MACRO_DESIGN_TOKEN_TREE_POLICY),
        MacroDesignImpactSummary{true, true, false, true, true, true, true, false, false, false, false},
        {
            "existing #[derive(...)] path stores DeriveDecl only and must become a general AttributeDecl surface",
            "macro input and output must be token-tree based instead of exposing private AST nodes",
            "lossless syntax and token spans must remain reconstructable after generated module parts are parsed",
        },
        {
            "macro_token_tree_fact",
            "macro_attribute_decl_fact",
            "macro_generated_part_parse_fact",
        },
        m21a_common_non_goals(),
    };
}

[[nodiscard]] MacroDesignCandidate make_hygiene_candidate()
{
    return MacroDesignCandidate{
        MacroDesignCapability::hygienic_name_resolution,
        MacroDesignGateStage::ready_for_implementation,
        MacroDesignPolicyDecision::selected_m21_frontend_query_path,
        std::string(QUERY_MACRO_DESIGN_HYGIENE_POLICY),
        MacroDesignImpactSummary{false, true, true, true, true, true, true, true, false, false, false},
        {
            "generated identifiers must not capture call-site locals by accident",
            "call-site fragments and definition-site helper references need distinct resolution marks",
            "declared exported names must be visible to lookup without exposing temporary generated names",
        },
        {
            "macro_hygiene_mark_fact",
            "macro_origin_id_fact",
            "macro_declared_name_fact",
        },
        m21a_common_non_goals(),
    };
}

[[nodiscard]] MacroDesignCandidate make_trace_candidate()
{
    return MacroDesignCandidate{
        MacroDesignCapability::expansion_source_map_and_debug_trace,
        MacroDesignGateStage::ready_for_implementation,
        MacroDesignPolicyDecision::selected_m21_frontend_query_path,
        std::string(QUERY_MACRO_DESIGN_TRACE_POLICY),
        MacroDesignImpactSummary{false, true, true, true, true, true, true, true, false, false, false},
        {
            "SourceManager currently stores file path and text only; generated ranges need expansion origins",
            "diagnostics from generated code must report an expansion stack back to the attached source item",
            "--emit-expanded and IDE semantic facts require stable expansion ids",
        },
        {
            "macro_expansion_origin_fact",
            "macro_expansion_trace_fact",
            "macro_generated_source_map_fact",
        },
        m21a_common_non_goals(),
    };
}

[[nodiscard]] MacroDesignCandidate make_query_candidate()
{
    return MacroDesignCandidate{
        MacroDesignCapability::query_backed_incremental_expansion,
        MacroDesignGateStage::ready_for_implementation,
        MacroDesignPolicyDecision::selected_m21_frontend_query_path,
        std::string(QUERY_MACRO_DESIGN_QUERY_POLICY),
        MacroDesignImpactSummary{true, true, true, true, true, true, true, true, false, false, false},
        {
            "macro output must be keyed by macro definition, input token tree, role, hygiene parent, and schema",
            "declared generated names let lookup avoid expanding unrelated attached macros",
            "generated module parts should use existing SourceRole::generated and ModulePartKind::generated identities",
        },
        {
            "macro_manifest_query_fact",
            "macro_expand_item_query_fact",
            "macro_expansion_fingerprint_fact",
        },
        m21a_common_non_goals(),
    };
}

[[nodiscard]] MacroDesignCandidate make_attached_item_candidate()
{
    return MacroDesignCandidate{
        MacroDesignCapability::attached_item_codegen_surface,
        MacroDesignGateStage::ready_for_implementation,
        MacroDesignPolicyDecision::selected_m21_frontend_query_path,
        std::string(QUERY_MACRO_DESIGN_ATTACHED_POLICY),
        MacroDesignImpactSummary{true, true, true, true, true, true, true, true, false, false, false},
        {
            "M20e builtin derive proves item attributes are the immediate user-facing codegen pressure point",
            "attached macros must declare peer/member/accessor-like roles and generated names before expansion",
            "early item macro expansion must finish before type and value name registration",
        },
        {
            "attached_macro_role_fact",
            "attached_macro_declared_name_fact",
            "attached_macro_generated_item_fact",
        },
        m21a_common_non_goals(),
    };
}

[[nodiscard]] MacroDesignCandidate make_typed_expression_candidate()
{
    return MacroDesignCandidate{
        MacroDesignCapability::typed_expression_macro_boundary,
        MacroDesignGateStage::future_stage,
        MacroDesignPolicyDecision::requires_typed_macro_engine,
        std::string(QUERY_MACRO_DESIGN_TYPED_EXPR_POLICY),
        MacroDesignImpactSummary{true, true, true, true, true, true, true, true, false, false, false},
        {
            "typed expression macros need an explicit expected-type and checked-fragment protocol",
            "expression macro expansion during sema must not mutate declaration-phase lookup state",
            "borrow and move analysis must consume checked generated expressions with expansion origins",
        },
        {
            "typed_macro_expected_type_fact",
            "typed_macro_checked_fragment_fact",
            "typed_macro_sema_phase_boundary_fact",
        },
        m21a_common_non_goals(),
    };
}

[[nodiscard]] MacroDesignCandidate make_external_proc_candidate()
{
    return MacroDesignCandidate{
        MacroDesignCapability::external_procedural_macro_sandbox,
        MacroDesignGateStage::blocked_by_dependency,
        MacroDesignPolicyDecision::requires_process_sandbox,
        std::string(QUERY_MACRO_DESIGN_EXTERNAL_POLICY),
        MacroDesignImpactSummary{false, false, false, true, true, true, true, true, false, false, true},
        {
            "external procedural macros must not run inside the compiler address space by default",
            "filesystem, environment, network, timeout, and memory permissions need a manifest and sandbox",
            "macro implementation identity must be fingerprinted before expansion output can be cached",
        },
        {
            "external_macro_sandbox_manifest_fact",
            "external_macro_permission_fact",
            "external_macro_implementation_fingerprint_fact",
        },
        m21a_common_non_goals(),
    };
}

} // namespace

std::string_view macro_design_capability_name(const MacroDesignCapability capability) noexcept
{
    switch (capability) {
        case MacroDesignCapability::token_tree_and_attribute_surface:
            return "token_tree_and_attribute_surface";
        case MacroDesignCapability::hygienic_name_resolution:
            return "hygienic_name_resolution";
        case MacroDesignCapability::expansion_source_map_and_debug_trace:
            return "expansion_source_map_and_debug_trace";
        case MacroDesignCapability::query_backed_incremental_expansion:
            return "query_backed_incremental_expansion";
        case MacroDesignCapability::attached_item_codegen_surface:
            return "attached_item_codegen_surface";
        case MacroDesignCapability::typed_expression_macro_boundary:
            return "typed_expression_macro_boundary";
        case MacroDesignCapability::external_procedural_macro_sandbox:
            return "external_procedural_macro_sandbox";
    }
    return "invalid";
}

std::string_view macro_design_gate_stage_name(const MacroDesignGateStage stage) noexcept
{
    switch (stage) {
        case MacroDesignGateStage::research_only:
            return "research_only";
        case MacroDesignGateStage::design_gate:
            return "design_gate";
        case MacroDesignGateStage::ready_for_implementation:
            return "ready_for_implementation";
        case MacroDesignGateStage::blocked_by_dependency:
            return "blocked_by_dependency";
        case MacroDesignGateStage::future_stage:
            return "future_stage";
    }
    return "invalid";
}

std::string_view macro_design_policy_decision_name(const MacroDesignPolicyDecision decision) noexcept
{
    switch (decision) {
        case MacroDesignPolicyDecision::rejected:
            return "rejected";
        case MacroDesignPolicyDecision::selected_m21_frontend_query_path:
            return "selected_m21_frontend_query_path";
        case MacroDesignPolicyDecision::requires_typed_macro_engine:
            return "requires_typed_macro_engine";
        case MacroDesignPolicyDecision::requires_process_sandbox:
            return "requires_process_sandbox";
        case MacroDesignPolicyDecision::requires_later_language_surface:
            return "requires_later_language_surface";
    }
    return "invalid";
}

bool is_valid(const MacroDesignCapability capability) noexcept
{
    switch (capability) {
        case MacroDesignCapability::token_tree_and_attribute_surface:
        case MacroDesignCapability::hygienic_name_resolution:
        case MacroDesignCapability::expansion_source_map_and_debug_trace:
        case MacroDesignCapability::query_backed_incremental_expansion:
        case MacroDesignCapability::attached_item_codegen_surface:
        case MacroDesignCapability::typed_expression_macro_boundary:
        case MacroDesignCapability::external_procedural_macro_sandbox:
            return true;
    }
    return false;
}

bool is_valid(const MacroDesignGateStage stage) noexcept
{
    switch (stage) {
        case MacroDesignGateStage::research_only:
        case MacroDesignGateStage::design_gate:
        case MacroDesignGateStage::ready_for_implementation:
        case MacroDesignGateStage::blocked_by_dependency:
        case MacroDesignGateStage::future_stage:
            return true;
    }
    return false;
}

bool is_valid(const MacroDesignPolicyDecision decision) noexcept
{
    switch (decision) {
        case MacroDesignPolicyDecision::rejected:
        case MacroDesignPolicyDecision::selected_m21_frontend_query_path:
        case MacroDesignPolicyDecision::requires_typed_macro_engine:
        case MacroDesignPolicyDecision::requires_process_sandbox:
        case MacroDesignPolicyDecision::requires_later_language_surface:
            return true;
    }
    return false;
}

bool is_valid(const MacroDesignCandidate& candidate) noexcept
{
    return candidate_shape_is_valid(candidate) && m21a_capability_gate_is_valid(candidate);
}

bool is_valid(const MacroDesignGate& gate) noexcept
{
    return is_valid_m21a_macro_design_gate(gate);
}

bool is_valid_m21a_macro_design_gate(const MacroDesignGate& gate) noexcept
{
    return std::string_view(gate.name) == QUERY_MACRO_DESIGN_M21A_GATE_NAME
        && gate_has_each_macro_capability_once(gate)
        && std::all_of(gate.candidates.begin(), gate.candidates.end(), [](const MacroDesignCandidate& candidate) {
               return is_valid(candidate);
           });
}

void record_macro_design_candidate(MacroDesignGate& gate, MacroDesignCandidate candidate)
{
    gate.candidates.push_back(std::move(candidate));
}

StableFingerprint128 macro_design_gate_fingerprint(const MacroDesignGate& gate) noexcept
{
    StableHashBuilder builder;
    builder.mix_string(QUERY_MACRO_DESIGN_GATE_FINGERPRINT_MARKER);
    builder.mix_string(gate.name);
    builder.mix_u64(gate.candidates.size());
    for (const MacroDesignCandidate& candidate : gate.candidates) {
        mix_candidate(builder, candidate);
    }
    return builder.finish();
}

std::string summarize_macro_design_gate(const MacroDesignGate& gate)
{
    base::u64 ready_count = 0;
    base::u64 blocked_count = 0;
    base::u64 future_count = 0;
    base::u64 query_key_impact_count = 0;
    base::u64 hygiene_required_count = 0;
    base::u64 source_map_impact_count = 0;
    base::u64 external_process_required_count = 0;
    for (const MacroDesignCandidate& candidate : gate.candidates) {
        if (candidate.stage == MacroDesignGateStage::ready_for_implementation) {
            ++ready_count;
        }
        if (candidate.stage == MacroDesignGateStage::blocked_by_dependency) {
            ++blocked_count;
        }
        if (candidate.stage == MacroDesignGateStage::future_stage) {
            ++future_count;
        }
        if (candidate.impact.query_key_impact) {
            ++query_key_impact_count;
        }
        if (candidate.impact.hygiene_required) {
            ++hygiene_required_count;
        }
        if (candidate.impact.source_map_impact) {
            ++source_map_impact_count;
        }
        if (candidate.impact.external_process_required) {
            ++external_process_required_count;
        }
    }

    std::ostringstream label;
    label << "macro_design_gate name="
          << (gate.name.empty() ? "<anonymous>" : gate.name)
          << " candidates=" << gate.candidates.size()
          << " ready_for_implementation=" << ready_count
          << " blocked_by_dependency=" << blocked_count
          << " future_stage=" << future_count
          << " query_key_impact=" << query_key_impact_count
          << " hygiene_required=" << hygiene_required_count
          << " source_map_impact=" << source_map_impact_count
          << " external_process_required=" << external_process_required_count
          << " fingerprint=" << debug_string(macro_design_gate_fingerprint(gate));
    return label.str();
}

std::string dump_macro_design_gate(const MacroDesignGate& gate)
{
    std::ostringstream stream;
    stream << "macro_design_gate name="
           << (gate.name.empty() ? "<anonymous>" : gate.name)
           << " candidates=" << gate.candidates.size()
           << " fingerprint=" << debug_string(macro_design_gate_fingerprint(gate)) << '\n';
    for (base::usize index = 0; index < gate.candidates.size(); ++index) {
        const MacroDesignCandidate& candidate = gate.candidates[index];
        stream << "  candidate #" << index
               << " capability=" << macro_design_capability_name(candidate.capability)
               << " stage=" << macro_design_gate_stage_name(candidate.stage)
               << " decision=" << macro_design_policy_decision_name(candidate.decision)
               << " selected_policy=" << candidate.selected_policy;
        append_impact_summary(stream, candidate.impact);
        stream << '\n';
        for (const std::string& blocker : candidate.blockers) {
            stream << "    blocker=" << blocker << '\n';
        }
        for (const std::string& fact : candidate.required_facts) {
            stream << "    required_fact=" << fact << '\n';
        }
        for (const std::string& non_goal : candidate.non_goals) {
            stream << "    non_goal=" << non_goal << '\n';
        }
    }
    return stream.str();
}

MacroDesignGate m21a_macro_design_gate_baseline()
{
    MacroDesignGate gate;
    gate.name = std::string(QUERY_MACRO_DESIGN_M21A_GATE_NAME);
    record_macro_design_candidate(gate, make_token_tree_candidate());
    record_macro_design_candidate(gate, make_hygiene_candidate());
    record_macro_design_candidate(gate, make_trace_candidate());
    record_macro_design_candidate(gate, make_query_candidate());
    record_macro_design_candidate(gate, make_attached_item_candidate());
    record_macro_design_candidate(gate, make_typed_expression_candidate());
    record_macro_design_candidate(gate, make_external_proc_candidate());
    gate.fingerprint = macro_design_gate_fingerprint(gate);
    return gate;
}

} // namespace aurex::query
