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
