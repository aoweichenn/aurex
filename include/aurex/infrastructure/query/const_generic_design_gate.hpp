#pragma once

#include <aurex/infrastructure/base/integer.hpp>
#include <aurex/infrastructure/query/stable_hash.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace aurex::query {

enum class ConstGenericCapability : base::u8 {
    typed_const_parameter_surface = 1,
    canonical_const_argument_identity,
    generic_instance_key_integration,
    array_length_type_integration,
    const_expression_evaluation_subset,
    trait_predicate_and_dyn_boundary,
};

enum class ConstGenericGateStage : base::u8 {
    research_only = 1,
    design_gate,
    ready_for_implementation,
    blocked_by_dependency,
    future_stage,
};

enum class ConstGenericPolicyDecision : base::u8 {
    rejected = 1,
    selected_m15_frontend_query_path,
    requires_comptime_engine,
    requires_trait_solver_extension,
    requires_runtime_or_std_boundary,
};

struct ConstGenericImpactSummary {
    bool parser_ast_impact = false;
    bool sema_type_system_impact = false;
    bool query_key_impact = false;
    bool incremental_cache_impact = false;
    bool ir_layout_impact = false;
    bool comptime_engine_required = false;
    bool trait_solver_impact = false;
    bool standard_library_required = false;
    bool runtime_required = false;
};

struct ConstGenericDesignCandidate {
    ConstGenericCapability capability = ConstGenericCapability::typed_const_parameter_surface;
    ConstGenericGateStage stage = ConstGenericGateStage::research_only;
    ConstGenericPolicyDecision decision = ConstGenericPolicyDecision::rejected;
    std::string selected_policy;
    ConstGenericImpactSummary impact;
    std::vector<std::string> blockers;
    std::vector<std::string> required_facts;
    std::vector<std::string> non_goals;
};

struct ConstGenericDesignGate {
    std::string name;
    std::vector<ConstGenericDesignCandidate> candidates;
    StableFingerprint128 fingerprint;
};

[[nodiscard]] std::string_view const_generic_capability_name(ConstGenericCapability capability) noexcept;
[[nodiscard]] std::string_view const_generic_gate_stage_name(ConstGenericGateStage stage) noexcept;
[[nodiscard]] std::string_view const_generic_policy_decision_name(ConstGenericPolicyDecision decision) noexcept;

[[nodiscard]] bool is_valid(ConstGenericCapability capability) noexcept;
[[nodiscard]] bool is_valid(ConstGenericGateStage stage) noexcept;
[[nodiscard]] bool is_valid(ConstGenericPolicyDecision decision) noexcept;
[[nodiscard]] bool is_valid(const ConstGenericDesignCandidate& candidate) noexcept;
[[nodiscard]] bool is_valid(const ConstGenericDesignGate& gate) noexcept;
[[nodiscard]] bool is_valid_m15_const_generic_design_gate(const ConstGenericDesignGate& gate) noexcept;

void record_const_generic_design_candidate(
    ConstGenericDesignGate& gate, ConstGenericDesignCandidate candidate);

[[nodiscard]] StableFingerprint128 const_generic_design_gate_fingerprint(
    const ConstGenericDesignGate& gate) noexcept;
[[nodiscard]] std::string summarize_const_generic_design_gate(const ConstGenericDesignGate& gate);
[[nodiscard]] std::string dump_const_generic_design_gate(const ConstGenericDesignGate& gate);
[[nodiscard]] ConstGenericDesignGate m15_const_generic_design_gate_baseline();

} // namespace aurex::query
