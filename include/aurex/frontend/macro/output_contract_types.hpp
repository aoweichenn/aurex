#pragma once

#include <aurex/frontend/syntax/ast/item_nodes.hpp>
#include <aurex/frontend/syntax/core/ast_ids.hpp>
#include <aurex/infrastructure/base/source.hpp>
#include <aurex/infrastructure/query/query_key.hpp>

#include <string>

namespace aurex::frontend::macro {

enum class AurexMacroOutputContractOriginKind : base::u8 {
    matcher_to_call_binding,
    user_derive_target_schema,
};

struct AurexMacroOutputContractAdmissionGate {
    syntax::ItemId consumer_item;
    syntax::ItemId macro_item;
    syntax::ModuleId module;
    base::u32 part_index = 0;
    base::u32 output_index = 0;
    query::ModulePartKey attached_part;
    query::ModulePartKey generated_part;
    query::StableFingerprint128 surface_admission_identity;
    query::StableFingerprint128 source_admission_identity;
    query::StableFingerprint128 matcher_identity;
    query::StableFingerprint128 target_schema_identity;
    query::StableFingerprint128 output_contract_identity;
    query::StableFingerprint128 token_buffer_identity;
    query::StableFingerprint128 declared_name_policy_identity;
    query::StableFingerprint128 hygiene_mark;
    query::StableFingerprint128 diagnostic_anchor_identity;
    query::StableFingerprint128 source_map_identity;
    AurexMacroOutputContractOriginKind origin_kind =
        AurexMacroOutputContractOriginKind::matcher_to_call_binding;
    syntax::MacroDeclKind macro_kind = syntax::MacroDeclKind::declarative;
    std::string macro_name;
    std::string consumer_name;
    std::string output_policy;
    std::string query_name;
    std::string token_buffer_name;
    std::string blocker_reason;
    base::SourceRange source_range{};
    base::SourceRange output_range{};
    base::u64 planned_token_count = 0;
    bool compiler_owned_output = true;
    bool source_anchor_available = true;
    bool source_map_available = true;
    bool hygiene_mark_available = true;
    bool diagnostic_projection_available = true;
    bool declared_name_policy_available = true;
    bool token_buffer_materialized = false;
    bool generated_source_text = false;
    bool parse_ready = false;
    bool parser_consumable = false;
    bool parser_consumption_enabled = false;
    bool ast_mutated = false;
    bool sema_visible_generated_items = false;
    bool standard_library_required = false;
    bool runtime_required = false;
    bool external_process_required = false;
    bool produced_user_generated_code = false;
    bool gate_visible = true;
    bool query_reusable = true;
};

struct AurexMacroOutputDeclaredNamePolicyAdmissionGate {
    syntax::ItemId consumer_item;
    syntax::ItemId macro_item;
    syntax::ModuleId module;
    base::u32 part_index = 0;
    base::u32 output_index = 0;
    query::ModulePartKey attached_part;
    query::ModulePartKey generated_part;
    query::StableFingerprint128 output_contract_identity;
    query::StableFingerprint128 declared_name_policy_identity;
    query::StableFingerprint128 declared_name_set_fingerprint;
    query::StableFingerprint128 hygiene_mark;
    query::StableFingerprint128 diagnostic_anchor_identity;
    AurexMacroOutputContractOriginKind origin_kind =
        AurexMacroOutputContractOriginKind::matcher_to_call_binding;
    syntax::MacroDeclKind macro_kind = syntax::MacroDeclKind::declarative;
    std::string macro_name;
    std::string consumer_name;
    std::string declared_name_policy;
    std::string declared_name_namespace;
    std::string query_name;
    std::string blocker_reason;
    base::u64 planned_declared_name_count = 0;
    bool compiler_owned_output = true;
    bool declared_name_set_reserved = true;
    bool lookup_visible = false;
    bool export_visible = false;
    bool sema_visible = false;
    bool parser_consumable = false;
    bool ast_mutated = false;
    bool standard_library_required = false;
    bool runtime_required = false;
    bool external_process_required = false;
    bool produced_user_generated_code = false;
    bool gate_visible = true;
    bool query_reusable = true;
};

struct AurexMacroOutputDiagnosticProjectionAdmissionGate {
    syntax::ItemId consumer_item;
    syntax::ItemId macro_item;
    syntax::ModuleId module;
    base::u32 part_index = 0;
    base::u32 output_index = 0;
    query::ModulePartKey attached_part;
    query::ModulePartKey generated_part;
    query::StableFingerprint128 output_contract_identity;
    query::StableFingerprint128 token_buffer_identity;
    query::StableFingerprint128 diagnostic_projection_identity;
    query::StableFingerprint128 diagnostic_anchor_identity;
    query::StableFingerprint128 source_map_identity;
    query::StableFingerprint128 hygiene_mark;
    AurexMacroOutputContractOriginKind origin_kind =
        AurexMacroOutputContractOriginKind::matcher_to_call_binding;
    syntax::MacroDeclKind macro_kind = syntax::MacroDeclKind::declarative;
    std::string macro_name;
    std::string consumer_name;
    std::string diagnostic_policy;
    std::string query_name;
    std::string blocker_category;
    std::string user_message;
    std::string blocker_reason;
    base::SourceRange primary_anchor{};
    base::SourceRange output_anchor{};
    bool compiler_owned_output = true;
    bool source_anchor_available = true;
    bool source_map_available = true;
    bool hygiene_mark_available = true;
    bool debug_projection_available = true;
    bool diagnostic_emission_enabled = false;
    bool parser_consumable = false;
    bool ast_mutated = false;
    bool sema_visible_generated_items = false;
    bool standard_library_required = false;
    bool runtime_required = false;
    bool external_process_required = false;
    bool produced_user_generated_code = false;
    bool gate_visible = true;
    bool query_reusable = true;
};

} // namespace aurex::frontend::macro
