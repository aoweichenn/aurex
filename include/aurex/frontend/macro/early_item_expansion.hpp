#pragma once

#include <aurex/frontend/syntax/core/ast.hpp>
#include <aurex/infrastructure/base/result.hpp>
#include <aurex/infrastructure/query/macro_expansion_facts.hpp>
#include <aurex/infrastructure/query/query_key.hpp>
#include <aurex/infrastructure/query/stable_hash.hpp>

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace aurex::frontend::macro {

enum class EarlyItemExpansionDisposition : base::u8 {
    builtin_derive_passthrough = 1,
    blocked_unimplemented_attribute,
};

enum class GeneratedModulePartLifecycleState : base::u8 {
    planned = 1,
    materialized_buffer_stub,
    parse_blocked,
    merge_blocked,
};

struct EarlyItemMacroInput {
    syntax::ItemId item;
    syntax::ModuleId module;
    base::u32 part_index = 0;
    base::u32 attribute_index = 0;
    std::string attribute_name;
    base::SourceRange attribute_range{};
    base::SourceRange token_tree_range{};
    bool has_token_tree = false;
    base::u64 token_count = 0;
    query::ModulePartKey attached_part;
    query::StableFingerprint128 token_tree_fingerprint;
    query::StableFingerprint128 query_key_fingerprint;
    EarlyItemExpansionDisposition disposition =
        EarlyItemExpansionDisposition::blocked_unimplemented_attribute;
};

struct GeneratedModulePartPlaceholder {
    syntax::ModuleId module;
    base::u32 source_part_index = 0;
    base::u32 generated_stable_index = 0;
    query::SourceRole source_role = query::SourceRole::generated;
    query::ModulePartKind part_kind = query::ModulePartKind::generated;
    query::ModulePartKey source_part;
    query::ModulePartKey generated_part;
    query::StableFingerprint128 output_fingerprint;
    bool parsed = false;
    bool merged = false;
    bool produced_user_generated_code = false;
};

struct GeneratedModulePartParseMergeStub {
    syntax::ModuleId module;
    base::u32 source_part_index = 0;
    base::u32 generated_stable_index = 0;
    query::ModulePartKey source_part;
    query::ModulePartKey generated_part;
    query::StableFingerprint128 generated_buffer_identity;
    query::StableFingerprint128 parse_config_fingerprint;
    query::StableFingerprint128 merge_ordering_key;
    query::StableFingerprint128 expansion_origin;
    std::string generated_buffer_name;
    std::string blocker_reason;
    GeneratedModulePartLifecycleState lifecycle_state =
        GeneratedModulePartLifecycleState::merge_blocked;
    bool materialized_buffer = true;
    bool parsed = false;
    bool merged = false;
    bool sema_visible = false;
    bool produced_user_generated_code = false;
};

struct ExpansionSourceMapPlaceholder {
    syntax::ItemId item;
    syntax::ModuleId module;
    base::u32 attribute_index = 0;
    base::SourceRange attribute_range{};
    base::SourceRange token_tree_range{};
    query::StableFingerprint128 expansion_origin;
    bool real_source_map = false;
    bool debug_trace_available = false;
};

struct ExpansionHygieneStub {
    syntax::ItemId item;
    syntax::ModuleId module;
    base::u32 part_index = 0;
    base::u32 attribute_index = 0;
    query::ModulePartKey attached_part;
    query::StableFingerprint128 expansion_origin;
    query::StableFingerprint128 call_site_mark;
    query::StableFingerprint128 definition_site_mark;
    query::StableFingerprint128 generated_fresh_mark;
    query::StableFingerprint128 declared_name_set;
    std::string policy;
    bool resolved = false;
    bool declared_names_visible = false;
    bool captures_call_site_locals = false;
};

struct ExpansionTraceStub {
    syntax::ItemId item;
    syntax::ModuleId module;
    base::u32 part_index = 0;
    base::u32 attribute_index = 0;
    query::ModulePartKey attached_part;
    base::SourceRange attribute_range{};
    base::SourceRange token_tree_range{};
    query::StableFingerprint128 expansion_origin;
    query::StableFingerprint128 trace_identity;
    query::StableFingerprint128 generated_source_map_identity;
    query::StableFingerprint128 diagnostic_anchor;
    std::string trace_policy;
    std::string blocker_reason;
    bool real_source_map = false;
    bool debug_trace_available = false;
    bool cli_emit_expanded_available = false;
};

struct GeneratedItemDeclarationStub {
    syntax::ItemId item;
    syntax::ModuleId module;
    base::u32 part_index = 0;
    base::u32 attribute_index = 0;
    query::ModulePartKey attached_part;
    query::ModulePartKey generated_part;
    query::StableFingerprint128 expansion_origin;
    query::StableFingerprint128 declaration_identity;
    query::StableFingerprint128 declared_name_set;
    query::StableFingerprint128 generated_item_key;
    std::string declaration_role;
    std::string generated_item_name;
    std::string blocker_reason;
    bool planned = true;
    bool materialized_tokens = false;
    bool parsed = false;
    bool merged = false;
    bool sema_visible = false;
    bool produced_user_generated_code = false;
};

struct DeclaredGeneratedNameStub {
    syntax::ItemId item;
    syntax::ModuleId module;
    base::u32 part_index = 0;
    base::u32 attribute_index = 0;
    query::ModulePartKey attached_part;
    query::ModulePartKey generated_part;
    query::StableFingerprint128 expansion_origin;
    query::StableFingerprint128 declared_name_set;
    query::StableFingerprint128 declared_name_identity;
    query::StableFingerprint128 hygiene_mark;
    std::string declared_name;
    std::string namespace_kind;
    std::string blocker_reason;
    bool lookup_visible = false;
    bool export_visible = false;
    bool sema_visible = false;
    bool produced_user_generated_code = false;
};

struct TokenMaterializationAdmissionStub {
    syntax::ItemId item;
    syntax::ModuleId module;
    base::u32 part_index = 0;
    base::u32 attribute_index = 0;
    query::ModulePartKey attached_part;
    query::ModulePartKey generated_part;
    query::StableFingerprint128 expansion_origin;
    query::StableFingerprint128 declaration_identity;
    query::StableFingerprint128 generated_item_key;
    query::StableFingerprint128 declared_name_set;
    query::StableFingerprint128 declared_name_identity;
    query::StableFingerprint128 hygiene_mark;
    query::StableFingerprint128 source_map_identity;
    query::StableFingerprint128 trace_identity;
    query::StableFingerprint128 token_plan_identity;
    query::StableFingerprint128 token_buffer_identity;
    std::string admission_policy;
    std::string token_stream_name;
    std::string blocker_reason;
    bool compiler_owned = true;
    bool admitted = true;
    bool materialized_tokens = false;
    bool generated_source_text = false;
    bool parse_ready = false;
    bool external_process_required = false;
    bool standard_library_required = false;
    bool runtime_required = false;
    bool produced_user_generated_code = false;
};

struct GeneratedTokenBufferStub {
    syntax::ItemId item;
    syntax::ModuleId module;
    base::u32 part_index = 0;
    base::u32 attribute_index = 0;
    query::ModulePartKey attached_part;
    query::ModulePartKey generated_part;
    query::StableFingerprint128 token_plan_identity;
    query::StableFingerprint128 token_buffer_identity;
    query::StableFingerprint128 materialization_identity;
    query::StableFingerprint128 source_map_identity;
    query::StableFingerprint128 hygiene_mark;
    std::string token_stream_name;
    std::string token_buffer_kind;
    std::string token_producer_policy;
    std::string blocker_reason;
    base::u64 token_count = 0;
    bool empty = true;
    bool materialized_tokens = false;
    bool generated_source_text = false;
    bool parser_consumable = false;
    bool produced_user_generated_code = false;
};

struct GeneratedTokenRecord {
    syntax::ItemId item;
    syntax::ModuleId module;
    base::u32 part_index = 0;
    base::u32 attribute_index = 0;
    base::u32 token_index = 0;
    query::StableFingerprint128 token_buffer_identity;
    query::StableFingerprint128 token_identity;
    query::StableFingerprint128 source_map_identity;
    query::StableFingerprint128 hygiene_mark;
    syntax::TokenKind kind = syntax::TokenKind::invalid;
    std::string text;
    std::string token_role;
    base::SourceRange anchor_range{};
    bool compiler_owned = true;
    bool parser_visible = false;
    bool produced_user_generated_code = false;
};

struct GeneratedTokenParserAdmissionGateStub {
    syntax::ItemId item;
    syntax::ModuleId module;
    base::u32 part_index = 0;
    base::u32 attribute_index = 0;
    query::ModulePartKey attached_part;
    query::ModulePartKey generated_part;
    query::StableFingerprint128 token_plan_identity;
    query::StableFingerprint128 token_buffer_identity;
    query::StableFingerprint128 materialization_identity;
    query::StableFingerprint128 source_map_identity;
    query::StableFingerprint128 hygiene_mark;
    query::StableFingerprint128 generated_buffer_identity;
    query::StableFingerprint128 parse_config_fingerprint;
    query::StableFingerprint128 parse_gate_identity;
    std::string token_stream_name;
    std::string parser_gate_policy;
    std::string blocker_reason;
    base::u64 token_count = 0;
    bool compiler_owned = true;
    bool token_buffer_materialized = false;
    bool token_records_available = false;
    bool parser_admitted = false;
    bool parse_ready = false;
    bool parser_consumable = false;
    bool generated_source_text = false;
    bool generated_part_parsed = false;
    bool generated_part_merged = false;
    bool sema_visible = false;
    bool produced_user_generated_code = false;
};

struct ParserAdmissionDiagnosticProjectionStub {
    syntax::ItemId item;
    syntax::ModuleId module;
    base::u32 part_index = 0;
    base::u32 attribute_index = 0;
    query::ModulePartKey attached_part;
    query::ModulePartKey generated_part;
    base::SourceRange primary_anchor{};
    base::SourceRange token_tree_anchor{};
    query::StableFingerprint128 parse_gate_identity;
    query::StableFingerprint128 diagnostic_identity;
    query::StableFingerprint128 diagnostic_anchor_identity;
    query::StableFingerprint128 token_plan_identity;
    query::StableFingerprint128 token_buffer_identity;
    query::StableFingerprint128 materialization_identity;
    query::StableFingerprint128 generated_buffer_identity;
    query::StableFingerprint128 parse_config_fingerprint;
    query::StableFingerprint128 source_map_identity;
    query::StableFingerprint128 hygiene_mark;
    query::StableFingerprint128 trace_identity;
    std::string diagnostic_policy;
    std::string blocker_category;
    std::string token_buffer_blocker;
    std::string generated_part_parse_blocker;
    std::string user_message;
    std::string debug_projection_name;
    base::u64 token_count = 0;
    bool token_buffer_materialized = false;
    bool token_records_available = false;
    bool parser_admitted = false;
    bool parse_ready = false;
    bool parser_consumable = false;
    bool generated_part_parsed = false;
    bool generated_part_merged = false;
    bool emit_expanded_available = false;
    bool debug_trace_available = false;
    bool source_map_available = false;
    bool produced_user_generated_code = false;
};

struct ParserAdmissionDiagnosticReportEntry {
    syntax::ItemId item;
    syntax::ModuleId module;
    base::u32 part_index = 0;
    base::u32 attribute_index = 0;
    base::u32 report_index = 0;
    query::ModulePartKey attached_part;
    query::ModulePartKey generated_part;
    base::SourceRange primary_anchor{};
    base::SourceRange token_tree_anchor{};
    query::StableFingerprint128 diagnostic_identity;
    query::StableFingerprint128 diagnostic_anchor_identity;
    query::StableFingerprint128 report_entry_identity;
    query::StableFingerprint128 parse_gate_identity;
    std::string blocker_category;
    std::string debug_projection_name;
    std::string query_projection_name;
    base::u64 token_count = 0;
    bool token_records_available = false;
    bool parser_admitted = false;
    bool report_visible = true;
    bool query_reusable = true;
    bool parser_consumable = false;
    bool emit_expanded_available = false;
    bool produced_user_generated_code = false;
};

struct ParserAdmissionDiagnosticReport {
    syntax::ModuleId module;
    base::u32 source_part_index = 0;
    query::ModulePartKey attached_part;
    query::ModulePartKey generated_part;
    query::StableFingerprint128 report_identity;
    query::StableFingerprint128 report_anchor_identity;
    query::StableFingerprint128 report_grouping_identity;
    query::StableFingerprint128 parse_config_fingerprint;
    query::StableFingerprint128 generated_buffer_identity;
    std::string report_policy;
    std::string report_query_name;
    std::string blocked_reason;
    base::u64 entry_count = 0;
    base::u64 blocked_entry_count = 0;
    base::u64 derive_entry_count = 0;
    base::u64 empty_entry_count = 0;
    base::u64 token_record_available_entry_count = 0;
    bool query_reusable = true;
    bool report_visible = true;
    bool source_anchor_ordered = true;
    bool parser_admitted = false;
    bool parse_ready = false;
    bool parser_consumable = false;
    bool emit_expanded_available = false;
    bool debug_trace_available = false;
    bool source_map_available = false;
    bool produced_user_generated_code = false;
};

struct GeneratedTokenParserReadinessPreflightEntry {
    syntax::ItemId item;
    syntax::ModuleId module;
    base::u32 part_index = 0;
    base::u32 attribute_index = 0;
    base::u32 preflight_index = 0;
    query::ModulePartKey attached_part;
    query::ModulePartKey generated_part;
    query::StableFingerprint128 token_plan_identity;
    query::StableFingerprint128 token_buffer_identity;
    query::StableFingerprint128 materialization_identity;
    query::StableFingerprint128 generated_buffer_identity;
    query::StableFingerprint128 parse_config_fingerprint;
    query::StableFingerprint128 parse_gate_identity;
    query::StableFingerprint128 diagnostic_identity;
    query::StableFingerprint128 diagnostic_anchor_identity;
    query::StableFingerprint128 report_entry_identity;
    query::StableFingerprint128 source_map_identity;
    query::StableFingerprint128 hygiene_mark;
    query::StableFingerprint128 trace_identity;
    query::StableFingerprint128 preflight_identity;
    std::string token_stream_name;
    std::string token_stream_shape;
    std::string delimiter_balance_state;
    std::string source_anchor_coverage_state;
    std::string readiness_policy;
    std::string blocker_reason;
    base::u64 token_count = 0;
    bool token_records_available = false;
    bool token_indices_contiguous = true;
    bool delimiter_balanced = true;
    bool source_anchors_covered = true;
    bool parse_config_compatible = true;
    bool hygiene_prerequisite_available = true;
    bool source_map_prerequisite_available = true;
    bool diagnostic_projection_available = true;
    bool parser_admitted = false;
    bool parse_ready = false;
    bool parser_consumable = false;
    bool generated_part_parsed = false;
    bool generated_part_merged = false;
    bool produced_user_generated_code = false;
};

struct GeneratedTokenParserConsumptionContractGate {
    syntax::ModuleId module;
    base::u32 source_part_index = 0;
    query::ModulePartKey attached_part;
    query::ModulePartKey generated_part;
    query::StableFingerprint128 generated_buffer_identity;
    query::StableFingerprint128 parse_config_fingerprint;
    query::StableFingerprint128 report_identity;
    query::StableFingerprint128 contract_identity;
    query::StableFingerprint128 contract_grouping_identity;
    query::StableFingerprint128 contract_anchor_identity;
    std::string contract_policy;
    std::string contract_query_name;
    std::string blocked_reason;
    base::u64 preflight_entry_count = 0;
    base::u64 blocked_entry_count = 0;
    base::u64 derive_entry_count = 0;
    base::u64 empty_entry_count = 0;
    base::u64 contiguous_index_entry_count = 0;
    base::u64 delimiter_balanced_entry_count = 0;
    base::u64 source_anchor_covered_entry_count = 0;
    base::u64 parse_config_compatible_entry_count = 0;
    base::u64 diagnostic_projection_entry_count = 0;
    bool query_reusable = true;
    bool contract_visible = true;
    bool all_entries_structurally_checked = true;
    bool parser_admitted = false;
    bool parse_ready = false;
    bool parser_consumable = false;
    bool generated_part_parsed = false;
    bool generated_part_merged = false;
    bool sema_visible = false;
    bool emit_expanded_available = false;
    bool debug_trace_available = false;
    bool source_map_available = false;
    bool produced_user_generated_code = false;
};

struct MacroExpansionBoundaryClosureReport {
    query::StableFingerprint128 closure_identity;
    query::StableFingerprint128 closure_grouping_identity;
    std::string closure_policy;
    std::string closure_query_name;
    std::string blocked_reason;
    base::u64 macro_input_count = 0;
    base::u64 generated_part_count = 0;
    base::u64 parser_admission_report_count = 0;
    base::u64 parser_readiness_preflight_entry_count = 0;
    base::u64 parser_consumption_contract_gate_count = 0;
    base::u64 blocked_contract_gate_count = 0;
    base::u64 parser_consumable_contract_gate_count = 0;
    bool m21m_preflight_available = true;
    bool m21n_contract_available = true;
    bool release_closure_complete = true;
    bool query_reusable = true;
    bool closure_visible = true;
    bool parser_consumption_enabled = false;
    bool emit_expanded_available = false;
    bool debug_trace_available = false;
    bool source_map_available = false;
    bool standard_library_required = false;
    bool runtime_required = false;
    bool external_process_required = false;
    bool produced_user_generated_code = false;
};

struct BuiltinDeriveExpansionAdmissionGate {
    syntax::ItemId item;
    syntax::ModuleId module;
    base::u32 part_index = 0;
    base::u32 attribute_index = 0;
    base::u32 admission_index = 0;
    query::ModulePartKey attached_part;
    query::ModulePartKey generated_part;
    query::StableFingerprint128 token_buffer_identity;
    query::StableFingerprint128 preflight_identity;
    query::StableFingerprint128 parse_gate_identity;
    query::StableFingerprint128 diagnostic_identity;
    query::StableFingerprint128 closure_identity;
    query::StableFingerprint128 admission_identity;
    std::string admission_policy;
    std::string admission_kind;
    std::string query_name;
    std::string blocker_reason;
    base::u64 token_count = 0;
    base::u64 capability_candidate_count = 0;
    base::u64 unsupported_candidate_count = 0;
    base::u64 duplicate_candidate_count = 0;
    bool builtin_derive_input = false;
    bool compiler_owned = true;
    bool token_records_available = false;
    bool preflight_available = false;
    bool admission_visible = true;
    bool query_reusable = true;
    bool parser_consumption_enabled = false;
    bool external_process_required = false;
    bool standard_library_required = false;
    bool runtime_required = false;
    bool generated_source_text = false;
    bool produced_user_generated_code = false;
};

struct BuiltinDeriveSemanticExpansionPlan {
    syntax::ItemId item;
    syntax::ModuleId module;
    base::u32 part_index = 0;
    base::u32 attribute_index = 0;
    base::u32 semantic_plan_index = 0;
    query::ModulePartKey attached_part;
    query::ModulePartKey generated_part;
    query::StableFingerprint128 token_buffer_identity;
    query::StableFingerprint128 preflight_identity;
    query::StableFingerprint128 admission_identity;
    query::StableFingerprint128 semantic_plan_identity;
    query::StableFingerprint128 capability_set_identity;
    std::string semantic_policy;
    std::string target_kind;
    std::string semantic_model;
    std::string blocker_reason;
    base::u64 capability_count = 0;
    base::u64 copy_capability_count = 0;
    base::u64 eq_capability_count = 0;
    base::u64 hash_capability_count = 0;
    bool builtin_derive_input = false;
    bool target_struct_or_enum = false;
    bool uses_existing_builtin_derive_capability_path = false;
    bool requires_ast_mutation = false;
    bool requires_generated_items = false;
    bool requires_standard_library = false;
    bool requires_runtime = false;
    bool external_process_required = false;
    bool parser_consumption_enabled = false;
    bool produced_user_generated_code = false;
    bool plan_visible = true;
    bool query_reusable = true;
};

struct BuiltinDeriveParserConsumptionReleaseGate {
    syntax::ModuleId module;
    base::u32 source_part_index = 0;
    query::ModulePartKey attached_part;
    query::ModulePartKey generated_part;
    query::StableFingerprint128 contract_identity;
    query::StableFingerprint128 closure_identity;
    query::StableFingerprint128 admission_group_identity;
    query::StableFingerprint128 semantic_plan_group_identity;
    query::StableFingerprint128 release_gate_identity;
    std::string release_policy;
    std::string release_query_name;
    std::string blocked_reason;
    base::u64 admission_count = 0;
    base::u64 derive_admission_count = 0;
    base::u64 semantic_plan_count = 0;
    base::u64 capability_total_count = 0;
    base::u64 parser_consumable_contract_count = 0;
    bool rollback_diagnostics_available = true;
    bool debug_trace_prerequisite_available = true;
    bool source_map_prerequisite_available = true;
    bool hygiene_prerequisite_available = true;
    bool parser_consumption_enabled = false;
    bool generated_part_parsed = false;
    bool generated_part_merged = false;
    bool emit_expanded_available = false;
    bool debug_trace_available = false;
    bool source_map_available = false;
    bool standard_library_required = false;
    bool runtime_required = false;
    bool external_process_required = false;
    bool produced_user_generated_code = false;
    bool release_visible = true;
    bool query_reusable = true;
};

struct BuiltinDeriveReleaseHardeningMatrix {
    syntax::ModuleId module;
    base::u32 source_part_index = 0;
    query::ModulePartKey attached_part;
    query::ModulePartKey generated_part;
    query::StableFingerprint128 release_gate_identity;
    query::StableFingerprint128 admission_group_identity;
    query::StableFingerprint128 semantic_plan_group_identity;
    query::StableFingerprint128 hardening_matrix_identity;
    std::string hardening_policy;
    std::string hardening_query_name;
    std::string blocked_reason;
    base::u64 part_local_admission_count = 0;
    base::u64 part_local_derive_admission_count = 0;
    base::u64 part_local_semantic_plan_count = 0;
    base::u64 part_local_release_gate_count = 0;
    base::u64 global_admission_count = 0;
    base::u64 global_semantic_plan_count = 0;
    base::u64 global_generated_part_count = 0;
    base::u64 cross_part_admission_count = 0;
    base::u64 cross_part_semantic_plan_count = 0;
    bool part_locality_preserved = true;
    bool multi_item_matrix_available = true;
    bool negative_matrix_complete = true;
    bool release_remains_blocked = true;
    bool parser_consumption_enabled = false;
    bool generated_part_parsed = false;
    bool generated_part_merged = false;
    bool emit_expanded_available = false;
    bool debug_trace_available = false;
    bool source_map_available = false;
    bool standard_library_required = false;
    bool runtime_required = false;
    bool external_process_required = false;
    bool produced_user_generated_code = false;
    bool matrix_visible = true;
    bool query_reusable = true;
};

struct BuiltinDeriveDebugDumpStabilityContract {
    syntax::ModuleId module;
    base::u32 source_part_index = 0;
    query::ModulePartKey attached_part;
    query::ModulePartKey generated_part;
    query::StableFingerprint128 release_gate_identity;
    query::StableFingerprint128 hardening_matrix_identity;
    query::StableFingerprint128 debug_dump_contract_identity;
    std::string debug_dump_policy;
    std::string debug_dump_query_name;
    std::string blocked_reason;
    base::u64 dump_section_count = 0;
    bool stable_ordering_available = true;
    bool identity_projection_available = true;
    bool summary_projection_available = true;
    bool drift_debuggable = true;
    bool debug_dump_contract_complete = true;
    bool emit_expanded_available = false;
    bool debug_trace_available = false;
    bool source_map_available = false;
    bool parser_consumption_enabled = false;
    bool standard_library_required = false;
    bool runtime_required = false;
    bool external_process_required = false;
    bool produced_user_generated_code = false;
    bool contract_visible = true;
    bool query_reusable = true;
};

struct BuiltinDeriveRollbackDiagnosticDesignGate {
    syntax::ModuleId module;
    base::u32 source_part_index = 0;
    query::ModulePartKey attached_part;
    query::ModulePartKey generated_part;
    query::StableFingerprint128 parser_consumption_contract_identity;
    query::StableFingerprint128 release_gate_identity;
    query::StableFingerprint128 hardening_matrix_identity;
    query::StableFingerprint128 debug_dump_contract_identity;
    query::StableFingerprint128 rollback_gate_identity;
    std::string rollback_policy;
    std::string rollback_query_name;
    std::string blocked_reason;
    base::u64 diagnostic_projection_count = 0;
    base::u64 diagnostic_report_entry_count = 0;
    base::u64 blocked_diagnostic_count = 0;
    base::u64 derive_diagnostic_count = 0;
    base::u64 empty_diagnostic_count = 0;
    base::u64 parser_consumption_contract_count = 0;
    bool rollback_diagnostic_design_available = true;
    bool diagnostic_grouping_available = true;
    bool source_anchor_available = true;
    bool token_tree_anchor_available = true;
    bool debug_dump_contract_available = true;
    bool release_rollback_plan_complete = true;
    bool rollback_execution_enabled = false;
    bool parser_consumption_enabled = false;
    bool generated_part_parsed = false;
    bool generated_part_merged = false;
    bool emit_expanded_available = false;
    bool debug_trace_available = false;
    bool source_map_available = false;
    bool standard_library_required = false;
    bool runtime_required = false;
    bool external_process_required = false;
    bool produced_user_generated_code = false;
    bool rollback_gate_visible = true;
    bool query_reusable = true;
};

struct EarlyItemExpansionSummary {
    base::u64 macro_input_count = 0;
    base::u64 attribute_input_count = 0;
    base::u64 builtin_derive_passthrough_count = 0;
    base::u64 blocked_attribute_count = 0;
    base::u64 generated_part_placeholder_count = 0;
    base::u64 generated_part_stub_count = 0;
    base::u64 materialized_buffer_stub_count = 0;
    base::u64 parse_blocked_count = 0;
    base::u64 merge_blocked_count = 0;
    base::u64 sema_visible_generated_part_count = 0;
    base::u64 source_map_placeholder_count = 0;
    base::u64 hygiene_stub_count = 0;
    base::u64 unresolved_hygiene_stub_count = 0;
    base::u64 declared_name_stub_count = 0;
    base::u64 call_site_capture_count = 0;
    base::u64 trace_stub_count = 0;
    base::u64 real_source_map_count = 0;
    base::u64 debug_trace_available_count = 0;
    base::u64 cli_emit_expanded_available_count = 0;
    base::u64 generated_item_declaration_stub_count = 0;
    base::u64 planned_generated_item_declaration_count = 0;
    base::u64 materialized_generated_item_count = 0;
    base::u64 declared_generated_name_stub_count = 0;
    base::u64 lookup_visible_declared_name_count = 0;
    base::u64 export_visible_declared_name_count = 0;
    base::u64 token_materialization_admission_stub_count = 0;
    base::u64 compiler_owned_admission_count = 0;
    base::u64 admitted_token_materialization_count = 0;
    base::u64 materialized_token_admission_count = 0;
    base::u64 generated_token_buffer_stub_count = 0;
    base::u64 empty_generated_token_buffer_count = 0;
    base::u64 materialized_token_buffer_count = 0;
    base::u64 compiler_owned_token_buffer_count = 0;
    base::u64 generated_token_record_count = 0;
    base::u64 compiler_owned_generated_token_record_count = 0;
    base::u64 parser_visible_generated_token_count = 0;
    base::u64 parser_admission_gate_stub_count = 0;
    base::u64 compiler_owned_parser_admission_gate_count = 0;
    base::u64 token_record_available_gate_count = 0;
    base::u64 parser_blocked_token_buffer_count = 0;
    base::u64 parser_admitted_token_buffer_count = 0;
    base::u64 parser_admission_diagnostic_stub_count = 0;
    base::u64 parser_admission_diagnostic_blocked_count = 0;
    base::u64 derive_parser_admission_diagnostic_count = 0;
    base::u64 empty_parser_admission_diagnostic_count = 0;
    base::u64 emit_expanded_projection_available_count = 0;
    base::u64 parser_admission_debug_trace_projection_count = 0;
    base::u64 parser_admission_source_map_projection_count = 0;
    base::u64 parser_admission_report_entry_count = 0;
    base::u64 parser_admission_report_count = 0;
    base::u64 parser_admission_report_blocked_entry_count = 0;
    base::u64 parser_admission_report_derive_entry_count = 0;
    base::u64 parser_admission_report_empty_entry_count = 0;
    base::u64 parser_admission_report_token_record_available_entry_count = 0;
    base::u64 parser_admission_report_visible_count = 0;
    base::u64 parser_admission_report_query_reusable_count = 0;
    base::u64 parser_admission_report_unordered_anchor_count = 0;
    base::u64 parser_admission_report_parser_consumable_count = 0;
    base::u64 parser_readiness_preflight_entry_count = 0;
    base::u64 parser_readiness_preflight_blocked_count = 0;
    base::u64 parser_readiness_preflight_derive_entry_count = 0;
    base::u64 parser_readiness_preflight_empty_entry_count = 0;
    base::u64 parser_readiness_preflight_contiguous_index_count = 0;
    base::u64 parser_readiness_preflight_delimiter_balanced_count = 0;
    base::u64 parser_readiness_preflight_source_anchor_covered_count = 0;
    base::u64 parser_readiness_preflight_parse_config_compatible_count = 0;
    base::u64 parser_readiness_preflight_parser_consumable_count = 0;
    base::u64 parser_consumption_contract_gate_count = 0;
    base::u64 parser_consumption_contract_blocked_gate_count = 0;
    base::u64 parser_consumption_contract_visible_count = 0;
    base::u64 parser_consumption_contract_query_reusable_count = 0;
    base::u64 parser_consumption_contract_parser_consumable_count = 0;
    base::u64 macro_boundary_closure_report_count = 0;
    base::u64 macro_boundary_closure_visible_count = 0;
    base::u64 macro_boundary_closure_query_reusable_count = 0;
    base::u64 macro_boundary_closure_complete_count = 0;
    base::u64 macro_boundary_closure_parser_consumption_enabled_count = 0;
    base::u64 builtin_derive_expansion_admission_gate_count = 0;
    base::u64 builtin_derive_expansion_derive_admission_count = 0;
    base::u64 builtin_derive_expansion_non_derive_blocked_count = 0;
    base::u64 builtin_derive_expansion_visible_count = 0;
    base::u64 builtin_derive_expansion_query_reusable_count = 0;
    base::u64 builtin_derive_expansion_capability_candidate_count = 0;
    base::u64 builtin_derive_semantic_plan_count = 0;
    base::u64 builtin_derive_semantic_plan_visible_count = 0;
    base::u64 builtin_derive_semantic_plan_query_reusable_count = 0;
    base::u64 builtin_derive_semantic_capability_count = 0;
    base::u64 builtin_derive_semantic_copy_capability_count = 0;
    base::u64 builtin_derive_semantic_eq_capability_count = 0;
    base::u64 builtin_derive_semantic_hash_capability_count = 0;
    base::u64 builtin_derive_parser_release_gate_count = 0;
    base::u64 builtin_derive_parser_release_visible_count = 0;
    base::u64 builtin_derive_parser_release_query_reusable_count = 0;
    base::u64 builtin_derive_parser_release_parser_consumable_count = 0;
    base::u64 builtin_derive_release_hardening_matrix_count = 0;
    base::u64 builtin_derive_release_hardening_visible_count = 0;
    base::u64 builtin_derive_release_hardening_query_reusable_count = 0;
    base::u64 builtin_derive_release_hardening_negative_matrix_complete_count = 0;
    base::u64 builtin_derive_release_hardening_parser_consumable_count = 0;
    base::u64 builtin_derive_debug_dump_contract_count = 0;
    base::u64 builtin_derive_debug_dump_contract_visible_count = 0;
    base::u64 builtin_derive_debug_dump_query_reusable_count = 0;
    base::u64 builtin_derive_debug_dump_complete_count = 0;
    base::u64 builtin_derive_debug_dump_parser_consumable_count = 0;
    base::u64 builtin_derive_rollback_diagnostic_gate_count = 0;
    base::u64 builtin_derive_rollback_diagnostic_visible_count = 0;
    base::u64 builtin_derive_rollback_diagnostic_query_reusable_count = 0;
    base::u64 builtin_derive_rollback_diagnostic_design_complete_count = 0;
    base::u64 builtin_derive_rollback_diagnostic_parser_consumable_count = 0;
    base::u64 generated_source_text_count = 0;
    base::u64 parse_ready_token_buffer_count = 0;
    base::u64 parsed_generated_part_count = 0;
    base::u64 merged_generated_part_count = 0;
    base::u64 user_generated_code_count = 0;
    base::u64 standard_library_required_count = 0;
    base::u64 runtime_required_count = 0;
    base::u64 external_process_required_count = 0;
};

struct EarlyItemExpansionResult {
    std::string name;
    query::MacroExpansionPlan plan;
    std::vector<EarlyItemMacroInput> inputs;
    std::vector<GeneratedModulePartPlaceholder> generated_parts;
    std::vector<GeneratedModulePartParseMergeStub> generated_part_stubs;
    std::vector<ExpansionSourceMapPlaceholder> source_maps;
    std::vector<ExpansionHygieneStub> hygiene_stubs;
    std::vector<ExpansionTraceStub> trace_stubs;
    std::vector<GeneratedItemDeclarationStub> generated_item_declarations;
    std::vector<DeclaredGeneratedNameStub> declared_generated_names;
    std::vector<TokenMaterializationAdmissionStub> token_materialization_admissions;
    std::vector<GeneratedTokenBufferStub> generated_token_buffers;
    std::vector<GeneratedTokenRecord> generated_token_records;
    std::vector<GeneratedTokenParserAdmissionGateStub> parser_admission_gates;
    std::vector<ParserAdmissionDiagnosticProjectionStub> parser_admission_diagnostics;
    std::vector<ParserAdmissionDiagnosticReportEntry> parser_admission_report_entries;
    std::vector<ParserAdmissionDiagnosticReport> parser_admission_reports;
    std::vector<GeneratedTokenParserReadinessPreflightEntry> parser_readiness_preflight_entries;
    std::vector<GeneratedTokenParserConsumptionContractGate> parser_consumption_contract_gates;
    std::vector<MacroExpansionBoundaryClosureReport> macro_boundary_closure_reports;
    std::vector<BuiltinDeriveExpansionAdmissionGate> builtin_derive_expansion_admissions;
    std::vector<BuiltinDeriveSemanticExpansionPlan> builtin_derive_semantic_plans;
    std::vector<BuiltinDeriveParserConsumptionReleaseGate> builtin_derive_parser_release_gates;
    std::vector<BuiltinDeriveReleaseHardeningMatrix> builtin_derive_release_hardening_matrices;
    std::vector<BuiltinDeriveDebugDumpStabilityContract> builtin_derive_debug_dump_contracts;
    std::vector<BuiltinDeriveRollbackDiagnosticDesignGate> builtin_derive_rollback_diagnostic_gates;
    EarlyItemExpansionSummary summary;
    query::StableFingerprint128 fingerprint;
};

[[nodiscard]] std::string_view early_item_expansion_disposition_name(
    EarlyItemExpansionDisposition disposition) noexcept;
[[nodiscard]] std::string_view generated_module_part_lifecycle_state_name(
    GeneratedModulePartLifecycleState state) noexcept;
[[nodiscard]] bool is_valid(EarlyItemExpansionDisposition disposition) noexcept;
[[nodiscard]] bool is_valid(GeneratedModulePartLifecycleState state) noexcept;
[[nodiscard]] bool is_valid(const EarlyItemMacroInput& input) noexcept;
[[nodiscard]] bool is_valid(const GeneratedModulePartPlaceholder& placeholder) noexcept;
[[nodiscard]] bool is_valid(const GeneratedModulePartParseMergeStub& stub) noexcept;
[[nodiscard]] bool is_valid(const ExpansionSourceMapPlaceholder& placeholder) noexcept;
[[nodiscard]] bool is_valid(const ExpansionHygieneStub& stub) noexcept;
[[nodiscard]] bool is_valid(const ExpansionTraceStub& stub) noexcept;
[[nodiscard]] bool is_valid(const GeneratedItemDeclarationStub& stub) noexcept;
[[nodiscard]] bool is_valid(const DeclaredGeneratedNameStub& stub) noexcept;
[[nodiscard]] bool is_valid(const TokenMaterializationAdmissionStub& stub) noexcept;
[[nodiscard]] bool is_valid(const GeneratedTokenBufferStub& stub) noexcept;
[[nodiscard]] bool is_valid(const GeneratedTokenRecord& record) noexcept;
[[nodiscard]] bool is_valid(const GeneratedTokenParserAdmissionGateStub& stub) noexcept;
[[nodiscard]] bool is_valid(const ParserAdmissionDiagnosticProjectionStub& stub) noexcept;
[[nodiscard]] bool is_valid(const ParserAdmissionDiagnosticReportEntry& entry) noexcept;
[[nodiscard]] bool is_valid(const ParserAdmissionDiagnosticReport& report) noexcept;
[[nodiscard]] bool is_valid(const GeneratedTokenParserReadinessPreflightEntry& entry) noexcept;
[[nodiscard]] bool is_valid(const GeneratedTokenParserConsumptionContractGate& gate) noexcept;
[[nodiscard]] bool is_valid(const MacroExpansionBoundaryClosureReport& report) noexcept;
[[nodiscard]] bool is_valid(const BuiltinDeriveExpansionAdmissionGate& gate) noexcept;
[[nodiscard]] bool is_valid(const BuiltinDeriveSemanticExpansionPlan& plan) noexcept;
[[nodiscard]] bool is_valid(const BuiltinDeriveParserConsumptionReleaseGate& gate) noexcept;
[[nodiscard]] bool is_valid(const BuiltinDeriveReleaseHardeningMatrix& matrix) noexcept;
[[nodiscard]] bool is_valid(const BuiltinDeriveDebugDumpStabilityContract& contract) noexcept;
[[nodiscard]] bool is_valid(const BuiltinDeriveRollbackDiagnosticDesignGate& gate) noexcept;
[[nodiscard]] bool is_valid(const EarlyItemExpansionSummary& summary, const EarlyItemExpansionResult& result) noexcept;
[[nodiscard]] bool is_valid(const EarlyItemExpansionResult& result) noexcept;

[[nodiscard]] EarlyItemExpansionSummary summarize_early_item_expansion_counts(
    const EarlyItemExpansionResult& result) noexcept;
[[nodiscard]] query::StableFingerprint128 early_item_expansion_fingerprint(
    const EarlyItemExpansionResult& result) noexcept;
[[nodiscard]] std::string summarize_early_item_expansion(const EarlyItemExpansionResult& result);
[[nodiscard]] std::string dump_early_item_expansion(const EarlyItemExpansionResult& result);

[[nodiscard]] base::Result<EarlyItemExpansionResult> expand_early_item_macros_noop(
    const syntax::AstModule& ast,
    std::span<const std::vector<query::ModulePartKey>> module_part_keys,
    const query::MacroExpansionPlan& plan = query::m21c_macro_expansion_plan_baseline());

} // namespace aurex::frontend::macro
