#pragma once

#include <aurex/infrastructure/base/integer.hpp>

namespace aurex::frontend::macro {

struct AurexMacroOutputContractBlockedEffectSummary {
    base::u64 generated_source_text_count = 0;
    base::u64 parse_ready_token_buffer_count = 0;
    base::u64 ast_mutation_count = 0;
    base::u64 sema_visible_generated_part_count = 0;
    base::u64 standard_library_required_count = 0;
    base::u64 runtime_required_count = 0;
    base::u64 external_process_required_count = 0;
    base::u64 user_generated_code_count = 0;

    [[nodiscard]] friend bool operator==(const AurexMacroOutputContractBlockedEffectSummary& lhs,
        const AurexMacroOutputContractBlockedEffectSummary& rhs) noexcept = default;
};

struct AurexMacroOutputContractSummary {
    base::u64 contract_gate_count = 0;
    base::u64 contract_call_binding_count = 0;
    base::u64 contract_user_derive_count = 0;
    base::u64 contract_visible_count = 0;
    base::u64 contract_query_reusable_count = 0;
    base::u64 contract_compiler_owned_count = 0;
    base::u64 contract_source_map_available_count = 0;
    base::u64 contract_hygiene_mark_available_count = 0;
    base::u64 contract_diagnostic_projection_available_count = 0;
    base::u64 contract_declared_name_policy_available_count = 0;
    base::u64 contract_planned_token_count = 0;
    base::u64 declared_name_policy_gate_count = 0;
    base::u64 declared_name_policy_visible_count = 0;
    base::u64 declared_name_policy_query_reusable_count = 0;
    base::u64 declared_name_set_reserved_count = 0;
    base::u64 lookup_visible_declared_name_count = 0;
    base::u64 export_visible_declared_name_count = 0;
    base::u64 sema_visible_declared_name_count = 0;
    base::u64 diagnostic_projection_gate_count = 0;
    base::u64 diagnostic_projection_visible_count = 0;
    base::u64 diagnostic_projection_query_reusable_count = 0;
    base::u64 diagnostic_projection_debuggable_count = 0;
    base::u64 diagnostic_emission_enabled_count = 0;
    AurexMacroOutputContractBlockedEffectSummary blocked_effects;

    [[nodiscard]] friend bool operator==(const AurexMacroOutputContractSummary& lhs,
        const AurexMacroOutputContractSummary& rhs) noexcept = default;
};

} // namespace aurex::frontend::macro
