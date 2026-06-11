#pragma once

#include <aurex/infrastructure/base/integer.hpp>
#include <aurex/infrastructure/query/stable_hash.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace aurex::query {

enum class MacroDesignCapability : base::u8 {
    token_tree_and_attribute_surface = 1,
    hygienic_name_resolution,
    expansion_source_map_and_debug_trace,
    query_backed_incremental_expansion,
    attached_item_codegen_surface,
    typed_expression_macro_boundary,
    external_procedural_macro_sandbox,
};

enum class MacroDesignGateStage : base::u8 {
    research_only = 1,
    design_gate,
    ready_for_implementation,
    blocked_by_dependency,
    future_stage,
};

enum class MacroDesignPolicyDecision : base::u8 {
    rejected = 1,
    selected_m21_frontend_query_path,
    requires_typed_macro_engine,
    requires_process_sandbox,
    requires_later_language_surface,
};

struct MacroDesignImpactSummary {
    bool lexer_parser_impact = false;
    bool ast_model_impact = false;
    bool sema_name_resolution_impact = false;
    bool query_key_impact = false;
    bool incremental_cache_impact = false;
    bool tooling_debug_impact = false;
    bool source_map_impact = false;
    bool hygiene_required = false;
    bool standard_library_required = false;
    bool runtime_required = false;
    bool external_process_required = false;
};

struct MacroDesignCandidate {
    MacroDesignCapability capability = MacroDesignCapability::token_tree_and_attribute_surface;
    MacroDesignGateStage stage = MacroDesignGateStage::research_only;
    MacroDesignPolicyDecision decision = MacroDesignPolicyDecision::rejected;
    std::string selected_policy;
    MacroDesignImpactSummary impact;
    std::vector<std::string> blockers;
    std::vector<std::string> required_facts;
    std::vector<std::string> non_goals;
};

struct MacroDesignGate {
    std::string name;
    std::vector<MacroDesignCandidate> candidates;
    StableFingerprint128 fingerprint;
};

[[nodiscard]] std::string_view macro_design_capability_name(MacroDesignCapability capability) noexcept;
[[nodiscard]] std::string_view macro_design_gate_stage_name(MacroDesignGateStage stage) noexcept;
[[nodiscard]] std::string_view macro_design_policy_decision_name(MacroDesignPolicyDecision decision) noexcept;

[[nodiscard]] bool is_valid(MacroDesignCapability capability) noexcept;
[[nodiscard]] bool is_valid(MacroDesignGateStage stage) noexcept;
[[nodiscard]] bool is_valid(MacroDesignPolicyDecision decision) noexcept;
[[nodiscard]] bool is_valid(const MacroDesignCandidate& candidate) noexcept;
[[nodiscard]] bool is_valid(const MacroDesignGate& gate) noexcept;
[[nodiscard]] bool is_valid_m21a_macro_design_gate(const MacroDesignGate& gate) noexcept;

void record_macro_design_candidate(MacroDesignGate& gate, MacroDesignCandidate candidate);

[[nodiscard]] StableFingerprint128 macro_design_gate_fingerprint(const MacroDesignGate& gate) noexcept;
[[nodiscard]] std::string summarize_macro_design_gate(const MacroDesignGate& gate);
[[nodiscard]] std::string dump_macro_design_gate(const MacroDesignGate& gate);
[[nodiscard]] MacroDesignGate m21a_macro_design_gate_baseline();

} // namespace aurex::query
