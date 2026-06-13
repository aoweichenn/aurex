#pragma once

#include <aurex/infrastructure/query/query_key.hpp>
#include <aurex/infrastructure/query/stable_hash.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace aurex::query {

enum class MacroExpansionFactKind : base::u8 {
    attribute_token_tree_input = 1,
    builtin_derive_passthrough,
    early_item_expansion_query_key,
    generated_module_part_noop,
    expansion_source_map_stub,
    unimplemented_item_attribute_blocker,
    external_procedural_macro_blocked,
    aurex_declarative_macro_surface,
    aurex_user_derive_macro_surface,
    aurex_compile_time_macro_execution_admission,
    aurex_macro_typed_matcher_admission,
    aurex_macro_definition_site_hygiene_admission,
    aurex_macro_debuggable_diagnostic_anchor,
    aurex_macro_call_site_admission,
    aurex_macro_matcher_to_call_binding_admission,
    aurex_user_derive_target_schema_admission,
    aurex_macro_output_contract_admission,
    aurex_macro_output_declared_name_policy_admission,
    aurex_macro_output_diagnostic_projection_admission,
};

enum class MacroExpansionStage : base::u8 {
    parsed_attribute_surface = 1,
    early_item_expansion,
    generated_part_planning,
    sema_blocker,
    future_stage,
};

enum class MacroExpansionPolicy : base::u8 {
    attribute_token_tree_v1 = 1,
    builtin_derive_passthrough_v1,
    expansion_query_fingerprint_v1,
    generated_module_part_noop_v1,
    source_map_trace_stub_v1,
    unimplemented_item_attribute_blocker_v1,
    external_proc_macro_sandbox_future_v1,
    aurex_declarative_macro_surface_v1,
    aurex_user_derive_macro_surface_v1,
    aurex_compile_time_macro_execution_admission_v1,
    aurex_macro_typed_matcher_admission_v1,
    aurex_macro_definition_site_hygiene_admission_v1,
    aurex_macro_debuggable_diagnostic_anchor_v1,
    aurex_macro_call_site_admission_v1,
    aurex_macro_matcher_to_call_binding_admission_v1,
    aurex_user_derive_target_schema_admission_v1,
    aurex_macro_output_contract_admission_v1,
    aurex_macro_output_declared_name_policy_admission_v1,
    aurex_macro_output_diagnostic_projection_admission_v1,
};

struct MacroExpansionFact {
    std::string fact_name;
    MacroExpansionFactKind kind = MacroExpansionFactKind::attribute_token_tree_input;
    MacroExpansionStage stage = MacroExpansionStage::parsed_attribute_surface;
    MacroExpansionPolicy policy = MacroExpansionPolicy::attribute_token_tree_v1;
    bool consumes_attribute_decl = false;
    bool consumes_attribute_token_tree = false;
    bool preserves_builtin_derive = false;
    bool requires_query_key = false;
    bool requires_generated_module_part = false;
    bool uses_generated_source_role = false;
    bool uses_generated_module_part_kind = false;
    bool requires_source_map = false;
    bool requires_hygiene = false;
    bool produces_user_generated_code = false;
    bool standard_library_required = false;
    bool runtime_required = false;
    bool external_process_required = false;
    bool blocks_unimplemented_item_attribute = false;
    SourceRole generated_source_role = SourceRole::generated;
    ModulePartKind generated_part_kind = ModulePartKind::generated;
    std::string input_fact;
    std::string output_fact;
    std::string blocker_fact;
};

