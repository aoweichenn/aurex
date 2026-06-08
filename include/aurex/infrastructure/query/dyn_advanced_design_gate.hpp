#pragma once

#include <aurex/infrastructure/base/integer.hpp>
#include <aurex/infrastructure/query/stable_hash.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace aurex::query {

enum class DynAdvancedCapability : base::u8 {
    supertrait_upcasting = 1,
    owning_dyn,
    dynamic_drop_dispatch,
    allocator_policy,
    multi_trait_composition,
};

enum class DynAdvancedGateStage : base::u8 {
    research_only = 1,
    design_gate,
    prototype_blocked,
    ready_for_future_stage,
};

enum class DynAdvancedPolicyDecision : base::u8 {
    rejected_for_m9c = 1,
    requires_new_abi_policy,
    requires_new_metadata_policy,
    requires_standard_library_stage,
    requires_runtime_stage,
};

struct DynAdvancedImpactSummary {
    bool abi_policy_required = false;
    bool metadata_policy_required = false;
    bool borrow_model_impact = false;
    bool drop_model_impact = false;
    bool resource_model_impact = false;
    bool tooling_cache_impact = false;
    bool standard_library_required = false;
    bool runtime_required = false;
};

struct DynAdvancedDesignCandidate {
    DynAdvancedCapability capability = DynAdvancedCapability::supertrait_upcasting;
    DynAdvancedGateStage stage = DynAdvancedGateStage::research_only;
    DynAdvancedPolicyDecision decision = DynAdvancedPolicyDecision::rejected_for_m9c;
    std::string required_abi_policy;
    std::string required_metadata_policy;
    DynAdvancedImpactSummary impact;
    std::vector<std::string> blockers;
    std::vector<std::string> required_facts;
    std::vector<std::string> non_goals;
};

struct DynAdvancedDesignGate {
    std::string name;
    std::vector<DynAdvancedDesignCandidate> candidates;
    StableFingerprint128 fingerprint;
};

[[nodiscard]] std::string_view dyn_advanced_capability_name(DynAdvancedCapability capability) noexcept;
[[nodiscard]] std::string_view dyn_advanced_gate_stage_name(DynAdvancedGateStage stage) noexcept;
[[nodiscard]] std::string_view dyn_advanced_policy_decision_name(DynAdvancedPolicyDecision decision) noexcept;

[[nodiscard]] bool is_valid(DynAdvancedCapability capability) noexcept;
[[nodiscard]] bool is_valid(DynAdvancedGateStage stage) noexcept;
[[nodiscard]] bool is_valid(DynAdvancedPolicyDecision decision) noexcept;
[[nodiscard]] bool is_valid(const DynAdvancedDesignCandidate& candidate) noexcept;
[[nodiscard]] bool is_valid(const DynAdvancedDesignGate& gate) noexcept;

void record_dyn_advanced_design_candidate(DynAdvancedDesignGate& gate, DynAdvancedDesignCandidate candidate);

[[nodiscard]] StableFingerprint128 dyn_advanced_design_gate_fingerprint(
    const DynAdvancedDesignGate& gate) noexcept;
[[nodiscard]] std::string summarize_dyn_advanced_design_gate(const DynAdvancedDesignGate& gate);
[[nodiscard]] std::string dump_dyn_advanced_design_gate(const DynAdvancedDesignGate& gate);
[[nodiscard]] DynAdvancedDesignGate m9c_dyn_advanced_design_gate_baseline();

} // namespace aurex::query