struct MacroExpansionSummary {
    base::u64 fact_count = 0;
    base::u64 attribute_input_count = 0;
    base::u64 builtin_derive_passthrough_count = 0;
    base::u64 query_key_count = 0;
    base::u64 generated_part_count = 0;
    base::u64 source_map_stub_count = 0;
    base::u64 sema_blocker_count = 0;
    base::u64 future_external_count = 0;
    base::u64 aurex_declarative_macro_surface_count = 0;
    base::u64 aurex_user_derive_macro_surface_count = 0;
    base::u64 aurex_compile_time_macro_execution_admission_count = 0;
    base::u64 aurex_macro_typed_matcher_admission_count = 0;
    base::u64 aurex_macro_definition_site_hygiene_admission_count = 0;
    base::u64 aurex_macro_debuggable_diagnostic_anchor_count = 0;
    base::u64 aurex_macro_call_site_admission_count = 0;
    base::u64 aurex_macro_matcher_to_call_binding_admission_count = 0;
    base::u64 aurex_user_derive_target_schema_admission_count = 0;
    base::u64 aurex_macro_output_contract_admission_count = 0;
    base::u64 aurex_macro_output_declared_name_policy_admission_count = 0;
    base::u64 aurex_macro_output_diagnostic_projection_admission_count = 0;
    base::u64 attribute_decl_input_count = 0;
    base::u64 token_tree_input_count = 0;
    base::u64 generated_source_role_count = 0;
    base::u64 generated_module_part_kind_count = 0;
    base::u64 user_generated_code_count = 0;
    base::u64 standard_library_required_count = 0;
    base::u64 runtime_required_count = 0;
    base::u64 external_process_required_count = 0;
    base::u64 unimplemented_item_attribute_blocker_count = 0;
};

struct MacroExpansionPlan {
    std::string name;
    std::vector<MacroExpansionFact> facts;
    MacroExpansionSummary summary;
    StableFingerprint128 fingerprint;
};

[[nodiscard]] std::string_view macro_expansion_fact_kind_name(MacroExpansionFactKind kind) noexcept;
[[nodiscard]] std::string_view macro_expansion_stage_name(MacroExpansionStage stage) noexcept;
[[nodiscard]] std::string_view macro_expansion_policy_name(MacroExpansionPolicy policy) noexcept;

[[nodiscard]] bool is_valid(MacroExpansionFactKind kind) noexcept;
[[nodiscard]] bool is_valid(MacroExpansionStage stage) noexcept;
[[nodiscard]] bool is_valid(MacroExpansionPolicy policy) noexcept;
[[nodiscard]] bool is_valid(const MacroExpansionFact& fact) noexcept;
[[nodiscard]] bool is_valid(const MacroExpansionSummary& summary, const MacroExpansionPlan& plan) noexcept;
[[nodiscard]] bool is_valid(const MacroExpansionPlan& plan) noexcept;
[[nodiscard]] bool is_valid_m21c_macro_expansion_plan(const MacroExpansionPlan& plan) noexcept;
[[nodiscard]] bool is_valid_m27_macro_expansion_plan(const MacroExpansionPlan& plan) noexcept;
[[nodiscard]] bool is_valid_m27b_macro_expansion_plan(const MacroExpansionPlan& plan) noexcept;
[[nodiscard]] bool is_valid_m27c_macro_expansion_plan(const MacroExpansionPlan& plan) noexcept;
[[nodiscard]] bool is_valid_m27d_macro_expansion_plan(const MacroExpansionPlan& plan) noexcept;

void record_macro_expansion_fact(MacroExpansionPlan& plan, MacroExpansionFact fact);

[[nodiscard]] MacroExpansionSummary summarize_macro_expansion_plan_counts(
    const MacroExpansionPlan& plan) noexcept;
[[nodiscard]] StableFingerprint128 macro_expansion_plan_fingerprint(
    const MacroExpansionPlan& plan) noexcept;
[[nodiscard]] std::string summarize_macro_expansion_plan(const MacroExpansionPlan& plan);
[[nodiscard]] std::string dump_macro_expansion_plan(const MacroExpansionPlan& plan);
[[nodiscard]] MacroExpansionPlan m21c_macro_expansion_plan_baseline();
[[nodiscard]] MacroExpansionPlan m27_macro_expansion_plan_baseline();
[[nodiscard]] MacroExpansionPlan m27b_macro_expansion_plan_baseline();
[[nodiscard]] MacroExpansionPlan m27c_macro_expansion_plan_baseline();
[[nodiscard]] MacroExpansionPlan m27d_macro_expansion_plan_baseline();

[[nodiscard]] std::string_view m21c_item_attribute_macro_unimplemented_prefix() noexcept;
[[nodiscard]] std::string m21c_item_attribute_macro_unimplemented_message(
    std::string_view attribute_name);

} // namespace aurex::query
