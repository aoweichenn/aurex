#include <aurex/frontend/macro/early_item_expansion.hpp>
#include <aurex/infrastructure/base/integer.hpp>

#include <algorithm>
#include <limits>
#include <sstream>
#include <utility>

namespace aurex::frontend::macro {
namespace {

constexpr std::string_view FRONTEND_MACRO_M26C_EXPANSION_NAME =
    "M26c Builtin Derive Cursor Rollback AST Mutation Verifier Closure";
constexpr std::string_view FRONTEND_MACRO_M26C_EXPANSION_FINGERPRINT_MARKER =
    "frontend.macro.m26c.builtin_derive_cursor_rollback_ast_mutation_verifier_closure.v1";
constexpr std::string_view FRONTEND_MACRO_M27A_AUREX_MACRO_SURFACE_ADMISSION_MARKER =
    "frontend.macro.m27a.aurex_macro_surface_admission_gate.v1";
constexpr std::string_view FRONTEND_MACRO_M27A_AUREX_MACRO_BODY_FINGERPRINT_MARKER =
    "frontend.macro.m27a.aurex_macro_body_token_tree.v1";
constexpr std::string_view FRONTEND_MACRO_M27A_AUREX_MACRO_SURFACE_POLICY =
    "aurex_macro_surface_admission_gate_v1";
constexpr std::string_view FRONTEND_MACRO_M27A_QUERY_NAME_PREFIX = "m27a-aurex-macro-surface:";
constexpr std::string_view FRONTEND_MACRO_M27B_TYPED_MATCHER_MARKER =
    "frontend.macro.m27b.typed_matcher_admission_gate.v1";
constexpr std::string_view FRONTEND_MACRO_M27B_MATCHER_FINGERPRINT_MARKER =
    "frontend.macro.m27b.typed_matcher_fingerprint.v1";
constexpr std::string_view FRONTEND_MACRO_M27B_DEFINITION_SITE_HYGIENE_MARKER =
    "frontend.macro.m27b.definition_site_hygiene_admission_gate.v1";
constexpr std::string_view FRONTEND_MACRO_M27B_DEFINITION_SITE_MARK_MARKER =
    "frontend.macro.m27b.definition_site_mark.v1";
constexpr std::string_view FRONTEND_MACRO_M27B_FRESH_NAME_SCOPE_MARKER =
    "frontend.macro.m27b.fresh_name_scope.v1";
constexpr std::string_view FRONTEND_MACRO_M27B_DIAGNOSTIC_ANCHOR_MARKER =
    "frontend.macro.m27b.diagnostic_anchor.v1";
constexpr std::string_view FRONTEND_MACRO_M27B_TYPED_MATCHER_POLICY =
    "aurex_macro_typed_matcher_admission_v1";
constexpr std::string_view FRONTEND_MACRO_M27B_DEFINITION_SITE_HYGIENE_POLICY =
    "aurex_macro_definition_site_hygiene_admission_v1";
constexpr std::string_view FRONTEND_MACRO_M27B_TYPED_MATCHER_QUERY_PREFIX = "m27b-aurex-macro-typed-matcher:";
constexpr std::string_view FRONTEND_MACRO_M27B_DEFINITION_SITE_HYGIENE_QUERY_PREFIX =
    "m27b-aurex-macro-definition-site-hygiene:";
constexpr std::string_view FRONTEND_MACRO_M27B_TYPED_MATCHER_BLOCKER =
    "Aurex typed matcher execution is admission-only in M27b";
constexpr std::string_view FRONTEND_MACRO_M27B_UNKNOWN_MATCHER_BLOCKER =
    "Aurex macro matcher shape is unrecognized and remains blocked in M27b";
constexpr std::string_view FRONTEND_MACRO_M27B_DEFINITION_SITE_HYGIENE_BLOCKER =
    "Aurex definition-site hygiene resolution is admission-only in M27b";
constexpr std::string_view FRONTEND_MACRO_M27B_MATCHER_KEYWORD_TEXT = "match";
constexpr std::string_view FRONTEND_MACRO_M27B_EXPR_LIST_MATCHER_TEXT = "expr_list";
constexpr std::string_view FRONTEND_MACRO_M27B_ITEM_MATCHER_TEXT = "item";
constexpr std::string_view FRONTEND_MACRO_M27B_TOKENS_MATCHER_TEXT = "tokens";
constexpr base::usize FRONTEND_MACRO_M27B_MATCHER_HEAD_OFFSET = 1U;
constexpr base::usize FRONTEND_MACRO_M27B_MATCHER_OPEN_PAREN_OFFSET = 2U;
constexpr base::usize FRONTEND_MACRO_M27B_MATCHER_BINDING_OFFSET = 3U;
constexpr base::usize FRONTEND_MACRO_M27B_MATCHER_CLOSE_PAREN_OFFSET = 4U;
constexpr base::usize FRONTEND_MACRO_M27B_MATCHER_ARROW_OFFSET = 5U;
constexpr base::usize FRONTEND_MACRO_M27B_MATCHER_OUTPUT_OPEN_OFFSET = 6U;
constexpr base::u32 FRONTEND_MACRO_M27B_MATCHER_TOP_LEVEL_DEPTH = 1U;
constexpr base::u32 FRONTEND_MACRO_M27B_MATCHER_BINDING_DEPTH = 2U;
constexpr std::string_view FRONTEND_MACRO_M27A_DECLARATIVE_BLOCKER =
    "Aurex declarative macro expansion is parser-blocked in M27a";
constexpr std::string_view FRONTEND_MACRO_M27B_USER_DERIVE_BLOCKER =
    "Aurex user derive macro expansion is admission-only in M27b";
constexpr std::string_view FRONTEND_MACRO_M27C_COMPILE_TIME_BLOCKER =
    "Aurex compile-time macro execution is admission-only in M27c";
constexpr std::string_view FRONTEND_MACRO_M21D_TOKEN_TREE_FINGERPRINT_MARKER =
    "frontend.macro.m21d.attribute_token_tree.v1";
constexpr std::string_view FRONTEND_MACRO_M21D_QUERY_KEY_FINGERPRINT_MARKER =
    "frontend.macro.m21d.early_item_query_key.v1";
constexpr std::string_view FRONTEND_MACRO_M21D_GENERATED_PART_NAME_PREFIX = "#macro-generated:";
constexpr std::string_view FRONTEND_MACRO_M21D_ITEM_MODULES_MISMATCH =
    "early item macro expansion requires one module owner per item";
constexpr std::string_view FRONTEND_MACRO_M21D_ITEM_PARTS_MISMATCH =
    "early item macro expansion requires one module part index per item";
constexpr std::string_view FRONTEND_MACRO_M21D_INVALID_PLAN =
    "early item macro expansion requires a valid M21c macro expansion plan";
constexpr std::string_view FRONTEND_MACRO_M21D_MISSING_MODULE_PART_KEY =
    "early item macro expansion missing module part key for attached item";
constexpr std::string_view FRONTEND_MACRO_M21D_GENERATED_PART_PLACEHOLDER_MARKER =
    "frontend.macro.m21d.generated_part_placeholder.v1";
constexpr base::u32 FRONTEND_MACRO_M21D_GENERATED_PART_INDEX_OFFSET = 100'000U;
constexpr std::string_view FRONTEND_MACRO_M21E_PARSE_MERGE_STUB_MARKER =
    "frontend.macro.m21e.generated_part_parse_merge_stub.v1";
constexpr std::string_view FRONTEND_MACRO_M21E_GENERATED_BUFFER_IDENTITY_MARKER =
    "frontend.macro.m21e.generated_part_buffer_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M21E_PARSE_CONFIG_MARKER =
    "frontend.macro.m21e.generated_part_parse_config.v1";
constexpr std::string_view FRONTEND_MACRO_M21E_MERGE_ORDERING_MARKER =
    "frontend.macro.m21e.generated_part_merge_ordering.v1";
constexpr std::string_view FRONTEND_MACRO_M21E_GENERATED_BUFFER_PREFIX =
    "m21e-noop-generated-buffer:";
constexpr std::string_view FRONTEND_MACRO_M21E_PARSE_MERGE_BLOCKER =
    "generated module part parse and merge are blocked in M21e";
constexpr std::string_view FRONTEND_MACRO_M21F_HYGIENE_STUB_MARKER =
    "frontend.macro.m21f.hygiene_stub.v1";
constexpr std::string_view FRONTEND_MACRO_M21F_CALL_SITE_MARK_MARKER =
    "frontend.macro.m21f.call_site_mark.v1";
constexpr std::string_view FRONTEND_MACRO_M21F_DEFINITION_SITE_MARK_MARKER =
    "frontend.macro.m21f.definition_site_mark.v1";
constexpr std::string_view FRONTEND_MACRO_M21F_GENERATED_FRESH_MARK_MARKER =
    "frontend.macro.m21f.generated_fresh_mark.v1";
constexpr std::string_view FRONTEND_MACRO_M21F_DECLARED_NAME_SET_MARKER =
    "frontend.macro.m21f.declared_name_set.v1";
constexpr std::string_view FRONTEND_MACRO_M21F_TRACE_STUB_MARKER =
    "frontend.macro.m21f.trace_stub.v1";
constexpr std::string_view FRONTEND_MACRO_M21F_TRACE_IDENTITY_MARKER =
    "frontend.macro.m21f.trace_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M21F_GENERATED_SOURCE_MAP_MARKER =
    "frontend.macro.m21f.generated_source_map_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M21F_DIAGNOSTIC_ANCHOR_MARKER =
    "frontend.macro.m21f.diagnostic_anchor.v1";
constexpr std::string_view FRONTEND_MACRO_M21F_HYGIENE_POLICY = "origin_mark_hygiene_v1";
constexpr std::string_view FRONTEND_MACRO_M21F_TRACE_POLICY = "expansion_source_map_debug_trace_v1";
constexpr std::string_view FRONTEND_MACRO_M21F_TRACE_BLOCKER =
    "real macro source map and debug trace are blocked in M21f";
constexpr std::string_view FRONTEND_MACRO_M21G_GENERATED_ITEM_DECLARATION_MARKER =
    "frontend.macro.m21g.generated_item_declaration_stub.v1";
constexpr std::string_view FRONTEND_MACRO_M21G_DECLARED_NAME_STUB_MARKER =
    "frontend.macro.m21g.declared_generated_name_stub.v1";
constexpr std::string_view FRONTEND_MACRO_M21G_GENERATED_ITEM_KEY_MARKER =
    "frontend.macro.m21g.generated_item_key.v1";
constexpr std::string_view FRONTEND_MACRO_M21G_DECLARATION_IDENTITY_MARKER =
    "frontend.macro.m21g.declaration_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M21G_DECLARED_NAME_IDENTITY_MARKER =
    "frontend.macro.m21g.declared_name_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M21G_DECLARATION_ROLE =
    "attached_item_codegen_declared_names_v1";
constexpr std::string_view FRONTEND_MACRO_M21G_DECLARED_NAME_NAMESPACE = "item";
constexpr std::string_view FRONTEND_MACRO_M21G_DECLARATION_BLOCKER =
    "generated item declaration materialization is blocked in M21g";
constexpr std::string_view FRONTEND_MACRO_M21G_DECLARED_NAME_BLOCKER =
    "declared generated name lookup is blocked in M21g";
constexpr std::string_view FRONTEND_MACRO_M21G_GENERATED_NAME_PREFIX =
    "__aurex_macro_declared:";
constexpr std::string_view FRONTEND_MACRO_M21G_MISSING_GENERATED_PART =
    "early item macro expansion missing generated module part placeholder";
constexpr std::string_view FRONTEND_MACRO_M21H_ADMISSION_STUB_MARKER =
    "frontend.macro.m21h.token_materialization_admission_stub.v1";
constexpr std::string_view FRONTEND_MACRO_M21H_TOKEN_BUFFER_STUB_MARKER =
    "frontend.macro.m21h.generated_token_buffer_stub.v1";
constexpr std::string_view FRONTEND_MACRO_M21I_GENERATED_TOKEN_RECORD_MARKER =
    "frontend.macro.m21i.generated_token_record.v1";
constexpr std::string_view FRONTEND_MACRO_M21I_TOKEN_MATERIALIZATION_IDENTITY_MARKER =
    "frontend.macro.m21i.token_materialization_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M21J_PARSER_ADMISSION_GATE_MARKER =
    "frontend.macro.m21j.generated_token_parser_admission_gate_stub.v1";
constexpr std::string_view FRONTEND_MACRO_M21J_PARSE_GATE_IDENTITY_MARKER =
    "frontend.macro.m21j.parse_gate_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M21K_DIAGNOSTIC_PROJECTION_MARKER =
    "frontend.macro.m21k.parser_admission_diagnostic_projection_stub.v1";
constexpr std::string_view FRONTEND_MACRO_M21K_DIAGNOSTIC_IDENTITY_MARKER =
    "frontend.macro.m21k.parser_admission_diagnostic_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M21K_DIAGNOSTIC_ANCHOR_MARKER =
    "frontend.macro.m21k.parser_admission_diagnostic_anchor.v1";
constexpr std::string_view FRONTEND_MACRO_M21L_REPORT_ENTRY_MARKER =
    "frontend.macro.m21l.parser_admission_report_entry.v1";
constexpr std::string_view FRONTEND_MACRO_M21L_REPORT_ENTRY_IDENTITY_MARKER =
    "frontend.macro.m21l.parser_admission_report_entry_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M21L_REPORT_MARKER =
    "frontend.macro.m21l.parser_admission_report.v1";
constexpr std::string_view FRONTEND_MACRO_M21L_REPORT_IDENTITY_MARKER =
    "frontend.macro.m21l.parser_admission_report_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M21L_REPORT_ANCHOR_IDENTITY_MARKER =
    "frontend.macro.m21l.parser_admission_report_anchor_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M21L_REPORT_GROUPING_IDENTITY_MARKER =
    "frontend.macro.m21l.parser_admission_report_grouping_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M21H_TOKEN_PLAN_IDENTITY_MARKER =
    "frontend.macro.m21h.token_plan_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M21H_TOKEN_BUFFER_IDENTITY_MARKER =
    "frontend.macro.m21h.token_buffer_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M21H_ADMISSION_POLICY =
    "compiler_owned_attached_item_token_materialization_admission_v1";
constexpr std::string_view FRONTEND_MACRO_M21H_TOKEN_BUFFER_KIND =
    "compiler_owned_empty_token_stream";
constexpr std::string_view FRONTEND_MACRO_M21I_DERIVE_TOKEN_BUFFER_KIND =
    "compiler_owned_builtin_derive_token_stream_prototype";
constexpr std::string_view FRONTEND_MACRO_M21H_TOKEN_STREAM_PREFIX =
    "m21h-token-stream:";
constexpr std::string_view FRONTEND_MACRO_M21I_DERIVE_TOKEN_PRODUCER_POLICY =
    "compiler_owned_builtin_derive_token_producer_prototype_v1";
constexpr std::string_view FRONTEND_MACRO_M21I_EMPTY_TOKEN_PRODUCER_POLICY =
    "compiler_owned_blocked_empty_token_producer_v1";
constexpr std::string_view FRONTEND_MACRO_M21I_DERIVE_ADMISSION_BLOCKER =
    "compiler-owned derive token prototype remains parser-blocked in M21i";
constexpr std::string_view FRONTEND_MACRO_M21I_EMPTY_ADMISSION_BLOCKER =
    "non-derive item attribute token materialization remains blocked in M21i";
constexpr std::string_view FRONTEND_MACRO_M21I_DERIVE_TOKEN_BUFFER_BLOCKER =
    "compiler-owned generated token buffer remains parser-blocked in M21i";
constexpr std::string_view FRONTEND_MACRO_M21I_EMPTY_TOKEN_BUFFER_BLOCKER =
    "non-derive generated token buffer remains empty and parser-blocked in M21i";
constexpr std::string_view FRONTEND_MACRO_M21J_PARSER_GATE_POLICY =
    "compiler_owned_generated_token_parser_admission_gate_v1";
constexpr std::string_view FRONTEND_MACRO_M21J_DERIVE_PARSE_BLOCKER =
    "compiler-owned derive generated token buffer parser admission remains blocked in M21j";
constexpr std::string_view FRONTEND_MACRO_M21J_EMPTY_PARSE_BLOCKER =
    "empty or non-derive generated token buffer parser admission remains blocked in M21j";
constexpr std::string_view FRONTEND_MACRO_M21J_MISSING_PARSE_MERGE_STUB =
    "early item macro expansion missing generated module part parse merge stub";
constexpr std::string_view FRONTEND_MACRO_M22_MISSING_INPUT_AST_VIEW =
    "early item macro expansion missing M22 input AST view";
constexpr std::string_view FRONTEND_MACRO_M21K_DIAGNOSTIC_POLICY =
    "parser_admission_blocked_diagnostic_projection_v1";
constexpr std::string_view FRONTEND_MACRO_M21K_DERIVE_BLOCKER_CATEGORY =
    "derive_token_buffer_parser_admission_blocked";
constexpr std::string_view FRONTEND_MACRO_M21K_EMPTY_BLOCKER_CATEGORY =
    "empty_token_buffer_parser_admission_blocked";
constexpr std::string_view FRONTEND_MACRO_M21K_GENERATED_PART_PARSE_BLOCKER =
    "generated module part parse remains blocked before parser admission diagnostics in M21k";
constexpr std::string_view FRONTEND_MACRO_M21K_DERIVE_USER_MESSAGE =
    "generated derive token buffer is compiler-owned but parser admission remains blocked in M21k";
constexpr std::string_view FRONTEND_MACRO_M21K_EMPTY_USER_MESSAGE =
    "generated token buffer is empty and parser admission remains blocked in M21k";
constexpr std::string_view FRONTEND_MACRO_M21K_DEBUG_PROJECTION_PREFIX =
    "m21k-parser-admission:";
constexpr std::string_view FRONTEND_MACRO_M21L_REPORT_POLICY =
    "parser_admission_blocked_report_query_projection_v1";
constexpr std::string_view FRONTEND_MACRO_M21L_QUERY_NAME_PREFIX =
    "m21l-parser-admission-report:";
constexpr std::string_view FRONTEND_MACRO_M21L_REPORT_BLOCKER =
    "parser admission diagnostic report remains parser-blocked in M21l";
constexpr std::string_view FRONTEND_MACRO_M21M_PREFLIGHT_ENTRY_MARKER =
    "frontend.macro.m21m.parser_readiness_preflight_entry.v1";
constexpr std::string_view FRONTEND_MACRO_M21M_PREFLIGHT_IDENTITY_MARKER =
    "frontend.macro.m21m.parser_readiness_preflight_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M21M_PREFLIGHT_POLICY =
    "generated_token_parser_consumption_readiness_preflight_v1";
constexpr std::string_view FRONTEND_MACRO_M21M_PREFLIGHT_BLOCKER =
    "parser consumption readiness preflight remains parser-blocked in M21m";
constexpr std::string_view FRONTEND_MACRO_M21M_DERIVE_TOKEN_STREAM_SHAPE =
    "derive_token_buffer_parser_input_candidate";
constexpr std::string_view FRONTEND_MACRO_M21M_EMPTY_TOKEN_STREAM_SHAPE =
    "empty_token_stream_parser_input_blocked";
constexpr std::string_view FRONTEND_MACRO_M21M_DELIMITER_BALANCED_STATE = "balanced";
constexpr std::string_view FRONTEND_MACRO_M21M_SOURCE_ANCHOR_COVERED_STATE = "covered";
constexpr std::string_view FRONTEND_MACRO_M21N_CONTRACT_GATE_MARKER =
    "frontend.macro.m21n.parser_consumption_contract_gate.v1";
constexpr std::string_view FRONTEND_MACRO_M21N_CONTRACT_IDENTITY_MARKER =
    "frontend.macro.m21n.parser_consumption_contract_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M21N_CONTRACT_GROUPING_IDENTITY_MARKER =
    "frontend.macro.m21n.parser_consumption_contract_grouping_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M21N_CONTRACT_ANCHOR_IDENTITY_MARKER =
    "frontend.macro.m21n.parser_consumption_contract_anchor_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M21N_CONTRACT_POLICY =
    "generated_token_parser_consumption_contract_gate_v1";
constexpr std::string_view FRONTEND_MACRO_M21N_CONTRACT_QUERY_NAME_PREFIX =
    "m21n-parser-consumption-contract:";
constexpr std::string_view FRONTEND_MACRO_M21N_CONTRACT_BLOCKER =
    "parser consumption contract remains parser-blocked in M21n";
constexpr std::string_view FRONTEND_MACRO_M21O_CLOSURE_REPORT_MARKER =
    "frontend.macro.m21o.macro_boundary_closure_report.v1";
constexpr std::string_view FRONTEND_MACRO_M21O_CLOSURE_IDENTITY_MARKER =
    "frontend.macro.m21o.macro_boundary_closure_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M21O_CLOSURE_GROUPING_IDENTITY_MARKER =
    "frontend.macro.m21o.macro_boundary_closure_grouping_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M21O_CLOSURE_POLICY =
    "m21_macro_expansion_boundary_release_closure_v1";
constexpr std::string_view FRONTEND_MACRO_M21O_CLOSURE_QUERY_NAME =
    "m21o-macro-boundary-closure";
constexpr std::string_view FRONTEND_MACRO_M21O_CLOSURE_BLOCKER =
    "M21 macro expansion boundary remains parser-blocked after M21o closure";
constexpr std::string_view FRONTEND_MACRO_M22A_ADMISSION_GATE_MARKER =
    "frontend.macro.m22a.builtin_derive_expansion_admission_gate.v1";
constexpr std::string_view FRONTEND_MACRO_M22A_ADMISSION_IDENTITY_MARKER =
    "frontend.macro.m22a.builtin_derive_expansion_admission_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M22A_ADMISSION_POLICY =
    "builtin_derive_expansion_admission_gate_v1";
constexpr std::string_view FRONTEND_MACRO_M22A_DERIVE_ADMISSION_KIND =
    "builtin_derive_expansion_candidate";
constexpr std::string_view FRONTEND_MACRO_M22A_NON_DERIVE_BLOCKED_KIND =
    "non_derive_attribute_expansion_blocked";
constexpr std::string_view FRONTEND_MACRO_M22A_QUERY_NAME_PREFIX =
    "m22a-builtin-derive-admission:";
constexpr std::string_view FRONTEND_MACRO_M22A_DERIVE_BLOCKER =
    "builtin derive expansion admission remains parser-blocked in M22a";
constexpr std::string_view FRONTEND_MACRO_M22A_NON_DERIVE_BLOCKER =
    "non-derive item attribute expansion remains blocked in M22a";
constexpr std::string_view FRONTEND_MACRO_M22B_SEMANTIC_PLAN_MARKER =
    "frontend.macro.m22b.builtin_derive_semantic_expansion_plan.v1";
constexpr std::string_view FRONTEND_MACRO_M22B_SEMANTIC_PLAN_IDENTITY_MARKER =
    "frontend.macro.m22b.builtin_derive_semantic_plan_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M22B_CAPABILITY_SET_IDENTITY_MARKER =
    "frontend.macro.m22b.builtin_derive_capability_set_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M22B_SEMANTIC_POLICY =
    "builtin_derive_semantic_expansion_plan_v1";
constexpr std::string_view FRONTEND_MACRO_M22B_SEMANTIC_MODEL =
    "capability_fact_lowering_plan";
constexpr std::string_view FRONTEND_MACRO_M22B_BLOCKER =
    "builtin derive semantic expansion remains capability-only and parser-blocked in M22b";
constexpr std::string_view FRONTEND_MACRO_M22C_RELEASE_GATE_MARKER =
    "frontend.macro.m22c.builtin_derive_parser_consumption_release_gate.v1";
constexpr std::string_view FRONTEND_MACRO_M22C_RELEASE_GATE_IDENTITY_MARKER =
    "frontend.macro.m22c.builtin_derive_parser_release_gate_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M22C_ADMISSION_GROUP_IDENTITY_MARKER =
    "frontend.macro.m22c.builtin_derive_admission_group_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M22C_SEMANTIC_PLAN_GROUP_IDENTITY_MARKER =
    "frontend.macro.m22c.builtin_derive_semantic_plan_group_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M22C_RELEASE_POLICY =
    "builtin_derive_parser_consumption_release_gate_v1";
constexpr std::string_view FRONTEND_MACRO_M22C_RELEASE_QUERY_NAME_PREFIX =
    "m22c-builtin-derive-parser-release:";
constexpr std::string_view FRONTEND_MACRO_M22C_RELEASE_BLOCKER =
    "builtin derive parser consumption release remains blocked in M22c";
constexpr std::string_view FRONTEND_MACRO_M22D_HARDENING_MATRIX_MARKER =
    "frontend.macro.m22d.builtin_derive_release_hardening_matrix.v1";
constexpr std::string_view FRONTEND_MACRO_M22D_HARDENING_MATRIX_IDENTITY_MARKER =
    "frontend.macro.m22d.builtin_derive_release_hardening_matrix_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M22D_HARDENING_POLICY =
    "builtin_derive_release_hardening_matrix_v1";
constexpr std::string_view FRONTEND_MACRO_M22D_HARDENING_QUERY_NAME_PREFIX =
    "m22d-builtin-derive-release-hardening:";
constexpr std::string_view FRONTEND_MACRO_M22D_HARDENING_BLOCKER =
    "builtin derive release hardening matrix keeps parser consumption blocked in M22d";
constexpr std::string_view FRONTEND_MACRO_M22E_DEBUG_DUMP_CONTRACT_MARKER =
    "frontend.macro.m22e.builtin_derive_debug_dump_stability_contract.v1";
constexpr std::string_view FRONTEND_MACRO_M22E_DEBUG_DUMP_CONTRACT_IDENTITY_MARKER =
    "frontend.macro.m22e.builtin_derive_debug_dump_stability_contract_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M22E_DEBUG_DUMP_POLICY =
    "builtin_derive_debug_dump_stability_contract_v1";
constexpr std::string_view FRONTEND_MACRO_M22E_DEBUG_DUMP_QUERY_NAME_PREFIX =
    "m22e-builtin-derive-debug-dump:";
constexpr std::string_view FRONTEND_MACRO_M22E_DEBUG_DUMP_BLOCKER =
    "builtin derive debug dump stability remains facts-only and parser-blocked in M22e";
constexpr base::u64 FRONTEND_MACRO_M22E_DEBUG_DUMP_SECTION_COUNT = 4U;
constexpr std::string_view FRONTEND_MACRO_M22F_ROLLBACK_GATE_MARKER =
    "frontend.macro.m22f.builtin_derive_rollback_diagnostic_design_gate.v1";
constexpr std::string_view FRONTEND_MACRO_M22F_ROLLBACK_GATE_IDENTITY_MARKER =
    "frontend.macro.m22f.builtin_derive_rollback_diagnostic_gate_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M22F_ROLLBACK_POLICY =
    "builtin_derive_rollback_diagnostic_design_gate_v1";
constexpr std::string_view FRONTEND_MACRO_M22F_ROLLBACK_QUERY_NAME_PREFIX =
    "m22f-builtin-derive-rollback-diagnostic:";
constexpr std::string_view FRONTEND_MACRO_M22F_ROLLBACK_BLOCKER =
    "builtin derive rollback diagnostics remain design-only and parser-blocked in M22f";
constexpr std::string_view FRONTEND_MACRO_M23A_ADMISSION_PROTOCOL_MARKER =
    "frontend.macro.m23a.builtin_derive_parser_consumption_admission_protocol.v1";
constexpr std::string_view FRONTEND_MACRO_M23A_ADMISSION_PROTOCOL_IDENTITY_MARKER =
    "frontend.macro.m23a.builtin_derive_parser_consumption_admission_protocol_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M23A_ADMISSION_POLICY =
    "builtin_derive_parser_consumption_admission_protocol_v1";
constexpr std::string_view FRONTEND_MACRO_M23A_ADMISSION_QUERY_NAME_PREFIX =
    "m23a-builtin-derive-parser-consumption-admission:";
constexpr std::string_view FRONTEND_MACRO_M23A_ADMISSION_BLOCKER =
    "builtin derive parser consumption admission protocol remains no-parser-consumption in M23a";
constexpr std::string_view FRONTEND_MACRO_M23B_CHECKPOINT_PROTOCOL_MARKER =
    "frontend.macro.m23b.builtin_derive_checkpoint_rollback_protocol.v1";
constexpr std::string_view FRONTEND_MACRO_M23B_CHECKPOINT_PROTOCOL_IDENTITY_MARKER =
    "frontend.macro.m23b.builtin_derive_checkpoint_rollback_protocol_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M23B_CHECKPOINT_POLICY =
    "builtin_derive_parser_checkpoint_rollback_protocol_v1";
constexpr std::string_view FRONTEND_MACRO_M23B_CHECKPOINT_QUERY_NAME_PREFIX =
    "m23b-builtin-derive-checkpoint-rollback:";
constexpr std::string_view FRONTEND_MACRO_M23B_CHECKPOINT_BLOCKER =
    "builtin derive checkpoint rollback protocol remains design-only and parser-blocked in M23b";
constexpr base::u64 FRONTEND_MACRO_M23B_CHECKPOINT_PLAN_COUNT = 3U;
constexpr std::string_view FRONTEND_MACRO_M23C_VERIFICATION_CLOSURE_MARKER =
    "frontend.macro.m23c.builtin_derive_preconsumption_verification_closure.v1";
constexpr std::string_view FRONTEND_MACRO_M23C_VERIFICATION_CLOSURE_IDENTITY_MARKER =
    "frontend.macro.m23c.builtin_derive_preconsumption_verification_closure_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M23C_VERIFICATION_POLICY =
    "builtin_derive_parser_preconsumption_verification_closure_v1";
constexpr std::string_view FRONTEND_MACRO_M23C_VERIFICATION_QUERY_NAME_PREFIX =
    "m23c-builtin-derive-preconsumption-verification:";
constexpr std::string_view FRONTEND_MACRO_M23C_VERIFICATION_BLOCKER =
    "builtin derive pre-consumption verification closure keeps parser consumption blocked in M23c";
constexpr std::string_view FRONTEND_MACRO_M24A_DRY_RUN_ADAPTER_MARKER =
    "frontend.macro.m24a.builtin_derive_controlled_parser_dry_run_adapter.v1";
constexpr std::string_view FRONTEND_MACRO_M24A_DRY_RUN_ADAPTER_IDENTITY_MARKER =
    "frontend.macro.m24a.builtin_derive_controlled_parser_dry_run_adapter_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M24A_DRY_RUN_ADAPTER_POLICY =
    "builtin_derive_controlled_parser_dry_run_adapter_v1";
constexpr std::string_view FRONTEND_MACRO_M24A_DRY_RUN_ADAPTER_QUERY_NAME_PREFIX =
    "m24a-builtin-derive-controlled-parser-dry-run:";
constexpr std::string_view FRONTEND_MACRO_M24A_DRY_RUN_ADAPTER_BLOCKER =
    "builtin derive controlled parser dry-run adapter remains execution-blocked in M24a";
constexpr base::u64 FRONTEND_MACRO_M24A_DRY_RUN_PREREQUISITE_COUNT = 5U;
constexpr std::string_view FRONTEND_MACRO_M24B_ROLLBACK_REPLAY_MARKER =
    "frontend.macro.m24b.builtin_derive_dry_run_rollback_diagnostic_replay.v1";
constexpr std::string_view FRONTEND_MACRO_M24B_ROLLBACK_REPLAY_IDENTITY_MARKER =
    "frontend.macro.m24b.builtin_derive_dry_run_rollback_diagnostic_replay_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M24B_ROLLBACK_REPLAY_POLICY =
    "builtin_derive_dry_run_rollback_diagnostic_replay_v1";
constexpr std::string_view FRONTEND_MACRO_M24B_ROLLBACK_REPLAY_QUERY_NAME_PREFIX =
    "m24b-builtin-derive-dry-run-rollback-replay:";
constexpr std::string_view FRONTEND_MACRO_M24B_ROLLBACK_REPLAY_BLOCKER =
    "builtin derive dry-run rollback diagnostic replay remains execution-blocked in M24b";
constexpr std::string_view FRONTEND_MACRO_M24C_NEGATIVE_MATRIX_MARKER =
    "frontend.macro.m24c.builtin_derive_dry_run_negative_matrix_closure.v1";
constexpr std::string_view FRONTEND_MACRO_M24C_NEGATIVE_MATRIX_IDENTITY_MARKER =
    "frontend.macro.m24c.builtin_derive_dry_run_negative_matrix_closure_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M24C_NEGATIVE_MATRIX_POLICY =
    "builtin_derive_dry_run_negative_matrix_closure_v1";
constexpr std::string_view FRONTEND_MACRO_M24C_NEGATIVE_MATRIX_QUERY_NAME_PREFIX =
    "m24c-builtin-derive-dry-run-negative-matrix:";
constexpr std::string_view FRONTEND_MACRO_M24C_NEGATIVE_MATRIX_BLOCKER =
    "builtin derive dry-run negative matrix keeps parser consumption blocked in M24c";
constexpr base::u64 FRONTEND_MACRO_M24C_NEGATIVE_CASE_COUNT = 8U;
constexpr std::string_view FRONTEND_MACRO_M25A_DRY_RUN_SESSION_MARKER =
    "frontend.macro.m25a.builtin_derive_parser_dry_run_session_boundary.v1";
constexpr std::string_view FRONTEND_MACRO_M25A_DRY_RUN_SESSION_IDENTITY_MARKER =
    "frontend.macro.m25a.builtin_derive_parser_dry_run_session_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M25A_DRY_RUN_SESSION_POLICY =
    "builtin_derive_parser_dry_run_session_boundary_v1";
constexpr std::string_view FRONTEND_MACRO_M25A_DRY_RUN_SESSION_QUERY_NAME_PREFIX =
    "m25a-builtin-derive-dry-run-session:";
constexpr std::string_view FRONTEND_MACRO_M25A_DRY_RUN_SESSION_BLOCKER =
    "builtin derive parser dry-run session remains check-only and uncommitted in M25a";
constexpr base::u64 FRONTEND_MACRO_M25A_PARSER_STATE_SNAPSHOT_COUNT = 1U;
constexpr std::string_view FRONTEND_MACRO_M25B_CURSOR_SNAPSHOT_MARKER =
    "frontend.macro.m25b.builtin_derive_token_cursor_snapshot_rollback_proof.v1";
constexpr std::string_view FRONTEND_MACRO_M25B_CURSOR_SNAPSHOT_IDENTITY_MARKER =
    "frontend.macro.m25b.builtin_derive_token_cursor_snapshot_rollback_proof_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M25B_CURSOR_SNAPSHOT_POLICY =
    "builtin_derive_token_cursor_snapshot_rollback_proof_v1";
constexpr std::string_view FRONTEND_MACRO_M25B_CURSOR_SNAPSHOT_QUERY_NAME_PREFIX =
    "m25b-builtin-derive-token-cursor-rollback-proof:";
constexpr std::string_view FRONTEND_MACRO_M25B_CURSOR_SNAPSHOT_BLOCKER =
    "builtin derive token cursor snapshot rollback proof keeps parser cursor unadvanced in M25b";
constexpr std::string_view FRONTEND_MACRO_M25C_DIAGNOSTIC_SHADOW_MARKER =
    "frontend.macro.m25c.builtin_derive_diagnostic_shadow_no_ast_mutation_closure.v1";
constexpr std::string_view FRONTEND_MACRO_M25C_DIAGNOSTIC_SHADOW_IDENTITY_MARKER =
    "frontend.macro.m25c.builtin_derive_diagnostic_shadow_no_ast_mutation_closure_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M25C_DIAGNOSTIC_SHADOW_POLICY =
    "builtin_derive_diagnostic_shadow_no_ast_mutation_closure_v1";
constexpr std::string_view FRONTEND_MACRO_M25C_DIAGNOSTIC_SHADOW_QUERY_NAME_PREFIX =
    "m25c-builtin-derive-diagnostic-shadow-no-ast-mutation:";
constexpr std::string_view FRONTEND_MACRO_M25C_DIAGNOSTIC_SHADOW_BLOCKER =
    "builtin derive diagnostic shadow replay remains non-executing and no-AST-mutation in M25c";
constexpr std::string_view FRONTEND_MACRO_M26A_DRY_RUN_ADMISSION_GATE_MARKER =
    "frontend.macro.m26a.builtin_derive_parser_dry_run_admission_gate.v1";
constexpr std::string_view FRONTEND_MACRO_M26A_DRY_RUN_ADMISSION_GATE_IDENTITY_MARKER =
    "frontend.macro.m26a.builtin_derive_parser_dry_run_admission_gate_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M26A_DRY_RUN_ADMISSION_GATE_POLICY =
    "builtin_derive_parser_dry_run_admission_gate_v1";
constexpr std::string_view FRONTEND_MACRO_M26A_DRY_RUN_ADMISSION_GATE_QUERY_NAME_PREFIX =
    "m26a-builtin-derive-parser-dry-run-admission:";
constexpr std::string_view FRONTEND_MACRO_M26A_DRY_RUN_ADMISSION_GATE_BLOCKER =
    "builtin derive parser dry-run execution admission remains blocked in M26a";
constexpr base::u64 FRONTEND_MACRO_M26A_DRY_RUN_ADMISSION_PREREQUISITE_COUNT = 5U;
constexpr std::string_view FRONTEND_MACRO_M26B_RECOVERY_SHADOW_GATE_MARKER =
    "frontend.macro.m26b.builtin_derive_error_recovery_shadow_diagnostic_gate.v1";
constexpr std::string_view FRONTEND_MACRO_M26B_RECOVERY_SHADOW_GATE_IDENTITY_MARKER =
    "frontend.macro.m26b.builtin_derive_error_recovery_shadow_diagnostic_gate_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M26B_RECOVERY_SHADOW_GATE_POLICY =
    "builtin_derive_error_recovery_shadow_diagnostic_gate_v1";
constexpr std::string_view FRONTEND_MACRO_M26B_RECOVERY_SHADOW_GATE_QUERY_NAME_PREFIX =
    "m26b-builtin-derive-error-recovery-shadow-diagnostic:";
constexpr std::string_view FRONTEND_MACRO_M26B_RECOVERY_SHADOW_GATE_BLOCKER =
    "builtin derive error recovery shadow diagnostics remain non-emitting in M26b";
constexpr std::string_view FRONTEND_MACRO_M26C_ROLLBACK_AST_VERIFIER_MARKER =
    "frontend.macro.m26c.builtin_derive_cursor_rollback_ast_mutation_verifier_closure.v1";
constexpr std::string_view FRONTEND_MACRO_M26C_ROLLBACK_AST_VERIFIER_IDENTITY_MARKER =
    "frontend.macro.m26c.builtin_derive_cursor_rollback_ast_mutation_verifier_closure_identity.v1";
constexpr std::string_view FRONTEND_MACRO_M26C_ROLLBACK_AST_VERIFIER_POLICY =
    "builtin_derive_cursor_rollback_ast_mutation_verifier_closure_v1";
constexpr std::string_view FRONTEND_MACRO_M26C_ROLLBACK_AST_VERIFIER_QUERY_NAME_PREFIX =
    "m26c-builtin-derive-cursor-rollback-ast-verifier:";
constexpr std::string_view FRONTEND_MACRO_M26C_ROLLBACK_AST_VERIFIER_BLOCKER =
    "builtin derive cursor rollback execution and AST mutation verifier remain check-only in M26c";
constexpr base::u64 FRONTEND_MACRO_M26C_AST_BASELINE_SNAPSHOT_COUNT = 1U;
constexpr std::string_view FRONTEND_MACRO_M22_TARGET_KIND_STRUCT = "struct";
constexpr std::string_view FRONTEND_MACRO_M22_TARGET_KIND_ENUM = "enum";
constexpr std::string_view FRONTEND_MACRO_M22_TARGET_KIND_OTHER = "other";
constexpr std::string_view FRONTEND_MACRO_M22_CAPABILITY_COPY = "Copy";
constexpr std::string_view FRONTEND_MACRO_M22_CAPABILITY_EQ = "Eq";
constexpr std::string_view FRONTEND_MACRO_M22_CAPABILITY_HASH = "Hash";
constexpr std::string_view FRONTEND_MACRO_M21I_DERIVE_BEGIN_TOKEN_TEXT =
    "__aurex_builtin_derive_begin";
constexpr std::string_view FRONTEND_MACRO_M21I_DERIVE_END_TOKEN_TEXT =
    "__aurex_builtin_derive_end";
constexpr std::string_view FRONTEND_MACRO_M21I_DERIVE_BEGIN_TOKEN_ROLE = "derive_codegen_begin";
constexpr std::string_view FRONTEND_MACRO_M21I_DERIVE_SOURCE_TOKEN_ROLE = "derive_source_token_placeholder";
constexpr std::string_view FRONTEND_MACRO_M21I_DERIVE_END_TOKEN_ROLE = "derive_codegen_end";
constexpr base::u64 FRONTEND_MACRO_M21I_DERIVE_SENTINEL_TOKEN_COUNT = 2U;
constexpr std::string_view FRONTEND_MACRO_M21I_DERIVE_SOURCE_TOKEN_PREFIX =
    "__aurex_builtin_derive_source_token_";
constexpr std::string_view FRONTEND_MACRO_M21I_GENERATED_TOKEN_RESERVE_CONTEXT =
    "compiler-owned generated token reserve";
constexpr base::u64 FRONTEND_MACRO_M21I_MAX_GENERATED_TOKEN_INDEX =
    static_cast<base::u64>(std::numeric_limits<base::u32>::max());

[[nodiscard]] base::Error internal_error(const std::string_view message)
{
    return base::Error{base::ErrorCode::internal_error, std::string(message)};
}

[[nodiscard]] bool source_range_is_well_formed(const base::SourceRange& range) noexcept
{
    return range.well_formed();
}

[[nodiscard]] bool source_ranges_equal(
    const base::SourceRange& lhs, const base::SourceRange& rhs) noexcept
{
    return lhs.source.value == rhs.source.value && lhs.begin == rhs.begin && lhs.end == rhs.end;
}

[[nodiscard]] base::SourceRange merged_range(
    const base::SourceRange& begin,
    const base::SourceRange& end) noexcept
{
    if (begin.source.value != end.source.value) {
        return begin;
    }
    return base::SourceRange{begin.source, begin.begin, end.end};
}

[[nodiscard]] bool is_nonzero_fingerprint(const query::StableFingerprint128 fingerprint) noexcept
{
    return fingerprint != query::StableFingerprint128{};
}

[[nodiscard]] bool item_id_in_range(const syntax::AstModule& ast, const syntax::ItemId item) noexcept
{
    return syntax::is_valid(item) && item.value < ast.items.size();
}

[[nodiscard]] bool module_id_in_range(const syntax::AstModule& ast, const syntax::ModuleId module) noexcept
{
    return syntax::is_valid(module) && module.value < ast.modules.size();
}

[[nodiscard]] base::usize count_item_attributes(const syntax::AstModule& ast) noexcept
{
    base::usize count = 0;
    for (base::usize item_index = 0; item_index < ast.items.size(); ++item_index) {
        count += ast.items[item_index].attributes.size();
    }
    return count;
}

[[nodiscard]] std::string module_part_generated_name(
    const syntax::ModuleId module, const base::u32 part_index)
{
    std::string name(FRONTEND_MACRO_M21D_GENERATED_PART_NAME_PREFIX);
    name += std::to_string(module.value);
    name.push_back(':');
    name += std::to_string(part_index);
    return name;
}

[[nodiscard]] std::string module_part_generated_virtual_buffer(
    const syntax::ModuleId module, const base::u32 part_index)
{
    std::string buffer(FRONTEND_MACRO_M21E_GENERATED_BUFFER_PREFIX);
    buffer += std::to_string(module.value);
    buffer.push_back(':');
    buffer += std::to_string(part_index);
    return buffer;
}

[[nodiscard]] std::string generated_item_name_for_input(const EarlyItemMacroInput& input)
{
    std::string name(FRONTEND_MACRO_M21G_GENERATED_NAME_PREFIX);
    name += std::to_string(input.module.value);
    name.push_back(':');
    name += std::to_string(input.part_index);
    name.push_back(':');
    name += std::to_string(input.item.value);
    name.push_back(':');
    name += std::to_string(input.attribute_index);
    name.push_back(':');
    name += input.attribute_name;
    return name;
}

[[nodiscard]] std::string token_stream_name_for_input(const EarlyItemMacroInput& input)
{
    std::string name(FRONTEND_MACRO_M21H_TOKEN_STREAM_PREFIX);
    name += std::to_string(input.module.value);
    name.push_back(':');
    name += std::to_string(input.part_index);
    name.push_back(':');
    name += std::to_string(input.item.value);
    name.push_back(':');
    name += std::to_string(input.attribute_index);
    name.push_back(':');
    name += input.attribute_name;
    return name;
}

[[nodiscard]] std::string_view macro_decl_kind_name(const syntax::MacroDeclKind kind) noexcept
{
    switch (kind) {
        case syntax::MacroDeclKind::declarative:
            return "declarative";
        case syntax::MacroDeclKind::derive:
            return "derive";
        case syntax::MacroDeclKind::compile_time:
            return "compile_time";
    }
    return "unknown";
}

[[nodiscard]] std::string_view typed_matcher_kind_name(const AurexMacroTypedMatcherKind kind) noexcept
{
    switch (kind) {
        case AurexMacroTypedMatcherKind::unknown:
            return "unknown";
        case AurexMacroTypedMatcherKind::expr_list:
            return "expr_list";
        case AurexMacroTypedMatcherKind::item:
            return "item";
        case AurexMacroTypedMatcherKind::tokens:
            return "tokens";
    }
    return "unknown";
}

[[nodiscard]] std::string_view macro_surface_blocker_reason(const syntax::MacroDeclKind kind) noexcept
{
    switch (kind) {
        case syntax::MacroDeclKind::declarative:
            return FRONTEND_MACRO_M27A_DECLARATIVE_BLOCKER;
        case syntax::MacroDeclKind::derive:
            return FRONTEND_MACRO_M27B_USER_DERIVE_BLOCKER;
        case syntax::MacroDeclKind::compile_time:
            return FRONTEND_MACRO_M27C_COMPILE_TIME_BLOCKER;
    }
    return FRONTEND_MACRO_M27A_DECLARATIVE_BLOCKER;
}

[[nodiscard]] std::string macro_surface_query_name(
    const syntax::ModuleId module,
    const base::u32 part_index,
    const syntax::ItemId item,
    const std::string_view macro_name)
{
    std::string name(FRONTEND_MACRO_M27A_QUERY_NAME_PREFIX);
    name += std::to_string(module.value);
    name.push_back(':');
    name += std::to_string(part_index);
    name.push_back(':');
    name += std::to_string(item.value);
    name.push_back(':');
    name += macro_name;
    return name;
}

[[nodiscard]] std::string macro_definition_site_hygiene_query_name(
    const syntax::ModuleId module,
    const base::u32 part_index,
    const syntax::ItemId item,
    const std::string_view macro_name)
{
    std::string name(FRONTEND_MACRO_M27B_DEFINITION_SITE_HYGIENE_QUERY_PREFIX);
    name += std::to_string(module.value);
    name.push_back(':');
    name += std::to_string(part_index);
    name.push_back(':');
    name += std::to_string(item.value);
    name.push_back(':');
    name += macro_name;
    return name;
}

[[nodiscard]] std::string macro_typed_matcher_query_name(
    const syntax::ModuleId module,
    const base::u32 part_index,
    const syntax::ItemId item,
    const base::u32 matcher_index,
    const std::string_view macro_name)
{
    std::string name(FRONTEND_MACRO_M27B_TYPED_MATCHER_QUERY_PREFIX);
    name += std::to_string(module.value);
    name.push_back(':');
    name += std::to_string(part_index);
    name.push_back(':');
    name += std::to_string(item.value);
    name.push_back(':');
    name += std::to_string(matcher_index);
    name.push_back(':');
    name += macro_name;
    return name;
}

[[nodiscard]] query::StableFingerprint128 generated_buffer_identity(
    const GeneratedModulePartPlaceholder& placeholder, const std::string_view generated_buffer_name) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21E_GENERATED_BUFFER_IDENTITY_MARKER);
    builder.mix_u32(placeholder.module.value);
    builder.mix_u32(placeholder.source_part_index);
    builder.mix_u32(placeholder.generated_stable_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.source_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    builder.mix_string(generated_buffer_name);
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 parse_config_fingerprint(
    const GeneratedModulePartPlaceholder& placeholder) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21E_PARSE_CONFIG_MARKER);
    builder.mix_fingerprint(query::stable_key_fingerprint(query::parser_config_key()));
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 merge_ordering_key(
    const GeneratedModulePartPlaceholder& placeholder) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21E_MERGE_ORDERING_MARKER);
    builder.mix_u32(placeholder.module.value);
    builder.mix_u32(placeholder.source_part_index);
    builder.mix_u32(placeholder.generated_stable_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.source_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    return builder.finish();
}

void mix_macro_input_identity(query::StableHashBuilder& builder, const EarlyItemMacroInput& input) noexcept
{
    builder.mix_u32(input.item.value);
    builder.mix_u32(input.module.value);
    builder.mix_u32(input.part_index);
    builder.mix_u32(input.attribute_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(input.attached_part));
    builder.mix_fingerprint(input.query_key_fingerprint);
}

[[nodiscard]] query::StableFingerprint128 hygiene_mark_fingerprint(
    const std::string_view marker, const EarlyItemMacroInput& input) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(marker);
    mix_macro_input_identity(builder, input);
    builder.mix_fingerprint(input.token_tree_fingerprint);
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 generated_item_stub_fingerprint(
    const std::string_view marker,
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const query::StableFingerprint128 declared_name_set,
    const std::string_view generated_item_name) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(marker);
    mix_macro_input_identity(builder, input);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    builder.mix_fingerprint(placeholder.output_fingerprint);
    builder.mix_fingerprint(declared_name_set);
    builder.mix_string(generated_item_name);
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 token_plan_identity(
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const ExpansionHygieneStub& hygiene,
    const ExpansionTraceStub& trace,
    const GeneratedItemDeclarationStub& declaration,
    const DeclaredGeneratedNameStub& declared_name,
    const std::string_view token_stream_name) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21H_TOKEN_PLAN_IDENTITY_MARKER);
    mix_macro_input_identity(builder, input);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    builder.mix_fingerprint(placeholder.output_fingerprint);
    builder.mix_fingerprint(declaration.declaration_identity);
    builder.mix_fingerprint(declaration.generated_item_key);
    builder.mix_fingerprint(hygiene.declared_name_set);
    builder.mix_fingerprint(declared_name.declared_name_identity);
    builder.mix_fingerprint(declared_name.hygiene_mark);
    builder.mix_fingerprint(trace.trace_identity);
    builder.mix_fingerprint(trace.generated_source_map_identity);
    builder.mix_string(token_stream_name);
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 token_buffer_identity(
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const ExpansionHygieneStub& hygiene,
    const ExpansionTraceStub& trace,
    const query::StableFingerprint128 token_plan,
    const std::string_view token_buffer_kind,
    const std::string_view token_stream_name) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21H_TOKEN_BUFFER_IDENTITY_MARKER);
    mix_macro_input_identity(builder, input);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    builder.mix_fingerprint(placeholder.output_fingerprint);
    builder.mix_fingerprint(token_plan);
    builder.mix_fingerprint(hygiene.generated_fresh_mark);
    builder.mix_fingerprint(trace.generated_source_map_identity);
    builder.mix_string(token_buffer_kind);
    builder.mix_string(token_stream_name);
    return builder.finish();
}

[[nodiscard]] bool compiler_owned_token_prototype_enabled(const EarlyItemMacroInput& input) noexcept
{
    return input.disposition == EarlyItemExpansionDisposition::builtin_derive_passthrough;
}

[[nodiscard]] std::string_view token_buffer_kind_for_input(const EarlyItemMacroInput& input) noexcept
{
    return compiler_owned_token_prototype_enabled(input) ? FRONTEND_MACRO_M21I_DERIVE_TOKEN_BUFFER_KIND
                                                        : FRONTEND_MACRO_M21H_TOKEN_BUFFER_KIND;
}

[[nodiscard]] std::string_view token_producer_policy_for_input(const EarlyItemMacroInput& input) noexcept
{
    return compiler_owned_token_prototype_enabled(input) ? FRONTEND_MACRO_M21I_DERIVE_TOKEN_PRODUCER_POLICY
                                                        : FRONTEND_MACRO_M21I_EMPTY_TOKEN_PRODUCER_POLICY;
}

[[nodiscard]] std::string_view token_admission_blocker_for_input(const EarlyItemMacroInput& input) noexcept
{
    return compiler_owned_token_prototype_enabled(input) ? FRONTEND_MACRO_M21I_DERIVE_ADMISSION_BLOCKER
                                                        : FRONTEND_MACRO_M21I_EMPTY_ADMISSION_BLOCKER;
}

[[nodiscard]] std::string_view token_buffer_blocker_for_input(const EarlyItemMacroInput& input) noexcept
{
    return compiler_owned_token_prototype_enabled(input) ? FRONTEND_MACRO_M21I_DERIVE_TOKEN_BUFFER_BLOCKER
                                                        : FRONTEND_MACRO_M21I_EMPTY_TOKEN_BUFFER_BLOCKER;
}

[[nodiscard]] std::string_view parser_admission_blocker_for_input(const EarlyItemMacroInput& input) noexcept
{
    return compiler_owned_token_prototype_enabled(input) ? FRONTEND_MACRO_M21J_DERIVE_PARSE_BLOCKER
                                                        : FRONTEND_MACRO_M21J_EMPTY_PARSE_BLOCKER;
}

[[nodiscard]] std::string_view parser_admission_diagnostic_category_for_input(
    const EarlyItemMacroInput& input) noexcept
{
    return compiler_owned_token_prototype_enabled(input) ? FRONTEND_MACRO_M21K_DERIVE_BLOCKER_CATEGORY
                                                        : FRONTEND_MACRO_M21K_EMPTY_BLOCKER_CATEGORY;
}

[[nodiscard]] std::string_view parser_admission_diagnostic_message_for_input(
    const EarlyItemMacroInput& input) noexcept
{
    return compiler_owned_token_prototype_enabled(input) ? FRONTEND_MACRO_M21K_DERIVE_USER_MESSAGE
                                                        : FRONTEND_MACRO_M21K_EMPTY_USER_MESSAGE;
}

[[nodiscard]] std::string parser_admission_debug_projection_name(const EarlyItemMacroInput& input)
{
    std::string name(FRONTEND_MACRO_M21K_DEBUG_PROJECTION_PREFIX);
    name += std::to_string(input.module.value);
    name.push_back(':');
    name += std::to_string(input.part_index);
    name.push_back(':');
    name += std::to_string(input.item.value);
    name.push_back(':');
    name += std::to_string(input.attribute_index);
    name.push_back(':');
    name += input.attribute_name;
    return name;
}

[[nodiscard]] std::string parser_admission_report_query_name(
    const syntax::ModuleId module, const base::u32 source_part_index)
{
    std::string name(FRONTEND_MACRO_M21L_QUERY_NAME_PREFIX);
    name += std::to_string(module.value);
    name.push_back(':');
    name += std::to_string(source_part_index);
    return name;
}

[[nodiscard]] std::string parser_consumption_contract_query_name(
    const syntax::ModuleId module, const base::u32 source_part_index)
{
    std::string name(FRONTEND_MACRO_M21N_CONTRACT_QUERY_NAME_PREFIX);
    name += std::to_string(module.value);
    name.push_back(':');
    name += std::to_string(source_part_index);
    return name;
}

[[nodiscard]] std::string builtin_derive_admission_query_name(const EarlyItemMacroInput& input)
{
    std::string name(FRONTEND_MACRO_M22A_QUERY_NAME_PREFIX);
    name += std::to_string(input.module.value);
    name.push_back(':');
    name += std::to_string(input.part_index);
    name.push_back(':');
    name += std::to_string(input.item.value);
    name.push_back(':');
    name += std::to_string(input.attribute_index);
    name.push_back(':');
    name += input.attribute_name;
    return name;
}

[[nodiscard]] std::string builtin_derive_parser_release_query_name(
    const syntax::ModuleId module, const base::u32 source_part_index)
{
    std::string name(FRONTEND_MACRO_M22C_RELEASE_QUERY_NAME_PREFIX);
    name += std::to_string(module.value);
    name.push_back(':');
    name += std::to_string(source_part_index);
    return name;
}

[[nodiscard]] std::string builtin_derive_release_hardening_query_name(
    const syntax::ModuleId module, const base::u32 source_part_index)
{
    std::string name(FRONTEND_MACRO_M22D_HARDENING_QUERY_NAME_PREFIX);
    name += std::to_string(module.value);
    name.push_back(':');
    name += std::to_string(source_part_index);
    return name;
}

[[nodiscard]] std::string builtin_derive_debug_dump_query_name(
    const syntax::ModuleId module, const base::u32 source_part_index)
{
    std::string name(FRONTEND_MACRO_M22E_DEBUG_DUMP_QUERY_NAME_PREFIX);
    name += std::to_string(module.value);
    name.push_back(':');
    name += std::to_string(source_part_index);
    return name;
}

[[nodiscard]] std::string builtin_derive_rollback_diagnostic_query_name(
    const syntax::ModuleId module, const base::u32 source_part_index)
{
    std::string name(FRONTEND_MACRO_M22F_ROLLBACK_QUERY_NAME_PREFIX);
    name += std::to_string(module.value);
    name.push_back(':');
    name += std::to_string(source_part_index);
    return name;
}

[[nodiscard]] std::string builtin_derive_parser_consumption_admission_query_name(
    const syntax::ModuleId module, const base::u32 source_part_index)
{
    std::string name(FRONTEND_MACRO_M23A_ADMISSION_QUERY_NAME_PREFIX);
    name += std::to_string(module.value);
    name.push_back(':');
    name += std::to_string(source_part_index);
    return name;
}

[[nodiscard]] std::string builtin_derive_checkpoint_rollback_query_name(
    const syntax::ModuleId module, const base::u32 source_part_index)
{
    std::string name(FRONTEND_MACRO_M23B_CHECKPOINT_QUERY_NAME_PREFIX);
    name += std::to_string(module.value);
    name.push_back(':');
    name += std::to_string(source_part_index);
    return name;
}

[[nodiscard]] std::string builtin_derive_preconsumption_verification_query_name(
    const syntax::ModuleId module, const base::u32 source_part_index)
{
    std::string name(FRONTEND_MACRO_M23C_VERIFICATION_QUERY_NAME_PREFIX);
    name += std::to_string(module.value);
    name.push_back(':');
    name += std::to_string(source_part_index);
    return name;
}

[[nodiscard]] std::string builtin_derive_controlled_dry_run_adapter_query_name(
    const syntax::ModuleId module, const base::u32 source_part_index)
{
    std::string name(FRONTEND_MACRO_M24A_DRY_RUN_ADAPTER_QUERY_NAME_PREFIX);
    name += std::to_string(module.value);
    name.push_back(':');
    name += std::to_string(source_part_index);
    return name;
}

[[nodiscard]] std::string builtin_derive_dry_run_rollback_replay_query_name(
    const syntax::ModuleId module, const base::u32 source_part_index)
{
    std::string name(FRONTEND_MACRO_M24B_ROLLBACK_REPLAY_QUERY_NAME_PREFIX);
    name += std::to_string(module.value);
    name.push_back(':');
    name += std::to_string(source_part_index);
    return name;
}

[[nodiscard]] std::string builtin_derive_dry_run_negative_matrix_query_name(
    const syntax::ModuleId module, const base::u32 source_part_index)
{
    std::string name(FRONTEND_MACRO_M24C_NEGATIVE_MATRIX_QUERY_NAME_PREFIX);
    name += std::to_string(module.value);
    name.push_back(':');
    name += std::to_string(source_part_index);
    return name;
}

[[nodiscard]] std::string builtin_derive_parser_dry_run_session_query_name(
    const syntax::ModuleId module, const base::u32 source_part_index)
{
    std::string name(FRONTEND_MACRO_M25A_DRY_RUN_SESSION_QUERY_NAME_PREFIX);
    name += std::to_string(module.value);
    name.push_back(':');
    name += std::to_string(source_part_index);
    return name;
}

[[nodiscard]] std::string builtin_derive_token_cursor_snapshot_query_name(
    const syntax::ModuleId module, const base::u32 source_part_index)
{
    std::string name(FRONTEND_MACRO_M25B_CURSOR_SNAPSHOT_QUERY_NAME_PREFIX);
    name += std::to_string(module.value);
    name.push_back(':');
    name += std::to_string(source_part_index);
    return name;
}

[[nodiscard]] std::string builtin_derive_diagnostic_shadow_query_name(
    const syntax::ModuleId module, const base::u32 source_part_index)
{
    std::string name(FRONTEND_MACRO_M25C_DIAGNOSTIC_SHADOW_QUERY_NAME_PREFIX);
    name += std::to_string(module.value);
    name.push_back(':');
    name += std::to_string(source_part_index);
    return name;
}

[[nodiscard]] std::string builtin_derive_dry_run_admission_gate_query_name(
    const syntax::ModuleId module, const base::u32 source_part_index)
{
    std::string name(FRONTEND_MACRO_M26A_DRY_RUN_ADMISSION_GATE_QUERY_NAME_PREFIX);
    name += std::to_string(module.value);
    name.push_back(':');
    name += std::to_string(source_part_index);
    return name;
}

[[nodiscard]] std::string builtin_derive_error_recovery_shadow_diagnostic_query_name(
    const syntax::ModuleId module, const base::u32 source_part_index)
{
    std::string name(FRONTEND_MACRO_M26B_RECOVERY_SHADOW_GATE_QUERY_NAME_PREFIX);
    name += std::to_string(module.value);
    name.push_back(':');
    name += std::to_string(source_part_index);
    return name;
}

[[nodiscard]] std::string builtin_derive_cursor_rollback_ast_verifier_query_name(
    const syntax::ModuleId module, const base::u32 source_part_index)
{
    std::string name(FRONTEND_MACRO_M26C_ROLLBACK_AST_VERIFIER_QUERY_NAME_PREFIX);
    name += std::to_string(module.value);
    name.push_back(':');
    name += std::to_string(source_part_index);
    return name;
}

[[nodiscard]] bool builtin_derive_input(const EarlyItemMacroInput& input) noexcept
{
    return input.disposition == EarlyItemExpansionDisposition::builtin_derive_passthrough;
}

[[nodiscard]] std::string_view builtin_derive_admission_kind_for_input(
    const EarlyItemMacroInput& input) noexcept
{
    return builtin_derive_input(input) ? FRONTEND_MACRO_M22A_DERIVE_ADMISSION_KIND
                                       : FRONTEND_MACRO_M22A_NON_DERIVE_BLOCKED_KIND;
}

[[nodiscard]] std::string_view builtin_derive_admission_blocker_for_input(
    const EarlyItemMacroInput& input) noexcept
{
    return builtin_derive_input(input) ? FRONTEND_MACRO_M22A_DERIVE_BLOCKER
                                       : FRONTEND_MACRO_M22A_NON_DERIVE_BLOCKER;
}

[[nodiscard]] bool builtin_derive_candidate_name_supported(const std::string_view name) noexcept
{
    return name == FRONTEND_MACRO_M22_CAPABILITY_COPY
        || name == FRONTEND_MACRO_M22_CAPABILITY_EQ
        || name == FRONTEND_MACRO_M22_CAPABILITY_HASH;
}

[[nodiscard]] bool attribute_token_is_derive_capability_candidate(
    const syntax::AttributeTokenDecl& token) noexcept
{
    return token.kind == syntax::TokenKind::identifier
        && token.depth == 1U
        && token.group == syntax::AttributeTokenTreeGroupKind::none;
}

[[nodiscard]] base::u64 count_builtin_derive_capability_candidates(
    const syntax::AttributeDecl& attribute) noexcept
{
    base::u64 count = 0;
    for (const syntax::AttributeTokenDecl& token : attribute.token_tree) {
        if (attribute_token_is_derive_capability_candidate(token)) {
            ++count;
        }
    }
    return count;
}

[[nodiscard]] base::u64 count_builtin_derive_unsupported_candidates(
    const syntax::AttributeDecl& attribute) noexcept
{
    base::u64 count = 0;
    for (const syntax::AttributeTokenDecl& token : attribute.token_tree) {
        if (attribute_token_is_derive_capability_candidate(token)
            && !builtin_derive_candidate_name_supported(token.text)) {
            ++count;
        }
    }
    return count;
}

[[nodiscard]] bool contains_string_view(
    const std::vector<std::string_view>& values, const std::string_view value) noexcept
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

[[nodiscard]] base::u64 count_builtin_derive_duplicate_candidates(
    const syntax::AttributeDecl& attribute)
{
    std::vector<std::string_view> seen;
    seen.reserve(attribute.token_tree.size());
    base::u64 duplicates = 0;
    for (const syntax::AttributeTokenDecl& token : attribute.token_tree) {
        if (!attribute_token_is_derive_capability_candidate(token)
            || !builtin_derive_candidate_name_supported(token.text)) {
            continue;
        }
        if (contains_string_view(seen, token.text)) {
            ++duplicates;
            continue;
        }
        seen.push_back(token.text);
    }
    return duplicates;
}

[[nodiscard]] std::string_view builtin_derive_target_kind(const syntax::ItemNode& item) noexcept
{
    if (item.kind == syntax::ItemKind::struct_decl) {
        return FRONTEND_MACRO_M22_TARGET_KIND_STRUCT;
    }
    if (item.kind == syntax::ItemKind::enum_decl) {
        return FRONTEND_MACRO_M22_TARGET_KIND_ENUM;
    }
    return FRONTEND_MACRO_M22_TARGET_KIND_OTHER;
}

[[nodiscard]] bool builtin_derive_target_is_struct_or_enum(const syntax::ItemNode& item) noexcept
{
    return item.kind == syntax::ItemKind::struct_decl || item.kind == syntax::ItemKind::enum_decl;
}

[[nodiscard]] base::u64 count_derive_capability(
    const syntax::ItemNode& item, const std::string_view capability) noexcept
{
    base::u64 count = 0;
    for (const syntax::DeriveDecl& derive : item.derives) {
        if (derive.name == capability) {
            ++count;
        }
    }
    return count;
}

[[nodiscard]] bool token_buffer_kind_is_compiler_owned(const std::string_view token_buffer_kind) noexcept
{
    return token_buffer_kind == FRONTEND_MACRO_M21I_DERIVE_TOKEN_BUFFER_KIND
        || token_buffer_kind == FRONTEND_MACRO_M21H_TOKEN_BUFFER_KIND;
}

[[nodiscard]] bool token_producer_policy_is_compiler_owned(const std::string_view token_producer_policy) noexcept
{
    return token_producer_policy == FRONTEND_MACRO_M21I_DERIVE_TOKEN_PRODUCER_POLICY
        || token_producer_policy == FRONTEND_MACRO_M21I_EMPTY_TOKEN_PRODUCER_POLICY;
}

[[nodiscard]] bool token_materialization_admission_state_is_valid(
    const TokenMaterializationAdmissionStub& stub) noexcept
{
    if (stub.blocker_reason == FRONTEND_MACRO_M21I_DERIVE_ADMISSION_BLOCKER) {
        return stub.materialized_tokens;
    }
    if (stub.blocker_reason == FRONTEND_MACRO_M21I_EMPTY_ADMISSION_BLOCKER) {
        return !stub.materialized_tokens;
    }
    return false;
}

[[nodiscard]] bool generated_token_buffer_state_is_valid(const GeneratedTokenBufferStub& stub) noexcept
{
    if (stub.token_buffer_kind == FRONTEND_MACRO_M21I_DERIVE_TOKEN_BUFFER_KIND) {
        return stub.token_producer_policy == FRONTEND_MACRO_M21I_DERIVE_TOKEN_PRODUCER_POLICY
            && stub.blocker_reason == FRONTEND_MACRO_M21I_DERIVE_TOKEN_BUFFER_BLOCKER
            && stub.token_count >= FRONTEND_MACRO_M21I_DERIVE_SENTINEL_TOKEN_COUNT
            && !stub.empty
            && stub.materialized_tokens;
    }
    if (stub.token_buffer_kind == FRONTEND_MACRO_M21H_TOKEN_BUFFER_KIND) {
        return stub.token_producer_policy == FRONTEND_MACRO_M21I_EMPTY_TOKEN_PRODUCER_POLICY
            && stub.blocker_reason == FRONTEND_MACRO_M21I_EMPTY_TOKEN_BUFFER_BLOCKER
            && stub.token_count == 0
            && stub.empty
            && !stub.materialized_tokens;
    }
    return false;
}

[[nodiscard]] bool parser_admission_diagnostic_category_is_valid(
    const ParserAdmissionDiagnosticProjectionStub& stub) noexcept
{
    if (stub.blocker_category == FRONTEND_MACRO_M21K_DERIVE_BLOCKER_CATEGORY) {
        return stub.token_buffer_blocker == FRONTEND_MACRO_M21J_DERIVE_PARSE_BLOCKER
            && stub.user_message == FRONTEND_MACRO_M21K_DERIVE_USER_MESSAGE
            && stub.token_count > 0
            && stub.token_buffer_materialized
            && stub.token_records_available;
    }
    if (stub.blocker_category == FRONTEND_MACRO_M21K_EMPTY_BLOCKER_CATEGORY) {
        return stub.token_buffer_blocker == FRONTEND_MACRO_M21J_EMPTY_PARSE_BLOCKER
            && stub.user_message == FRONTEND_MACRO_M21K_EMPTY_USER_MESSAGE
            && stub.token_count == 0
            && !stub.token_buffer_materialized
            && !stub.token_records_available;
    }
    return false;
}

[[nodiscard]] bool parser_admission_report_entry_category_is_valid(
    const ParserAdmissionDiagnosticReportEntry& entry) noexcept
{
    if (entry.blocker_category == FRONTEND_MACRO_M21K_DERIVE_BLOCKER_CATEGORY) {
        return entry.token_count > 0 && entry.token_records_available;
    }
    if (entry.blocker_category == FRONTEND_MACRO_M21K_EMPTY_BLOCKER_CATEGORY) {
        return entry.token_count == 0 && !entry.token_records_available;
    }
    return false;
}

[[nodiscard]] std::string_view parser_readiness_token_stream_shape_for_category(
    const std::string_view blocker_category) noexcept
{
    return blocker_category == FRONTEND_MACRO_M21K_DERIVE_BLOCKER_CATEGORY
        ? FRONTEND_MACRO_M21M_DERIVE_TOKEN_STREAM_SHAPE
        : FRONTEND_MACRO_M21M_EMPTY_TOKEN_STREAM_SHAPE;
}

[[nodiscard]] bool parser_readiness_preflight_category_is_valid(
    const GeneratedTokenParserReadinessPreflightEntry& entry) noexcept
{
    if (entry.token_stream_shape == FRONTEND_MACRO_M21M_DERIVE_TOKEN_STREAM_SHAPE) {
        return entry.token_count > 0 && entry.token_records_available;
    }
    if (entry.token_stream_shape == FRONTEND_MACRO_M21M_EMPTY_TOKEN_STREAM_SHAPE) {
        return entry.token_count == 0 && !entry.token_records_available;
    }
    return false;
}

[[nodiscard]] bool token_opens_parser_preflight_delimiter(const syntax::TokenKind kind) noexcept
{
    return kind == syntax::TokenKind::l_paren
        || kind == syntax::TokenKind::l_brace
        || kind == syntax::TokenKind::l_bracket;
}

[[nodiscard]] bool token_closes_parser_preflight_delimiter(const syntax::TokenKind kind) noexcept
{
    return kind == syntax::TokenKind::r_paren
        || kind == syntax::TokenKind::r_brace
        || kind == syntax::TokenKind::r_bracket;
}

[[nodiscard]] syntax::TokenKind matching_parser_preflight_delimiter(
    const syntax::TokenKind kind) noexcept
{
    if (kind == syntax::TokenKind::l_paren) {
        return syntax::TokenKind::r_paren;
    }
    if (kind == syntax::TokenKind::l_brace) {
        return syntax::TokenKind::r_brace;
    }
    if (kind == syntax::TokenKind::l_bracket) {
        return syntax::TokenKind::r_bracket;
    }
    return syntax::TokenKind::invalid;
}

[[nodiscard]] bool source_range_contains(
    const base::SourceRange& container, const base::SourceRange& range) noexcept
{
    return source_range_is_well_formed(container)
        && source_range_is_well_formed(range)
        && container.source.value == range.source.value
        && container.begin <= range.begin
        && range.end <= container.end;
}

[[nodiscard]] bool source_anchor_covered_by_input(
    const EarlyItemMacroInput& input, const base::SourceRange& range) noexcept
{
    return source_range_contains(input.attribute_range, range)
        || source_range_contains(input.token_tree_range, range);
}

[[nodiscard]] bool generated_token_record_belongs_to_buffer(
    const GeneratedTokenRecord& record, const GeneratedTokenBufferStub& buffer) noexcept
{
    return record.token_buffer_identity == buffer.token_buffer_identity;
}

[[nodiscard]] bool generated_token_indices_are_contiguous(
    const GeneratedTokenBufferStub& buffer,
    const std::vector<GeneratedTokenRecord>& records) noexcept
{
    base::u64 expected_index = 0;
    for (const GeneratedTokenRecord& record : records) {
        if (!generated_token_record_belongs_to_buffer(record, buffer)) {
            continue;
        }
        if (record.token_index != expected_index) {
            return false;
        }
        ++expected_index;
    }
    return expected_index == buffer.token_count;
}

[[nodiscard]] bool generated_token_delimiters_are_balanced(
    const GeneratedTokenBufferStub& buffer,
    const std::vector<GeneratedTokenRecord>& records)
{
    std::vector<syntax::TokenKind> expected_closing_tokens;
    for (const GeneratedTokenRecord& record : records) {
        if (!generated_token_record_belongs_to_buffer(record, buffer)) {
            continue;
        }
        if (token_opens_parser_preflight_delimiter(record.kind)) {
            expected_closing_tokens.push_back(matching_parser_preflight_delimiter(record.kind));
            continue;
        }
        if (!token_closes_parser_preflight_delimiter(record.kind)) {
            continue;
        }
        if (expected_closing_tokens.empty() || expected_closing_tokens.back() != record.kind) {
            return false;
        }
        expected_closing_tokens.pop_back();
    }
    return expected_closing_tokens.empty();
}

[[nodiscard]] bool generated_token_source_anchors_are_covered(
    const EarlyItemMacroInput& input,
    const GeneratedTokenBufferStub& buffer,
    const std::vector<GeneratedTokenRecord>& records) noexcept
{
    base::u64 covered_records = 0;
    for (const GeneratedTokenRecord& record : records) {
        if (!generated_token_record_belongs_to_buffer(record, buffer)) {
            continue;
        }
        if (!source_anchor_covered_by_input(input, record.anchor_range)) {
            return false;
        }
        ++covered_records;
    }
    return covered_records == buffer.token_count;
}

[[nodiscard]] base::u64 generated_token_count_for_attribute(
    const EarlyItemMacroInput& input) noexcept
{
    if (!compiler_owned_token_prototype_enabled(input)) {
        return 0;
    }
    return input.token_count + FRONTEND_MACRO_M21I_DERIVE_SENTINEL_TOKEN_COUNT;
}

[[nodiscard]] std::string generated_source_token_text(const base::u32 token_index)
{
    std::string token_text(FRONTEND_MACRO_M21I_DERIVE_SOURCE_TOKEN_PREFIX);
    token_text += std::to_string(token_index);
    return token_text;
}

[[nodiscard]] query::StableFingerprint128 generated_token_materialization_identity(
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const TokenMaterializationAdmissionStub& admission,
    const std::string_view token_buffer_kind,
    const std::string_view token_producer_policy,
    const base::u64 token_count) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21I_TOKEN_MATERIALIZATION_IDENTITY_MARKER);
    mix_macro_input_identity(builder, input);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    builder.mix_fingerprint(placeholder.output_fingerprint);
    builder.mix_fingerprint(admission.token_plan_identity);
    builder.mix_fingerprint(admission.token_buffer_identity);
    builder.mix_fingerprint(admission.source_map_identity);
    builder.mix_fingerprint(admission.hygiene_mark);
    builder.mix_string(token_buffer_kind);
    builder.mix_string(token_producer_policy);
    builder.mix_u64(token_count);
    builder.mix_bool(compiler_owned_token_prototype_enabled(input));
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 generated_token_record_identity(
    const EarlyItemMacroInput& input,
    const GeneratedTokenBufferStub& buffer,
    const base::u32 token_index,
    const syntax::TokenKind token_kind,
    const std::string_view token_text,
    const std::string_view token_role,
    const base::SourceRange& anchor_range) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21I_GENERATED_TOKEN_RECORD_MARKER);
    mix_macro_input_identity(builder, input);
    builder.mix_fingerprint(buffer.token_buffer_identity);
    builder.mix_fingerprint(buffer.materialization_identity);
    builder.mix_fingerprint(buffer.source_map_identity);
    builder.mix_fingerprint(buffer.hygiene_mark);
    builder.mix_u32(token_index);
    builder.mix_u8(static_cast<base::u8>(token_kind));
    builder.mix_string(token_text);
    builder.mix_string(token_role);
    builder.mix_u64(static_cast<base::u64>(anchor_range.source.value));
    builder.mix_u64(static_cast<base::u64>(anchor_range.begin));
    builder.mix_u64(static_cast<base::u64>(anchor_range.end));
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 generated_token_parser_admission_gate_identity(
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const GeneratedModulePartParseMergeStub& parse_merge_stub,
    const GeneratedTokenBufferStub& buffer,
    const bool token_records_available) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21J_PARSE_GATE_IDENTITY_MARKER);
    mix_macro_input_identity(builder, input);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    builder.mix_fingerprint(placeholder.output_fingerprint);
    builder.mix_fingerprint(parse_merge_stub.generated_buffer_identity);
    builder.mix_fingerprint(parse_merge_stub.parse_config_fingerprint);
    builder.mix_fingerprint(parse_merge_stub.expansion_origin);
    builder.mix_fingerprint(buffer.token_plan_identity);
    builder.mix_fingerprint(buffer.token_buffer_identity);
    builder.mix_fingerprint(buffer.materialization_identity);
    builder.mix_fingerprint(buffer.source_map_identity);
    builder.mix_fingerprint(buffer.hygiene_mark);
    builder.mix_string(buffer.token_stream_name);
    builder.mix_string(FRONTEND_MACRO_M21J_PARSER_GATE_POLICY);
    builder.mix_string(parser_admission_blocker_for_input(input));
    builder.mix_u64(buffer.token_count);
    builder.mix_bool(buffer.materialized_tokens);
    builder.mix_bool(token_records_available);
    builder.mix_bool(compiler_owned_token_prototype_enabled(input));
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 parser_admission_diagnostic_anchor_identity(
    const EarlyItemMacroInput& input,
    const GeneratedTokenParserAdmissionGateStub& gate,
    const ExpansionTraceStub& trace) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21K_DIAGNOSTIC_ANCHOR_MARKER);
    mix_macro_input_identity(builder, input);
    builder.mix_u64(static_cast<base::u64>(input.attribute_range.source.value));
    builder.mix_u64(static_cast<base::u64>(input.attribute_range.begin));
    builder.mix_u64(static_cast<base::u64>(input.attribute_range.end));
    builder.mix_u64(static_cast<base::u64>(input.token_tree_range.source.value));
    builder.mix_u64(static_cast<base::u64>(input.token_tree_range.begin));
    builder.mix_u64(static_cast<base::u64>(input.token_tree_range.end));
    builder.mix_fingerprint(gate.parse_gate_identity);
    builder.mix_fingerprint(trace.trace_identity);
    builder.mix_fingerprint(trace.diagnostic_anchor);
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 parser_admission_diagnostic_identity(
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const GeneratedModulePartParseMergeStub& parse_merge_stub,
    const GeneratedTokenBufferStub& buffer,
    const GeneratedTokenParserAdmissionGateStub& gate,
    const ExpansionTraceStub& trace,
    const query::StableFingerprint128 diagnostic_anchor,
    const std::string_view debug_projection_name) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21K_DIAGNOSTIC_IDENTITY_MARKER);
    mix_macro_input_identity(builder, input);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    builder.mix_fingerprint(placeholder.output_fingerprint);
    builder.mix_fingerprint(gate.parse_gate_identity);
    builder.mix_fingerprint(diagnostic_anchor);
    builder.mix_fingerprint(buffer.token_plan_identity);
    builder.mix_fingerprint(buffer.token_buffer_identity);
    builder.mix_fingerprint(buffer.materialization_identity);
    builder.mix_fingerprint(parse_merge_stub.generated_buffer_identity);
    builder.mix_fingerprint(parse_merge_stub.parse_config_fingerprint);
    builder.mix_fingerprint(buffer.source_map_identity);
    builder.mix_fingerprint(buffer.hygiene_mark);
    builder.mix_fingerprint(trace.trace_identity);
    builder.mix_fingerprint(trace.generated_source_map_identity);
    builder.mix_string(FRONTEND_MACRO_M21K_DIAGNOSTIC_POLICY);
    builder.mix_string(parser_admission_diagnostic_category_for_input(input));
    builder.mix_string(parser_admission_blocker_for_input(input));
    builder.mix_string(FRONTEND_MACRO_M21K_GENERATED_PART_PARSE_BLOCKER);
    builder.mix_string(parser_admission_diagnostic_message_for_input(input));
    builder.mix_string(debug_projection_name);
    builder.mix_u64(buffer.token_count);
    builder.mix_bool(buffer.materialized_tokens);
    builder.mix_bool(buffer.token_count > 0);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 trace_stub_fingerprint(
    const std::string_view marker, const EarlyItemMacroInput& input) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(marker);
    mix_macro_input_identity(builder, input);
    builder.mix_u64(static_cast<base::u64>(input.attribute_range.source.value));
    builder.mix_u64(static_cast<base::u64>(input.attribute_range.begin));
    builder.mix_u64(static_cast<base::u64>(input.attribute_range.end));
    builder.mix_u64(static_cast<base::u64>(input.token_tree_range.source.value));
    builder.mix_u64(static_cast<base::u64>(input.token_tree_range.begin));
    builder.mix_u64(static_cast<base::u64>(input.token_tree_range.end));
    return builder.finish();
}

[[nodiscard]] GeneratedModulePartParseMergeStub make_parse_merge_stub(
    const GeneratedModulePartPlaceholder& placeholder)
{
    const std::string generated_buffer_name =
        module_part_generated_virtual_buffer(placeholder.module, placeholder.generated_stable_index);
    return GeneratedModulePartParseMergeStub{
        placeholder.module,
        placeholder.source_part_index,
        placeholder.generated_stable_index,
        placeholder.source_part,
        placeholder.generated_part,
        generated_buffer_identity(placeholder, generated_buffer_name),
        parse_config_fingerprint(placeholder),
        merge_ordering_key(placeholder),
        placeholder.output_fingerprint,
        generated_buffer_name,
        std::string(FRONTEND_MACRO_M21E_PARSE_MERGE_BLOCKER),
        GeneratedModulePartLifecycleState::merge_blocked,
        true,
        false,
        false,
        false,
        false,
    };
}

[[nodiscard]] ExpansionHygieneStub make_hygiene_stub(const EarlyItemMacroInput& input)
{
    return ExpansionHygieneStub{
        input.item,
        input.module,
        input.part_index,
        input.attribute_index,
        input.attached_part,
        input.query_key_fingerprint,
        hygiene_mark_fingerprint(FRONTEND_MACRO_M21F_CALL_SITE_MARK_MARKER, input),
        hygiene_mark_fingerprint(FRONTEND_MACRO_M21F_DEFINITION_SITE_MARK_MARKER, input),
        hygiene_mark_fingerprint(FRONTEND_MACRO_M21F_GENERATED_FRESH_MARK_MARKER, input),
        hygiene_mark_fingerprint(FRONTEND_MACRO_M21F_DECLARED_NAME_SET_MARKER, input),
        std::string(FRONTEND_MACRO_M21F_HYGIENE_POLICY),
        false,
        false,
        false,
    };
}

[[nodiscard]] ExpansionTraceStub make_trace_stub(const EarlyItemMacroInput& input)
{
    return ExpansionTraceStub{
        input.item,
        input.module,
        input.part_index,
        input.attribute_index,
        input.attached_part,
        input.attribute_range,
        input.token_tree_range,
        input.query_key_fingerprint,
        trace_stub_fingerprint(FRONTEND_MACRO_M21F_TRACE_IDENTITY_MARKER, input),
        trace_stub_fingerprint(FRONTEND_MACRO_M21F_GENERATED_SOURCE_MAP_MARKER, input),
        trace_stub_fingerprint(FRONTEND_MACRO_M21F_DIAGNOSTIC_ANCHOR_MARKER, input),
        std::string(FRONTEND_MACRO_M21F_TRACE_POLICY),
        std::string(FRONTEND_MACRO_M21F_TRACE_BLOCKER),
        false,
        false,
        false,
    };
}

[[nodiscard]] GeneratedItemDeclarationStub make_generated_item_declaration_stub(
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const ExpansionHygieneStub& hygiene)
{
    const std::string generated_item_name = generated_item_name_for_input(input);
    return GeneratedItemDeclarationStub{
        input.item,
        input.module,
        input.part_index,
        input.attribute_index,
        input.attached_part,
        placeholder.generated_part,
        input.query_key_fingerprint,
        generated_item_stub_fingerprint(FRONTEND_MACRO_M21G_DECLARATION_IDENTITY_MARKER,
            input, placeholder, hygiene.declared_name_set, generated_item_name),
        hygiene.declared_name_set,
        generated_item_stub_fingerprint(FRONTEND_MACRO_M21G_GENERATED_ITEM_KEY_MARKER,
            input, placeholder, hygiene.declared_name_set, generated_item_name),
        std::string(FRONTEND_MACRO_M21G_DECLARATION_ROLE),
        generated_item_name,
        std::string(FRONTEND_MACRO_M21G_DECLARATION_BLOCKER),
        true,
        false,
        false,
        false,
        false,
        false,
    };
}

[[nodiscard]] DeclaredGeneratedNameStub make_declared_generated_name_stub(
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const ExpansionHygieneStub& hygiene,
    const GeneratedItemDeclarationStub& declaration)
{
    return DeclaredGeneratedNameStub{
        input.item,
        input.module,
        input.part_index,
        input.attribute_index,
        input.attached_part,
        placeholder.generated_part,
        input.query_key_fingerprint,
        hygiene.declared_name_set,
        generated_item_stub_fingerprint(FRONTEND_MACRO_M21G_DECLARED_NAME_IDENTITY_MARKER,
            input, placeholder, hygiene.declared_name_set, declaration.generated_item_name),
        hygiene.generated_fresh_mark,
        declaration.generated_item_name,
        std::string(FRONTEND_MACRO_M21G_DECLARED_NAME_NAMESPACE),
        std::string(FRONTEND_MACRO_M21G_DECLARED_NAME_BLOCKER),
        false,
        false,
        false,
        false,
    };
}

[[nodiscard]] TokenMaterializationAdmissionStub make_token_materialization_admission_stub(
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const ExpansionHygieneStub& hygiene,
    const ExpansionTraceStub& trace,
    const GeneratedItemDeclarationStub& declaration,
    const DeclaredGeneratedNameStub& declared_name)
{
    const std::string token_stream_name = token_stream_name_for_input(input);
    const std::string_view token_buffer_kind = token_buffer_kind_for_input(input);
    const query::StableFingerprint128 token_plan = token_plan_identity(
        input, placeholder, hygiene, trace, declaration, declared_name, token_stream_name);
    return TokenMaterializationAdmissionStub{
        input.item,
        input.module,
        input.part_index,
        input.attribute_index,
        input.attached_part,
        placeholder.generated_part,
        input.query_key_fingerprint,
        declaration.declaration_identity,
        declaration.generated_item_key,
        hygiene.declared_name_set,
        declared_name.declared_name_identity,
        declared_name.hygiene_mark,
        trace.generated_source_map_identity,
        trace.trace_identity,
        token_plan,
        token_buffer_identity(input, placeholder, hygiene, trace, token_plan, token_buffer_kind, token_stream_name),
        std::string(FRONTEND_MACRO_M21H_ADMISSION_POLICY),
        token_stream_name,
        std::string(token_admission_blocker_for_input(input)),
        true,
        true,
        compiler_owned_token_prototype_enabled(input),
        false,
        false,
        false,
        false,
        false,
        false,
    };
}

[[nodiscard]] GeneratedTokenBufferStub make_generated_token_buffer_stub(
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const ExpansionHygieneStub& hygiene,
    const ExpansionTraceStub& trace,
    const TokenMaterializationAdmissionStub& admission)
{
    const base::u64 token_count = generated_token_count_for_attribute(input);
    const bool materialized_tokens = token_count > 0;
    const std::string_view token_buffer_kind = token_buffer_kind_for_input(input);
    const std::string_view token_producer_policy = token_producer_policy_for_input(input);
    return GeneratedTokenBufferStub{
        input.item,
        input.module,
        input.part_index,
        input.attribute_index,
        input.attached_part,
        placeholder.generated_part,
        admission.token_plan_identity,
        admission.token_buffer_identity,
        generated_token_materialization_identity(
            input, placeholder, admission, token_buffer_kind, token_producer_policy, token_count),
        trace.generated_source_map_identity,
        hygiene.generated_fresh_mark,
        admission.token_stream_name,
        std::string(token_buffer_kind),
        std::string(token_producer_policy),
        std::string(token_buffer_blocker_for_input(input)),
        token_count,
        !materialized_tokens,
        materialized_tokens,
        false,
        false,
        false,
    };
}

void push_generated_token_record(std::vector<GeneratedTokenRecord>& records,
    const EarlyItemMacroInput& input,
    const GeneratedTokenBufferStub& buffer,
    const base::u32 token_index,
    const syntax::TokenKind token_kind,
    const std::string_view token_text,
    const std::string_view token_role,
    const base::SourceRange anchor_range)
{
    records.push_back(GeneratedTokenRecord{
        input.item,
        input.module,
        input.part_index,
        input.attribute_index,
        token_index,
        buffer.token_buffer_identity,
        generated_token_record_identity(input, buffer, token_index, token_kind, token_text, token_role, anchor_range),
        buffer.source_map_identity,
        buffer.hygiene_mark,
        token_kind,
        std::string(token_text),
        std::string(token_role),
        anchor_range,
        true,
        false,
        false,
    });
}

void append_generated_token_records_for_attribute(std::vector<GeneratedTokenRecord>& records,
    const EarlyItemMacroInput& input,
    const syntax::AttributeDecl& attribute,
    const GeneratedTokenBufferStub& buffer)
{
    if (!compiler_owned_token_prototype_enabled(input)) {
        return;
    }
    base::u32 token_index = 0;
    push_generated_token_record(records, input, buffer, token_index, syntax::TokenKind::identifier,
        FRONTEND_MACRO_M21I_DERIVE_BEGIN_TOKEN_TEXT, FRONTEND_MACRO_M21I_DERIVE_BEGIN_TOKEN_ROLE,
        input.attribute_range);
    ++token_index;
    for (const syntax::AttributeTokenDecl& source_token : attribute.token_tree) {
        const std::string token_text = generated_source_token_text(token_index);
        push_generated_token_record(records, input, buffer, token_index, source_token.kind, token_text,
            FRONTEND_MACRO_M21I_DERIVE_SOURCE_TOKEN_ROLE, source_token.range);
        ++token_index;
    }
    push_generated_token_record(records, input, buffer, token_index, syntax::TokenKind::identifier,
        FRONTEND_MACRO_M21I_DERIVE_END_TOKEN_TEXT, FRONTEND_MACRO_M21I_DERIVE_END_TOKEN_ROLE,
        input.attribute_range);
}

[[nodiscard]] GeneratedTokenParserAdmissionGateStub make_generated_token_parser_admission_gate_stub(
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const GeneratedModulePartParseMergeStub& parse_merge_stub,
    const GeneratedTokenBufferStub& buffer)
{
    const bool token_records_available = buffer.token_count > 0;
    return GeneratedTokenParserAdmissionGateStub{
        input.item,
        input.module,
        input.part_index,
        input.attribute_index,
        input.attached_part,
        placeholder.generated_part,
        buffer.token_plan_identity,
        buffer.token_buffer_identity,
        buffer.materialization_identity,
        buffer.source_map_identity,
        buffer.hygiene_mark,
        parse_merge_stub.generated_buffer_identity,
        parse_merge_stub.parse_config_fingerprint,
        generated_token_parser_admission_gate_identity(
            input, placeholder, parse_merge_stub, buffer, token_records_available),
        buffer.token_stream_name,
        std::string(FRONTEND_MACRO_M21J_PARSER_GATE_POLICY),
        std::string(parser_admission_blocker_for_input(input)),
        buffer.token_count,
        true,
        buffer.materialized_tokens,
        token_records_available,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
    };
}

[[nodiscard]] ParserAdmissionDiagnosticProjectionStub make_parser_admission_diagnostic_projection_stub(
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const GeneratedModulePartParseMergeStub& parse_merge_stub,
    const ExpansionTraceStub& trace,
    const GeneratedTokenBufferStub& buffer,
    const GeneratedTokenParserAdmissionGateStub& gate)
{
    const std::string debug_projection_name = parser_admission_debug_projection_name(input);
    const query::StableFingerprint128 diagnostic_anchor =
        parser_admission_diagnostic_anchor_identity(input, gate, trace);
    return ParserAdmissionDiagnosticProjectionStub{
        input.item,
        input.module,
        input.part_index,
        input.attribute_index,
        input.attached_part,
        placeholder.generated_part,
        input.attribute_range,
        input.token_tree_range,
        gate.parse_gate_identity,
        parser_admission_diagnostic_identity(input, placeholder, parse_merge_stub, buffer, gate, trace,
            diagnostic_anchor, debug_projection_name),
        diagnostic_anchor,
        gate.token_plan_identity,
        gate.token_buffer_identity,
        gate.materialization_identity,
        parse_merge_stub.generated_buffer_identity,
        parse_merge_stub.parse_config_fingerprint,
        buffer.source_map_identity,
        buffer.hygiene_mark,
        trace.trace_identity,
        std::string(FRONTEND_MACRO_M21K_DIAGNOSTIC_POLICY),
        std::string(parser_admission_diagnostic_category_for_input(input)),
        std::string(parser_admission_blocker_for_input(input)),
        std::string(FRONTEND_MACRO_M21K_GENERATED_PART_PARSE_BLOCKER),
        std::string(parser_admission_diagnostic_message_for_input(input)),
        debug_projection_name,
        gate.token_count,
        gate.token_buffer_materialized,
        gate.token_records_available,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
    };
}

[[nodiscard]] query::StableFingerprint128 parser_admission_report_entry_identity(
    const ParserAdmissionDiagnosticProjectionStub& diagnostic,
    const base::u32 report_index,
    const std::string_view query_projection_name) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21L_REPORT_ENTRY_IDENTITY_MARKER);
    builder.mix_u32(diagnostic.item.value);
    builder.mix_u32(diagnostic.module.value);
    builder.mix_u32(diagnostic.part_index);
    builder.mix_u32(diagnostic.attribute_index);
    builder.mix_u32(report_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(diagnostic.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(diagnostic.generated_part));
    builder.mix_u64(static_cast<base::u64>(diagnostic.primary_anchor.source.value));
    builder.mix_u64(static_cast<base::u64>(diagnostic.primary_anchor.begin));
    builder.mix_u64(static_cast<base::u64>(diagnostic.primary_anchor.end));
    builder.mix_u64(static_cast<base::u64>(diagnostic.token_tree_anchor.source.value));
    builder.mix_u64(static_cast<base::u64>(diagnostic.token_tree_anchor.begin));
    builder.mix_u64(static_cast<base::u64>(diagnostic.token_tree_anchor.end));
    builder.mix_fingerprint(diagnostic.diagnostic_identity);
    builder.mix_fingerprint(diagnostic.diagnostic_anchor_identity);
    builder.mix_fingerprint(diagnostic.parse_gate_identity);
    builder.mix_string(diagnostic.blocker_category);
    builder.mix_string(diagnostic.debug_projection_name);
    builder.mix_string(query_projection_name);
    builder.mix_u64(diagnostic.token_count);
    builder.mix_bool(diagnostic.token_records_available);
    builder.mix_bool(false);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    return builder.finish();
}

[[nodiscard]] ParserAdmissionDiagnosticReportEntry make_parser_admission_report_entry(
    const ParserAdmissionDiagnosticProjectionStub& diagnostic,
    const base::u32 report_index)
{
    const std::string query_projection_name =
        parser_admission_report_query_name(diagnostic.module, diagnostic.part_index);
    return ParserAdmissionDiagnosticReportEntry{
        diagnostic.item,
        diagnostic.module,
        diagnostic.part_index,
        diagnostic.attribute_index,
        report_index,
        diagnostic.attached_part,
        diagnostic.generated_part,
        diagnostic.primary_anchor,
        diagnostic.token_tree_anchor,
        diagnostic.diagnostic_identity,
        diagnostic.diagnostic_anchor_identity,
        parser_admission_report_entry_identity(diagnostic, report_index, query_projection_name),
        diagnostic.parse_gate_identity,
        diagnostic.blocker_category,
        diagnostic.debug_projection_name,
        query_projection_name,
        diagnostic.token_count,
        diagnostic.token_records_available,
        false,
        true,
        true,
        false,
        false,
        false,
    };
}

[[nodiscard]] bool source_range_less_or_equal(
    const base::SourceRange& lhs, const base::SourceRange& rhs) noexcept
{
    if (lhs.source.value != rhs.source.value) {
        return lhs.source.value < rhs.source.value;
    }
    if (lhs.begin != rhs.begin) {
        return lhs.begin < rhs.begin;
    }
    return lhs.end <= rhs.end;
}

[[nodiscard]] bool parser_admission_report_entries_are_ordered(
    const std::vector<ParserAdmissionDiagnosticReportEntry>& entries,
    const syntax::ModuleId module,
    const base::u32 source_part_index) noexcept
{
    bool saw_entry = false;
    base::SourceRange previous{};
    for (const ParserAdmissionDiagnosticReportEntry& entry : entries) {
        if (entry.module.value != module.value || entry.part_index != source_part_index) {
            continue;
        }
        if (saw_entry && !source_range_less_or_equal(previous, entry.primary_anchor)) {
            return false;
        }
        previous = entry.primary_anchor;
        saw_entry = true;
    }
    return true;
}

[[nodiscard]] query::StableFingerprint128 parser_admission_report_grouping_identity(
    const GeneratedModulePartPlaceholder& placeholder,
    const std::vector<ParserAdmissionDiagnosticReportEntry>& entries) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21L_REPORT_GROUPING_IDENTITY_MARKER);
    builder.mix_u32(placeholder.module.value);
    builder.mix_u32(placeholder.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.source_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    base::u64 entry_count = 0;
    for (const ParserAdmissionDiagnosticReportEntry& entry : entries) {
        if (entry.module.value != placeholder.module.value
            || entry.part_index != placeholder.source_part_index) {
            continue;
        }
        ++entry_count;
        builder.mix_fingerprint(entry.report_entry_identity);
        builder.mix_fingerprint(entry.diagnostic_identity);
        builder.mix_fingerprint(entry.diagnostic_anchor_identity);
        builder.mix_u32(entry.report_index);
        builder.mix_string(entry.blocker_category);
    }
    builder.mix_u64(entry_count);
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 parser_admission_report_anchor_identity(
    const GeneratedModulePartPlaceholder& placeholder,
    const std::vector<ParserAdmissionDiagnosticReportEntry>& entries) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21L_REPORT_ANCHOR_IDENTITY_MARKER);
    builder.mix_u32(placeholder.module.value);
    builder.mix_u32(placeholder.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    for (const ParserAdmissionDiagnosticReportEntry& entry : entries) {
        if (entry.module.value != placeholder.module.value
            || entry.part_index != placeholder.source_part_index) {
            continue;
        }
        builder.mix_u32(entry.report_index);
        builder.mix_u64(static_cast<base::u64>(entry.primary_anchor.source.value));
        builder.mix_u64(static_cast<base::u64>(entry.primary_anchor.begin));
        builder.mix_u64(static_cast<base::u64>(entry.primary_anchor.end));
        builder.mix_fingerprint(entry.diagnostic_anchor_identity);
    }
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 parser_admission_report_identity(
    const GeneratedModulePartPlaceholder& placeholder,
    const GeneratedModulePartParseMergeStub& parse_merge_stub,
    const query::StableFingerprint128 grouping_identity,
    const query::StableFingerprint128 anchor_identity,
    const std::string_view report_query_name,
    const std::vector<ParserAdmissionDiagnosticReportEntry>& entries) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21L_REPORT_IDENTITY_MARKER);
    builder.mix_u32(placeholder.module.value);
    builder.mix_u32(placeholder.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.source_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    builder.mix_fingerprint(parse_merge_stub.generated_buffer_identity);
    builder.mix_fingerprint(parse_merge_stub.parse_config_fingerprint);
    builder.mix_fingerprint(grouping_identity);
    builder.mix_fingerprint(anchor_identity);
    builder.mix_string(FRONTEND_MACRO_M21L_REPORT_POLICY);
    builder.mix_string(report_query_name);
    builder.mix_string(FRONTEND_MACRO_M21L_REPORT_BLOCKER);
    for (const ParserAdmissionDiagnosticReportEntry& entry : entries) {
        if (entry.module.value != placeholder.module.value
            || entry.part_index != placeholder.source_part_index) {
            continue;
        }
        builder.mix_fingerprint(entry.report_entry_identity);
        builder.mix_bool(entry.token_records_available);
        builder.mix_bool(entry.parser_admitted);
    }
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(parser_admission_report_entries_are_ordered(
        entries, placeholder.module, placeholder.source_part_index));
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    return builder.finish();
}

[[nodiscard]] ParserAdmissionDiagnosticReport make_parser_admission_report(
    const GeneratedModulePartPlaceholder& placeholder,
    const GeneratedModulePartParseMergeStub& parse_merge_stub,
    const std::vector<ParserAdmissionDiagnosticReportEntry>& entries)
{
    const std::string report_query_name =
        parser_admission_report_query_name(placeholder.module, placeholder.source_part_index);
    const query::StableFingerprint128 grouping_identity =
        parser_admission_report_grouping_identity(placeholder, entries);
    const query::StableFingerprint128 anchor_identity =
        parser_admission_report_anchor_identity(placeholder, entries);
    ParserAdmissionDiagnosticReport report{
        placeholder.module,
        placeholder.source_part_index,
        placeholder.source_part,
        placeholder.generated_part,
        {},
        anchor_identity,
        grouping_identity,
        parse_merge_stub.parse_config_fingerprint,
        parse_merge_stub.generated_buffer_identity,
        std::string(FRONTEND_MACRO_M21L_REPORT_POLICY),
        report_query_name,
        std::string(FRONTEND_MACRO_M21L_REPORT_BLOCKER),
        0,
        0,
        0,
        0,
        0,
        true,
        true,
        parser_admission_report_entries_are_ordered(entries, placeholder.module, placeholder.source_part_index),
        false,
        false,
        false,
        false,
        false,
        false,
        false,
    };
    for (const ParserAdmissionDiagnosticReportEntry& entry : entries) {
        if (entry.module.value != placeholder.module.value
            || entry.part_index != placeholder.source_part_index) {
            continue;
        }
        ++report.entry_count;
        if (!entry.parser_admitted) {
            ++report.blocked_entry_count;
        }
        if (entry.blocker_category == FRONTEND_MACRO_M21K_DERIVE_BLOCKER_CATEGORY) {
            ++report.derive_entry_count;
        }
        if (entry.blocker_category == FRONTEND_MACRO_M21K_EMPTY_BLOCKER_CATEGORY) {
            ++report.empty_entry_count;
        }
        if (entry.token_records_available) {
            ++report.token_record_available_entry_count;
        }
    }
    report.report_identity = parser_admission_report_identity(
        placeholder, parse_merge_stub, report.report_grouping_identity, report.report_anchor_identity,
        report.report_query_name, entries);
    return report;
}

[[nodiscard]] query::StableFingerprint128 parser_readiness_preflight_identity(
    const EarlyItemMacroInput& input,
    const GeneratedTokenBufferStub& buffer,
    const GeneratedTokenParserAdmissionGateStub& gate,
    const ParserAdmissionDiagnosticProjectionStub& diagnostic,
    const ParserAdmissionDiagnosticReportEntry& report_entry,
    const base::u32 preflight_index,
    const std::string_view token_stream_shape,
    const bool token_indices_contiguous,
    const bool delimiter_balanced,
    const bool source_anchors_covered) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21M_PREFLIGHT_IDENTITY_MARKER);
    mix_macro_input_identity(builder, input);
    builder.mix_u32(preflight_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(buffer.generated_part));
    builder.mix_fingerprint(buffer.token_plan_identity);
    builder.mix_fingerprint(buffer.token_buffer_identity);
    builder.mix_fingerprint(buffer.materialization_identity);
    builder.mix_fingerprint(gate.generated_buffer_identity);
    builder.mix_fingerprint(gate.parse_config_fingerprint);
    builder.mix_fingerprint(gate.parse_gate_identity);
    builder.mix_fingerprint(diagnostic.diagnostic_identity);
    builder.mix_fingerprint(diagnostic.diagnostic_anchor_identity);
    builder.mix_fingerprint(report_entry.report_entry_identity);
    builder.mix_fingerprint(buffer.source_map_identity);
    builder.mix_fingerprint(buffer.hygiene_mark);
    builder.mix_fingerprint(diagnostic.trace_identity);
    builder.mix_string(buffer.token_stream_name);
    builder.mix_string(token_stream_shape);
    builder.mix_string(FRONTEND_MACRO_M21M_DELIMITER_BALANCED_STATE);
    builder.mix_string(FRONTEND_MACRO_M21M_SOURCE_ANCHOR_COVERED_STATE);
    builder.mix_string(FRONTEND_MACRO_M21M_PREFLIGHT_POLICY);
    builder.mix_string(FRONTEND_MACRO_M21M_PREFLIGHT_BLOCKER);
    builder.mix_u64(buffer.token_count);
    builder.mix_bool(gate.token_records_available);
    builder.mix_bool(token_indices_contiguous);
    builder.mix_bool(delimiter_balanced);
    builder.mix_bool(source_anchors_covered);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    return builder.finish();
}

[[nodiscard]] GeneratedTokenParserReadinessPreflightEntry make_parser_readiness_preflight_entry(
    const EarlyItemMacroInput& input,
    const GeneratedTokenBufferStub& buffer,
    const GeneratedTokenParserAdmissionGateStub& gate,
    const ParserAdmissionDiagnosticProjectionStub& diagnostic,
    const ParserAdmissionDiagnosticReportEntry& report_entry,
    const std::vector<GeneratedTokenRecord>& records,
    const base::u32 preflight_index)
{
    const std::string_view token_stream_shape =
        parser_readiness_token_stream_shape_for_category(report_entry.blocker_category);
    const bool token_indices_contiguous =
        generated_token_indices_are_contiguous(buffer, records);
    const bool delimiter_balanced =
        generated_token_delimiters_are_balanced(buffer, records);
    const bool source_anchors_covered =
        generated_token_source_anchors_are_covered(input, buffer, records);
    return GeneratedTokenParserReadinessPreflightEntry{
        input.item,
        input.module,
        input.part_index,
        input.attribute_index,
        preflight_index,
        input.attached_part,
        buffer.generated_part,
        buffer.token_plan_identity,
        buffer.token_buffer_identity,
        buffer.materialization_identity,
        gate.generated_buffer_identity,
        gate.parse_config_fingerprint,
        gate.parse_gate_identity,
        diagnostic.diagnostic_identity,
        diagnostic.diagnostic_anchor_identity,
        report_entry.report_entry_identity,
        buffer.source_map_identity,
        buffer.hygiene_mark,
        diagnostic.trace_identity,
        parser_readiness_preflight_identity(input, buffer, gate, diagnostic, report_entry,
            preflight_index, token_stream_shape, token_indices_contiguous, delimiter_balanced,
            source_anchors_covered),
        buffer.token_stream_name,
        std::string(token_stream_shape),
        std::string(FRONTEND_MACRO_M21M_DELIMITER_BALANCED_STATE),
        std::string(FRONTEND_MACRO_M21M_SOURCE_ANCHOR_COVERED_STATE),
        std::string(FRONTEND_MACRO_M21M_PREFLIGHT_POLICY),
        std::string(FRONTEND_MACRO_M21M_PREFLIGHT_BLOCKER),
        buffer.token_count,
        gate.token_records_available,
        token_indices_contiguous,
        delimiter_balanced,
        source_anchors_covered,
        true,
        true,
        true,
        true,
        false,
        false,
        false,
        false,
        false,
        false,
    };
}

[[nodiscard]] query::StableFingerprint128 parser_consumption_contract_grouping_identity(
    const GeneratedModulePartPlaceholder& placeholder,
    const std::vector<GeneratedTokenParserReadinessPreflightEntry>& entries) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21N_CONTRACT_GROUPING_IDENTITY_MARKER);
    builder.mix_u32(placeholder.module.value);
    builder.mix_u32(placeholder.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.source_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    base::u64 entry_count = 0;
    for (const GeneratedTokenParserReadinessPreflightEntry& entry : entries) {
        if (entry.module.value != placeholder.module.value
            || entry.part_index != placeholder.source_part_index) {
            continue;
        }
        ++entry_count;
        builder.mix_fingerprint(entry.preflight_identity);
        builder.mix_fingerprint(entry.parse_gate_identity);
        builder.mix_fingerprint(entry.diagnostic_identity);
        builder.mix_fingerprint(entry.report_entry_identity);
        builder.mix_string(entry.token_stream_shape);
    }
    builder.mix_u64(entry_count);
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 parser_consumption_contract_anchor_identity(
    const GeneratedModulePartPlaceholder& placeholder,
    const std::vector<GeneratedTokenParserReadinessPreflightEntry>& entries) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21N_CONTRACT_ANCHOR_IDENTITY_MARKER);
    builder.mix_u32(placeholder.module.value);
    builder.mix_u32(placeholder.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    for (const GeneratedTokenParserReadinessPreflightEntry& entry : entries) {
        if (entry.module.value != placeholder.module.value
            || entry.part_index != placeholder.source_part_index) {
            continue;
        }
        builder.mix_u32(entry.preflight_index);
        builder.mix_fingerprint(entry.diagnostic_anchor_identity);
        builder.mix_bool(entry.source_anchors_covered);
    }
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 parser_consumption_contract_identity(
    const GeneratedModulePartPlaceholder& placeholder,
    const GeneratedModulePartParseMergeStub& parse_merge_stub,
    const ParserAdmissionDiagnosticReport& report,
    const query::StableFingerprint128 grouping_identity,
    const query::StableFingerprint128 anchor_identity,
    const std::string_view query_name,
    const std::vector<GeneratedTokenParserReadinessPreflightEntry>& entries) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21N_CONTRACT_IDENTITY_MARKER);
    builder.mix_u32(placeholder.module.value);
    builder.mix_u32(placeholder.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.source_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    builder.mix_fingerprint(parse_merge_stub.generated_buffer_identity);
    builder.mix_fingerprint(parse_merge_stub.parse_config_fingerprint);
    builder.mix_fingerprint(report.report_identity);
    builder.mix_fingerprint(grouping_identity);
    builder.mix_fingerprint(anchor_identity);
    builder.mix_string(FRONTEND_MACRO_M21N_CONTRACT_POLICY);
    builder.mix_string(query_name);
    builder.mix_string(FRONTEND_MACRO_M21N_CONTRACT_BLOCKER);
    for (const GeneratedTokenParserReadinessPreflightEntry& entry : entries) {
        if (entry.module.value != placeholder.module.value
            || entry.part_index != placeholder.source_part_index) {
            continue;
        }
        builder.mix_fingerprint(entry.preflight_identity);
        builder.mix_bool(entry.token_indices_contiguous);
        builder.mix_bool(entry.delimiter_balanced);
        builder.mix_bool(entry.source_anchors_covered);
        builder.mix_bool(entry.parse_config_compatible);
        builder.mix_bool(entry.diagnostic_projection_available);
        builder.mix_bool(entry.parser_consumable);
    }
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    return builder.finish();
}

[[nodiscard]] GeneratedTokenParserConsumptionContractGate make_parser_consumption_contract_gate(
    const GeneratedModulePartPlaceholder& placeholder,
    const GeneratedModulePartParseMergeStub& parse_merge_stub,
    const ParserAdmissionDiagnosticReport& report,
    const std::vector<GeneratedTokenParserReadinessPreflightEntry>& entries)
{
    const std::string query_name =
        parser_consumption_contract_query_name(placeholder.module, placeholder.source_part_index);
    const query::StableFingerprint128 grouping_identity =
        parser_consumption_contract_grouping_identity(placeholder, entries);
    const query::StableFingerprint128 anchor_identity =
        parser_consumption_contract_anchor_identity(placeholder, entries);
    GeneratedTokenParserConsumptionContractGate gate{
        placeholder.module,
        placeholder.source_part_index,
        placeholder.source_part,
        placeholder.generated_part,
        parse_merge_stub.generated_buffer_identity,
        parse_merge_stub.parse_config_fingerprint,
        report.report_identity,
        {},
        grouping_identity,
        anchor_identity,
        std::string(FRONTEND_MACRO_M21N_CONTRACT_POLICY),
        query_name,
        std::string(FRONTEND_MACRO_M21N_CONTRACT_BLOCKER),
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        true,
        true,
        true,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
    };
    for (const GeneratedTokenParserReadinessPreflightEntry& entry : entries) {
        if (entry.module.value != placeholder.module.value
            || entry.part_index != placeholder.source_part_index) {
            continue;
        }
        ++gate.preflight_entry_count;
        if (!entry.parser_admitted) {
            ++gate.blocked_entry_count;
        }
        if (entry.token_stream_shape == FRONTEND_MACRO_M21M_DERIVE_TOKEN_STREAM_SHAPE) {
            ++gate.derive_entry_count;
        }
        if (entry.token_stream_shape == FRONTEND_MACRO_M21M_EMPTY_TOKEN_STREAM_SHAPE) {
            ++gate.empty_entry_count;
        }
        if (entry.token_indices_contiguous) {
            ++gate.contiguous_index_entry_count;
        }
        if (entry.delimiter_balanced) {
            ++gate.delimiter_balanced_entry_count;
        }
        if (entry.source_anchors_covered) {
            ++gate.source_anchor_covered_entry_count;
        }
        if (entry.parse_config_compatible) {
            ++gate.parse_config_compatible_entry_count;
        }
        if (entry.diagnostic_projection_available) {
            ++gate.diagnostic_projection_entry_count;
        }
    }
    gate.all_entries_structurally_checked =
        gate.preflight_entry_count == gate.contiguous_index_entry_count
        && gate.preflight_entry_count == gate.delimiter_balanced_entry_count
        && gate.preflight_entry_count == gate.source_anchor_covered_entry_count
        && gate.preflight_entry_count == gate.parse_config_compatible_entry_count
        && gate.preflight_entry_count == gate.diagnostic_projection_entry_count;
    gate.contract_identity = parser_consumption_contract_identity(placeholder, parse_merge_stub,
        report, gate.contract_grouping_identity, gate.contract_anchor_identity, gate.contract_query_name,
        entries);
    return gate;
}

[[nodiscard]] query::StableFingerprint128 macro_boundary_closure_grouping_identity(
    const EarlyItemExpansionResult& result) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21O_CLOSURE_GROUPING_IDENTITY_MARKER);
    builder.mix_u64(static_cast<base::u64>(result.inputs.size()));
    builder.mix_u64(static_cast<base::u64>(result.generated_parts.size()));
    builder.mix_u64(static_cast<base::u64>(result.parser_admission_reports.size()));
    builder.mix_u64(static_cast<base::u64>(result.parser_readiness_preflight_entries.size()));
    builder.mix_u64(static_cast<base::u64>(result.parser_consumption_contract_gates.size()));
    for (const ParserAdmissionDiagnosticReport& report : result.parser_admission_reports) {
        builder.mix_fingerprint(report.report_identity);
    }
    for (const GeneratedTokenParserReadinessPreflightEntry& entry :
        result.parser_readiness_preflight_entries) {
        builder.mix_fingerprint(entry.preflight_identity);
    }
    for (const GeneratedTokenParserConsumptionContractGate& gate :
        result.parser_consumption_contract_gates) {
        builder.mix_fingerprint(gate.contract_identity);
    }
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 macro_boundary_closure_identity(
    const EarlyItemExpansionResult& result,
    const query::StableFingerprint128 grouping_identity,
    const base::u64 blocked_contract_gate_count,
    const base::u64 parser_consumable_contract_gate_count) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21O_CLOSURE_IDENTITY_MARKER);
    builder.mix_fingerprint(query::macro_expansion_plan_fingerprint(result.plan));
    builder.mix_fingerprint(grouping_identity);
    builder.mix_string(FRONTEND_MACRO_M21O_CLOSURE_POLICY);
    builder.mix_string(FRONTEND_MACRO_M21O_CLOSURE_QUERY_NAME);
    builder.mix_string(FRONTEND_MACRO_M21O_CLOSURE_BLOCKER);
    builder.mix_u64(static_cast<base::u64>(result.inputs.size()));
    builder.mix_u64(static_cast<base::u64>(result.generated_parts.size()));
    builder.mix_u64(static_cast<base::u64>(result.parser_admission_reports.size()));
    builder.mix_u64(static_cast<base::u64>(result.parser_readiness_preflight_entries.size()));
    builder.mix_u64(static_cast<base::u64>(result.parser_consumption_contract_gates.size()));
    builder.mix_u64(blocked_contract_gate_count);
    builder.mix_u64(parser_consumable_contract_gate_count);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    return builder.finish();
}

[[nodiscard]] MacroExpansionBoundaryClosureReport make_macro_boundary_closure_report(
    const EarlyItemExpansionResult& result)
{
    base::u64 blocked_contract_gate_count = 0;
    base::u64 parser_consumable_contract_gate_count = 0;
    for (const GeneratedTokenParserConsumptionContractGate& gate :
        result.parser_consumption_contract_gates) {
        if (!gate.parser_admitted) {
            ++blocked_contract_gate_count;
        }
        if (gate.parser_consumable) {
            ++parser_consumable_contract_gate_count;
        }
    }
    const query::StableFingerprint128 grouping_identity =
        macro_boundary_closure_grouping_identity(result);
    return MacroExpansionBoundaryClosureReport{
        macro_boundary_closure_identity(result, grouping_identity, blocked_contract_gate_count,
            parser_consumable_contract_gate_count),
        grouping_identity,
        std::string(FRONTEND_MACRO_M21O_CLOSURE_POLICY),
        std::string(FRONTEND_MACRO_M21O_CLOSURE_QUERY_NAME),
        std::string(FRONTEND_MACRO_M21O_CLOSURE_BLOCKER),
        static_cast<base::u64>(result.inputs.size()),
        static_cast<base::u64>(result.generated_parts.size()),
        static_cast<base::u64>(result.parser_admission_reports.size()),
        static_cast<base::u64>(result.parser_readiness_preflight_entries.size()),
        static_cast<base::u64>(result.parser_consumption_contract_gates.size()),
        blocked_contract_gate_count,
        parser_consumable_contract_gate_count,
        true,
        true,
        true,
        true,
        true,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
    };
}

[[nodiscard]] query::StableFingerprint128 builtin_derive_admission_identity(
    const EarlyItemMacroInput& input,
    const GeneratedTokenBufferStub& buffer,
    const GeneratedTokenParserAdmissionGateStub& parser_gate,
    const GeneratedTokenParserReadinessPreflightEntry& preflight,
    const ParserAdmissionDiagnosticProjectionStub& diagnostic,
    const MacroExpansionBoundaryClosureReport& closure,
    const std::string_view query_name,
    const base::u32 admission_index,
    const base::u64 capability_candidate_count,
    const base::u64 unsupported_candidate_count,
    const base::u64 duplicate_candidate_count) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M22A_ADMISSION_IDENTITY_MARKER);
    mix_macro_input_identity(builder, input);
    builder.mix_u32(admission_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(buffer.generated_part));
    builder.mix_fingerprint(buffer.token_buffer_identity);
    builder.mix_fingerprint(preflight.preflight_identity);
    builder.mix_fingerprint(parser_gate.parse_gate_identity);
    builder.mix_fingerprint(diagnostic.diagnostic_identity);
    builder.mix_fingerprint(closure.closure_identity);
    builder.mix_string(FRONTEND_MACRO_M22A_ADMISSION_POLICY);
    builder.mix_string(builtin_derive_admission_kind_for_input(input));
    builder.mix_string(query_name);
    builder.mix_string(builtin_derive_admission_blocker_for_input(input));
    builder.mix_u64(buffer.token_count);
    builder.mix_u64(capability_candidate_count);
    builder.mix_u64(unsupported_candidate_count);
    builder.mix_u64(duplicate_candidate_count);
    builder.mix_bool(builtin_derive_input(input));
    builder.mix_bool(true);
    builder.mix_bool(parser_gate.token_records_available);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    return builder.finish();
}

[[nodiscard]] BuiltinDeriveExpansionAdmissionGate make_builtin_derive_expansion_admission_gate(
    const EarlyItemMacroInput& input,
    const syntax::AttributeDecl& attribute,
    const GeneratedTokenBufferStub& buffer,
    const GeneratedTokenParserAdmissionGateStub& parser_gate,
    const GeneratedTokenParserReadinessPreflightEntry& preflight,
    const ParserAdmissionDiagnosticProjectionStub& diagnostic,
    const MacroExpansionBoundaryClosureReport& closure,
    const base::u32 admission_index)
{
    const std::string query_name = builtin_derive_admission_query_name(input);
    const base::u64 capability_candidate_count =
        builtin_derive_input(input) ? count_builtin_derive_capability_candidates(attribute) : 0U;
    const base::u64 unsupported_candidate_count =
        builtin_derive_input(input) ? count_builtin_derive_unsupported_candidates(attribute) : 0U;
    const base::u64 duplicate_candidate_count =
        builtin_derive_input(input) ? count_builtin_derive_duplicate_candidates(attribute) : 0U;
    return BuiltinDeriveExpansionAdmissionGate{
        input.item,
        input.module,
        input.part_index,
        input.attribute_index,
        admission_index,
        input.attached_part,
        buffer.generated_part,
        buffer.token_buffer_identity,
        preflight.preflight_identity,
        parser_gate.parse_gate_identity,
        diagnostic.diagnostic_identity,
        closure.closure_identity,
        builtin_derive_admission_identity(input, buffer, parser_gate, preflight, diagnostic, closure,
            query_name, admission_index, capability_candidate_count, unsupported_candidate_count,
            duplicate_candidate_count),
        std::string(FRONTEND_MACRO_M22A_ADMISSION_POLICY),
        std::string(builtin_derive_admission_kind_for_input(input)),
        query_name,
        std::string(builtin_derive_admission_blocker_for_input(input)),
        buffer.token_count,
        capability_candidate_count,
        unsupported_candidate_count,
        duplicate_candidate_count,
        builtin_derive_input(input),
        true,
        parser_gate.token_records_available,
        true,
        true,
        true,
        false,
        false,
        false,
        false,
        false,
        false,
    };
}

[[nodiscard]] query::StableFingerprint128 builtin_derive_capability_set_identity(
    const EarlyItemMacroInput& input,
    const BuiltinDeriveExpansionAdmissionGate& admission,
    const std::string_view target_kind,
    const base::u64 copy_capability_count,
    const base::u64 eq_capability_count,
    const base::u64 hash_capability_count) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M22B_CAPABILITY_SET_IDENTITY_MARKER);
    mix_macro_input_identity(builder, input);
    builder.mix_fingerprint(admission.admission_identity);
    builder.mix_string(target_kind);
    builder.mix_u64(copy_capability_count);
    builder.mix_u64(eq_capability_count);
    builder.mix_u64(hash_capability_count);
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 builtin_derive_semantic_plan_identity(
    const EarlyItemMacroInput& input,
    const BuiltinDeriveExpansionAdmissionGate& admission,
    const query::StableFingerprint128 capability_set_identity,
    const std::string_view target_kind,
    const base::u32 semantic_plan_index,
    const base::u64 capability_count,
    const base::u64 copy_capability_count,
    const base::u64 eq_capability_count,
    const base::u64 hash_capability_count,
    const bool target_struct_or_enum) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M22B_SEMANTIC_PLAN_IDENTITY_MARKER);
    mix_macro_input_identity(builder, input);
    builder.mix_u32(semantic_plan_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(admission.generated_part));
    builder.mix_fingerprint(admission.token_buffer_identity);
    builder.mix_fingerprint(admission.preflight_identity);
    builder.mix_fingerprint(admission.admission_identity);
    builder.mix_fingerprint(capability_set_identity);
    builder.mix_string(FRONTEND_MACRO_M22B_SEMANTIC_POLICY);
    builder.mix_string(target_kind);
    builder.mix_string(FRONTEND_MACRO_M22B_SEMANTIC_MODEL);
    builder.mix_string(FRONTEND_MACRO_M22B_BLOCKER);
    builder.mix_u64(capability_count);
    builder.mix_u64(copy_capability_count);
    builder.mix_u64(eq_capability_count);
    builder.mix_u64(hash_capability_count);
    builder.mix_bool(builtin_derive_input(input));
    builder.mix_bool(target_struct_or_enum);
    builder.mix_bool(builtin_derive_input(input));
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(true);
    builder.mix_bool(true);
    return builder.finish();
}

[[nodiscard]] BuiltinDeriveSemanticExpansionPlan make_builtin_derive_semantic_expansion_plan(
    const EarlyItemMacroInput& input,
    const syntax::ItemNode& item,
    const BuiltinDeriveExpansionAdmissionGate& admission,
    const base::u32 semantic_plan_index)
{
    const base::u64 copy_capability_count = builtin_derive_input(input)
        ? count_derive_capability(item, FRONTEND_MACRO_M22_CAPABILITY_COPY)
        : 0U;
    const base::u64 eq_capability_count = builtin_derive_input(input)
        ? count_derive_capability(item, FRONTEND_MACRO_M22_CAPABILITY_EQ)
        : 0U;
    const base::u64 hash_capability_count = builtin_derive_input(input)
        ? count_derive_capability(item, FRONTEND_MACRO_M22_CAPABILITY_HASH)
        : 0U;
    const std::string_view target_kind = builtin_derive_target_kind(item);
    const bool target_struct_or_enum = builtin_derive_target_is_struct_or_enum(item);
    const base::u64 capability_count = copy_capability_count + eq_capability_count + hash_capability_count;
    const query::StableFingerprint128 capability_set_identity =
        builtin_derive_capability_set_identity(input, admission, target_kind, copy_capability_count,
            eq_capability_count, hash_capability_count);
    return BuiltinDeriveSemanticExpansionPlan{
        input.item,
        input.module,
        input.part_index,
        input.attribute_index,
        semantic_plan_index,
        input.attached_part,
        admission.generated_part,
        admission.token_buffer_identity,
        admission.preflight_identity,
        admission.admission_identity,
        builtin_derive_semantic_plan_identity(input, admission, capability_set_identity,
            target_kind, semantic_plan_index, capability_count, copy_capability_count,
            eq_capability_count, hash_capability_count, target_struct_or_enum),
        capability_set_identity,
        std::string(FRONTEND_MACRO_M22B_SEMANTIC_POLICY),
        std::string(target_kind),
        std::string(FRONTEND_MACRO_M22B_SEMANTIC_MODEL),
        std::string(FRONTEND_MACRO_M22B_BLOCKER),
        capability_count,
        copy_capability_count,
        eq_capability_count,
        hash_capability_count,
        builtin_derive_input(input),
        target_struct_or_enum,
        builtin_derive_input(input),
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        true,
        true,
    };
}

[[nodiscard]] query::StableFingerprint128 builtin_derive_admission_group_identity(
    const GeneratedModulePartPlaceholder& placeholder,
    const std::vector<BuiltinDeriveExpansionAdmissionGate>& admissions) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M22C_ADMISSION_GROUP_IDENTITY_MARKER);
    builder.mix_u32(placeholder.module.value);
    builder.mix_u32(placeholder.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    base::u64 admission_count = 0;
    for (const BuiltinDeriveExpansionAdmissionGate& admission : admissions) {
        if (admission.module.value != placeholder.module.value
            || admission.part_index != placeholder.source_part_index) {
            continue;
        }
        ++admission_count;
        builder.mix_fingerprint(admission.admission_identity);
        builder.mix_string(admission.admission_kind);
        builder.mix_u64(admission.capability_candidate_count);
    }
    builder.mix_u64(admission_count);
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 builtin_derive_semantic_plan_group_identity(
    const GeneratedModulePartPlaceholder& placeholder,
    const std::vector<BuiltinDeriveSemanticExpansionPlan>& plans) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M22C_SEMANTIC_PLAN_GROUP_IDENTITY_MARKER);
    builder.mix_u32(placeholder.module.value);
    builder.mix_u32(placeholder.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    base::u64 plan_count = 0;
    for (const BuiltinDeriveSemanticExpansionPlan& plan : plans) {
        if (plan.module.value != placeholder.module.value
            || plan.part_index != placeholder.source_part_index) {
            continue;
        }
        ++plan_count;
        builder.mix_fingerprint(plan.semantic_plan_identity);
        builder.mix_fingerprint(plan.capability_set_identity);
        builder.mix_u64(plan.capability_count);
    }
    builder.mix_u64(plan_count);
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 builtin_derive_parser_release_gate_identity(
    const GeneratedModulePartPlaceholder& placeholder,
    const GeneratedTokenParserConsumptionContractGate& contract,
    const MacroExpansionBoundaryClosureReport& closure,
    const query::StableFingerprint128 admission_group_identity,
    const query::StableFingerprint128 semantic_plan_group_identity,
    const std::string_view release_query_name,
    const base::u64 admission_count,
    const base::u64 derive_admission_count,
    const base::u64 semantic_plan_count,
    const base::u64 capability_total_count,
    const base::u64 parser_consumable_contract_count) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M22C_RELEASE_GATE_IDENTITY_MARKER);
    builder.mix_u32(placeholder.module.value);
    builder.mix_u32(placeholder.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.source_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    builder.mix_fingerprint(contract.contract_identity);
    builder.mix_fingerprint(closure.closure_identity);
    builder.mix_fingerprint(admission_group_identity);
    builder.mix_fingerprint(semantic_plan_group_identity);
    builder.mix_string(FRONTEND_MACRO_M22C_RELEASE_POLICY);
    builder.mix_string(release_query_name);
    builder.mix_string(FRONTEND_MACRO_M22C_RELEASE_BLOCKER);
    builder.mix_u64(admission_count);
    builder.mix_u64(derive_admission_count);
    builder.mix_u64(semantic_plan_count);
    builder.mix_u64(capability_total_count);
    builder.mix_u64(parser_consumable_contract_count);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(true);
    builder.mix_bool(true);
    return builder.finish();
}

[[nodiscard]] BuiltinDeriveParserConsumptionReleaseGate make_builtin_derive_parser_consumption_release_gate(
    const GeneratedModulePartPlaceholder& placeholder,
    const GeneratedTokenParserConsumptionContractGate& contract,
    const MacroExpansionBoundaryClosureReport& closure,
    const std::vector<BuiltinDeriveExpansionAdmissionGate>& admissions,
    const std::vector<BuiltinDeriveSemanticExpansionPlan>& plans)
{
    base::u64 admission_count = 0;
    base::u64 derive_admission_count = 0;
    base::u64 semantic_plan_count = 0;
    base::u64 capability_total_count = 0;
    for (const BuiltinDeriveExpansionAdmissionGate& admission : admissions) {
        if (admission.module.value != placeholder.module.value
            || admission.part_index != placeholder.source_part_index) {
            continue;
        }
        ++admission_count;
        if (admission.builtin_derive_input) {
            ++derive_admission_count;
        }
    }
    for (const BuiltinDeriveSemanticExpansionPlan& plan : plans) {
        if (plan.module.value != placeholder.module.value
            || plan.part_index != placeholder.source_part_index) {
            continue;
        }
        ++semantic_plan_count;
        capability_total_count += plan.capability_count;
    }
    const base::u64 parser_consumable_contract_count = contract.parser_consumable ? 1U : 0U;
    const query::StableFingerprint128 admission_group_identity =
        builtin_derive_admission_group_identity(placeholder, admissions);
    const query::StableFingerprint128 semantic_plan_group_identity =
        builtin_derive_semantic_plan_group_identity(placeholder, plans);
    const std::string release_query_name =
        builtin_derive_parser_release_query_name(placeholder.module, placeholder.source_part_index);
    return BuiltinDeriveParserConsumptionReleaseGate{
        placeholder.module,
        placeholder.source_part_index,
        placeholder.source_part,
        placeholder.generated_part,
        contract.contract_identity,
        closure.closure_identity,
        admission_group_identity,
        semantic_plan_group_identity,
        builtin_derive_parser_release_gate_identity(placeholder, contract, closure,
            admission_group_identity, semantic_plan_group_identity, release_query_name, admission_count,
            derive_admission_count, semantic_plan_count, capability_total_count,
            parser_consumable_contract_count),
        std::string(FRONTEND_MACRO_M22C_RELEASE_POLICY),
        release_query_name,
        std::string(FRONTEND_MACRO_M22C_RELEASE_BLOCKER),
        admission_count,
        derive_admission_count,
        semantic_plan_count,
        capability_total_count,
        parser_consumable_contract_count,
        true,
        true,
        true,
        true,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        true,
        true,
    };
}

[[nodiscard]] bool fact_belongs_to_part(
    const syntax::ModuleId module,
    const base::u32 source_part_index,
    const syntax::ModuleId fact_module,
    const base::u32 fact_part_index) noexcept
{
    return fact_module.value == module.value && fact_part_index == source_part_index;
}

[[nodiscard]] base::u64 count_part_local_admissions(
    const GeneratedModulePartPlaceholder& placeholder,
    const std::vector<BuiltinDeriveExpansionAdmissionGate>& admissions) noexcept
{
    return static_cast<base::u64>(std::count_if(admissions.begin(), admissions.end(),
        [&placeholder](const BuiltinDeriveExpansionAdmissionGate& admission) {
            return fact_belongs_to_part(placeholder.module, placeholder.source_part_index,
                admission.module, admission.part_index);
        }));
}

[[nodiscard]] base::u64 count_part_local_derive_admissions(
    const GeneratedModulePartPlaceholder& placeholder,
    const std::vector<BuiltinDeriveExpansionAdmissionGate>& admissions) noexcept
{
    return static_cast<base::u64>(std::count_if(admissions.begin(), admissions.end(),
        [&placeholder](const BuiltinDeriveExpansionAdmissionGate& admission) {
            return fact_belongs_to_part(placeholder.module, placeholder.source_part_index,
                       admission.module, admission.part_index)
                && admission.builtin_derive_input;
        }));
}

[[nodiscard]] base::u64 count_part_local_semantic_plans(
    const GeneratedModulePartPlaceholder& placeholder,
    const std::vector<BuiltinDeriveSemanticExpansionPlan>& plans) noexcept
{
    return static_cast<base::u64>(std::count_if(plans.begin(), plans.end(),
        [&placeholder](const BuiltinDeriveSemanticExpansionPlan& plan) {
            return fact_belongs_to_part(placeholder.module, placeholder.source_part_index,
                plan.module, plan.part_index);
        }));
}

[[nodiscard]] base::u64 count_part_local_parser_release_gates(
    const GeneratedModulePartPlaceholder& placeholder,
    const std::vector<BuiltinDeriveParserConsumptionReleaseGate>& gates) noexcept
{
    return static_cast<base::u64>(std::count_if(gates.begin(), gates.end(),
        [&placeholder](const BuiltinDeriveParserConsumptionReleaseGate& gate) {
            return fact_belongs_to_part(placeholder.module, placeholder.source_part_index,
                gate.module, gate.source_part_index);
        }));
}

[[nodiscard]] base::u64 count_part_local_diagnostics(
    const GeneratedModulePartPlaceholder& placeholder,
    const std::vector<ParserAdmissionDiagnosticProjectionStub>& diagnostics) noexcept
{
    return static_cast<base::u64>(std::count_if(diagnostics.begin(), diagnostics.end(),
        [&placeholder](const ParserAdmissionDiagnosticProjectionStub& diagnostic) {
            return fact_belongs_to_part(placeholder.module, placeholder.source_part_index,
                diagnostic.module, diagnostic.part_index);
        }));
}

[[nodiscard]] base::u64 count_part_local_report_entries(
    const GeneratedModulePartPlaceholder& placeholder,
    const std::vector<ParserAdmissionDiagnosticReportEntry>& entries) noexcept
{
    return static_cast<base::u64>(std::count_if(entries.begin(), entries.end(),
        [&placeholder](const ParserAdmissionDiagnosticReportEntry& entry) {
            return fact_belongs_to_part(placeholder.module, placeholder.source_part_index,
                entry.module, entry.part_index);
        }));
}

[[nodiscard]] base::u64 count_part_local_blocked_diagnostics(
    const GeneratedModulePartPlaceholder& placeholder,
    const std::vector<ParserAdmissionDiagnosticProjectionStub>& diagnostics) noexcept
{
    return static_cast<base::u64>(std::count_if(diagnostics.begin(), diagnostics.end(),
        [&placeholder](const ParserAdmissionDiagnosticProjectionStub& diagnostic) {
            return fact_belongs_to_part(placeholder.module, placeholder.source_part_index,
                       diagnostic.module, diagnostic.part_index)
                && !diagnostic.parser_admitted;
        }));
}

[[nodiscard]] base::u64 count_part_local_derive_diagnostics(
    const GeneratedModulePartPlaceholder& placeholder,
    const std::vector<ParserAdmissionDiagnosticProjectionStub>& diagnostics) noexcept
{
    return static_cast<base::u64>(std::count_if(diagnostics.begin(), diagnostics.end(),
        [&placeholder](const ParserAdmissionDiagnosticProjectionStub& diagnostic) {
            return fact_belongs_to_part(placeholder.module, placeholder.source_part_index,
                       diagnostic.module, diagnostic.part_index)
                && diagnostic.blocker_category == FRONTEND_MACRO_M21K_DERIVE_BLOCKER_CATEGORY;
        }));
}

[[nodiscard]] base::u64 count_part_local_empty_diagnostics(
    const GeneratedModulePartPlaceholder& placeholder,
    const std::vector<ParserAdmissionDiagnosticProjectionStub>& diagnostics) noexcept
{
    return static_cast<base::u64>(std::count_if(diagnostics.begin(), diagnostics.end(),
        [&placeholder](const ParserAdmissionDiagnosticProjectionStub& diagnostic) {
            return fact_belongs_to_part(placeholder.module, placeholder.source_part_index,
                       diagnostic.module, diagnostic.part_index)
                && diagnostic.blocker_category == FRONTEND_MACRO_M21K_EMPTY_BLOCKER_CATEGORY;
        }));
}

[[nodiscard]] query::StableFingerprint128 builtin_derive_release_hardening_matrix_identity(
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveParserConsumptionReleaseGate& release_gate,
    const std::string_view query_name,
    const base::u64 part_local_admission_count,
    const base::u64 part_local_derive_admission_count,
    const base::u64 part_local_semantic_plan_count,
    const base::u64 part_local_release_gate_count,
    const base::u64 global_admission_count,
    const base::u64 global_semantic_plan_count,
    const base::u64 global_generated_part_count,
    const base::u64 cross_part_admission_count,
    const base::u64 cross_part_semantic_plan_count) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M22D_HARDENING_MATRIX_IDENTITY_MARKER);
    builder.mix_u32(placeholder.module.value);
    builder.mix_u32(placeholder.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.source_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    builder.mix_fingerprint(release_gate.release_gate_identity);
    builder.mix_fingerprint(release_gate.admission_group_identity);
    builder.mix_fingerprint(release_gate.semantic_plan_group_identity);
    builder.mix_string(FRONTEND_MACRO_M22D_HARDENING_POLICY);
    builder.mix_string(query_name);
    builder.mix_string(FRONTEND_MACRO_M22D_HARDENING_BLOCKER);
    builder.mix_u64(part_local_admission_count);
    builder.mix_u64(part_local_derive_admission_count);
    builder.mix_u64(part_local_semantic_plan_count);
    builder.mix_u64(part_local_release_gate_count);
    builder.mix_u64(global_admission_count);
    builder.mix_u64(global_semantic_plan_count);
    builder.mix_u64(global_generated_part_count);
    builder.mix_u64(cross_part_admission_count);
    builder.mix_u64(cross_part_semantic_plan_count);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(true);
    builder.mix_bool(true);
    return builder.finish();
}

[[nodiscard]] BuiltinDeriveReleaseHardeningMatrix make_builtin_derive_release_hardening_matrix(
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveParserConsumptionReleaseGate& release_gate,
    const std::vector<GeneratedModulePartPlaceholder>& generated_parts,
    const std::vector<BuiltinDeriveExpansionAdmissionGate>& admissions,
    const std::vector<BuiltinDeriveSemanticExpansionPlan>& plans,
    const std::vector<BuiltinDeriveParserConsumptionReleaseGate>& release_gates)
{
    const base::u64 part_local_admission_count = count_part_local_admissions(placeholder, admissions);
    const base::u64 part_local_derive_admission_count =
        count_part_local_derive_admissions(placeholder, admissions);
    const base::u64 part_local_semantic_plan_count = count_part_local_semantic_plans(placeholder, plans);
    const base::u64 part_local_release_gate_count = count_part_local_parser_release_gates(
        placeholder, release_gates);
    const base::u64 global_admission_count = static_cast<base::u64>(admissions.size());
    const base::u64 global_semantic_plan_count = static_cast<base::u64>(plans.size());
    const base::u64 global_generated_part_count = static_cast<base::u64>(generated_parts.size());
    const base::u64 cross_part_admission_count = global_admission_count - part_local_admission_count;
    const base::u64 cross_part_semantic_plan_count = global_semantic_plan_count - part_local_semantic_plan_count;
    const std::string query_name =
        builtin_derive_release_hardening_query_name(placeholder.module, placeholder.source_part_index);
    return BuiltinDeriveReleaseHardeningMatrix{
        placeholder.module,
        placeholder.source_part_index,
        placeholder.source_part,
        placeholder.generated_part,
        release_gate.release_gate_identity,
        release_gate.admission_group_identity,
        release_gate.semantic_plan_group_identity,
        builtin_derive_release_hardening_matrix_identity(placeholder, release_gate, query_name,
            part_local_admission_count, part_local_derive_admission_count,
            part_local_semantic_plan_count, part_local_release_gate_count,
            global_admission_count, global_semantic_plan_count, global_generated_part_count,
            cross_part_admission_count, cross_part_semantic_plan_count),
        std::string(FRONTEND_MACRO_M22D_HARDENING_POLICY),
        query_name,
        std::string(FRONTEND_MACRO_M22D_HARDENING_BLOCKER),
        part_local_admission_count,
        part_local_derive_admission_count,
        part_local_semantic_plan_count,
        part_local_release_gate_count,
        global_admission_count,
        global_semantic_plan_count,
        global_generated_part_count,
        cross_part_admission_count,
        cross_part_semantic_plan_count,
        true,
        true,
        true,
        true,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        true,
        true,
    };
}

[[nodiscard]] query::StableFingerprint128 builtin_derive_debug_dump_contract_identity(
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveParserConsumptionReleaseGate& release_gate,
    const BuiltinDeriveReleaseHardeningMatrix& matrix,
    const std::string_view query_name) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M22E_DEBUG_DUMP_CONTRACT_IDENTITY_MARKER);
    builder.mix_u32(placeholder.module.value);
    builder.mix_u32(placeholder.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.source_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    builder.mix_fingerprint(release_gate.release_gate_identity);
    builder.mix_fingerprint(matrix.hardening_matrix_identity);
    builder.mix_string(FRONTEND_MACRO_M22E_DEBUG_DUMP_POLICY);
    builder.mix_string(query_name);
    builder.mix_string(FRONTEND_MACRO_M22E_DEBUG_DUMP_BLOCKER);
    builder.mix_u64(FRONTEND_MACRO_M22E_DEBUG_DUMP_SECTION_COUNT);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(true);
    builder.mix_bool(true);
    return builder.finish();
}

[[nodiscard]] BuiltinDeriveDebugDumpStabilityContract make_builtin_derive_debug_dump_stability_contract(
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveParserConsumptionReleaseGate& release_gate,
    const BuiltinDeriveReleaseHardeningMatrix& matrix)
{
    const std::string query_name =
        builtin_derive_debug_dump_query_name(placeholder.module, placeholder.source_part_index);
    return BuiltinDeriveDebugDumpStabilityContract{
        placeholder.module,
        placeholder.source_part_index,
        placeholder.source_part,
        placeholder.generated_part,
        release_gate.release_gate_identity,
        matrix.hardening_matrix_identity,
        builtin_derive_debug_dump_contract_identity(placeholder, release_gate, matrix, query_name),
        std::string(FRONTEND_MACRO_M22E_DEBUG_DUMP_POLICY),
        query_name,
        std::string(FRONTEND_MACRO_M22E_DEBUG_DUMP_BLOCKER),
        FRONTEND_MACRO_M22E_DEBUG_DUMP_SECTION_COUNT,
        true,
        true,
        true,
        true,
        true,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        true,
        true,
    };
}

[[nodiscard]] query::StableFingerprint128 builtin_derive_rollback_diagnostic_gate_identity(
    const GeneratedModulePartPlaceholder& placeholder,
    const GeneratedTokenParserConsumptionContractGate& parser_contract,
    const BuiltinDeriveParserConsumptionReleaseGate& release_gate,
    const BuiltinDeriveReleaseHardeningMatrix& matrix,
    const BuiltinDeriveDebugDumpStabilityContract& debug_contract,
    const std::string_view query_name,
    const base::u64 diagnostic_projection_count,
    const base::u64 diagnostic_report_entry_count,
    const base::u64 blocked_diagnostic_count,
    const base::u64 derive_diagnostic_count,
    const base::u64 empty_diagnostic_count,
    const base::u64 parser_consumption_contract_count) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M22F_ROLLBACK_GATE_IDENTITY_MARKER);
    builder.mix_u32(placeholder.module.value);
    builder.mix_u32(placeholder.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.source_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    builder.mix_fingerprint(parser_contract.contract_identity);
    builder.mix_fingerprint(release_gate.release_gate_identity);
    builder.mix_fingerprint(matrix.hardening_matrix_identity);
    builder.mix_fingerprint(debug_contract.debug_dump_contract_identity);
    builder.mix_string(FRONTEND_MACRO_M22F_ROLLBACK_POLICY);
    builder.mix_string(query_name);
    builder.mix_string(FRONTEND_MACRO_M22F_ROLLBACK_BLOCKER);
    builder.mix_u64(diagnostic_projection_count);
    builder.mix_u64(diagnostic_report_entry_count);
    builder.mix_u64(blocked_diagnostic_count);
    builder.mix_u64(derive_diagnostic_count);
    builder.mix_u64(empty_diagnostic_count);
    builder.mix_u64(parser_consumption_contract_count);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(true);
    builder.mix_bool(true);
    return builder.finish();
}

[[nodiscard]] BuiltinDeriveRollbackDiagnosticDesignGate make_builtin_derive_rollback_diagnostic_design_gate(
    const GeneratedModulePartPlaceholder& placeholder,
    const GeneratedTokenParserConsumptionContractGate& parser_contract,
    const BuiltinDeriveParserConsumptionReleaseGate& release_gate,
    const BuiltinDeriveReleaseHardeningMatrix& matrix,
    const BuiltinDeriveDebugDumpStabilityContract& debug_contract,
    const std::vector<ParserAdmissionDiagnosticProjectionStub>& diagnostics,
    const std::vector<ParserAdmissionDiagnosticReportEntry>& report_entries)
{
    const base::u64 diagnostic_projection_count = count_part_local_diagnostics(placeholder, diagnostics);
    const base::u64 diagnostic_report_entry_count = count_part_local_report_entries(placeholder, report_entries);
    const base::u64 blocked_diagnostic_count = count_part_local_blocked_diagnostics(placeholder, diagnostics);
    const base::u64 derive_diagnostic_count = count_part_local_derive_diagnostics(placeholder, diagnostics);
    const base::u64 empty_diagnostic_count = count_part_local_empty_diagnostics(placeholder, diagnostics);
    const base::u64 parser_consumption_contract_count = parser_contract.contract_visible ? 1U : 0U;
    const std::string query_name =
        builtin_derive_rollback_diagnostic_query_name(placeholder.module, placeholder.source_part_index);
    return BuiltinDeriveRollbackDiagnosticDesignGate{
        placeholder.module,
        placeholder.source_part_index,
        placeholder.source_part,
        placeholder.generated_part,
        parser_contract.contract_identity,
        release_gate.release_gate_identity,
        matrix.hardening_matrix_identity,
        debug_contract.debug_dump_contract_identity,
        builtin_derive_rollback_diagnostic_gate_identity(placeholder, parser_contract, release_gate, matrix,
            debug_contract, query_name, diagnostic_projection_count, diagnostic_report_entry_count,
            blocked_diagnostic_count, derive_diagnostic_count, empty_diagnostic_count,
            parser_consumption_contract_count),
        std::string(FRONTEND_MACRO_M22F_ROLLBACK_POLICY),
        query_name,
        std::string(FRONTEND_MACRO_M22F_ROLLBACK_BLOCKER),
        diagnostic_projection_count,
        diagnostic_report_entry_count,
        blocked_diagnostic_count,
        derive_diagnostic_count,
        empty_diagnostic_count,
        parser_consumption_contract_count,
        true,
        true,
        true,
        true,
        true,
        true,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        true,
        true,
    };
}

[[nodiscard]] base::u64 count_part_local_token_buffers(
    const GeneratedModulePartPlaceholder& placeholder,
    const std::vector<GeneratedTokenBufferStub>& buffers) noexcept
{
    return static_cast<base::u64>(std::count_if(buffers.begin(), buffers.end(),
        [&placeholder](const GeneratedTokenBufferStub& buffer) {
            return fact_belongs_to_part(placeholder.module, placeholder.source_part_index,
                buffer.module, buffer.part_index);
        }));
}

[[nodiscard]] base::u64 count_part_local_token_records(
    const GeneratedModulePartPlaceholder& placeholder,
    const std::vector<GeneratedTokenRecord>& records) noexcept
{
    return static_cast<base::u64>(std::count_if(records.begin(), records.end(),
        [&placeholder](const GeneratedTokenRecord& record) {
            return fact_belongs_to_part(placeholder.module, placeholder.source_part_index,
                record.module, record.part_index);
        }));
}

[[nodiscard]] base::u64 count_part_local_derive_candidate_buffers(
    const GeneratedModulePartPlaceholder& placeholder,
    const std::vector<GeneratedTokenBufferStub>& buffers) noexcept
{
    return static_cast<base::u64>(std::count_if(buffers.begin(), buffers.end(),
        [&placeholder](const GeneratedTokenBufferStub& buffer) {
            return fact_belongs_to_part(placeholder.module, placeholder.source_part_index,
                       buffer.module, buffer.part_index)
                && buffer.token_buffer_kind == FRONTEND_MACRO_M21I_DERIVE_TOKEN_BUFFER_KIND;
        }));
}

[[nodiscard]] base::u64 count_part_local_empty_candidate_buffers(
    const GeneratedModulePartPlaceholder& placeholder,
    const std::vector<GeneratedTokenBufferStub>& buffers) noexcept
{
    return static_cast<base::u64>(std::count_if(buffers.begin(), buffers.end(),
        [&placeholder](const GeneratedTokenBufferStub& buffer) {
            return fact_belongs_to_part(placeholder.module, placeholder.source_part_index,
                       buffer.module, buffer.part_index)
                && buffer.token_buffer_kind == FRONTEND_MACRO_M21H_TOKEN_BUFFER_KIND;
        }));
}

[[nodiscard]] query::StableFingerprint128 builtin_derive_parser_consumption_admission_protocol_identity(
    const GeneratedModulePartPlaceholder& placeholder,
    const GeneratedTokenParserConsumptionContractGate& parser_contract,
    const BuiltinDeriveParserConsumptionReleaseGate& release_gate,
    const BuiltinDeriveRollbackDiagnosticDesignGate& rollback_gate,
    const std::string_view query_name,
    const base::u64 token_buffer_count,
    const base::u64 token_record_count,
    const base::u64 derive_candidate_count,
    const base::u64 empty_candidate_count,
    const base::u64 blocked_diagnostic_count) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M23A_ADMISSION_PROTOCOL_IDENTITY_MARKER);
    builder.mix_u32(placeholder.module.value);
    builder.mix_u32(placeholder.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.source_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    builder.mix_fingerprint(parser_contract.contract_identity);
    builder.mix_fingerprint(release_gate.release_gate_identity);
    builder.mix_fingerprint(rollback_gate.rollback_gate_identity);
    builder.mix_string(FRONTEND_MACRO_M23A_ADMISSION_POLICY);
    builder.mix_string(query_name);
    builder.mix_string(FRONTEND_MACRO_M23A_ADMISSION_BLOCKER);
    builder.mix_u64(token_buffer_count);
    builder.mix_u64(token_record_count);
    builder.mix_u64(derive_candidate_count);
    builder.mix_u64(empty_candidate_count);
    builder.mix_u64(blocked_diagnostic_count);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(true);
    builder.mix_bool(true);
    return builder.finish();
}

[[nodiscard]] BuiltinDeriveParserConsumptionAdmissionProtocol
make_builtin_derive_parser_consumption_admission_protocol(
    const GeneratedModulePartPlaceholder& placeholder,
    const GeneratedTokenParserConsumptionContractGate& parser_contract,
    const BuiltinDeriveParserConsumptionReleaseGate& release_gate,
    const BuiltinDeriveRollbackDiagnosticDesignGate& rollback_gate,
    const std::vector<GeneratedTokenBufferStub>& buffers,
    const std::vector<GeneratedTokenRecord>& records,
    const std::vector<ParserAdmissionDiagnosticProjectionStub>& diagnostics)
{
    const base::u64 token_buffer_count = count_part_local_token_buffers(placeholder, buffers);
    const base::u64 token_record_count = count_part_local_token_records(placeholder, records);
    const base::u64 derive_candidate_count = count_part_local_derive_candidate_buffers(placeholder, buffers);
    const base::u64 empty_candidate_count = count_part_local_empty_candidate_buffers(placeholder, buffers);
    const base::u64 blocked_diagnostic_count = count_part_local_blocked_diagnostics(placeholder, diagnostics);
    const std::string query_name =
        builtin_derive_parser_consumption_admission_query_name(placeholder.module,
            placeholder.source_part_index);
    return BuiltinDeriveParserConsumptionAdmissionProtocol{
        placeholder.module,
        placeholder.source_part_index,
        placeholder.source_part,
        placeholder.generated_part,
        parser_contract.contract_identity,
        release_gate.release_gate_identity,
        rollback_gate.rollback_gate_identity,
        builtin_derive_parser_consumption_admission_protocol_identity(placeholder, parser_contract,
            release_gate, rollback_gate, query_name, token_buffer_count, token_record_count,
            derive_candidate_count, empty_candidate_count, blocked_diagnostic_count),
        std::string(FRONTEND_MACRO_M23A_ADMISSION_POLICY),
        query_name,
        std::string(FRONTEND_MACRO_M23A_ADMISSION_BLOCKER),
        token_buffer_count,
        token_record_count,
        derive_candidate_count,
        empty_candidate_count,
        blocked_diagnostic_count,
        true,
        true,
        true,
        true,
        true,
        true,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        true,
        true,
    };
}

[[nodiscard]] query::StableFingerprint128 builtin_derive_checkpoint_rollback_protocol_identity(
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveParserConsumptionAdmissionProtocol& admission_protocol,
    const BuiltinDeriveRollbackDiagnosticDesignGate& rollback_gate,
    const std::string_view query_name,
    const base::u64 checkpoint_count,
    const base::u64 rollback_plan_count,
    const base::u64 token_record_count,
    const base::u64 diagnostic_anchor_count) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M23B_CHECKPOINT_PROTOCOL_IDENTITY_MARKER);
    builder.mix_u32(placeholder.module.value);
    builder.mix_u32(placeholder.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.source_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    builder.mix_fingerprint(admission_protocol.admission_protocol_identity);
    builder.mix_fingerprint(rollback_gate.rollback_gate_identity);
    builder.mix_string(FRONTEND_MACRO_M23B_CHECKPOINT_POLICY);
    builder.mix_string(query_name);
    builder.mix_string(FRONTEND_MACRO_M23B_CHECKPOINT_BLOCKER);
    builder.mix_u64(checkpoint_count);
    builder.mix_u64(rollback_plan_count);
    builder.mix_u64(token_record_count);
    builder.mix_u64(diagnostic_anchor_count);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(true);
    builder.mix_bool(true);
    return builder.finish();
}

[[nodiscard]] BuiltinDeriveParserConsumptionCheckpointRollbackProtocol
make_builtin_derive_checkpoint_rollback_protocol(
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveParserConsumptionAdmissionProtocol& admission_protocol,
    const BuiltinDeriveRollbackDiagnosticDesignGate& rollback_gate)
{
    const base::u64 checkpoint_count = FRONTEND_MACRO_M23B_CHECKPOINT_PLAN_COUNT;
    const base::u64 rollback_plan_count = FRONTEND_MACRO_M23B_CHECKPOINT_PLAN_COUNT;
    const base::u64 token_record_count = admission_protocol.token_record_count;
    const base::u64 diagnostic_anchor_count = admission_protocol.blocked_diagnostic_count;
    const std::string query_name =
        builtin_derive_checkpoint_rollback_query_name(placeholder.module, placeholder.source_part_index);
    return BuiltinDeriveParserConsumptionCheckpointRollbackProtocol{
        placeholder.module,
        placeholder.source_part_index,
        placeholder.source_part,
        placeholder.generated_part,
        admission_protocol.admission_protocol_identity,
        rollback_gate.rollback_gate_identity,
        builtin_derive_checkpoint_rollback_protocol_identity(placeholder, admission_protocol, rollback_gate,
            query_name, checkpoint_count, rollback_plan_count, token_record_count, diagnostic_anchor_count),
        std::string(FRONTEND_MACRO_M23B_CHECKPOINT_POLICY),
        query_name,
        std::string(FRONTEND_MACRO_M23B_CHECKPOINT_BLOCKER),
        checkpoint_count,
        rollback_plan_count,
        token_record_count,
        diagnostic_anchor_count,
        true,
        true,
        true,
        true,
        true,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        true,
        true,
    };
}

[[nodiscard]] query::StableFingerprint128 builtin_derive_preconsumption_verification_closure_identity(
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveParserConsumptionAdmissionProtocol& admission_protocol,
    const BuiltinDeriveParserConsumptionCheckpointRollbackProtocol& checkpoint_protocol,
    const BuiltinDeriveDebugDumpStabilityContract& debug_contract,
    const std::string_view query_name,
    const base::u64 admission_protocol_count,
    const base::u64 checkpoint_protocol_count,
    const base::u64 hardening_matrix_count,
    const base::u64 debug_dump_contract_count,
    const base::u64 rollback_gate_count) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M23C_VERIFICATION_CLOSURE_IDENTITY_MARKER);
    builder.mix_u32(placeholder.module.value);
    builder.mix_u32(placeholder.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.source_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    builder.mix_fingerprint(admission_protocol.admission_protocol_identity);
    builder.mix_fingerprint(checkpoint_protocol.checkpoint_protocol_identity);
    builder.mix_fingerprint(debug_contract.debug_dump_contract_identity);
    builder.mix_string(FRONTEND_MACRO_M23C_VERIFICATION_POLICY);
    builder.mix_string(query_name);
    builder.mix_string(FRONTEND_MACRO_M23C_VERIFICATION_BLOCKER);
    builder.mix_u64(admission_protocol_count);
    builder.mix_u64(checkpoint_protocol_count);
    builder.mix_u64(hardening_matrix_count);
    builder.mix_u64(debug_dump_contract_count);
    builder.mix_u64(rollback_gate_count);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(true);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(false);
    builder.mix_bool(true);
    builder.mix_bool(true);
    return builder.finish();
}

[[nodiscard]] BuiltinDeriveParserPreConsumptionVerificationClosure
make_builtin_derive_preconsumption_verification_closure(
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveParserConsumptionAdmissionProtocol& admission_protocol,
    const BuiltinDeriveParserConsumptionCheckpointRollbackProtocol& checkpoint_protocol,
    const BuiltinDeriveReleaseHardeningMatrix& matrix,
    const BuiltinDeriveDebugDumpStabilityContract& debug_contract,
    const BuiltinDeriveRollbackDiagnosticDesignGate& rollback_gate)
{
    const base::u64 admission_protocol_count = admission_protocol.protocol_visible ? 1U : 0U;
    const base::u64 checkpoint_protocol_count = checkpoint_protocol.protocol_visible ? 1U : 0U;
    const base::u64 hardening_matrix_count = matrix.matrix_visible ? 1U : 0U;
    const base::u64 debug_dump_contract_count = debug_contract.contract_visible ? 1U : 0U;
    const base::u64 rollback_gate_count = rollback_gate.rollback_gate_visible ? 1U : 0U;
    const std::string query_name =
        builtin_derive_preconsumption_verification_query_name(placeholder.module,
            placeholder.source_part_index);
    return BuiltinDeriveParserPreConsumptionVerificationClosure{
        placeholder.module,
        placeholder.source_part_index,
        placeholder.source_part,
        placeholder.generated_part,
        admission_protocol.admission_protocol_identity,
        checkpoint_protocol.checkpoint_protocol_identity,
        debug_contract.debug_dump_contract_identity,
        builtin_derive_preconsumption_verification_closure_identity(placeholder, admission_protocol,
            checkpoint_protocol, debug_contract, query_name, admission_protocol_count,
            checkpoint_protocol_count, hardening_matrix_count, debug_dump_contract_count, rollback_gate_count),
        std::string(FRONTEND_MACRO_M23C_VERIFICATION_POLICY),
        query_name,
        std::string(FRONTEND_MACRO_M23C_VERIFICATION_BLOCKER),
        admission_protocol_count,
        checkpoint_protocol_count,
        hardening_matrix_count,
        debug_dump_contract_count,
        rollback_gate_count,
        true,
        true,
        true,
        true,
        true,
        true,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        true,
        true,
    };
}

[[nodiscard]] query::StableFingerprint128 builtin_derive_controlled_dry_run_adapter_identity(
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveParserPreConsumptionVerificationClosure& verification_closure,
    const BuiltinDeriveParserConsumptionAdmissionProtocol& admission_protocol,
    const BuiltinDeriveParserConsumptionCheckpointRollbackProtocol& checkpoint_protocol,
    const std::string_view query_name,
    const base::u64 token_record_count,
    const base::u64 diagnostic_anchor_count,
    const base::u64 prerequisite_count) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M24A_DRY_RUN_ADAPTER_IDENTITY_MARKER);
    builder.mix_u32(placeholder.module.value);
    builder.mix_u32(placeholder.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.source_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    builder.mix_fingerprint(verification_closure.verification_closure_identity);
    builder.mix_fingerprint(admission_protocol.admission_protocol_identity);
    builder.mix_fingerprint(checkpoint_protocol.checkpoint_protocol_identity);
    builder.mix_string(FRONTEND_MACRO_M24A_DRY_RUN_ADAPTER_POLICY);
    builder.mix_string(query_name);
    builder.mix_string(FRONTEND_MACRO_M24A_DRY_RUN_ADAPTER_BLOCKER);
    builder.mix_u64(token_record_count);
    builder.mix_u64(diagnostic_anchor_count);
    builder.mix_u64(prerequisite_count);
    builder.mix_bool(true);  // verification_closure_available
    builder.mix_bool(true);  // admission_protocol_available
    builder.mix_bool(true);  // checkpoint_protocol_available
    builder.mix_bool(true);  // compiler_owned_tokens_available
    builder.mix_bool(true);  // diagnostic_replay_available
    builder.mix_bool(true);  // dry_run_adapter_complete
    builder.mix_bool(false); // dry_run_executed
    builder.mix_bool(false); // parser_consumption_enabled
    builder.mix_bool(false); // parser_admitted
    builder.mix_bool(false); // generated_part_parsed
    builder.mix_bool(false); // generated_part_merged
    builder.mix_bool(false); // sema_visible
    builder.mix_bool(false); // emit_expanded_available
    builder.mix_bool(false); // debug_trace_available
    builder.mix_bool(false); // source_map_available
    builder.mix_bool(false); // standard_library_required
    builder.mix_bool(false); // runtime_required
    builder.mix_bool(false); // external_process_required
    builder.mix_bool(false); // produced_user_generated_code
    builder.mix_bool(true);  // adapter_visible
    builder.mix_bool(true);  // query_reusable
    return builder.finish();
}

[[nodiscard]] BuiltinDeriveControlledParserDryRunAdapter
make_builtin_derive_controlled_dry_run_adapter(
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveParserPreConsumptionVerificationClosure& verification_closure,
    const BuiltinDeriveParserConsumptionAdmissionProtocol& admission_protocol,
    const BuiltinDeriveParserConsumptionCheckpointRollbackProtocol& checkpoint_protocol)
{
    const base::u64 token_record_count = checkpoint_protocol.token_record_count;
    const base::u64 diagnostic_anchor_count = checkpoint_protocol.diagnostic_anchor_count;
    const base::u64 prerequisite_count = FRONTEND_MACRO_M24A_DRY_RUN_PREREQUISITE_COUNT;
    const std::string query_name =
        builtin_derive_controlled_dry_run_adapter_query_name(placeholder.module,
            placeholder.source_part_index);
    BuiltinDeriveControlledParserDryRunAdapter adapter;
    adapter.module = placeholder.module;
    adapter.source_part_index = placeholder.source_part_index;
    adapter.attached_part = placeholder.source_part;
    adapter.generated_part = placeholder.generated_part;
    adapter.verification_closure_identity = verification_closure.verification_closure_identity;
    adapter.admission_protocol_identity = admission_protocol.admission_protocol_identity;
    adapter.checkpoint_protocol_identity = checkpoint_protocol.checkpoint_protocol_identity;
    adapter.dry_run_adapter_identity = builtin_derive_controlled_dry_run_adapter_identity(
        placeholder, verification_closure, admission_protocol, checkpoint_protocol, query_name,
        token_record_count, diagnostic_anchor_count, prerequisite_count);
    adapter.adapter_policy = std::string(FRONTEND_MACRO_M24A_DRY_RUN_ADAPTER_POLICY);
    adapter.adapter_query_name = query_name;
    adapter.blocked_reason = std::string(FRONTEND_MACRO_M24A_DRY_RUN_ADAPTER_BLOCKER);
    adapter.token_record_count = token_record_count;
    adapter.diagnostic_anchor_count = diagnostic_anchor_count;
    adapter.prerequisite_count = prerequisite_count;
    return adapter;
}

[[nodiscard]] query::StableFingerprint128 builtin_derive_dry_run_rollback_replay_identity(
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveControlledParserDryRunAdapter& adapter,
    const BuiltinDeriveParserConsumptionCheckpointRollbackProtocol& checkpoint_protocol,
    const BuiltinDeriveRollbackDiagnosticDesignGate& rollback_gate,
    const std::string_view query_name,
    const base::u64 diagnostic_anchor_count,
    const base::u64 report_entry_count,
    const base::u64 planned_replay_count,
    const base::u64 executed_replay_count) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M24B_ROLLBACK_REPLAY_IDENTITY_MARKER);
    builder.mix_u32(placeholder.module.value);
    builder.mix_u32(placeholder.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.source_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    builder.mix_fingerprint(adapter.dry_run_adapter_identity);
    builder.mix_fingerprint(checkpoint_protocol.checkpoint_protocol_identity);
    builder.mix_fingerprint(rollback_gate.rollback_gate_identity);
    builder.mix_string(FRONTEND_MACRO_M24B_ROLLBACK_REPLAY_POLICY);
    builder.mix_string(query_name);
    builder.mix_string(FRONTEND_MACRO_M24B_ROLLBACK_REPLAY_BLOCKER);
    builder.mix_u64(diagnostic_anchor_count);
    builder.mix_u64(report_entry_count);
    builder.mix_u64(planned_replay_count);
    builder.mix_u64(executed_replay_count);
    builder.mix_bool(true);  // dry_run_adapter_available
    builder.mix_bool(true);  // checkpoint_protocol_available
    builder.mix_bool(true);  // rollback_gate_available
    builder.mix_bool(true);  // diagnostic_replay_plan_available
    builder.mix_bool(true);  // replay_protocol_complete
    builder.mix_bool(false); // replay_execution_enabled
    builder.mix_bool(false); // dry_run_executed
    builder.mix_bool(false); // parser_consumption_enabled
    builder.mix_bool(false); // generated_part_parsed
    builder.mix_bool(false); // generated_part_merged
    builder.mix_bool(false); // sema_visible
    builder.mix_bool(false); // emit_expanded_available
    builder.mix_bool(false); // debug_trace_available
    builder.mix_bool(false); // source_map_available
    builder.mix_bool(false); // standard_library_required
    builder.mix_bool(false); // runtime_required
    builder.mix_bool(false); // external_process_required
    builder.mix_bool(false); // produced_user_generated_code
    builder.mix_bool(true);  // replay_visible
    builder.mix_bool(true);  // query_reusable
    return builder.finish();
}

[[nodiscard]] BuiltinDeriveDryRunRollbackDiagnosticReplay
make_builtin_derive_dry_run_rollback_replay(
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveControlledParserDryRunAdapter& adapter,
    const BuiltinDeriveParserConsumptionCheckpointRollbackProtocol& checkpoint_protocol,
    const BuiltinDeriveRollbackDiagnosticDesignGate& rollback_gate)
{
    const base::u64 diagnostic_anchor_count = adapter.diagnostic_anchor_count;
    const base::u64 report_entry_count = rollback_gate.diagnostic_report_entry_count;
    const base::u64 planned_replay_count = diagnostic_anchor_count;
    const base::u64 executed_replay_count = 0U;
    const std::string query_name =
        builtin_derive_dry_run_rollback_replay_query_name(placeholder.module,
            placeholder.source_part_index);
    BuiltinDeriveDryRunRollbackDiagnosticReplay replay;
    replay.module = placeholder.module;
    replay.source_part_index = placeholder.source_part_index;
    replay.attached_part = placeholder.source_part;
    replay.generated_part = placeholder.generated_part;
    replay.dry_run_adapter_identity = adapter.dry_run_adapter_identity;
    replay.checkpoint_protocol_identity = checkpoint_protocol.checkpoint_protocol_identity;
    replay.rollback_gate_identity = rollback_gate.rollback_gate_identity;
    replay.replay_protocol_identity = builtin_derive_dry_run_rollback_replay_identity(
        placeholder, adapter, checkpoint_protocol, rollback_gate, query_name, diagnostic_anchor_count,
        report_entry_count, planned_replay_count, executed_replay_count);
    replay.replay_policy = std::string(FRONTEND_MACRO_M24B_ROLLBACK_REPLAY_POLICY);
    replay.replay_query_name = query_name;
    replay.blocked_reason = std::string(FRONTEND_MACRO_M24B_ROLLBACK_REPLAY_BLOCKER);
    replay.diagnostic_anchor_count = diagnostic_anchor_count;
    replay.report_entry_count = report_entry_count;
    replay.planned_replay_count = planned_replay_count;
    replay.executed_replay_count = executed_replay_count;
    return replay;
}

[[nodiscard]] query::StableFingerprint128 builtin_derive_dry_run_negative_matrix_identity(
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveControlledParserDryRunAdapter& adapter,
    const BuiltinDeriveDryRunRollbackDiagnosticReplay& replay,
    const BuiltinDeriveParserPreConsumptionVerificationClosure& verification_closure,
    const std::string_view query_name,
    const base::u64 dry_run_adapter_count,
    const base::u64 rollback_replay_count,
    const base::u64 verification_closure_count,
    const base::u64 negative_case_count,
    const base::u64 parser_consumable_case_count) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M24C_NEGATIVE_MATRIX_IDENTITY_MARKER);
    builder.mix_u32(placeholder.module.value);
    builder.mix_u32(placeholder.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.source_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    builder.mix_fingerprint(adapter.dry_run_adapter_identity);
    builder.mix_fingerprint(replay.replay_protocol_identity);
    builder.mix_fingerprint(verification_closure.verification_closure_identity);
    builder.mix_string(FRONTEND_MACRO_M24C_NEGATIVE_MATRIX_POLICY);
    builder.mix_string(query_name);
    builder.mix_string(FRONTEND_MACRO_M24C_NEGATIVE_MATRIX_BLOCKER);
    builder.mix_u64(dry_run_adapter_count);
    builder.mix_u64(rollback_replay_count);
    builder.mix_u64(verification_closure_count);
    builder.mix_u64(negative_case_count);
    builder.mix_u64(parser_consumable_case_count);
    builder.mix_bool(true);  // dry_run_adapter_available
    builder.mix_bool(true);  // rollback_replay_available
    builder.mix_bool(true);  // verification_closure_available
    builder.mix_bool(true);  // negative_matrix_complete
    builder.mix_bool(false); // dry_run_executed
    builder.mix_bool(false); // parser_consumption_enabled
    builder.mix_bool(false); // parser_admitted
    builder.mix_bool(false); // generated_part_parsed
    builder.mix_bool(false); // generated_part_merged
    builder.mix_bool(false); // sema_visible
    builder.mix_bool(false); // emit_expanded_available
    builder.mix_bool(false); // debug_trace_available
    builder.mix_bool(false); // source_map_available
    builder.mix_bool(false); // standard_library_required
    builder.mix_bool(false); // runtime_required
    builder.mix_bool(false); // external_process_required
    builder.mix_bool(false); // produced_user_generated_code
    builder.mix_bool(true);  // matrix_visible
    builder.mix_bool(true);  // query_reusable
    return builder.finish();
}

[[nodiscard]] BuiltinDeriveDryRunNegativeMatrixClosure make_builtin_derive_dry_run_negative_matrix(
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveControlledParserDryRunAdapter& adapter,
    const BuiltinDeriveDryRunRollbackDiagnosticReplay& replay,
    const BuiltinDeriveParserPreConsumptionVerificationClosure& verification_closure)
{
    const base::u64 dry_run_adapter_count = adapter.adapter_visible ? 1U : 0U;
    const base::u64 rollback_replay_count = replay.replay_visible ? 1U : 0U;
    const base::u64 verification_closure_count = verification_closure.closure_visible ? 1U : 0U;
    const base::u64 negative_case_count = FRONTEND_MACRO_M24C_NEGATIVE_CASE_COUNT;
    const base::u64 parser_consumable_case_count = 0U;
    const std::string query_name =
        builtin_derive_dry_run_negative_matrix_query_name(placeholder.module,
            placeholder.source_part_index);
    BuiltinDeriveDryRunNegativeMatrixClosure matrix;
    matrix.module = placeholder.module;
    matrix.source_part_index = placeholder.source_part_index;
    matrix.attached_part = placeholder.source_part;
    matrix.generated_part = placeholder.generated_part;
    matrix.dry_run_adapter_identity = adapter.dry_run_adapter_identity;
    matrix.rollback_replay_identity = replay.replay_protocol_identity;
    matrix.verification_closure_identity = verification_closure.verification_closure_identity;
    matrix.negative_matrix_identity = builtin_derive_dry_run_negative_matrix_identity(
        placeholder, adapter, replay, verification_closure, query_name, dry_run_adapter_count,
        rollback_replay_count, verification_closure_count, negative_case_count,
        parser_consumable_case_count);
    matrix.matrix_policy = std::string(FRONTEND_MACRO_M24C_NEGATIVE_MATRIX_POLICY);
    matrix.matrix_query_name = query_name;
    matrix.blocked_reason = std::string(FRONTEND_MACRO_M24C_NEGATIVE_MATRIX_BLOCKER);
    matrix.dry_run_adapter_count = dry_run_adapter_count;
    matrix.rollback_replay_count = rollback_replay_count;
    matrix.verification_closure_count = verification_closure_count;
    matrix.negative_case_count = negative_case_count;
    matrix.parser_consumable_case_count = parser_consumable_case_count;
    return matrix;
}

[[nodiscard]] query::StableFingerprint128 builtin_derive_parser_dry_run_session_identity(
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveControlledParserDryRunAdapter& adapter,
    const BuiltinDeriveDryRunNegativeMatrixClosure& matrix,
    const GeneratedModulePartParseMergeStub& parse_merge_stub,
    const std::string_view query_name,
    const base::u64 token_buffer_candidate_count,
    const base::u64 token_record_count,
    const base::u64 diagnostic_anchor_count,
    const base::u64 parser_state_snapshot_count,
    const base::u64 committed_parse_count) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M25A_DRY_RUN_SESSION_IDENTITY_MARKER);
    builder.mix_u32(placeholder.module.value);
    builder.mix_u32(placeholder.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.source_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    builder.mix_fingerprint(adapter.dry_run_adapter_identity);
    builder.mix_fingerprint(matrix.negative_matrix_identity);
    builder.mix_fingerprint(parse_merge_stub.generated_buffer_identity);
    builder.mix_fingerprint(parse_merge_stub.parse_config_fingerprint);
    builder.mix_string(FRONTEND_MACRO_M25A_DRY_RUN_SESSION_POLICY);
    builder.mix_string(query_name);
    builder.mix_string(FRONTEND_MACRO_M25A_DRY_RUN_SESSION_BLOCKER);
    builder.mix_u64(token_buffer_candidate_count);
    builder.mix_u64(token_record_count);
    builder.mix_u64(diagnostic_anchor_count);
    builder.mix_u64(parser_state_snapshot_count);
    builder.mix_u64(committed_parse_count);
    builder.mix_bool(true);  // dry_run_adapter_available
    builder.mix_bool(true);  // negative_matrix_available
    builder.mix_bool(true);  // compiler_owned_token_stream_available
    builder.mix_bool(true);  // sandbox_available
    builder.mix_bool(true);  // check_only
    builder.mix_bool(true);  // dry_run_session_complete
    builder.mix_bool(false); // dry_run_executed
    builder.mix_bool(false); // session_committed
    builder.mix_bool(false); // parser_consumption_enabled
    builder.mix_bool(false); // parser_admitted
    builder.mix_bool(false); // parser_cursor_advanced
    builder.mix_bool(false); // generated_part_parsed
    builder.mix_bool(false); // generated_part_merged
    builder.mix_bool(false); // ast_mutated
    builder.mix_bool(false); // sema_visible
    builder.mix_bool(false); // emit_expanded_available
    builder.mix_bool(false); // debug_trace_available
    builder.mix_bool(false); // source_map_available
    builder.mix_bool(false); // standard_library_required
    builder.mix_bool(false); // runtime_required
    builder.mix_bool(false); // external_process_required
    builder.mix_bool(false); // produced_user_generated_code
    builder.mix_bool(true);  // session_visible
    builder.mix_bool(true);  // query_reusable
    return builder.finish();
}

[[nodiscard]] BuiltinDeriveParserDryRunSessionBoundary
make_builtin_derive_parser_dry_run_session(
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveControlledParserDryRunAdapter& adapter,
    const BuiltinDeriveDryRunNegativeMatrixClosure& matrix,
    const GeneratedModulePartParseMergeStub& parse_merge_stub)
{
    const base::u64 token_buffer_candidate_count = adapter.token_record_count > 0U ? 1U : 0U;
    const base::u64 token_record_count = adapter.token_record_count;
    const base::u64 diagnostic_anchor_count = adapter.diagnostic_anchor_count;
    const base::u64 parser_state_snapshot_count = FRONTEND_MACRO_M25A_PARSER_STATE_SNAPSHOT_COUNT;
    const base::u64 committed_parse_count = 0U;
    const std::string query_name =
        builtin_derive_parser_dry_run_session_query_name(placeholder.module,
            placeholder.source_part_index);
    BuiltinDeriveParserDryRunSessionBoundary session;
    session.module = placeholder.module;
    session.source_part_index = placeholder.source_part_index;
    session.attached_part = placeholder.source_part;
    session.generated_part = placeholder.generated_part;
    session.dry_run_adapter_identity = adapter.dry_run_adapter_identity;
    session.negative_matrix_identity = matrix.negative_matrix_identity;
    session.generated_buffer_identity = parse_merge_stub.generated_buffer_identity;
    session.parse_config_fingerprint = parse_merge_stub.parse_config_fingerprint;
    session.dry_run_session_identity = builtin_derive_parser_dry_run_session_identity(
        placeholder, adapter, matrix, parse_merge_stub, query_name, token_buffer_candidate_count,
        token_record_count, diagnostic_anchor_count, parser_state_snapshot_count, committed_parse_count);
    session.session_policy = std::string(FRONTEND_MACRO_M25A_DRY_RUN_SESSION_POLICY);
    session.session_query_name = query_name;
    session.blocked_reason = std::string(FRONTEND_MACRO_M25A_DRY_RUN_SESSION_BLOCKER);
    session.token_buffer_candidate_count = token_buffer_candidate_count;
    session.token_record_count = token_record_count;
    session.diagnostic_anchor_count = diagnostic_anchor_count;
    session.parser_state_snapshot_count = parser_state_snapshot_count;
    session.committed_parse_count = committed_parse_count;
    return session;
}

[[nodiscard]] query::StableFingerprint128 builtin_derive_token_cursor_snapshot_identity(
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveParserDryRunSessionBoundary& session,
    const BuiltinDeriveParserConsumptionCheckpointRollbackProtocol& checkpoint_protocol,
    const BuiltinDeriveDryRunRollbackDiagnosticReplay& replay,
    const std::string_view query_name,
    const base::u64 token_record_count,
    const base::u64 checkpoint_count,
    const base::u64 cursor_snapshot_count,
    const base::u64 parser_state_snapshot_count,
    const base::u64 rollback_proof_count,
    const base::u64 cursor_commit_count) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M25B_CURSOR_SNAPSHOT_IDENTITY_MARKER);
    builder.mix_u32(placeholder.module.value);
    builder.mix_u32(placeholder.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.source_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    builder.mix_fingerprint(session.dry_run_session_identity);
    builder.mix_fingerprint(checkpoint_protocol.checkpoint_protocol_identity);
    builder.mix_fingerprint(replay.replay_protocol_identity);
    builder.mix_string(FRONTEND_MACRO_M25B_CURSOR_SNAPSHOT_POLICY);
    builder.mix_string(query_name);
    builder.mix_string(FRONTEND_MACRO_M25B_CURSOR_SNAPSHOT_BLOCKER);
    builder.mix_u64(token_record_count);
    builder.mix_u64(checkpoint_count);
    builder.mix_u64(cursor_snapshot_count);
    builder.mix_u64(parser_state_snapshot_count);
    builder.mix_u64(rollback_proof_count);
    builder.mix_u64(cursor_commit_count);
    builder.mix_bool(true);  // dry_run_session_available
    builder.mix_bool(true);  // checkpoint_protocol_available
    builder.mix_bool(true);  // rollback_replay_available
    builder.mix_bool(true);  // token_cursor_snapshot_available
    builder.mix_bool(true);  // parser_state_snapshot_available
    builder.mix_bool(true);  // rollback_proof_complete
    builder.mix_bool(false); // replay_execution_enabled
    builder.mix_bool(false); // rollback_execution_enabled
    builder.mix_bool(false); // dry_run_executed
    builder.mix_bool(false); // parser_cursor_advanced
    builder.mix_bool(false); // session_committed
    builder.mix_bool(false); // parser_consumption_enabled
    builder.mix_bool(false); // parser_admitted
    builder.mix_bool(false); // generated_part_parsed
    builder.mix_bool(false); // generated_part_merged
    builder.mix_bool(false); // ast_mutated
    builder.mix_bool(false); // sema_visible
    builder.mix_bool(false); // standard_library_required
    builder.mix_bool(false); // runtime_required
    builder.mix_bool(false); // external_process_required
    builder.mix_bool(false); // produced_user_generated_code
    builder.mix_bool(true);  // proof_visible
    builder.mix_bool(true);  // query_reusable
    return builder.finish();
}

[[nodiscard]] BuiltinDeriveTokenCursorSnapshotRollbackProof
make_builtin_derive_token_cursor_snapshot_proof(
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveParserDryRunSessionBoundary& session,
    const BuiltinDeriveParserConsumptionCheckpointRollbackProtocol& checkpoint_protocol,
    const BuiltinDeriveDryRunRollbackDiagnosticReplay& replay)
{
    const base::u64 token_record_count = session.token_record_count;
    const base::u64 checkpoint_count = checkpoint_protocol.checkpoint_count;
    const base::u64 cursor_snapshot_count = checkpoint_protocol.checkpoint_count;
    const base::u64 parser_state_snapshot_count = checkpoint_protocol.checkpoint_count;
    const base::u64 rollback_proof_count = checkpoint_protocol.rollback_plan_count;
    const base::u64 cursor_commit_count = 0U;
    const std::string query_name =
        builtin_derive_token_cursor_snapshot_query_name(placeholder.module,
            placeholder.source_part_index);
    BuiltinDeriveTokenCursorSnapshotRollbackProof proof;
    proof.module = placeholder.module;
    proof.source_part_index = placeholder.source_part_index;
    proof.attached_part = placeholder.source_part;
    proof.generated_part = placeholder.generated_part;
    proof.dry_run_session_identity = session.dry_run_session_identity;
    proof.checkpoint_protocol_identity = checkpoint_protocol.checkpoint_protocol_identity;
    proof.rollback_replay_identity = replay.replay_protocol_identity;
    proof.cursor_snapshot_identity = builtin_derive_token_cursor_snapshot_identity(placeholder,
        session, checkpoint_protocol, replay, query_name, token_record_count, checkpoint_count,
        cursor_snapshot_count, parser_state_snapshot_count, rollback_proof_count,
        cursor_commit_count);
    proof.snapshot_policy = std::string(FRONTEND_MACRO_M25B_CURSOR_SNAPSHOT_POLICY);
    proof.snapshot_query_name = query_name;
    proof.blocked_reason = std::string(FRONTEND_MACRO_M25B_CURSOR_SNAPSHOT_BLOCKER);
    proof.token_record_count = token_record_count;
    proof.checkpoint_count = checkpoint_count;
    proof.cursor_snapshot_count = cursor_snapshot_count;
    proof.parser_state_snapshot_count = parser_state_snapshot_count;
    proof.rollback_proof_count = rollback_proof_count;
    proof.cursor_commit_count = cursor_commit_count;
    return proof;
}

[[nodiscard]] query::StableFingerprint128 builtin_derive_diagnostic_shadow_closure_identity(
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveParserDryRunSessionBoundary& session,
    const BuiltinDeriveTokenCursorSnapshotRollbackProof& proof,
    const BuiltinDeriveDryRunRollbackDiagnosticReplay& replay,
    const BuiltinDeriveDryRunNegativeMatrixClosure& matrix,
    const std::string_view query_name,
    const base::u64 dry_run_session_count,
    const base::u64 cursor_snapshot_proof_count,
    const base::u64 rollback_replay_count,
    const base::u64 negative_matrix_count,
    const base::u64 diagnostic_shadow_count,
    const base::u64 executed_shadow_count,
    const base::u64 ast_mutation_count,
    const base::u64 parser_consumable_case_count) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M25C_DIAGNOSTIC_SHADOW_IDENTITY_MARKER);
    builder.mix_u32(placeholder.module.value);
    builder.mix_u32(placeholder.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.source_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    builder.mix_fingerprint(session.dry_run_session_identity);
    builder.mix_fingerprint(proof.cursor_snapshot_identity);
    builder.mix_fingerprint(replay.replay_protocol_identity);
    builder.mix_fingerprint(matrix.negative_matrix_identity);
    builder.mix_string(FRONTEND_MACRO_M25C_DIAGNOSTIC_SHADOW_POLICY);
    builder.mix_string(query_name);
    builder.mix_string(FRONTEND_MACRO_M25C_DIAGNOSTIC_SHADOW_BLOCKER);
    builder.mix_u64(dry_run_session_count);
    builder.mix_u64(cursor_snapshot_proof_count);
    builder.mix_u64(rollback_replay_count);
    builder.mix_u64(negative_matrix_count);
    builder.mix_u64(diagnostic_shadow_count);
    builder.mix_u64(executed_shadow_count);
    builder.mix_u64(ast_mutation_count);
    builder.mix_u64(parser_consumable_case_count);
    builder.mix_bool(true);  // dry_run_session_available
    builder.mix_bool(true);  // cursor_snapshot_proof_available
    builder.mix_bool(true);  // rollback_replay_available
    builder.mix_bool(true);  // negative_matrix_available
    builder.mix_bool(true);  // diagnostic_shadow_available
    builder.mix_bool(true);  // no_ast_mutation_verified
    builder.mix_bool(true);  // closure_complete
    builder.mix_bool(false); // dry_run_executed
    builder.mix_bool(false); // replay_execution_enabled
    builder.mix_bool(false); // session_committed
    builder.mix_bool(false); // parser_cursor_advanced
    builder.mix_bool(false); // parser_consumption_enabled
    builder.mix_bool(false); // parser_admitted
    builder.mix_bool(false); // generated_part_parsed
    builder.mix_bool(false); // generated_part_merged
    builder.mix_bool(false); // ast_mutated
    builder.mix_bool(false); // sema_visible
    builder.mix_bool(false); // emit_expanded_available
    builder.mix_bool(false); // debug_trace_available
    builder.mix_bool(false); // source_map_available
    builder.mix_bool(false); // standard_library_required
    builder.mix_bool(false); // runtime_required
    builder.mix_bool(false); // external_process_required
    builder.mix_bool(false); // produced_user_generated_code
    builder.mix_bool(true);  // closure_visible
    builder.mix_bool(true);  // query_reusable
    return builder.finish();
}

[[nodiscard]] BuiltinDeriveDiagnosticShadowNoAstMutationClosure
make_builtin_derive_diagnostic_shadow_no_ast_mutation_closure(
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveParserDryRunSessionBoundary& session,
    const BuiltinDeriveTokenCursorSnapshotRollbackProof& proof,
    const BuiltinDeriveDryRunRollbackDiagnosticReplay& replay,
    const BuiltinDeriveDryRunNegativeMatrixClosure& matrix)
{
    const base::u64 dry_run_session_count = session.session_visible ? 1U : 0U;
    const base::u64 cursor_snapshot_proof_count = proof.proof_visible ? 1U : 0U;
    const base::u64 rollback_replay_count = replay.replay_visible ? 1U : 0U;
    const base::u64 negative_matrix_count = matrix.matrix_visible ? 1U : 0U;
    const base::u64 diagnostic_shadow_count = replay.planned_replay_count;
    const base::u64 executed_shadow_count = 0U;
    const base::u64 ast_mutation_count = 0U;
    const base::u64 parser_consumable_case_count = 0U;
    const std::string query_name =
        builtin_derive_diagnostic_shadow_query_name(placeholder.module, placeholder.source_part_index);
    BuiltinDeriveDiagnosticShadowNoAstMutationClosure closure;
    closure.module = placeholder.module;
    closure.source_part_index = placeholder.source_part_index;
    closure.attached_part = placeholder.source_part;
    closure.generated_part = placeholder.generated_part;
    closure.dry_run_session_identity = session.dry_run_session_identity;
    closure.cursor_snapshot_identity = proof.cursor_snapshot_identity;
    closure.rollback_replay_identity = replay.replay_protocol_identity;
    closure.negative_matrix_identity = matrix.negative_matrix_identity;
    closure.closure_identity = builtin_derive_diagnostic_shadow_closure_identity(placeholder,
        session, proof, replay, matrix, query_name, dry_run_session_count,
        cursor_snapshot_proof_count, rollback_replay_count, negative_matrix_count,
        diagnostic_shadow_count, executed_shadow_count, ast_mutation_count,
        parser_consumable_case_count);
    closure.closure_policy = std::string(FRONTEND_MACRO_M25C_DIAGNOSTIC_SHADOW_POLICY);
    closure.closure_query_name = query_name;
    closure.blocked_reason = std::string(FRONTEND_MACRO_M25C_DIAGNOSTIC_SHADOW_BLOCKER);
    closure.dry_run_session_count = dry_run_session_count;
    closure.cursor_snapshot_proof_count = cursor_snapshot_proof_count;
    closure.rollback_replay_count = rollback_replay_count;
    closure.negative_matrix_count = negative_matrix_count;
    closure.diagnostic_shadow_count = diagnostic_shadow_count;
    closure.executed_shadow_count = executed_shadow_count;
    closure.ast_mutation_count = ast_mutation_count;
    closure.parser_consumable_case_count = parser_consumable_case_count;
    return closure;
}

[[nodiscard]] query::StableFingerprint128 builtin_derive_dry_run_admission_gate_identity(
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveParserDryRunSessionBoundary& session,
    const BuiltinDeriveTokenCursorSnapshotRollbackProof& proof,
    const BuiltinDeriveDiagnosticShadowNoAstMutationClosure& closure,
    const GeneratedModulePartParseMergeStub& parse_merge_stub,
    const std::string_view query_name,
    const base::u64 dry_run_session_count,
    const base::u64 cursor_snapshot_proof_count,
    const base::u64 diagnostic_shadow_closure_count,
    const base::u64 admission_prerequisite_count,
    const base::u64 token_buffer_candidate_count,
    const base::u64 token_record_count,
    const base::u64 dry_run_execution_admitted_count,
    const base::u64 parser_consumable_case_count) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M26A_DRY_RUN_ADMISSION_GATE_IDENTITY_MARKER);
    builder.mix_u32(placeholder.module.value);
    builder.mix_u32(placeholder.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.source_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    builder.mix_fingerprint(session.dry_run_session_identity);
    builder.mix_fingerprint(proof.cursor_snapshot_identity);
    builder.mix_fingerprint(closure.closure_identity);
    builder.mix_fingerprint(parse_merge_stub.generated_buffer_identity);
    builder.mix_fingerprint(parse_merge_stub.parse_config_fingerprint);
    builder.mix_string(FRONTEND_MACRO_M26A_DRY_RUN_ADMISSION_GATE_POLICY);
    builder.mix_string(query_name);
    builder.mix_string(FRONTEND_MACRO_M26A_DRY_RUN_ADMISSION_GATE_BLOCKER);
    builder.mix_u64(dry_run_session_count);
    builder.mix_u64(cursor_snapshot_proof_count);
    builder.mix_u64(diagnostic_shadow_closure_count);
    builder.mix_u64(admission_prerequisite_count);
    builder.mix_u64(token_buffer_candidate_count);
    builder.mix_u64(token_record_count);
    builder.mix_u64(dry_run_execution_admitted_count);
    builder.mix_u64(parser_consumable_case_count);
    builder.mix_bool(true);  // dry_run_session_available
    builder.mix_bool(true);  // cursor_snapshot_proof_available
    builder.mix_bool(true);  // diagnostic_shadow_closure_available
    builder.mix_bool(true);  // generated_buffer_available
    builder.mix_bool(true);  // parse_config_available
    builder.mix_bool(true);  // admission_gate_complete
    builder.mix_bool(false); // dry_run_execution_admitted
    builder.mix_bool(false); // dry_run_executed
    builder.mix_bool(false); // diagnostic_shadow_executed
    builder.mix_bool(false); // rollback_execution_enabled
    builder.mix_bool(false); // session_committed
    builder.mix_bool(false); // parser_cursor_advanced
    builder.mix_bool(false); // parser_consumption_enabled
    builder.mix_bool(false); // parser_admitted
    builder.mix_bool(false); // generated_part_parsed
    builder.mix_bool(false); // generated_part_merged
    builder.mix_bool(false); // ast_mutated
    builder.mix_bool(false); // sema_visible
    builder.mix_bool(false); // emit_expanded_available
    builder.mix_bool(false); // debug_trace_available
    builder.mix_bool(false); // source_map_available
    builder.mix_bool(false); // standard_library_required
    builder.mix_bool(false); // runtime_required
    builder.mix_bool(false); // external_process_required
    builder.mix_bool(false); // produced_user_generated_code
    builder.mix_bool(true);  // gate_visible
    builder.mix_bool(true);  // query_reusable
    return builder.finish();
}

[[nodiscard]] BuiltinDeriveParserDryRunAdmissionGate
make_builtin_derive_parser_dry_run_admission_gate(
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveParserDryRunSessionBoundary& session,
    const BuiltinDeriveTokenCursorSnapshotRollbackProof& proof,
    const BuiltinDeriveDiagnosticShadowNoAstMutationClosure& closure,
    const GeneratedModulePartParseMergeStub& parse_merge_stub)
{
    const base::u64 dry_run_session_count = session.session_visible ? 1U : 0U;
    const base::u64 cursor_snapshot_proof_count = proof.proof_visible ? 1U : 0U;
    const base::u64 diagnostic_shadow_closure_count = closure.closure_visible ? 1U : 0U;
    const base::u64 admission_prerequisite_count =
        FRONTEND_MACRO_M26A_DRY_RUN_ADMISSION_PREREQUISITE_COUNT;
    const base::u64 token_buffer_candidate_count = session.token_buffer_candidate_count;
    const base::u64 token_record_count = session.token_record_count;
    const base::u64 dry_run_execution_admitted_count = 0U;
    const base::u64 parser_consumable_case_count = 0U;
    const std::string query_name =
        builtin_derive_dry_run_admission_gate_query_name(placeholder.module,
            placeholder.source_part_index);
    BuiltinDeriveParserDryRunAdmissionGate gate;
    gate.module = placeholder.module;
    gate.source_part_index = placeholder.source_part_index;
    gate.attached_part = placeholder.source_part;
    gate.generated_part = placeholder.generated_part;
    gate.dry_run_session_identity = session.dry_run_session_identity;
    gate.cursor_snapshot_identity = proof.cursor_snapshot_identity;
    gate.diagnostic_shadow_closure_identity = closure.closure_identity;
    gate.generated_buffer_identity = parse_merge_stub.generated_buffer_identity;
    gate.parse_config_fingerprint = parse_merge_stub.parse_config_fingerprint;
    gate.admission_gate_identity = builtin_derive_dry_run_admission_gate_identity(
        placeholder, session, proof, closure, parse_merge_stub, query_name, dry_run_session_count,
        cursor_snapshot_proof_count, diagnostic_shadow_closure_count, admission_prerequisite_count,
        token_buffer_candidate_count, token_record_count, dry_run_execution_admitted_count,
        parser_consumable_case_count);
    gate.admission_policy = std::string(FRONTEND_MACRO_M26A_DRY_RUN_ADMISSION_GATE_POLICY);
    gate.admission_query_name = query_name;
    gate.blocked_reason = std::string(FRONTEND_MACRO_M26A_DRY_RUN_ADMISSION_GATE_BLOCKER);
    gate.dry_run_session_count = dry_run_session_count;
    gate.cursor_snapshot_proof_count = cursor_snapshot_proof_count;
    gate.diagnostic_shadow_closure_count = diagnostic_shadow_closure_count;
    gate.admission_prerequisite_count = admission_prerequisite_count;
    gate.token_buffer_candidate_count = token_buffer_candidate_count;
    gate.token_record_count = token_record_count;
    gate.dry_run_execution_admitted_count = dry_run_execution_admitted_count;
    gate.parser_consumable_case_count = parser_consumable_case_count;
    return gate;
}

[[nodiscard]] query::StableFingerprint128 builtin_derive_error_recovery_shadow_identity(
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveParserDryRunAdmissionGate& admission_gate,
    const BuiltinDeriveDiagnosticShadowNoAstMutationClosure& closure,
    const BuiltinDeriveDryRunRollbackDiagnosticReplay& replay,
    const ParserAdmissionDiagnosticReport& report,
    const std::string_view query_name,
    const base::u64 diagnostic_shadow_count,
    const base::u64 report_entry_count,
    const base::u64 planned_recovery_count,
    const base::u64 executed_recovery_count,
    const base::u64 emitted_diagnostic_count) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M26B_RECOVERY_SHADOW_GATE_IDENTITY_MARKER);
    builder.mix_u32(placeholder.module.value);
    builder.mix_u32(placeholder.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.source_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    builder.mix_fingerprint(admission_gate.admission_gate_identity);
    builder.mix_fingerprint(closure.closure_identity);
    builder.mix_fingerprint(replay.replay_protocol_identity);
    builder.mix_fingerprint(report.report_identity);
    builder.mix_string(FRONTEND_MACRO_M26B_RECOVERY_SHADOW_GATE_POLICY);
    builder.mix_string(query_name);
    builder.mix_string(FRONTEND_MACRO_M26B_RECOVERY_SHADOW_GATE_BLOCKER);
    builder.mix_u64(diagnostic_shadow_count);
    builder.mix_u64(report_entry_count);
    builder.mix_u64(planned_recovery_count);
    builder.mix_u64(executed_recovery_count);
    builder.mix_u64(emitted_diagnostic_count);
    builder.mix_bool(true);  // dry_run_admission_gate_available
    builder.mix_bool(true);  // diagnostic_shadow_closure_available
    builder.mix_bool(true);  // rollback_replay_available
    builder.mix_bool(true);  // parser_report_available
    builder.mix_bool(true);  // recovery_shadow_plan_available
    builder.mix_bool(true);  // recovery_shadow_complete
    builder.mix_bool(false); // recovery_execution_enabled
    builder.mix_bool(false); // diagnostic_emission_enabled
    builder.mix_bool(false); // dry_run_execution_admitted
    builder.mix_bool(false); // dry_run_executed
    builder.mix_bool(false); // rollback_execution_enabled
    builder.mix_bool(false); // session_committed
    builder.mix_bool(false); // parser_cursor_advanced
    builder.mix_bool(false); // parser_consumption_enabled
    builder.mix_bool(false); // parser_admitted
    builder.mix_bool(false); // generated_part_parsed
    builder.mix_bool(false); // generated_part_merged
    builder.mix_bool(false); // ast_mutated
    builder.mix_bool(false); // sema_visible
    builder.mix_bool(false); // emit_expanded_available
    builder.mix_bool(false); // debug_trace_available
    builder.mix_bool(false); // source_map_available
    builder.mix_bool(false); // standard_library_required
    builder.mix_bool(false); // runtime_required
    builder.mix_bool(false); // external_process_required
    builder.mix_bool(false); // produced_user_generated_code
    builder.mix_bool(true);  // gate_visible
    builder.mix_bool(true);  // query_reusable
    return builder.finish();
}

[[nodiscard]] BuiltinDeriveErrorRecoveryShadowDiagnosticGate
make_builtin_derive_error_recovery_shadow_diagnostic_gate(
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveParserDryRunAdmissionGate& admission_gate,
    const BuiltinDeriveDiagnosticShadowNoAstMutationClosure& closure,
    const BuiltinDeriveDryRunRollbackDiagnosticReplay& replay,
    const ParserAdmissionDiagnosticReport& report)
{
    const base::u64 diagnostic_shadow_count = closure.diagnostic_shadow_count;
    const base::u64 report_entry_count = report.entry_count;
    const base::u64 planned_recovery_count = report.blocked_entry_count;
    const base::u64 executed_recovery_count = 0U;
    const base::u64 emitted_diagnostic_count = 0U;
    const std::string query_name =
        builtin_derive_error_recovery_shadow_diagnostic_query_name(placeholder.module,
            placeholder.source_part_index);
    BuiltinDeriveErrorRecoveryShadowDiagnosticGate gate;
    gate.module = placeholder.module;
    gate.source_part_index = placeholder.source_part_index;
    gate.attached_part = placeholder.source_part;
    gate.generated_part = placeholder.generated_part;
    gate.dry_run_admission_gate_identity = admission_gate.admission_gate_identity;
    gate.diagnostic_shadow_closure_identity = closure.closure_identity;
    gate.rollback_replay_identity = replay.replay_protocol_identity;
    gate.parser_report_identity = report.report_identity;
    gate.recovery_shadow_identity = builtin_derive_error_recovery_shadow_identity(
        placeholder, admission_gate, closure, replay, report, query_name, diagnostic_shadow_count,
        report_entry_count, planned_recovery_count, executed_recovery_count,
        emitted_diagnostic_count);
    gate.recovery_policy = std::string(FRONTEND_MACRO_M26B_RECOVERY_SHADOW_GATE_POLICY);
    gate.recovery_query_name = query_name;
    gate.blocked_reason = std::string(FRONTEND_MACRO_M26B_RECOVERY_SHADOW_GATE_BLOCKER);
    gate.diagnostic_shadow_count = diagnostic_shadow_count;
    gate.report_entry_count = report_entry_count;
    gate.planned_recovery_count = planned_recovery_count;
    gate.executed_recovery_count = executed_recovery_count;
    gate.emitted_diagnostic_count = emitted_diagnostic_count;
    return gate;
}

[[nodiscard]] query::StableFingerprint128 builtin_derive_cursor_rollback_ast_verifier_identity(
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveParserDryRunAdmissionGate& admission_gate,
    const BuiltinDeriveErrorRecoveryShadowDiagnosticGate& recovery_gate,
    const BuiltinDeriveTokenCursorSnapshotRollbackProof& proof,
    const BuiltinDeriveParserDryRunSessionBoundary& session,
    const BuiltinDeriveDiagnosticShadowNoAstMutationClosure& diagnostic_closure,
    const std::string_view query_name,
    const base::u64 cursor_snapshot_count,
    const base::u64 rollback_proof_count,
    const base::u64 recovery_shadow_count,
    const base::u64 ast_baseline_snapshot_count,
    const base::u64 ast_mutation_count,
    const base::u64 cursor_commit_count,
    const base::u64 session_commit_count,
    const base::u64 parser_consumable_case_count) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M26C_ROLLBACK_AST_VERIFIER_IDENTITY_MARKER);
    builder.mix_u32(placeholder.module.value);
    builder.mix_u32(placeholder.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.source_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(placeholder.generated_part));
    builder.mix_fingerprint(admission_gate.admission_gate_identity);
    builder.mix_fingerprint(recovery_gate.recovery_shadow_identity);
    builder.mix_fingerprint(proof.cursor_snapshot_identity);
    builder.mix_fingerprint(session.dry_run_session_identity);
    builder.mix_fingerprint(diagnostic_closure.closure_identity);
    builder.mix_string(FRONTEND_MACRO_M26C_ROLLBACK_AST_VERIFIER_POLICY);
    builder.mix_string(query_name);
    builder.mix_string(FRONTEND_MACRO_M26C_ROLLBACK_AST_VERIFIER_BLOCKER);
    builder.mix_u64(cursor_snapshot_count);
    builder.mix_u64(rollback_proof_count);
    builder.mix_u64(recovery_shadow_count);
    builder.mix_u64(ast_baseline_snapshot_count);
    builder.mix_u64(ast_mutation_count);
    builder.mix_u64(cursor_commit_count);
    builder.mix_u64(session_commit_count);
    builder.mix_u64(parser_consumable_case_count);
    builder.mix_bool(true);  // dry_run_admission_gate_available
    builder.mix_bool(true);  // recovery_shadow_available
    builder.mix_bool(true);  // cursor_snapshot_proof_available
    builder.mix_bool(true);  // dry_run_session_available
    builder.mix_bool(true);  // diagnostic_shadow_closure_available
    builder.mix_bool(true);  // ast_baseline_available
    builder.mix_bool(true);  // rollback_execution_guard_available
    builder.mix_bool(true);  // ast_mutation_verifier_complete
    builder.mix_bool(false); // rollback_execution_enabled
    builder.mix_bool(false); // recovery_execution_enabled
    builder.mix_bool(false); // diagnostic_emission_enabled
    builder.mix_bool(false); // dry_run_execution_admitted
    builder.mix_bool(false); // dry_run_executed
    builder.mix_bool(false); // session_committed
    builder.mix_bool(false); // parser_cursor_advanced
    builder.mix_bool(false); // parser_consumption_enabled
    builder.mix_bool(false); // parser_admitted
    builder.mix_bool(false); // generated_part_parsed
    builder.mix_bool(false); // generated_part_merged
    builder.mix_bool(false); // ast_mutated
    builder.mix_bool(false); // sema_visible
    builder.mix_bool(false); // emit_expanded_available
    builder.mix_bool(false); // debug_trace_available
    builder.mix_bool(false); // source_map_available
    builder.mix_bool(false); // standard_library_required
    builder.mix_bool(false); // runtime_required
    builder.mix_bool(false); // external_process_required
    builder.mix_bool(false); // produced_user_generated_code
    builder.mix_bool(true);  // closure_visible
    builder.mix_bool(true);  // query_reusable
    return builder.finish();
}

[[nodiscard]] BuiltinDeriveCursorRollbackAstMutationVerifierClosure
make_builtin_derive_cursor_rollback_ast_mutation_verifier_closure(
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveParserDryRunAdmissionGate& admission_gate,
    const BuiltinDeriveErrorRecoveryShadowDiagnosticGate& recovery_gate,
    const BuiltinDeriveTokenCursorSnapshotRollbackProof& proof,
    const BuiltinDeriveParserDryRunSessionBoundary& session,
    const BuiltinDeriveDiagnosticShadowNoAstMutationClosure& diagnostic_closure)
{
    const base::u64 cursor_snapshot_count = proof.cursor_snapshot_count;
    const base::u64 rollback_proof_count = proof.rollback_proof_count;
    const base::u64 recovery_shadow_count = recovery_gate.gate_visible ? 1U : 0U;
    const base::u64 ast_baseline_snapshot_count = FRONTEND_MACRO_M26C_AST_BASELINE_SNAPSHOT_COUNT;
    const base::u64 ast_mutation_count = 0U;
    const base::u64 cursor_commit_count = 0U;
    const base::u64 session_commit_count = 0U;
    const base::u64 parser_consumable_case_count = 0U;
    const std::string query_name =
        builtin_derive_cursor_rollback_ast_verifier_query_name(placeholder.module,
            placeholder.source_part_index);
    BuiltinDeriveCursorRollbackAstMutationVerifierClosure closure;
    closure.module = placeholder.module;
    closure.source_part_index = placeholder.source_part_index;
    closure.attached_part = placeholder.source_part;
    closure.generated_part = placeholder.generated_part;
    closure.dry_run_admission_gate_identity = admission_gate.admission_gate_identity;
    closure.recovery_shadow_identity = recovery_gate.recovery_shadow_identity;
    closure.cursor_snapshot_identity = proof.cursor_snapshot_identity;
    closure.dry_run_session_identity = session.dry_run_session_identity;
    closure.diagnostic_shadow_closure_identity = diagnostic_closure.closure_identity;
    closure.verifier_closure_identity = builtin_derive_cursor_rollback_ast_verifier_identity(
        placeholder, admission_gate, recovery_gate, proof, session, diagnostic_closure, query_name,
        cursor_snapshot_count, rollback_proof_count, recovery_shadow_count,
        ast_baseline_snapshot_count, ast_mutation_count, cursor_commit_count,
        session_commit_count, parser_consumable_case_count);
    closure.verifier_policy = std::string(FRONTEND_MACRO_M26C_ROLLBACK_AST_VERIFIER_POLICY);
    closure.verifier_query_name = query_name;
    closure.blocked_reason = std::string(FRONTEND_MACRO_M26C_ROLLBACK_AST_VERIFIER_BLOCKER);
    closure.cursor_snapshot_count = cursor_snapshot_count;
    closure.rollback_proof_count = rollback_proof_count;
    closure.recovery_shadow_count = recovery_shadow_count;
    closure.ast_baseline_snapshot_count = ast_baseline_snapshot_count;
    closure.ast_mutation_count = ast_mutation_count;
    closure.cursor_commit_count = cursor_commit_count;
    closure.session_commit_count = session_commit_count;
    closure.parser_consumable_case_count = parser_consumable_case_count;
    return closure;
}

[[nodiscard]] query::ModulePartKey generated_module_part_key(
    const query::ModulePartKey source_part, const syntax::ModuleId module, const base::u32 part_index)
{
    const std::string name = module_part_generated_name(module, part_index);
    const std::string virtual_buffer = module_part_generated_virtual_buffer(module, part_index);
    const query::FileKey generated_file = query::file_key(
        source_part.module.package, name, query::SourceRole::generated, virtual_buffer);
    return query::module_part_key(source_part.module, generated_file, query::ModulePartKind::generated, name,
        part_index);
}

[[nodiscard]] query::StableFingerprint128 fingerprint_attribute_token_tree(
    const syntax::AttributeDecl& attribute) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21D_TOKEN_TREE_FINGERPRINT_MARKER);
    builder.mix_string(attribute.name);
    builder.mix_bool(attribute.has_token_tree);
    builder.mix_u64(static_cast<base::u64>(attribute.token_tree.size()));
    for (const syntax::AttributeTokenDecl& token : attribute.token_tree) {
        builder.mix_u8(static_cast<base::u8>(token.kind));
        builder.mix_string(token.text);
        builder.mix_u64(static_cast<base::u64>(token.range.source.value));
        builder.mix_u64(static_cast<base::u64>(token.range.begin));
        builder.mix_u64(static_cast<base::u64>(token.range.end));
        builder.mix_u32(token.depth);
        builder.mix_u8(static_cast<base::u8>(token.group));
    }
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 fingerprint_early_item_query_key(
    const syntax::ItemId item,
    const syntax::ModuleId module,
    const base::u32 part_index,
    const base::u32 attribute_index,
    const syntax::AttributeDecl& attribute,
    const query::ModulePartKey attached_part,
    const query::StableFingerprint128 token_tree_fingerprint) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21D_QUERY_KEY_FINGERPRINT_MARKER);
    builder.mix_u32(item.value);
    builder.mix_u32(module.value);
    builder.mix_u32(part_index);
    builder.mix_u32(attribute_index);
    builder.mix_string(attribute.name);
    builder.mix_fingerprint(query::stable_key_fingerprint(attached_part));
    builder.mix_fingerprint(token_tree_fingerprint);
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 fingerprint_macro_body_tokens(
    const syntax::ItemNode& item) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M27A_AUREX_MACRO_BODY_FINGERPRINT_MARKER);
    builder.mix_u8(static_cast<base::u8>(item.macro_kind));
    builder.mix_string(item.name);
    builder.mix_u64(static_cast<base::u64>(item.macro_body_range.source.value));
    builder.mix_u64(static_cast<base::u64>(item.macro_body_range.begin));
    builder.mix_u64(static_cast<base::u64>(item.macro_body_range.end));
    builder.mix_u64(static_cast<base::u64>(item.macro_body_tokens.size()));
    for (const syntax::AttributeTokenDecl& token : item.macro_body_tokens) {
        builder.mix_u8(static_cast<base::u8>(token.kind));
        builder.mix_string(token.text);
        builder.mix_u64(static_cast<base::u64>(token.range.source.value));
        builder.mix_u64(static_cast<base::u64>(token.range.begin));
        builder.mix_u64(static_cast<base::u64>(token.range.end));
        builder.mix_u32(token.depth);
        builder.mix_u8(static_cast<base::u8>(token.group));
    }
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 macro_surface_admission_identity(
    const syntax::ItemId item,
    const syntax::ModuleId module,
    const base::u32 part_index,
    const query::ModulePartKey attached_part,
    const syntax::ItemNode& macro_item,
    const query::StableFingerprint128 body_fingerprint,
    const std::string_view query_name) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M27A_AUREX_MACRO_SURFACE_ADMISSION_MARKER);
    builder.mix_u32(item.value);
    builder.mix_u32(module.value);
    builder.mix_u32(part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(attached_part));
    builder.mix_u8(static_cast<base::u8>(macro_item.macro_kind));
    builder.mix_string(macro_item.name);
    builder.mix_fingerprint(body_fingerprint);
    builder.mix_u64(static_cast<base::u64>(macro_item.macro_body_tokens.size()));
    builder.mix_u64(macro_item.macro_match_clause_count);
    builder.mix_bool(macro_item.macro_body_balanced);
    builder.mix_string(query_name);
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 macro_surface_admission_identity(
    const AurexMacroSurfaceAdmissionGate& gate) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M27A_AUREX_MACRO_SURFACE_ADMISSION_MARKER);
    builder.mix_u32(gate.item.value);
    builder.mix_u32(gate.module.value);
    builder.mix_u32(gate.part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(gate.attached_part));
    builder.mix_u8(static_cast<base::u8>(gate.macro_kind));
    builder.mix_string(gate.macro_name);
    builder.mix_fingerprint(gate.body_fingerprint);
    builder.mix_u64(gate.body_token_count);
    builder.mix_u64(gate.match_clause_count);
    builder.mix_bool(gate.body_balanced);
    builder.mix_string(gate.query_name);
    return builder.finish();
}

struct TypedMatcherCandidate {
    base::u32 matcher_index = 0;
    AurexMacroTypedMatcherKind matcher_kind = AurexMacroTypedMatcherKind::unknown;
    std::string matcher_head;
    std::string binding_name;
    base::SourceRange matcher_range{};
    base::SourceRange output_range{};
    bool matcher_shape_recognized = false;
};

[[nodiscard]] AurexMacroTypedMatcherKind matcher_kind_from_head(const std::string_view head) noexcept
{
    if (head == FRONTEND_MACRO_M27B_EXPR_LIST_MATCHER_TEXT) {
        return AurexMacroTypedMatcherKind::expr_list;
    }
    if (head == FRONTEND_MACRO_M27B_ITEM_MATCHER_TEXT) {
        return AurexMacroTypedMatcherKind::item;
    }
    if (head == FRONTEND_MACRO_M27B_TOKENS_MATCHER_TEXT) {
        return AurexMacroTypedMatcherKind::tokens;
    }
    return AurexMacroTypedMatcherKind::unknown;
}

[[nodiscard]] bool token_is_match_keyword(const syntax::AttributeTokenDecl& token) noexcept
{
    return token.kind == syntax::TokenKind::kw_match
        || (token.kind == syntax::TokenKind::identifier
            && token.text == FRONTEND_MACRO_M27B_MATCHER_KEYWORD_TEXT);
}

[[nodiscard]] base::usize find_matching_top_level_output_brace(
    const syntax::AstArenaVector<syntax::AttributeTokenDecl>& tokens,
    const base::usize start) noexcept
{
    if (start >= tokens.size() || tokens[start].kind != syntax::TokenKind::l_brace) {
        return tokens.size();
    }
    const base::u32 output_depth = tokens[start].depth;
    for (base::usize index = start + 1U; index < tokens.size(); ++index) {
        if (tokens[index].kind == syntax::TokenKind::r_brace && tokens[index].depth == output_depth) {
            return index;
        }
    }
    return tokens.size();
}

[[nodiscard]] bool token_at_matches(
    const syntax::AstArenaVector<syntax::AttributeTokenDecl>& tokens,
    const base::usize index,
    const base::u32 depth,
    const syntax::TokenKind kind) noexcept
{
    return index < tokens.size()
        && tokens[index].depth == depth
        && tokens[index].kind == kind;
}

[[nodiscard]] bool has_m27b_typed_matcher_shape(
    const syntax::AstArenaVector<syntax::AttributeTokenDecl>& tokens,
    const base::usize match_index) noexcept
{
    return match_index + FRONTEND_MACRO_M27B_MATCHER_OUTPUT_OPEN_OFFSET < tokens.size()
        && token_at_matches(tokens,
            match_index + FRONTEND_MACRO_M27B_MATCHER_HEAD_OFFSET,
            FRONTEND_MACRO_M27B_MATCHER_TOP_LEVEL_DEPTH,
            syntax::TokenKind::identifier)
        && token_at_matches(tokens,
            match_index + FRONTEND_MACRO_M27B_MATCHER_OPEN_PAREN_OFFSET,
            FRONTEND_MACRO_M27B_MATCHER_TOP_LEVEL_DEPTH,
            syntax::TokenKind::l_paren)
        && token_at_matches(tokens,
            match_index + FRONTEND_MACRO_M27B_MATCHER_BINDING_OFFSET,
            FRONTEND_MACRO_M27B_MATCHER_BINDING_DEPTH,
            syntax::TokenKind::identifier)
        && token_at_matches(tokens,
            match_index + FRONTEND_MACRO_M27B_MATCHER_CLOSE_PAREN_OFFSET,
            FRONTEND_MACRO_M27B_MATCHER_TOP_LEVEL_DEPTH,
            syntax::TokenKind::r_paren)
        && token_at_matches(tokens,
            match_index + FRONTEND_MACRO_M27B_MATCHER_ARROW_OFFSET,
            FRONTEND_MACRO_M27B_MATCHER_TOP_LEVEL_DEPTH,
            syntax::TokenKind::arrow)
        && token_at_matches(tokens,
            match_index + FRONTEND_MACRO_M27B_MATCHER_OUTPUT_OPEN_OFFSET,
            FRONTEND_MACRO_M27B_MATCHER_TOP_LEVEL_DEPTH,
            syntax::TokenKind::l_brace);
}

[[nodiscard]] std::string next_token_text_or_empty(
    const syntax::AstArenaVector<syntax::AttributeTokenDecl>& tokens,
    const base::usize match_index)
{
    const base::usize head_index = match_index + FRONTEND_MACRO_M27B_MATCHER_HEAD_OFFSET;
    return head_index < tokens.size() ? std::string(tokens[head_index].text) : std::string{};
}

[[nodiscard]] std::vector<TypedMatcherCandidate> collect_typed_matcher_candidates(
    const syntax::ItemNode& macro_item)
{
    std::vector<TypedMatcherCandidate> candidates;
    base::u32 matcher_index = 0U;
    const auto& tokens = macro_item.macro_body_tokens;
    for (base::usize index = 0; index < tokens.size(); ++index) {
        const syntax::AttributeTokenDecl& match_token = tokens[index];
        if (match_token.depth != FRONTEND_MACRO_M27B_MATCHER_TOP_LEVEL_DEPTH
            || !token_is_match_keyword(match_token)) {
            continue;
        }

        TypedMatcherCandidate candidate;
        candidate.matcher_index = matcher_index;
        ++matcher_index;
        candidate.matcher_range = match_token.range;
        if (has_m27b_typed_matcher_shape(tokens, index)) {
            candidate.matcher_head =
                std::string(tokens[index + FRONTEND_MACRO_M27B_MATCHER_HEAD_OFFSET].text);
            candidate.binding_name =
                std::string(tokens[index + FRONTEND_MACRO_M27B_MATCHER_BINDING_OFFSET].text);
            candidate.matcher_kind = matcher_kind_from_head(candidate.matcher_head);
            const base::usize output_open = index + FRONTEND_MACRO_M27B_MATCHER_OUTPUT_OPEN_OFFSET;
            const base::usize output_close = find_matching_top_level_output_brace(tokens, output_open);
            if (output_close < tokens.size()) {
                candidate.output_range = merged_range(tokens[output_open].range, tokens[output_close].range);
                candidate.matcher_range = merged_range(match_token.range, tokens[output_close].range);
                candidate.matcher_shape_recognized = candidate.matcher_kind != AurexMacroTypedMatcherKind::unknown;
            }
        } else {
            candidate.matcher_head = next_token_text_or_empty(tokens, index);
        }
        candidates.push_back(std::move(candidate));
    }
    return candidates;
}

[[nodiscard]] base::usize count_aurex_macro_matcher_candidates(const syntax::AstModule& ast)
{
    base::usize count = 0;
    for (base::usize item_index = 0; item_index < ast.items.size(); ++item_index) {
        const syntax::ItemNode& item = ast.items[item_index];
        if (item.kind != syntax::ItemKind::macro_decl) {
            continue;
        }
        count = base::checked_add_usize(
            count,
            static_cast<base::usize>(item.macro_match_clause_count),
            "M27b Aurex macro typed matcher candidate reserve");
    }
    return count;
}

[[nodiscard]] query::StableFingerprint128 matcher_fingerprint(
    const AurexMacroTypedMatcherAdmissionGate& gate) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M27B_MATCHER_FINGERPRINT_MARKER);
    builder.mix_u32(gate.item.value);
    builder.mix_u32(gate.module.value);
    builder.mix_u32(gate.part_index);
    builder.mix_u32(gate.matcher_index);
    builder.mix_u8(static_cast<base::u8>(gate.macro_kind));
    builder.mix_u8(static_cast<base::u8>(gate.matcher_kind));
    builder.mix_string(gate.macro_name);
    builder.mix_string(gate.matcher_head);
    builder.mix_string(gate.binding_name);
    builder.mix_u64(static_cast<base::u64>(gate.matcher_range.source.value));
    builder.mix_u64(static_cast<base::u64>(gate.matcher_range.begin));
    builder.mix_u64(static_cast<base::u64>(gate.matcher_range.end));
    builder.mix_u64(static_cast<base::u64>(gate.output_range.source.value));
    builder.mix_u64(static_cast<base::u64>(gate.output_range.begin));
    builder.mix_u64(static_cast<base::u64>(gate.output_range.end));
    builder.mix_bool(gate.matcher_shape_recognized);
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 definition_site_mark(
    const AurexMacroSurfaceAdmissionGate& surface) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M27B_DEFINITION_SITE_MARK_MARKER);
    builder.mix_u32(surface.item.value);
    builder.mix_u32(surface.module.value);
    builder.mix_u32(surface.part_index);
    builder.mix_string(surface.macro_name);
    builder.mix_fingerprint(surface.admission_identity);
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 fresh_name_scope(
    const AurexMacroSurfaceAdmissionGate& surface) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M27B_FRESH_NAME_SCOPE_MARKER);
    builder.mix_u32(surface.item.value);
    builder.mix_u32(surface.module.value);
    builder.mix_u32(surface.part_index);
    builder.mix_string(surface.macro_name);
    builder.mix_fingerprint(surface.body_fingerprint);
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 diagnostic_anchor_identity(
    const AurexMacroSurfaceAdmissionGate& surface,
    const base::SourceRange& anchor) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M27B_DIAGNOSTIC_ANCHOR_MARKER);
    builder.mix_u32(surface.item.value);
    builder.mix_u32(surface.module.value);
    builder.mix_u32(surface.part_index);
    builder.mix_string(surface.macro_name);
    builder.mix_u64(static_cast<base::u64>(anchor.source.value));
    builder.mix_u64(static_cast<base::u64>(anchor.begin));
    builder.mix_u64(static_cast<base::u64>(anchor.end));
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 macro_definition_site_hygiene_identity(
    const AurexMacroDefinitionSiteHygieneAdmissionGate& gate) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M27B_DEFINITION_SITE_HYGIENE_MARKER);
    builder.mix_u32(gate.item.value);
    builder.mix_u32(gate.module.value);
    builder.mix_u32(gate.part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(gate.attached_part));
    builder.mix_fingerprint(gate.surface_admission_identity);
    builder.mix_fingerprint(gate.body_fingerprint);
    builder.mix_fingerprint(gate.definition_site_mark);
    builder.mix_fingerprint(gate.fresh_name_scope);
    builder.mix_fingerprint(gate.diagnostic_anchor_identity);
    builder.mix_u8(static_cast<base::u8>(gate.macro_kind));
    builder.mix_string(gate.macro_name);
    builder.mix_string(gate.query_name);
    return builder.finish();
}

[[nodiscard]] query::StableFingerprint128 macro_typed_matcher_identity(
    const AurexMacroTypedMatcherAdmissionGate& gate) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M27B_TYPED_MATCHER_MARKER);
    builder.mix_u32(gate.item.value);
    builder.mix_u32(gate.module.value);
    builder.mix_u32(gate.part_index);
    builder.mix_u32(gate.matcher_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(gate.attached_part));
    builder.mix_fingerprint(gate.surface_admission_identity);
    builder.mix_fingerprint(gate.body_fingerprint);
    builder.mix_fingerprint(gate.matcher_fingerprint);
    builder.mix_fingerprint(gate.definition_site_hygiene_identity);
    builder.mix_fingerprint(gate.diagnostic_anchor_identity);
    builder.mix_u8(static_cast<base::u8>(gate.macro_kind));
    builder.mix_u8(static_cast<base::u8>(gate.matcher_kind));
    builder.mix_string(gate.macro_name);
    builder.mix_string(gate.matcher_head);
    builder.mix_string(gate.binding_name);
    builder.mix_bool(gate.matcher_shape_recognized);
    builder.mix_string(gate.query_name);
    return builder.finish();
}

[[nodiscard]] EarlyItemExpansionDisposition disposition_for_attribute(
    const syntax::AttributeDecl& attribute) noexcept
{
    return attribute.name == "derive" ? EarlyItemExpansionDisposition::builtin_derive_passthrough
                                      : EarlyItemExpansionDisposition::blocked_unimplemented_attribute;
}

[[nodiscard]] base::usize count_compiler_owned_generated_token_records(const syntax::AstModule& ast)
{
    base::usize count = 0;
    for (base::usize item_index = 0; item_index < ast.items.size(); ++item_index) {
        const syntax::ItemNode& item = ast.items[item_index];
        for (base::usize attribute_index = 0; attribute_index < item.attributes.size(); ++attribute_index) {
            const syntax::AttributeDecl& attribute = item.attributes[attribute_index];
            if (disposition_for_attribute(attribute) != EarlyItemExpansionDisposition::builtin_derive_passthrough) {
                continue;
            }
            count = base::checked_add_usize(
                count, attribute.token_tree.size(), FRONTEND_MACRO_M21I_GENERATED_TOKEN_RESERVE_CONTEXT);
            count = base::checked_add_usize(count,
                static_cast<base::usize>(FRONTEND_MACRO_M21I_DERIVE_SENTINEL_TOKEN_COUNT),
                FRONTEND_MACRO_M21I_GENERATED_TOKEN_RESERVE_CONTEXT);
        }
    }
    return count;
}

void mix_input(query::StableHashBuilder& builder, const EarlyItemMacroInput& input) noexcept
{
    builder.mix_u32(input.item.value);
    builder.mix_u32(input.module.value);
    builder.mix_u32(input.part_index);
    builder.mix_u32(input.attribute_index);
    builder.mix_string(input.attribute_name);
    builder.mix_u64(static_cast<base::u64>(input.attribute_range.source.value));
    builder.mix_u64(static_cast<base::u64>(input.attribute_range.begin));
    builder.mix_u64(static_cast<base::u64>(input.attribute_range.end));
    builder.mix_u64(static_cast<base::u64>(input.token_tree_range.source.value));
    builder.mix_u64(static_cast<base::u64>(input.token_tree_range.begin));
    builder.mix_u64(static_cast<base::u64>(input.token_tree_range.end));
    builder.mix_bool(input.has_token_tree);
    builder.mix_u64(input.token_count);
    builder.mix_fingerprint(query::stable_key_fingerprint(input.attached_part));
    builder.mix_fingerprint(input.token_tree_fingerprint);
    builder.mix_fingerprint(input.query_key_fingerprint);
    builder.mix_u8(static_cast<base::u8>(input.disposition));
}

void mix_generated_part(query::StableHashBuilder& builder, const GeneratedModulePartPlaceholder& part) noexcept
{
    builder.mix_u32(part.module.value);
    builder.mix_u32(part.source_part_index);
    builder.mix_u32(part.generated_stable_index);
    builder.mix_u8(static_cast<base::u8>(part.source_role));
    builder.mix_u8(static_cast<base::u8>(part.part_kind));
    builder.mix_fingerprint(query::stable_key_fingerprint(part.source_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(part.generated_part));
    builder.mix_fingerprint(part.output_fingerprint);
    builder.mix_bool(part.parsed);
    builder.mix_bool(part.merged);
    builder.mix_bool(part.produced_user_generated_code);
}

void mix_parse_merge_stub(query::StableHashBuilder& builder, const GeneratedModulePartParseMergeStub& stub) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M21E_PARSE_MERGE_STUB_MARKER);
    builder.mix_u32(stub.module.value);
    builder.mix_u32(stub.source_part_index);
    builder.mix_u32(stub.generated_stable_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(stub.source_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(stub.generated_part));
    builder.mix_fingerprint(stub.generated_buffer_identity);
    builder.mix_fingerprint(stub.parse_config_fingerprint);
    builder.mix_fingerprint(stub.merge_ordering_key);
    builder.mix_fingerprint(stub.expansion_origin);
    builder.mix_string(stub.generated_buffer_name);
    builder.mix_string(stub.blocker_reason);
    builder.mix_u8(static_cast<base::u8>(stub.lifecycle_state));
    builder.mix_bool(stub.materialized_buffer);
    builder.mix_bool(stub.parsed);
    builder.mix_bool(stub.merged);
    builder.mix_bool(stub.sema_visible);
    builder.mix_bool(stub.produced_user_generated_code);
}

void mix_source_map(query::StableHashBuilder& builder, const ExpansionSourceMapPlaceholder& source_map) noexcept
{
    builder.mix_u32(source_map.item.value);
    builder.mix_u32(source_map.module.value);
    builder.mix_u32(source_map.attribute_index);
    builder.mix_u64(static_cast<base::u64>(source_map.attribute_range.source.value));
    builder.mix_u64(static_cast<base::u64>(source_map.attribute_range.begin));
    builder.mix_u64(static_cast<base::u64>(source_map.attribute_range.end));
    builder.mix_u64(static_cast<base::u64>(source_map.token_tree_range.source.value));
    builder.mix_u64(static_cast<base::u64>(source_map.token_tree_range.begin));
    builder.mix_u64(static_cast<base::u64>(source_map.token_tree_range.end));
    builder.mix_fingerprint(source_map.expansion_origin);
    builder.mix_bool(source_map.real_source_map);
    builder.mix_bool(source_map.debug_trace_available);
}

void mix_hygiene_stub(query::StableHashBuilder& builder, const ExpansionHygieneStub& stub) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M21F_HYGIENE_STUB_MARKER);
    builder.mix_u32(stub.item.value);
    builder.mix_u32(stub.module.value);
    builder.mix_u32(stub.part_index);
    builder.mix_u32(stub.attribute_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(stub.attached_part));
    builder.mix_fingerprint(stub.expansion_origin);
    builder.mix_fingerprint(stub.call_site_mark);
    builder.mix_fingerprint(stub.definition_site_mark);
    builder.mix_fingerprint(stub.generated_fresh_mark);
    builder.mix_fingerprint(stub.declared_name_set);
    builder.mix_string(stub.policy);
    builder.mix_bool(stub.resolved);
    builder.mix_bool(stub.declared_names_visible);
    builder.mix_bool(stub.captures_call_site_locals);
}

void mix_trace_stub(query::StableHashBuilder& builder, const ExpansionTraceStub& stub) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M21F_TRACE_STUB_MARKER);
    builder.mix_u32(stub.item.value);
    builder.mix_u32(stub.module.value);
    builder.mix_u32(stub.part_index);
    builder.mix_u32(stub.attribute_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(stub.attached_part));
    builder.mix_u64(static_cast<base::u64>(stub.attribute_range.source.value));
    builder.mix_u64(static_cast<base::u64>(stub.attribute_range.begin));
    builder.mix_u64(static_cast<base::u64>(stub.attribute_range.end));
    builder.mix_u64(static_cast<base::u64>(stub.token_tree_range.source.value));
    builder.mix_u64(static_cast<base::u64>(stub.token_tree_range.begin));
    builder.mix_u64(static_cast<base::u64>(stub.token_tree_range.end));
    builder.mix_fingerprint(stub.expansion_origin);
    builder.mix_fingerprint(stub.trace_identity);
    builder.mix_fingerprint(stub.generated_source_map_identity);
    builder.mix_fingerprint(stub.diagnostic_anchor);
    builder.mix_string(stub.trace_policy);
    builder.mix_string(stub.blocker_reason);
    builder.mix_bool(stub.real_source_map);
    builder.mix_bool(stub.debug_trace_available);
    builder.mix_bool(stub.cli_emit_expanded_available);
}

void mix_generated_item_declaration_stub(
    query::StableHashBuilder& builder, const GeneratedItemDeclarationStub& stub) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M21G_GENERATED_ITEM_DECLARATION_MARKER);
    builder.mix_u32(stub.item.value);
    builder.mix_u32(stub.module.value);
    builder.mix_u32(stub.part_index);
    builder.mix_u32(stub.attribute_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(stub.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(stub.generated_part));
    builder.mix_fingerprint(stub.expansion_origin);
    builder.mix_fingerprint(stub.declaration_identity);
    builder.mix_fingerprint(stub.declared_name_set);
    builder.mix_fingerprint(stub.generated_item_key);
    builder.mix_string(stub.declaration_role);
    builder.mix_string(stub.generated_item_name);
    builder.mix_string(stub.blocker_reason);
    builder.mix_bool(stub.planned);
    builder.mix_bool(stub.materialized_tokens);
    builder.mix_bool(stub.parsed);
    builder.mix_bool(stub.merged);
    builder.mix_bool(stub.sema_visible);
    builder.mix_bool(stub.produced_user_generated_code);
}

void mix_declared_generated_name_stub(
    query::StableHashBuilder& builder, const DeclaredGeneratedNameStub& stub) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M21G_DECLARED_NAME_STUB_MARKER);
    builder.mix_u32(stub.item.value);
    builder.mix_u32(stub.module.value);
    builder.mix_u32(stub.part_index);
    builder.mix_u32(stub.attribute_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(stub.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(stub.generated_part));
    builder.mix_fingerprint(stub.expansion_origin);
    builder.mix_fingerprint(stub.declared_name_set);
    builder.mix_fingerprint(stub.declared_name_identity);
    builder.mix_fingerprint(stub.hygiene_mark);
    builder.mix_string(stub.declared_name);
    builder.mix_string(stub.namespace_kind);
    builder.mix_string(stub.blocker_reason);
    builder.mix_bool(stub.lookup_visible);
    builder.mix_bool(stub.export_visible);
    builder.mix_bool(stub.sema_visible);
    builder.mix_bool(stub.produced_user_generated_code);
}

void mix_token_materialization_admission_stub(
    query::StableHashBuilder& builder, const TokenMaterializationAdmissionStub& stub) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M21H_ADMISSION_STUB_MARKER);
    builder.mix_u32(stub.item.value);
    builder.mix_u32(stub.module.value);
    builder.mix_u32(stub.part_index);
    builder.mix_u32(stub.attribute_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(stub.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(stub.generated_part));
    builder.mix_fingerprint(stub.expansion_origin);
    builder.mix_fingerprint(stub.declaration_identity);
    builder.mix_fingerprint(stub.generated_item_key);
    builder.mix_fingerprint(stub.declared_name_set);
    builder.mix_fingerprint(stub.declared_name_identity);
    builder.mix_fingerprint(stub.hygiene_mark);
    builder.mix_fingerprint(stub.source_map_identity);
    builder.mix_fingerprint(stub.trace_identity);
    builder.mix_fingerprint(stub.token_plan_identity);
    builder.mix_fingerprint(stub.token_buffer_identity);
    builder.mix_string(stub.admission_policy);
    builder.mix_string(stub.token_stream_name);
    builder.mix_string(stub.blocker_reason);
    builder.mix_bool(stub.compiler_owned);
    builder.mix_bool(stub.admitted);
    builder.mix_bool(stub.materialized_tokens);
    builder.mix_bool(stub.generated_source_text);
    builder.mix_bool(stub.parse_ready);
    builder.mix_bool(stub.external_process_required);
    builder.mix_bool(stub.standard_library_required);
    builder.mix_bool(stub.runtime_required);
    builder.mix_bool(stub.produced_user_generated_code);
}

void mix_generated_token_buffer_stub(
    query::StableHashBuilder& builder, const GeneratedTokenBufferStub& stub) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M21H_TOKEN_BUFFER_STUB_MARKER);
    builder.mix_u32(stub.item.value);
    builder.mix_u32(stub.module.value);
    builder.mix_u32(stub.part_index);
    builder.mix_u32(stub.attribute_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(stub.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(stub.generated_part));
    builder.mix_fingerprint(stub.token_plan_identity);
    builder.mix_fingerprint(stub.token_buffer_identity);
    builder.mix_fingerprint(stub.materialization_identity);
    builder.mix_fingerprint(stub.source_map_identity);
    builder.mix_fingerprint(stub.hygiene_mark);
    builder.mix_string(stub.token_stream_name);
    builder.mix_string(stub.token_buffer_kind);
    builder.mix_string(stub.token_producer_policy);
    builder.mix_string(stub.blocker_reason);
    builder.mix_u64(stub.token_count);
    builder.mix_bool(stub.empty);
    builder.mix_bool(stub.materialized_tokens);
    builder.mix_bool(stub.generated_source_text);
    builder.mix_bool(stub.parser_consumable);
    builder.mix_bool(stub.produced_user_generated_code);
}

void mix_generated_token_record(query::StableHashBuilder& builder, const GeneratedTokenRecord& record) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M21I_GENERATED_TOKEN_RECORD_MARKER);
    builder.mix_u32(record.item.value);
    builder.mix_u32(record.module.value);
    builder.mix_u32(record.part_index);
    builder.mix_u32(record.attribute_index);
    builder.mix_u32(record.token_index);
    builder.mix_fingerprint(record.token_buffer_identity);
    builder.mix_fingerprint(record.token_identity);
    builder.mix_fingerprint(record.source_map_identity);
    builder.mix_fingerprint(record.hygiene_mark);
    builder.mix_u8(static_cast<base::u8>(record.kind));
    builder.mix_string(record.text);
    builder.mix_string(record.token_role);
    builder.mix_u64(static_cast<base::u64>(record.anchor_range.source.value));
    builder.mix_u64(static_cast<base::u64>(record.anchor_range.begin));
    builder.mix_u64(static_cast<base::u64>(record.anchor_range.end));
    builder.mix_bool(record.compiler_owned);
    builder.mix_bool(record.parser_visible);
    builder.mix_bool(record.produced_user_generated_code);
}

void mix_generated_token_parser_admission_gate_stub(
    query::StableHashBuilder& builder, const GeneratedTokenParserAdmissionGateStub& stub) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M21J_PARSER_ADMISSION_GATE_MARKER);
    builder.mix_u32(stub.item.value);
    builder.mix_u32(stub.module.value);
    builder.mix_u32(stub.part_index);
    builder.mix_u32(stub.attribute_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(stub.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(stub.generated_part));
    builder.mix_fingerprint(stub.token_plan_identity);
    builder.mix_fingerprint(stub.token_buffer_identity);
    builder.mix_fingerprint(stub.materialization_identity);
    builder.mix_fingerprint(stub.source_map_identity);
    builder.mix_fingerprint(stub.hygiene_mark);
    builder.mix_fingerprint(stub.generated_buffer_identity);
    builder.mix_fingerprint(stub.parse_config_fingerprint);
    builder.mix_fingerprint(stub.parse_gate_identity);
    builder.mix_string(stub.token_stream_name);
    builder.mix_string(stub.parser_gate_policy);
    builder.mix_string(stub.blocker_reason);
    builder.mix_u64(stub.token_count);
    builder.mix_bool(stub.compiler_owned);
    builder.mix_bool(stub.token_buffer_materialized);
    builder.mix_bool(stub.token_records_available);
    builder.mix_bool(stub.parser_admitted);
    builder.mix_bool(stub.parse_ready);
    builder.mix_bool(stub.parser_consumable);
    builder.mix_bool(stub.generated_source_text);
    builder.mix_bool(stub.generated_part_parsed);
    builder.mix_bool(stub.generated_part_merged);
    builder.mix_bool(stub.sema_visible);
    builder.mix_bool(stub.produced_user_generated_code);
}

void mix_parser_admission_diagnostic_projection_stub(
    query::StableHashBuilder& builder, const ParserAdmissionDiagnosticProjectionStub& stub) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M21K_DIAGNOSTIC_PROJECTION_MARKER);
    builder.mix_u32(stub.item.value);
    builder.mix_u32(stub.module.value);
    builder.mix_u32(stub.part_index);
    builder.mix_u32(stub.attribute_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(stub.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(stub.generated_part));
    builder.mix_u64(static_cast<base::u64>(stub.primary_anchor.source.value));
    builder.mix_u64(static_cast<base::u64>(stub.primary_anchor.begin));
    builder.mix_u64(static_cast<base::u64>(stub.primary_anchor.end));
    builder.mix_u64(static_cast<base::u64>(stub.token_tree_anchor.source.value));
    builder.mix_u64(static_cast<base::u64>(stub.token_tree_anchor.begin));
    builder.mix_u64(static_cast<base::u64>(stub.token_tree_anchor.end));
    builder.mix_fingerprint(stub.parse_gate_identity);
    builder.mix_fingerprint(stub.diagnostic_identity);
    builder.mix_fingerprint(stub.diagnostic_anchor_identity);
    builder.mix_fingerprint(stub.token_plan_identity);
    builder.mix_fingerprint(stub.token_buffer_identity);
    builder.mix_fingerprint(stub.materialization_identity);
    builder.mix_fingerprint(stub.generated_buffer_identity);
    builder.mix_fingerprint(stub.parse_config_fingerprint);
    builder.mix_fingerprint(stub.source_map_identity);
    builder.mix_fingerprint(stub.hygiene_mark);
    builder.mix_fingerprint(stub.trace_identity);
    builder.mix_string(stub.diagnostic_policy);
    builder.mix_string(stub.blocker_category);
    builder.mix_string(stub.token_buffer_blocker);
    builder.mix_string(stub.generated_part_parse_blocker);
    builder.mix_string(stub.user_message);
    builder.mix_string(stub.debug_projection_name);
    builder.mix_u64(stub.token_count);
    builder.mix_bool(stub.token_buffer_materialized);
    builder.mix_bool(stub.token_records_available);
    builder.mix_bool(stub.parser_admitted);
    builder.mix_bool(stub.parse_ready);
    builder.mix_bool(stub.parser_consumable);
    builder.mix_bool(stub.generated_part_parsed);
    builder.mix_bool(stub.generated_part_merged);
    builder.mix_bool(stub.emit_expanded_available);
    builder.mix_bool(stub.debug_trace_available);
    builder.mix_bool(stub.source_map_available);
    builder.mix_bool(stub.produced_user_generated_code);
}

void mix_parser_admission_report_entry(
    query::StableHashBuilder& builder, const ParserAdmissionDiagnosticReportEntry& entry) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M21L_REPORT_ENTRY_MARKER);
    builder.mix_u32(entry.item.value);
    builder.mix_u32(entry.module.value);
    builder.mix_u32(entry.part_index);
    builder.mix_u32(entry.attribute_index);
    builder.mix_u32(entry.report_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(entry.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(entry.generated_part));
    builder.mix_u64(static_cast<base::u64>(entry.primary_anchor.source.value));
    builder.mix_u64(static_cast<base::u64>(entry.primary_anchor.begin));
    builder.mix_u64(static_cast<base::u64>(entry.primary_anchor.end));
    builder.mix_u64(static_cast<base::u64>(entry.token_tree_anchor.source.value));
    builder.mix_u64(static_cast<base::u64>(entry.token_tree_anchor.begin));
    builder.mix_u64(static_cast<base::u64>(entry.token_tree_anchor.end));
    builder.mix_fingerprint(entry.diagnostic_identity);
    builder.mix_fingerprint(entry.diagnostic_anchor_identity);
    builder.mix_fingerprint(entry.report_entry_identity);
    builder.mix_fingerprint(entry.parse_gate_identity);
    builder.mix_string(entry.blocker_category);
    builder.mix_string(entry.debug_projection_name);
    builder.mix_string(entry.query_projection_name);
    builder.mix_u64(entry.token_count);
    builder.mix_bool(entry.token_records_available);
    builder.mix_bool(entry.parser_admitted);
    builder.mix_bool(entry.report_visible);
    builder.mix_bool(entry.query_reusable);
    builder.mix_bool(entry.parser_consumable);
    builder.mix_bool(entry.emit_expanded_available);
    builder.mix_bool(entry.produced_user_generated_code);
}

void mix_parser_admission_report(
    query::StableHashBuilder& builder, const ParserAdmissionDiagnosticReport& report) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M21L_REPORT_MARKER);
    builder.mix_u32(report.module.value);
    builder.mix_u32(report.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(report.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(report.generated_part));
    builder.mix_fingerprint(report.report_identity);
    builder.mix_fingerprint(report.report_anchor_identity);
    builder.mix_fingerprint(report.report_grouping_identity);
    builder.mix_fingerprint(report.parse_config_fingerprint);
    builder.mix_fingerprint(report.generated_buffer_identity);
    builder.mix_string(report.report_policy);
    builder.mix_string(report.report_query_name);
    builder.mix_string(report.blocked_reason);
    builder.mix_u64(report.entry_count);
    builder.mix_u64(report.blocked_entry_count);
    builder.mix_u64(report.derive_entry_count);
    builder.mix_u64(report.empty_entry_count);
    builder.mix_u64(report.token_record_available_entry_count);
    builder.mix_bool(report.query_reusable);
    builder.mix_bool(report.report_visible);
    builder.mix_bool(report.source_anchor_ordered);
    builder.mix_bool(report.parser_admitted);
    builder.mix_bool(report.parse_ready);
    builder.mix_bool(report.parser_consumable);
    builder.mix_bool(report.emit_expanded_available);
    builder.mix_bool(report.debug_trace_available);
    builder.mix_bool(report.source_map_available);
    builder.mix_bool(report.produced_user_generated_code);
}

void mix_parser_readiness_preflight_entry(
    query::StableHashBuilder& builder, const GeneratedTokenParserReadinessPreflightEntry& entry) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M21M_PREFLIGHT_ENTRY_MARKER);
    builder.mix_u32(entry.item.value);
    builder.mix_u32(entry.module.value);
    builder.mix_u32(entry.part_index);
    builder.mix_u32(entry.attribute_index);
    builder.mix_u32(entry.preflight_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(entry.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(entry.generated_part));
    builder.mix_fingerprint(entry.token_plan_identity);
    builder.mix_fingerprint(entry.token_buffer_identity);
    builder.mix_fingerprint(entry.materialization_identity);
    builder.mix_fingerprint(entry.generated_buffer_identity);
    builder.mix_fingerprint(entry.parse_config_fingerprint);
    builder.mix_fingerprint(entry.parse_gate_identity);
    builder.mix_fingerprint(entry.diagnostic_identity);
    builder.mix_fingerprint(entry.diagnostic_anchor_identity);
    builder.mix_fingerprint(entry.report_entry_identity);
    builder.mix_fingerprint(entry.source_map_identity);
    builder.mix_fingerprint(entry.hygiene_mark);
    builder.mix_fingerprint(entry.trace_identity);
    builder.mix_fingerprint(entry.preflight_identity);
    builder.mix_string(entry.token_stream_name);
    builder.mix_string(entry.token_stream_shape);
    builder.mix_string(entry.delimiter_balance_state);
    builder.mix_string(entry.source_anchor_coverage_state);
    builder.mix_string(entry.readiness_policy);
    builder.mix_string(entry.blocker_reason);
    builder.mix_u64(entry.token_count);
    builder.mix_bool(entry.token_records_available);
    builder.mix_bool(entry.token_indices_contiguous);
    builder.mix_bool(entry.delimiter_balanced);
    builder.mix_bool(entry.source_anchors_covered);
    builder.mix_bool(entry.parse_config_compatible);
    builder.mix_bool(entry.hygiene_prerequisite_available);
    builder.mix_bool(entry.source_map_prerequisite_available);
    builder.mix_bool(entry.diagnostic_projection_available);
    builder.mix_bool(entry.parser_admitted);
    builder.mix_bool(entry.parse_ready);
    builder.mix_bool(entry.parser_consumable);
    builder.mix_bool(entry.generated_part_parsed);
    builder.mix_bool(entry.generated_part_merged);
    builder.mix_bool(entry.produced_user_generated_code);
}

void mix_parser_consumption_contract_gate(
    query::StableHashBuilder& builder, const GeneratedTokenParserConsumptionContractGate& gate) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M21N_CONTRACT_GATE_MARKER);
    builder.mix_u32(gate.module.value);
    builder.mix_u32(gate.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(gate.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(gate.generated_part));
    builder.mix_fingerprint(gate.generated_buffer_identity);
    builder.mix_fingerprint(gate.parse_config_fingerprint);
    builder.mix_fingerprint(gate.report_identity);
    builder.mix_fingerprint(gate.contract_identity);
    builder.mix_fingerprint(gate.contract_grouping_identity);
    builder.mix_fingerprint(gate.contract_anchor_identity);
    builder.mix_string(gate.contract_policy);
    builder.mix_string(gate.contract_query_name);
    builder.mix_string(gate.blocked_reason);
    builder.mix_u64(gate.preflight_entry_count);
    builder.mix_u64(gate.blocked_entry_count);
    builder.mix_u64(gate.derive_entry_count);
    builder.mix_u64(gate.empty_entry_count);
    builder.mix_u64(gate.contiguous_index_entry_count);
    builder.mix_u64(gate.delimiter_balanced_entry_count);
    builder.mix_u64(gate.source_anchor_covered_entry_count);
    builder.mix_u64(gate.parse_config_compatible_entry_count);
    builder.mix_u64(gate.diagnostic_projection_entry_count);
    builder.mix_bool(gate.query_reusable);
    builder.mix_bool(gate.contract_visible);
    builder.mix_bool(gate.all_entries_structurally_checked);
    builder.mix_bool(gate.parser_admitted);
    builder.mix_bool(gate.parse_ready);
    builder.mix_bool(gate.parser_consumable);
    builder.mix_bool(gate.generated_part_parsed);
    builder.mix_bool(gate.generated_part_merged);
    builder.mix_bool(gate.sema_visible);
    builder.mix_bool(gate.emit_expanded_available);
    builder.mix_bool(gate.debug_trace_available);
    builder.mix_bool(gate.source_map_available);
    builder.mix_bool(gate.produced_user_generated_code);
}

void mix_macro_boundary_closure_report(
    query::StableHashBuilder& builder, const MacroExpansionBoundaryClosureReport& report) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M21O_CLOSURE_REPORT_MARKER);
    builder.mix_fingerprint(report.closure_identity);
    builder.mix_fingerprint(report.closure_grouping_identity);
    builder.mix_string(report.closure_policy);
    builder.mix_string(report.closure_query_name);
    builder.mix_string(report.blocked_reason);
    builder.mix_u64(report.macro_input_count);
    builder.mix_u64(report.generated_part_count);
    builder.mix_u64(report.parser_admission_report_count);
    builder.mix_u64(report.parser_readiness_preflight_entry_count);
    builder.mix_u64(report.parser_consumption_contract_gate_count);
    builder.mix_u64(report.blocked_contract_gate_count);
    builder.mix_u64(report.parser_consumable_contract_gate_count);
    builder.mix_bool(report.m21m_preflight_available);
    builder.mix_bool(report.m21n_contract_available);
    builder.mix_bool(report.release_closure_complete);
    builder.mix_bool(report.query_reusable);
    builder.mix_bool(report.closure_visible);
    builder.mix_bool(report.parser_consumption_enabled);
    builder.mix_bool(report.emit_expanded_available);
    builder.mix_bool(report.debug_trace_available);
    builder.mix_bool(report.source_map_available);
    builder.mix_bool(report.standard_library_required);
    builder.mix_bool(report.runtime_required);
    builder.mix_bool(report.external_process_required);
    builder.mix_bool(report.produced_user_generated_code);
}

void mix_builtin_derive_expansion_admission_gate(
    query::StableHashBuilder& builder, const BuiltinDeriveExpansionAdmissionGate& gate) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M22A_ADMISSION_GATE_MARKER);
    builder.mix_u32(gate.item.value);
    builder.mix_u32(gate.module.value);
    builder.mix_u32(gate.part_index);
    builder.mix_u32(gate.attribute_index);
    builder.mix_u32(gate.admission_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(gate.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(gate.generated_part));
    builder.mix_fingerprint(gate.token_buffer_identity);
    builder.mix_fingerprint(gate.preflight_identity);
    builder.mix_fingerprint(gate.parse_gate_identity);
    builder.mix_fingerprint(gate.diagnostic_identity);
    builder.mix_fingerprint(gate.closure_identity);
    builder.mix_fingerprint(gate.admission_identity);
    builder.mix_string(gate.admission_policy);
    builder.mix_string(gate.admission_kind);
    builder.mix_string(gate.query_name);
    builder.mix_string(gate.blocker_reason);
    builder.mix_u64(gate.token_count);
    builder.mix_u64(gate.capability_candidate_count);
    builder.mix_u64(gate.unsupported_candidate_count);
    builder.mix_u64(gate.duplicate_candidate_count);
    builder.mix_bool(gate.builtin_derive_input);
    builder.mix_bool(gate.compiler_owned);
    builder.mix_bool(gate.token_records_available);
    builder.mix_bool(gate.preflight_available);
    builder.mix_bool(gate.admission_visible);
    builder.mix_bool(gate.query_reusable);
    builder.mix_bool(gate.parser_consumption_enabled);
    builder.mix_bool(gate.external_process_required);
    builder.mix_bool(gate.standard_library_required);
    builder.mix_bool(gate.runtime_required);
    builder.mix_bool(gate.generated_source_text);
    builder.mix_bool(gate.produced_user_generated_code);
}

void mix_builtin_derive_semantic_expansion_plan(
    query::StableHashBuilder& builder, const BuiltinDeriveSemanticExpansionPlan& plan) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M22B_SEMANTIC_PLAN_MARKER);
    builder.mix_u32(plan.item.value);
    builder.mix_u32(plan.module.value);
    builder.mix_u32(plan.part_index);
    builder.mix_u32(plan.attribute_index);
    builder.mix_u32(plan.semantic_plan_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(plan.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(plan.generated_part));
    builder.mix_fingerprint(plan.token_buffer_identity);
    builder.mix_fingerprint(plan.preflight_identity);
    builder.mix_fingerprint(plan.admission_identity);
    builder.mix_fingerprint(plan.semantic_plan_identity);
    builder.mix_fingerprint(plan.capability_set_identity);
    builder.mix_string(plan.semantic_policy);
    builder.mix_string(plan.target_kind);
    builder.mix_string(plan.semantic_model);
    builder.mix_string(plan.blocker_reason);
    builder.mix_u64(plan.capability_count);
    builder.mix_u64(plan.copy_capability_count);
    builder.mix_u64(plan.eq_capability_count);
    builder.mix_u64(plan.hash_capability_count);
    builder.mix_bool(plan.builtin_derive_input);
    builder.mix_bool(plan.target_struct_or_enum);
    builder.mix_bool(plan.uses_existing_builtin_derive_capability_path);
    builder.mix_bool(plan.requires_ast_mutation);
    builder.mix_bool(plan.requires_generated_items);
    builder.mix_bool(plan.requires_standard_library);
    builder.mix_bool(plan.requires_runtime);
    builder.mix_bool(plan.external_process_required);
    builder.mix_bool(plan.parser_consumption_enabled);
    builder.mix_bool(plan.produced_user_generated_code);
    builder.mix_bool(plan.plan_visible);
    builder.mix_bool(plan.query_reusable);
}

void mix_builtin_derive_parser_consumption_release_gate(
    query::StableHashBuilder& builder, const BuiltinDeriveParserConsumptionReleaseGate& gate) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M22C_RELEASE_GATE_MARKER);
    builder.mix_u32(gate.module.value);
    builder.mix_u32(gate.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(gate.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(gate.generated_part));
    builder.mix_fingerprint(gate.contract_identity);
    builder.mix_fingerprint(gate.closure_identity);
    builder.mix_fingerprint(gate.admission_group_identity);
    builder.mix_fingerprint(gate.semantic_plan_group_identity);
    builder.mix_fingerprint(gate.release_gate_identity);
    builder.mix_string(gate.release_policy);
    builder.mix_string(gate.release_query_name);
    builder.mix_string(gate.blocked_reason);
    builder.mix_u64(gate.admission_count);
    builder.mix_u64(gate.derive_admission_count);
    builder.mix_u64(gate.semantic_plan_count);
    builder.mix_u64(gate.capability_total_count);
    builder.mix_u64(gate.parser_consumable_contract_count);
    builder.mix_bool(gate.rollback_diagnostics_available);
    builder.mix_bool(gate.debug_trace_prerequisite_available);
    builder.mix_bool(gate.source_map_prerequisite_available);
    builder.mix_bool(gate.hygiene_prerequisite_available);
    builder.mix_bool(gate.parser_consumption_enabled);
    builder.mix_bool(gate.generated_part_parsed);
    builder.mix_bool(gate.generated_part_merged);
    builder.mix_bool(gate.emit_expanded_available);
    builder.mix_bool(gate.debug_trace_available);
    builder.mix_bool(gate.source_map_available);
    builder.mix_bool(gate.standard_library_required);
    builder.mix_bool(gate.runtime_required);
    builder.mix_bool(gate.external_process_required);
    builder.mix_bool(gate.produced_user_generated_code);
    builder.mix_bool(gate.release_visible);
    builder.mix_bool(gate.query_reusable);
}

void mix_builtin_derive_release_hardening_matrix(
    query::StableHashBuilder& builder, const BuiltinDeriveReleaseHardeningMatrix& matrix) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M22D_HARDENING_MATRIX_MARKER);
    builder.mix_u32(matrix.module.value);
    builder.mix_u32(matrix.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(matrix.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(matrix.generated_part));
    builder.mix_fingerprint(matrix.release_gate_identity);
    builder.mix_fingerprint(matrix.admission_group_identity);
    builder.mix_fingerprint(matrix.semantic_plan_group_identity);
    builder.mix_fingerprint(matrix.hardening_matrix_identity);
    builder.mix_string(matrix.hardening_policy);
    builder.mix_string(matrix.hardening_query_name);
    builder.mix_string(matrix.blocked_reason);
    builder.mix_u64(matrix.part_local_admission_count);
    builder.mix_u64(matrix.part_local_derive_admission_count);
    builder.mix_u64(matrix.part_local_semantic_plan_count);
    builder.mix_u64(matrix.part_local_release_gate_count);
    builder.mix_u64(matrix.global_admission_count);
    builder.mix_u64(matrix.global_semantic_plan_count);
    builder.mix_u64(matrix.global_generated_part_count);
    builder.mix_u64(matrix.cross_part_admission_count);
    builder.mix_u64(matrix.cross_part_semantic_plan_count);
    builder.mix_bool(matrix.part_locality_preserved);
    builder.mix_bool(matrix.multi_item_matrix_available);
    builder.mix_bool(matrix.negative_matrix_complete);
    builder.mix_bool(matrix.release_remains_blocked);
    builder.mix_bool(matrix.parser_consumption_enabled);
    builder.mix_bool(matrix.generated_part_parsed);
    builder.mix_bool(matrix.generated_part_merged);
    builder.mix_bool(matrix.emit_expanded_available);
    builder.mix_bool(matrix.debug_trace_available);
    builder.mix_bool(matrix.source_map_available);
    builder.mix_bool(matrix.standard_library_required);
    builder.mix_bool(matrix.runtime_required);
    builder.mix_bool(matrix.external_process_required);
    builder.mix_bool(matrix.produced_user_generated_code);
    builder.mix_bool(matrix.matrix_visible);
    builder.mix_bool(matrix.query_reusable);
}

void mix_builtin_derive_debug_dump_stability_contract(
    query::StableHashBuilder& builder, const BuiltinDeriveDebugDumpStabilityContract& contract) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M22E_DEBUG_DUMP_CONTRACT_MARKER);
    builder.mix_u32(contract.module.value);
    builder.mix_u32(contract.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(contract.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(contract.generated_part));
    builder.mix_fingerprint(contract.release_gate_identity);
    builder.mix_fingerprint(contract.hardening_matrix_identity);
    builder.mix_fingerprint(contract.debug_dump_contract_identity);
    builder.mix_string(contract.debug_dump_policy);
    builder.mix_string(contract.debug_dump_query_name);
    builder.mix_string(contract.blocked_reason);
    builder.mix_u64(contract.dump_section_count);
    builder.mix_bool(contract.stable_ordering_available);
    builder.mix_bool(contract.identity_projection_available);
    builder.mix_bool(contract.summary_projection_available);
    builder.mix_bool(contract.drift_debuggable);
    builder.mix_bool(contract.debug_dump_contract_complete);
    builder.mix_bool(contract.emit_expanded_available);
    builder.mix_bool(contract.debug_trace_available);
    builder.mix_bool(contract.source_map_available);
    builder.mix_bool(contract.parser_consumption_enabled);
    builder.mix_bool(contract.standard_library_required);
    builder.mix_bool(contract.runtime_required);
    builder.mix_bool(contract.external_process_required);
    builder.mix_bool(contract.produced_user_generated_code);
    builder.mix_bool(contract.contract_visible);
    builder.mix_bool(contract.query_reusable);
}

void mix_builtin_derive_rollback_diagnostic_design_gate(
    query::StableHashBuilder& builder, const BuiltinDeriveRollbackDiagnosticDesignGate& gate) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M22F_ROLLBACK_GATE_MARKER);
    builder.mix_u32(gate.module.value);
    builder.mix_u32(gate.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(gate.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(gate.generated_part));
    builder.mix_fingerprint(gate.parser_consumption_contract_identity);
    builder.mix_fingerprint(gate.release_gate_identity);
    builder.mix_fingerprint(gate.hardening_matrix_identity);
    builder.mix_fingerprint(gate.debug_dump_contract_identity);
    builder.mix_fingerprint(gate.rollback_gate_identity);
    builder.mix_string(gate.rollback_policy);
    builder.mix_string(gate.rollback_query_name);
    builder.mix_string(gate.blocked_reason);
    builder.mix_u64(gate.diagnostic_projection_count);
    builder.mix_u64(gate.diagnostic_report_entry_count);
    builder.mix_u64(gate.blocked_diagnostic_count);
    builder.mix_u64(gate.derive_diagnostic_count);
    builder.mix_u64(gate.empty_diagnostic_count);
    builder.mix_u64(gate.parser_consumption_contract_count);
    builder.mix_bool(gate.rollback_diagnostic_design_available);
    builder.mix_bool(gate.diagnostic_grouping_available);
    builder.mix_bool(gate.source_anchor_available);
    builder.mix_bool(gate.token_tree_anchor_available);
    builder.mix_bool(gate.debug_dump_contract_available);
    builder.mix_bool(gate.release_rollback_plan_complete);
    builder.mix_bool(gate.rollback_execution_enabled);
    builder.mix_bool(gate.parser_consumption_enabled);
    builder.mix_bool(gate.generated_part_parsed);
    builder.mix_bool(gate.generated_part_merged);
    builder.mix_bool(gate.emit_expanded_available);
    builder.mix_bool(gate.debug_trace_available);
    builder.mix_bool(gate.source_map_available);
    builder.mix_bool(gate.standard_library_required);
    builder.mix_bool(gate.runtime_required);
    builder.mix_bool(gate.external_process_required);
    builder.mix_bool(gate.produced_user_generated_code);
    builder.mix_bool(gate.rollback_gate_visible);
    builder.mix_bool(gate.query_reusable);
}

void mix_builtin_derive_parser_consumption_admission_protocol(
    query::StableHashBuilder& builder,
    const BuiltinDeriveParserConsumptionAdmissionProtocol& protocol) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M23A_ADMISSION_PROTOCOL_MARKER);
    builder.mix_u32(protocol.module.value);
    builder.mix_u32(protocol.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(protocol.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(protocol.generated_part));
    builder.mix_fingerprint(protocol.parser_consumption_contract_identity);
    builder.mix_fingerprint(protocol.release_gate_identity);
    builder.mix_fingerprint(protocol.rollback_gate_identity);
    builder.mix_fingerprint(protocol.admission_protocol_identity);
    builder.mix_string(protocol.admission_policy);
    builder.mix_string(protocol.admission_query_name);
    builder.mix_string(protocol.blocked_reason);
    builder.mix_u64(protocol.token_buffer_count);
    builder.mix_u64(protocol.token_record_count);
    builder.mix_u64(protocol.derive_candidate_count);
    builder.mix_u64(protocol.empty_candidate_count);
    builder.mix_u64(protocol.blocked_diagnostic_count);
    builder.mix_bool(protocol.release_gate_available);
    builder.mix_bool(protocol.rollback_gate_available);
    builder.mix_bool(protocol.parser_contract_available);
    builder.mix_bool(protocol.deterministic_order_available);
    builder.mix_bool(protocol.generated_tokens_checkpointed);
    builder.mix_bool(protocol.admission_protocol_complete);
    builder.mix_bool(protocol.parser_consumption_enabled);
    builder.mix_bool(protocol.parser_admitted);
    builder.mix_bool(protocol.generated_part_parsed);
    builder.mix_bool(protocol.generated_part_merged);
    builder.mix_bool(protocol.emit_expanded_available);
    builder.mix_bool(protocol.debug_trace_available);
    builder.mix_bool(protocol.source_map_available);
    builder.mix_bool(protocol.standard_library_required);
    builder.mix_bool(protocol.runtime_required);
    builder.mix_bool(protocol.external_process_required);
    builder.mix_bool(protocol.produced_user_generated_code);
    builder.mix_bool(protocol.protocol_visible);
    builder.mix_bool(protocol.query_reusable);
}

void mix_builtin_derive_checkpoint_rollback_protocol(
    query::StableHashBuilder& builder,
    const BuiltinDeriveParserConsumptionCheckpointRollbackProtocol& protocol) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M23B_CHECKPOINT_PROTOCOL_MARKER);
    builder.mix_u32(protocol.module.value);
    builder.mix_u32(protocol.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(protocol.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(protocol.generated_part));
    builder.mix_fingerprint(protocol.admission_protocol_identity);
    builder.mix_fingerprint(protocol.rollback_gate_identity);
    builder.mix_fingerprint(protocol.checkpoint_protocol_identity);
    builder.mix_string(protocol.checkpoint_policy);
    builder.mix_string(protocol.checkpoint_query_name);
    builder.mix_string(protocol.blocked_reason);
    builder.mix_u64(protocol.checkpoint_count);
    builder.mix_u64(protocol.rollback_plan_count);
    builder.mix_u64(protocol.token_record_count);
    builder.mix_u64(protocol.diagnostic_anchor_count);
    builder.mix_bool(protocol.parser_state_checkpoint_available);
    builder.mix_bool(protocol.token_cursor_checkpoint_available);
    builder.mix_bool(protocol.generated_part_checkpoint_available);
    builder.mix_bool(protocol.diagnostic_replay_available);
    builder.mix_bool(protocol.rollback_protocol_complete);
    builder.mix_bool(protocol.rollback_execution_enabled);
    builder.mix_bool(protocol.parser_consumption_enabled);
    builder.mix_bool(protocol.generated_part_parsed);
    builder.mix_bool(protocol.generated_part_merged);
    builder.mix_bool(protocol.emit_expanded_available);
    builder.mix_bool(protocol.debug_trace_available);
    builder.mix_bool(protocol.source_map_available);
    builder.mix_bool(protocol.standard_library_required);
    builder.mix_bool(protocol.runtime_required);
    builder.mix_bool(protocol.external_process_required);
    builder.mix_bool(protocol.produced_user_generated_code);
    builder.mix_bool(protocol.protocol_visible);
    builder.mix_bool(protocol.query_reusable);
}

void mix_builtin_derive_preconsumption_verification_closure(
    query::StableHashBuilder& builder,
    const BuiltinDeriveParserPreConsumptionVerificationClosure& closure) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M23C_VERIFICATION_CLOSURE_MARKER);
    builder.mix_u32(closure.module.value);
    builder.mix_u32(closure.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(closure.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(closure.generated_part));
    builder.mix_fingerprint(closure.admission_protocol_identity);
    builder.mix_fingerprint(closure.checkpoint_protocol_identity);
    builder.mix_fingerprint(closure.debug_dump_contract_identity);
    builder.mix_fingerprint(closure.verification_closure_identity);
    builder.mix_string(closure.verification_policy);
    builder.mix_string(closure.verification_query_name);
    builder.mix_string(closure.blocked_reason);
    builder.mix_u64(closure.admission_protocol_count);
    builder.mix_u64(closure.checkpoint_protocol_count);
    builder.mix_u64(closure.hardening_matrix_count);
    builder.mix_u64(closure.debug_dump_contract_count);
    builder.mix_u64(closure.rollback_gate_count);
    builder.mix_bool(closure.admission_protocol_available);
    builder.mix_bool(closure.checkpoint_protocol_available);
    builder.mix_bool(closure.release_hardening_available);
    builder.mix_bool(closure.debug_dump_contract_available);
    builder.mix_bool(closure.rollback_gate_available);
    builder.mix_bool(closure.verification_closure_complete);
    builder.mix_bool(closure.parser_consumption_enabled);
    builder.mix_bool(closure.generated_part_parsed);
    builder.mix_bool(closure.generated_part_merged);
    builder.mix_bool(closure.sema_visible);
    builder.mix_bool(closure.emit_expanded_available);
    builder.mix_bool(closure.debug_trace_available);
    builder.mix_bool(closure.source_map_available);
    builder.mix_bool(closure.standard_library_required);
    builder.mix_bool(closure.runtime_required);
    builder.mix_bool(closure.external_process_required);
    builder.mix_bool(closure.produced_user_generated_code);
    builder.mix_bool(closure.closure_visible);
    builder.mix_bool(closure.query_reusable);
}

void mix_builtin_derive_controlled_dry_run_adapter(
    query::StableHashBuilder& builder,
    const BuiltinDeriveControlledParserDryRunAdapter& adapter) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M24A_DRY_RUN_ADAPTER_MARKER);
    builder.mix_u32(adapter.module.value);
    builder.mix_u32(adapter.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(adapter.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(adapter.generated_part));
    builder.mix_fingerprint(adapter.verification_closure_identity);
    builder.mix_fingerprint(adapter.admission_protocol_identity);
    builder.mix_fingerprint(adapter.checkpoint_protocol_identity);
    builder.mix_fingerprint(adapter.dry_run_adapter_identity);
    builder.mix_string(adapter.adapter_policy);
    builder.mix_string(adapter.adapter_query_name);
    builder.mix_string(adapter.blocked_reason);
    builder.mix_u64(adapter.token_record_count);
    builder.mix_u64(adapter.diagnostic_anchor_count);
    builder.mix_u64(adapter.prerequisite_count);
    builder.mix_bool(adapter.verification_closure_available);
    builder.mix_bool(adapter.admission_protocol_available);
    builder.mix_bool(adapter.checkpoint_protocol_available);
    builder.mix_bool(adapter.compiler_owned_tokens_available);
    builder.mix_bool(adapter.diagnostic_replay_available);
    builder.mix_bool(adapter.dry_run_adapter_complete);
    builder.mix_bool(adapter.dry_run_executed);
    builder.mix_bool(adapter.parser_consumption_enabled);
    builder.mix_bool(adapter.parser_admitted);
    builder.mix_bool(adapter.generated_part_parsed);
    builder.mix_bool(adapter.generated_part_merged);
    builder.mix_bool(adapter.sema_visible);
    builder.mix_bool(adapter.emit_expanded_available);
    builder.mix_bool(adapter.debug_trace_available);
    builder.mix_bool(adapter.source_map_available);
    builder.mix_bool(adapter.standard_library_required);
    builder.mix_bool(adapter.runtime_required);
    builder.mix_bool(adapter.external_process_required);
    builder.mix_bool(adapter.produced_user_generated_code);
    builder.mix_bool(adapter.adapter_visible);
    builder.mix_bool(adapter.query_reusable);
}

void mix_builtin_derive_dry_run_rollback_replay(
    query::StableHashBuilder& builder,
    const BuiltinDeriveDryRunRollbackDiagnosticReplay& replay) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M24B_ROLLBACK_REPLAY_MARKER);
    builder.mix_u32(replay.module.value);
    builder.mix_u32(replay.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(replay.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(replay.generated_part));
    builder.mix_fingerprint(replay.dry_run_adapter_identity);
    builder.mix_fingerprint(replay.checkpoint_protocol_identity);
    builder.mix_fingerprint(replay.rollback_gate_identity);
    builder.mix_fingerprint(replay.replay_protocol_identity);
    builder.mix_string(replay.replay_policy);
    builder.mix_string(replay.replay_query_name);
    builder.mix_string(replay.blocked_reason);
    builder.mix_u64(replay.diagnostic_anchor_count);
    builder.mix_u64(replay.report_entry_count);
    builder.mix_u64(replay.planned_replay_count);
    builder.mix_u64(replay.executed_replay_count);
    builder.mix_bool(replay.dry_run_adapter_available);
    builder.mix_bool(replay.checkpoint_protocol_available);
    builder.mix_bool(replay.rollback_gate_available);
    builder.mix_bool(replay.diagnostic_replay_plan_available);
    builder.mix_bool(replay.replay_protocol_complete);
    builder.mix_bool(replay.replay_execution_enabled);
    builder.mix_bool(replay.dry_run_executed);
    builder.mix_bool(replay.parser_consumption_enabled);
    builder.mix_bool(replay.generated_part_parsed);
    builder.mix_bool(replay.generated_part_merged);
    builder.mix_bool(replay.sema_visible);
    builder.mix_bool(replay.emit_expanded_available);
    builder.mix_bool(replay.debug_trace_available);
    builder.mix_bool(replay.source_map_available);
    builder.mix_bool(replay.standard_library_required);
    builder.mix_bool(replay.runtime_required);
    builder.mix_bool(replay.external_process_required);
    builder.mix_bool(replay.produced_user_generated_code);
    builder.mix_bool(replay.replay_visible);
    builder.mix_bool(replay.query_reusable);
}

void mix_builtin_derive_dry_run_negative_matrix(
    query::StableHashBuilder& builder,
    const BuiltinDeriveDryRunNegativeMatrixClosure& matrix) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M24C_NEGATIVE_MATRIX_MARKER);
    builder.mix_u32(matrix.module.value);
    builder.mix_u32(matrix.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(matrix.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(matrix.generated_part));
    builder.mix_fingerprint(matrix.dry_run_adapter_identity);
    builder.mix_fingerprint(matrix.rollback_replay_identity);
    builder.mix_fingerprint(matrix.verification_closure_identity);
    builder.mix_fingerprint(matrix.negative_matrix_identity);
    builder.mix_string(matrix.matrix_policy);
    builder.mix_string(matrix.matrix_query_name);
    builder.mix_string(matrix.blocked_reason);
    builder.mix_u64(matrix.dry_run_adapter_count);
    builder.mix_u64(matrix.rollback_replay_count);
    builder.mix_u64(matrix.verification_closure_count);
    builder.mix_u64(matrix.negative_case_count);
    builder.mix_u64(matrix.parser_consumable_case_count);
    builder.mix_bool(matrix.dry_run_adapter_available);
    builder.mix_bool(matrix.rollback_replay_available);
    builder.mix_bool(matrix.verification_closure_available);
    builder.mix_bool(matrix.negative_matrix_complete);
    builder.mix_bool(matrix.dry_run_executed);
    builder.mix_bool(matrix.parser_consumption_enabled);
    builder.mix_bool(matrix.parser_admitted);
    builder.mix_bool(matrix.generated_part_parsed);
    builder.mix_bool(matrix.generated_part_merged);
    builder.mix_bool(matrix.sema_visible);
    builder.mix_bool(matrix.emit_expanded_available);
    builder.mix_bool(matrix.debug_trace_available);
    builder.mix_bool(matrix.source_map_available);
    builder.mix_bool(matrix.standard_library_required);
    builder.mix_bool(matrix.runtime_required);
    builder.mix_bool(matrix.external_process_required);
    builder.mix_bool(matrix.produced_user_generated_code);
    builder.mix_bool(matrix.matrix_visible);
    builder.mix_bool(matrix.query_reusable);
}

void mix_builtin_derive_parser_dry_run_session(
    query::StableHashBuilder& builder,
    const BuiltinDeriveParserDryRunSessionBoundary& session) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M25A_DRY_RUN_SESSION_MARKER);
    builder.mix_u32(session.module.value);
    builder.mix_u32(session.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(session.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(session.generated_part));
    builder.mix_fingerprint(session.dry_run_adapter_identity);
    builder.mix_fingerprint(session.negative_matrix_identity);
    builder.mix_fingerprint(session.generated_buffer_identity);
    builder.mix_fingerprint(session.parse_config_fingerprint);
    builder.mix_fingerprint(session.dry_run_session_identity);
    builder.mix_string(session.session_policy);
    builder.mix_string(session.session_query_name);
    builder.mix_string(session.blocked_reason);
    builder.mix_u64(session.token_buffer_candidate_count);
    builder.mix_u64(session.token_record_count);
    builder.mix_u64(session.diagnostic_anchor_count);
    builder.mix_u64(session.parser_state_snapshot_count);
    builder.mix_u64(session.committed_parse_count);
    builder.mix_bool(session.dry_run_adapter_available);
    builder.mix_bool(session.negative_matrix_available);
    builder.mix_bool(session.compiler_owned_token_stream_available);
    builder.mix_bool(session.sandbox_available);
    builder.mix_bool(session.check_only);
    builder.mix_bool(session.dry_run_session_complete);
    builder.mix_bool(session.dry_run_executed);
    builder.mix_bool(session.session_committed);
    builder.mix_bool(session.parser_consumption_enabled);
    builder.mix_bool(session.parser_admitted);
    builder.mix_bool(session.parser_cursor_advanced);
    builder.mix_bool(session.generated_part_parsed);
    builder.mix_bool(session.generated_part_merged);
    builder.mix_bool(session.ast_mutated);
    builder.mix_bool(session.sema_visible);
    builder.mix_bool(session.emit_expanded_available);
    builder.mix_bool(session.debug_trace_available);
    builder.mix_bool(session.source_map_available);
    builder.mix_bool(session.standard_library_required);
    builder.mix_bool(session.runtime_required);
    builder.mix_bool(session.external_process_required);
    builder.mix_bool(session.produced_user_generated_code);
    builder.mix_bool(session.session_visible);
    builder.mix_bool(session.query_reusable);
}

void mix_builtin_derive_token_cursor_snapshot_proof(
    query::StableHashBuilder& builder,
    const BuiltinDeriveTokenCursorSnapshotRollbackProof& proof) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M25B_CURSOR_SNAPSHOT_MARKER);
    builder.mix_u32(proof.module.value);
    builder.mix_u32(proof.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(proof.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(proof.generated_part));
    builder.mix_fingerprint(proof.dry_run_session_identity);
    builder.mix_fingerprint(proof.checkpoint_protocol_identity);
    builder.mix_fingerprint(proof.rollback_replay_identity);
    builder.mix_fingerprint(proof.cursor_snapshot_identity);
    builder.mix_string(proof.snapshot_policy);
    builder.mix_string(proof.snapshot_query_name);
    builder.mix_string(proof.blocked_reason);
    builder.mix_u64(proof.token_record_count);
    builder.mix_u64(proof.checkpoint_count);
    builder.mix_u64(proof.cursor_snapshot_count);
    builder.mix_u64(proof.parser_state_snapshot_count);
    builder.mix_u64(proof.rollback_proof_count);
    builder.mix_u64(proof.cursor_commit_count);
    builder.mix_bool(proof.dry_run_session_available);
    builder.mix_bool(proof.checkpoint_protocol_available);
    builder.mix_bool(proof.rollback_replay_available);
    builder.mix_bool(proof.token_cursor_snapshot_available);
    builder.mix_bool(proof.parser_state_snapshot_available);
    builder.mix_bool(proof.rollback_proof_complete);
    builder.mix_bool(proof.replay_execution_enabled);
    builder.mix_bool(proof.rollback_execution_enabled);
    builder.mix_bool(proof.dry_run_executed);
    builder.mix_bool(proof.parser_cursor_advanced);
    builder.mix_bool(proof.session_committed);
    builder.mix_bool(proof.parser_consumption_enabled);
    builder.mix_bool(proof.parser_admitted);
    builder.mix_bool(proof.generated_part_parsed);
    builder.mix_bool(proof.generated_part_merged);
    builder.mix_bool(proof.ast_mutated);
    builder.mix_bool(proof.sema_visible);
    builder.mix_bool(proof.standard_library_required);
    builder.mix_bool(proof.runtime_required);
    builder.mix_bool(proof.external_process_required);
    builder.mix_bool(proof.produced_user_generated_code);
    builder.mix_bool(proof.proof_visible);
    builder.mix_bool(proof.query_reusable);
}

void mix_builtin_derive_diagnostic_shadow_no_ast_mutation_closure(
    query::StableHashBuilder& builder,
    const BuiltinDeriveDiagnosticShadowNoAstMutationClosure& closure) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M25C_DIAGNOSTIC_SHADOW_MARKER);
    builder.mix_u32(closure.module.value);
    builder.mix_u32(closure.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(closure.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(closure.generated_part));
    builder.mix_fingerprint(closure.dry_run_session_identity);
    builder.mix_fingerprint(closure.cursor_snapshot_identity);
    builder.mix_fingerprint(closure.rollback_replay_identity);
    builder.mix_fingerprint(closure.negative_matrix_identity);
    builder.mix_fingerprint(closure.closure_identity);
    builder.mix_string(closure.closure_policy);
    builder.mix_string(closure.closure_query_name);
    builder.mix_string(closure.blocked_reason);
    builder.mix_u64(closure.dry_run_session_count);
    builder.mix_u64(closure.cursor_snapshot_proof_count);
    builder.mix_u64(closure.rollback_replay_count);
    builder.mix_u64(closure.negative_matrix_count);
    builder.mix_u64(closure.diagnostic_shadow_count);
    builder.mix_u64(closure.executed_shadow_count);
    builder.mix_u64(closure.ast_mutation_count);
    builder.mix_u64(closure.parser_consumable_case_count);
    builder.mix_bool(closure.dry_run_session_available);
    builder.mix_bool(closure.cursor_snapshot_proof_available);
    builder.mix_bool(closure.rollback_replay_available);
    builder.mix_bool(closure.negative_matrix_available);
    builder.mix_bool(closure.diagnostic_shadow_available);
    builder.mix_bool(closure.no_ast_mutation_verified);
    builder.mix_bool(closure.closure_complete);
    builder.mix_bool(closure.dry_run_executed);
    builder.mix_bool(closure.replay_execution_enabled);
    builder.mix_bool(closure.session_committed);
    builder.mix_bool(closure.parser_cursor_advanced);
    builder.mix_bool(closure.parser_consumption_enabled);
    builder.mix_bool(closure.parser_admitted);
    builder.mix_bool(closure.generated_part_parsed);
    builder.mix_bool(closure.generated_part_merged);
    builder.mix_bool(closure.ast_mutated);
    builder.mix_bool(closure.sema_visible);
    builder.mix_bool(closure.emit_expanded_available);
    builder.mix_bool(closure.debug_trace_available);
    builder.mix_bool(closure.source_map_available);
    builder.mix_bool(closure.standard_library_required);
    builder.mix_bool(closure.runtime_required);
    builder.mix_bool(closure.external_process_required);
    builder.mix_bool(closure.produced_user_generated_code);
    builder.mix_bool(closure.closure_visible);
    builder.mix_bool(closure.query_reusable);
}

void mix_builtin_derive_parser_dry_run_admission_gate(
    query::StableHashBuilder& builder,
    const BuiltinDeriveParserDryRunAdmissionGate& gate) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M26A_DRY_RUN_ADMISSION_GATE_MARKER);
    builder.mix_u32(gate.module.value);
    builder.mix_u32(gate.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(gate.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(gate.generated_part));
    builder.mix_fingerprint(gate.dry_run_session_identity);
    builder.mix_fingerprint(gate.cursor_snapshot_identity);
    builder.mix_fingerprint(gate.diagnostic_shadow_closure_identity);
    builder.mix_fingerprint(gate.generated_buffer_identity);
    builder.mix_fingerprint(gate.parse_config_fingerprint);
    builder.mix_fingerprint(gate.admission_gate_identity);
    builder.mix_string(gate.admission_policy);
    builder.mix_string(gate.admission_query_name);
    builder.mix_string(gate.blocked_reason);
    builder.mix_u64(gate.dry_run_session_count);
    builder.mix_u64(gate.cursor_snapshot_proof_count);
    builder.mix_u64(gate.diagnostic_shadow_closure_count);
    builder.mix_u64(gate.admission_prerequisite_count);
    builder.mix_u64(gate.token_buffer_candidate_count);
    builder.mix_u64(gate.token_record_count);
    builder.mix_u64(gate.dry_run_execution_admitted_count);
    builder.mix_u64(gate.parser_consumable_case_count);
    builder.mix_bool(gate.dry_run_session_available);
    builder.mix_bool(gate.cursor_snapshot_proof_available);
    builder.mix_bool(gate.diagnostic_shadow_closure_available);
    builder.mix_bool(gate.generated_buffer_available);
    builder.mix_bool(gate.parse_config_available);
    builder.mix_bool(gate.admission_gate_complete);
    builder.mix_bool(gate.dry_run_execution_admitted);
    builder.mix_bool(gate.dry_run_executed);
    builder.mix_bool(gate.diagnostic_shadow_executed);
    builder.mix_bool(gate.rollback_execution_enabled);
    builder.mix_bool(gate.session_committed);
    builder.mix_bool(gate.parser_cursor_advanced);
    builder.mix_bool(gate.parser_consumption_enabled);
    builder.mix_bool(gate.parser_admitted);
    builder.mix_bool(gate.generated_part_parsed);
    builder.mix_bool(gate.generated_part_merged);
    builder.mix_bool(gate.ast_mutated);
    builder.mix_bool(gate.sema_visible);
    builder.mix_bool(gate.emit_expanded_available);
    builder.mix_bool(gate.debug_trace_available);
    builder.mix_bool(gate.source_map_available);
    builder.mix_bool(gate.standard_library_required);
    builder.mix_bool(gate.runtime_required);
    builder.mix_bool(gate.external_process_required);
    builder.mix_bool(gate.produced_user_generated_code);
    builder.mix_bool(gate.gate_visible);
    builder.mix_bool(gate.query_reusable);
}

void mix_builtin_derive_error_recovery_shadow_diagnostic_gate(
    query::StableHashBuilder& builder,
    const BuiltinDeriveErrorRecoveryShadowDiagnosticGate& gate) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M26B_RECOVERY_SHADOW_GATE_MARKER);
    builder.mix_u32(gate.module.value);
    builder.mix_u32(gate.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(gate.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(gate.generated_part));
    builder.mix_fingerprint(gate.dry_run_admission_gate_identity);
    builder.mix_fingerprint(gate.diagnostic_shadow_closure_identity);
    builder.mix_fingerprint(gate.rollback_replay_identity);
    builder.mix_fingerprint(gate.parser_report_identity);
    builder.mix_fingerprint(gate.recovery_shadow_identity);
    builder.mix_string(gate.recovery_policy);
    builder.mix_string(gate.recovery_query_name);
    builder.mix_string(gate.blocked_reason);
    builder.mix_u64(gate.diagnostic_shadow_count);
    builder.mix_u64(gate.report_entry_count);
    builder.mix_u64(gate.planned_recovery_count);
    builder.mix_u64(gate.executed_recovery_count);
    builder.mix_u64(gate.emitted_diagnostic_count);
    builder.mix_bool(gate.dry_run_admission_gate_available);
    builder.mix_bool(gate.diagnostic_shadow_closure_available);
    builder.mix_bool(gate.rollback_replay_available);
    builder.mix_bool(gate.parser_report_available);
    builder.mix_bool(gate.recovery_shadow_plan_available);
    builder.mix_bool(gate.recovery_shadow_complete);
    builder.mix_bool(gate.recovery_execution_enabled);
    builder.mix_bool(gate.diagnostic_emission_enabled);
    builder.mix_bool(gate.dry_run_execution_admitted);
    builder.mix_bool(gate.dry_run_executed);
    builder.mix_bool(gate.rollback_execution_enabled);
    builder.mix_bool(gate.session_committed);
    builder.mix_bool(gate.parser_cursor_advanced);
    builder.mix_bool(gate.parser_consumption_enabled);
    builder.mix_bool(gate.parser_admitted);
    builder.mix_bool(gate.generated_part_parsed);
    builder.mix_bool(gate.generated_part_merged);
    builder.mix_bool(gate.ast_mutated);
    builder.mix_bool(gate.sema_visible);
    builder.mix_bool(gate.emit_expanded_available);
    builder.mix_bool(gate.debug_trace_available);
    builder.mix_bool(gate.source_map_available);
    builder.mix_bool(gate.standard_library_required);
    builder.mix_bool(gate.runtime_required);
    builder.mix_bool(gate.external_process_required);
    builder.mix_bool(gate.produced_user_generated_code);
    builder.mix_bool(gate.gate_visible);
    builder.mix_bool(gate.query_reusable);
}

void mix_builtin_derive_cursor_rollback_ast_mutation_verifier_closure(
    query::StableHashBuilder& builder,
    const BuiltinDeriveCursorRollbackAstMutationVerifierClosure& closure) noexcept
{
    builder.mix_string(FRONTEND_MACRO_M26C_ROLLBACK_AST_VERIFIER_MARKER);
    builder.mix_u32(closure.module.value);
    builder.mix_u32(closure.source_part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(closure.attached_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(closure.generated_part));
    builder.mix_fingerprint(closure.dry_run_admission_gate_identity);
    builder.mix_fingerprint(closure.recovery_shadow_identity);
    builder.mix_fingerprint(closure.cursor_snapshot_identity);
    builder.mix_fingerprint(closure.dry_run_session_identity);
    builder.mix_fingerprint(closure.diagnostic_shadow_closure_identity);
    builder.mix_fingerprint(closure.verifier_closure_identity);
    builder.mix_string(closure.verifier_policy);
    builder.mix_string(closure.verifier_query_name);
    builder.mix_string(closure.blocked_reason);
    builder.mix_u64(closure.cursor_snapshot_count);
    builder.mix_u64(closure.rollback_proof_count);
    builder.mix_u64(closure.recovery_shadow_count);
    builder.mix_u64(closure.ast_baseline_snapshot_count);
    builder.mix_u64(closure.ast_mutation_count);
    builder.mix_u64(closure.cursor_commit_count);
    builder.mix_u64(closure.session_commit_count);
    builder.mix_u64(closure.parser_consumable_case_count);
    builder.mix_bool(closure.dry_run_admission_gate_available);
    builder.mix_bool(closure.recovery_shadow_available);
    builder.mix_bool(closure.cursor_snapshot_proof_available);
    builder.mix_bool(closure.dry_run_session_available);
    builder.mix_bool(closure.diagnostic_shadow_closure_available);
    builder.mix_bool(closure.ast_baseline_available);
    builder.mix_bool(closure.rollback_execution_guard_available);
    builder.mix_bool(closure.ast_mutation_verifier_complete);
    builder.mix_bool(closure.rollback_execution_enabled);
    builder.mix_bool(closure.recovery_execution_enabled);
    builder.mix_bool(closure.diagnostic_emission_enabled);
    builder.mix_bool(closure.dry_run_execution_admitted);
    builder.mix_bool(closure.dry_run_executed);
    builder.mix_bool(closure.session_committed);
    builder.mix_bool(closure.parser_cursor_advanced);
    builder.mix_bool(closure.parser_consumption_enabled);
    builder.mix_bool(closure.parser_admitted);
    builder.mix_bool(closure.generated_part_parsed);
    builder.mix_bool(closure.generated_part_merged);
    builder.mix_bool(closure.ast_mutated);
    builder.mix_bool(closure.sema_visible);
    builder.mix_bool(closure.emit_expanded_available);
    builder.mix_bool(closure.debug_trace_available);
    builder.mix_bool(closure.source_map_available);
    builder.mix_bool(closure.standard_library_required);
    builder.mix_bool(closure.runtime_required);
    builder.mix_bool(closure.external_process_required);
    builder.mix_bool(closure.produced_user_generated_code);
    builder.mix_bool(closure.closure_visible);
    builder.mix_bool(closure.query_reusable);
}

void mix_aurex_macro_surface_admission_gate(
    query::StableHashBuilder& builder,
    const AurexMacroSurfaceAdmissionGate& gate) noexcept
{
    builder.mix_u32(gate.item.value);
    builder.mix_u32(gate.module.value);
    builder.mix_u32(gate.part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(gate.attached_part));
    builder.mix_fingerprint(gate.body_fingerprint);
    builder.mix_fingerprint(gate.admission_identity);
    builder.mix_u8(static_cast<base::u8>(gate.macro_kind));
    builder.mix_string(gate.macro_name);
    builder.mix_string(gate.admission_policy);
    builder.mix_string(gate.query_name);
    builder.mix_string(gate.blocker_reason);
    builder.mix_u64(static_cast<base::u64>(gate.macro_range.source.value));
    builder.mix_u64(static_cast<base::u64>(gate.macro_range.begin));
    builder.mix_u64(static_cast<base::u64>(gate.macro_range.end));
    builder.mix_u64(static_cast<base::u64>(gate.body_range.source.value));
    builder.mix_u64(static_cast<base::u64>(gate.body_range.begin));
    builder.mix_u64(static_cast<base::u64>(gate.body_range.end));
    builder.mix_u64(gate.body_token_count);
    builder.mix_u64(gate.match_clause_count);
    builder.mix_bool(gate.body_balanced);
    builder.mix_bool(gate.declarative_surface);
    builder.mix_bool(gate.user_derive_surface);
    builder.mix_bool(gate.compile_time_execution_surface);
    builder.mix_bool(gate.expansion_enabled);
    builder.mix_bool(gate.compile_time_execution_enabled);
    builder.mix_bool(gate.ast_mutated);
    builder.mix_bool(gate.parser_consumption_enabled);
    builder.mix_bool(gate.sema_visible_generated_items);
    builder.mix_bool(gate.standard_library_required);
    builder.mix_bool(gate.runtime_required);
    builder.mix_bool(gate.external_process_required);
    builder.mix_bool(gate.produced_user_generated_code);
    builder.mix_bool(gate.gate_visible);
    builder.mix_bool(gate.query_reusable);
}

void mix_aurex_macro_definition_site_hygiene_gate(
    query::StableHashBuilder& builder,
    const AurexMacroDefinitionSiteHygieneAdmissionGate& gate) noexcept
{
    builder.mix_u32(gate.item.value);
    builder.mix_u32(gate.module.value);
    builder.mix_u32(gate.part_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(gate.attached_part));
    builder.mix_fingerprint(gate.surface_admission_identity);
    builder.mix_fingerprint(gate.body_fingerprint);
    builder.mix_fingerprint(gate.definition_site_mark);
    builder.mix_fingerprint(gate.fresh_name_scope);
    builder.mix_fingerprint(gate.diagnostic_anchor_identity);
    builder.mix_fingerprint(gate.hygiene_identity);
    builder.mix_u8(static_cast<base::u8>(gate.macro_kind));
    builder.mix_string(gate.macro_name);
    builder.mix_string(gate.hygiene_policy);
    builder.mix_string(gate.query_name);
    builder.mix_string(gate.blocker_reason);
    builder.mix_u64(static_cast<base::u64>(gate.macro_range.source.value));
    builder.mix_u64(static_cast<base::u64>(gate.macro_range.begin));
    builder.mix_u64(static_cast<base::u64>(gate.macro_range.end));
    builder.mix_u64(static_cast<base::u64>(gate.body_range.source.value));
    builder.mix_u64(static_cast<base::u64>(gate.body_range.begin));
    builder.mix_u64(static_cast<base::u64>(gate.body_range.end));
    builder.mix_bool(gate.definition_site_scope_available);
    builder.mix_bool(gate.fresh_name_scope_reserved);
    builder.mix_bool(gate.diagnostic_anchor_available);
    builder.mix_bool(gate.hygiene_resolution_enabled);
    builder.mix_bool(gate.declared_names_visible);
    builder.mix_bool(gate.captures_call_site_locals);
    builder.mix_bool(gate.standard_library_required);
    builder.mix_bool(gate.runtime_required);
    builder.mix_bool(gate.external_process_required);
    builder.mix_bool(gate.produced_user_generated_code);
    builder.mix_bool(gate.gate_visible);
    builder.mix_bool(gate.query_reusable);
}

void mix_aurex_macro_typed_matcher_admission_gate(
    query::StableHashBuilder& builder,
    const AurexMacroTypedMatcherAdmissionGate& gate) noexcept
{
    builder.mix_u32(gate.item.value);
    builder.mix_u32(gate.module.value);
    builder.mix_u32(gate.part_index);
    builder.mix_u32(gate.matcher_index);
    builder.mix_fingerprint(query::stable_key_fingerprint(gate.attached_part));
    builder.mix_fingerprint(gate.surface_admission_identity);
    builder.mix_fingerprint(gate.body_fingerprint);
    builder.mix_fingerprint(gate.matcher_fingerprint);
    builder.mix_fingerprint(gate.matcher_identity);
    builder.mix_fingerprint(gate.definition_site_hygiene_identity);
    builder.mix_fingerprint(gate.diagnostic_anchor_identity);
    builder.mix_u8(static_cast<base::u8>(gate.macro_kind));
    builder.mix_u8(static_cast<base::u8>(gate.matcher_kind));
    builder.mix_string(gate.macro_name);
    builder.mix_string(gate.matcher_head);
    builder.mix_string(gate.binding_name);
    builder.mix_string(gate.matcher_policy);
    builder.mix_string(gate.query_name);
    builder.mix_string(gate.blocker_reason);
    builder.mix_u64(static_cast<base::u64>(gate.matcher_range.source.value));
    builder.mix_u64(static_cast<base::u64>(gate.matcher_range.begin));
    builder.mix_u64(static_cast<base::u64>(gate.matcher_range.end));
    builder.mix_u64(static_cast<base::u64>(gate.output_range.source.value));
    builder.mix_u64(static_cast<base::u64>(gate.output_range.begin));
    builder.mix_u64(static_cast<base::u64>(gate.output_range.end));
    builder.mix_bool(gate.matcher_shape_recognized);
    builder.mix_bool(gate.expr_list_matcher);
    builder.mix_bool(gate.item_matcher);
    builder.mix_bool(gate.token_stream_matcher);
    builder.mix_bool(gate.unknown_matcher);
    builder.mix_bool(gate.definition_site_hygiene_available);
    builder.mix_bool(gate.fresh_name_scope_available);
    builder.mix_bool(gate.diagnostic_anchor_available);
    builder.mix_bool(gate.matcher_execution_enabled);
    builder.mix_bool(gate.expansion_enabled);
    builder.mix_bool(gate.compile_time_execution_enabled);
    builder.mix_bool(gate.parser_consumption_enabled);
    builder.mix_bool(gate.ast_mutated);
    builder.mix_bool(gate.sema_visible_generated_items);
    builder.mix_bool(gate.standard_library_required);
    builder.mix_bool(gate.runtime_required);
    builder.mix_bool(gate.external_process_required);
    builder.mix_bool(gate.produced_user_generated_code);
    builder.mix_bool(gate.gate_visible);
    builder.mix_bool(gate.query_reusable);
}

void mix_summary(query::StableHashBuilder& builder, const EarlyItemExpansionSummary& summary) noexcept
{
    builder.mix_u64(summary.macro_input_count);
    builder.mix_u64(summary.attribute_input_count);
    builder.mix_u64(summary.builtin_derive_passthrough_count);
    builder.mix_u64(summary.blocked_attribute_count);
    builder.mix_u64(summary.generated_part_placeholder_count);
    builder.mix_u64(summary.generated_part_stub_count);
    builder.mix_u64(summary.materialized_buffer_stub_count);
    builder.mix_u64(summary.parse_blocked_count);
    builder.mix_u64(summary.merge_blocked_count);
    builder.mix_u64(summary.sema_visible_generated_part_count);
    builder.mix_u64(summary.source_map_placeholder_count);
    builder.mix_u64(summary.hygiene_stub_count);
    builder.mix_u64(summary.unresolved_hygiene_stub_count);
    builder.mix_u64(summary.declared_name_stub_count);
    builder.mix_u64(summary.call_site_capture_count);
    builder.mix_u64(summary.trace_stub_count);
    builder.mix_u64(summary.real_source_map_count);
    builder.mix_u64(summary.debug_trace_available_count);
    builder.mix_u64(summary.cli_emit_expanded_available_count);
    builder.mix_u64(summary.generated_item_declaration_stub_count);
    builder.mix_u64(summary.planned_generated_item_declaration_count);
    builder.mix_u64(summary.materialized_generated_item_count);
    builder.mix_u64(summary.declared_generated_name_stub_count);
    builder.mix_u64(summary.lookup_visible_declared_name_count);
    builder.mix_u64(summary.export_visible_declared_name_count);
    builder.mix_u64(summary.token_materialization_admission_stub_count);
    builder.mix_u64(summary.compiler_owned_admission_count);
    builder.mix_u64(summary.admitted_token_materialization_count);
    builder.mix_u64(summary.materialized_token_admission_count);
    builder.mix_u64(summary.generated_token_buffer_stub_count);
    builder.mix_u64(summary.empty_generated_token_buffer_count);
    builder.mix_u64(summary.materialized_token_buffer_count);
    builder.mix_u64(summary.compiler_owned_token_buffer_count);
    builder.mix_u64(summary.generated_token_record_count);
    builder.mix_u64(summary.compiler_owned_generated_token_record_count);
    builder.mix_u64(summary.parser_visible_generated_token_count);
    builder.mix_u64(summary.parser_admission_gate_stub_count);
    builder.mix_u64(summary.compiler_owned_parser_admission_gate_count);
    builder.mix_u64(summary.token_record_available_gate_count);
    builder.mix_u64(summary.parser_blocked_token_buffer_count);
    builder.mix_u64(summary.parser_admitted_token_buffer_count);
    builder.mix_u64(summary.parser_admission_diagnostic_stub_count);
    builder.mix_u64(summary.parser_admission_diagnostic_blocked_count);
    builder.mix_u64(summary.derive_parser_admission_diagnostic_count);
    builder.mix_u64(summary.empty_parser_admission_diagnostic_count);
    builder.mix_u64(summary.emit_expanded_projection_available_count);
    builder.mix_u64(summary.parser_admission_debug_trace_projection_count);
    builder.mix_u64(summary.parser_admission_source_map_projection_count);
    builder.mix_u64(summary.parser_admission_report_entry_count);
    builder.mix_u64(summary.parser_admission_report_count);
    builder.mix_u64(summary.parser_admission_report_blocked_entry_count);
    builder.mix_u64(summary.parser_admission_report_derive_entry_count);
    builder.mix_u64(summary.parser_admission_report_empty_entry_count);
    builder.mix_u64(summary.parser_admission_report_token_record_available_entry_count);
    builder.mix_u64(summary.parser_admission_report_visible_count);
    builder.mix_u64(summary.parser_admission_report_query_reusable_count);
    builder.mix_u64(summary.parser_admission_report_unordered_anchor_count);
    builder.mix_u64(summary.parser_admission_report_parser_consumable_count);
    builder.mix_u64(summary.parser_readiness_preflight_entry_count);
    builder.mix_u64(summary.parser_readiness_preflight_blocked_count);
    builder.mix_u64(summary.parser_readiness_preflight_derive_entry_count);
    builder.mix_u64(summary.parser_readiness_preflight_empty_entry_count);
    builder.mix_u64(summary.parser_readiness_preflight_contiguous_index_count);
    builder.mix_u64(summary.parser_readiness_preflight_delimiter_balanced_count);
    builder.mix_u64(summary.parser_readiness_preflight_source_anchor_covered_count);
    builder.mix_u64(summary.parser_readiness_preflight_parse_config_compatible_count);
    builder.mix_u64(summary.parser_readiness_preflight_parser_consumable_count);
    builder.mix_u64(summary.parser_consumption_contract_gate_count);
    builder.mix_u64(summary.parser_consumption_contract_blocked_gate_count);
    builder.mix_u64(summary.parser_consumption_contract_visible_count);
    builder.mix_u64(summary.parser_consumption_contract_query_reusable_count);
    builder.mix_u64(summary.parser_consumption_contract_parser_consumable_count);
    builder.mix_u64(summary.macro_boundary_closure_report_count);
    builder.mix_u64(summary.macro_boundary_closure_visible_count);
    builder.mix_u64(summary.macro_boundary_closure_query_reusable_count);
    builder.mix_u64(summary.macro_boundary_closure_complete_count);
    builder.mix_u64(summary.macro_boundary_closure_parser_consumption_enabled_count);
    builder.mix_u64(summary.builtin_derive_expansion_admission_gate_count);
    builder.mix_u64(summary.builtin_derive_expansion_derive_admission_count);
    builder.mix_u64(summary.builtin_derive_expansion_non_derive_blocked_count);
    builder.mix_u64(summary.builtin_derive_expansion_visible_count);
    builder.mix_u64(summary.builtin_derive_expansion_query_reusable_count);
    builder.mix_u64(summary.builtin_derive_expansion_capability_candidate_count);
    builder.mix_u64(summary.builtin_derive_semantic_plan_count);
    builder.mix_u64(summary.builtin_derive_semantic_plan_visible_count);
    builder.mix_u64(summary.builtin_derive_semantic_plan_query_reusable_count);
    builder.mix_u64(summary.builtin_derive_semantic_capability_count);
    builder.mix_u64(summary.builtin_derive_semantic_copy_capability_count);
    builder.mix_u64(summary.builtin_derive_semantic_eq_capability_count);
    builder.mix_u64(summary.builtin_derive_semantic_hash_capability_count);
    builder.mix_u64(summary.builtin_derive_parser_release_gate_count);
    builder.mix_u64(summary.builtin_derive_parser_release_visible_count);
    builder.mix_u64(summary.builtin_derive_parser_release_query_reusable_count);
    builder.mix_u64(summary.builtin_derive_parser_release_parser_consumable_count);
    builder.mix_u64(summary.builtin_derive_release_hardening_matrix_count);
    builder.mix_u64(summary.builtin_derive_release_hardening_visible_count);
    builder.mix_u64(summary.builtin_derive_release_hardening_query_reusable_count);
    builder.mix_u64(summary.builtin_derive_release_hardening_negative_matrix_complete_count);
    builder.mix_u64(summary.builtin_derive_release_hardening_parser_consumable_count);
    builder.mix_u64(summary.builtin_derive_debug_dump_contract_count);
    builder.mix_u64(summary.builtin_derive_debug_dump_contract_visible_count);
    builder.mix_u64(summary.builtin_derive_debug_dump_query_reusable_count);
    builder.mix_u64(summary.builtin_derive_debug_dump_complete_count);
    builder.mix_u64(summary.builtin_derive_debug_dump_parser_consumable_count);
    builder.mix_u64(summary.builtin_derive_rollback_diagnostic_gate_count);
    builder.mix_u64(summary.builtin_derive_rollback_diagnostic_visible_count);
    builder.mix_u64(summary.builtin_derive_rollback_diagnostic_query_reusable_count);
    builder.mix_u64(summary.builtin_derive_rollback_diagnostic_design_complete_count);
    builder.mix_u64(summary.builtin_derive_rollback_diagnostic_parser_consumable_count);
    builder.mix_u64(summary.builtin_derive_parser_consumption_admission_protocol_count);
    builder.mix_u64(summary.builtin_derive_parser_consumption_admission_visible_count);
    builder.mix_u64(summary.builtin_derive_parser_consumption_admission_query_reusable_count);
    builder.mix_u64(summary.builtin_derive_parser_consumption_admission_complete_count);
    builder.mix_u64(summary.builtin_derive_parser_consumption_admission_parser_consumable_count);
    builder.mix_u64(summary.builtin_derive_checkpoint_rollback_protocol_count);
    builder.mix_u64(summary.builtin_derive_checkpoint_rollback_visible_count);
    builder.mix_u64(summary.builtin_derive_checkpoint_rollback_query_reusable_count);
    builder.mix_u64(summary.builtin_derive_checkpoint_rollback_complete_count);
    builder.mix_u64(summary.builtin_derive_checkpoint_rollback_parser_consumable_count);
    builder.mix_u64(summary.builtin_derive_preconsumption_verification_closure_count);
    builder.mix_u64(summary.builtin_derive_preconsumption_verification_visible_count);
    builder.mix_u64(summary.builtin_derive_preconsumption_verification_query_reusable_count);
    builder.mix_u64(summary.builtin_derive_preconsumption_verification_complete_count);
    builder.mix_u64(summary.builtin_derive_preconsumption_verification_parser_consumable_count);
    builder.mix_u64(summary.builtin_derive_controlled_dry_run_adapter_count);
    builder.mix_u64(summary.builtin_derive_controlled_dry_run_adapter_visible_count);
    builder.mix_u64(summary.builtin_derive_controlled_dry_run_adapter_query_reusable_count);
    builder.mix_u64(summary.builtin_derive_controlled_dry_run_adapter_complete_count);
    builder.mix_u64(summary.builtin_derive_controlled_dry_run_adapter_executed_count);
    builder.mix_u64(summary.builtin_derive_dry_run_rollback_replay_count);
    builder.mix_u64(summary.builtin_derive_dry_run_rollback_replay_visible_count);
    builder.mix_u64(summary.builtin_derive_dry_run_rollback_replay_query_reusable_count);
    builder.mix_u64(summary.builtin_derive_dry_run_rollback_replay_complete_count);
    builder.mix_u64(summary.builtin_derive_dry_run_rollback_replay_executed_count);
    builder.mix_u64(summary.builtin_derive_dry_run_negative_matrix_count);
    builder.mix_u64(summary.builtin_derive_dry_run_negative_matrix_visible_count);
    builder.mix_u64(summary.builtin_derive_dry_run_negative_matrix_query_reusable_count);
    builder.mix_u64(summary.builtin_derive_dry_run_negative_matrix_complete_count);
    builder.mix_u64(summary.builtin_derive_dry_run_negative_matrix_parser_consumable_count);
    builder.mix_u64(summary.builtin_derive_parser_dry_run_session_count);
    builder.mix_u64(summary.builtin_derive_parser_dry_run_session_visible_count);
    builder.mix_u64(summary.builtin_derive_parser_dry_run_session_query_reusable_count);
    builder.mix_u64(summary.builtin_derive_parser_dry_run_session_complete_count);
    builder.mix_u64(summary.builtin_derive_parser_dry_run_session_executed_count);
    builder.mix_u64(summary.builtin_derive_parser_dry_run_session_committed_count);
    builder.mix_u64(summary.builtin_derive_token_cursor_snapshot_proof_count);
    builder.mix_u64(summary.builtin_derive_token_cursor_snapshot_proof_visible_count);
    builder.mix_u64(summary.builtin_derive_token_cursor_snapshot_proof_query_reusable_count);
    builder.mix_u64(summary.builtin_derive_token_cursor_snapshot_proof_complete_count);
    builder.mix_u64(summary.builtin_derive_token_cursor_snapshot_proof_cursor_advanced_count);
    builder.mix_u64(summary.builtin_derive_token_cursor_snapshot_proof_committed_count);
    builder.mix_u64(summary.builtin_derive_diagnostic_shadow_no_ast_mutation_closure_count);
    builder.mix_u64(summary.builtin_derive_diagnostic_shadow_no_ast_mutation_visible_count);
    builder.mix_u64(summary.builtin_derive_diagnostic_shadow_no_ast_mutation_query_reusable_count);
    builder.mix_u64(summary.builtin_derive_diagnostic_shadow_no_ast_mutation_complete_count);
    builder.mix_u64(summary.builtin_derive_diagnostic_shadow_no_ast_mutation_executed_count);
    builder.mix_u64(summary.builtin_derive_diagnostic_shadow_no_ast_mutation_ast_mutation_count);
    builder.mix_u64(summary.builtin_derive_diagnostic_shadow_no_ast_mutation_parser_consumable_count);
    builder.mix_u64(summary.builtin_derive_parser_dry_run_admission_gate_count);
    builder.mix_u64(summary.builtin_derive_parser_dry_run_admission_gate_visible_count);
    builder.mix_u64(summary.builtin_derive_parser_dry_run_admission_gate_query_reusable_count);
    builder.mix_u64(summary.builtin_derive_parser_dry_run_admission_gate_complete_count);
    builder.mix_u64(summary.builtin_derive_parser_dry_run_admission_gate_execution_admitted_count);
    builder.mix_u64(summary.builtin_derive_parser_dry_run_admission_gate_executed_count);
    builder.mix_u64(summary.builtin_derive_parser_dry_run_admission_gate_parser_consumable_count);
    builder.mix_u64(summary.builtin_derive_error_recovery_shadow_diagnostic_gate_count);
    builder.mix_u64(summary.builtin_derive_error_recovery_shadow_diagnostic_gate_visible_count);
    builder.mix_u64(summary.builtin_derive_error_recovery_shadow_diagnostic_gate_query_reusable_count);
    builder.mix_u64(summary.builtin_derive_error_recovery_shadow_diagnostic_gate_complete_count);
    builder.mix_u64(summary.builtin_derive_error_recovery_shadow_diagnostic_gate_recovery_executed_count);
    builder.mix_u64(summary.builtin_derive_error_recovery_shadow_diagnostic_gate_diagnostic_emitted_count);
    builder.mix_u64(summary.builtin_derive_error_recovery_shadow_diagnostic_gate_parser_consumable_count);
    builder.mix_u64(summary.builtin_derive_cursor_rollback_ast_mutation_verifier_closure_count);
    builder.mix_u64(summary.builtin_derive_cursor_rollback_ast_mutation_verifier_visible_count);
    builder.mix_u64(summary.builtin_derive_cursor_rollback_ast_mutation_verifier_query_reusable_count);
    builder.mix_u64(summary.builtin_derive_cursor_rollback_ast_mutation_verifier_complete_count);
    builder.mix_u64(summary.builtin_derive_cursor_rollback_ast_mutation_verifier_rollback_executed_count);
    builder.mix_u64(summary.builtin_derive_cursor_rollback_ast_mutation_verifier_ast_mutation_count);
    builder.mix_u64(summary.builtin_derive_cursor_rollback_ast_mutation_verifier_parser_consumable_count);
    builder.mix_u64(summary.aurex_macro_surface_source_item_count);
    builder.mix_u64(summary.aurex_macro_surface_admission_gate_count);
    builder.mix_u64(summary.aurex_macro_declarative_surface_count);
    builder.mix_u64(summary.aurex_macro_user_derive_surface_count);
    builder.mix_u64(summary.aurex_macro_compile_time_surface_count);
    builder.mix_u64(summary.aurex_macro_surface_visible_count);
    builder.mix_u64(summary.aurex_macro_surface_query_reusable_count);
    builder.mix_u64(summary.aurex_macro_surface_body_balanced_count);
    builder.mix_u64(summary.aurex_macro_surface_match_clause_count);
    builder.mix_u64(summary.aurex_macro_surface_expansion_enabled_count);
    builder.mix_u64(summary.aurex_macro_surface_compile_time_execution_enabled_count);
    builder.mix_u64(summary.aurex_macro_surface_parser_consumable_count);
    builder.mix_u64(summary.aurex_macro_definition_site_hygiene_gate_count);
    builder.mix_u64(summary.aurex_macro_definition_site_hygiene_visible_count);
    builder.mix_u64(summary.aurex_macro_definition_site_hygiene_query_reusable_count);
    builder.mix_u64(summary.aurex_macro_definition_site_scope_available_count);
    builder.mix_u64(summary.aurex_macro_fresh_name_scope_reserved_count);
    builder.mix_u64(summary.aurex_macro_diagnostic_anchor_available_count);
    builder.mix_u64(summary.aurex_macro_hygiene_resolution_enabled_count);
    builder.mix_u64(summary.aurex_macro_declared_names_visible_count);
    builder.mix_u64(summary.aurex_macro_typed_matcher_admission_gate_count);
    builder.mix_u64(summary.aurex_macro_typed_matcher_recognized_count);
    builder.mix_u64(summary.aurex_macro_expr_list_matcher_count);
    builder.mix_u64(summary.aurex_macro_item_matcher_count);
    builder.mix_u64(summary.aurex_macro_token_stream_matcher_count);
    builder.mix_u64(summary.aurex_macro_unknown_matcher_count);
    builder.mix_u64(summary.aurex_macro_typed_matcher_visible_count);
    builder.mix_u64(summary.aurex_macro_typed_matcher_query_reusable_count);
    builder.mix_u64(summary.aurex_macro_typed_matcher_execution_enabled_count);
    builder.mix_u64(summary.generated_source_text_count);
    builder.mix_u64(summary.parse_ready_token_buffer_count);
    builder.mix_u64(summary.parsed_generated_part_count);
    builder.mix_u64(summary.merged_generated_part_count);
    builder.mix_u64(summary.ast_mutation_count);
    builder.mix_u64(summary.user_generated_code_count);
    builder.mix_u64(summary.standard_library_required_count);
    builder.mix_u64(summary.runtime_required_count);
    builder.mix_u64(summary.external_process_required_count);
}

[[nodiscard]] bool summary_equals(
    const EarlyItemExpansionSummary& lhs, const EarlyItemExpansionSummary& rhs) noexcept
{
    return lhs.macro_input_count == rhs.macro_input_count
        && lhs.attribute_input_count == rhs.attribute_input_count
        && lhs.builtin_derive_passthrough_count == rhs.builtin_derive_passthrough_count
        && lhs.blocked_attribute_count == rhs.blocked_attribute_count
        && lhs.generated_part_placeholder_count == rhs.generated_part_placeholder_count
        && lhs.generated_part_stub_count == rhs.generated_part_stub_count
        && lhs.materialized_buffer_stub_count == rhs.materialized_buffer_stub_count
        && lhs.parse_blocked_count == rhs.parse_blocked_count
        && lhs.merge_blocked_count == rhs.merge_blocked_count
        && lhs.sema_visible_generated_part_count == rhs.sema_visible_generated_part_count
        && lhs.source_map_placeholder_count == rhs.source_map_placeholder_count
        && lhs.hygiene_stub_count == rhs.hygiene_stub_count
        && lhs.unresolved_hygiene_stub_count == rhs.unresolved_hygiene_stub_count
        && lhs.declared_name_stub_count == rhs.declared_name_stub_count
        && lhs.call_site_capture_count == rhs.call_site_capture_count
        && lhs.trace_stub_count == rhs.trace_stub_count
        && lhs.real_source_map_count == rhs.real_source_map_count
        && lhs.debug_trace_available_count == rhs.debug_trace_available_count
        && lhs.cli_emit_expanded_available_count == rhs.cli_emit_expanded_available_count
        && lhs.generated_item_declaration_stub_count == rhs.generated_item_declaration_stub_count
        && lhs.planned_generated_item_declaration_count == rhs.planned_generated_item_declaration_count
        && lhs.materialized_generated_item_count == rhs.materialized_generated_item_count
        && lhs.declared_generated_name_stub_count == rhs.declared_generated_name_stub_count
        && lhs.lookup_visible_declared_name_count == rhs.lookup_visible_declared_name_count
        && lhs.export_visible_declared_name_count == rhs.export_visible_declared_name_count
        && lhs.token_materialization_admission_stub_count == rhs.token_materialization_admission_stub_count
        && lhs.compiler_owned_admission_count == rhs.compiler_owned_admission_count
        && lhs.admitted_token_materialization_count == rhs.admitted_token_materialization_count
        && lhs.materialized_token_admission_count == rhs.materialized_token_admission_count
        && lhs.generated_token_buffer_stub_count == rhs.generated_token_buffer_stub_count
        && lhs.empty_generated_token_buffer_count == rhs.empty_generated_token_buffer_count
        && lhs.materialized_token_buffer_count == rhs.materialized_token_buffer_count
        && lhs.compiler_owned_token_buffer_count == rhs.compiler_owned_token_buffer_count
        && lhs.generated_token_record_count == rhs.generated_token_record_count
        && lhs.compiler_owned_generated_token_record_count == rhs.compiler_owned_generated_token_record_count
        && lhs.parser_visible_generated_token_count == rhs.parser_visible_generated_token_count
        && lhs.parser_admission_gate_stub_count == rhs.parser_admission_gate_stub_count
        && lhs.compiler_owned_parser_admission_gate_count == rhs.compiler_owned_parser_admission_gate_count
        && lhs.token_record_available_gate_count == rhs.token_record_available_gate_count
        && lhs.parser_blocked_token_buffer_count == rhs.parser_blocked_token_buffer_count
        && lhs.parser_admitted_token_buffer_count == rhs.parser_admitted_token_buffer_count
        && lhs.parser_admission_diagnostic_stub_count == rhs.parser_admission_diagnostic_stub_count
        && lhs.parser_admission_diagnostic_blocked_count == rhs.parser_admission_diagnostic_blocked_count
        && lhs.derive_parser_admission_diagnostic_count == rhs.derive_parser_admission_diagnostic_count
        && lhs.empty_parser_admission_diagnostic_count == rhs.empty_parser_admission_diagnostic_count
        && lhs.emit_expanded_projection_available_count == rhs.emit_expanded_projection_available_count
        && lhs.parser_admission_debug_trace_projection_count == rhs.parser_admission_debug_trace_projection_count
        && lhs.parser_admission_source_map_projection_count == rhs.parser_admission_source_map_projection_count
        && lhs.parser_admission_report_entry_count == rhs.parser_admission_report_entry_count
        && lhs.parser_admission_report_count == rhs.parser_admission_report_count
        && lhs.parser_admission_report_blocked_entry_count == rhs.parser_admission_report_blocked_entry_count
        && lhs.parser_admission_report_derive_entry_count == rhs.parser_admission_report_derive_entry_count
        && lhs.parser_admission_report_empty_entry_count == rhs.parser_admission_report_empty_entry_count
        && lhs.parser_admission_report_token_record_available_entry_count
            == rhs.parser_admission_report_token_record_available_entry_count
        && lhs.parser_admission_report_visible_count == rhs.parser_admission_report_visible_count
        && lhs.parser_admission_report_query_reusable_count == rhs.parser_admission_report_query_reusable_count
        && lhs.parser_admission_report_unordered_anchor_count == rhs.parser_admission_report_unordered_anchor_count
        && lhs.parser_admission_report_parser_consumable_count
            == rhs.parser_admission_report_parser_consumable_count
        && lhs.parser_readiness_preflight_entry_count == rhs.parser_readiness_preflight_entry_count
        && lhs.parser_readiness_preflight_blocked_count == rhs.parser_readiness_preflight_blocked_count
        && lhs.parser_readiness_preflight_derive_entry_count
            == rhs.parser_readiness_preflight_derive_entry_count
        && lhs.parser_readiness_preflight_empty_entry_count
            == rhs.parser_readiness_preflight_empty_entry_count
        && lhs.parser_readiness_preflight_contiguous_index_count
            == rhs.parser_readiness_preflight_contiguous_index_count
        && lhs.parser_readiness_preflight_delimiter_balanced_count
            == rhs.parser_readiness_preflight_delimiter_balanced_count
        && lhs.parser_readiness_preflight_source_anchor_covered_count
            == rhs.parser_readiness_preflight_source_anchor_covered_count
        && lhs.parser_readiness_preflight_parse_config_compatible_count
            == rhs.parser_readiness_preflight_parse_config_compatible_count
        && lhs.parser_readiness_preflight_parser_consumable_count
            == rhs.parser_readiness_preflight_parser_consumable_count
        && lhs.parser_consumption_contract_gate_count == rhs.parser_consumption_contract_gate_count
        && lhs.parser_consumption_contract_blocked_gate_count
            == rhs.parser_consumption_contract_blocked_gate_count
        && lhs.parser_consumption_contract_visible_count == rhs.parser_consumption_contract_visible_count
        && lhs.parser_consumption_contract_query_reusable_count
            == rhs.parser_consumption_contract_query_reusable_count
        && lhs.parser_consumption_contract_parser_consumable_count
            == rhs.parser_consumption_contract_parser_consumable_count
        && lhs.macro_boundary_closure_report_count == rhs.macro_boundary_closure_report_count
        && lhs.macro_boundary_closure_visible_count == rhs.macro_boundary_closure_visible_count
        && lhs.macro_boundary_closure_query_reusable_count
            == rhs.macro_boundary_closure_query_reusable_count
        && lhs.macro_boundary_closure_complete_count == rhs.macro_boundary_closure_complete_count
        && lhs.macro_boundary_closure_parser_consumption_enabled_count
            == rhs.macro_boundary_closure_parser_consumption_enabled_count
        && lhs.builtin_derive_expansion_admission_gate_count
            == rhs.builtin_derive_expansion_admission_gate_count
        && lhs.builtin_derive_expansion_derive_admission_count
            == rhs.builtin_derive_expansion_derive_admission_count
        && lhs.builtin_derive_expansion_non_derive_blocked_count
            == rhs.builtin_derive_expansion_non_derive_blocked_count
        && lhs.builtin_derive_expansion_visible_count
            == rhs.builtin_derive_expansion_visible_count
        && lhs.builtin_derive_expansion_query_reusable_count
            == rhs.builtin_derive_expansion_query_reusable_count
        && lhs.builtin_derive_expansion_capability_candidate_count
            == rhs.builtin_derive_expansion_capability_candidate_count
        && lhs.builtin_derive_semantic_plan_count == rhs.builtin_derive_semantic_plan_count
        && lhs.builtin_derive_semantic_plan_visible_count
            == rhs.builtin_derive_semantic_plan_visible_count
        && lhs.builtin_derive_semantic_plan_query_reusable_count
            == rhs.builtin_derive_semantic_plan_query_reusable_count
        && lhs.builtin_derive_semantic_capability_count
            == rhs.builtin_derive_semantic_capability_count
        && lhs.builtin_derive_semantic_copy_capability_count
            == rhs.builtin_derive_semantic_copy_capability_count
        && lhs.builtin_derive_semantic_eq_capability_count
            == rhs.builtin_derive_semantic_eq_capability_count
        && lhs.builtin_derive_semantic_hash_capability_count
            == rhs.builtin_derive_semantic_hash_capability_count
        && lhs.builtin_derive_parser_release_gate_count
            == rhs.builtin_derive_parser_release_gate_count
        && lhs.builtin_derive_parser_release_visible_count
            == rhs.builtin_derive_parser_release_visible_count
        && lhs.builtin_derive_parser_release_query_reusable_count
            == rhs.builtin_derive_parser_release_query_reusable_count
        && lhs.builtin_derive_parser_release_parser_consumable_count
            == rhs.builtin_derive_parser_release_parser_consumable_count
        && lhs.builtin_derive_release_hardening_matrix_count
            == rhs.builtin_derive_release_hardening_matrix_count
        && lhs.builtin_derive_release_hardening_visible_count
            == rhs.builtin_derive_release_hardening_visible_count
        && lhs.builtin_derive_release_hardening_query_reusable_count
            == rhs.builtin_derive_release_hardening_query_reusable_count
        && lhs.builtin_derive_release_hardening_negative_matrix_complete_count
            == rhs.builtin_derive_release_hardening_negative_matrix_complete_count
        && lhs.builtin_derive_release_hardening_parser_consumable_count
            == rhs.builtin_derive_release_hardening_parser_consumable_count
        && lhs.builtin_derive_debug_dump_contract_count
            == rhs.builtin_derive_debug_dump_contract_count
        && lhs.builtin_derive_debug_dump_contract_visible_count
            == rhs.builtin_derive_debug_dump_contract_visible_count
        && lhs.builtin_derive_debug_dump_query_reusable_count
            == rhs.builtin_derive_debug_dump_query_reusable_count
        && lhs.builtin_derive_debug_dump_complete_count
            == rhs.builtin_derive_debug_dump_complete_count
        && lhs.builtin_derive_debug_dump_parser_consumable_count
            == rhs.builtin_derive_debug_dump_parser_consumable_count
        && lhs.builtin_derive_rollback_diagnostic_gate_count
            == rhs.builtin_derive_rollback_diagnostic_gate_count
        && lhs.builtin_derive_rollback_diagnostic_visible_count
            == rhs.builtin_derive_rollback_diagnostic_visible_count
        && lhs.builtin_derive_rollback_diagnostic_query_reusable_count
            == rhs.builtin_derive_rollback_diagnostic_query_reusable_count
        && lhs.builtin_derive_rollback_diagnostic_design_complete_count
            == rhs.builtin_derive_rollback_diagnostic_design_complete_count
        && lhs.builtin_derive_rollback_diagnostic_parser_consumable_count
            == rhs.builtin_derive_rollback_diagnostic_parser_consumable_count
        && lhs.builtin_derive_parser_consumption_admission_protocol_count
            == rhs.builtin_derive_parser_consumption_admission_protocol_count
        && lhs.builtin_derive_parser_consumption_admission_visible_count
            == rhs.builtin_derive_parser_consumption_admission_visible_count
        && lhs.builtin_derive_parser_consumption_admission_query_reusable_count
            == rhs.builtin_derive_parser_consumption_admission_query_reusable_count
        && lhs.builtin_derive_parser_consumption_admission_complete_count
            == rhs.builtin_derive_parser_consumption_admission_complete_count
        && lhs.builtin_derive_parser_consumption_admission_parser_consumable_count
            == rhs.builtin_derive_parser_consumption_admission_parser_consumable_count
        && lhs.builtin_derive_checkpoint_rollback_protocol_count
            == rhs.builtin_derive_checkpoint_rollback_protocol_count
        && lhs.builtin_derive_checkpoint_rollback_visible_count
            == rhs.builtin_derive_checkpoint_rollback_visible_count
        && lhs.builtin_derive_checkpoint_rollback_query_reusable_count
            == rhs.builtin_derive_checkpoint_rollback_query_reusable_count
        && lhs.builtin_derive_checkpoint_rollback_complete_count
            == rhs.builtin_derive_checkpoint_rollback_complete_count
        && lhs.builtin_derive_checkpoint_rollback_parser_consumable_count
            == rhs.builtin_derive_checkpoint_rollback_parser_consumable_count
        && lhs.builtin_derive_preconsumption_verification_closure_count
            == rhs.builtin_derive_preconsumption_verification_closure_count
        && lhs.builtin_derive_preconsumption_verification_visible_count
            == rhs.builtin_derive_preconsumption_verification_visible_count
        && lhs.builtin_derive_preconsumption_verification_query_reusable_count
            == rhs.builtin_derive_preconsumption_verification_query_reusable_count
        && lhs.builtin_derive_preconsumption_verification_complete_count
            == rhs.builtin_derive_preconsumption_verification_complete_count
        && lhs.builtin_derive_preconsumption_verification_parser_consumable_count
            == rhs.builtin_derive_preconsumption_verification_parser_consumable_count
        && lhs.builtin_derive_controlled_dry_run_adapter_count
            == rhs.builtin_derive_controlled_dry_run_adapter_count
        && lhs.builtin_derive_controlled_dry_run_adapter_visible_count
            == rhs.builtin_derive_controlled_dry_run_adapter_visible_count
        && lhs.builtin_derive_controlled_dry_run_adapter_query_reusable_count
            == rhs.builtin_derive_controlled_dry_run_adapter_query_reusable_count
        && lhs.builtin_derive_controlled_dry_run_adapter_complete_count
            == rhs.builtin_derive_controlled_dry_run_adapter_complete_count
        && lhs.builtin_derive_controlled_dry_run_adapter_executed_count
            == rhs.builtin_derive_controlled_dry_run_adapter_executed_count
        && lhs.builtin_derive_dry_run_rollback_replay_count
            == rhs.builtin_derive_dry_run_rollback_replay_count
        && lhs.builtin_derive_dry_run_rollback_replay_visible_count
            == rhs.builtin_derive_dry_run_rollback_replay_visible_count
        && lhs.builtin_derive_dry_run_rollback_replay_query_reusable_count
            == rhs.builtin_derive_dry_run_rollback_replay_query_reusable_count
        && lhs.builtin_derive_dry_run_rollback_replay_complete_count
            == rhs.builtin_derive_dry_run_rollback_replay_complete_count
        && lhs.builtin_derive_dry_run_rollback_replay_executed_count
            == rhs.builtin_derive_dry_run_rollback_replay_executed_count
        && lhs.builtin_derive_dry_run_negative_matrix_count
            == rhs.builtin_derive_dry_run_negative_matrix_count
        && lhs.builtin_derive_dry_run_negative_matrix_visible_count
            == rhs.builtin_derive_dry_run_negative_matrix_visible_count
        && lhs.builtin_derive_dry_run_negative_matrix_query_reusable_count
            == rhs.builtin_derive_dry_run_negative_matrix_query_reusable_count
        && lhs.builtin_derive_dry_run_negative_matrix_complete_count
            == rhs.builtin_derive_dry_run_negative_matrix_complete_count
        && lhs.builtin_derive_dry_run_negative_matrix_parser_consumable_count
            == rhs.builtin_derive_dry_run_negative_matrix_parser_consumable_count
        && lhs.builtin_derive_parser_dry_run_session_count
            == rhs.builtin_derive_parser_dry_run_session_count
        && lhs.builtin_derive_parser_dry_run_session_visible_count
            == rhs.builtin_derive_parser_dry_run_session_visible_count
        && lhs.builtin_derive_parser_dry_run_session_query_reusable_count
            == rhs.builtin_derive_parser_dry_run_session_query_reusable_count
        && lhs.builtin_derive_parser_dry_run_session_complete_count
            == rhs.builtin_derive_parser_dry_run_session_complete_count
        && lhs.builtin_derive_parser_dry_run_session_executed_count
            == rhs.builtin_derive_parser_dry_run_session_executed_count
        && lhs.builtin_derive_parser_dry_run_session_committed_count
            == rhs.builtin_derive_parser_dry_run_session_committed_count
        && lhs.builtin_derive_token_cursor_snapshot_proof_count
            == rhs.builtin_derive_token_cursor_snapshot_proof_count
        && lhs.builtin_derive_token_cursor_snapshot_proof_visible_count
            == rhs.builtin_derive_token_cursor_snapshot_proof_visible_count
        && lhs.builtin_derive_token_cursor_snapshot_proof_query_reusable_count
            == rhs.builtin_derive_token_cursor_snapshot_proof_query_reusable_count
        && lhs.builtin_derive_token_cursor_snapshot_proof_complete_count
            == rhs.builtin_derive_token_cursor_snapshot_proof_complete_count
        && lhs.builtin_derive_token_cursor_snapshot_proof_cursor_advanced_count
            == rhs.builtin_derive_token_cursor_snapshot_proof_cursor_advanced_count
        && lhs.builtin_derive_token_cursor_snapshot_proof_committed_count
            == rhs.builtin_derive_token_cursor_snapshot_proof_committed_count
        && lhs.builtin_derive_diagnostic_shadow_no_ast_mutation_closure_count
            == rhs.builtin_derive_diagnostic_shadow_no_ast_mutation_closure_count
        && lhs.builtin_derive_diagnostic_shadow_no_ast_mutation_visible_count
            == rhs.builtin_derive_diagnostic_shadow_no_ast_mutation_visible_count
        && lhs.builtin_derive_diagnostic_shadow_no_ast_mutation_query_reusable_count
            == rhs.builtin_derive_diagnostic_shadow_no_ast_mutation_query_reusable_count
        && lhs.builtin_derive_diagnostic_shadow_no_ast_mutation_complete_count
            == rhs.builtin_derive_diagnostic_shadow_no_ast_mutation_complete_count
        && lhs.builtin_derive_diagnostic_shadow_no_ast_mutation_executed_count
            == rhs.builtin_derive_diagnostic_shadow_no_ast_mutation_executed_count
        && lhs.builtin_derive_diagnostic_shadow_no_ast_mutation_ast_mutation_count
            == rhs.builtin_derive_diagnostic_shadow_no_ast_mutation_ast_mutation_count
        && lhs.builtin_derive_diagnostic_shadow_no_ast_mutation_parser_consumable_count
            == rhs.builtin_derive_diagnostic_shadow_no_ast_mutation_parser_consumable_count
        && lhs.builtin_derive_parser_dry_run_admission_gate_count
            == rhs.builtin_derive_parser_dry_run_admission_gate_count
        && lhs.builtin_derive_parser_dry_run_admission_gate_visible_count
            == rhs.builtin_derive_parser_dry_run_admission_gate_visible_count
        && lhs.builtin_derive_parser_dry_run_admission_gate_query_reusable_count
            == rhs.builtin_derive_parser_dry_run_admission_gate_query_reusable_count
        && lhs.builtin_derive_parser_dry_run_admission_gate_complete_count
            == rhs.builtin_derive_parser_dry_run_admission_gate_complete_count
        && lhs.builtin_derive_parser_dry_run_admission_gate_execution_admitted_count
            == rhs.builtin_derive_parser_dry_run_admission_gate_execution_admitted_count
        && lhs.builtin_derive_parser_dry_run_admission_gate_executed_count
            == rhs.builtin_derive_parser_dry_run_admission_gate_executed_count
        && lhs.builtin_derive_parser_dry_run_admission_gate_parser_consumable_count
            == rhs.builtin_derive_parser_dry_run_admission_gate_parser_consumable_count
        && lhs.builtin_derive_error_recovery_shadow_diagnostic_gate_count
            == rhs.builtin_derive_error_recovery_shadow_diagnostic_gate_count
        && lhs.builtin_derive_error_recovery_shadow_diagnostic_gate_visible_count
            == rhs.builtin_derive_error_recovery_shadow_diagnostic_gate_visible_count
        && lhs.builtin_derive_error_recovery_shadow_diagnostic_gate_query_reusable_count
            == rhs.builtin_derive_error_recovery_shadow_diagnostic_gate_query_reusable_count
        && lhs.builtin_derive_error_recovery_shadow_diagnostic_gate_complete_count
            == rhs.builtin_derive_error_recovery_shadow_diagnostic_gate_complete_count
        && lhs.builtin_derive_error_recovery_shadow_diagnostic_gate_recovery_executed_count
            == rhs.builtin_derive_error_recovery_shadow_diagnostic_gate_recovery_executed_count
        && lhs.builtin_derive_error_recovery_shadow_diagnostic_gate_diagnostic_emitted_count
            == rhs.builtin_derive_error_recovery_shadow_diagnostic_gate_diagnostic_emitted_count
        && lhs.builtin_derive_error_recovery_shadow_diagnostic_gate_parser_consumable_count
            == rhs.builtin_derive_error_recovery_shadow_diagnostic_gate_parser_consumable_count
        && lhs.builtin_derive_cursor_rollback_ast_mutation_verifier_closure_count
            == rhs.builtin_derive_cursor_rollback_ast_mutation_verifier_closure_count
        && lhs.builtin_derive_cursor_rollback_ast_mutation_verifier_visible_count
            == rhs.builtin_derive_cursor_rollback_ast_mutation_verifier_visible_count
        && lhs.builtin_derive_cursor_rollback_ast_mutation_verifier_query_reusable_count
            == rhs.builtin_derive_cursor_rollback_ast_mutation_verifier_query_reusable_count
        && lhs.builtin_derive_cursor_rollback_ast_mutation_verifier_complete_count
            == rhs.builtin_derive_cursor_rollback_ast_mutation_verifier_complete_count
        && lhs.builtin_derive_cursor_rollback_ast_mutation_verifier_rollback_executed_count
            == rhs.builtin_derive_cursor_rollback_ast_mutation_verifier_rollback_executed_count
        && lhs.builtin_derive_cursor_rollback_ast_mutation_verifier_ast_mutation_count
            == rhs.builtin_derive_cursor_rollback_ast_mutation_verifier_ast_mutation_count
        && lhs.builtin_derive_cursor_rollback_ast_mutation_verifier_parser_consumable_count
            == rhs.builtin_derive_cursor_rollback_ast_mutation_verifier_parser_consumable_count
        && lhs.aurex_macro_surface_source_item_count == rhs.aurex_macro_surface_source_item_count
        && lhs.aurex_macro_surface_admission_gate_count == rhs.aurex_macro_surface_admission_gate_count
        && lhs.aurex_macro_declarative_surface_count == rhs.aurex_macro_declarative_surface_count
        && lhs.aurex_macro_user_derive_surface_count == rhs.aurex_macro_user_derive_surface_count
        && lhs.aurex_macro_compile_time_surface_count == rhs.aurex_macro_compile_time_surface_count
        && lhs.aurex_macro_surface_visible_count == rhs.aurex_macro_surface_visible_count
        && lhs.aurex_macro_surface_query_reusable_count == rhs.aurex_macro_surface_query_reusable_count
        && lhs.aurex_macro_surface_body_balanced_count == rhs.aurex_macro_surface_body_balanced_count
        && lhs.aurex_macro_surface_match_clause_count == rhs.aurex_macro_surface_match_clause_count
        && lhs.aurex_macro_surface_expansion_enabled_count == rhs.aurex_macro_surface_expansion_enabled_count
        && lhs.aurex_macro_surface_compile_time_execution_enabled_count
            == rhs.aurex_macro_surface_compile_time_execution_enabled_count
        && lhs.aurex_macro_surface_parser_consumable_count == rhs.aurex_macro_surface_parser_consumable_count
        && lhs.aurex_macro_definition_site_hygiene_gate_count
            == rhs.aurex_macro_definition_site_hygiene_gate_count
        && lhs.aurex_macro_definition_site_hygiene_visible_count
            == rhs.aurex_macro_definition_site_hygiene_visible_count
        && lhs.aurex_macro_definition_site_hygiene_query_reusable_count
            == rhs.aurex_macro_definition_site_hygiene_query_reusable_count
        && lhs.aurex_macro_definition_site_scope_available_count
            == rhs.aurex_macro_definition_site_scope_available_count
        && lhs.aurex_macro_fresh_name_scope_reserved_count
            == rhs.aurex_macro_fresh_name_scope_reserved_count
        && lhs.aurex_macro_diagnostic_anchor_available_count
            == rhs.aurex_macro_diagnostic_anchor_available_count
        && lhs.aurex_macro_hygiene_resolution_enabled_count
            == rhs.aurex_macro_hygiene_resolution_enabled_count
        && lhs.aurex_macro_declared_names_visible_count
            == rhs.aurex_macro_declared_names_visible_count
        && lhs.aurex_macro_typed_matcher_admission_gate_count
            == rhs.aurex_macro_typed_matcher_admission_gate_count
        && lhs.aurex_macro_typed_matcher_recognized_count
            == rhs.aurex_macro_typed_matcher_recognized_count
        && lhs.aurex_macro_expr_list_matcher_count == rhs.aurex_macro_expr_list_matcher_count
        && lhs.aurex_macro_item_matcher_count == rhs.aurex_macro_item_matcher_count
        && lhs.aurex_macro_token_stream_matcher_count == rhs.aurex_macro_token_stream_matcher_count
        && lhs.aurex_macro_unknown_matcher_count == rhs.aurex_macro_unknown_matcher_count
        && lhs.aurex_macro_typed_matcher_visible_count == rhs.aurex_macro_typed_matcher_visible_count
        && lhs.aurex_macro_typed_matcher_query_reusable_count
            == rhs.aurex_macro_typed_matcher_query_reusable_count
        && lhs.aurex_macro_typed_matcher_execution_enabled_count
            == rhs.aurex_macro_typed_matcher_execution_enabled_count
        && lhs.generated_source_text_count == rhs.generated_source_text_count
        && lhs.parse_ready_token_buffer_count == rhs.parse_ready_token_buffer_count
        && lhs.parsed_generated_part_count == rhs.parsed_generated_part_count
        && lhs.merged_generated_part_count == rhs.merged_generated_part_count
        && lhs.ast_mutation_count == rhs.ast_mutation_count
        && lhs.user_generated_code_count == rhs.user_generated_code_count
        && lhs.standard_library_required_count == rhs.standard_library_required_count
        && lhs.runtime_required_count == rhs.runtime_required_count
        && lhs.external_process_required_count == rhs.external_process_required_count;
}

[[nodiscard]] bool generated_part_exists_for(
    const std::vector<GeneratedModulePartPlaceholder>& generated_parts,
    const syntax::ModuleId module,
    const base::u32 source_part_index) noexcept
{
    return std::any_of(generated_parts.begin(), generated_parts.end(),
        [module, source_part_index](const GeneratedModulePartPlaceholder& part) {
            return part.module.value == module.value && part.source_part_index == source_part_index;
        });
}

[[nodiscard]] const GeneratedModulePartPlaceholder* find_generated_part_for(
    const std::vector<GeneratedModulePartPlaceholder>& generated_parts,
    const syntax::ModuleId module,
    const base::u32 source_part_index) noexcept
{
    const auto found = std::find_if(generated_parts.begin(), generated_parts.end(),
        [module, source_part_index](const GeneratedModulePartPlaceholder& part) {
            return part.module.value == module.value && part.source_part_index == source_part_index;
        });
    return found == generated_parts.end() ? nullptr : &*found;
}

[[nodiscard]] const GeneratedModulePartParseMergeStub* find_parse_merge_stub_for(
    const std::vector<GeneratedModulePartParseMergeStub>& stubs,
    const syntax::ModuleId module,
    const base::u32 source_part_index) noexcept
{
    const auto found = std::find_if(stubs.begin(), stubs.end(),
        [module, source_part_index](const GeneratedModulePartParseMergeStub& stub) {
            return stub.module.value == module.value && stub.source_part_index == source_part_index;
        });
    return found == stubs.end() ? nullptr : &*found;
}

[[nodiscard]] const ParserAdmissionDiagnosticProjectionStub* find_parser_admission_diagnostic_for_entry(
    const std::vector<ParserAdmissionDiagnosticProjectionStub>& diagnostics,
    const ParserAdmissionDiagnosticReportEntry& entry) noexcept
{
    const auto found = std::find_if(diagnostics.begin(), diagnostics.end(),
        [&entry](const ParserAdmissionDiagnosticProjectionStub& diagnostic) {
            return diagnostic.item.value == entry.item.value
                && diagnostic.module.value == entry.module.value
                && diagnostic.part_index == entry.part_index
                && diagnostic.attribute_index == entry.attribute_index
                && diagnostic.diagnostic_identity == entry.diagnostic_identity;
        });
    return found == diagnostics.end() ? nullptr : &*found;
}

[[nodiscard]] bool stub_matches_placeholder(
    const GeneratedModulePartParseMergeStub& stub,
    const GeneratedModulePartPlaceholder& placeholder) noexcept
{
    const std::string expected_buffer_name =
        module_part_generated_virtual_buffer(placeholder.module, placeholder.generated_stable_index);
    return stub.module.value == placeholder.module.value
        && stub.source_part_index == placeholder.source_part_index
        && stub.generated_stable_index == placeholder.generated_stable_index
        && stub.source_part == placeholder.source_part
        && stub.generated_part == placeholder.generated_part
        && stub.generated_buffer_name == expected_buffer_name
        && stub.blocker_reason == FRONTEND_MACRO_M21E_PARSE_MERGE_BLOCKER
        && stub.generated_buffer_identity == generated_buffer_identity(placeholder, expected_buffer_name)
        && stub.parse_config_fingerprint == parse_config_fingerprint(placeholder)
        && stub.merge_ordering_key == merge_ordering_key(placeholder)
        && stub.expansion_origin == placeholder.output_fingerprint;
}

[[nodiscard]] bool source_map_matches_input(
    const ExpansionSourceMapPlaceholder& source_map, const EarlyItemMacroInput& input) noexcept
{
    return source_map.item.value == input.item.value
        && source_map.module.value == input.module.value
        && source_map.attribute_index == input.attribute_index
        && source_ranges_equal(source_map.attribute_range, input.attribute_range)
        && source_ranges_equal(source_map.token_tree_range, input.token_tree_range)
        && source_map.expansion_origin == input.query_key_fingerprint;
}

[[nodiscard]] bool hygiene_stub_matches_input(
    const ExpansionHygieneStub& stub, const EarlyItemMacroInput& input) noexcept
{
    return stub.item.value == input.item.value
        && stub.module.value == input.module.value
        && stub.part_index == input.part_index
        && stub.attribute_index == input.attribute_index
        && stub.attached_part == input.attached_part
        && stub.expansion_origin == input.query_key_fingerprint
        && stub.call_site_mark == hygiene_mark_fingerprint(FRONTEND_MACRO_M21F_CALL_SITE_MARK_MARKER, input)
        && stub.definition_site_mark == hygiene_mark_fingerprint(
               FRONTEND_MACRO_M21F_DEFINITION_SITE_MARK_MARKER, input)
        && stub.generated_fresh_mark == hygiene_mark_fingerprint(
               FRONTEND_MACRO_M21F_GENERATED_FRESH_MARK_MARKER, input)
        && stub.declared_name_set == hygiene_mark_fingerprint(
               FRONTEND_MACRO_M21F_DECLARED_NAME_SET_MARKER, input)
        && stub.policy == FRONTEND_MACRO_M21F_HYGIENE_POLICY;
}

[[nodiscard]] bool trace_stub_matches_input(
    const ExpansionTraceStub& stub, const EarlyItemMacroInput& input) noexcept
{
    return stub.item.value == input.item.value
        && stub.module.value == input.module.value
        && stub.part_index == input.part_index
        && stub.attribute_index == input.attribute_index
        && stub.attached_part == input.attached_part
        && source_ranges_equal(stub.attribute_range, input.attribute_range)
        && source_ranges_equal(stub.token_tree_range, input.token_tree_range)
        && stub.expansion_origin == input.query_key_fingerprint
        && stub.trace_identity == trace_stub_fingerprint(FRONTEND_MACRO_M21F_TRACE_IDENTITY_MARKER, input)
        && stub.generated_source_map_identity == trace_stub_fingerprint(
               FRONTEND_MACRO_M21F_GENERATED_SOURCE_MAP_MARKER, input)
        && stub.diagnostic_anchor == trace_stub_fingerprint(
               FRONTEND_MACRO_M21F_DIAGNOSTIC_ANCHOR_MARKER, input)
        && stub.trace_policy == FRONTEND_MACRO_M21F_TRACE_POLICY
        && stub.blocker_reason == FRONTEND_MACRO_M21F_TRACE_BLOCKER;
}

[[nodiscard]] bool generated_item_declaration_matches_input(
    const GeneratedItemDeclarationStub& declaration,
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const ExpansionHygieneStub& hygiene) noexcept
{
    const std::string expected_name = generated_item_name_for_input(input);
    return declaration.item.value == input.item.value
        && declaration.module.value == input.module.value
        && declaration.part_index == input.part_index
        && declaration.attribute_index == input.attribute_index
        && declaration.attached_part == input.attached_part
        && declaration.generated_part == placeholder.generated_part
        && declaration.expansion_origin == input.query_key_fingerprint
        && declaration.declared_name_set == hygiene.declared_name_set
        && declaration.declaration_identity == generated_item_stub_fingerprint(
               FRONTEND_MACRO_M21G_DECLARATION_IDENTITY_MARKER,
               input, placeholder, hygiene.declared_name_set, expected_name)
        && declaration.generated_item_key == generated_item_stub_fingerprint(
               FRONTEND_MACRO_M21G_GENERATED_ITEM_KEY_MARKER,
               input, placeholder, hygiene.declared_name_set, expected_name)
        && declaration.declaration_role == FRONTEND_MACRO_M21G_DECLARATION_ROLE
        && declaration.generated_item_name == expected_name
        && declaration.blocker_reason == FRONTEND_MACRO_M21G_DECLARATION_BLOCKER;
}

[[nodiscard]] bool declared_generated_name_matches_input(
    const DeclaredGeneratedNameStub& name,
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const ExpansionHygieneStub& hygiene,
    const GeneratedItemDeclarationStub& declaration) noexcept
{
    const std::string expected_name = generated_item_name_for_input(input);
    return name.item.value == input.item.value
        && name.module.value == input.module.value
        && name.part_index == input.part_index
        && name.attribute_index == input.attribute_index
        && name.attached_part == input.attached_part
        && name.generated_part == placeholder.generated_part
        && name.expansion_origin == input.query_key_fingerprint
        && name.declared_name_set == hygiene.declared_name_set
        && name.declared_name_identity == generated_item_stub_fingerprint(
               FRONTEND_MACRO_M21G_DECLARED_NAME_IDENTITY_MARKER,
               input, placeholder, hygiene.declared_name_set, expected_name)
        && name.hygiene_mark == hygiene.generated_fresh_mark
        && name.declared_name == declaration.generated_item_name
        && name.declared_name == expected_name
        && name.namespace_kind == FRONTEND_MACRO_M21G_DECLARED_NAME_NAMESPACE
        && name.blocker_reason == FRONTEND_MACRO_M21G_DECLARED_NAME_BLOCKER;
}

[[nodiscard]] bool token_materialization_admission_matches_input(
    const TokenMaterializationAdmissionStub& admission,
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const ExpansionHygieneStub& hygiene,
    const ExpansionTraceStub& trace,
    const GeneratedItemDeclarationStub& declaration,
    const DeclaredGeneratedNameStub& declared_name) noexcept
{
    const std::string expected_stream_name = token_stream_name_for_input(input);
    const query::StableFingerprint128 expected_token_plan = token_plan_identity(
        input, placeholder, hygiene, trace, declaration, declared_name, expected_stream_name);
    return admission.item.value == input.item.value
        && admission.module.value == input.module.value
        && admission.part_index == input.part_index
        && admission.attribute_index == input.attribute_index
        && admission.attached_part == input.attached_part
        && admission.generated_part == placeholder.generated_part
        && admission.expansion_origin == input.query_key_fingerprint
        && admission.declaration_identity == declaration.declaration_identity
        && admission.generated_item_key == declaration.generated_item_key
        && admission.declared_name_set == hygiene.declared_name_set
        && admission.declared_name_identity == declared_name.declared_name_identity
        && admission.hygiene_mark == declared_name.hygiene_mark
        && admission.hygiene_mark == hygiene.generated_fresh_mark
        && admission.source_map_identity == trace.generated_source_map_identity
        && admission.trace_identity == trace.trace_identity
        && admission.token_plan_identity == expected_token_plan
        && admission.token_buffer_identity == token_buffer_identity(
               input, placeholder, hygiene, trace, expected_token_plan, token_buffer_kind_for_input(input),
               expected_stream_name)
        && admission.admission_policy == FRONTEND_MACRO_M21H_ADMISSION_POLICY
        && admission.token_stream_name == expected_stream_name
        && admission.blocker_reason == token_admission_blocker_for_input(input);
}

[[nodiscard]] bool generated_token_buffer_matches_input(
    const GeneratedTokenBufferStub& buffer,
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const ExpansionHygieneStub& hygiene,
    const ExpansionTraceStub& trace,
    const TokenMaterializationAdmissionStub& admission) noexcept
{
    const base::u64 expected_token_count = generated_token_count_for_attribute(input);
    const bool materialized_tokens = expected_token_count > 0;
    return buffer.item.value == input.item.value
        && buffer.module.value == input.module.value
        && buffer.part_index == input.part_index
        && buffer.attribute_index == input.attribute_index
        && buffer.attached_part == input.attached_part
        && buffer.generated_part == placeholder.generated_part
        && buffer.token_plan_identity == admission.token_plan_identity
        && buffer.token_buffer_identity == admission.token_buffer_identity
        && buffer.materialization_identity == generated_token_materialization_identity(input, placeholder,
               admission, token_buffer_kind_for_input(input), token_producer_policy_for_input(input),
               expected_token_count)
        && buffer.source_map_identity == trace.generated_source_map_identity
        && buffer.hygiene_mark == hygiene.generated_fresh_mark
        && buffer.token_stream_name == admission.token_stream_name
        && buffer.token_buffer_kind == token_buffer_kind_for_input(input)
        && buffer.token_producer_policy == token_producer_policy_for_input(input)
        && buffer.blocker_reason == token_buffer_blocker_for_input(input)
        && buffer.token_count == expected_token_count
        && buffer.empty != materialized_tokens
        && buffer.materialized_tokens == materialized_tokens;
}

[[nodiscard]] bool generated_token_record_matches_buffer(
    const GeneratedTokenRecord& record,
    const EarlyItemMacroInput& input,
    const GeneratedTokenBufferStub& buffer,
    const base::u32 token_index) noexcept
{
    if (buffer.token_count == 0) {
        return false;
    }
    const bool is_begin_token = token_index == 0;
    const bool is_end_token = static_cast<base::u64>(token_index) + 1U == buffer.token_count;
    if (is_begin_token) {
        if (record.kind != syntax::TokenKind::identifier
            || record.text != FRONTEND_MACRO_M21I_DERIVE_BEGIN_TOKEN_TEXT
            || record.token_role != FRONTEND_MACRO_M21I_DERIVE_BEGIN_TOKEN_ROLE
            || !source_ranges_equal(record.anchor_range, input.attribute_range)) {
            return false;
        }
    } else if (is_end_token) {
        if (record.kind != syntax::TokenKind::identifier
            || record.text != FRONTEND_MACRO_M21I_DERIVE_END_TOKEN_TEXT
            || record.token_role != FRONTEND_MACRO_M21I_DERIVE_END_TOKEN_ROLE
            || !source_ranges_equal(record.anchor_range, input.attribute_range)) {
            return false;
        }
    } else if (record.text != generated_source_token_text(token_index)
        || record.token_role != FRONTEND_MACRO_M21I_DERIVE_SOURCE_TOKEN_ROLE) {
        return false;
    }
    return record.item.value == input.item.value
        && record.module.value == input.module.value
        && record.part_index == input.part_index
        && record.attribute_index == input.attribute_index
        && record.token_index == token_index
        && record.token_buffer_identity == buffer.token_buffer_identity
        && record.token_identity == generated_token_record_identity(input, buffer, token_index, record.kind,
               record.text, record.token_role, record.anchor_range)
        && record.source_map_identity == buffer.source_map_identity
        && record.hygiene_mark == buffer.hygiene_mark
        && record.compiler_owned
        && !record.parser_visible
        && !record.produced_user_generated_code;
}

[[nodiscard]] bool generated_token_records_match_buffers(const EarlyItemExpansionResult& result) noexcept
{
    base::usize record_cursor = 0;
    for (base::usize input_index = 0; input_index < result.inputs.size(); ++input_index) {
        const EarlyItemMacroInput& input = result.inputs[input_index];
        const GeneratedTokenBufferStub& buffer = result.generated_token_buffers[input_index];
        const base::usize remaining_records = result.generated_token_records.size() - record_cursor;
        if (buffer.token_count > static_cast<base::u64>(remaining_records)) {
            return false;
        }
        for (base::u64 token_offset = 0; token_offset < buffer.token_count; ++token_offset) {
            if (record_cursor >= result.generated_token_records.size()) {
                return false;
            }
            if (token_offset > FRONTEND_MACRO_M21I_MAX_GENERATED_TOKEN_INDEX) {
                return false;
            }
            const base::u32 token_index = static_cast<base::u32>(token_offset);
            if (!generated_token_record_matches_buffer(
                    result.generated_token_records[record_cursor], input, buffer, token_index)) {
                return false;
            }
            ++record_cursor;
        }
    }
    return record_cursor == result.generated_token_records.size();
}

[[nodiscard]] bool parser_admission_gate_matches_input(
    const GeneratedTokenParserAdmissionGateStub& gate,
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const GeneratedModulePartParseMergeStub& parse_merge_stub,
    const GeneratedTokenBufferStub& buffer) noexcept
{
    const bool token_records_available = buffer.token_count > 0;
    return gate.item.value == input.item.value
        && gate.module.value == input.module.value
        && gate.part_index == input.part_index
        && gate.attribute_index == input.attribute_index
        && gate.attached_part == input.attached_part
        && gate.generated_part == placeholder.generated_part
        && gate.token_plan_identity == buffer.token_plan_identity
        && gate.token_buffer_identity == buffer.token_buffer_identity
        && gate.materialization_identity == buffer.materialization_identity
        && gate.source_map_identity == buffer.source_map_identity
        && gate.hygiene_mark == buffer.hygiene_mark
        && gate.generated_buffer_identity == parse_merge_stub.generated_buffer_identity
        && gate.parse_config_fingerprint == parse_merge_stub.parse_config_fingerprint
        && gate.parse_gate_identity == generated_token_parser_admission_gate_identity(
               input, placeholder, parse_merge_stub, buffer, token_records_available)
        && gate.token_stream_name == buffer.token_stream_name
        && gate.parser_gate_policy == FRONTEND_MACRO_M21J_PARSER_GATE_POLICY
        && gate.blocker_reason == parser_admission_blocker_for_input(input)
        && gate.token_count == buffer.token_count
        && gate.compiler_owned
        && gate.token_buffer_materialized == buffer.materialized_tokens
        && gate.token_records_available == token_records_available
        && !gate.parser_admitted
        && !gate.parse_ready
        && !gate.parser_consumable
        && !gate.generated_source_text
        && !gate.generated_part_parsed
        && !gate.generated_part_merged
        && !gate.sema_visible
        && !gate.produced_user_generated_code;
}

[[nodiscard]] bool parser_admission_diagnostic_matches_input(
    const ParserAdmissionDiagnosticProjectionStub& diagnostic,
    const EarlyItemMacroInput& input,
    const GeneratedModulePartPlaceholder& placeholder,
    const GeneratedModulePartParseMergeStub& parse_merge_stub,
    const ExpansionTraceStub& trace,
    const GeneratedTokenBufferStub& buffer,
    const GeneratedTokenParserAdmissionGateStub& gate) noexcept
{
    const std::string expected_debug_projection_name = parser_admission_debug_projection_name(input);
    const query::StableFingerprint128 expected_anchor =
        parser_admission_diagnostic_anchor_identity(input, gate, trace);
    return diagnostic.item.value == input.item.value
        && diagnostic.module.value == input.module.value
        && diagnostic.part_index == input.part_index
        && diagnostic.attribute_index == input.attribute_index
        && diagnostic.attached_part == input.attached_part
        && diagnostic.generated_part == placeholder.generated_part
        && source_ranges_equal(diagnostic.primary_anchor, input.attribute_range)
        && source_ranges_equal(diagnostic.token_tree_anchor, input.token_tree_range)
        && diagnostic.parse_gate_identity == gate.parse_gate_identity
        && diagnostic.diagnostic_anchor_identity == expected_anchor
        && diagnostic.diagnostic_identity == parser_admission_diagnostic_identity(input, placeholder,
               parse_merge_stub, buffer, gate, trace, expected_anchor, expected_debug_projection_name)
        && diagnostic.token_plan_identity == gate.token_plan_identity
        && diagnostic.token_buffer_identity == gate.token_buffer_identity
        && diagnostic.materialization_identity == gate.materialization_identity
        && diagnostic.generated_buffer_identity == parse_merge_stub.generated_buffer_identity
        && diagnostic.parse_config_fingerprint == parse_merge_stub.parse_config_fingerprint
        && diagnostic.source_map_identity == buffer.source_map_identity
        && diagnostic.hygiene_mark == buffer.hygiene_mark
        && diagnostic.trace_identity == trace.trace_identity
        && diagnostic.diagnostic_policy == FRONTEND_MACRO_M21K_DIAGNOSTIC_POLICY
        && diagnostic.blocker_category == parser_admission_diagnostic_category_for_input(input)
        && diagnostic.token_buffer_blocker == parser_admission_blocker_for_input(input)
        && diagnostic.generated_part_parse_blocker == FRONTEND_MACRO_M21K_GENERATED_PART_PARSE_BLOCKER
        && diagnostic.user_message == parser_admission_diagnostic_message_for_input(input)
        && diagnostic.debug_projection_name == expected_debug_projection_name
        && diagnostic.token_count == gate.token_count
        && diagnostic.token_buffer_materialized == gate.token_buffer_materialized
        && diagnostic.token_records_available == gate.token_records_available
        && !diagnostic.parser_admitted
        && !diagnostic.parse_ready
        && !diagnostic.parser_consumable
        && !diagnostic.generated_part_parsed
        && !diagnostic.generated_part_merged
        && !diagnostic.emit_expanded_available
        && !diagnostic.debug_trace_available
        && !diagnostic.source_map_available
        && !diagnostic.produced_user_generated_code;
}

[[nodiscard]] bool parser_admission_report_entry_matches_diagnostic(
    const ParserAdmissionDiagnosticReportEntry& entry,
    const ParserAdmissionDiagnosticProjectionStub& diagnostic,
    const base::u32 report_index) noexcept
{
    const std::string expected_query_projection_name =
        parser_admission_report_query_name(diagnostic.module, diagnostic.part_index);
    return entry.item.value == diagnostic.item.value
        && entry.module.value == diagnostic.module.value
        && entry.part_index == diagnostic.part_index
        && entry.attribute_index == diagnostic.attribute_index
        && entry.report_index == report_index
        && entry.attached_part == diagnostic.attached_part
        && entry.generated_part == diagnostic.generated_part
        && source_ranges_equal(entry.primary_anchor, diagnostic.primary_anchor)
        && source_ranges_equal(entry.token_tree_anchor, diagnostic.token_tree_anchor)
        && entry.diagnostic_identity == diagnostic.diagnostic_identity
        && entry.diagnostic_anchor_identity == diagnostic.diagnostic_anchor_identity
        && entry.report_entry_identity == parser_admission_report_entry_identity(
               diagnostic, report_index, expected_query_projection_name)
        && entry.parse_gate_identity == diagnostic.parse_gate_identity
        && entry.blocker_category == diagnostic.blocker_category
        && entry.debug_projection_name == diagnostic.debug_projection_name
        && entry.query_projection_name == expected_query_projection_name
        && entry.token_count == diagnostic.token_count
        && entry.token_records_available == diagnostic.token_records_available
        && !entry.parser_admitted
        && entry.report_visible
        && entry.query_reusable
        && !entry.parser_consumable
        && !entry.emit_expanded_available
        && !entry.produced_user_generated_code;
}

[[nodiscard]] bool parser_admission_report_matches_group(
    const ParserAdmissionDiagnosticReport& report,
    const GeneratedModulePartPlaceholder& placeholder,
    const GeneratedModulePartParseMergeStub& parse_merge_stub,
    const std::vector<ParserAdmissionDiagnosticReportEntry>& entries) noexcept
{
    const std::string expected_query_name =
        parser_admission_report_query_name(placeholder.module, placeholder.source_part_index);
    const query::StableFingerprint128 expected_grouping_identity =
        parser_admission_report_grouping_identity(placeholder, entries);
    const query::StableFingerprint128 expected_anchor_identity =
        parser_admission_report_anchor_identity(placeholder, entries);
    base::u64 entry_count = 0;
    base::u64 blocked_count = 0;
    base::u64 derive_count = 0;
    base::u64 empty_count = 0;
    base::u64 token_record_available_count = 0;
    for (const ParserAdmissionDiagnosticReportEntry& entry : entries) {
        if (entry.module.value != placeholder.module.value
            || entry.part_index != placeholder.source_part_index) {
            continue;
        }
        ++entry_count;
        if (!entry.parser_admitted) {
            ++blocked_count;
        }
        if (entry.blocker_category == FRONTEND_MACRO_M21K_DERIVE_BLOCKER_CATEGORY) {
            ++derive_count;
        }
        if (entry.blocker_category == FRONTEND_MACRO_M21K_EMPTY_BLOCKER_CATEGORY) {
            ++empty_count;
        }
        if (entry.token_records_available) {
            ++token_record_available_count;
        }
    }
    return report.module.value == placeholder.module.value
        && report.source_part_index == placeholder.source_part_index
        && report.attached_part == placeholder.source_part
        && report.generated_part == placeholder.generated_part
        && report.report_identity == parser_admission_report_identity(placeholder, parse_merge_stub,
               expected_grouping_identity, expected_anchor_identity, expected_query_name, entries)
        && report.report_anchor_identity == expected_anchor_identity
        && report.report_grouping_identity == expected_grouping_identity
        && report.parse_config_fingerprint == parse_merge_stub.parse_config_fingerprint
        && report.generated_buffer_identity == parse_merge_stub.generated_buffer_identity
        && report.report_policy == FRONTEND_MACRO_M21L_REPORT_POLICY
        && report.report_query_name == expected_query_name
        && report.blocked_reason == FRONTEND_MACRO_M21L_REPORT_BLOCKER
        && report.entry_count == entry_count
        && report.blocked_entry_count == blocked_count
        && report.derive_entry_count == derive_count
        && report.empty_entry_count == empty_count
        && report.token_record_available_entry_count == token_record_available_count
        && report.query_reusable
        && report.report_visible
        && report.source_anchor_ordered == parser_admission_report_entries_are_ordered(
               entries, placeholder.module, placeholder.source_part_index)
        && !report.parser_admitted
        && !report.parse_ready
        && !report.parser_consumable
        && !report.emit_expanded_available
        && !report.debug_trace_available
        && !report.source_map_available
        && !report.produced_user_generated_code;
}

[[nodiscard]] bool parser_admission_report_entries_match_diagnostics(
    const EarlyItemExpansionResult& result) noexcept
{
    if (result.parser_admission_report_entries.size() != result.parser_admission_diagnostics.size()) {
        return false;
    }
    for (base::usize index = 0; index < result.parser_admission_report_entries.size(); ++index) {
        if (index > std::numeric_limits<base::u32>::max()) {
            return false;
        }
        const ParserAdmissionDiagnosticReportEntry& entry = result.parser_admission_report_entries[index];
        const ParserAdmissionDiagnosticProjectionStub& diagnostic = result.parser_admission_diagnostics[index];
        const ParserAdmissionDiagnosticProjectionStub* const found_diagnostic =
            find_parser_admission_diagnostic_for_entry(result.parser_admission_diagnostics, entry);
        const base::u32 report_index = static_cast<base::u32>(index);
        if (found_diagnostic != &diagnostic
            || !parser_admission_report_entry_matches_diagnostic(entry, diagnostic, report_index)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool parser_admission_reports_match_groups(const EarlyItemExpansionResult& result) noexcept
{
    if (result.parser_admission_reports.size() != result.generated_parts.size()) {
        return false;
    }
    for (base::usize index = 0; index < result.parser_admission_reports.size(); ++index) {
        const GeneratedModulePartPlaceholder& placeholder = result.generated_parts[index];
        const GeneratedModulePartParseMergeStub* const parse_merge_stub =
            find_parse_merge_stub_for(result.generated_part_stubs, placeholder.module,
                placeholder.source_part_index);
        if (parse_merge_stub == nullptr
            || !parser_admission_report_matches_group(result.parser_admission_reports[index],
                placeholder, *parse_merge_stub, result.parser_admission_report_entries)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool parser_readiness_preflight_entry_matches_input(
    const GeneratedTokenParserReadinessPreflightEntry& entry,
    const EarlyItemMacroInput& input,
    const GeneratedTokenBufferStub& buffer,
    const GeneratedTokenParserAdmissionGateStub& gate,
    const ParserAdmissionDiagnosticProjectionStub& diagnostic,
    const ParserAdmissionDiagnosticReportEntry& report_entry,
    const std::vector<GeneratedTokenRecord>& records,
    const base::u32 preflight_index) noexcept
{
    const std::string_view expected_shape =
        parser_readiness_token_stream_shape_for_category(report_entry.blocker_category);
    const bool token_indices_contiguous =
        generated_token_indices_are_contiguous(buffer, records);
    const bool delimiter_balanced =
        generated_token_delimiters_are_balanced(buffer, records);
    const bool source_anchors_covered =
        generated_token_source_anchors_are_covered(input, buffer, records);
    return entry.item.value == input.item.value
        && entry.module.value == input.module.value
        && entry.part_index == input.part_index
        && entry.attribute_index == input.attribute_index
        && entry.preflight_index == preflight_index
        && entry.attached_part == input.attached_part
        && entry.generated_part == buffer.generated_part
        && entry.token_plan_identity == buffer.token_plan_identity
        && entry.token_buffer_identity == buffer.token_buffer_identity
        && entry.materialization_identity == buffer.materialization_identity
        && entry.generated_buffer_identity == gate.generated_buffer_identity
        && entry.parse_config_fingerprint == gate.parse_config_fingerprint
        && entry.parse_gate_identity == gate.parse_gate_identity
        && entry.diagnostic_identity == diagnostic.diagnostic_identity
        && entry.diagnostic_anchor_identity == diagnostic.diagnostic_anchor_identity
        && entry.report_entry_identity == report_entry.report_entry_identity
        && entry.source_map_identity == buffer.source_map_identity
        && entry.hygiene_mark == buffer.hygiene_mark
        && entry.trace_identity == diagnostic.trace_identity
        && entry.preflight_identity == parser_readiness_preflight_identity(input, buffer, gate,
               diagnostic, report_entry, preflight_index, expected_shape, token_indices_contiguous,
               delimiter_balanced, source_anchors_covered)
        && entry.token_stream_name == buffer.token_stream_name
        && entry.token_stream_shape == expected_shape
        && entry.delimiter_balance_state == FRONTEND_MACRO_M21M_DELIMITER_BALANCED_STATE
        && entry.source_anchor_coverage_state == FRONTEND_MACRO_M21M_SOURCE_ANCHOR_COVERED_STATE
        && entry.readiness_policy == FRONTEND_MACRO_M21M_PREFLIGHT_POLICY
        && entry.blocker_reason == FRONTEND_MACRO_M21M_PREFLIGHT_BLOCKER
        && entry.token_count == buffer.token_count
        && entry.token_records_available == gate.token_records_available
        && entry.token_indices_contiguous == token_indices_contiguous
        && entry.delimiter_balanced == delimiter_balanced
        && entry.source_anchors_covered == source_anchors_covered
        && entry.parse_config_compatible
        && entry.hygiene_prerequisite_available
        && entry.source_map_prerequisite_available
        && entry.diagnostic_projection_available
        && !entry.parser_admitted
        && !entry.parse_ready
        && !entry.parser_consumable
        && !entry.generated_part_parsed
        && !entry.generated_part_merged
        && !entry.produced_user_generated_code;
}

[[nodiscard]] bool parser_readiness_preflight_entries_match_inputs(
    const EarlyItemExpansionResult& result) noexcept
{
    if (result.parser_readiness_preflight_entries.size() != result.inputs.size()) {
        return false;
    }
    for (base::usize index = 0; index < result.parser_readiness_preflight_entries.size(); ++index) {
        if (index > std::numeric_limits<base::u32>::max()) {
            return false;
        }
        if (!parser_readiness_preflight_entry_matches_input(
                result.parser_readiness_preflight_entries[index],
                result.inputs[index],
                result.generated_token_buffers[index],
                result.parser_admission_gates[index],
                result.parser_admission_diagnostics[index],
                result.parser_admission_report_entries[index],
                result.generated_token_records,
                static_cast<base::u32>(index))) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool parser_consumption_contract_gate_matches_group(
    const GeneratedTokenParserConsumptionContractGate& gate,
    const GeneratedModulePartPlaceholder& placeholder,
    const GeneratedModulePartParseMergeStub& parse_merge_stub,
    const ParserAdmissionDiagnosticReport& report,
    const std::vector<GeneratedTokenParserReadinessPreflightEntry>& entries) noexcept
{
    const std::string expected_query_name =
        parser_consumption_contract_query_name(placeholder.module, placeholder.source_part_index);
    const query::StableFingerprint128 expected_grouping_identity =
        parser_consumption_contract_grouping_identity(placeholder, entries);
    const query::StableFingerprint128 expected_anchor_identity =
        parser_consumption_contract_anchor_identity(placeholder, entries);
    base::u64 entry_count = 0;
    base::u64 blocked_count = 0;
    base::u64 derive_count = 0;
    base::u64 empty_count = 0;
    base::u64 contiguous_count = 0;
    base::u64 delimiter_count = 0;
    base::u64 source_anchor_count = 0;
    base::u64 parse_config_count = 0;
    base::u64 diagnostic_count = 0;
    for (const GeneratedTokenParserReadinessPreflightEntry& entry : entries) {
        if (entry.module.value != placeholder.module.value
            || entry.part_index != placeholder.source_part_index) {
            continue;
        }
        ++entry_count;
        if (!entry.parser_admitted) {
            ++blocked_count;
        }
        if (entry.token_stream_shape == FRONTEND_MACRO_M21M_DERIVE_TOKEN_STREAM_SHAPE) {
            ++derive_count;
        }
        if (entry.token_stream_shape == FRONTEND_MACRO_M21M_EMPTY_TOKEN_STREAM_SHAPE) {
            ++empty_count;
        }
        if (entry.token_indices_contiguous) {
            ++contiguous_count;
        }
        if (entry.delimiter_balanced) {
            ++delimiter_count;
        }
        if (entry.source_anchors_covered) {
            ++source_anchor_count;
        }
        if (entry.parse_config_compatible) {
            ++parse_config_count;
        }
        if (entry.diagnostic_projection_available) {
            ++diagnostic_count;
        }
    }
    const bool structurally_checked = entry_count == contiguous_count
        && entry_count == delimiter_count
        && entry_count == source_anchor_count
        && entry_count == parse_config_count
        && entry_count == diagnostic_count;
    return gate.module.value == placeholder.module.value
        && gate.source_part_index == placeholder.source_part_index
        && gate.attached_part == placeholder.source_part
        && gate.generated_part == placeholder.generated_part
        && gate.generated_buffer_identity == parse_merge_stub.generated_buffer_identity
        && gate.parse_config_fingerprint == parse_merge_stub.parse_config_fingerprint
        && gate.report_identity == report.report_identity
        && gate.contract_identity == parser_consumption_contract_identity(placeholder, parse_merge_stub,
               report, expected_grouping_identity, expected_anchor_identity, expected_query_name, entries)
        && gate.contract_grouping_identity == expected_grouping_identity
        && gate.contract_anchor_identity == expected_anchor_identity
        && gate.contract_policy == FRONTEND_MACRO_M21N_CONTRACT_POLICY
        && gate.contract_query_name == expected_query_name
        && gate.blocked_reason == FRONTEND_MACRO_M21N_CONTRACT_BLOCKER
        && gate.preflight_entry_count == entry_count
        && gate.blocked_entry_count == blocked_count
        && gate.derive_entry_count == derive_count
        && gate.empty_entry_count == empty_count
        && gate.contiguous_index_entry_count == contiguous_count
        && gate.delimiter_balanced_entry_count == delimiter_count
        && gate.source_anchor_covered_entry_count == source_anchor_count
        && gate.parse_config_compatible_entry_count == parse_config_count
        && gate.diagnostic_projection_entry_count == diagnostic_count
        && gate.query_reusable
        && gate.contract_visible
        && gate.all_entries_structurally_checked == structurally_checked
        && !gate.parser_admitted
        && !gate.parse_ready
        && !gate.parser_consumable
        && !gate.generated_part_parsed
        && !gate.generated_part_merged
        && !gate.sema_visible
        && !gate.emit_expanded_available
        && !gate.debug_trace_available
        && !gate.source_map_available
        && !gate.produced_user_generated_code;
}

[[nodiscard]] bool parser_consumption_contract_gates_match_groups(
    const EarlyItemExpansionResult& result) noexcept
{
    if (result.parser_consumption_contract_gates.size() != result.generated_parts.size()
        || result.parser_admission_reports.size() != result.generated_parts.size()) {
        return false;
    }
    for (base::usize index = 0; index < result.parser_consumption_contract_gates.size(); ++index) {
        const GeneratedModulePartPlaceholder& placeholder = result.generated_parts[index];
        const GeneratedModulePartParseMergeStub* const parse_merge_stub =
            find_parse_merge_stub_for(result.generated_part_stubs, placeholder.module,
                placeholder.source_part_index);
        if (parse_merge_stub == nullptr
            || !parser_consumption_contract_gate_matches_group(
                result.parser_consumption_contract_gates[index], placeholder, *parse_merge_stub,
                result.parser_admission_reports[index], result.parser_readiness_preflight_entries)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool macro_boundary_closure_report_matches_result(
    const MacroExpansionBoundaryClosureReport& report,
    const EarlyItemExpansionResult& result) noexcept
{
    base::u64 blocked_contract_gate_count = 0;
    base::u64 parser_consumable_contract_gate_count = 0;
    for (const GeneratedTokenParserConsumptionContractGate& gate :
        result.parser_consumption_contract_gates) {
        if (!gate.parser_admitted) {
            ++blocked_contract_gate_count;
        }
        if (gate.parser_consumable) {
            ++parser_consumable_contract_gate_count;
        }
    }
    const query::StableFingerprint128 expected_grouping_identity =
        macro_boundary_closure_grouping_identity(result);
    return report.closure_identity == macro_boundary_closure_identity(result, expected_grouping_identity,
               blocked_contract_gate_count, parser_consumable_contract_gate_count)
        && report.closure_grouping_identity == expected_grouping_identity
        && report.closure_policy == FRONTEND_MACRO_M21O_CLOSURE_POLICY
        && report.closure_query_name == FRONTEND_MACRO_M21O_CLOSURE_QUERY_NAME
        && report.blocked_reason == FRONTEND_MACRO_M21O_CLOSURE_BLOCKER
        && report.macro_input_count == result.inputs.size()
        && report.generated_part_count == result.generated_parts.size()
        && report.parser_admission_report_count == result.parser_admission_reports.size()
        && report.parser_readiness_preflight_entry_count
            == result.parser_readiness_preflight_entries.size()
        && report.parser_consumption_contract_gate_count
            == result.parser_consumption_contract_gates.size()
        && report.blocked_contract_gate_count == blocked_contract_gate_count
        && report.parser_consumable_contract_gate_count == parser_consumable_contract_gate_count
        && report.m21m_preflight_available
        && report.m21n_contract_available
        && report.release_closure_complete
        && report.query_reusable
        && report.closure_visible
        && !report.parser_consumption_enabled
        && !report.emit_expanded_available
        && !report.debug_trace_available
        && !report.source_map_available
        && !report.standard_library_required
        && !report.runtime_required
        && !report.external_process_required
        && !report.produced_user_generated_code;
}

[[nodiscard]] bool macro_boundary_closure_reports_match_result(
    const EarlyItemExpansionResult& result) noexcept
{
    return result.macro_boundary_closure_reports.size() == 1U
        && macro_boundary_closure_report_matches_result(
            result.macro_boundary_closure_reports.front(), result);
}

[[nodiscard]] bool builtin_derive_expansion_admission_matches_input(
    const BuiltinDeriveExpansionAdmissionGate& admission,
    const EarlyItemMacroInput& input,
    const GeneratedTokenBufferStub& buffer,
    const GeneratedTokenParserAdmissionGateStub& parser_gate,
    const GeneratedTokenParserReadinessPreflightEntry& preflight,
    const ParserAdmissionDiagnosticProjectionStub& diagnostic,
    const MacroExpansionBoundaryClosureReport& closure,
    const base::u32 admission_index) noexcept
{
    const std::string expected_query_name = builtin_derive_admission_query_name(input);
    return admission.item.value == input.item.value
        && admission.module.value == input.module.value
        && admission.part_index == input.part_index
        && admission.attribute_index == input.attribute_index
        && admission.admission_index == admission_index
        && admission.attached_part == input.attached_part
        && admission.generated_part == buffer.generated_part
        && admission.token_buffer_identity == buffer.token_buffer_identity
        && admission.preflight_identity == preflight.preflight_identity
        && admission.parse_gate_identity == parser_gate.parse_gate_identity
        && admission.diagnostic_identity == diagnostic.diagnostic_identity
        && admission.closure_identity == closure.closure_identity
        && admission.admission_identity == builtin_derive_admission_identity(input, buffer, parser_gate,
               preflight, diagnostic, closure, expected_query_name, admission_index,
               admission.capability_candidate_count, admission.unsupported_candidate_count,
               admission.duplicate_candidate_count)
        && admission.admission_policy == FRONTEND_MACRO_M22A_ADMISSION_POLICY
        && admission.admission_kind == builtin_derive_admission_kind_for_input(input)
        && admission.query_name == expected_query_name
        && admission.blocker_reason == builtin_derive_admission_blocker_for_input(input)
        && admission.token_count == buffer.token_count
        && admission.builtin_derive_input == builtin_derive_input(input)
        && admission.compiler_owned
        && admission.token_records_available == parser_gate.token_records_available
        && admission.preflight_available
        && admission.admission_visible
        && admission.query_reusable
        && !admission.parser_consumption_enabled
        && !admission.external_process_required
        && !admission.standard_library_required
        && !admission.runtime_required
        && !admission.generated_source_text
        && !admission.produced_user_generated_code;
}

[[nodiscard]] bool builtin_derive_expansion_admissions_match_inputs(
    const EarlyItemExpansionResult& result) noexcept
{
    if (result.builtin_derive_expansion_admissions.size() != result.inputs.size()
        || result.macro_boundary_closure_reports.size() != 1U) {
        return false;
    }
    const MacroExpansionBoundaryClosureReport& closure = result.macro_boundary_closure_reports.front();
    for (base::usize index = 0; index < result.builtin_derive_expansion_admissions.size(); ++index) {
        if (index > std::numeric_limits<base::u32>::max()) {
            return false;
        }
        if (!builtin_derive_expansion_admission_matches_input(
                result.builtin_derive_expansion_admissions[index],
                result.inputs[index],
                result.generated_token_buffers[index],
                result.parser_admission_gates[index],
                result.parser_readiness_preflight_entries[index],
                result.parser_admission_diagnostics[index],
                closure,
                static_cast<base::u32>(index))) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool builtin_derive_semantic_plan_matches_admission(
    const BuiltinDeriveSemanticExpansionPlan& plan,
    const EarlyItemMacroInput& input,
    const BuiltinDeriveExpansionAdmissionGate& admission,
    const base::u32 semantic_plan_index) noexcept
{
    const bool target_struct_or_enum = plan.target_kind == FRONTEND_MACRO_M22_TARGET_KIND_STRUCT
        || plan.target_kind == FRONTEND_MACRO_M22_TARGET_KIND_ENUM;
    const query::StableFingerprint128 expected_capability_set_identity =
        builtin_derive_capability_set_identity(input, admission, plan.target_kind,
            plan.copy_capability_count, plan.eq_capability_count, plan.hash_capability_count);
    const query::StableFingerprint128 expected_semantic_plan_identity =
        builtin_derive_semantic_plan_identity(input, admission, expected_capability_set_identity,
            plan.target_kind, semantic_plan_index, plan.capability_count, plan.copy_capability_count,
            plan.eq_capability_count, plan.hash_capability_count, target_struct_or_enum);
    return plan.item.value == input.item.value
        && plan.module.value == input.module.value
        && plan.part_index == input.part_index
        && plan.attribute_index == input.attribute_index
        && plan.semantic_plan_index == semantic_plan_index
        && plan.attached_part == input.attached_part
        && plan.generated_part == admission.generated_part
        && plan.token_buffer_identity == admission.token_buffer_identity
        && plan.preflight_identity == admission.preflight_identity
        && plan.admission_identity == admission.admission_identity
        && plan.semantic_plan_identity == expected_semantic_plan_identity
        && plan.capability_set_identity == expected_capability_set_identity
        && plan.semantic_plan_identity != plan.capability_set_identity
        && plan.semantic_policy == FRONTEND_MACRO_M22B_SEMANTIC_POLICY
        && (plan.target_kind == FRONTEND_MACRO_M22_TARGET_KIND_STRUCT
            || plan.target_kind == FRONTEND_MACRO_M22_TARGET_KIND_ENUM
            || plan.target_kind == FRONTEND_MACRO_M22_TARGET_KIND_OTHER)
        && plan.semantic_model == FRONTEND_MACRO_M22B_SEMANTIC_MODEL
        && plan.blocker_reason == FRONTEND_MACRO_M22B_BLOCKER
        && plan.capability_count == plan.copy_capability_count + plan.eq_capability_count
            + plan.hash_capability_count
        && plan.capability_count <= admission.capability_candidate_count
        && plan.builtin_derive_input == admission.builtin_derive_input
        && plan.builtin_derive_input == builtin_derive_input(input)
        && plan.target_struct_or_enum == target_struct_or_enum
        && plan.uses_existing_builtin_derive_capability_path == plan.builtin_derive_input
        && !plan.requires_ast_mutation
        && !plan.requires_generated_items
        && !plan.requires_standard_library
        && !plan.requires_runtime
        && !plan.external_process_required
        && !plan.parser_consumption_enabled
        && !plan.produced_user_generated_code
        && plan.plan_visible
        && plan.query_reusable;
}

[[nodiscard]] bool builtin_derive_semantic_plans_match_inputs(
    const EarlyItemExpansionResult& result) noexcept
{
    if (result.builtin_derive_semantic_plans.size() != result.inputs.size()
        || result.builtin_derive_expansion_admissions.size() != result.inputs.size()) {
        return false;
    }
    for (base::usize index = 0; index < result.builtin_derive_semantic_plans.size(); ++index) {
        if (index > std::numeric_limits<base::u32>::max()) {
            return false;
        }
        if (!builtin_derive_semantic_plan_matches_admission(
                result.builtin_derive_semantic_plans[index],
                result.inputs[index],
                result.builtin_derive_expansion_admissions[index],
                static_cast<base::u32>(index))) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool builtin_derive_parser_release_gate_matches_group(
    const BuiltinDeriveParserConsumptionReleaseGate& gate,
    const GeneratedModulePartPlaceholder& placeholder,
    const GeneratedTokenParserConsumptionContractGate& contract,
    const MacroExpansionBoundaryClosureReport& closure,
    const std::vector<BuiltinDeriveExpansionAdmissionGate>& admissions,
    const std::vector<BuiltinDeriveSemanticExpansionPlan>& plans) noexcept
{
    base::u64 admission_count = 0;
    base::u64 derive_admission_count = 0;
    base::u64 semantic_plan_count = 0;
    base::u64 capability_total_count = 0;
    for (const BuiltinDeriveExpansionAdmissionGate& admission : admissions) {
        if (admission.module.value != placeholder.module.value
            || admission.part_index != placeholder.source_part_index) {
            continue;
        }
        ++admission_count;
        if (admission.builtin_derive_input) {
            ++derive_admission_count;
        }
    }
    for (const BuiltinDeriveSemanticExpansionPlan& plan : plans) {
        if (plan.module.value != placeholder.module.value
            || plan.part_index != placeholder.source_part_index) {
            continue;
        }
        ++semantic_plan_count;
        capability_total_count += plan.capability_count;
    }
    const base::u64 parser_consumable_contract_count = contract.parser_consumable ? 1U : 0U;
    const query::StableFingerprint128 expected_admission_group_identity =
        builtin_derive_admission_group_identity(placeholder, admissions);
    const query::StableFingerprint128 expected_semantic_plan_group_identity =
        builtin_derive_semantic_plan_group_identity(placeholder, plans);
    const std::string expected_query_name =
        builtin_derive_parser_release_query_name(placeholder.module, placeholder.source_part_index);
    return gate.module.value == placeholder.module.value
        && gate.source_part_index == placeholder.source_part_index
        && gate.attached_part == placeholder.source_part
        && gate.generated_part == placeholder.generated_part
        && gate.contract_identity == contract.contract_identity
        && gate.closure_identity == closure.closure_identity
        && gate.admission_group_identity == expected_admission_group_identity
        && gate.semantic_plan_group_identity == expected_semantic_plan_group_identity
        && gate.release_gate_identity == builtin_derive_parser_release_gate_identity(placeholder, contract,
               closure, expected_admission_group_identity, expected_semantic_plan_group_identity,
               expected_query_name, admission_count, derive_admission_count, semantic_plan_count,
               capability_total_count, parser_consumable_contract_count)
        && gate.release_policy == FRONTEND_MACRO_M22C_RELEASE_POLICY
        && gate.release_query_name == expected_query_name
        && gate.blocked_reason == FRONTEND_MACRO_M22C_RELEASE_BLOCKER
        && gate.admission_count == admission_count
        && gate.derive_admission_count == derive_admission_count
        && gate.semantic_plan_count == semantic_plan_count
        && gate.capability_total_count == capability_total_count
        && gate.parser_consumable_contract_count == parser_consumable_contract_count
        && gate.rollback_diagnostics_available
        && gate.debug_trace_prerequisite_available
        && gate.source_map_prerequisite_available
        && gate.hygiene_prerequisite_available
        && !gate.parser_consumption_enabled
        && !gate.generated_part_parsed
        && !gate.generated_part_merged
        && !gate.emit_expanded_available
        && !gate.debug_trace_available
        && !gate.source_map_available
        && !gate.standard_library_required
        && !gate.runtime_required
        && !gate.external_process_required
        && !gate.produced_user_generated_code
        && gate.release_visible
        && gate.query_reusable;
}

[[nodiscard]] bool builtin_derive_parser_release_gates_match_groups(
    const EarlyItemExpansionResult& result) noexcept
{
    if (result.builtin_derive_parser_release_gates.size() != result.generated_parts.size()
        || result.parser_consumption_contract_gates.size() != result.generated_parts.size()
        || result.macro_boundary_closure_reports.size() != 1U) {
        return false;
    }
    const MacroExpansionBoundaryClosureReport& closure = result.macro_boundary_closure_reports.front();
    for (base::usize index = 0; index < result.builtin_derive_parser_release_gates.size(); ++index) {
        const GeneratedModulePartPlaceholder& placeholder = result.generated_parts[index];
        if (!builtin_derive_parser_release_gate_matches_group(
                result.builtin_derive_parser_release_gates[index],
                placeholder,
                result.parser_consumption_contract_gates[index],
                closure,
                result.builtin_derive_expansion_admissions,
                result.builtin_derive_semantic_plans)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool builtin_derive_release_hardening_matrix_matches_group(
    const BuiltinDeriveReleaseHardeningMatrix& matrix,
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveParserConsumptionReleaseGate& release_gate,
    const std::vector<GeneratedModulePartPlaceholder>& generated_parts,
    const std::vector<BuiltinDeriveExpansionAdmissionGate>& admissions,
    const std::vector<BuiltinDeriveSemanticExpansionPlan>& plans,
    const std::vector<BuiltinDeriveParserConsumptionReleaseGate>& release_gates) noexcept
{
    const base::u64 part_local_admission_count = count_part_local_admissions(placeholder, admissions);
    const base::u64 part_local_derive_admission_count =
        count_part_local_derive_admissions(placeholder, admissions);
    const base::u64 part_local_semantic_plan_count = count_part_local_semantic_plans(placeholder, plans);
    const base::u64 part_local_release_gate_count =
        count_part_local_parser_release_gates(placeholder, release_gates);
    const base::u64 global_admission_count = static_cast<base::u64>(admissions.size());
    const base::u64 global_semantic_plan_count = static_cast<base::u64>(plans.size());
    const base::u64 global_generated_part_count = static_cast<base::u64>(generated_parts.size());
    const base::u64 cross_part_admission_count = global_admission_count - part_local_admission_count;
    const base::u64 cross_part_semantic_plan_count = global_semantic_plan_count - part_local_semantic_plan_count;
    const std::string expected_query_name =
        builtin_derive_release_hardening_query_name(placeholder.module, placeholder.source_part_index);
    return matrix.module.value == placeholder.module.value
        && matrix.source_part_index == placeholder.source_part_index
        && matrix.attached_part == placeholder.source_part
        && matrix.generated_part == placeholder.generated_part
        && matrix.release_gate_identity == release_gate.release_gate_identity
        && matrix.admission_group_identity == release_gate.admission_group_identity
        && matrix.semantic_plan_group_identity == release_gate.semantic_plan_group_identity
        && matrix.hardening_matrix_identity == builtin_derive_release_hardening_matrix_identity(
               placeholder, release_gate, expected_query_name, part_local_admission_count,
               part_local_derive_admission_count, part_local_semantic_plan_count,
               part_local_release_gate_count, global_admission_count, global_semantic_plan_count,
               global_generated_part_count, cross_part_admission_count, cross_part_semantic_plan_count)
        && matrix.hardening_policy == FRONTEND_MACRO_M22D_HARDENING_POLICY
        && matrix.hardening_query_name == expected_query_name
        && matrix.blocked_reason == FRONTEND_MACRO_M22D_HARDENING_BLOCKER
        && matrix.part_local_admission_count == part_local_admission_count
        && matrix.part_local_derive_admission_count == part_local_derive_admission_count
        && matrix.part_local_semantic_plan_count == part_local_semantic_plan_count
        && matrix.part_local_release_gate_count == part_local_release_gate_count
        && matrix.global_admission_count == global_admission_count
        && matrix.global_semantic_plan_count == global_semantic_plan_count
        && matrix.global_generated_part_count == global_generated_part_count
        && matrix.cross_part_admission_count == cross_part_admission_count
        && matrix.cross_part_semantic_plan_count == cross_part_semantic_plan_count
        && matrix.part_locality_preserved
        && matrix.multi_item_matrix_available
        && matrix.negative_matrix_complete
        && matrix.release_remains_blocked
        && !matrix.parser_consumption_enabled
        && !matrix.generated_part_parsed
        && !matrix.generated_part_merged
        && !matrix.emit_expanded_available
        && !matrix.debug_trace_available
        && !matrix.source_map_available
        && !matrix.standard_library_required
        && !matrix.runtime_required
        && !matrix.external_process_required
        && !matrix.produced_user_generated_code
        && matrix.matrix_visible
        && matrix.query_reusable;
}

[[nodiscard]] bool builtin_derive_release_hardening_matrices_match_groups(
    const EarlyItemExpansionResult& result) noexcept
{
    if (result.builtin_derive_release_hardening_matrices.size() != result.generated_parts.size()
        || result.builtin_derive_parser_release_gates.size() != result.generated_parts.size()) {
        return false;
    }
    for (base::usize index = 0; index < result.builtin_derive_release_hardening_matrices.size(); ++index) {
        const GeneratedModulePartPlaceholder& placeholder = result.generated_parts[index];
        if (!builtin_derive_release_hardening_matrix_matches_group(
                result.builtin_derive_release_hardening_matrices[index],
                placeholder,
                result.builtin_derive_parser_release_gates[index],
                result.generated_parts,
                result.builtin_derive_expansion_admissions,
                result.builtin_derive_semantic_plans,
                result.builtin_derive_parser_release_gates)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool builtin_derive_debug_dump_contract_matches_group(
    const BuiltinDeriveDebugDumpStabilityContract& contract,
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveParserConsumptionReleaseGate& release_gate,
    const BuiltinDeriveReleaseHardeningMatrix& matrix) noexcept
{
    const std::string expected_query_name =
        builtin_derive_debug_dump_query_name(placeholder.module, placeholder.source_part_index);
    return contract.module.value == placeholder.module.value
        && contract.source_part_index == placeholder.source_part_index
        && contract.attached_part == placeholder.source_part
        && contract.generated_part == placeholder.generated_part
        && contract.release_gate_identity == release_gate.release_gate_identity
        && contract.hardening_matrix_identity == matrix.hardening_matrix_identity
        && contract.debug_dump_contract_identity == builtin_derive_debug_dump_contract_identity(
               placeholder, release_gate, matrix, expected_query_name)
        && contract.debug_dump_policy == FRONTEND_MACRO_M22E_DEBUG_DUMP_POLICY
        && contract.debug_dump_query_name == expected_query_name
        && contract.blocked_reason == FRONTEND_MACRO_M22E_DEBUG_DUMP_BLOCKER
        && contract.dump_section_count == FRONTEND_MACRO_M22E_DEBUG_DUMP_SECTION_COUNT
        && contract.stable_ordering_available
        && contract.identity_projection_available
        && contract.summary_projection_available
        && contract.drift_debuggable
        && contract.debug_dump_contract_complete
        && !contract.emit_expanded_available
        && !contract.debug_trace_available
        && !contract.source_map_available
        && !contract.parser_consumption_enabled
        && !contract.standard_library_required
        && !contract.runtime_required
        && !contract.external_process_required
        && !contract.produced_user_generated_code
        && contract.contract_visible
        && contract.query_reusable;
}

[[nodiscard]] bool builtin_derive_debug_dump_contracts_match_groups(
    const EarlyItemExpansionResult& result) noexcept
{
    if (result.builtin_derive_debug_dump_contracts.size() != result.generated_parts.size()
        || result.builtin_derive_parser_release_gates.size() != result.generated_parts.size()
        || result.builtin_derive_release_hardening_matrices.size() != result.generated_parts.size()) {
        return false;
    }
    for (base::usize index = 0; index < result.builtin_derive_debug_dump_contracts.size(); ++index) {
        const GeneratedModulePartPlaceholder& placeholder = result.generated_parts[index];
        if (!builtin_derive_debug_dump_contract_matches_group(
                result.builtin_derive_debug_dump_contracts[index],
                placeholder,
                result.builtin_derive_parser_release_gates[index],
                result.builtin_derive_release_hardening_matrices[index])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool builtin_derive_rollback_diagnostic_gate_matches_group(
    const BuiltinDeriveRollbackDiagnosticDesignGate& gate,
    const GeneratedModulePartPlaceholder& placeholder,
    const GeneratedTokenParserConsumptionContractGate& parser_contract,
    const BuiltinDeriveParserConsumptionReleaseGate& release_gate,
    const BuiltinDeriveReleaseHardeningMatrix& matrix,
    const BuiltinDeriveDebugDumpStabilityContract& debug_contract,
    const std::vector<ParserAdmissionDiagnosticProjectionStub>& diagnostics,
    const std::vector<ParserAdmissionDiagnosticReportEntry>& report_entries) noexcept
{
    const base::u64 diagnostic_projection_count = count_part_local_diagnostics(placeholder, diagnostics);
    const base::u64 diagnostic_report_entry_count = count_part_local_report_entries(placeholder, report_entries);
    const base::u64 blocked_diagnostic_count = count_part_local_blocked_diagnostics(placeholder, diagnostics);
    const base::u64 derive_diagnostic_count = count_part_local_derive_diagnostics(placeholder, diagnostics);
    const base::u64 empty_diagnostic_count = count_part_local_empty_diagnostics(placeholder, diagnostics);
    const base::u64 parser_consumption_contract_count = parser_contract.contract_visible ? 1U : 0U;
    const std::string expected_query_name =
        builtin_derive_rollback_diagnostic_query_name(placeholder.module, placeholder.source_part_index);
    return gate.module.value == placeholder.module.value
        && gate.source_part_index == placeholder.source_part_index
        && gate.attached_part == placeholder.source_part
        && gate.generated_part == placeholder.generated_part
        && gate.parser_consumption_contract_identity == parser_contract.contract_identity
        && gate.release_gate_identity == release_gate.release_gate_identity
        && gate.hardening_matrix_identity == matrix.hardening_matrix_identity
        && gate.debug_dump_contract_identity == debug_contract.debug_dump_contract_identity
        && gate.rollback_gate_identity == builtin_derive_rollback_diagnostic_gate_identity(
               placeholder, parser_contract, release_gate, matrix, debug_contract, expected_query_name,
               diagnostic_projection_count, diagnostic_report_entry_count, blocked_diagnostic_count,
               derive_diagnostic_count, empty_diagnostic_count, parser_consumption_contract_count)
        && gate.rollback_policy == FRONTEND_MACRO_M22F_ROLLBACK_POLICY
        && gate.rollback_query_name == expected_query_name
        && gate.blocked_reason == FRONTEND_MACRO_M22F_ROLLBACK_BLOCKER
        && gate.diagnostic_projection_count == diagnostic_projection_count
        && gate.diagnostic_report_entry_count == diagnostic_report_entry_count
        && gate.blocked_diagnostic_count == blocked_diagnostic_count
        && gate.derive_diagnostic_count == derive_diagnostic_count
        && gate.empty_diagnostic_count == empty_diagnostic_count
        && gate.parser_consumption_contract_count == parser_consumption_contract_count
        && gate.rollback_diagnostic_design_available
        && gate.diagnostic_grouping_available
        && gate.source_anchor_available
        && gate.token_tree_anchor_available
        && gate.debug_dump_contract_available
        && gate.release_rollback_plan_complete
        && !gate.rollback_execution_enabled
        && !gate.parser_consumption_enabled
        && !gate.generated_part_parsed
        && !gate.generated_part_merged
        && !gate.emit_expanded_available
        && !gate.debug_trace_available
        && !gate.source_map_available
        && !gate.standard_library_required
        && !gate.runtime_required
        && !gate.external_process_required
        && !gate.produced_user_generated_code
        && gate.rollback_gate_visible
        && gate.query_reusable;
}

[[nodiscard]] bool builtin_derive_rollback_diagnostic_gates_match_groups(
    const EarlyItemExpansionResult& result) noexcept
{
    if (result.builtin_derive_rollback_diagnostic_gates.size() != result.generated_parts.size()
        || result.parser_consumption_contract_gates.size() != result.generated_parts.size()
        || result.builtin_derive_parser_release_gates.size() != result.generated_parts.size()
        || result.builtin_derive_release_hardening_matrices.size() != result.generated_parts.size()
        || result.builtin_derive_debug_dump_contracts.size() != result.generated_parts.size()) {
        return false;
    }
    for (base::usize index = 0; index < result.builtin_derive_rollback_diagnostic_gates.size(); ++index) {
        const GeneratedModulePartPlaceholder& placeholder = result.generated_parts[index];
        if (!builtin_derive_rollback_diagnostic_gate_matches_group(
                result.builtin_derive_rollback_diagnostic_gates[index],
                placeholder,
                result.parser_consumption_contract_gates[index],
                result.builtin_derive_parser_release_gates[index],
                result.builtin_derive_release_hardening_matrices[index],
                result.builtin_derive_debug_dump_contracts[index],
                result.parser_admission_diagnostics,
                result.parser_admission_report_entries)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool builtin_derive_parser_consumption_admission_protocol_matches_group(
    const BuiltinDeriveParserConsumptionAdmissionProtocol& protocol,
    const GeneratedModulePartPlaceholder& placeholder,
    const GeneratedTokenParserConsumptionContractGate& parser_contract,
    const BuiltinDeriveParserConsumptionReleaseGate& release_gate,
    const BuiltinDeriveRollbackDiagnosticDesignGate& rollback_gate,
    const std::vector<GeneratedTokenBufferStub>& buffers,
    const std::vector<GeneratedTokenRecord>& records,
    const std::vector<ParserAdmissionDiagnosticProjectionStub>& diagnostics) noexcept
{
    const base::u64 token_buffer_count = count_part_local_token_buffers(placeholder, buffers);
    const base::u64 token_record_count = count_part_local_token_records(placeholder, records);
    const base::u64 derive_candidate_count = count_part_local_derive_candidate_buffers(placeholder, buffers);
    const base::u64 empty_candidate_count = count_part_local_empty_candidate_buffers(placeholder, buffers);
    const base::u64 blocked_diagnostic_count = count_part_local_blocked_diagnostics(placeholder, diagnostics);
    const std::string expected_query_name =
        builtin_derive_parser_consumption_admission_query_name(placeholder.module,
            placeholder.source_part_index);
    return protocol.module.value == placeholder.module.value
        && protocol.source_part_index == placeholder.source_part_index
        && protocol.attached_part == placeholder.source_part
        && protocol.generated_part == placeholder.generated_part
        && protocol.parser_consumption_contract_identity == parser_contract.contract_identity
        && protocol.release_gate_identity == release_gate.release_gate_identity
        && protocol.rollback_gate_identity == rollback_gate.rollback_gate_identity
        && protocol.admission_protocol_identity
            == builtin_derive_parser_consumption_admission_protocol_identity(placeholder, parser_contract,
                release_gate, rollback_gate, expected_query_name, token_buffer_count, token_record_count,
                derive_candidate_count, empty_candidate_count, blocked_diagnostic_count)
        && protocol.admission_policy == FRONTEND_MACRO_M23A_ADMISSION_POLICY
        && protocol.admission_query_name == expected_query_name
        && protocol.blocked_reason == FRONTEND_MACRO_M23A_ADMISSION_BLOCKER
        && protocol.token_buffer_count == token_buffer_count
        && protocol.token_record_count == token_record_count
        && protocol.derive_candidate_count == derive_candidate_count
        && protocol.empty_candidate_count == empty_candidate_count
        && protocol.blocked_diagnostic_count == blocked_diagnostic_count
        && protocol.release_gate_available
        && protocol.rollback_gate_available
        && protocol.parser_contract_available
        && protocol.deterministic_order_available
        && protocol.generated_tokens_checkpointed
        && protocol.admission_protocol_complete
        && !protocol.parser_consumption_enabled
        && !protocol.parser_admitted
        && !protocol.generated_part_parsed
        && !protocol.generated_part_merged
        && !protocol.emit_expanded_available
        && !protocol.debug_trace_available
        && !protocol.source_map_available
        && !protocol.standard_library_required
        && !protocol.runtime_required
        && !protocol.external_process_required
        && !protocol.produced_user_generated_code
        && protocol.protocol_visible
        && protocol.query_reusable;
}

[[nodiscard]] bool builtin_derive_parser_consumption_admission_protocols_match_groups(
    const EarlyItemExpansionResult& result) noexcept
{
    if (result.builtin_derive_parser_consumption_admission_protocols.size()
            != result.generated_parts.size()
        || result.parser_consumption_contract_gates.size() != result.generated_parts.size()
        || result.builtin_derive_parser_release_gates.size() != result.generated_parts.size()
        || result.builtin_derive_rollback_diagnostic_gates.size() != result.generated_parts.size()) {
        return false;
    }
    for (base::usize index = 0;
         index < result.builtin_derive_parser_consumption_admission_protocols.size();
         ++index) {
        const GeneratedModulePartPlaceholder& placeholder = result.generated_parts[index];
        if (!builtin_derive_parser_consumption_admission_protocol_matches_group(
                result.builtin_derive_parser_consumption_admission_protocols[index],
                placeholder,
                result.parser_consumption_contract_gates[index],
                result.builtin_derive_parser_release_gates[index],
                result.builtin_derive_rollback_diagnostic_gates[index],
                result.generated_token_buffers,
                result.generated_token_records,
                result.parser_admission_diagnostics)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool builtin_derive_checkpoint_rollback_protocol_matches_group(
    const BuiltinDeriveParserConsumptionCheckpointRollbackProtocol& protocol,
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveParserConsumptionAdmissionProtocol& admission_protocol,
    const BuiltinDeriveRollbackDiagnosticDesignGate& rollback_gate) noexcept
{
    const base::u64 checkpoint_count = FRONTEND_MACRO_M23B_CHECKPOINT_PLAN_COUNT;
    const base::u64 rollback_plan_count = FRONTEND_MACRO_M23B_CHECKPOINT_PLAN_COUNT;
    const base::u64 token_record_count = admission_protocol.token_record_count;
    const base::u64 diagnostic_anchor_count = admission_protocol.blocked_diagnostic_count;
    const std::string expected_query_name =
        builtin_derive_checkpoint_rollback_query_name(placeholder.module, placeholder.source_part_index);
    return protocol.module.value == placeholder.module.value
        && protocol.source_part_index == placeholder.source_part_index
        && protocol.attached_part == placeholder.source_part
        && protocol.generated_part == placeholder.generated_part
        && protocol.admission_protocol_identity == admission_protocol.admission_protocol_identity
        && protocol.rollback_gate_identity == rollback_gate.rollback_gate_identity
        && protocol.checkpoint_protocol_identity == builtin_derive_checkpoint_rollback_protocol_identity(
               placeholder, admission_protocol, rollback_gate, expected_query_name, checkpoint_count,
               rollback_plan_count, token_record_count, diagnostic_anchor_count)
        && protocol.checkpoint_policy == FRONTEND_MACRO_M23B_CHECKPOINT_POLICY
        && protocol.checkpoint_query_name == expected_query_name
        && protocol.blocked_reason == FRONTEND_MACRO_M23B_CHECKPOINT_BLOCKER
        && protocol.checkpoint_count == checkpoint_count
        && protocol.rollback_plan_count == rollback_plan_count
        && protocol.token_record_count == token_record_count
        && protocol.diagnostic_anchor_count == diagnostic_anchor_count
        && protocol.parser_state_checkpoint_available
        && protocol.token_cursor_checkpoint_available
        && protocol.generated_part_checkpoint_available
        && protocol.diagnostic_replay_available
        && protocol.rollback_protocol_complete
        && !protocol.rollback_execution_enabled
        && !protocol.parser_consumption_enabled
        && !protocol.generated_part_parsed
        && !protocol.generated_part_merged
        && !protocol.emit_expanded_available
        && !protocol.debug_trace_available
        && !protocol.source_map_available
        && !protocol.standard_library_required
        && !protocol.runtime_required
        && !protocol.external_process_required
        && !protocol.produced_user_generated_code
        && protocol.protocol_visible
        && protocol.query_reusable;
}

[[nodiscard]] bool builtin_derive_checkpoint_rollback_protocols_match_groups(
    const EarlyItemExpansionResult& result) noexcept
{
    if (result.builtin_derive_checkpoint_rollback_protocols.size() != result.generated_parts.size()
        || result.builtin_derive_parser_consumption_admission_protocols.size()
            != result.generated_parts.size()
        || result.builtin_derive_rollback_diagnostic_gates.size() != result.generated_parts.size()) {
        return false;
    }
    for (base::usize index = 0; index < result.builtin_derive_checkpoint_rollback_protocols.size(); ++index) {
        const GeneratedModulePartPlaceholder& placeholder = result.generated_parts[index];
        if (!builtin_derive_checkpoint_rollback_protocol_matches_group(
                result.builtin_derive_checkpoint_rollback_protocols[index],
                placeholder,
                result.builtin_derive_parser_consumption_admission_protocols[index],
                result.builtin_derive_rollback_diagnostic_gates[index])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool builtin_derive_preconsumption_verification_closure_matches_group(
    const BuiltinDeriveParserPreConsumptionVerificationClosure& closure,
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveParserConsumptionAdmissionProtocol& admission_protocol,
    const BuiltinDeriveParserConsumptionCheckpointRollbackProtocol& checkpoint_protocol,
    const BuiltinDeriveReleaseHardeningMatrix& matrix,
    const BuiltinDeriveDebugDumpStabilityContract& debug_contract,
    const BuiltinDeriveRollbackDiagnosticDesignGate& rollback_gate) noexcept
{
    const base::u64 admission_protocol_count = admission_protocol.protocol_visible ? 1U : 0U;
    const base::u64 checkpoint_protocol_count = checkpoint_protocol.protocol_visible ? 1U : 0U;
    const base::u64 hardening_matrix_count = matrix.matrix_visible ? 1U : 0U;
    const base::u64 debug_dump_contract_count = debug_contract.contract_visible ? 1U : 0U;
    const base::u64 rollback_gate_count = rollback_gate.rollback_gate_visible ? 1U : 0U;
    const std::string expected_query_name =
        builtin_derive_preconsumption_verification_query_name(placeholder.module,
            placeholder.source_part_index);
    return closure.module.value == placeholder.module.value
        && closure.source_part_index == placeholder.source_part_index
        && closure.attached_part == placeholder.source_part
        && closure.generated_part == placeholder.generated_part
        && closure.admission_protocol_identity == admission_protocol.admission_protocol_identity
        && closure.checkpoint_protocol_identity == checkpoint_protocol.checkpoint_protocol_identity
        && closure.debug_dump_contract_identity == debug_contract.debug_dump_contract_identity
        && closure.verification_closure_identity
            == builtin_derive_preconsumption_verification_closure_identity(placeholder, admission_protocol,
                checkpoint_protocol, debug_contract, expected_query_name, admission_protocol_count,
                checkpoint_protocol_count, hardening_matrix_count, debug_dump_contract_count,
                rollback_gate_count)
        && closure.verification_policy == FRONTEND_MACRO_M23C_VERIFICATION_POLICY
        && closure.verification_query_name == expected_query_name
        && closure.blocked_reason == FRONTEND_MACRO_M23C_VERIFICATION_BLOCKER
        && closure.admission_protocol_count == admission_protocol_count
        && closure.checkpoint_protocol_count == checkpoint_protocol_count
        && closure.hardening_matrix_count == hardening_matrix_count
        && closure.debug_dump_contract_count == debug_dump_contract_count
        && closure.rollback_gate_count == rollback_gate_count
        && closure.admission_protocol_available
        && closure.checkpoint_protocol_available
        && closure.release_hardening_available
        && closure.debug_dump_contract_available
        && closure.rollback_gate_available
        && closure.verification_closure_complete
        && !closure.parser_consumption_enabled
        && !closure.generated_part_parsed
        && !closure.generated_part_merged
        && !closure.sema_visible
        && !closure.emit_expanded_available
        && !closure.debug_trace_available
        && !closure.source_map_available
        && !closure.standard_library_required
        && !closure.runtime_required
        && !closure.external_process_required
        && !closure.produced_user_generated_code
        && closure.closure_visible
        && closure.query_reusable;
}

[[nodiscard]] bool builtin_derive_preconsumption_verification_closures_match_groups(
    const EarlyItemExpansionResult& result) noexcept
{
    if (result.builtin_derive_preconsumption_verification_closures.size() != result.generated_parts.size()
        || result.builtin_derive_parser_consumption_admission_protocols.size()
            != result.generated_parts.size()
        || result.builtin_derive_checkpoint_rollback_protocols.size() != result.generated_parts.size()
        || result.builtin_derive_release_hardening_matrices.size() != result.generated_parts.size()
        || result.builtin_derive_debug_dump_contracts.size() != result.generated_parts.size()
        || result.builtin_derive_rollback_diagnostic_gates.size() != result.generated_parts.size()) {
        return false;
    }
    for (base::usize index = 0;
         index < result.builtin_derive_preconsumption_verification_closures.size();
         ++index) {
        const GeneratedModulePartPlaceholder& placeholder = result.generated_parts[index];
        if (!builtin_derive_preconsumption_verification_closure_matches_group(
                result.builtin_derive_preconsumption_verification_closures[index],
                placeholder,
                result.builtin_derive_parser_consumption_admission_protocols[index],
                result.builtin_derive_checkpoint_rollback_protocols[index],
                result.builtin_derive_release_hardening_matrices[index],
                result.builtin_derive_debug_dump_contracts[index],
                result.builtin_derive_rollback_diagnostic_gates[index])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool builtin_derive_controlled_dry_run_adapter_matches_group(
    const BuiltinDeriveControlledParserDryRunAdapter& adapter,
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveParserPreConsumptionVerificationClosure& verification_closure,
    const BuiltinDeriveParserConsumptionAdmissionProtocol& admission_protocol,
    const BuiltinDeriveParserConsumptionCheckpointRollbackProtocol& checkpoint_protocol) noexcept
{
    const base::u64 token_record_count = checkpoint_protocol.token_record_count;
    const base::u64 diagnostic_anchor_count = checkpoint_protocol.diagnostic_anchor_count;
    const base::u64 prerequisite_count = FRONTEND_MACRO_M24A_DRY_RUN_PREREQUISITE_COUNT;
    const std::string expected_query_name =
        builtin_derive_controlled_dry_run_adapter_query_name(placeholder.module,
            placeholder.source_part_index);
    return adapter.module.value == placeholder.module.value
        && adapter.source_part_index == placeholder.source_part_index
        && adapter.attached_part == placeholder.source_part
        && adapter.generated_part == placeholder.generated_part
        && adapter.verification_closure_identity == verification_closure.verification_closure_identity
        && adapter.admission_protocol_identity == admission_protocol.admission_protocol_identity
        && adapter.checkpoint_protocol_identity == checkpoint_protocol.checkpoint_protocol_identity
        && adapter.dry_run_adapter_identity
            == builtin_derive_controlled_dry_run_adapter_identity(placeholder, verification_closure,
                admission_protocol, checkpoint_protocol, expected_query_name, token_record_count,
                diagnostic_anchor_count, prerequisite_count)
        && adapter.adapter_policy == FRONTEND_MACRO_M24A_DRY_RUN_ADAPTER_POLICY
        && adapter.adapter_query_name == expected_query_name
        && adapter.blocked_reason == FRONTEND_MACRO_M24A_DRY_RUN_ADAPTER_BLOCKER
        && adapter.token_record_count == token_record_count
        && adapter.diagnostic_anchor_count == diagnostic_anchor_count
        && adapter.prerequisite_count == prerequisite_count
        && adapter.verification_closure_available
        && adapter.admission_protocol_available
        && adapter.checkpoint_protocol_available
        && adapter.compiler_owned_tokens_available
        && adapter.diagnostic_replay_available
        && adapter.dry_run_adapter_complete
        && !adapter.dry_run_executed
        && !adapter.parser_consumption_enabled
        && !adapter.parser_admitted
        && !adapter.generated_part_parsed
        && !adapter.generated_part_merged
        && !adapter.sema_visible
        && !adapter.emit_expanded_available
        && !adapter.debug_trace_available
        && !adapter.source_map_available
        && !adapter.standard_library_required
        && !adapter.runtime_required
        && !adapter.external_process_required
        && !adapter.produced_user_generated_code
        && adapter.adapter_visible
        && adapter.query_reusable;
}

[[nodiscard]] bool builtin_derive_controlled_dry_run_adapters_match_groups(
    const EarlyItemExpansionResult& result) noexcept
{
    if (result.builtin_derive_controlled_dry_run_adapters.size() != result.generated_parts.size()
        || result.builtin_derive_preconsumption_verification_closures.size() != result.generated_parts.size()
        || result.builtin_derive_parser_consumption_admission_protocols.size()
            != result.generated_parts.size()
        || result.builtin_derive_checkpoint_rollback_protocols.size() != result.generated_parts.size()) {
        return false;
    }
    for (base::usize index = 0; index < result.builtin_derive_controlled_dry_run_adapters.size();
         ++index) {
        const GeneratedModulePartPlaceholder& placeholder = result.generated_parts[index];
        if (!builtin_derive_controlled_dry_run_adapter_matches_group(
                result.builtin_derive_controlled_dry_run_adapters[index],
                placeholder,
                result.builtin_derive_preconsumption_verification_closures[index],
                result.builtin_derive_parser_consumption_admission_protocols[index],
                result.builtin_derive_checkpoint_rollback_protocols[index])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool builtin_derive_dry_run_rollback_replay_matches_group(
    const BuiltinDeriveDryRunRollbackDiagnosticReplay& replay,
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveControlledParserDryRunAdapter& adapter,
    const BuiltinDeriveParserConsumptionCheckpointRollbackProtocol& checkpoint_protocol,
    const BuiltinDeriveRollbackDiagnosticDesignGate& rollback_gate) noexcept
{
    const base::u64 diagnostic_anchor_count = adapter.diagnostic_anchor_count;
    const base::u64 report_entry_count = rollback_gate.diagnostic_report_entry_count;
    const base::u64 planned_replay_count = diagnostic_anchor_count;
    const base::u64 executed_replay_count = 0U;
    const std::string expected_query_name =
        builtin_derive_dry_run_rollback_replay_query_name(placeholder.module,
            placeholder.source_part_index);
    return replay.module.value == placeholder.module.value
        && replay.source_part_index == placeholder.source_part_index
        && replay.attached_part == placeholder.source_part
        && replay.generated_part == placeholder.generated_part
        && replay.dry_run_adapter_identity == adapter.dry_run_adapter_identity
        && replay.checkpoint_protocol_identity == checkpoint_protocol.checkpoint_protocol_identity
        && replay.rollback_gate_identity == rollback_gate.rollback_gate_identity
        && replay.replay_protocol_identity == builtin_derive_dry_run_rollback_replay_identity(
               placeholder, adapter, checkpoint_protocol, rollback_gate, expected_query_name,
               diagnostic_anchor_count, report_entry_count, planned_replay_count, executed_replay_count)
        && replay.replay_policy == FRONTEND_MACRO_M24B_ROLLBACK_REPLAY_POLICY
        && replay.replay_query_name == expected_query_name
        && replay.blocked_reason == FRONTEND_MACRO_M24B_ROLLBACK_REPLAY_BLOCKER
        && replay.diagnostic_anchor_count == diagnostic_anchor_count
        && replay.report_entry_count == report_entry_count
        && replay.planned_replay_count == planned_replay_count
        && replay.executed_replay_count == executed_replay_count
        && replay.dry_run_adapter_available
        && replay.checkpoint_protocol_available
        && replay.rollback_gate_available
        && replay.diagnostic_replay_plan_available
        && replay.replay_protocol_complete
        && !replay.replay_execution_enabled
        && !replay.dry_run_executed
        && !replay.parser_consumption_enabled
        && !replay.generated_part_parsed
        && !replay.generated_part_merged
        && !replay.sema_visible
        && !replay.emit_expanded_available
        && !replay.debug_trace_available
        && !replay.source_map_available
        && !replay.standard_library_required
        && !replay.runtime_required
        && !replay.external_process_required
        && !replay.produced_user_generated_code
        && replay.replay_visible
        && replay.query_reusable;
}

[[nodiscard]] bool builtin_derive_dry_run_rollback_replays_match_groups(
    const EarlyItemExpansionResult& result) noexcept
{
    if (result.builtin_derive_dry_run_rollback_replays.size() != result.generated_parts.size()
        || result.builtin_derive_controlled_dry_run_adapters.size() != result.generated_parts.size()
        || result.builtin_derive_checkpoint_rollback_protocols.size() != result.generated_parts.size()
        || result.builtin_derive_rollback_diagnostic_gates.size() != result.generated_parts.size()) {
        return false;
    }
    for (base::usize index = 0; index < result.builtin_derive_dry_run_rollback_replays.size(); ++index) {
        const GeneratedModulePartPlaceholder& placeholder = result.generated_parts[index];
        if (!builtin_derive_dry_run_rollback_replay_matches_group(
                result.builtin_derive_dry_run_rollback_replays[index],
                placeholder,
                result.builtin_derive_controlled_dry_run_adapters[index],
                result.builtin_derive_checkpoint_rollback_protocols[index],
                result.builtin_derive_rollback_diagnostic_gates[index])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool builtin_derive_dry_run_negative_matrix_matches_group(
    const BuiltinDeriveDryRunNegativeMatrixClosure& matrix,
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveControlledParserDryRunAdapter& adapter,
    const BuiltinDeriveDryRunRollbackDiagnosticReplay& replay,
    const BuiltinDeriveParserPreConsumptionVerificationClosure& verification_closure) noexcept
{
    const base::u64 dry_run_adapter_count = adapter.adapter_visible ? 1U : 0U;
    const base::u64 rollback_replay_count = replay.replay_visible ? 1U : 0U;
    const base::u64 verification_closure_count = verification_closure.closure_visible ? 1U : 0U;
    const base::u64 negative_case_count = FRONTEND_MACRO_M24C_NEGATIVE_CASE_COUNT;
    const base::u64 parser_consumable_case_count = 0U;
    const std::string expected_query_name =
        builtin_derive_dry_run_negative_matrix_query_name(placeholder.module,
            placeholder.source_part_index);
    return matrix.module.value == placeholder.module.value
        && matrix.source_part_index == placeholder.source_part_index
        && matrix.attached_part == placeholder.source_part
        && matrix.generated_part == placeholder.generated_part
        && matrix.dry_run_adapter_identity == adapter.dry_run_adapter_identity
        && matrix.rollback_replay_identity == replay.replay_protocol_identity
        && matrix.verification_closure_identity == verification_closure.verification_closure_identity
        && matrix.negative_matrix_identity == builtin_derive_dry_run_negative_matrix_identity(
               placeholder, adapter, replay, verification_closure, expected_query_name,
               dry_run_adapter_count, rollback_replay_count, verification_closure_count,
               negative_case_count, parser_consumable_case_count)
        && matrix.matrix_policy == FRONTEND_MACRO_M24C_NEGATIVE_MATRIX_POLICY
        && matrix.matrix_query_name == expected_query_name
        && matrix.blocked_reason == FRONTEND_MACRO_M24C_NEGATIVE_MATRIX_BLOCKER
        && matrix.dry_run_adapter_count == dry_run_adapter_count
        && matrix.rollback_replay_count == rollback_replay_count
        && matrix.verification_closure_count == verification_closure_count
        && matrix.negative_case_count == negative_case_count
        && matrix.parser_consumable_case_count == parser_consumable_case_count
        && matrix.dry_run_adapter_available
        && matrix.rollback_replay_available
        && matrix.verification_closure_available
        && matrix.negative_matrix_complete
        && !matrix.dry_run_executed
        && !matrix.parser_consumption_enabled
        && !matrix.parser_admitted
        && !matrix.generated_part_parsed
        && !matrix.generated_part_merged
        && !matrix.sema_visible
        && !matrix.emit_expanded_available
        && !matrix.debug_trace_available
        && !matrix.source_map_available
        && !matrix.standard_library_required
        && !matrix.runtime_required
        && !matrix.external_process_required
        && !matrix.produced_user_generated_code
        && matrix.matrix_visible
        && matrix.query_reusable;
}

[[nodiscard]] bool builtin_derive_dry_run_negative_matrices_match_groups(
    const EarlyItemExpansionResult& result) noexcept
{
    if (result.builtin_derive_dry_run_negative_matrices.size() != result.generated_parts.size()
        || result.builtin_derive_controlled_dry_run_adapters.size() != result.generated_parts.size()
        || result.builtin_derive_dry_run_rollback_replays.size() != result.generated_parts.size()
        || result.builtin_derive_preconsumption_verification_closures.size() != result.generated_parts.size()) {
        return false;
    }
    for (base::usize index = 0; index < result.builtin_derive_dry_run_negative_matrices.size();
         ++index) {
        const GeneratedModulePartPlaceholder& placeholder = result.generated_parts[index];
        if (!builtin_derive_dry_run_negative_matrix_matches_group(
                result.builtin_derive_dry_run_negative_matrices[index],
                placeholder,
                result.builtin_derive_controlled_dry_run_adapters[index],
                result.builtin_derive_dry_run_rollback_replays[index],
                result.builtin_derive_preconsumption_verification_closures[index])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool builtin_derive_parser_dry_run_session_matches_group(
    const BuiltinDeriveParserDryRunSessionBoundary& session,
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveControlledParserDryRunAdapter& adapter,
    const BuiltinDeriveDryRunNegativeMatrixClosure& matrix,
    const GeneratedModulePartParseMergeStub& parse_merge_stub) noexcept
{
    const base::u64 token_buffer_candidate_count = adapter.token_record_count > 0U ? 1U : 0U;
    const base::u64 token_record_count = adapter.token_record_count;
    const base::u64 diagnostic_anchor_count = adapter.diagnostic_anchor_count;
    const base::u64 parser_state_snapshot_count = FRONTEND_MACRO_M25A_PARSER_STATE_SNAPSHOT_COUNT;
    const base::u64 committed_parse_count = 0U;
    const std::string expected_query_name =
        builtin_derive_parser_dry_run_session_query_name(placeholder.module,
            placeholder.source_part_index);
    return session.module.value == placeholder.module.value
        && session.source_part_index == placeholder.source_part_index
        && session.attached_part == placeholder.source_part
        && session.generated_part == placeholder.generated_part
        && session.dry_run_adapter_identity == adapter.dry_run_adapter_identity
        && session.negative_matrix_identity == matrix.negative_matrix_identity
        && session.generated_buffer_identity == parse_merge_stub.generated_buffer_identity
        && session.parse_config_fingerprint == parse_merge_stub.parse_config_fingerprint
        && session.dry_run_session_identity == builtin_derive_parser_dry_run_session_identity(
               placeholder, adapter, matrix, parse_merge_stub, expected_query_name,
               token_buffer_candidate_count, token_record_count, diagnostic_anchor_count,
               parser_state_snapshot_count, committed_parse_count)
        && session.session_policy == FRONTEND_MACRO_M25A_DRY_RUN_SESSION_POLICY
        && session.session_query_name == expected_query_name
        && session.blocked_reason == FRONTEND_MACRO_M25A_DRY_RUN_SESSION_BLOCKER
        && session.token_buffer_candidate_count == token_buffer_candidate_count
        && session.token_record_count == token_record_count
        && session.diagnostic_anchor_count == diagnostic_anchor_count
        && session.parser_state_snapshot_count == parser_state_snapshot_count
        && session.committed_parse_count == committed_parse_count
        && session.dry_run_adapter_available
        && session.negative_matrix_available
        && session.compiler_owned_token_stream_available
        && session.sandbox_available
        && session.check_only
        && session.dry_run_session_complete
        && !session.dry_run_executed
        && !session.session_committed
        && !session.parser_consumption_enabled
        && !session.parser_admitted
        && !session.parser_cursor_advanced
        && !session.generated_part_parsed
        && !session.generated_part_merged
        && !session.ast_mutated
        && !session.sema_visible
        && !session.emit_expanded_available
        && !session.debug_trace_available
        && !session.source_map_available
        && !session.standard_library_required
        && !session.runtime_required
        && !session.external_process_required
        && !session.produced_user_generated_code
        && session.session_visible
        && session.query_reusable;
}

[[nodiscard]] bool builtin_derive_parser_dry_run_sessions_match_groups(
    const EarlyItemExpansionResult& result) noexcept
{
    if (result.builtin_derive_parser_dry_run_sessions.size() != result.generated_parts.size()
        || result.builtin_derive_controlled_dry_run_adapters.size() != result.generated_parts.size()
        || result.builtin_derive_dry_run_negative_matrices.size() != result.generated_parts.size()
        || result.generated_part_stubs.size() != result.generated_parts.size()) {
        return false;
    }
    for (base::usize index = 0; index < result.builtin_derive_parser_dry_run_sessions.size();
         ++index) {
        if (!builtin_derive_parser_dry_run_session_matches_group(
                result.builtin_derive_parser_dry_run_sessions[index],
                result.generated_parts[index],
                result.builtin_derive_controlled_dry_run_adapters[index],
                result.builtin_derive_dry_run_negative_matrices[index],
                result.generated_part_stubs[index])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool builtin_derive_token_cursor_snapshot_proof_matches_group(
    const BuiltinDeriveTokenCursorSnapshotRollbackProof& proof,
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveParserDryRunSessionBoundary& session,
    const BuiltinDeriveParserConsumptionCheckpointRollbackProtocol& checkpoint_protocol,
    const BuiltinDeriveDryRunRollbackDiagnosticReplay& replay) noexcept
{
    const base::u64 token_record_count = session.token_record_count;
    const base::u64 checkpoint_count = checkpoint_protocol.checkpoint_count;
    const base::u64 cursor_snapshot_count = checkpoint_protocol.checkpoint_count;
    const base::u64 parser_state_snapshot_count = checkpoint_protocol.checkpoint_count;
    const base::u64 rollback_proof_count = checkpoint_protocol.rollback_plan_count;
    const base::u64 cursor_commit_count = 0U;
    const std::string expected_query_name =
        builtin_derive_token_cursor_snapshot_query_name(placeholder.module,
            placeholder.source_part_index);
    return proof.module.value == placeholder.module.value
        && proof.source_part_index == placeholder.source_part_index
        && proof.attached_part == placeholder.source_part
        && proof.generated_part == placeholder.generated_part
        && proof.dry_run_session_identity == session.dry_run_session_identity
        && proof.checkpoint_protocol_identity == checkpoint_protocol.checkpoint_protocol_identity
        && proof.rollback_replay_identity == replay.replay_protocol_identity
        && proof.cursor_snapshot_identity == builtin_derive_token_cursor_snapshot_identity(
               placeholder, session, checkpoint_protocol, replay, expected_query_name,
               token_record_count, checkpoint_count, cursor_snapshot_count,
               parser_state_snapshot_count, rollback_proof_count, cursor_commit_count)
        && proof.snapshot_policy == FRONTEND_MACRO_M25B_CURSOR_SNAPSHOT_POLICY
        && proof.snapshot_query_name == expected_query_name
        && proof.blocked_reason == FRONTEND_MACRO_M25B_CURSOR_SNAPSHOT_BLOCKER
        && proof.token_record_count == token_record_count
        && proof.checkpoint_count == checkpoint_count
        && proof.cursor_snapshot_count == cursor_snapshot_count
        && proof.parser_state_snapshot_count == parser_state_snapshot_count
        && proof.rollback_proof_count == rollback_proof_count
        && proof.cursor_commit_count == cursor_commit_count
        && proof.dry_run_session_available
        && proof.checkpoint_protocol_available
        && proof.rollback_replay_available
        && proof.token_cursor_snapshot_available
        && proof.parser_state_snapshot_available
        && proof.rollback_proof_complete
        && !proof.replay_execution_enabled
        && !proof.rollback_execution_enabled
        && !proof.dry_run_executed
        && !proof.parser_cursor_advanced
        && !proof.session_committed
        && !proof.parser_consumption_enabled
        && !proof.parser_admitted
        && !proof.generated_part_parsed
        && !proof.generated_part_merged
        && !proof.ast_mutated
        && !proof.sema_visible
        && !proof.standard_library_required
        && !proof.runtime_required
        && !proof.external_process_required
        && !proof.produced_user_generated_code
        && proof.proof_visible
        && proof.query_reusable;
}

[[nodiscard]] bool builtin_derive_token_cursor_snapshot_proofs_match_groups(
    const EarlyItemExpansionResult& result) noexcept
{
    if (result.builtin_derive_token_cursor_snapshot_proofs.size() != result.generated_parts.size()
        || result.builtin_derive_parser_dry_run_sessions.size() != result.generated_parts.size()
        || result.builtin_derive_checkpoint_rollback_protocols.size() != result.generated_parts.size()
        || result.builtin_derive_dry_run_rollback_replays.size() != result.generated_parts.size()) {
        return false;
    }
    for (base::usize index = 0; index < result.builtin_derive_token_cursor_snapshot_proofs.size();
         ++index) {
        if (!builtin_derive_token_cursor_snapshot_proof_matches_group(
                result.builtin_derive_token_cursor_snapshot_proofs[index],
                result.generated_parts[index],
                result.builtin_derive_parser_dry_run_sessions[index],
                result.builtin_derive_checkpoint_rollback_protocols[index],
                result.builtin_derive_dry_run_rollback_replays[index])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool builtin_derive_diagnostic_shadow_closure_matches_group(
    const BuiltinDeriveDiagnosticShadowNoAstMutationClosure& closure,
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveParserDryRunSessionBoundary& session,
    const BuiltinDeriveTokenCursorSnapshotRollbackProof& proof,
    const BuiltinDeriveDryRunRollbackDiagnosticReplay& replay,
    const BuiltinDeriveDryRunNegativeMatrixClosure& matrix) noexcept
{
    const base::u64 dry_run_session_count = session.session_visible ? 1U : 0U;
    const base::u64 cursor_snapshot_proof_count = proof.proof_visible ? 1U : 0U;
    const base::u64 rollback_replay_count = replay.replay_visible ? 1U : 0U;
    const base::u64 negative_matrix_count = matrix.matrix_visible ? 1U : 0U;
    const base::u64 diagnostic_shadow_count = replay.planned_replay_count;
    const base::u64 executed_shadow_count = 0U;
    const base::u64 ast_mutation_count = 0U;
    const base::u64 parser_consumable_case_count = 0U;
    const std::string expected_query_name =
        builtin_derive_diagnostic_shadow_query_name(placeholder.module,
            placeholder.source_part_index);
    return closure.module.value == placeholder.module.value
        && closure.source_part_index == placeholder.source_part_index
        && closure.attached_part == placeholder.source_part
        && closure.generated_part == placeholder.generated_part
        && closure.dry_run_session_identity == session.dry_run_session_identity
        && closure.cursor_snapshot_identity == proof.cursor_snapshot_identity
        && closure.rollback_replay_identity == replay.replay_protocol_identity
        && closure.negative_matrix_identity == matrix.negative_matrix_identity
        && closure.closure_identity == builtin_derive_diagnostic_shadow_closure_identity(
               placeholder, session, proof, replay, matrix, expected_query_name,
               dry_run_session_count, cursor_snapshot_proof_count, rollback_replay_count,
               negative_matrix_count, diagnostic_shadow_count, executed_shadow_count,
               ast_mutation_count, parser_consumable_case_count)
        && closure.closure_policy == FRONTEND_MACRO_M25C_DIAGNOSTIC_SHADOW_POLICY
        && closure.closure_query_name == expected_query_name
        && closure.blocked_reason == FRONTEND_MACRO_M25C_DIAGNOSTIC_SHADOW_BLOCKER
        && closure.dry_run_session_count == dry_run_session_count
        && closure.cursor_snapshot_proof_count == cursor_snapshot_proof_count
        && closure.rollback_replay_count == rollback_replay_count
        && closure.negative_matrix_count == negative_matrix_count
        && closure.diagnostic_shadow_count == diagnostic_shadow_count
        && closure.executed_shadow_count == executed_shadow_count
        && closure.ast_mutation_count == ast_mutation_count
        && closure.parser_consumable_case_count == parser_consumable_case_count
        && closure.dry_run_session_available
        && closure.cursor_snapshot_proof_available
        && closure.rollback_replay_available
        && closure.negative_matrix_available
        && closure.diagnostic_shadow_available
        && closure.no_ast_mutation_verified
        && closure.closure_complete
        && !closure.dry_run_executed
        && !closure.replay_execution_enabled
        && !closure.session_committed
        && !closure.parser_cursor_advanced
        && !closure.parser_consumption_enabled
        && !closure.parser_admitted
        && !closure.generated_part_parsed
        && !closure.generated_part_merged
        && !closure.ast_mutated
        && !closure.sema_visible
        && !closure.emit_expanded_available
        && !closure.debug_trace_available
        && !closure.source_map_available
        && !closure.standard_library_required
        && !closure.runtime_required
        && !closure.external_process_required
        && !closure.produced_user_generated_code
        && closure.closure_visible
        && closure.query_reusable;
}

[[nodiscard]] bool builtin_derive_diagnostic_shadow_closures_match_groups(
    const EarlyItemExpansionResult& result) noexcept
{
    if (result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures.size()
            != result.generated_parts.size()
        || result.builtin_derive_parser_dry_run_sessions.size() != result.generated_parts.size()
        || result.builtin_derive_token_cursor_snapshot_proofs.size() != result.generated_parts.size()
        || result.builtin_derive_dry_run_rollback_replays.size() != result.generated_parts.size()
        || result.builtin_derive_dry_run_negative_matrices.size() != result.generated_parts.size()) {
        return false;
    }
    for (base::usize index = 0;
         index < result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures.size();
         ++index) {
        if (!builtin_derive_diagnostic_shadow_closure_matches_group(
                result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures[index],
                result.generated_parts[index],
                result.builtin_derive_parser_dry_run_sessions[index],
                result.builtin_derive_token_cursor_snapshot_proofs[index],
                result.builtin_derive_dry_run_rollback_replays[index],
                result.builtin_derive_dry_run_negative_matrices[index])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool builtin_derive_parser_dry_run_admission_gate_matches_group(
    const BuiltinDeriveParserDryRunAdmissionGate& gate,
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveParserDryRunSessionBoundary& session,
    const BuiltinDeriveTokenCursorSnapshotRollbackProof& proof,
    const BuiltinDeriveDiagnosticShadowNoAstMutationClosure& closure,
    const GeneratedModulePartParseMergeStub& parse_merge_stub) noexcept
{
    const base::u64 dry_run_session_count = session.session_visible ? 1U : 0U;
    const base::u64 cursor_snapshot_proof_count = proof.proof_visible ? 1U : 0U;
    const base::u64 diagnostic_shadow_closure_count = closure.closure_visible ? 1U : 0U;
    const base::u64 admission_prerequisite_count =
        FRONTEND_MACRO_M26A_DRY_RUN_ADMISSION_PREREQUISITE_COUNT;
    const base::u64 token_buffer_candidate_count = session.token_buffer_candidate_count;
    const base::u64 token_record_count = session.token_record_count;
    const base::u64 dry_run_execution_admitted_count = 0U;
    const base::u64 parser_consumable_case_count = 0U;
    const std::string expected_query_name =
        builtin_derive_dry_run_admission_gate_query_name(placeholder.module,
            placeholder.source_part_index);
    return gate.module.value == placeholder.module.value
        && gate.source_part_index == placeholder.source_part_index
        && gate.attached_part == placeholder.source_part
        && gate.generated_part == placeholder.generated_part
        && gate.dry_run_session_identity == session.dry_run_session_identity
        && gate.cursor_snapshot_identity == proof.cursor_snapshot_identity
        && gate.diagnostic_shadow_closure_identity == closure.closure_identity
        && gate.generated_buffer_identity == parse_merge_stub.generated_buffer_identity
        && gate.parse_config_fingerprint == parse_merge_stub.parse_config_fingerprint
        && gate.admission_gate_identity == builtin_derive_dry_run_admission_gate_identity(
               placeholder, session, proof, closure, parse_merge_stub, expected_query_name,
               dry_run_session_count, cursor_snapshot_proof_count,
               diagnostic_shadow_closure_count, admission_prerequisite_count,
               token_buffer_candidate_count, token_record_count,
               dry_run_execution_admitted_count, parser_consumable_case_count)
        && gate.admission_policy == FRONTEND_MACRO_M26A_DRY_RUN_ADMISSION_GATE_POLICY
        && gate.admission_query_name == expected_query_name
        && gate.blocked_reason == FRONTEND_MACRO_M26A_DRY_RUN_ADMISSION_GATE_BLOCKER
        && gate.dry_run_session_count == dry_run_session_count
        && gate.cursor_snapshot_proof_count == cursor_snapshot_proof_count
        && gate.diagnostic_shadow_closure_count == diagnostic_shadow_closure_count
        && gate.admission_prerequisite_count == admission_prerequisite_count
        && gate.token_buffer_candidate_count == token_buffer_candidate_count
        && gate.token_record_count == token_record_count
        && gate.dry_run_execution_admitted_count == dry_run_execution_admitted_count
        && gate.parser_consumable_case_count == parser_consumable_case_count
        && gate.dry_run_session_available
        && gate.cursor_snapshot_proof_available
        && gate.diagnostic_shadow_closure_available
        && gate.generated_buffer_available
        && gate.parse_config_available
        && gate.admission_gate_complete
        && !gate.dry_run_execution_admitted
        && !gate.dry_run_executed
        && !gate.diagnostic_shadow_executed
        && !gate.rollback_execution_enabled
        && !gate.session_committed
        && !gate.parser_cursor_advanced
        && !gate.parser_consumption_enabled
        && !gate.parser_admitted
        && !gate.generated_part_parsed
        && !gate.generated_part_merged
        && !gate.ast_mutated
        && !gate.sema_visible
        && !gate.emit_expanded_available
        && !gate.debug_trace_available
        && !gate.source_map_available
        && !gate.standard_library_required
        && !gate.runtime_required
        && !gate.external_process_required
        && !gate.produced_user_generated_code
        && gate.gate_visible
        && gate.query_reusable;
}

[[nodiscard]] bool builtin_derive_parser_dry_run_admission_gates_match_groups(
    const EarlyItemExpansionResult& result) noexcept
{
    if (result.builtin_derive_parser_dry_run_admission_gates.size() != result.generated_parts.size()
        || result.builtin_derive_parser_dry_run_sessions.size() != result.generated_parts.size()
        || result.builtin_derive_token_cursor_snapshot_proofs.size() != result.generated_parts.size()
        || result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures.size()
            != result.generated_parts.size()
        || result.generated_part_stubs.size() != result.generated_parts.size()) {
        return false;
    }
    for (base::usize index = 0; index < result.builtin_derive_parser_dry_run_admission_gates.size();
         ++index) {
        if (!builtin_derive_parser_dry_run_admission_gate_matches_group(
                result.builtin_derive_parser_dry_run_admission_gates[index],
                result.generated_parts[index],
                result.builtin_derive_parser_dry_run_sessions[index],
                result.builtin_derive_token_cursor_snapshot_proofs[index],
                result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures[index],
                result.generated_part_stubs[index])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool builtin_derive_error_recovery_shadow_diagnostic_gate_matches_group(
    const BuiltinDeriveErrorRecoveryShadowDiagnosticGate& gate,
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveParserDryRunAdmissionGate& admission_gate,
    const BuiltinDeriveDiagnosticShadowNoAstMutationClosure& closure,
    const BuiltinDeriveDryRunRollbackDiagnosticReplay& replay,
    const ParserAdmissionDiagnosticReport& report) noexcept
{
    const base::u64 diagnostic_shadow_count = closure.diagnostic_shadow_count;
    const base::u64 report_entry_count = report.entry_count;
    const base::u64 planned_recovery_count = report.blocked_entry_count;
    const base::u64 executed_recovery_count = 0U;
    const base::u64 emitted_diagnostic_count = 0U;
    const std::string expected_query_name =
        builtin_derive_error_recovery_shadow_diagnostic_query_name(placeholder.module,
            placeholder.source_part_index);
    return gate.module.value == placeholder.module.value
        && gate.source_part_index == placeholder.source_part_index
        && gate.attached_part == placeholder.source_part
        && gate.generated_part == placeholder.generated_part
        && gate.dry_run_admission_gate_identity == admission_gate.admission_gate_identity
        && gate.diagnostic_shadow_closure_identity == closure.closure_identity
        && gate.rollback_replay_identity == replay.replay_protocol_identity
        && gate.parser_report_identity == report.report_identity
        && gate.recovery_shadow_identity == builtin_derive_error_recovery_shadow_identity(
               placeholder, admission_gate, closure, replay, report, expected_query_name,
               diagnostic_shadow_count, report_entry_count, planned_recovery_count,
               executed_recovery_count, emitted_diagnostic_count)
        && gate.recovery_policy == FRONTEND_MACRO_M26B_RECOVERY_SHADOW_GATE_POLICY
        && gate.recovery_query_name == expected_query_name
        && gate.blocked_reason == FRONTEND_MACRO_M26B_RECOVERY_SHADOW_GATE_BLOCKER
        && gate.diagnostic_shadow_count == diagnostic_shadow_count
        && gate.report_entry_count == report_entry_count
        && gate.planned_recovery_count == planned_recovery_count
        && gate.executed_recovery_count == executed_recovery_count
        && gate.emitted_diagnostic_count == emitted_diagnostic_count
        && gate.dry_run_admission_gate_available
        && gate.diagnostic_shadow_closure_available
        && gate.rollback_replay_available
        && gate.parser_report_available
        && gate.recovery_shadow_plan_available
        && gate.recovery_shadow_complete
        && !gate.recovery_execution_enabled
        && !gate.diagnostic_emission_enabled
        && !gate.dry_run_execution_admitted
        && !gate.dry_run_executed
        && !gate.rollback_execution_enabled
        && !gate.session_committed
        && !gate.parser_cursor_advanced
        && !gate.parser_consumption_enabled
        && !gate.parser_admitted
        && !gate.generated_part_parsed
        && !gate.generated_part_merged
        && !gate.ast_mutated
        && !gate.sema_visible
        && !gate.emit_expanded_available
        && !gate.debug_trace_available
        && !gate.source_map_available
        && !gate.standard_library_required
        && !gate.runtime_required
        && !gate.external_process_required
        && !gate.produced_user_generated_code
        && gate.gate_visible
        && gate.query_reusable;
}

[[nodiscard]] bool builtin_derive_error_recovery_shadow_diagnostic_gates_match_groups(
    const EarlyItemExpansionResult& result) noexcept
{
    if (result.builtin_derive_error_recovery_shadow_diagnostic_gates.size()
            != result.generated_parts.size()
        || result.builtin_derive_parser_dry_run_admission_gates.size() != result.generated_parts.size()
        || result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures.size()
            != result.generated_parts.size()
        || result.builtin_derive_dry_run_rollback_replays.size() != result.generated_parts.size()
        || result.parser_admission_reports.size() != result.generated_parts.size()) {
        return false;
    }
    for (base::usize index = 0;
         index < result.builtin_derive_error_recovery_shadow_diagnostic_gates.size();
         ++index) {
        if (!builtin_derive_error_recovery_shadow_diagnostic_gate_matches_group(
                result.builtin_derive_error_recovery_shadow_diagnostic_gates[index],
                result.generated_parts[index],
                result.builtin_derive_parser_dry_run_admission_gates[index],
                result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures[index],
                result.builtin_derive_dry_run_rollback_replays[index],
                result.parser_admission_reports[index])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool builtin_derive_cursor_rollback_ast_mutation_verifier_closure_matches_group(
    const BuiltinDeriveCursorRollbackAstMutationVerifierClosure& closure,
    const GeneratedModulePartPlaceholder& placeholder,
    const BuiltinDeriveParserDryRunAdmissionGate& admission_gate,
    const BuiltinDeriveErrorRecoveryShadowDiagnosticGate& recovery_gate,
    const BuiltinDeriveTokenCursorSnapshotRollbackProof& proof,
    const BuiltinDeriveParserDryRunSessionBoundary& session,
    const BuiltinDeriveDiagnosticShadowNoAstMutationClosure& diagnostic_closure) noexcept
{
    const base::u64 cursor_snapshot_count = proof.cursor_snapshot_count;
    const base::u64 rollback_proof_count = proof.rollback_proof_count;
    const base::u64 recovery_shadow_count = recovery_gate.gate_visible ? 1U : 0U;
    const base::u64 ast_baseline_snapshot_count = FRONTEND_MACRO_M26C_AST_BASELINE_SNAPSHOT_COUNT;
    const base::u64 ast_mutation_count = 0U;
    const base::u64 cursor_commit_count = 0U;
    const base::u64 session_commit_count = 0U;
    const base::u64 parser_consumable_case_count = 0U;
    const std::string expected_query_name =
        builtin_derive_cursor_rollback_ast_verifier_query_name(placeholder.module,
            placeholder.source_part_index);
    return closure.module.value == placeholder.module.value
        && closure.source_part_index == placeholder.source_part_index
        && closure.attached_part == placeholder.source_part
        && closure.generated_part == placeholder.generated_part
        && closure.dry_run_admission_gate_identity == admission_gate.admission_gate_identity
        && closure.recovery_shadow_identity == recovery_gate.recovery_shadow_identity
        && closure.cursor_snapshot_identity == proof.cursor_snapshot_identity
        && closure.dry_run_session_identity == session.dry_run_session_identity
        && closure.diagnostic_shadow_closure_identity == diagnostic_closure.closure_identity
        && closure.verifier_closure_identity
            == builtin_derive_cursor_rollback_ast_verifier_identity(
                placeholder, admission_gate, recovery_gate, proof, session, diagnostic_closure,
                expected_query_name, cursor_snapshot_count, rollback_proof_count,
                recovery_shadow_count, ast_baseline_snapshot_count, ast_mutation_count,
                cursor_commit_count, session_commit_count, parser_consumable_case_count)
        && closure.verifier_policy == FRONTEND_MACRO_M26C_ROLLBACK_AST_VERIFIER_POLICY
        && closure.verifier_query_name == expected_query_name
        && closure.blocked_reason == FRONTEND_MACRO_M26C_ROLLBACK_AST_VERIFIER_BLOCKER
        && closure.cursor_snapshot_count == cursor_snapshot_count
        && closure.rollback_proof_count == rollback_proof_count
        && closure.recovery_shadow_count == recovery_shadow_count
        && closure.ast_baseline_snapshot_count == ast_baseline_snapshot_count
        && closure.ast_mutation_count == ast_mutation_count
        && closure.cursor_commit_count == cursor_commit_count
        && closure.session_commit_count == session_commit_count
        && closure.parser_consumable_case_count == parser_consumable_case_count
        && closure.dry_run_admission_gate_available
        && closure.recovery_shadow_available
        && closure.cursor_snapshot_proof_available
        && closure.dry_run_session_available
        && closure.diagnostic_shadow_closure_available
        && closure.ast_baseline_available
        && closure.rollback_execution_guard_available
        && closure.ast_mutation_verifier_complete
        && !closure.rollback_execution_enabled
        && !closure.recovery_execution_enabled
        && !closure.diagnostic_emission_enabled
        && !closure.dry_run_execution_admitted
        && !closure.dry_run_executed
        && !closure.session_committed
        && !closure.parser_cursor_advanced
        && !closure.parser_consumption_enabled
        && !closure.parser_admitted
        && !closure.generated_part_parsed
        && !closure.generated_part_merged
        && !closure.ast_mutated
        && !closure.sema_visible
        && !closure.emit_expanded_available
        && !closure.debug_trace_available
        && !closure.source_map_available
        && !closure.standard_library_required
        && !closure.runtime_required
        && !closure.external_process_required
        && !closure.produced_user_generated_code
        && closure.closure_visible
        && closure.query_reusable;
}

[[nodiscard]] bool builtin_derive_cursor_rollback_ast_mutation_verifier_closures_match_groups(
    const EarlyItemExpansionResult& result) noexcept
{
    if (result.builtin_derive_cursor_rollback_ast_mutation_verifier_closures.size()
            != result.generated_parts.size()
        || result.builtin_derive_parser_dry_run_admission_gates.size() != result.generated_parts.size()
        || result.builtin_derive_error_recovery_shadow_diagnostic_gates.size()
            != result.generated_parts.size()
        || result.builtin_derive_token_cursor_snapshot_proofs.size() != result.generated_parts.size()
        || result.builtin_derive_parser_dry_run_sessions.size() != result.generated_parts.size()
        || result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures.size()
            != result.generated_parts.size()) {
        return false;
    }
    for (base::usize index = 0;
         index < result.builtin_derive_cursor_rollback_ast_mutation_verifier_closures.size();
         ++index) {
        if (!builtin_derive_cursor_rollback_ast_mutation_verifier_closure_matches_group(
                result.builtin_derive_cursor_rollback_ast_mutation_verifier_closures[index],
                result.generated_parts[index],
                result.builtin_derive_parser_dry_run_admission_gates[index],
                result.builtin_derive_error_recovery_shadow_diagnostic_gates[index],
                result.builtin_derive_token_cursor_snapshot_proofs[index],
                result.builtin_derive_parser_dry_run_sessions[index],
                result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures[index])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool generated_part_stubs_match_placeholders(
    const std::vector<GeneratedModulePartPlaceholder>& generated_parts,
    const std::vector<GeneratedModulePartParseMergeStub>& generated_part_stubs) noexcept
{
    if (generated_parts.size() != generated_part_stubs.size()) {
        return false;
    }
    for (base::usize index = 0; index < generated_parts.size(); ++index) {
        if (!stub_matches_placeholder(generated_part_stubs[index], generated_parts[index])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool aurex_macro_hygiene_gates_match_surfaces(
    const EarlyItemExpansionResult& result) noexcept
{
    if (result.aurex_macro_definition_site_hygiene_gates.size()
        != result.aurex_macro_surface_admission_gates.size()) {
        return false;
    }
    for (const AurexMacroSurfaceAdmissionGate& surface : result.aurex_macro_surface_admission_gates) {
        const auto found = std::find_if(result.aurex_macro_definition_site_hygiene_gates.begin(),
            result.aurex_macro_definition_site_hygiene_gates.end(),
            [&surface](const AurexMacroDefinitionSiteHygieneAdmissionGate& gate) {
                return gate.item.value == surface.item.value
                    && gate.module.value == surface.module.value
                    && gate.part_index == surface.part_index
                    && gate.macro_name == surface.macro_name
                    && gate.surface_admission_identity == surface.admission_identity
                    && gate.body_fingerprint == surface.body_fingerprint;
            });
        if (found == result.aurex_macro_definition_site_hygiene_gates.end()) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool aurex_macro_typed_matcher_gates_match_surfaces(
    const EarlyItemExpansionResult& result) noexcept
{
    base::u64 expected_matchers = 0U;
    for (const AurexMacroSurfaceAdmissionGate& surface : result.aurex_macro_surface_admission_gates) {
        expected_matchers += surface.match_clause_count;
    }
    if (expected_matchers
        != static_cast<base::u64>(result.aurex_macro_typed_matcher_admission_gates.size())) {
        return false;
    }
    for (const AurexMacroTypedMatcherAdmissionGate& matcher :
        result.aurex_macro_typed_matcher_admission_gates) {
        const auto surface = std::find_if(result.aurex_macro_surface_admission_gates.begin(),
            result.aurex_macro_surface_admission_gates.end(),
            [&matcher](const AurexMacroSurfaceAdmissionGate& gate) {
                return gate.item.value == matcher.item.value
                    && gate.module.value == matcher.module.value
                    && gate.part_index == matcher.part_index
                    && gate.macro_name == matcher.macro_name
                    && gate.admission_identity == matcher.surface_admission_identity
                    && gate.body_fingerprint == matcher.body_fingerprint;
            });
        if (surface == result.aurex_macro_surface_admission_gates.end()) {
            return false;
        }
        const auto hygiene = std::find_if(result.aurex_macro_definition_site_hygiene_gates.begin(),
            result.aurex_macro_definition_site_hygiene_gates.end(),
            [&matcher](const AurexMacroDefinitionSiteHygieneAdmissionGate& gate) {
                return gate.item.value == matcher.item.value
                    && gate.module.value == matcher.module.value
                    && gate.part_index == matcher.part_index
                    && gate.macro_name == matcher.macro_name
                    && gate.hygiene_identity == matcher.definition_site_hygiene_identity;
            });
        if (hygiene == result.aurex_macro_definition_site_hygiene_gates.end()) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool per_input_stubs_match_inputs(const EarlyItemExpansionResult& result) noexcept
{
    if (result.inputs.size() != result.source_maps.size()
        || result.inputs.size() != result.hygiene_stubs.size()
        || result.inputs.size() != result.trace_stubs.size()
        || result.inputs.size() != result.generated_item_declarations.size()
        || result.inputs.size() != result.declared_generated_names.size()
        || result.inputs.size() != result.token_materialization_admissions.size()
        || result.inputs.size() != result.generated_token_buffers.size()
        || result.inputs.size() != result.parser_admission_gates.size()
        || result.inputs.size() != result.parser_admission_diagnostics.size()) {
        return false;
    }
    for (base::usize index = 0; index < result.inputs.size(); ++index) {
        const EarlyItemMacroInput& input = result.inputs[index];
        const GeneratedModulePartPlaceholder* const placeholder =
            find_generated_part_for(result.generated_parts, input.module, input.part_index);
        if (placeholder == nullptr) {
            return false;
        }
        const GeneratedModulePartParseMergeStub* const parse_merge_stub =
            find_parse_merge_stub_for(result.generated_part_stubs, input.module, input.part_index);
        if (parse_merge_stub == nullptr) {
            return false;
        }
        if (!source_map_matches_input(result.source_maps[index], input)
            || !hygiene_stub_matches_input(result.hygiene_stubs[index], input)
            || !trace_stub_matches_input(result.trace_stubs[index], input)
            || !generated_item_declaration_matches_input(result.generated_item_declarations[index],
                input, *placeholder, result.hygiene_stubs[index])
            || !declared_generated_name_matches_input(result.declared_generated_names[index],
                input, *placeholder, result.hygiene_stubs[index], result.generated_item_declarations[index])
            || !token_materialization_admission_matches_input(result.token_materialization_admissions[index],
                input, *placeholder, result.hygiene_stubs[index], result.trace_stubs[index],
                result.generated_item_declarations[index], result.declared_generated_names[index])
            || !generated_token_buffer_matches_input(result.generated_token_buffers[index],
                input, *placeholder, result.hygiene_stubs[index], result.trace_stubs[index],
                result.token_materialization_admissions[index])
            || !parser_admission_gate_matches_input(result.parser_admission_gates[index],
                input, *placeholder, *parse_merge_stub, result.generated_token_buffers[index])
            || !parser_admission_diagnostic_matches_input(result.parser_admission_diagnostics[index],
                input, *placeholder, *parse_merge_stub, result.trace_stubs[index],
                result.generated_token_buffers[index], result.parser_admission_gates[index])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] GeneratedModulePartPlaceholder make_generated_part_placeholder(
    const query::ModulePartKey source_part, const syntax::ModuleId module, const base::u32 source_part_index)
{
    const base::u32 generated_stable_index = base::checked_u32(
        base::checked_add_usize(source_part_index, FRONTEND_MACRO_M21D_GENERATED_PART_INDEX_OFFSET,
            "early item macro generated part stable index"),
        "early item macro generated part stable index");
    const query::ModulePartKey generated =
        generated_module_part_key(source_part, module, generated_stable_index);
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M21D_GENERATED_PART_PLACEHOLDER_MARKER);
    builder.mix_fingerprint(query::stable_key_fingerprint(source_part));
    builder.mix_fingerprint(query::stable_key_fingerprint(generated));
    builder.mix_u32(module.value);
    builder.mix_u32(source_part_index);
    return GeneratedModulePartPlaceholder{
        module,
        source_part_index,
        generated_stable_index,
        query::SourceRole::generated,
        query::ModulePartKind::generated,
        source_part,
        generated,
        builder.finish(),
        false,
        false,
        false,
    };
}

[[nodiscard]] ExpansionSourceMapPlaceholder make_source_map_placeholder(
    const EarlyItemMacroInput& input) noexcept
{
    return ExpansionSourceMapPlaceholder{
        input.item,
        input.module,
        input.attribute_index,
        input.attribute_range,
        input.token_tree_range,
        input.query_key_fingerprint,
        false,
        false,
    };
}

[[nodiscard]] base::Result<query::ModulePartKey> module_part_key_for_item(const syntax::AstModule& ast,
    const std::span<const std::vector<query::ModulePartKey>> module_part_keys, const syntax::ItemId item)
{
    if (!item_id_in_range(ast, item)) {
        return base::Result<query::ModulePartKey>::fail(internal_error(FRONTEND_MACRO_M21D_ITEM_MODULES_MISMATCH));
    }
    const syntax::ModuleId module = ast.item_modules[item.value];
    const base::u32 part_index = ast.item_part_indices[item.value];
    if (!module_id_in_range(ast, module) || module.value >= module_part_keys.size()
        || part_index >= module_part_keys[module.value].size()
        || !query::is_valid(module_part_keys[module.value][part_index])) {
        return base::Result<query::ModulePartKey>::fail(internal_error(FRONTEND_MACRO_M21D_MISSING_MODULE_PART_KEY));
    }
    return base::Result<query::ModulePartKey>::ok(module_part_keys[module.value][part_index]);
}

[[nodiscard]] EarlyItemMacroInput make_macro_input(const syntax::AstModule& ast,
    const syntax::ItemId item,
    const base::u32 attribute_index,
    const syntax::AttributeDecl& attribute,
    const query::ModulePartKey attached_part) noexcept
{
    const query::StableFingerprint128 token_tree_fingerprint = fingerprint_attribute_token_tree(attribute);
    const syntax::ModuleId module = ast.item_modules[item.value];
    const base::u32 part_index = ast.item_part_indices[item.value];
    const query::StableFingerprint128 query_key_fingerprint = fingerprint_early_item_query_key(
        item, module, part_index, attribute_index, attribute, attached_part, token_tree_fingerprint);
    return EarlyItemMacroInput{
        item,
        module,
        part_index,
        attribute_index,
        std::string(attribute.name),
        attribute.range,
        attribute.token_tree_range,
        attribute.has_token_tree,
        static_cast<base::u64>(attribute.token_tree.size()),
        attached_part,
        token_tree_fingerprint,
        query_key_fingerprint,
        disposition_for_attribute(attribute),
    };
}

[[nodiscard]] AurexMacroSurfaceAdmissionGate make_aurex_macro_surface_admission_gate(
    const syntax::AstModule& ast,
    const syntax::ItemId item,
    const query::ModulePartKey attached_part)
{
    const syntax::ItemNode& macro_item = ast.items[item.value];
    const syntax::ModuleId module = ast.item_modules[item.value];
    const base::u32 part_index = ast.item_part_indices[item.value];
    const query::StableFingerprint128 body_fingerprint = fingerprint_macro_body_tokens(macro_item);
    const std::string query_name = macro_surface_query_name(module, part_index, item, macro_item.name);

    AurexMacroSurfaceAdmissionGate gate;
    gate.item = item;
    gate.module = module;
    gate.part_index = part_index;
    gate.attached_part = attached_part;
    gate.body_fingerprint = body_fingerprint;
    gate.admission_identity = macro_surface_admission_identity(
        item, module, part_index, attached_part, macro_item, body_fingerprint, query_name);
    gate.macro_kind = macro_item.macro_kind;
    gate.macro_name = std::string(macro_item.name);
    gate.admission_policy = std::string(FRONTEND_MACRO_M27A_AUREX_MACRO_SURFACE_POLICY);
    gate.query_name = query_name;
    gate.blocker_reason = std::string(macro_surface_blocker_reason(macro_item.macro_kind));
    gate.macro_range = macro_item.range;
    gate.body_range = macro_item.macro_body_range;
    gate.body_token_count = static_cast<base::u64>(macro_item.macro_body_tokens.size());
    gate.match_clause_count = macro_item.macro_match_clause_count;
    gate.body_balanced = macro_item.macro_body_balanced;
    gate.declarative_surface = macro_item.macro_kind == syntax::MacroDeclKind::declarative;
    gate.user_derive_surface = macro_item.macro_kind == syntax::MacroDeclKind::derive;
    gate.compile_time_execution_surface = macro_item.macro_kind == syntax::MacroDeclKind::compile_time;
    return gate;
}

[[nodiscard]] AurexMacroDefinitionSiteHygieneAdmissionGate
make_aurex_macro_definition_site_hygiene_gate(const AurexMacroSurfaceAdmissionGate& surface)
{
    const std::string query_name =
        macro_definition_site_hygiene_query_name(surface.module, surface.part_index, surface.item,
            surface.macro_name);
    AurexMacroDefinitionSiteHygieneAdmissionGate gate;
    gate.item = surface.item;
    gate.module = surface.module;
    gate.part_index = surface.part_index;
    gate.attached_part = surface.attached_part;
    gate.surface_admission_identity = surface.admission_identity;
    gate.body_fingerprint = surface.body_fingerprint;
    gate.definition_site_mark = definition_site_mark(surface);
    gate.fresh_name_scope = fresh_name_scope(surface);
    gate.diagnostic_anchor_identity = diagnostic_anchor_identity(surface, surface.body_range);
    gate.macro_kind = surface.macro_kind;
    gate.macro_name = surface.macro_name;
    gate.hygiene_policy = std::string(FRONTEND_MACRO_M27B_DEFINITION_SITE_HYGIENE_POLICY);
    gate.query_name = query_name;
    gate.blocker_reason = std::string(FRONTEND_MACRO_M27B_DEFINITION_SITE_HYGIENE_BLOCKER);
    gate.macro_range = surface.macro_range;
    gate.body_range = surface.body_range;
    gate.hygiene_identity = macro_definition_site_hygiene_identity(gate);
    return gate;
}

[[nodiscard]] AurexMacroTypedMatcherAdmissionGate make_aurex_macro_typed_matcher_gate(
    const AurexMacroSurfaceAdmissionGate& surface,
    const AurexMacroDefinitionSiteHygieneAdmissionGate& hygiene,
    const TypedMatcherCandidate& candidate)
{
    const std::string query_name = macro_typed_matcher_query_name(surface.module, surface.part_index,
        surface.item, candidate.matcher_index, surface.macro_name);
    AurexMacroTypedMatcherAdmissionGate gate;
    gate.item = surface.item;
    gate.module = surface.module;
    gate.part_index = surface.part_index;
    gate.matcher_index = candidate.matcher_index;
    gate.attached_part = surface.attached_part;
    gate.surface_admission_identity = surface.admission_identity;
    gate.body_fingerprint = surface.body_fingerprint;
    gate.definition_site_hygiene_identity = hygiene.hygiene_identity;
    gate.diagnostic_anchor_identity = diagnostic_anchor_identity(surface, candidate.matcher_range);
    gate.macro_kind = surface.macro_kind;
    gate.matcher_kind = candidate.matcher_kind;
    gate.macro_name = surface.macro_name;
    gate.matcher_head = candidate.matcher_head;
    gate.binding_name = candidate.binding_name;
    gate.matcher_policy = std::string(FRONTEND_MACRO_M27B_TYPED_MATCHER_POLICY);
    gate.query_name = query_name;
    gate.blocker_reason = std::string(candidate.matcher_shape_recognized
            ? FRONTEND_MACRO_M27B_TYPED_MATCHER_BLOCKER
            : FRONTEND_MACRO_M27B_UNKNOWN_MATCHER_BLOCKER);
    gate.matcher_range = candidate.matcher_range;
    gate.output_range = candidate.output_range;
    gate.matcher_shape_recognized = candidate.matcher_shape_recognized;
    gate.expr_list_matcher = candidate.matcher_kind == AurexMacroTypedMatcherKind::expr_list;
    gate.item_matcher = candidate.matcher_kind == AurexMacroTypedMatcherKind::item;
    gate.token_stream_matcher = candidate.matcher_kind == AurexMacroTypedMatcherKind::tokens;
    gate.unknown_matcher = candidate.matcher_kind == AurexMacroTypedMatcherKind::unknown;
    gate.matcher_fingerprint = matcher_fingerprint(gate);
    gate.matcher_identity = macro_typed_matcher_identity(gate);
    return gate;
}

} // namespace

std::string_view early_item_expansion_disposition_name(
    const EarlyItemExpansionDisposition disposition) noexcept
{
    switch (disposition) {
        case EarlyItemExpansionDisposition::builtin_derive_passthrough:
            return "builtin_derive_passthrough";
        case EarlyItemExpansionDisposition::blocked_unimplemented_attribute:
            return "blocked_unimplemented_attribute";
    }
    return "invalid";
}

std::string_view generated_module_part_lifecycle_state_name(
    const GeneratedModulePartLifecycleState state) noexcept
{
    switch (state) {
        case GeneratedModulePartLifecycleState::planned:
            return "planned";
        case GeneratedModulePartLifecycleState::materialized_buffer_stub:
            return "materialized_buffer_stub";
        case GeneratedModulePartLifecycleState::parse_blocked:
            return "parse_blocked";
        case GeneratedModulePartLifecycleState::merge_blocked:
            return "merge_blocked";
    }
    return "invalid";
}

bool is_valid(const EarlyItemExpansionDisposition disposition) noexcept
{
    switch (disposition) {
        case EarlyItemExpansionDisposition::builtin_derive_passthrough:
        case EarlyItemExpansionDisposition::blocked_unimplemented_attribute:
            return true;
    }
    return false;
}

bool is_valid(const GeneratedModulePartLifecycleState state) noexcept
{
    switch (state) {
        case GeneratedModulePartLifecycleState::planned:
        case GeneratedModulePartLifecycleState::materialized_buffer_stub:
        case GeneratedModulePartLifecycleState::parse_blocked:
        case GeneratedModulePartLifecycleState::merge_blocked:
            return true;
    }
    return false;
}

bool is_valid(const EarlyItemMacroInput& input) noexcept
{
    return syntax::is_valid(input.item)
        && syntax::is_valid(input.module)
        && !input.attribute_name.empty()
        && source_range_is_well_formed(input.attribute_range)
        && source_range_is_well_formed(input.token_tree_range)
        && query::is_valid(input.attached_part)
        && input.token_tree_fingerprint.byte_count > 0
        && input.query_key_fingerprint.byte_count > 0
        && is_valid(input.disposition);
}

bool is_valid(const GeneratedModulePartPlaceholder& placeholder) noexcept
{
    return syntax::is_valid(placeholder.module)
        && placeholder.source_role == query::SourceRole::generated
        && placeholder.part_kind == query::ModulePartKind::generated
        && query::is_valid(placeholder.source_part)
        && query::is_valid(placeholder.generated_part)
        && placeholder.generated_part.kind == query::ModulePartKind::generated
        && placeholder.generated_part.file.role == query::SourceRole::generated
        && placeholder.output_fingerprint.byte_count > 0
        && !placeholder.parsed
        && !placeholder.merged
        && !placeholder.produced_user_generated_code;
}

bool is_valid(const GeneratedModulePartParseMergeStub& stub) noexcept
{
    return syntax::is_valid(stub.module)
        && query::is_valid(stub.source_part)
        && query::is_valid(stub.generated_part)
        && stub.generated_part.kind == query::ModulePartKind::generated
        && stub.generated_part.file.role == query::SourceRole::generated
        && is_nonzero_fingerprint(stub.generated_buffer_identity)
        && is_nonzero_fingerprint(stub.parse_config_fingerprint)
        && is_nonzero_fingerprint(stub.merge_ordering_key)
        && is_nonzero_fingerprint(stub.expansion_origin)
        && !stub.generated_buffer_name.empty()
        && !stub.blocker_reason.empty()
        && is_valid(stub.lifecycle_state)
        && stub.lifecycle_state == GeneratedModulePartLifecycleState::merge_blocked
        && stub.materialized_buffer
        && !stub.parsed
        && !stub.merged
        && !stub.sema_visible
        && !stub.produced_user_generated_code;
}

bool is_valid(const ExpansionSourceMapPlaceholder& placeholder) noexcept
{
    return syntax::is_valid(placeholder.item)
        && syntax::is_valid(placeholder.module)
        && source_range_is_well_formed(placeholder.attribute_range)
        && source_range_is_well_formed(placeholder.token_tree_range)
        && placeholder.expansion_origin.byte_count > 0
        && !placeholder.real_source_map
        && !placeholder.debug_trace_available;
}

bool is_valid(const ExpansionHygieneStub& stub) noexcept
{
    return syntax::is_valid(stub.item)
        && syntax::is_valid(stub.module)
        && query::is_valid(stub.attached_part)
        && is_nonzero_fingerprint(stub.expansion_origin)
        && is_nonzero_fingerprint(stub.call_site_mark)
        && is_nonzero_fingerprint(stub.definition_site_mark)
        && is_nonzero_fingerprint(stub.generated_fresh_mark)
        && is_nonzero_fingerprint(stub.declared_name_set)
        && stub.policy == FRONTEND_MACRO_M21F_HYGIENE_POLICY
        && !stub.resolved
        && !stub.declared_names_visible
        && !stub.captures_call_site_locals;
}

bool is_valid(const ExpansionTraceStub& stub) noexcept
{
    return syntax::is_valid(stub.item)
        && syntax::is_valid(stub.module)
        && query::is_valid(stub.attached_part)
        && source_range_is_well_formed(stub.attribute_range)
        && source_range_is_well_formed(stub.token_tree_range)
        && is_nonzero_fingerprint(stub.expansion_origin)
        && is_nonzero_fingerprint(stub.trace_identity)
        && is_nonzero_fingerprint(stub.generated_source_map_identity)
        && is_nonzero_fingerprint(stub.diagnostic_anchor)
        && stub.trace_policy == FRONTEND_MACRO_M21F_TRACE_POLICY
        && stub.blocker_reason == FRONTEND_MACRO_M21F_TRACE_BLOCKER
        && !stub.real_source_map
        && !stub.debug_trace_available
        && !stub.cli_emit_expanded_available;
}

bool is_valid(const GeneratedItemDeclarationStub& stub) noexcept
{
    return syntax::is_valid(stub.item)
        && syntax::is_valid(stub.module)
        && query::is_valid(stub.attached_part)
        && query::is_valid(stub.generated_part)
        && stub.generated_part.kind == query::ModulePartKind::generated
        && stub.generated_part.file.role == query::SourceRole::generated
        && is_nonzero_fingerprint(stub.expansion_origin)
        && is_nonzero_fingerprint(stub.declaration_identity)
        && is_nonzero_fingerprint(stub.declared_name_set)
        && is_nonzero_fingerprint(stub.generated_item_key)
        && stub.declaration_identity != stub.generated_item_key
        && stub.declaration_role == FRONTEND_MACRO_M21G_DECLARATION_ROLE
        && !stub.generated_item_name.empty()
        && stub.blocker_reason == FRONTEND_MACRO_M21G_DECLARATION_BLOCKER
        && stub.planned
        && !stub.materialized_tokens
        && !stub.parsed
        && !stub.merged
        && !stub.sema_visible
        && !stub.produced_user_generated_code;
}

bool is_valid(const DeclaredGeneratedNameStub& stub) noexcept
{
    return syntax::is_valid(stub.item)
        && syntax::is_valid(stub.module)
        && query::is_valid(stub.attached_part)
        && query::is_valid(stub.generated_part)
        && stub.generated_part.kind == query::ModulePartKind::generated
        && stub.generated_part.file.role == query::SourceRole::generated
        && is_nonzero_fingerprint(stub.expansion_origin)
        && is_nonzero_fingerprint(stub.declared_name_set)
        && is_nonzero_fingerprint(stub.declared_name_identity)
        && is_nonzero_fingerprint(stub.hygiene_mark)
        && !stub.declared_name.empty()
        && stub.namespace_kind == FRONTEND_MACRO_M21G_DECLARED_NAME_NAMESPACE
        && stub.blocker_reason == FRONTEND_MACRO_M21G_DECLARED_NAME_BLOCKER
        && !stub.lookup_visible
        && !stub.export_visible
        && !stub.sema_visible
        && !stub.produced_user_generated_code;
}

bool is_valid(const TokenMaterializationAdmissionStub& stub) noexcept
{
    return syntax::is_valid(stub.item)
        && syntax::is_valid(stub.module)
        && query::is_valid(stub.attached_part)
        && query::is_valid(stub.generated_part)
        && stub.generated_part.kind == query::ModulePartKind::generated
        && stub.generated_part.file.role == query::SourceRole::generated
        && is_nonzero_fingerprint(stub.expansion_origin)
        && is_nonzero_fingerprint(stub.declaration_identity)
        && is_nonzero_fingerprint(stub.generated_item_key)
        && is_nonzero_fingerprint(stub.declared_name_set)
        && is_nonzero_fingerprint(stub.declared_name_identity)
        && is_nonzero_fingerprint(stub.hygiene_mark)
        && is_nonzero_fingerprint(stub.source_map_identity)
        && is_nonzero_fingerprint(stub.trace_identity)
        && is_nonzero_fingerprint(stub.token_plan_identity)
        && is_nonzero_fingerprint(stub.token_buffer_identity)
        && stub.token_plan_identity != stub.token_buffer_identity
        && stub.admission_policy == FRONTEND_MACRO_M21H_ADMISSION_POLICY
        && !stub.token_stream_name.empty()
        && token_materialization_admission_state_is_valid(stub)
        && stub.compiler_owned
        && stub.admitted
        && !stub.generated_source_text
        && !stub.parse_ready
        && !stub.external_process_required
        && !stub.standard_library_required
        && !stub.runtime_required
        && !stub.produced_user_generated_code;
}

bool is_valid(const GeneratedTokenBufferStub& stub) noexcept
{
    return syntax::is_valid(stub.item)
        && syntax::is_valid(stub.module)
        && query::is_valid(stub.attached_part)
        && query::is_valid(stub.generated_part)
        && stub.generated_part.kind == query::ModulePartKind::generated
        && stub.generated_part.file.role == query::SourceRole::generated
        && is_nonzero_fingerprint(stub.token_plan_identity)
        && is_nonzero_fingerprint(stub.token_buffer_identity)
        && is_nonzero_fingerprint(stub.materialization_identity)
        && is_nonzero_fingerprint(stub.source_map_identity)
        && is_nonzero_fingerprint(stub.hygiene_mark)
        && stub.token_plan_identity != stub.token_buffer_identity
        && stub.materialization_identity != stub.token_buffer_identity
        && !stub.token_stream_name.empty()
        && token_buffer_kind_is_compiler_owned(stub.token_buffer_kind)
        && token_producer_policy_is_compiler_owned(stub.token_producer_policy)
        && generated_token_buffer_state_is_valid(stub)
        && !stub.generated_source_text
        && !stub.parser_consumable
        && !stub.produced_user_generated_code;
}

bool is_valid(const GeneratedTokenRecord& record) noexcept
{
    return syntax::is_valid(record.item)
        && syntax::is_valid(record.module)
        && is_nonzero_fingerprint(record.token_buffer_identity)
        && is_nonzero_fingerprint(record.token_identity)
        && is_nonzero_fingerprint(record.source_map_identity)
        && is_nonzero_fingerprint(record.hygiene_mark)
        && record.kind != syntax::TokenKind::invalid
        && record.kind != syntax::TokenKind::eof
        && !record.text.empty()
        && !record.token_role.empty()
        && source_range_is_well_formed(record.anchor_range)
        && record.compiler_owned
        && !record.parser_visible
        && !record.produced_user_generated_code;
}

bool is_valid(const GeneratedTokenParserAdmissionGateStub& stub) noexcept
{
    return syntax::is_valid(stub.item)
        && syntax::is_valid(stub.module)
        && query::is_valid(stub.attached_part)
        && query::is_valid(stub.generated_part)
        && stub.generated_part.kind == query::ModulePartKind::generated
        && stub.generated_part.file.role == query::SourceRole::generated
        && is_nonzero_fingerprint(stub.token_plan_identity)
        && is_nonzero_fingerprint(stub.token_buffer_identity)
        && is_nonzero_fingerprint(stub.materialization_identity)
        && is_nonzero_fingerprint(stub.source_map_identity)
        && is_nonzero_fingerprint(stub.hygiene_mark)
        && is_nonzero_fingerprint(stub.generated_buffer_identity)
        && is_nonzero_fingerprint(stub.parse_config_fingerprint)
        && is_nonzero_fingerprint(stub.parse_gate_identity)
        && stub.token_plan_identity != stub.token_buffer_identity
        && stub.materialization_identity != stub.token_buffer_identity
        && stub.parse_gate_identity != stub.token_buffer_identity
        && !stub.token_stream_name.empty()
        && stub.parser_gate_policy == FRONTEND_MACRO_M21J_PARSER_GATE_POLICY
        && (stub.blocker_reason == FRONTEND_MACRO_M21J_DERIVE_PARSE_BLOCKER
            || stub.blocker_reason == FRONTEND_MACRO_M21J_EMPTY_PARSE_BLOCKER)
        && stub.compiler_owned
        && stub.token_buffer_materialized == (stub.token_count > 0)
        && stub.token_records_available == (stub.token_count > 0)
        && !stub.parser_admitted
        && !stub.parse_ready
        && !stub.parser_consumable
        && !stub.generated_source_text
        && !stub.generated_part_parsed
        && !stub.generated_part_merged
        && !stub.sema_visible
        && !stub.produced_user_generated_code;
}

bool is_valid(const ParserAdmissionDiagnosticProjectionStub& stub) noexcept
{
    return syntax::is_valid(stub.item)
        && syntax::is_valid(stub.module)
        && query::is_valid(stub.attached_part)
        && query::is_valid(stub.generated_part)
        && stub.generated_part.kind == query::ModulePartKind::generated
        && stub.generated_part.file.role == query::SourceRole::generated
        && source_range_is_well_formed(stub.primary_anchor)
        && source_range_is_well_formed(stub.token_tree_anchor)
        && is_nonzero_fingerprint(stub.parse_gate_identity)
        && is_nonzero_fingerprint(stub.diagnostic_identity)
        && is_nonzero_fingerprint(stub.diagnostic_anchor_identity)
        && is_nonzero_fingerprint(stub.token_plan_identity)
        && is_nonzero_fingerprint(stub.token_buffer_identity)
        && is_nonzero_fingerprint(stub.materialization_identity)
        && is_nonzero_fingerprint(stub.generated_buffer_identity)
        && is_nonzero_fingerprint(stub.parse_config_fingerprint)
        && is_nonzero_fingerprint(stub.source_map_identity)
        && is_nonzero_fingerprint(stub.hygiene_mark)
        && is_nonzero_fingerprint(stub.trace_identity)
        && stub.token_plan_identity != stub.token_buffer_identity
        && stub.materialization_identity != stub.token_buffer_identity
        && stub.parse_gate_identity != stub.token_buffer_identity
        && stub.diagnostic_identity != stub.parse_gate_identity
        && stub.diagnostic_anchor_identity != stub.diagnostic_identity
        && stub.diagnostic_policy == FRONTEND_MACRO_M21K_DIAGNOSTIC_POLICY
        && parser_admission_diagnostic_category_is_valid(stub)
        && stub.generated_part_parse_blocker == FRONTEND_MACRO_M21K_GENERATED_PART_PARSE_BLOCKER
        && !stub.debug_projection_name.empty()
        && !stub.parser_admitted
        && !stub.parse_ready
        && !stub.parser_consumable
        && !stub.generated_part_parsed
        && !stub.generated_part_merged
        && !stub.emit_expanded_available
        && !stub.debug_trace_available
        && !stub.source_map_available
        && !stub.produced_user_generated_code;
}

bool is_valid(const ParserAdmissionDiagnosticReportEntry& entry) noexcept
{
    return syntax::is_valid(entry.item)
        && syntax::is_valid(entry.module)
        && query::is_valid(entry.attached_part)
        && query::is_valid(entry.generated_part)
        && entry.generated_part.kind == query::ModulePartKind::generated
        && entry.generated_part.file.role == query::SourceRole::generated
        && source_range_is_well_formed(entry.primary_anchor)
        && source_range_is_well_formed(entry.token_tree_anchor)
        && is_nonzero_fingerprint(entry.diagnostic_identity)
        && is_nonzero_fingerprint(entry.diagnostic_anchor_identity)
        && is_nonzero_fingerprint(entry.report_entry_identity)
        && is_nonzero_fingerprint(entry.parse_gate_identity)
        && entry.diagnostic_identity != entry.parse_gate_identity
        && entry.report_entry_identity != entry.diagnostic_identity
        && entry.diagnostic_anchor_identity != entry.diagnostic_identity
        && parser_admission_report_entry_category_is_valid(entry)
        && !entry.debug_projection_name.empty()
        && !entry.query_projection_name.empty()
        && !entry.parser_admitted
        && entry.report_visible
        && entry.query_reusable
        && !entry.parser_consumable
        && !entry.emit_expanded_available
        && !entry.produced_user_generated_code;
}

bool is_valid(const ParserAdmissionDiagnosticReport& report) noexcept
{
    return syntax::is_valid(report.module)
        && query::is_valid(report.attached_part)
        && query::is_valid(report.generated_part)
        && report.generated_part.kind == query::ModulePartKind::generated
        && report.generated_part.file.role == query::SourceRole::generated
        && is_nonzero_fingerprint(report.report_identity)
        && is_nonzero_fingerprint(report.report_anchor_identity)
        && is_nonzero_fingerprint(report.report_grouping_identity)
        && is_nonzero_fingerprint(report.parse_config_fingerprint)
        && is_nonzero_fingerprint(report.generated_buffer_identity)
        && report.report_identity != report.report_anchor_identity
        && report.report_identity != report.report_grouping_identity
        && report.report_policy == FRONTEND_MACRO_M21L_REPORT_POLICY
        && !report.report_query_name.empty()
        && report.blocked_reason == FRONTEND_MACRO_M21L_REPORT_BLOCKER
        && report.entry_count == report.blocked_entry_count
        && report.entry_count == report.derive_entry_count + report.empty_entry_count
        && report.token_record_available_entry_count <= report.entry_count
        && report.query_reusable
        && report.report_visible
        && report.source_anchor_ordered
        && !report.parser_admitted
        && !report.parse_ready
        && !report.parser_consumable
        && !report.emit_expanded_available
        && !report.debug_trace_available
        && !report.source_map_available
        && !report.produced_user_generated_code;
}

bool is_valid(const GeneratedTokenParserReadinessPreflightEntry& entry) noexcept
{
    return syntax::is_valid(entry.item)
        && syntax::is_valid(entry.module)
        && query::is_valid(entry.attached_part)
        && query::is_valid(entry.generated_part)
        && entry.generated_part.kind == query::ModulePartKind::generated
        && entry.generated_part.file.role == query::SourceRole::generated
        && is_nonzero_fingerprint(entry.token_plan_identity)
        && is_nonzero_fingerprint(entry.token_buffer_identity)
        && is_nonzero_fingerprint(entry.materialization_identity)
        && is_nonzero_fingerprint(entry.generated_buffer_identity)
        && is_nonzero_fingerprint(entry.parse_config_fingerprint)
        && is_nonzero_fingerprint(entry.parse_gate_identity)
        && is_nonzero_fingerprint(entry.diagnostic_identity)
        && is_nonzero_fingerprint(entry.diagnostic_anchor_identity)
        && is_nonzero_fingerprint(entry.report_entry_identity)
        && is_nonzero_fingerprint(entry.source_map_identity)
        && is_nonzero_fingerprint(entry.hygiene_mark)
        && is_nonzero_fingerprint(entry.trace_identity)
        && is_nonzero_fingerprint(entry.preflight_identity)
        && entry.preflight_identity != entry.parse_gate_identity
        && entry.preflight_identity != entry.diagnostic_identity
        && !entry.token_stream_name.empty()
        && parser_readiness_preflight_category_is_valid(entry)
        && entry.delimiter_balance_state == FRONTEND_MACRO_M21M_DELIMITER_BALANCED_STATE
        && entry.source_anchor_coverage_state == FRONTEND_MACRO_M21M_SOURCE_ANCHOR_COVERED_STATE
        && entry.readiness_policy == FRONTEND_MACRO_M21M_PREFLIGHT_POLICY
        && entry.blocker_reason == FRONTEND_MACRO_M21M_PREFLIGHT_BLOCKER
        && entry.token_indices_contiguous
        && entry.delimiter_balanced
        && entry.source_anchors_covered
        && entry.parse_config_compatible
        && entry.hygiene_prerequisite_available
        && entry.source_map_prerequisite_available
        && entry.diagnostic_projection_available
        && !entry.parser_admitted
        && !entry.parse_ready
        && !entry.parser_consumable
        && !entry.generated_part_parsed
        && !entry.generated_part_merged
        && !entry.produced_user_generated_code;
}

bool is_valid(const GeneratedTokenParserConsumptionContractGate& gate) noexcept
{
    return syntax::is_valid(gate.module)
        && query::is_valid(gate.attached_part)
        && query::is_valid(gate.generated_part)
        && gate.generated_part.kind == query::ModulePartKind::generated
        && gate.generated_part.file.role == query::SourceRole::generated
        && is_nonzero_fingerprint(gate.generated_buffer_identity)
        && is_nonzero_fingerprint(gate.parse_config_fingerprint)
        && is_nonzero_fingerprint(gate.report_identity)
        && is_nonzero_fingerprint(gate.contract_identity)
        && is_nonzero_fingerprint(gate.contract_grouping_identity)
        && is_nonzero_fingerprint(gate.contract_anchor_identity)
        && gate.contract_identity != gate.contract_grouping_identity
        && gate.contract_identity != gate.contract_anchor_identity
        && gate.contract_policy == FRONTEND_MACRO_M21N_CONTRACT_POLICY
        && !gate.contract_query_name.empty()
        && gate.blocked_reason == FRONTEND_MACRO_M21N_CONTRACT_BLOCKER
        && gate.preflight_entry_count == gate.blocked_entry_count
        && gate.preflight_entry_count == gate.derive_entry_count + gate.empty_entry_count
        && gate.preflight_entry_count == gate.contiguous_index_entry_count
        && gate.preflight_entry_count == gate.delimiter_balanced_entry_count
        && gate.preflight_entry_count == gate.source_anchor_covered_entry_count
        && gate.preflight_entry_count == gate.parse_config_compatible_entry_count
        && gate.preflight_entry_count == gate.diagnostic_projection_entry_count
        && gate.query_reusable
        && gate.contract_visible
        && gate.all_entries_structurally_checked
        && !gate.parser_admitted
        && !gate.parse_ready
        && !gate.parser_consumable
        && !gate.generated_part_parsed
        && !gate.generated_part_merged
        && !gate.sema_visible
        && !gate.emit_expanded_available
        && !gate.debug_trace_available
        && !gate.source_map_available
        && !gate.produced_user_generated_code;
}

bool is_valid(const MacroExpansionBoundaryClosureReport& report) noexcept
{
    return is_nonzero_fingerprint(report.closure_identity)
        && is_nonzero_fingerprint(report.closure_grouping_identity)
        && report.closure_identity != report.closure_grouping_identity
        && report.closure_policy == FRONTEND_MACRO_M21O_CLOSURE_POLICY
        && report.closure_query_name == FRONTEND_MACRO_M21O_CLOSURE_QUERY_NAME
        && report.blocked_reason == FRONTEND_MACRO_M21O_CLOSURE_BLOCKER
        && report.parser_consumable_contract_gate_count == 0U
        && report.m21m_preflight_available
        && report.m21n_contract_available
        && report.release_closure_complete
        && report.query_reusable
        && report.closure_visible
        && !report.parser_consumption_enabled
        && !report.emit_expanded_available
        && !report.debug_trace_available
        && !report.source_map_available
        && !report.standard_library_required
        && !report.runtime_required
        && !report.external_process_required
        && !report.produced_user_generated_code;
}

bool is_valid(const BuiltinDeriveExpansionAdmissionGate& gate) noexcept
{
    const bool derive_admission = gate.admission_kind == FRONTEND_MACRO_M22A_DERIVE_ADMISSION_KIND;
    const bool non_derive_blocked = gate.admission_kind == FRONTEND_MACRO_M22A_NON_DERIVE_BLOCKED_KIND;
    return syntax::is_valid(gate.item)
        && syntax::is_valid(gate.module)
        && query::is_valid(gate.attached_part)
        && query::is_valid(gate.generated_part)
        && gate.generated_part.kind == query::ModulePartKind::generated
        && gate.generated_part.file.role == query::SourceRole::generated
        && is_nonzero_fingerprint(gate.token_buffer_identity)
        && is_nonzero_fingerprint(gate.preflight_identity)
        && is_nonzero_fingerprint(gate.parse_gate_identity)
        && is_nonzero_fingerprint(gate.diagnostic_identity)
        && is_nonzero_fingerprint(gate.closure_identity)
        && is_nonzero_fingerprint(gate.admission_identity)
        && gate.admission_identity != gate.token_buffer_identity
        && gate.admission_identity != gate.preflight_identity
        && gate.admission_policy == FRONTEND_MACRO_M22A_ADMISSION_POLICY
        && (derive_admission || non_derive_blocked)
        && gate.builtin_derive_input == derive_admission
        && !gate.query_name.empty()
        && ((derive_admission && gate.blocker_reason == FRONTEND_MACRO_M22A_DERIVE_BLOCKER)
            || (non_derive_blocked && gate.blocker_reason == FRONTEND_MACRO_M22A_NON_DERIVE_BLOCKER))
        && (!derive_admission || gate.token_count > 0)
        && (!non_derive_blocked || gate.token_count == 0)
        && (!non_derive_blocked || gate.capability_candidate_count == 0U)
        && gate.unsupported_candidate_count <= gate.capability_candidate_count
        && gate.duplicate_candidate_count <= gate.capability_candidate_count
        && gate.compiler_owned
        && gate.token_records_available == (gate.token_count > 0)
        && gate.preflight_available
        && gate.admission_visible
        && gate.query_reusable
        && !gate.parser_consumption_enabled
        && !gate.external_process_required
        && !gate.standard_library_required
        && !gate.runtime_required
        && !gate.generated_source_text
        && !gate.produced_user_generated_code;
}

bool is_valid(const BuiltinDeriveSemanticExpansionPlan& plan) noexcept
{
    const bool target_kind_valid = plan.target_kind == FRONTEND_MACRO_M22_TARGET_KIND_STRUCT
        || plan.target_kind == FRONTEND_MACRO_M22_TARGET_KIND_ENUM
        || plan.target_kind == FRONTEND_MACRO_M22_TARGET_KIND_OTHER;
    const bool target_struct_or_enum = plan.target_kind == FRONTEND_MACRO_M22_TARGET_KIND_STRUCT
        || plan.target_kind == FRONTEND_MACRO_M22_TARGET_KIND_ENUM;
    return syntax::is_valid(plan.item)
        && syntax::is_valid(plan.module)
        && query::is_valid(plan.attached_part)
        && query::is_valid(plan.generated_part)
        && plan.generated_part.kind == query::ModulePartKind::generated
        && plan.generated_part.file.role == query::SourceRole::generated
        && is_nonzero_fingerprint(plan.token_buffer_identity)
        && is_nonzero_fingerprint(plan.preflight_identity)
        && is_nonzero_fingerprint(plan.admission_identity)
        && is_nonzero_fingerprint(plan.semantic_plan_identity)
        && is_nonzero_fingerprint(plan.capability_set_identity)
        && plan.semantic_plan_identity != plan.capability_set_identity
        && plan.semantic_plan_identity != plan.admission_identity
        && plan.semantic_policy == FRONTEND_MACRO_M22B_SEMANTIC_POLICY
        && target_kind_valid
        && plan.target_struct_or_enum == target_struct_or_enum
        && plan.semantic_model == FRONTEND_MACRO_M22B_SEMANTIC_MODEL
        && plan.blocker_reason == FRONTEND_MACRO_M22B_BLOCKER
        && plan.capability_count == plan.copy_capability_count + plan.eq_capability_count
            + plan.hash_capability_count
        && (!plan.builtin_derive_input || plan.uses_existing_builtin_derive_capability_path)
        && (!plan.uses_existing_builtin_derive_capability_path || plan.builtin_derive_input)
        && !plan.requires_ast_mutation
        && !plan.requires_generated_items
        && !plan.requires_standard_library
        && !plan.requires_runtime
        && !plan.external_process_required
        && !plan.parser_consumption_enabled
        && !plan.produced_user_generated_code
        && plan.plan_visible
        && plan.query_reusable;
}

bool is_valid(const BuiltinDeriveParserConsumptionReleaseGate& gate) noexcept
{
    return syntax::is_valid(gate.module)
        && query::is_valid(gate.attached_part)
        && query::is_valid(gate.generated_part)
        && gate.generated_part.kind == query::ModulePartKind::generated
        && gate.generated_part.file.role == query::SourceRole::generated
        && is_nonzero_fingerprint(gate.contract_identity)
        && is_nonzero_fingerprint(gate.closure_identity)
        && is_nonzero_fingerprint(gate.admission_group_identity)
        && is_nonzero_fingerprint(gate.semantic_plan_group_identity)
        && is_nonzero_fingerprint(gate.release_gate_identity)
        && gate.release_gate_identity != gate.contract_identity
        && gate.release_gate_identity != gate.closure_identity
        && gate.release_gate_identity != gate.admission_group_identity
        && gate.release_policy == FRONTEND_MACRO_M22C_RELEASE_POLICY
        && !gate.release_query_name.empty()
        && gate.blocked_reason == FRONTEND_MACRO_M22C_RELEASE_BLOCKER
        && gate.derive_admission_count <= gate.admission_count
        && gate.semantic_plan_count == gate.admission_count
        && gate.parser_consumable_contract_count == 0U
        && gate.rollback_diagnostics_available
        && gate.debug_trace_prerequisite_available
        && gate.source_map_prerequisite_available
        && gate.hygiene_prerequisite_available
        && !gate.parser_consumption_enabled
        && !gate.generated_part_parsed
        && !gate.generated_part_merged
        && !gate.emit_expanded_available
        && !gate.debug_trace_available
        && !gate.source_map_available
        && !gate.standard_library_required
        && !gate.runtime_required
        && !gate.external_process_required
        && !gate.produced_user_generated_code
        && gate.release_visible
        && gate.query_reusable;
}

bool is_valid(const BuiltinDeriveReleaseHardeningMatrix& matrix) noexcept
{
    return syntax::is_valid(matrix.module)
        && query::is_valid(matrix.attached_part)
        && query::is_valid(matrix.generated_part)
        && matrix.generated_part.kind == query::ModulePartKind::generated
        && matrix.generated_part.file.role == query::SourceRole::generated
        && is_nonzero_fingerprint(matrix.release_gate_identity)
        && is_nonzero_fingerprint(matrix.admission_group_identity)
        && is_nonzero_fingerprint(matrix.semantic_plan_group_identity)
        && is_nonzero_fingerprint(matrix.hardening_matrix_identity)
        && matrix.hardening_matrix_identity != matrix.release_gate_identity
        && matrix.hardening_matrix_identity != matrix.admission_group_identity
        && matrix.hardening_policy == FRONTEND_MACRO_M22D_HARDENING_POLICY
        && !matrix.hardening_query_name.empty()
        && matrix.blocked_reason == FRONTEND_MACRO_M22D_HARDENING_BLOCKER
        && matrix.part_local_derive_admission_count <= matrix.part_local_admission_count
        && matrix.part_local_semantic_plan_count == matrix.part_local_admission_count
        && matrix.part_local_release_gate_count == 1U
        && matrix.global_admission_count >= matrix.part_local_admission_count
        && matrix.global_semantic_plan_count >= matrix.part_local_semantic_plan_count
        && matrix.global_admission_count == matrix.part_local_admission_count + matrix.cross_part_admission_count
        && matrix.global_semantic_plan_count
            == matrix.part_local_semantic_plan_count + matrix.cross_part_semantic_plan_count
        && matrix.global_generated_part_count > 0U
        && matrix.part_locality_preserved
        && matrix.multi_item_matrix_available
        && matrix.negative_matrix_complete
        && matrix.release_remains_blocked
        && !matrix.parser_consumption_enabled
        && !matrix.generated_part_parsed
        && !matrix.generated_part_merged
        && !matrix.emit_expanded_available
        && !matrix.debug_trace_available
        && !matrix.source_map_available
        && !matrix.standard_library_required
        && !matrix.runtime_required
        && !matrix.external_process_required
        && !matrix.produced_user_generated_code
        && matrix.matrix_visible
        && matrix.query_reusable;
}

bool is_valid(const BuiltinDeriveDebugDumpStabilityContract& contract) noexcept
{
    return syntax::is_valid(contract.module)
        && query::is_valid(contract.attached_part)
        && query::is_valid(contract.generated_part)
        && contract.generated_part.kind == query::ModulePartKind::generated
        && contract.generated_part.file.role == query::SourceRole::generated
        && is_nonzero_fingerprint(contract.release_gate_identity)
        && is_nonzero_fingerprint(contract.hardening_matrix_identity)
        && is_nonzero_fingerprint(contract.debug_dump_contract_identity)
        && contract.debug_dump_contract_identity != contract.release_gate_identity
        && contract.debug_dump_contract_identity != contract.hardening_matrix_identity
        && contract.debug_dump_policy == FRONTEND_MACRO_M22E_DEBUG_DUMP_POLICY
        && !contract.debug_dump_query_name.empty()
        && contract.blocked_reason == FRONTEND_MACRO_M22E_DEBUG_DUMP_BLOCKER
        && contract.dump_section_count == FRONTEND_MACRO_M22E_DEBUG_DUMP_SECTION_COUNT
        && contract.stable_ordering_available
        && contract.identity_projection_available
        && contract.summary_projection_available
        && contract.drift_debuggable
        && contract.debug_dump_contract_complete
        && !contract.emit_expanded_available
        && !contract.debug_trace_available
        && !contract.source_map_available
        && !contract.parser_consumption_enabled
        && !contract.standard_library_required
        && !contract.runtime_required
        && !contract.external_process_required
        && !contract.produced_user_generated_code
        && contract.contract_visible
        && contract.query_reusable;
}

bool is_valid(const BuiltinDeriveRollbackDiagnosticDesignGate& gate) noexcept
{
    return syntax::is_valid(gate.module)
        && query::is_valid(gate.attached_part)
        && query::is_valid(gate.generated_part)
        && gate.generated_part.kind == query::ModulePartKind::generated
        && gate.generated_part.file.role == query::SourceRole::generated
        && is_nonzero_fingerprint(gate.parser_consumption_contract_identity)
        && is_nonzero_fingerprint(gate.release_gate_identity)
        && is_nonzero_fingerprint(gate.hardening_matrix_identity)
        && is_nonzero_fingerprint(gate.debug_dump_contract_identity)
        && is_nonzero_fingerprint(gate.rollback_gate_identity)
        && gate.rollback_gate_identity != gate.parser_consumption_contract_identity
        && gate.rollback_gate_identity != gate.release_gate_identity
        && gate.rollback_gate_identity != gate.hardening_matrix_identity
        && gate.rollback_gate_identity != gate.debug_dump_contract_identity
        && gate.rollback_policy == FRONTEND_MACRO_M22F_ROLLBACK_POLICY
        && !gate.rollback_query_name.empty()
        && gate.blocked_reason == FRONTEND_MACRO_M22F_ROLLBACK_BLOCKER
        && gate.diagnostic_projection_count == gate.diagnostic_report_entry_count
        && gate.diagnostic_projection_count == gate.blocked_diagnostic_count
        && gate.diagnostic_projection_count == gate.derive_diagnostic_count + gate.empty_diagnostic_count
        && gate.parser_consumption_contract_count == 1U
        && gate.rollback_diagnostic_design_available
        && gate.diagnostic_grouping_available
        && gate.source_anchor_available
        && gate.token_tree_anchor_available
        && gate.debug_dump_contract_available
        && gate.release_rollback_plan_complete
        && !gate.rollback_execution_enabled
        && !gate.parser_consumption_enabled
        && !gate.generated_part_parsed
        && !gate.generated_part_merged
        && !gate.emit_expanded_available
        && !gate.debug_trace_available
        && !gate.source_map_available
        && !gate.standard_library_required
        && !gate.runtime_required
        && !gate.external_process_required
        && !gate.produced_user_generated_code
        && gate.rollback_gate_visible
        && gate.query_reusable;
}

bool is_valid(const BuiltinDeriveParserConsumptionAdmissionProtocol& protocol) noexcept
{
    return syntax::is_valid(protocol.module)
        && query::is_valid(protocol.attached_part)
        && query::is_valid(protocol.generated_part)
        && protocol.generated_part.kind == query::ModulePartKind::generated
        && protocol.generated_part.file.role == query::SourceRole::generated
        && is_nonzero_fingerprint(protocol.parser_consumption_contract_identity)
        && is_nonzero_fingerprint(protocol.release_gate_identity)
        && is_nonzero_fingerprint(protocol.rollback_gate_identity)
        && is_nonzero_fingerprint(protocol.admission_protocol_identity)
        && protocol.admission_protocol_identity != protocol.parser_consumption_contract_identity
        && protocol.admission_protocol_identity != protocol.release_gate_identity
        && protocol.admission_protocol_identity != protocol.rollback_gate_identity
        && protocol.admission_policy == FRONTEND_MACRO_M23A_ADMISSION_POLICY
        && !protocol.admission_query_name.empty()
        && protocol.blocked_reason == FRONTEND_MACRO_M23A_ADMISSION_BLOCKER
        && protocol.token_buffer_count == protocol.derive_candidate_count + protocol.empty_candidate_count
        && protocol.blocked_diagnostic_count == protocol.token_buffer_count
        && (!protocol.parser_contract_available || protocol.parser_consumption_contract_identity.byte_count > 0U)
        && protocol.release_gate_available
        && protocol.rollback_gate_available
        && protocol.parser_contract_available
        && protocol.deterministic_order_available
        && protocol.generated_tokens_checkpointed
        && protocol.admission_protocol_complete
        && !protocol.parser_consumption_enabled
        && !protocol.parser_admitted
        && !protocol.generated_part_parsed
        && !protocol.generated_part_merged
        && !protocol.emit_expanded_available
        && !protocol.debug_trace_available
        && !protocol.source_map_available
        && !protocol.standard_library_required
        && !protocol.runtime_required
        && !protocol.external_process_required
        && !protocol.produced_user_generated_code
        && protocol.protocol_visible
        && protocol.query_reusable;
}

bool is_valid(const BuiltinDeriveParserConsumptionCheckpointRollbackProtocol& protocol) noexcept
{
    return syntax::is_valid(protocol.module)
        && query::is_valid(protocol.attached_part)
        && query::is_valid(protocol.generated_part)
        && protocol.generated_part.kind == query::ModulePartKind::generated
        && protocol.generated_part.file.role == query::SourceRole::generated
        && is_nonzero_fingerprint(protocol.admission_protocol_identity)
        && is_nonzero_fingerprint(protocol.rollback_gate_identity)
        && is_nonzero_fingerprint(protocol.checkpoint_protocol_identity)
        && protocol.checkpoint_protocol_identity != protocol.admission_protocol_identity
        && protocol.checkpoint_protocol_identity != protocol.rollback_gate_identity
        && protocol.checkpoint_policy == FRONTEND_MACRO_M23B_CHECKPOINT_POLICY
        && !protocol.checkpoint_query_name.empty()
        && protocol.blocked_reason == FRONTEND_MACRO_M23B_CHECKPOINT_BLOCKER
        && protocol.checkpoint_count == FRONTEND_MACRO_M23B_CHECKPOINT_PLAN_COUNT
        && protocol.rollback_plan_count == protocol.checkpoint_count
        && protocol.diagnostic_anchor_count > 0U
        && protocol.parser_state_checkpoint_available
        && protocol.token_cursor_checkpoint_available
        && protocol.generated_part_checkpoint_available
        && protocol.diagnostic_replay_available
        && protocol.rollback_protocol_complete
        && !protocol.rollback_execution_enabled
        && !protocol.parser_consumption_enabled
        && !protocol.generated_part_parsed
        && !protocol.generated_part_merged
        && !protocol.emit_expanded_available
        && !protocol.debug_trace_available
        && !protocol.source_map_available
        && !protocol.standard_library_required
        && !protocol.runtime_required
        && !protocol.external_process_required
        && !protocol.produced_user_generated_code
        && protocol.protocol_visible
        && protocol.query_reusable;
}

bool is_valid(const BuiltinDeriveParserPreConsumptionVerificationClosure& closure) noexcept
{
    return syntax::is_valid(closure.module)
        && query::is_valid(closure.attached_part)
        && query::is_valid(closure.generated_part)
        && closure.generated_part.kind == query::ModulePartKind::generated
        && closure.generated_part.file.role == query::SourceRole::generated
        && is_nonzero_fingerprint(closure.admission_protocol_identity)
        && is_nonzero_fingerprint(closure.checkpoint_protocol_identity)
        && is_nonzero_fingerprint(closure.debug_dump_contract_identity)
        && is_nonzero_fingerprint(closure.verification_closure_identity)
        && closure.verification_closure_identity != closure.admission_protocol_identity
        && closure.verification_closure_identity != closure.checkpoint_protocol_identity
        && closure.verification_closure_identity != closure.debug_dump_contract_identity
        && closure.verification_policy == FRONTEND_MACRO_M23C_VERIFICATION_POLICY
        && !closure.verification_query_name.empty()
        && closure.blocked_reason == FRONTEND_MACRO_M23C_VERIFICATION_BLOCKER
        && closure.admission_protocol_count == 1U
        && closure.checkpoint_protocol_count == 1U
        && closure.hardening_matrix_count == 1U
        && closure.debug_dump_contract_count == 1U
        && closure.rollback_gate_count == 1U
        && closure.admission_protocol_available
        && closure.checkpoint_protocol_available
        && closure.release_hardening_available
        && closure.debug_dump_contract_available
        && closure.rollback_gate_available
        && closure.verification_closure_complete
        && !closure.parser_consumption_enabled
        && !closure.generated_part_parsed
        && !closure.generated_part_merged
        && !closure.sema_visible
        && !closure.emit_expanded_available
        && !closure.debug_trace_available
        && !closure.source_map_available
        && !closure.standard_library_required
        && !closure.runtime_required
        && !closure.external_process_required
        && !closure.produced_user_generated_code
        && closure.closure_visible
        && closure.query_reusable;
}

bool is_valid(const BuiltinDeriveControlledParserDryRunAdapter& adapter) noexcept
{
    return syntax::is_valid(adapter.module)
        && query::is_valid(adapter.attached_part)
        && query::is_valid(adapter.generated_part)
        && adapter.generated_part.kind == query::ModulePartKind::generated
        && adapter.generated_part.file.role == query::SourceRole::generated
        && is_nonzero_fingerprint(adapter.verification_closure_identity)
        && is_nonzero_fingerprint(adapter.admission_protocol_identity)
        && is_nonzero_fingerprint(adapter.checkpoint_protocol_identity)
        && is_nonzero_fingerprint(adapter.dry_run_adapter_identity)
        && adapter.dry_run_adapter_identity != adapter.verification_closure_identity
        && adapter.dry_run_adapter_identity != adapter.admission_protocol_identity
        && adapter.dry_run_adapter_identity != adapter.checkpoint_protocol_identity
        && adapter.adapter_policy == FRONTEND_MACRO_M24A_DRY_RUN_ADAPTER_POLICY
        && !adapter.adapter_query_name.empty()
        && adapter.blocked_reason == FRONTEND_MACRO_M24A_DRY_RUN_ADAPTER_BLOCKER
        && adapter.diagnostic_anchor_count > 0U
        && adapter.prerequisite_count == FRONTEND_MACRO_M24A_DRY_RUN_PREREQUISITE_COUNT
        && adapter.verification_closure_available
        && adapter.admission_protocol_available
        && adapter.checkpoint_protocol_available
        && adapter.compiler_owned_tokens_available
        && adapter.diagnostic_replay_available
        && adapter.dry_run_adapter_complete
        && !adapter.dry_run_executed
        && !adapter.parser_consumption_enabled
        && !adapter.parser_admitted
        && !adapter.generated_part_parsed
        && !adapter.generated_part_merged
        && !adapter.sema_visible
        && !adapter.emit_expanded_available
        && !adapter.debug_trace_available
        && !adapter.source_map_available
        && !adapter.standard_library_required
        && !adapter.runtime_required
        && !adapter.external_process_required
        && !adapter.produced_user_generated_code
        && adapter.adapter_visible
        && adapter.query_reusable;
}

bool is_valid(const BuiltinDeriveDryRunRollbackDiagnosticReplay& replay) noexcept
{
    return syntax::is_valid(replay.module)
        && query::is_valid(replay.attached_part)
        && query::is_valid(replay.generated_part)
        && replay.generated_part.kind == query::ModulePartKind::generated
        && replay.generated_part.file.role == query::SourceRole::generated
        && is_nonzero_fingerprint(replay.dry_run_adapter_identity)
        && is_nonzero_fingerprint(replay.checkpoint_protocol_identity)
        && is_nonzero_fingerprint(replay.rollback_gate_identity)
        && is_nonzero_fingerprint(replay.replay_protocol_identity)
        && replay.replay_protocol_identity != replay.dry_run_adapter_identity
        && replay.replay_protocol_identity != replay.checkpoint_protocol_identity
        && replay.replay_protocol_identity != replay.rollback_gate_identity
        && replay.replay_policy == FRONTEND_MACRO_M24B_ROLLBACK_REPLAY_POLICY
        && !replay.replay_query_name.empty()
        && replay.blocked_reason == FRONTEND_MACRO_M24B_ROLLBACK_REPLAY_BLOCKER
        && replay.diagnostic_anchor_count > 0U
        && replay.report_entry_count == replay.diagnostic_anchor_count
        && replay.planned_replay_count == replay.diagnostic_anchor_count
        && replay.executed_replay_count == 0U
        && replay.dry_run_adapter_available
        && replay.checkpoint_protocol_available
        && replay.rollback_gate_available
        && replay.diagnostic_replay_plan_available
        && replay.replay_protocol_complete
        && !replay.replay_execution_enabled
        && !replay.dry_run_executed
        && !replay.parser_consumption_enabled
        && !replay.generated_part_parsed
        && !replay.generated_part_merged
        && !replay.sema_visible
        && !replay.emit_expanded_available
        && !replay.debug_trace_available
        && !replay.source_map_available
        && !replay.standard_library_required
        && !replay.runtime_required
        && !replay.external_process_required
        && !replay.produced_user_generated_code
        && replay.replay_visible
        && replay.query_reusable;
}

bool is_valid(const BuiltinDeriveDryRunNegativeMatrixClosure& matrix) noexcept
{
    return syntax::is_valid(matrix.module)
        && query::is_valid(matrix.attached_part)
        && query::is_valid(matrix.generated_part)
        && matrix.generated_part.kind == query::ModulePartKind::generated
        && matrix.generated_part.file.role == query::SourceRole::generated
        && is_nonzero_fingerprint(matrix.dry_run_adapter_identity)
        && is_nonzero_fingerprint(matrix.rollback_replay_identity)
        && is_nonzero_fingerprint(matrix.verification_closure_identity)
        && is_nonzero_fingerprint(matrix.negative_matrix_identity)
        && matrix.negative_matrix_identity != matrix.dry_run_adapter_identity
        && matrix.negative_matrix_identity != matrix.rollback_replay_identity
        && matrix.negative_matrix_identity != matrix.verification_closure_identity
        && matrix.matrix_policy == FRONTEND_MACRO_M24C_NEGATIVE_MATRIX_POLICY
        && !matrix.matrix_query_name.empty()
        && matrix.blocked_reason == FRONTEND_MACRO_M24C_NEGATIVE_MATRIX_BLOCKER
        && matrix.dry_run_adapter_count == 1U
        && matrix.rollback_replay_count == 1U
        && matrix.verification_closure_count == 1U
        && matrix.negative_case_count == FRONTEND_MACRO_M24C_NEGATIVE_CASE_COUNT
        && matrix.parser_consumable_case_count == 0U
        && matrix.dry_run_adapter_available
        && matrix.rollback_replay_available
        && matrix.verification_closure_available
        && matrix.negative_matrix_complete
        && !matrix.dry_run_executed
        && !matrix.parser_consumption_enabled
        && !matrix.parser_admitted
        && !matrix.generated_part_parsed
        && !matrix.generated_part_merged
        && !matrix.sema_visible
        && !matrix.emit_expanded_available
        && !matrix.debug_trace_available
        && !matrix.source_map_available
        && !matrix.standard_library_required
        && !matrix.runtime_required
        && !matrix.external_process_required
        && !matrix.produced_user_generated_code
        && matrix.matrix_visible
        && matrix.query_reusable;
}

bool is_valid(const BuiltinDeriveParserDryRunSessionBoundary& session) noexcept
{
    return syntax::is_valid(session.module)
        && query::is_valid(session.attached_part)
        && query::is_valid(session.generated_part)
        && session.generated_part.kind == query::ModulePartKind::generated
        && session.generated_part.file.role == query::SourceRole::generated
        && is_nonzero_fingerprint(session.dry_run_adapter_identity)
        && is_nonzero_fingerprint(session.negative_matrix_identity)
        && is_nonzero_fingerprint(session.generated_buffer_identity)
        && is_nonzero_fingerprint(session.parse_config_fingerprint)
        && is_nonzero_fingerprint(session.dry_run_session_identity)
        && session.dry_run_session_identity != session.dry_run_adapter_identity
        && session.dry_run_session_identity != session.negative_matrix_identity
        && session.dry_run_session_identity != session.generated_buffer_identity
        && session.dry_run_session_identity != session.parse_config_fingerprint
        && session.session_policy == FRONTEND_MACRO_M25A_DRY_RUN_SESSION_POLICY
        && !session.session_query_name.empty()
        && session.blocked_reason == FRONTEND_MACRO_M25A_DRY_RUN_SESSION_BLOCKER
        && session.diagnostic_anchor_count > 0U
        && session.parser_state_snapshot_count == FRONTEND_MACRO_M25A_PARSER_STATE_SNAPSHOT_COUNT
        && session.committed_parse_count == 0U
        && session.dry_run_adapter_available
        && session.negative_matrix_available
        && session.compiler_owned_token_stream_available
        && session.sandbox_available
        && session.check_only
        && session.dry_run_session_complete
        && !session.dry_run_executed
        && !session.session_committed
        && !session.parser_consumption_enabled
        && !session.parser_admitted
        && !session.parser_cursor_advanced
        && !session.generated_part_parsed
        && !session.generated_part_merged
        && !session.ast_mutated
        && !session.sema_visible
        && !session.emit_expanded_available
        && !session.debug_trace_available
        && !session.source_map_available
        && !session.standard_library_required
        && !session.runtime_required
        && !session.external_process_required
        && !session.produced_user_generated_code
        && session.session_visible
        && session.query_reusable;
}

bool is_valid(const BuiltinDeriveTokenCursorSnapshotRollbackProof& proof) noexcept
{
    return syntax::is_valid(proof.module)
        && query::is_valid(proof.attached_part)
        && query::is_valid(proof.generated_part)
        && proof.generated_part.kind == query::ModulePartKind::generated
        && proof.generated_part.file.role == query::SourceRole::generated
        && is_nonzero_fingerprint(proof.dry_run_session_identity)
        && is_nonzero_fingerprint(proof.checkpoint_protocol_identity)
        && is_nonzero_fingerprint(proof.rollback_replay_identity)
        && is_nonzero_fingerprint(proof.cursor_snapshot_identity)
        && proof.cursor_snapshot_identity != proof.dry_run_session_identity
        && proof.cursor_snapshot_identity != proof.checkpoint_protocol_identity
        && proof.cursor_snapshot_identity != proof.rollback_replay_identity
        && proof.snapshot_policy == FRONTEND_MACRO_M25B_CURSOR_SNAPSHOT_POLICY
        && !proof.snapshot_query_name.empty()
        && proof.blocked_reason == FRONTEND_MACRO_M25B_CURSOR_SNAPSHOT_BLOCKER
        && proof.checkpoint_count == FRONTEND_MACRO_M23B_CHECKPOINT_PLAN_COUNT
        && proof.cursor_snapshot_count == proof.checkpoint_count
        && proof.parser_state_snapshot_count == proof.checkpoint_count
        && proof.rollback_proof_count == proof.checkpoint_count
        && proof.cursor_commit_count == 0U
        && proof.dry_run_session_available
        && proof.checkpoint_protocol_available
        && proof.rollback_replay_available
        && proof.token_cursor_snapshot_available
        && proof.parser_state_snapshot_available
        && proof.rollback_proof_complete
        && !proof.replay_execution_enabled
        && !proof.rollback_execution_enabled
        && !proof.dry_run_executed
        && !proof.parser_cursor_advanced
        && !proof.session_committed
        && !proof.parser_consumption_enabled
        && !proof.parser_admitted
        && !proof.generated_part_parsed
        && !proof.generated_part_merged
        && !proof.ast_mutated
        && !proof.sema_visible
        && !proof.standard_library_required
        && !proof.runtime_required
        && !proof.external_process_required
        && !proof.produced_user_generated_code
        && proof.proof_visible
        && proof.query_reusable;
}

bool is_valid(const BuiltinDeriveDiagnosticShadowNoAstMutationClosure& closure) noexcept
{
    return syntax::is_valid(closure.module)
        && query::is_valid(closure.attached_part)
        && query::is_valid(closure.generated_part)
        && closure.generated_part.kind == query::ModulePartKind::generated
        && closure.generated_part.file.role == query::SourceRole::generated
        && is_nonzero_fingerprint(closure.dry_run_session_identity)
        && is_nonzero_fingerprint(closure.cursor_snapshot_identity)
        && is_nonzero_fingerprint(closure.rollback_replay_identity)
        && is_nonzero_fingerprint(closure.negative_matrix_identity)
        && is_nonzero_fingerprint(closure.closure_identity)
        && closure.closure_identity != closure.dry_run_session_identity
        && closure.closure_identity != closure.cursor_snapshot_identity
        && closure.closure_identity != closure.rollback_replay_identity
        && closure.closure_identity != closure.negative_matrix_identity
        && closure.closure_policy == FRONTEND_MACRO_M25C_DIAGNOSTIC_SHADOW_POLICY
        && !closure.closure_query_name.empty()
        && closure.blocked_reason == FRONTEND_MACRO_M25C_DIAGNOSTIC_SHADOW_BLOCKER
        && closure.dry_run_session_count == 1U
        && closure.cursor_snapshot_proof_count == 1U
        && closure.rollback_replay_count == 1U
        && closure.negative_matrix_count == 1U
        && closure.diagnostic_shadow_count > 0U
        && closure.executed_shadow_count == 0U
        && closure.ast_mutation_count == 0U
        && closure.parser_consumable_case_count == 0U
        && closure.dry_run_session_available
        && closure.cursor_snapshot_proof_available
        && closure.rollback_replay_available
        && closure.negative_matrix_available
        && closure.diagnostic_shadow_available
        && closure.no_ast_mutation_verified
        && closure.closure_complete
        && !closure.dry_run_executed
        && !closure.replay_execution_enabled
        && !closure.session_committed
        && !closure.parser_cursor_advanced
        && !closure.parser_consumption_enabled
        && !closure.parser_admitted
        && !closure.generated_part_parsed
        && !closure.generated_part_merged
        && !closure.ast_mutated
        && !closure.sema_visible
        && !closure.emit_expanded_available
        && !closure.debug_trace_available
        && !closure.source_map_available
        && !closure.standard_library_required
        && !closure.runtime_required
        && !closure.external_process_required
        && !closure.produced_user_generated_code
        && closure.closure_visible
        && closure.query_reusable;
}

bool is_valid(const BuiltinDeriveParserDryRunAdmissionGate& gate) noexcept
{
    return syntax::is_valid(gate.module)
        && query::is_valid(gate.attached_part)
        && query::is_valid(gate.generated_part)
        && gate.generated_part.kind == query::ModulePartKind::generated
        && gate.generated_part.file.role == query::SourceRole::generated
        && is_nonzero_fingerprint(gate.dry_run_session_identity)
        && is_nonzero_fingerprint(gate.cursor_snapshot_identity)
        && is_nonzero_fingerprint(gate.diagnostic_shadow_closure_identity)
        && is_nonzero_fingerprint(gate.generated_buffer_identity)
        && is_nonzero_fingerprint(gate.parse_config_fingerprint)
        && is_nonzero_fingerprint(gate.admission_gate_identity)
        && gate.admission_gate_identity != gate.dry_run_session_identity
        && gate.admission_gate_identity != gate.cursor_snapshot_identity
        && gate.admission_gate_identity != gate.diagnostic_shadow_closure_identity
        && gate.admission_gate_identity != gate.generated_buffer_identity
        && gate.admission_gate_identity != gate.parse_config_fingerprint
        && gate.admission_policy == FRONTEND_MACRO_M26A_DRY_RUN_ADMISSION_GATE_POLICY
        && !gate.admission_query_name.empty()
        && gate.blocked_reason == FRONTEND_MACRO_M26A_DRY_RUN_ADMISSION_GATE_BLOCKER
        && gate.dry_run_session_count == 1U
        && gate.cursor_snapshot_proof_count == 1U
        && gate.diagnostic_shadow_closure_count == 1U
        && gate.admission_prerequisite_count == FRONTEND_MACRO_M26A_DRY_RUN_ADMISSION_PREREQUISITE_COUNT
        && gate.dry_run_execution_admitted_count == 0U
        && gate.parser_consumable_case_count == 0U
        && gate.dry_run_session_available
        && gate.cursor_snapshot_proof_available
        && gate.diagnostic_shadow_closure_available
        && gate.generated_buffer_available
        && gate.parse_config_available
        && gate.admission_gate_complete
        && !gate.dry_run_execution_admitted
        && !gate.dry_run_executed
        && !gate.diagnostic_shadow_executed
        && !gate.rollback_execution_enabled
        && !gate.session_committed
        && !gate.parser_cursor_advanced
        && !gate.parser_consumption_enabled
        && !gate.parser_admitted
        && !gate.generated_part_parsed
        && !gate.generated_part_merged
        && !gate.ast_mutated
        && !gate.sema_visible
        && !gate.emit_expanded_available
        && !gate.debug_trace_available
        && !gate.source_map_available
        && !gate.standard_library_required
        && !gate.runtime_required
        && !gate.external_process_required
        && !gate.produced_user_generated_code
        && gate.gate_visible
        && gate.query_reusable;
}

bool is_valid(const BuiltinDeriveErrorRecoveryShadowDiagnosticGate& gate) noexcept
{
    return syntax::is_valid(gate.module)
        && query::is_valid(gate.attached_part)
        && query::is_valid(gate.generated_part)
        && gate.generated_part.kind == query::ModulePartKind::generated
        && gate.generated_part.file.role == query::SourceRole::generated
        && is_nonzero_fingerprint(gate.dry_run_admission_gate_identity)
        && is_nonzero_fingerprint(gate.diagnostic_shadow_closure_identity)
        && is_nonzero_fingerprint(gate.rollback_replay_identity)
        && is_nonzero_fingerprint(gate.parser_report_identity)
        && is_nonzero_fingerprint(gate.recovery_shadow_identity)
        && gate.recovery_shadow_identity != gate.dry_run_admission_gate_identity
        && gate.recovery_shadow_identity != gate.diagnostic_shadow_closure_identity
        && gate.recovery_shadow_identity != gate.rollback_replay_identity
        && gate.recovery_shadow_identity != gate.parser_report_identity
        && gate.recovery_policy == FRONTEND_MACRO_M26B_RECOVERY_SHADOW_GATE_POLICY
        && !gate.recovery_query_name.empty()
        && gate.blocked_reason == FRONTEND_MACRO_M26B_RECOVERY_SHADOW_GATE_BLOCKER
        && gate.diagnostic_shadow_count > 0U
        && gate.report_entry_count > 0U
        && gate.planned_recovery_count == gate.report_entry_count
        && gate.executed_recovery_count == 0U
        && gate.emitted_diagnostic_count == 0U
        && gate.dry_run_admission_gate_available
        && gate.diagnostic_shadow_closure_available
        && gate.rollback_replay_available
        && gate.parser_report_available
        && gate.recovery_shadow_plan_available
        && gate.recovery_shadow_complete
        && !gate.recovery_execution_enabled
        && !gate.diagnostic_emission_enabled
        && !gate.dry_run_execution_admitted
        && !gate.dry_run_executed
        && !gate.rollback_execution_enabled
        && !gate.session_committed
        && !gate.parser_cursor_advanced
        && !gate.parser_consumption_enabled
        && !gate.parser_admitted
        && !gate.generated_part_parsed
        && !gate.generated_part_merged
        && !gate.ast_mutated
        && !gate.sema_visible
        && !gate.emit_expanded_available
        && !gate.debug_trace_available
        && !gate.source_map_available
        && !gate.standard_library_required
        && !gate.runtime_required
        && !gate.external_process_required
        && !gate.produced_user_generated_code
        && gate.gate_visible
        && gate.query_reusable;
}

bool is_valid(const BuiltinDeriveCursorRollbackAstMutationVerifierClosure& closure) noexcept
{
    return syntax::is_valid(closure.module)
        && query::is_valid(closure.attached_part)
        && query::is_valid(closure.generated_part)
        && closure.generated_part.kind == query::ModulePartKind::generated
        && closure.generated_part.file.role == query::SourceRole::generated
        && is_nonzero_fingerprint(closure.dry_run_admission_gate_identity)
        && is_nonzero_fingerprint(closure.recovery_shadow_identity)
        && is_nonzero_fingerprint(closure.cursor_snapshot_identity)
        && is_nonzero_fingerprint(closure.dry_run_session_identity)
        && is_nonzero_fingerprint(closure.diagnostic_shadow_closure_identity)
        && is_nonzero_fingerprint(closure.verifier_closure_identity)
        && closure.verifier_closure_identity != closure.dry_run_admission_gate_identity
        && closure.verifier_closure_identity != closure.recovery_shadow_identity
        && closure.verifier_closure_identity != closure.cursor_snapshot_identity
        && closure.verifier_closure_identity != closure.dry_run_session_identity
        && closure.verifier_closure_identity != closure.diagnostic_shadow_closure_identity
        && closure.verifier_policy == FRONTEND_MACRO_M26C_ROLLBACK_AST_VERIFIER_POLICY
        && !closure.verifier_query_name.empty()
        && closure.blocked_reason == FRONTEND_MACRO_M26C_ROLLBACK_AST_VERIFIER_BLOCKER
        && closure.cursor_snapshot_count == FRONTEND_MACRO_M23B_CHECKPOINT_PLAN_COUNT
        && closure.rollback_proof_count == FRONTEND_MACRO_M23B_CHECKPOINT_PLAN_COUNT
        && closure.recovery_shadow_count == 1U
        && closure.ast_baseline_snapshot_count == FRONTEND_MACRO_M26C_AST_BASELINE_SNAPSHOT_COUNT
        && closure.ast_mutation_count == 0U
        && closure.cursor_commit_count == 0U
        && closure.session_commit_count == 0U
        && closure.parser_consumable_case_count == 0U
        && closure.dry_run_admission_gate_available
        && closure.recovery_shadow_available
        && closure.cursor_snapshot_proof_available
        && closure.dry_run_session_available
        && closure.diagnostic_shadow_closure_available
        && closure.ast_baseline_available
        && closure.rollback_execution_guard_available
        && closure.ast_mutation_verifier_complete
        && !closure.rollback_execution_enabled
        && !closure.recovery_execution_enabled
        && !closure.diagnostic_emission_enabled
        && !closure.dry_run_execution_admitted
        && !closure.dry_run_executed
        && !closure.session_committed
        && !closure.parser_cursor_advanced
        && !closure.parser_consumption_enabled
        && !closure.parser_admitted
        && !closure.generated_part_parsed
        && !closure.generated_part_merged
        && !closure.ast_mutated
        && !closure.sema_visible
        && !closure.emit_expanded_available
        && !closure.debug_trace_available
        && !closure.source_map_available
        && !closure.standard_library_required
        && !closure.runtime_required
        && !closure.external_process_required
        && !closure.produced_user_generated_code
        && closure.closure_visible
        && closure.query_reusable;
}

bool is_valid(const AurexMacroSurfaceAdmissionGate& gate) noexcept
{
    const base::u64 surface_count = (gate.declarative_surface ? 1U : 0U)
        + (gate.user_derive_surface ? 1U : 0U)
        + (gate.compile_time_execution_surface ? 1U : 0U);
    const bool kind_matches_surface =
        (gate.macro_kind == syntax::MacroDeclKind::declarative && gate.declarative_surface)
        || (gate.macro_kind == syntax::MacroDeclKind::derive && gate.user_derive_surface)
        || (gate.macro_kind == syntax::MacroDeclKind::compile_time && gate.compile_time_execution_surface);

    return syntax::is_valid(gate.item)
        && syntax::is_valid(gate.module)
        && query::is_valid(gate.attached_part)
        && is_nonzero_fingerprint(gate.body_fingerprint)
        && is_nonzero_fingerprint(gate.admission_identity)
        && !gate.macro_name.empty()
        && gate.admission_policy == FRONTEND_MACRO_M27A_AUREX_MACRO_SURFACE_POLICY
        && gate.query_name == macro_surface_query_name(gate.module, gate.part_index, gate.item, gate.macro_name)
        && gate.admission_identity == macro_surface_admission_identity(gate)
        && gate.blocker_reason == macro_surface_blocker_reason(gate.macro_kind)
        && source_range_is_well_formed(gate.macro_range)
        && source_range_is_well_formed(gate.body_range)
        && gate.body_token_count > 0U
        && gate.body_balanced
        && surface_count == 1U
        && kind_matches_surface
        && !gate.expansion_enabled
        && !gate.compile_time_execution_enabled
        && !gate.ast_mutated
        && !gate.parser_consumption_enabled
        && !gate.sema_visible_generated_items
        && !gate.standard_library_required
        && !gate.runtime_required
        && !gate.external_process_required
        && !gate.produced_user_generated_code
        && gate.gate_visible
        && gate.query_reusable;
}

bool is_valid(const AurexMacroDefinitionSiteHygieneAdmissionGate& gate) noexcept
{
    return syntax::is_valid(gate.item)
        && syntax::is_valid(gate.module)
        && query::is_valid(gate.attached_part)
        && is_nonzero_fingerprint(gate.surface_admission_identity)
        && is_nonzero_fingerprint(gate.body_fingerprint)
        && is_nonzero_fingerprint(gate.definition_site_mark)
        && is_nonzero_fingerprint(gate.fresh_name_scope)
        && is_nonzero_fingerprint(gate.diagnostic_anchor_identity)
        && is_nonzero_fingerprint(gate.hygiene_identity)
        && !gate.macro_name.empty()
        && gate.hygiene_policy == FRONTEND_MACRO_M27B_DEFINITION_SITE_HYGIENE_POLICY
        && gate.query_name == macro_definition_site_hygiene_query_name(
               gate.module, gate.part_index, gate.item, gate.macro_name)
        && gate.blocker_reason == FRONTEND_MACRO_M27B_DEFINITION_SITE_HYGIENE_BLOCKER
        && source_range_is_well_formed(gate.macro_range)
        && source_range_is_well_formed(gate.body_range)
        && gate.hygiene_identity == macro_definition_site_hygiene_identity(gate)
        && gate.definition_site_scope_available
        && gate.fresh_name_scope_reserved
        && gate.diagnostic_anchor_available
        && !gate.hygiene_resolution_enabled
        && !gate.declared_names_visible
        && !gate.captures_call_site_locals
        && !gate.standard_library_required
        && !gate.runtime_required
        && !gate.external_process_required
        && !gate.produced_user_generated_code
        && gate.gate_visible
        && gate.query_reusable;
}

bool is_valid(const AurexMacroTypedMatcherAdmissionGate& gate) noexcept
{
    const base::u64 matcher_kind_count = (gate.expr_list_matcher ? 1U : 0U)
        + (gate.item_matcher ? 1U : 0U)
        + (gate.token_stream_matcher ? 1U : 0U)
        + (gate.unknown_matcher ? 1U : 0U);
    const bool kind_matches_flags =
        (gate.matcher_kind == AurexMacroTypedMatcherKind::expr_list && gate.expr_list_matcher)
        || (gate.matcher_kind == AurexMacroTypedMatcherKind::item && gate.item_matcher)
        || (gate.matcher_kind == AurexMacroTypedMatcherKind::tokens && gate.token_stream_matcher)
        || (gate.matcher_kind == AurexMacroTypedMatcherKind::unknown && gate.unknown_matcher);

    return syntax::is_valid(gate.item)
        && syntax::is_valid(gate.module)
        && query::is_valid(gate.attached_part)
        && is_nonzero_fingerprint(gate.surface_admission_identity)
        && is_nonzero_fingerprint(gate.body_fingerprint)
        && is_nonzero_fingerprint(gate.matcher_fingerprint)
        && is_nonzero_fingerprint(gate.matcher_identity)
        && is_nonzero_fingerprint(gate.definition_site_hygiene_identity)
        && is_nonzero_fingerprint(gate.diagnostic_anchor_identity)
        && !gate.macro_name.empty()
        && gate.matcher_policy == FRONTEND_MACRO_M27B_TYPED_MATCHER_POLICY
        && gate.query_name == macro_typed_matcher_query_name(
               gate.module, gate.part_index, gate.item, gate.matcher_index, gate.macro_name)
        && gate.matcher_fingerprint == matcher_fingerprint(gate)
        && gate.matcher_identity == macro_typed_matcher_identity(gate)
        && source_range_is_well_formed(gate.matcher_range)
        && (gate.matcher_shape_recognized ? source_range_is_well_formed(gate.output_range) : true)
        && matcher_kind_count == 1U
        && kind_matches_flags
        && gate.matcher_shape_recognized == (gate.matcher_kind != AurexMacroTypedMatcherKind::unknown)
        && gate.blocker_reason == (gate.matcher_shape_recognized
               ? FRONTEND_MACRO_M27B_TYPED_MATCHER_BLOCKER
               : FRONTEND_MACRO_M27B_UNKNOWN_MATCHER_BLOCKER)
        && gate.definition_site_hygiene_available
        && gate.fresh_name_scope_available
        && gate.diagnostic_anchor_available
        && !gate.matcher_execution_enabled
        && !gate.expansion_enabled
        && !gate.compile_time_execution_enabled
        && !gate.parser_consumption_enabled
        && !gate.ast_mutated
        && !gate.sema_visible_generated_items
        && !gate.standard_library_required
        && !gate.runtime_required
        && !gate.external_process_required
        && !gate.produced_user_generated_code
        && gate.gate_visible
        && gate.query_reusable;
}

bool is_valid(const EarlyItemExpansionSummary& summary, const EarlyItemExpansionResult& result) noexcept
{
    return summary_equals(summary, summarize_early_item_expansion_counts(result));
}

bool is_valid(const EarlyItemExpansionResult& result) noexcept
{
    return std::string_view(result.name) == FRONTEND_MACRO_M26C_EXPANSION_NAME
        && query::is_valid_m21c_macro_expansion_plan(result.plan)
        && std::all_of(result.inputs.begin(), result.inputs.end(), [](const EarlyItemMacroInput& input) {
               return is_valid(input);
           })
        && std::all_of(result.generated_parts.begin(), result.generated_parts.end(),
               [](const GeneratedModulePartPlaceholder& placeholder) {
                   return is_valid(placeholder);
               })
        && std::all_of(result.generated_part_stubs.begin(), result.generated_part_stubs.end(),
               [](const GeneratedModulePartParseMergeStub& stub) {
                   return is_valid(stub);
               })
        && generated_part_stubs_match_placeholders(result.generated_parts, result.generated_part_stubs)
        && std::all_of(result.source_maps.begin(), result.source_maps.end(),
               [](const ExpansionSourceMapPlaceholder& placeholder) {
                   return is_valid(placeholder);
               })
        && std::all_of(result.hygiene_stubs.begin(), result.hygiene_stubs.end(),
               [](const ExpansionHygieneStub& stub) {
                   return is_valid(stub);
               })
        && std::all_of(result.trace_stubs.begin(), result.trace_stubs.end(),
               [](const ExpansionTraceStub& stub) {
                   return is_valid(stub);
               })
        && std::all_of(result.generated_item_declarations.begin(), result.generated_item_declarations.end(),
               [](const GeneratedItemDeclarationStub& stub) {
                   return is_valid(stub);
               })
        && std::all_of(result.declared_generated_names.begin(), result.declared_generated_names.end(),
               [](const DeclaredGeneratedNameStub& stub) {
                   return is_valid(stub);
               })
        && std::all_of(result.token_materialization_admissions.begin(),
               result.token_materialization_admissions.end(),
               [](const TokenMaterializationAdmissionStub& stub) {
                   return is_valid(stub);
               })
        && std::all_of(result.generated_token_buffers.begin(), result.generated_token_buffers.end(),
               [](const GeneratedTokenBufferStub& stub) {
                   return is_valid(stub);
               })
        && std::all_of(result.generated_token_records.begin(), result.generated_token_records.end(),
               [](const GeneratedTokenRecord& record) {
                   return is_valid(record);
               })
        && std::all_of(result.parser_admission_gates.begin(), result.parser_admission_gates.end(),
               [](const GeneratedTokenParserAdmissionGateStub& stub) {
                   return is_valid(stub);
               })
        && std::all_of(result.parser_admission_diagnostics.begin(),
               result.parser_admission_diagnostics.end(),
               [](const ParserAdmissionDiagnosticProjectionStub& stub) {
                   return is_valid(stub);
               })
        && std::all_of(result.parser_admission_report_entries.begin(),
               result.parser_admission_report_entries.end(),
               [](const ParserAdmissionDiagnosticReportEntry& entry) {
                   return is_valid(entry);
               })
        && std::all_of(result.parser_admission_reports.begin(), result.parser_admission_reports.end(),
               [](const ParserAdmissionDiagnosticReport& report) {
                   return is_valid(report);
               })
        && std::all_of(result.parser_readiness_preflight_entries.begin(),
               result.parser_readiness_preflight_entries.end(),
               [](const GeneratedTokenParserReadinessPreflightEntry& entry) {
                   return is_valid(entry);
               })
        && std::all_of(result.parser_consumption_contract_gates.begin(),
               result.parser_consumption_contract_gates.end(),
               [](const GeneratedTokenParserConsumptionContractGate& gate) {
                   return is_valid(gate);
               })
        && std::all_of(result.macro_boundary_closure_reports.begin(),
               result.macro_boundary_closure_reports.end(),
               [](const MacroExpansionBoundaryClosureReport& report) {
                   return is_valid(report);
               })
        && std::all_of(result.builtin_derive_expansion_admissions.begin(),
               result.builtin_derive_expansion_admissions.end(),
               [](const BuiltinDeriveExpansionAdmissionGate& gate) {
                   return is_valid(gate);
               })
        && std::all_of(result.builtin_derive_semantic_plans.begin(),
               result.builtin_derive_semantic_plans.end(),
               [](const BuiltinDeriveSemanticExpansionPlan& plan) {
                   return is_valid(plan);
               })
        && std::all_of(result.builtin_derive_parser_release_gates.begin(),
               result.builtin_derive_parser_release_gates.end(),
               [](const BuiltinDeriveParserConsumptionReleaseGate& gate) {
                   return is_valid(gate);
               })
        && std::all_of(result.builtin_derive_release_hardening_matrices.begin(),
               result.builtin_derive_release_hardening_matrices.end(),
               [](const BuiltinDeriveReleaseHardeningMatrix& matrix) {
                   return is_valid(matrix);
               })
        && std::all_of(result.builtin_derive_debug_dump_contracts.begin(),
               result.builtin_derive_debug_dump_contracts.end(),
               [](const BuiltinDeriveDebugDumpStabilityContract& contract) {
                   return is_valid(contract);
               })
        && std::all_of(result.builtin_derive_rollback_diagnostic_gates.begin(),
               result.builtin_derive_rollback_diagnostic_gates.end(),
               [](const BuiltinDeriveRollbackDiagnosticDesignGate& gate) {
                   return is_valid(gate);
               })
        && std::all_of(result.builtin_derive_parser_consumption_admission_protocols.begin(),
               result.builtin_derive_parser_consumption_admission_protocols.end(),
               [](const BuiltinDeriveParserConsumptionAdmissionProtocol& protocol) {
                   return is_valid(protocol);
               })
        && std::all_of(result.builtin_derive_checkpoint_rollback_protocols.begin(),
               result.builtin_derive_checkpoint_rollback_protocols.end(),
               [](const BuiltinDeriveParserConsumptionCheckpointRollbackProtocol& protocol) {
                   return is_valid(protocol);
               })
        && std::all_of(result.builtin_derive_preconsumption_verification_closures.begin(),
               result.builtin_derive_preconsumption_verification_closures.end(),
               [](const BuiltinDeriveParserPreConsumptionVerificationClosure& closure) {
                   return is_valid(closure);
               })
        && std::all_of(result.builtin_derive_controlled_dry_run_adapters.begin(),
               result.builtin_derive_controlled_dry_run_adapters.end(),
               [](const BuiltinDeriveControlledParserDryRunAdapter& adapter) {
                   return is_valid(adapter);
               })
        && std::all_of(result.builtin_derive_dry_run_rollback_replays.begin(),
               result.builtin_derive_dry_run_rollback_replays.end(),
               [](const BuiltinDeriveDryRunRollbackDiagnosticReplay& replay) {
                   return is_valid(replay);
               })
        && std::all_of(result.builtin_derive_dry_run_negative_matrices.begin(),
               result.builtin_derive_dry_run_negative_matrices.end(),
               [](const BuiltinDeriveDryRunNegativeMatrixClosure& matrix) {
                   return is_valid(matrix);
               })
        && std::all_of(result.builtin_derive_parser_dry_run_sessions.begin(),
               result.builtin_derive_parser_dry_run_sessions.end(),
               [](const BuiltinDeriveParserDryRunSessionBoundary& session) {
                   return is_valid(session);
               })
        && std::all_of(result.builtin_derive_token_cursor_snapshot_proofs.begin(),
               result.builtin_derive_token_cursor_snapshot_proofs.end(),
               [](const BuiltinDeriveTokenCursorSnapshotRollbackProof& proof) {
                   return is_valid(proof);
               })
        && std::all_of(result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures.begin(),
               result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures.end(),
               [](const BuiltinDeriveDiagnosticShadowNoAstMutationClosure& closure) {
                   return is_valid(closure);
               })
        && std::all_of(result.builtin_derive_parser_dry_run_admission_gates.begin(),
               result.builtin_derive_parser_dry_run_admission_gates.end(),
               [](const BuiltinDeriveParserDryRunAdmissionGate& gate) {
                   return is_valid(gate);
               })
        && std::all_of(result.builtin_derive_error_recovery_shadow_diagnostic_gates.begin(),
               result.builtin_derive_error_recovery_shadow_diagnostic_gates.end(),
               [](const BuiltinDeriveErrorRecoveryShadowDiagnosticGate& gate) {
                   return is_valid(gate);
               })
        && std::all_of(result.builtin_derive_cursor_rollback_ast_mutation_verifier_closures.begin(),
               result.builtin_derive_cursor_rollback_ast_mutation_verifier_closures.end(),
               [](const BuiltinDeriveCursorRollbackAstMutationVerifierClosure& closure) {
                   return is_valid(closure);
               })
        && std::all_of(result.aurex_macro_surface_admission_gates.begin(),
               result.aurex_macro_surface_admission_gates.end(),
               [](const AurexMacroSurfaceAdmissionGate& gate) {
                   return is_valid(gate);
               })
        && std::all_of(result.aurex_macro_definition_site_hygiene_gates.begin(),
               result.aurex_macro_definition_site_hygiene_gates.end(),
               [](const AurexMacroDefinitionSiteHygieneAdmissionGate& gate) {
                   return is_valid(gate);
               })
        && std::all_of(result.aurex_macro_typed_matcher_admission_gates.begin(),
               result.aurex_macro_typed_matcher_admission_gates.end(),
               [](const AurexMacroTypedMatcherAdmissionGate& gate) {
                   return is_valid(gate);
               })
        && per_input_stubs_match_inputs(result)
        && generated_token_records_match_buffers(result)
        && parser_admission_report_entries_match_diagnostics(result)
        && parser_admission_reports_match_groups(result)
        && parser_readiness_preflight_entries_match_inputs(result)
        && parser_consumption_contract_gates_match_groups(result)
        && macro_boundary_closure_reports_match_result(result)
        && builtin_derive_expansion_admissions_match_inputs(result)
        && builtin_derive_semantic_plans_match_inputs(result)
        && builtin_derive_parser_release_gates_match_groups(result)
        && builtin_derive_release_hardening_matrices_match_groups(result)
        && builtin_derive_debug_dump_contracts_match_groups(result)
        && builtin_derive_rollback_diagnostic_gates_match_groups(result)
        && builtin_derive_parser_consumption_admission_protocols_match_groups(result)
        && builtin_derive_checkpoint_rollback_protocols_match_groups(result)
        && builtin_derive_preconsumption_verification_closures_match_groups(result)
        && builtin_derive_controlled_dry_run_adapters_match_groups(result)
        && builtin_derive_dry_run_rollback_replays_match_groups(result)
        && builtin_derive_dry_run_negative_matrices_match_groups(result)
        && builtin_derive_parser_dry_run_sessions_match_groups(result)
        && builtin_derive_token_cursor_snapshot_proofs_match_groups(result)
        && builtin_derive_diagnostic_shadow_closures_match_groups(result)
        && builtin_derive_parser_dry_run_admission_gates_match_groups(result)
        && builtin_derive_error_recovery_shadow_diagnostic_gates_match_groups(result)
        && builtin_derive_cursor_rollback_ast_mutation_verifier_closures_match_groups(result)
        && result.aurex_macro_surface_source_item_count
            == static_cast<base::u64>(result.aurex_macro_surface_admission_gates.size())
        && aurex_macro_hygiene_gates_match_surfaces(result)
        && aurex_macro_typed_matcher_gates_match_surfaces(result)
        && is_valid(result.summary, result)
        && result.fingerprint == early_item_expansion_fingerprint(result);
}

EarlyItemExpansionSummary summarize_early_item_expansion_counts(
    const EarlyItemExpansionResult& result) noexcept
{
    EarlyItemExpansionSummary summary;
    summary.macro_input_count = static_cast<base::u64>(result.inputs.size());
    summary.attribute_input_count = static_cast<base::u64>(result.inputs.size());
    for (const EarlyItemMacroInput& input : result.inputs) {
        switch (input.disposition) {
            case EarlyItemExpansionDisposition::builtin_derive_passthrough:
                ++summary.builtin_derive_passthrough_count;
                break;
            case EarlyItemExpansionDisposition::blocked_unimplemented_attribute:
                ++summary.blocked_attribute_count;
                break;
        }
    }
    summary.generated_part_placeholder_count = static_cast<base::u64>(result.generated_parts.size());
    for (const GeneratedModulePartPlaceholder& part : result.generated_parts) {
        if (part.parsed) {
            ++summary.parsed_generated_part_count;
        }
        if (part.merged) {
            ++summary.merged_generated_part_count;
        }
        if (part.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.generated_part_stub_count = static_cast<base::u64>(result.generated_part_stubs.size());
    for (const GeneratedModulePartParseMergeStub& stub : result.generated_part_stubs) {
        if (stub.materialized_buffer) {
            ++summary.materialized_buffer_stub_count;
        }
        if (stub.lifecycle_state == GeneratedModulePartLifecycleState::parse_blocked
            || stub.lifecycle_state == GeneratedModulePartLifecycleState::merge_blocked) {
            ++summary.parse_blocked_count;
        }
        if (stub.lifecycle_state == GeneratedModulePartLifecycleState::merge_blocked) {
            ++summary.merge_blocked_count;
        }
        if (stub.sema_visible) {
            ++summary.sema_visible_generated_part_count;
        }
        if (stub.parsed) {
            ++summary.parsed_generated_part_count;
        }
        if (stub.merged) {
            ++summary.merged_generated_part_count;
        }
        if (stub.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.source_map_placeholder_count = static_cast<base::u64>(result.source_maps.size());
    summary.hygiene_stub_count = static_cast<base::u64>(result.hygiene_stubs.size());
    for (const ExpansionHygieneStub& stub : result.hygiene_stubs) {
        if (!stub.resolved) {
            ++summary.unresolved_hygiene_stub_count;
        }
        if (is_nonzero_fingerprint(stub.declared_name_set)) {
            ++summary.declared_name_stub_count;
        }
        if (stub.captures_call_site_locals) {
            ++summary.call_site_capture_count;
        }
    }
    summary.trace_stub_count = static_cast<base::u64>(result.trace_stubs.size());
    for (const ExpansionTraceStub& stub : result.trace_stubs) {
        if (stub.real_source_map) {
            ++summary.real_source_map_count;
        }
        if (stub.debug_trace_available) {
            ++summary.debug_trace_available_count;
        }
        if (stub.cli_emit_expanded_available) {
            ++summary.cli_emit_expanded_available_count;
        }
    }
    summary.generated_item_declaration_stub_count =
        static_cast<base::u64>(result.generated_item_declarations.size());
    for (const GeneratedItemDeclarationStub& stub : result.generated_item_declarations) {
        if (stub.planned) {
            ++summary.planned_generated_item_declaration_count;
        }
        if (stub.materialized_tokens) {
            ++summary.materialized_generated_item_count;
        }
        if (stub.parsed) {
            ++summary.parsed_generated_part_count;
        }
        if (stub.merged) {
            ++summary.merged_generated_part_count;
        }
        if (stub.sema_visible) {
            ++summary.sema_visible_generated_part_count;
        }
        if (stub.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.declared_generated_name_stub_count =
        static_cast<base::u64>(result.declared_generated_names.size());
    for (const DeclaredGeneratedNameStub& stub : result.declared_generated_names) {
        if (stub.lookup_visible) {
            ++summary.lookup_visible_declared_name_count;
        }
        if (stub.export_visible) {
            ++summary.export_visible_declared_name_count;
        }
        if (stub.sema_visible) {
            ++summary.sema_visible_generated_part_count;
        }
        if (stub.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.token_materialization_admission_stub_count =
        static_cast<base::u64>(result.token_materialization_admissions.size());
    for (const TokenMaterializationAdmissionStub& stub : result.token_materialization_admissions) {
        if (stub.compiler_owned) {
            ++summary.compiler_owned_admission_count;
        }
        if (stub.admitted) {
            ++summary.admitted_token_materialization_count;
        }
        if (stub.materialized_tokens) {
            ++summary.materialized_token_admission_count;
        }
        if (stub.generated_source_text) {
            ++summary.generated_source_text_count;
        }
        if (stub.parse_ready) {
            ++summary.parse_ready_token_buffer_count;
        }
        if (stub.external_process_required) {
            ++summary.external_process_required_count;
        }
        if (stub.standard_library_required) {
            ++summary.standard_library_required_count;
        }
        if (stub.runtime_required) {
            ++summary.runtime_required_count;
        }
        if (stub.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.generated_token_buffer_stub_count =
        static_cast<base::u64>(result.generated_token_buffers.size());
    for (const GeneratedTokenBufferStub& stub : result.generated_token_buffers) {
        if (token_buffer_kind_is_compiler_owned(stub.token_buffer_kind)
            && token_producer_policy_is_compiler_owned(stub.token_producer_policy)) {
            ++summary.compiler_owned_token_buffer_count;
        }
        if (stub.empty) {
            ++summary.empty_generated_token_buffer_count;
        }
        if (stub.materialized_tokens) {
            ++summary.materialized_token_buffer_count;
        }
        if (stub.generated_source_text) {
            ++summary.generated_source_text_count;
        }
        if (stub.parser_consumable) {
            ++summary.parse_ready_token_buffer_count;
        }
        if (stub.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.generated_token_record_count = static_cast<base::u64>(result.generated_token_records.size());
    for (const GeneratedTokenRecord& record : result.generated_token_records) {
        if (record.compiler_owned) {
            ++summary.compiler_owned_generated_token_record_count;
        }
        if (record.parser_visible) {
            ++summary.parser_visible_generated_token_count;
        }
        if (record.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.parser_admission_gate_stub_count = static_cast<base::u64>(result.parser_admission_gates.size());
    for (const GeneratedTokenParserAdmissionGateStub& stub : result.parser_admission_gates) {
        if (stub.compiler_owned) {
            ++summary.compiler_owned_parser_admission_gate_count;
        }
        if (stub.token_records_available) {
            ++summary.token_record_available_gate_count;
        }
        if (!stub.parser_admitted) {
            ++summary.parser_blocked_token_buffer_count;
        }
        if (stub.parser_admitted) {
            ++summary.parser_admitted_token_buffer_count;
        }
        if (stub.generated_source_text) {
            ++summary.generated_source_text_count;
        }
        if (stub.parse_ready || stub.parser_consumable) {
            ++summary.parse_ready_token_buffer_count;
        }
        if (stub.generated_part_parsed) {
            ++summary.parsed_generated_part_count;
        }
        if (stub.generated_part_merged) {
            ++summary.merged_generated_part_count;
        }
        if (stub.sema_visible) {
            ++summary.sema_visible_generated_part_count;
        }
        if (stub.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.parser_admission_diagnostic_stub_count =
        static_cast<base::u64>(result.parser_admission_diagnostics.size());
    for (const ParserAdmissionDiagnosticProjectionStub& stub : result.parser_admission_diagnostics) {
        if (!stub.parser_admitted) {
            ++summary.parser_admission_diagnostic_blocked_count;
        }
        if (stub.blocker_category == FRONTEND_MACRO_M21K_DERIVE_BLOCKER_CATEGORY) {
            ++summary.derive_parser_admission_diagnostic_count;
        }
        if (stub.blocker_category == FRONTEND_MACRO_M21K_EMPTY_BLOCKER_CATEGORY) {
            ++summary.empty_parser_admission_diagnostic_count;
        }
        if (stub.emit_expanded_available) {
            ++summary.emit_expanded_projection_available_count;
        }
        if (stub.debug_trace_available) {
            ++summary.parser_admission_debug_trace_projection_count;
        }
        if (stub.source_map_available) {
            ++summary.parser_admission_source_map_projection_count;
        }
        if (stub.parse_ready || stub.parser_consumable) {
            ++summary.parse_ready_token_buffer_count;
        }
        if (stub.generated_part_parsed) {
            ++summary.parsed_generated_part_count;
        }
        if (stub.generated_part_merged) {
            ++summary.merged_generated_part_count;
        }
        if (stub.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.parser_admission_report_entry_count =
        static_cast<base::u64>(result.parser_admission_report_entries.size());
    for (const ParserAdmissionDiagnosticReportEntry& entry : result.parser_admission_report_entries) {
        if (!entry.parser_admitted) {
            ++summary.parser_admission_report_blocked_entry_count;
        }
        if (entry.blocker_category == FRONTEND_MACRO_M21K_DERIVE_BLOCKER_CATEGORY) {
            ++summary.parser_admission_report_derive_entry_count;
        }
        if (entry.blocker_category == FRONTEND_MACRO_M21K_EMPTY_BLOCKER_CATEGORY) {
            ++summary.parser_admission_report_empty_entry_count;
        }
        if (entry.token_records_available) {
            ++summary.parser_admission_report_token_record_available_entry_count;
        }
        if (entry.emit_expanded_available) {
            ++summary.emit_expanded_projection_available_count;
        }
        if (entry.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.parser_admission_report_count =
        static_cast<base::u64>(result.parser_admission_reports.size());
    for (const ParserAdmissionDiagnosticReport& report : result.parser_admission_reports) {
        if (report.report_visible) {
            ++summary.parser_admission_report_visible_count;
        }
        if (report.query_reusable) {
            ++summary.parser_admission_report_query_reusable_count;
        }
        if (!report.source_anchor_ordered) {
            ++summary.parser_admission_report_unordered_anchor_count;
        }
        if (report.parser_consumable) {
            ++summary.parser_admission_report_parser_consumable_count;
            ++summary.parse_ready_token_buffer_count;
        }
        if (report.parse_ready) {
            ++summary.parse_ready_token_buffer_count;
        }
        if (report.emit_expanded_available) {
            ++summary.emit_expanded_projection_available_count;
        }
        if (report.debug_trace_available) {
            ++summary.parser_admission_debug_trace_projection_count;
        }
        if (report.source_map_available) {
            ++summary.parser_admission_source_map_projection_count;
        }
        if (report.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.parser_readiness_preflight_entry_count =
        static_cast<base::u64>(result.parser_readiness_preflight_entries.size());
    for (const GeneratedTokenParserReadinessPreflightEntry& entry :
        result.parser_readiness_preflight_entries) {
        if (!entry.parser_admitted) {
            ++summary.parser_readiness_preflight_blocked_count;
        }
        if (entry.token_stream_shape == FRONTEND_MACRO_M21M_DERIVE_TOKEN_STREAM_SHAPE) {
            ++summary.parser_readiness_preflight_derive_entry_count;
        }
        if (entry.token_stream_shape == FRONTEND_MACRO_M21M_EMPTY_TOKEN_STREAM_SHAPE) {
            ++summary.parser_readiness_preflight_empty_entry_count;
        }
        if (entry.token_indices_contiguous) {
            ++summary.parser_readiness_preflight_contiguous_index_count;
        }
        if (entry.delimiter_balanced) {
            ++summary.parser_readiness_preflight_delimiter_balanced_count;
        }
        if (entry.source_anchors_covered) {
            ++summary.parser_readiness_preflight_source_anchor_covered_count;
        }
        if (entry.parse_config_compatible) {
            ++summary.parser_readiness_preflight_parse_config_compatible_count;
        }
        if (entry.parser_consumable) {
            ++summary.parser_readiness_preflight_parser_consumable_count;
            ++summary.parse_ready_token_buffer_count;
        }
        if (entry.parse_ready) {
            ++summary.parse_ready_token_buffer_count;
        }
        if (entry.generated_part_parsed) {
            ++summary.parsed_generated_part_count;
        }
        if (entry.generated_part_merged) {
            ++summary.merged_generated_part_count;
        }
        if (entry.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.parser_consumption_contract_gate_count =
        static_cast<base::u64>(result.parser_consumption_contract_gates.size());
    for (const GeneratedTokenParserConsumptionContractGate& gate :
        result.parser_consumption_contract_gates) {
        if (!gate.parser_admitted) {
            ++summary.parser_consumption_contract_blocked_gate_count;
        }
        if (gate.contract_visible) {
            ++summary.parser_consumption_contract_visible_count;
        }
        if (gate.query_reusable) {
            ++summary.parser_consumption_contract_query_reusable_count;
        }
        if (gate.parser_consumable) {
            ++summary.parser_consumption_contract_parser_consumable_count;
            ++summary.parse_ready_token_buffer_count;
        }
        if (gate.parse_ready) {
            ++summary.parse_ready_token_buffer_count;
        }
        if (gate.generated_part_parsed) {
            ++summary.parsed_generated_part_count;
        }
        if (gate.generated_part_merged) {
            ++summary.merged_generated_part_count;
        }
        if (gate.sema_visible) {
            ++summary.sema_visible_generated_part_count;
        }
        if (gate.emit_expanded_available) {
            ++summary.emit_expanded_projection_available_count;
        }
        if (gate.debug_trace_available) {
            ++summary.parser_admission_debug_trace_projection_count;
        }
        if (gate.source_map_available) {
            ++summary.parser_admission_source_map_projection_count;
        }
        if (gate.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.macro_boundary_closure_report_count =
        static_cast<base::u64>(result.macro_boundary_closure_reports.size());
    for (const MacroExpansionBoundaryClosureReport& report :
        result.macro_boundary_closure_reports) {
        if (report.closure_visible) {
            ++summary.macro_boundary_closure_visible_count;
        }
        if (report.query_reusable) {
            ++summary.macro_boundary_closure_query_reusable_count;
        }
        if (report.release_closure_complete) {
            ++summary.macro_boundary_closure_complete_count;
        }
        if (report.parser_consumption_enabled) {
            ++summary.macro_boundary_closure_parser_consumption_enabled_count;
            ++summary.parse_ready_token_buffer_count;
        }
        if (report.emit_expanded_available) {
            ++summary.emit_expanded_projection_available_count;
        }
        if (report.debug_trace_available) {
            ++summary.parser_admission_debug_trace_projection_count;
        }
        if (report.source_map_available) {
            ++summary.parser_admission_source_map_projection_count;
        }
        if (report.standard_library_required) {
            ++summary.standard_library_required_count;
        }
        if (report.runtime_required) {
            ++summary.runtime_required_count;
        }
        if (report.external_process_required) {
            ++summary.external_process_required_count;
        }
        if (report.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.builtin_derive_expansion_admission_gate_count =
        static_cast<base::u64>(result.builtin_derive_expansion_admissions.size());
    for (const BuiltinDeriveExpansionAdmissionGate& gate :
        result.builtin_derive_expansion_admissions) {
        if (gate.builtin_derive_input) {
            ++summary.builtin_derive_expansion_derive_admission_count;
        }
        if (gate.admission_kind == FRONTEND_MACRO_M22A_NON_DERIVE_BLOCKED_KIND) {
            ++summary.builtin_derive_expansion_non_derive_blocked_count;
        }
        if (gate.admission_visible) {
            ++summary.builtin_derive_expansion_visible_count;
        }
        if (gate.query_reusable) {
            ++summary.builtin_derive_expansion_query_reusable_count;
        }
        summary.builtin_derive_expansion_capability_candidate_count +=
            gate.capability_candidate_count;
        if (gate.parser_consumption_enabled) {
            ++summary.parse_ready_token_buffer_count;
        }
        if (gate.external_process_required) {
            ++summary.external_process_required_count;
        }
        if (gate.standard_library_required) {
            ++summary.standard_library_required_count;
        }
        if (gate.runtime_required) {
            ++summary.runtime_required_count;
        }
        if (gate.generated_source_text) {
            ++summary.generated_source_text_count;
        }
        if (gate.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.builtin_derive_semantic_plan_count =
        static_cast<base::u64>(result.builtin_derive_semantic_plans.size());
    for (const BuiltinDeriveSemanticExpansionPlan& plan :
        result.builtin_derive_semantic_plans) {
        if (plan.plan_visible) {
            ++summary.builtin_derive_semantic_plan_visible_count;
        }
        if (plan.query_reusable) {
            ++summary.builtin_derive_semantic_plan_query_reusable_count;
        }
        summary.builtin_derive_semantic_capability_count += plan.capability_count;
        summary.builtin_derive_semantic_copy_capability_count += plan.copy_capability_count;
        summary.builtin_derive_semantic_eq_capability_count += plan.eq_capability_count;
        summary.builtin_derive_semantic_hash_capability_count += plan.hash_capability_count;
        if (plan.requires_standard_library) {
            ++summary.standard_library_required_count;
        }
        if (plan.requires_runtime) {
            ++summary.runtime_required_count;
        }
        if (plan.external_process_required) {
            ++summary.external_process_required_count;
        }
        if (plan.parser_consumption_enabled) {
            ++summary.parse_ready_token_buffer_count;
        }
        if (plan.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.builtin_derive_parser_release_gate_count =
        static_cast<base::u64>(result.builtin_derive_parser_release_gates.size());
    for (const BuiltinDeriveParserConsumptionReleaseGate& gate :
        result.builtin_derive_parser_release_gates) {
        if (gate.release_visible) {
            ++summary.builtin_derive_parser_release_visible_count;
        }
        if (gate.query_reusable) {
            ++summary.builtin_derive_parser_release_query_reusable_count;
        }
        if (gate.parser_consumable_contract_count > 0U || gate.parser_consumption_enabled) {
            ++summary.builtin_derive_parser_release_parser_consumable_count;
            ++summary.parse_ready_token_buffer_count;
        }
        if (gate.generated_part_parsed) {
            ++summary.parsed_generated_part_count;
        }
        if (gate.generated_part_merged) {
            ++summary.merged_generated_part_count;
        }
        if (gate.emit_expanded_available) {
            ++summary.emit_expanded_projection_available_count;
        }
        if (gate.debug_trace_available) {
            ++summary.parser_admission_debug_trace_projection_count;
        }
        if (gate.source_map_available) {
            ++summary.parser_admission_source_map_projection_count;
        }
        if (gate.standard_library_required) {
            ++summary.standard_library_required_count;
        }
        if (gate.runtime_required) {
            ++summary.runtime_required_count;
        }
        if (gate.external_process_required) {
            ++summary.external_process_required_count;
        }
        if (gate.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.builtin_derive_release_hardening_matrix_count =
        static_cast<base::u64>(result.builtin_derive_release_hardening_matrices.size());
    for (const BuiltinDeriveReleaseHardeningMatrix& matrix :
        result.builtin_derive_release_hardening_matrices) {
        if (matrix.matrix_visible) {
            ++summary.builtin_derive_release_hardening_visible_count;
        }
        if (matrix.query_reusable) {
            ++summary.builtin_derive_release_hardening_query_reusable_count;
        }
        if (matrix.negative_matrix_complete) {
            ++summary.builtin_derive_release_hardening_negative_matrix_complete_count;
        }
        if (!matrix.release_remains_blocked || matrix.parser_consumption_enabled) {
            ++summary.builtin_derive_release_hardening_parser_consumable_count;
            ++summary.parse_ready_token_buffer_count;
        }
        if (matrix.generated_part_parsed) {
            ++summary.parsed_generated_part_count;
        }
        if (matrix.generated_part_merged) {
            ++summary.merged_generated_part_count;
        }
        if (matrix.emit_expanded_available) {
            ++summary.emit_expanded_projection_available_count;
        }
        if (matrix.debug_trace_available) {
            ++summary.parser_admission_debug_trace_projection_count;
        }
        if (matrix.source_map_available) {
            ++summary.parser_admission_source_map_projection_count;
        }
        if (matrix.standard_library_required) {
            ++summary.standard_library_required_count;
        }
        if (matrix.runtime_required) {
            ++summary.runtime_required_count;
        }
        if (matrix.external_process_required) {
            ++summary.external_process_required_count;
        }
        if (matrix.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.builtin_derive_debug_dump_contract_count =
        static_cast<base::u64>(result.builtin_derive_debug_dump_contracts.size());
    for (const BuiltinDeriveDebugDumpStabilityContract& contract :
        result.builtin_derive_debug_dump_contracts) {
        if (contract.contract_visible) {
            ++summary.builtin_derive_debug_dump_contract_visible_count;
        }
        if (contract.query_reusable) {
            ++summary.builtin_derive_debug_dump_query_reusable_count;
        }
        if (contract.debug_dump_contract_complete) {
            ++summary.builtin_derive_debug_dump_complete_count;
        }
        if (contract.parser_consumption_enabled) {
            ++summary.builtin_derive_debug_dump_parser_consumable_count;
            ++summary.parse_ready_token_buffer_count;
        }
        if (contract.emit_expanded_available) {
            ++summary.emit_expanded_projection_available_count;
        }
        if (contract.debug_trace_available) {
            ++summary.parser_admission_debug_trace_projection_count;
        }
        if (contract.source_map_available) {
            ++summary.parser_admission_source_map_projection_count;
        }
        if (contract.standard_library_required) {
            ++summary.standard_library_required_count;
        }
        if (contract.runtime_required) {
            ++summary.runtime_required_count;
        }
        if (contract.external_process_required) {
            ++summary.external_process_required_count;
        }
        if (contract.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.builtin_derive_rollback_diagnostic_gate_count =
        static_cast<base::u64>(result.builtin_derive_rollback_diagnostic_gates.size());
    for (const BuiltinDeriveRollbackDiagnosticDesignGate& gate :
        result.builtin_derive_rollback_diagnostic_gates) {
        if (gate.rollback_gate_visible) {
            ++summary.builtin_derive_rollback_diagnostic_visible_count;
        }
        if (gate.query_reusable) {
            ++summary.builtin_derive_rollback_diagnostic_query_reusable_count;
        }
        if (gate.rollback_diagnostic_design_available
            && gate.diagnostic_grouping_available
            && gate.source_anchor_available
            && gate.token_tree_anchor_available
            && gate.debug_dump_contract_available
            && gate.release_rollback_plan_complete) {
            ++summary.builtin_derive_rollback_diagnostic_design_complete_count;
        }
        if (gate.rollback_execution_enabled || gate.parser_consumption_enabled) {
            ++summary.builtin_derive_rollback_diagnostic_parser_consumable_count;
            ++summary.parse_ready_token_buffer_count;
        }
        if (gate.generated_part_parsed) {
            ++summary.parsed_generated_part_count;
        }
        if (gate.generated_part_merged) {
            ++summary.merged_generated_part_count;
        }
        if (gate.emit_expanded_available) {
            ++summary.emit_expanded_projection_available_count;
        }
        if (gate.debug_trace_available) {
            ++summary.parser_admission_debug_trace_projection_count;
        }
        if (gate.source_map_available) {
            ++summary.parser_admission_source_map_projection_count;
        }
        if (gate.standard_library_required) {
            ++summary.standard_library_required_count;
        }
        if (gate.runtime_required) {
            ++summary.runtime_required_count;
        }
        if (gate.external_process_required) {
            ++summary.external_process_required_count;
        }
        if (gate.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.builtin_derive_parser_consumption_admission_protocol_count =
        static_cast<base::u64>(result.builtin_derive_parser_consumption_admission_protocols.size());
    for (const BuiltinDeriveParserConsumptionAdmissionProtocol& protocol :
        result.builtin_derive_parser_consumption_admission_protocols) {
        if (protocol.protocol_visible) {
            ++summary.builtin_derive_parser_consumption_admission_visible_count;
        }
        if (protocol.query_reusable) {
            ++summary.builtin_derive_parser_consumption_admission_query_reusable_count;
        }
        if (protocol.release_gate_available
            && protocol.rollback_gate_available
            && protocol.parser_contract_available
            && protocol.deterministic_order_available
            && protocol.generated_tokens_checkpointed
            && protocol.admission_protocol_complete) {
            ++summary.builtin_derive_parser_consumption_admission_complete_count;
        }
        if (protocol.parser_consumption_enabled || protocol.parser_admitted) {
            ++summary.builtin_derive_parser_consumption_admission_parser_consumable_count;
            ++summary.parse_ready_token_buffer_count;
        }
        if (protocol.generated_part_parsed) {
            ++summary.parsed_generated_part_count;
        }
        if (protocol.generated_part_merged) {
            ++summary.merged_generated_part_count;
        }
        if (protocol.emit_expanded_available) {
            ++summary.emit_expanded_projection_available_count;
        }
        if (protocol.debug_trace_available) {
            ++summary.parser_admission_debug_trace_projection_count;
        }
        if (protocol.source_map_available) {
            ++summary.parser_admission_source_map_projection_count;
        }
        if (protocol.standard_library_required) {
            ++summary.standard_library_required_count;
        }
        if (protocol.runtime_required) {
            ++summary.runtime_required_count;
        }
        if (protocol.external_process_required) {
            ++summary.external_process_required_count;
        }
        if (protocol.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.builtin_derive_checkpoint_rollback_protocol_count =
        static_cast<base::u64>(result.builtin_derive_checkpoint_rollback_protocols.size());
    for (const BuiltinDeriveParserConsumptionCheckpointRollbackProtocol& protocol :
        result.builtin_derive_checkpoint_rollback_protocols) {
        if (protocol.protocol_visible) {
            ++summary.builtin_derive_checkpoint_rollback_visible_count;
        }
        if (protocol.query_reusable) {
            ++summary.builtin_derive_checkpoint_rollback_query_reusable_count;
        }
        if (protocol.parser_state_checkpoint_available
            && protocol.token_cursor_checkpoint_available
            && protocol.generated_part_checkpoint_available
            && protocol.diagnostic_replay_available
            && protocol.rollback_protocol_complete) {
            ++summary.builtin_derive_checkpoint_rollback_complete_count;
        }
        if (protocol.rollback_execution_enabled || protocol.parser_consumption_enabled) {
            ++summary.builtin_derive_checkpoint_rollback_parser_consumable_count;
            ++summary.parse_ready_token_buffer_count;
        }
        if (protocol.generated_part_parsed) {
            ++summary.parsed_generated_part_count;
        }
        if (protocol.generated_part_merged) {
            ++summary.merged_generated_part_count;
        }
        if (protocol.emit_expanded_available) {
            ++summary.emit_expanded_projection_available_count;
        }
        if (protocol.debug_trace_available) {
            ++summary.parser_admission_debug_trace_projection_count;
        }
        if (protocol.source_map_available) {
            ++summary.parser_admission_source_map_projection_count;
        }
        if (protocol.standard_library_required) {
            ++summary.standard_library_required_count;
        }
        if (protocol.runtime_required) {
            ++summary.runtime_required_count;
        }
        if (protocol.external_process_required) {
            ++summary.external_process_required_count;
        }
        if (protocol.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.builtin_derive_preconsumption_verification_closure_count =
        static_cast<base::u64>(result.builtin_derive_preconsumption_verification_closures.size());
    for (const BuiltinDeriveParserPreConsumptionVerificationClosure& closure :
        result.builtin_derive_preconsumption_verification_closures) {
        if (closure.closure_visible) {
            ++summary.builtin_derive_preconsumption_verification_visible_count;
        }
        if (closure.query_reusable) {
            ++summary.builtin_derive_preconsumption_verification_query_reusable_count;
        }
        if (closure.admission_protocol_available
            && closure.checkpoint_protocol_available
            && closure.release_hardening_available
            && closure.debug_dump_contract_available
            && closure.rollback_gate_available
            && closure.verification_closure_complete) {
            ++summary.builtin_derive_preconsumption_verification_complete_count;
        }
        if (closure.parser_consumption_enabled) {
            ++summary.builtin_derive_preconsumption_verification_parser_consumable_count;
            ++summary.parse_ready_token_buffer_count;
        }
        if (closure.generated_part_parsed) {
            ++summary.parsed_generated_part_count;
        }
        if (closure.generated_part_merged) {
            ++summary.merged_generated_part_count;
        }
        if (closure.sema_visible) {
            ++summary.sema_visible_generated_part_count;
        }
        if (closure.emit_expanded_available) {
            ++summary.emit_expanded_projection_available_count;
        }
        if (closure.debug_trace_available) {
            ++summary.parser_admission_debug_trace_projection_count;
        }
        if (closure.source_map_available) {
            ++summary.parser_admission_source_map_projection_count;
        }
        if (closure.standard_library_required) {
            ++summary.standard_library_required_count;
        }
        if (closure.runtime_required) {
            ++summary.runtime_required_count;
        }
        if (closure.external_process_required) {
            ++summary.external_process_required_count;
        }
        if (closure.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.builtin_derive_controlled_dry_run_adapter_count =
        static_cast<base::u64>(result.builtin_derive_controlled_dry_run_adapters.size());
    for (const BuiltinDeriveControlledParserDryRunAdapter& adapter :
        result.builtin_derive_controlled_dry_run_adapters) {
        if (adapter.adapter_visible) {
            ++summary.builtin_derive_controlled_dry_run_adapter_visible_count;
        }
        if (adapter.query_reusable) {
            ++summary.builtin_derive_controlled_dry_run_adapter_query_reusable_count;
        }
        if (adapter.verification_closure_available
            && adapter.admission_protocol_available
            && adapter.checkpoint_protocol_available
            && adapter.compiler_owned_tokens_available
            && adapter.diagnostic_replay_available
            && adapter.dry_run_adapter_complete) {
            ++summary.builtin_derive_controlled_dry_run_adapter_complete_count;
        }
        if (adapter.dry_run_executed) {
            ++summary.builtin_derive_controlled_dry_run_adapter_executed_count;
        }
        if (adapter.parser_consumption_enabled || adapter.parser_admitted) {
            ++summary.parse_ready_token_buffer_count;
        }
        if (adapter.parser_admitted) {
            ++summary.parser_admitted_token_buffer_count;
        }
        if (adapter.generated_part_parsed) {
            ++summary.parsed_generated_part_count;
        }
        if (adapter.generated_part_merged) {
            ++summary.merged_generated_part_count;
        }
        if (adapter.sema_visible) {
            ++summary.sema_visible_generated_part_count;
        }
        if (adapter.emit_expanded_available) {
            ++summary.emit_expanded_projection_available_count;
        }
        if (adapter.debug_trace_available) {
            ++summary.parser_admission_debug_trace_projection_count;
        }
        if (adapter.source_map_available) {
            ++summary.parser_admission_source_map_projection_count;
        }
        if (adapter.standard_library_required) {
            ++summary.standard_library_required_count;
        }
        if (adapter.runtime_required) {
            ++summary.runtime_required_count;
        }
        if (adapter.external_process_required) {
            ++summary.external_process_required_count;
        }
        if (adapter.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.builtin_derive_dry_run_rollback_replay_count =
        static_cast<base::u64>(result.builtin_derive_dry_run_rollback_replays.size());
    for (const BuiltinDeriveDryRunRollbackDiagnosticReplay& replay :
        result.builtin_derive_dry_run_rollback_replays) {
        if (replay.replay_visible) {
            ++summary.builtin_derive_dry_run_rollback_replay_visible_count;
        }
        if (replay.query_reusable) {
            ++summary.builtin_derive_dry_run_rollback_replay_query_reusable_count;
        }
        if (replay.dry_run_adapter_available
            && replay.checkpoint_protocol_available
            && replay.rollback_gate_available
            && replay.diagnostic_replay_plan_available
            && replay.replay_protocol_complete) {
            ++summary.builtin_derive_dry_run_rollback_replay_complete_count;
        }
        if (replay.replay_execution_enabled || replay.dry_run_executed
            || replay.executed_replay_count > 0U) {
            ++summary.builtin_derive_dry_run_rollback_replay_executed_count;
        }
        if (replay.parser_consumption_enabled) {
            ++summary.parse_ready_token_buffer_count;
        }
        if (replay.generated_part_parsed) {
            ++summary.parsed_generated_part_count;
        }
        if (replay.generated_part_merged) {
            ++summary.merged_generated_part_count;
        }
        if (replay.sema_visible) {
            ++summary.sema_visible_generated_part_count;
        }
        if (replay.emit_expanded_available) {
            ++summary.emit_expanded_projection_available_count;
        }
        if (replay.debug_trace_available) {
            ++summary.parser_admission_debug_trace_projection_count;
        }
        if (replay.source_map_available) {
            ++summary.parser_admission_source_map_projection_count;
        }
        if (replay.standard_library_required) {
            ++summary.standard_library_required_count;
        }
        if (replay.runtime_required) {
            ++summary.runtime_required_count;
        }
        if (replay.external_process_required) {
            ++summary.external_process_required_count;
        }
        if (replay.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.builtin_derive_dry_run_negative_matrix_count =
        static_cast<base::u64>(result.builtin_derive_dry_run_negative_matrices.size());
    for (const BuiltinDeriveDryRunNegativeMatrixClosure& matrix :
        result.builtin_derive_dry_run_negative_matrices) {
        if (matrix.matrix_visible) {
            ++summary.builtin_derive_dry_run_negative_matrix_visible_count;
        }
        if (matrix.query_reusable) {
            ++summary.builtin_derive_dry_run_negative_matrix_query_reusable_count;
        }
        if (matrix.dry_run_adapter_available
            && matrix.rollback_replay_available
            && matrix.verification_closure_available
            && matrix.negative_matrix_complete) {
            ++summary.builtin_derive_dry_run_negative_matrix_complete_count;
        }
        if (matrix.parser_consumable_case_count > 0U
            || matrix.parser_consumption_enabled
            || matrix.parser_admitted) {
            ++summary.builtin_derive_dry_run_negative_matrix_parser_consumable_count;
            ++summary.parse_ready_token_buffer_count;
        }
        if (matrix.parser_admitted) {
            ++summary.parser_admitted_token_buffer_count;
        }
        if (matrix.generated_part_parsed) {
            ++summary.parsed_generated_part_count;
        }
        if (matrix.generated_part_merged) {
            ++summary.merged_generated_part_count;
        }
        if (matrix.sema_visible) {
            ++summary.sema_visible_generated_part_count;
        }
        if (matrix.emit_expanded_available) {
            ++summary.emit_expanded_projection_available_count;
        }
        if (matrix.debug_trace_available) {
            ++summary.parser_admission_debug_trace_projection_count;
        }
        if (matrix.source_map_available) {
            ++summary.parser_admission_source_map_projection_count;
        }
        if (matrix.standard_library_required) {
            ++summary.standard_library_required_count;
        }
        if (matrix.runtime_required) {
            ++summary.runtime_required_count;
        }
        if (matrix.external_process_required) {
            ++summary.external_process_required_count;
        }
        if (matrix.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.builtin_derive_parser_dry_run_session_count =
        static_cast<base::u64>(result.builtin_derive_parser_dry_run_sessions.size());
    for (const BuiltinDeriveParserDryRunSessionBoundary& session :
        result.builtin_derive_parser_dry_run_sessions) {
        if (session.session_visible) {
            ++summary.builtin_derive_parser_dry_run_session_visible_count;
        }
        if (session.query_reusable) {
            ++summary.builtin_derive_parser_dry_run_session_query_reusable_count;
        }
        if (session.dry_run_adapter_available
            && session.negative_matrix_available
            && session.compiler_owned_token_stream_available
            && session.sandbox_available
            && session.check_only
            && session.dry_run_session_complete) {
            ++summary.builtin_derive_parser_dry_run_session_complete_count;
        }
        if (session.dry_run_executed) {
            ++summary.builtin_derive_parser_dry_run_session_executed_count;
        }
        if (session.session_committed || session.committed_parse_count > 0U) {
            ++summary.builtin_derive_parser_dry_run_session_committed_count;
        }
        if (session.parser_consumption_enabled
            || session.parser_admitted
            || session.parser_cursor_advanced
            || session.session_committed
            || session.committed_parse_count > 0U) {
            ++summary.parse_ready_token_buffer_count;
        }
        if (session.parser_admitted) {
            ++summary.parser_admitted_token_buffer_count;
        }
        if (session.generated_part_parsed) {
            ++summary.parsed_generated_part_count;
        }
        if (session.generated_part_merged) {
            ++summary.merged_generated_part_count;
        }
        if (session.ast_mutated) {
            ++summary.ast_mutation_count;
        }
        if (session.sema_visible) {
            ++summary.sema_visible_generated_part_count;
        }
        if (session.emit_expanded_available) {
            ++summary.emit_expanded_projection_available_count;
        }
        if (session.debug_trace_available) {
            ++summary.parser_admission_debug_trace_projection_count;
        }
        if (session.source_map_available) {
            ++summary.parser_admission_source_map_projection_count;
        }
        if (session.standard_library_required) {
            ++summary.standard_library_required_count;
        }
        if (session.runtime_required) {
            ++summary.runtime_required_count;
        }
        if (session.external_process_required) {
            ++summary.external_process_required_count;
        }
        if (session.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.builtin_derive_token_cursor_snapshot_proof_count =
        static_cast<base::u64>(result.builtin_derive_token_cursor_snapshot_proofs.size());
    for (const BuiltinDeriveTokenCursorSnapshotRollbackProof& proof :
        result.builtin_derive_token_cursor_snapshot_proofs) {
        if (proof.proof_visible) {
            ++summary.builtin_derive_token_cursor_snapshot_proof_visible_count;
        }
        if (proof.query_reusable) {
            ++summary.builtin_derive_token_cursor_snapshot_proof_query_reusable_count;
        }
        if (proof.dry_run_session_available
            && proof.checkpoint_protocol_available
            && proof.rollback_replay_available
            && proof.token_cursor_snapshot_available
            && proof.parser_state_snapshot_available
            && proof.rollback_proof_complete) {
            ++summary.builtin_derive_token_cursor_snapshot_proof_complete_count;
        }
        if (proof.parser_cursor_advanced) {
            ++summary.builtin_derive_token_cursor_snapshot_proof_cursor_advanced_count;
        }
        if (proof.session_committed || proof.cursor_commit_count > 0U) {
            ++summary.builtin_derive_token_cursor_snapshot_proof_committed_count;
        }
        if (proof.parser_consumption_enabled
            || proof.parser_admitted
            || proof.parser_cursor_advanced
            || proof.session_committed
            || proof.cursor_commit_count > 0U) {
            ++summary.parse_ready_token_buffer_count;
        }
        if (proof.parser_admitted) {
            ++summary.parser_admitted_token_buffer_count;
        }
        if (proof.generated_part_parsed) {
            ++summary.parsed_generated_part_count;
        }
        if (proof.generated_part_merged) {
            ++summary.merged_generated_part_count;
        }
        if (proof.ast_mutated) {
            ++summary.ast_mutation_count;
        }
        if (proof.sema_visible) {
            ++summary.sema_visible_generated_part_count;
        }
        if (proof.standard_library_required) {
            ++summary.standard_library_required_count;
        }
        if (proof.runtime_required) {
            ++summary.runtime_required_count;
        }
        if (proof.external_process_required) {
            ++summary.external_process_required_count;
        }
        if (proof.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.builtin_derive_diagnostic_shadow_no_ast_mutation_closure_count =
        static_cast<base::u64>(
            result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures.size());
    for (const BuiltinDeriveDiagnosticShadowNoAstMutationClosure& closure :
        result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures) {
        if (closure.closure_visible) {
            ++summary.builtin_derive_diagnostic_shadow_no_ast_mutation_visible_count;
        }
        if (closure.query_reusable) {
            ++summary.builtin_derive_diagnostic_shadow_no_ast_mutation_query_reusable_count;
        }
        if (closure.dry_run_session_available
            && closure.cursor_snapshot_proof_available
            && closure.rollback_replay_available
            && closure.negative_matrix_available
            && closure.diagnostic_shadow_available
            && closure.no_ast_mutation_verified
            && closure.closure_complete) {
            ++summary.builtin_derive_diagnostic_shadow_no_ast_mutation_complete_count;
        }
        if (closure.dry_run_executed
            || closure.replay_execution_enabled
            || closure.executed_shadow_count > 0U) {
            ++summary.builtin_derive_diagnostic_shadow_no_ast_mutation_executed_count;
        }
        if (closure.ast_mutated || closure.ast_mutation_count > 0U) {
            ++summary.builtin_derive_diagnostic_shadow_no_ast_mutation_ast_mutation_count;
            ++summary.ast_mutation_count;
        }
        if (closure.parser_consumable_case_count > 0U
            || closure.parser_consumption_enabled
            || closure.parser_admitted
            || closure.parser_cursor_advanced
            || closure.session_committed) {
            ++summary.builtin_derive_diagnostic_shadow_no_ast_mutation_parser_consumable_count;
            ++summary.parse_ready_token_buffer_count;
        }
        if (closure.parser_admitted) {
            ++summary.parser_admitted_token_buffer_count;
        }
        if (closure.generated_part_parsed) {
            ++summary.parsed_generated_part_count;
        }
        if (closure.generated_part_merged) {
            ++summary.merged_generated_part_count;
        }
        if (closure.sema_visible) {
            ++summary.sema_visible_generated_part_count;
        }
        if (closure.emit_expanded_available) {
            ++summary.emit_expanded_projection_available_count;
        }
        if (closure.debug_trace_available) {
            ++summary.parser_admission_debug_trace_projection_count;
        }
        if (closure.source_map_available) {
            ++summary.parser_admission_source_map_projection_count;
        }
        if (closure.standard_library_required) {
            ++summary.standard_library_required_count;
        }
        if (closure.runtime_required) {
            ++summary.runtime_required_count;
        }
        if (closure.external_process_required) {
            ++summary.external_process_required_count;
        }
        if (closure.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.builtin_derive_parser_dry_run_admission_gate_count =
        static_cast<base::u64>(result.builtin_derive_parser_dry_run_admission_gates.size());
    for (const BuiltinDeriveParserDryRunAdmissionGate& gate :
        result.builtin_derive_parser_dry_run_admission_gates) {
        if (gate.gate_visible) {
            ++summary.builtin_derive_parser_dry_run_admission_gate_visible_count;
        }
        if (gate.query_reusable) {
            ++summary.builtin_derive_parser_dry_run_admission_gate_query_reusable_count;
        }
        if (gate.dry_run_session_available
            && gate.cursor_snapshot_proof_available
            && gate.diagnostic_shadow_closure_available
            && gate.generated_buffer_available
            && gate.parse_config_available
            && gate.admission_gate_complete) {
            ++summary.builtin_derive_parser_dry_run_admission_gate_complete_count;
        }
        if (gate.dry_run_execution_admitted || gate.dry_run_execution_admitted_count > 0U) {
            ++summary.builtin_derive_parser_dry_run_admission_gate_execution_admitted_count;
            ++summary.parse_ready_token_buffer_count;
        }
        if (gate.dry_run_executed || gate.diagnostic_shadow_executed) {
            ++summary.builtin_derive_parser_dry_run_admission_gate_executed_count;
        }
        if (gate.parser_consumable_case_count > 0U
            || gate.parser_consumption_enabled
            || gate.parser_admitted
            || gate.parser_cursor_advanced
            || gate.session_committed) {
            ++summary.builtin_derive_parser_dry_run_admission_gate_parser_consumable_count;
            ++summary.parse_ready_token_buffer_count;
        }
        if (gate.parser_admitted) {
            ++summary.parser_admitted_token_buffer_count;
        }
        if (gate.generated_part_parsed) {
            ++summary.parsed_generated_part_count;
        }
        if (gate.generated_part_merged) {
            ++summary.merged_generated_part_count;
        }
        if (gate.ast_mutated) {
            ++summary.ast_mutation_count;
        }
        if (gate.sema_visible) {
            ++summary.sema_visible_generated_part_count;
        }
        if (gate.emit_expanded_available) {
            ++summary.emit_expanded_projection_available_count;
        }
        if (gate.debug_trace_available) {
            ++summary.parser_admission_debug_trace_projection_count;
        }
        if (gate.source_map_available) {
            ++summary.parser_admission_source_map_projection_count;
        }
        if (gate.standard_library_required) {
            ++summary.standard_library_required_count;
        }
        if (gate.runtime_required) {
            ++summary.runtime_required_count;
        }
        if (gate.external_process_required) {
            ++summary.external_process_required_count;
        }
        if (gate.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.builtin_derive_error_recovery_shadow_diagnostic_gate_count =
        static_cast<base::u64>(
            result.builtin_derive_error_recovery_shadow_diagnostic_gates.size());
    for (const BuiltinDeriveErrorRecoveryShadowDiagnosticGate& gate :
        result.builtin_derive_error_recovery_shadow_diagnostic_gates) {
        if (gate.gate_visible) {
            ++summary.builtin_derive_error_recovery_shadow_diagnostic_gate_visible_count;
        }
        if (gate.query_reusable) {
            ++summary.builtin_derive_error_recovery_shadow_diagnostic_gate_query_reusable_count;
        }
        if (gate.dry_run_admission_gate_available
            && gate.diagnostic_shadow_closure_available
            && gate.rollback_replay_available
            && gate.parser_report_available
            && gate.recovery_shadow_plan_available
            && gate.recovery_shadow_complete) {
            ++summary.builtin_derive_error_recovery_shadow_diagnostic_gate_complete_count;
        }
        if (gate.recovery_execution_enabled || gate.executed_recovery_count > 0U) {
            ++summary.builtin_derive_error_recovery_shadow_diagnostic_gate_recovery_executed_count;
        }
        if (gate.diagnostic_emission_enabled || gate.emitted_diagnostic_count > 0U) {
            ++summary.builtin_derive_error_recovery_shadow_diagnostic_gate_diagnostic_emitted_count;
        }
        if (gate.parser_consumption_enabled
            || gate.parser_admitted
            || gate.parser_cursor_advanced
            || gate.session_committed) {
            ++summary.builtin_derive_error_recovery_shadow_diagnostic_gate_parser_consumable_count;
            ++summary.parse_ready_token_buffer_count;
        }
        if (gate.parser_admitted) {
            ++summary.parser_admitted_token_buffer_count;
        }
        if (gate.generated_part_parsed) {
            ++summary.parsed_generated_part_count;
        }
        if (gate.generated_part_merged) {
            ++summary.merged_generated_part_count;
        }
        if (gate.ast_mutated) {
            ++summary.ast_mutation_count;
        }
        if (gate.sema_visible) {
            ++summary.sema_visible_generated_part_count;
        }
        if (gate.emit_expanded_available) {
            ++summary.emit_expanded_projection_available_count;
        }
        if (gate.debug_trace_available) {
            ++summary.parser_admission_debug_trace_projection_count;
        }
        if (gate.source_map_available) {
            ++summary.parser_admission_source_map_projection_count;
        }
        if (gate.standard_library_required) {
            ++summary.standard_library_required_count;
        }
        if (gate.runtime_required) {
            ++summary.runtime_required_count;
        }
        if (gate.external_process_required) {
            ++summary.external_process_required_count;
        }
        if (gate.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.builtin_derive_cursor_rollback_ast_mutation_verifier_closure_count =
        static_cast<base::u64>(
            result.builtin_derive_cursor_rollback_ast_mutation_verifier_closures.size());
    for (const BuiltinDeriveCursorRollbackAstMutationVerifierClosure& closure :
        result.builtin_derive_cursor_rollback_ast_mutation_verifier_closures) {
        if (closure.closure_visible) {
            ++summary.builtin_derive_cursor_rollback_ast_mutation_verifier_visible_count;
        }
        if (closure.query_reusable) {
            ++summary.builtin_derive_cursor_rollback_ast_mutation_verifier_query_reusable_count;
        }
        if (closure.dry_run_admission_gate_available
            && closure.recovery_shadow_available
            && closure.cursor_snapshot_proof_available
            && closure.dry_run_session_available
            && closure.diagnostic_shadow_closure_available
            && closure.ast_baseline_available
            && closure.rollback_execution_guard_available
            && closure.ast_mutation_verifier_complete) {
            ++summary.builtin_derive_cursor_rollback_ast_mutation_verifier_complete_count;
        }
        if (closure.rollback_execution_enabled
            || closure.recovery_execution_enabled
            || closure.cursor_commit_count > 0U
            || closure.session_commit_count > 0U) {
            ++summary.builtin_derive_cursor_rollback_ast_mutation_verifier_rollback_executed_count;
        }
        if (closure.ast_mutated || closure.ast_mutation_count > 0U) {
            ++summary.builtin_derive_cursor_rollback_ast_mutation_verifier_ast_mutation_count;
            ++summary.ast_mutation_count;
        }
        if (closure.parser_consumable_case_count > 0U
            || closure.parser_consumption_enabled
            || closure.parser_admitted
            || closure.parser_cursor_advanced
            || closure.session_committed) {
            ++summary.builtin_derive_cursor_rollback_ast_mutation_verifier_parser_consumable_count;
            ++summary.parse_ready_token_buffer_count;
        }
        if (closure.parser_admitted) {
            ++summary.parser_admitted_token_buffer_count;
        }
        if (closure.generated_part_parsed) {
            ++summary.parsed_generated_part_count;
        }
        if (closure.generated_part_merged) {
            ++summary.merged_generated_part_count;
        }
        if (closure.sema_visible) {
            ++summary.sema_visible_generated_part_count;
        }
        if (closure.emit_expanded_available) {
            ++summary.emit_expanded_projection_available_count;
        }
        if (closure.debug_trace_available) {
            ++summary.parser_admission_debug_trace_projection_count;
        }
        if (closure.source_map_available) {
            ++summary.parser_admission_source_map_projection_count;
        }
        if (closure.standard_library_required) {
            ++summary.standard_library_required_count;
        }
        if (closure.runtime_required) {
            ++summary.runtime_required_count;
        }
        if (closure.external_process_required) {
            ++summary.external_process_required_count;
        }
        if (closure.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.aurex_macro_surface_source_item_count = result.aurex_macro_surface_source_item_count;
    summary.aurex_macro_surface_admission_gate_count =
        static_cast<base::u64>(result.aurex_macro_surface_admission_gates.size());
    for (const AurexMacroSurfaceAdmissionGate& gate : result.aurex_macro_surface_admission_gates) {
        if (gate.declarative_surface) {
            ++summary.aurex_macro_declarative_surface_count;
        }
        if (gate.user_derive_surface) {
            ++summary.aurex_macro_user_derive_surface_count;
        }
        if (gate.compile_time_execution_surface) {
            ++summary.aurex_macro_compile_time_surface_count;
        }
        if (gate.gate_visible) {
            ++summary.aurex_macro_surface_visible_count;
        }
        if (gate.query_reusable) {
            ++summary.aurex_macro_surface_query_reusable_count;
        }
        if (gate.body_balanced) {
            ++summary.aurex_macro_surface_body_balanced_count;
        }
        summary.aurex_macro_surface_match_clause_count += gate.match_clause_count;
        if (gate.expansion_enabled) {
            ++summary.aurex_macro_surface_expansion_enabled_count;
        }
        if (gate.compile_time_execution_enabled) {
            ++summary.aurex_macro_surface_compile_time_execution_enabled_count;
        }
        if (gate.parser_consumption_enabled) {
            ++summary.aurex_macro_surface_parser_consumable_count;
            ++summary.parse_ready_token_buffer_count;
        }
        if (gate.ast_mutated) {
            ++summary.ast_mutation_count;
        }
        if (gate.sema_visible_generated_items) {
            ++summary.sema_visible_generated_part_count;
        }
        if (gate.standard_library_required) {
            ++summary.standard_library_required_count;
        }
        if (gate.runtime_required) {
            ++summary.runtime_required_count;
        }
        if (gate.external_process_required) {
            ++summary.external_process_required_count;
        }
        if (gate.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.aurex_macro_definition_site_hygiene_gate_count =
        static_cast<base::u64>(result.aurex_macro_definition_site_hygiene_gates.size());
    for (const AurexMacroDefinitionSiteHygieneAdmissionGate& gate :
        result.aurex_macro_definition_site_hygiene_gates) {
        if (gate.gate_visible) {
            ++summary.aurex_macro_definition_site_hygiene_visible_count;
        }
        if (gate.query_reusable) {
            ++summary.aurex_macro_definition_site_hygiene_query_reusable_count;
        }
        if (gate.definition_site_scope_available) {
            ++summary.aurex_macro_definition_site_scope_available_count;
        }
        if (gate.fresh_name_scope_reserved) {
            ++summary.aurex_macro_fresh_name_scope_reserved_count;
        }
        if (gate.diagnostic_anchor_available) {
            ++summary.aurex_macro_diagnostic_anchor_available_count;
        }
        if (gate.hygiene_resolution_enabled) {
            ++summary.aurex_macro_hygiene_resolution_enabled_count;
        }
        if (gate.declared_names_visible) {
            ++summary.aurex_macro_declared_names_visible_count;
        }
        if (gate.standard_library_required) {
            ++summary.standard_library_required_count;
        }
        if (gate.runtime_required) {
            ++summary.runtime_required_count;
        }
        if (gate.external_process_required) {
            ++summary.external_process_required_count;
        }
        if (gate.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    summary.aurex_macro_typed_matcher_admission_gate_count =
        static_cast<base::u64>(result.aurex_macro_typed_matcher_admission_gates.size());
    for (const AurexMacroTypedMatcherAdmissionGate& gate :
        result.aurex_macro_typed_matcher_admission_gates) {
        if (gate.matcher_shape_recognized) {
            ++summary.aurex_macro_typed_matcher_recognized_count;
        }
        if (gate.expr_list_matcher) {
            ++summary.aurex_macro_expr_list_matcher_count;
        }
        if (gate.item_matcher) {
            ++summary.aurex_macro_item_matcher_count;
        }
        if (gate.token_stream_matcher) {
            ++summary.aurex_macro_token_stream_matcher_count;
        }
        if (gate.unknown_matcher) {
            ++summary.aurex_macro_unknown_matcher_count;
        }
        if (gate.gate_visible) {
            ++summary.aurex_macro_typed_matcher_visible_count;
        }
        if (gate.query_reusable) {
            ++summary.aurex_macro_typed_matcher_query_reusable_count;
        }
        if (gate.diagnostic_anchor_available) {
            ++summary.aurex_macro_diagnostic_anchor_available_count;
        }
        if (gate.matcher_execution_enabled) {
            ++summary.aurex_macro_typed_matcher_execution_enabled_count;
        }
        if (gate.expansion_enabled) {
            ++summary.aurex_macro_surface_expansion_enabled_count;
        }
        if (gate.compile_time_execution_enabled) {
            ++summary.aurex_macro_surface_compile_time_execution_enabled_count;
        }
        if (gate.parser_consumption_enabled) {
            ++summary.aurex_macro_surface_parser_consumable_count;
            ++summary.parse_ready_token_buffer_count;
        }
        if (gate.ast_mutated) {
            ++summary.ast_mutation_count;
        }
        if (gate.sema_visible_generated_items) {
            ++summary.sema_visible_generated_part_count;
        }
        if (gate.standard_library_required) {
            ++summary.standard_library_required_count;
        }
        if (gate.runtime_required) {
            ++summary.runtime_required_count;
        }
        if (gate.external_process_required) {
            ++summary.external_process_required_count;
        }
        if (gate.produced_user_generated_code) {
            ++summary.user_generated_code_count;
        }
    }
    return summary;
}

query::StableFingerprint128 early_item_expansion_fingerprint(
    const EarlyItemExpansionResult& result) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(FRONTEND_MACRO_M26C_EXPANSION_FINGERPRINT_MARKER);
    builder.mix_string(result.name);
    builder.mix_fingerprint(query::macro_expansion_plan_fingerprint(result.plan));
    builder.mix_u64(static_cast<base::u64>(result.inputs.size()));
    for (const EarlyItemMacroInput& input : result.inputs) {
        mix_input(builder, input);
    }
    builder.mix_u64(static_cast<base::u64>(result.generated_parts.size()));
    for (const GeneratedModulePartPlaceholder& part : result.generated_parts) {
        mix_generated_part(builder, part);
    }
    builder.mix_u64(static_cast<base::u64>(result.generated_part_stubs.size()));
    for (const GeneratedModulePartParseMergeStub& stub : result.generated_part_stubs) {
        mix_parse_merge_stub(builder, stub);
    }
    builder.mix_u64(static_cast<base::u64>(result.source_maps.size()));
    for (const ExpansionSourceMapPlaceholder& source_map : result.source_maps) {
        mix_source_map(builder, source_map);
    }
    builder.mix_u64(static_cast<base::u64>(result.hygiene_stubs.size()));
    for (const ExpansionHygieneStub& stub : result.hygiene_stubs) {
        mix_hygiene_stub(builder, stub);
    }
    builder.mix_u64(static_cast<base::u64>(result.trace_stubs.size()));
    for (const ExpansionTraceStub& stub : result.trace_stubs) {
        mix_trace_stub(builder, stub);
    }
    builder.mix_u64(static_cast<base::u64>(result.generated_item_declarations.size()));
    for (const GeneratedItemDeclarationStub& stub : result.generated_item_declarations) {
        mix_generated_item_declaration_stub(builder, stub);
    }
    builder.mix_u64(static_cast<base::u64>(result.declared_generated_names.size()));
    for (const DeclaredGeneratedNameStub& stub : result.declared_generated_names) {
        mix_declared_generated_name_stub(builder, stub);
    }
    builder.mix_u64(static_cast<base::u64>(result.token_materialization_admissions.size()));
    for (const TokenMaterializationAdmissionStub& stub : result.token_materialization_admissions) {
        mix_token_materialization_admission_stub(builder, stub);
    }
    builder.mix_u64(static_cast<base::u64>(result.generated_token_buffers.size()));
    for (const GeneratedTokenBufferStub& stub : result.generated_token_buffers) {
        mix_generated_token_buffer_stub(builder, stub);
    }
    builder.mix_u64(static_cast<base::u64>(result.generated_token_records.size()));
    for (const GeneratedTokenRecord& record : result.generated_token_records) {
        mix_generated_token_record(builder, record);
    }
    builder.mix_u64(static_cast<base::u64>(result.parser_admission_gates.size()));
    for (const GeneratedTokenParserAdmissionGateStub& stub : result.parser_admission_gates) {
        mix_generated_token_parser_admission_gate_stub(builder, stub);
    }
    builder.mix_u64(static_cast<base::u64>(result.parser_admission_diagnostics.size()));
    for (const ParserAdmissionDiagnosticProjectionStub& stub : result.parser_admission_diagnostics) {
        mix_parser_admission_diagnostic_projection_stub(builder, stub);
    }
    builder.mix_u64(static_cast<base::u64>(result.parser_admission_report_entries.size()));
    for (const ParserAdmissionDiagnosticReportEntry& entry : result.parser_admission_report_entries) {
        mix_parser_admission_report_entry(builder, entry);
    }
    builder.mix_u64(static_cast<base::u64>(result.parser_admission_reports.size()));
    for (const ParserAdmissionDiagnosticReport& report : result.parser_admission_reports) {
        mix_parser_admission_report(builder, report);
    }
    builder.mix_u64(static_cast<base::u64>(result.parser_readiness_preflight_entries.size()));
    for (const GeneratedTokenParserReadinessPreflightEntry& entry :
        result.parser_readiness_preflight_entries) {
        mix_parser_readiness_preflight_entry(builder, entry);
    }
    builder.mix_u64(static_cast<base::u64>(result.parser_consumption_contract_gates.size()));
    for (const GeneratedTokenParserConsumptionContractGate& gate :
        result.parser_consumption_contract_gates) {
        mix_parser_consumption_contract_gate(builder, gate);
    }
    builder.mix_u64(static_cast<base::u64>(result.macro_boundary_closure_reports.size()));
    for (const MacroExpansionBoundaryClosureReport& report :
        result.macro_boundary_closure_reports) {
        mix_macro_boundary_closure_report(builder, report);
    }
    builder.mix_u64(static_cast<base::u64>(result.builtin_derive_expansion_admissions.size()));
    for (const BuiltinDeriveExpansionAdmissionGate& gate :
        result.builtin_derive_expansion_admissions) {
        mix_builtin_derive_expansion_admission_gate(builder, gate);
    }
    builder.mix_u64(static_cast<base::u64>(result.builtin_derive_semantic_plans.size()));
    for (const BuiltinDeriveSemanticExpansionPlan& plan :
        result.builtin_derive_semantic_plans) {
        mix_builtin_derive_semantic_expansion_plan(builder, plan);
    }
    builder.mix_u64(static_cast<base::u64>(result.builtin_derive_parser_release_gates.size()));
    for (const BuiltinDeriveParserConsumptionReleaseGate& gate :
        result.builtin_derive_parser_release_gates) {
        mix_builtin_derive_parser_consumption_release_gate(builder, gate);
    }
    builder.mix_u64(static_cast<base::u64>(result.builtin_derive_release_hardening_matrices.size()));
    for (const BuiltinDeriveReleaseHardeningMatrix& matrix :
        result.builtin_derive_release_hardening_matrices) {
        mix_builtin_derive_release_hardening_matrix(builder, matrix);
    }
    builder.mix_u64(static_cast<base::u64>(result.builtin_derive_debug_dump_contracts.size()));
    for (const BuiltinDeriveDebugDumpStabilityContract& contract :
        result.builtin_derive_debug_dump_contracts) {
        mix_builtin_derive_debug_dump_stability_contract(builder, contract);
    }
    builder.mix_u64(static_cast<base::u64>(result.builtin_derive_rollback_diagnostic_gates.size()));
    for (const BuiltinDeriveRollbackDiagnosticDesignGate& gate :
        result.builtin_derive_rollback_diagnostic_gates) {
        mix_builtin_derive_rollback_diagnostic_design_gate(builder, gate);
    }
    builder.mix_u64(
        static_cast<base::u64>(result.builtin_derive_parser_consumption_admission_protocols.size()));
    for (const BuiltinDeriveParserConsumptionAdmissionProtocol& protocol :
        result.builtin_derive_parser_consumption_admission_protocols) {
        mix_builtin_derive_parser_consumption_admission_protocol(builder, protocol);
    }
    builder.mix_u64(static_cast<base::u64>(result.builtin_derive_checkpoint_rollback_protocols.size()));
    for (const BuiltinDeriveParserConsumptionCheckpointRollbackProtocol& protocol :
        result.builtin_derive_checkpoint_rollback_protocols) {
        mix_builtin_derive_checkpoint_rollback_protocol(builder, protocol);
    }
    builder.mix_u64(
        static_cast<base::u64>(result.builtin_derive_preconsumption_verification_closures.size()));
    for (const BuiltinDeriveParserPreConsumptionVerificationClosure& closure :
        result.builtin_derive_preconsumption_verification_closures) {
        mix_builtin_derive_preconsumption_verification_closure(builder, closure);
    }
    builder.mix_u64(
        static_cast<base::u64>(result.builtin_derive_controlled_dry_run_adapters.size()));
    for (const BuiltinDeriveControlledParserDryRunAdapter& adapter :
        result.builtin_derive_controlled_dry_run_adapters) {
        mix_builtin_derive_controlled_dry_run_adapter(builder, adapter);
    }
    builder.mix_u64(
        static_cast<base::u64>(result.builtin_derive_dry_run_rollback_replays.size()));
    for (const BuiltinDeriveDryRunRollbackDiagnosticReplay& replay :
        result.builtin_derive_dry_run_rollback_replays) {
        mix_builtin_derive_dry_run_rollback_replay(builder, replay);
    }
    builder.mix_u64(
        static_cast<base::u64>(result.builtin_derive_dry_run_negative_matrices.size()));
    for (const BuiltinDeriveDryRunNegativeMatrixClosure& matrix :
        result.builtin_derive_dry_run_negative_matrices) {
        mix_builtin_derive_dry_run_negative_matrix(builder, matrix);
    }
    builder.mix_u64(
        static_cast<base::u64>(result.builtin_derive_parser_dry_run_sessions.size()));
    for (const BuiltinDeriveParserDryRunSessionBoundary& session :
        result.builtin_derive_parser_dry_run_sessions) {
        mix_builtin_derive_parser_dry_run_session(builder, session);
    }
    builder.mix_u64(
        static_cast<base::u64>(result.builtin_derive_token_cursor_snapshot_proofs.size()));
    for (const BuiltinDeriveTokenCursorSnapshotRollbackProof& proof :
        result.builtin_derive_token_cursor_snapshot_proofs) {
        mix_builtin_derive_token_cursor_snapshot_proof(builder, proof);
    }
    builder.mix_u64(static_cast<base::u64>(
        result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures.size()));
    for (const BuiltinDeriveDiagnosticShadowNoAstMutationClosure& closure :
        result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures) {
        mix_builtin_derive_diagnostic_shadow_no_ast_mutation_closure(builder, closure);
    }
    builder.mix_u64(static_cast<base::u64>(
        result.builtin_derive_parser_dry_run_admission_gates.size()));
    for (const BuiltinDeriveParserDryRunAdmissionGate& gate :
        result.builtin_derive_parser_dry_run_admission_gates) {
        mix_builtin_derive_parser_dry_run_admission_gate(builder, gate);
    }
    builder.mix_u64(static_cast<base::u64>(
        result.builtin_derive_error_recovery_shadow_diagnostic_gates.size()));
    for (const BuiltinDeriveErrorRecoveryShadowDiagnosticGate& gate :
        result.builtin_derive_error_recovery_shadow_diagnostic_gates) {
        mix_builtin_derive_error_recovery_shadow_diagnostic_gate(builder, gate);
    }
    builder.mix_u64(static_cast<base::u64>(
        result.builtin_derive_cursor_rollback_ast_mutation_verifier_closures.size()));
    for (const BuiltinDeriveCursorRollbackAstMutationVerifierClosure& closure :
        result.builtin_derive_cursor_rollback_ast_mutation_verifier_closures) {
        mix_builtin_derive_cursor_rollback_ast_mutation_verifier_closure(builder, closure);
    }
    builder.mix_u64(result.aurex_macro_surface_source_item_count);
    builder.mix_u64(static_cast<base::u64>(result.aurex_macro_surface_admission_gates.size()));
    for (const AurexMacroSurfaceAdmissionGate& gate : result.aurex_macro_surface_admission_gates) {
        mix_aurex_macro_surface_admission_gate(builder, gate);
    }
    builder.mix_u64(static_cast<base::u64>(result.aurex_macro_definition_site_hygiene_gates.size()));
    for (const AurexMacroDefinitionSiteHygieneAdmissionGate& gate :
        result.aurex_macro_definition_site_hygiene_gates) {
        mix_aurex_macro_definition_site_hygiene_gate(builder, gate);
    }
    builder.mix_u64(static_cast<base::u64>(result.aurex_macro_typed_matcher_admission_gates.size()));
    for (const AurexMacroTypedMatcherAdmissionGate& gate :
        result.aurex_macro_typed_matcher_admission_gates) {
        mix_aurex_macro_typed_matcher_admission_gate(builder, gate);
    }
    mix_summary(builder, summarize_early_item_expansion_counts(result));
    return builder.finish();
}

std::string summarize_early_item_expansion(const EarlyItemExpansionResult& result)
{
    const EarlyItemExpansionSummary summary = summarize_early_item_expansion_counts(result);
    std::ostringstream stream;
    stream << "early_item_expansion name="
           << (result.name.empty() ? "<anonymous>" : result.name)
           << " attributes=" << summary.attribute_input_count
           << " builtin_derive_passthrough=" << summary.builtin_derive_passthrough_count
           << " blocked_attributes=" << summary.blocked_attribute_count
           << " generated_part_placeholders=" << summary.generated_part_placeholder_count
           << " generated_part_stubs=" << summary.generated_part_stub_count
           << " materialized_buffer_stubs=" << summary.materialized_buffer_stub_count
           << " parse_blocked=" << summary.parse_blocked_count
           << " merge_blocked=" << summary.merge_blocked_count
           << " sema_visible_generated_parts=" << summary.sema_visible_generated_part_count
           << " source_map_placeholders=" << summary.source_map_placeholder_count
           << " hygiene_stubs=" << summary.hygiene_stub_count
           << " unresolved_hygiene_stubs=" << summary.unresolved_hygiene_stub_count
           << " declared_name_stubs=" << summary.declared_name_stub_count
           << " trace_stubs=" << summary.trace_stub_count
           << " real_source_maps=" << summary.real_source_map_count
           << " debug_traces=" << summary.debug_trace_available_count
           << " cli_emit_expanded=" << summary.cli_emit_expanded_available_count
           << " generated_item_declarations=" << summary.generated_item_declaration_stub_count
           << " planned_generated_item_declarations="
           << summary.planned_generated_item_declaration_count
           << " materialized_generated_items=" << summary.materialized_generated_item_count
           << " declared_generated_names=" << summary.declared_generated_name_stub_count
           << " lookup_visible_declared_names=" << summary.lookup_visible_declared_name_count
           << " export_visible_declared_names=" << summary.export_visible_declared_name_count
           << " token_materialization_admissions="
           << summary.token_materialization_admission_stub_count
           << " compiler_owned_admissions=" << summary.compiler_owned_admission_count
           << " admitted_token_materializations="
           << summary.admitted_token_materialization_count
           << " materialized_token_admissions="
           << summary.materialized_token_admission_count
           << " generated_token_buffers=" << summary.generated_token_buffer_stub_count
           << " empty_generated_token_buffers="
           << summary.empty_generated_token_buffer_count
           << " materialized_token_buffers=" << summary.materialized_token_buffer_count
           << " compiler_owned_token_buffers=" << summary.compiler_owned_token_buffer_count
           << " generated_token_records=" << summary.generated_token_record_count
           << " compiler_owned_generated_token_records="
           << summary.compiler_owned_generated_token_record_count
           << " parser_visible_generated_tokens=" << summary.parser_visible_generated_token_count
           << " parser_admission_gates=" << summary.parser_admission_gate_stub_count
           << " compiler_owned_parser_admission_gates="
           << summary.compiler_owned_parser_admission_gate_count
           << " token_record_available_gates=" << summary.token_record_available_gate_count
           << " parser_blocked_token_buffers=" << summary.parser_blocked_token_buffer_count
           << " parser_admitted_token_buffers=" << summary.parser_admitted_token_buffer_count
           << " parser_admission_diagnostics=" << summary.parser_admission_diagnostic_stub_count
           << " parser_admission_diagnostics_blocked="
           << summary.parser_admission_diagnostic_blocked_count
           << " derive_parser_admission_diagnostics="
           << summary.derive_parser_admission_diagnostic_count
           << " empty_parser_admission_diagnostics="
           << summary.empty_parser_admission_diagnostic_count
           << " emit_expanded_projections="
           << summary.emit_expanded_projection_available_count
           << " parser_admission_debug_trace_projections="
           << summary.parser_admission_debug_trace_projection_count
           << " parser_admission_source_map_projections="
           << summary.parser_admission_source_map_projection_count
           << " parser_admission_report_entries="
           << summary.parser_admission_report_entry_count
           << " parser_admission_reports=" << summary.parser_admission_report_count
           << " parser_admission_report_blocked_entries="
           << summary.parser_admission_report_blocked_entry_count
           << " derive_parser_admission_report_entries="
           << summary.parser_admission_report_derive_entry_count
           << " empty_parser_admission_report_entries="
           << summary.parser_admission_report_empty_entry_count
           << " parser_admission_report_token_record_available_entries="
           << summary.parser_admission_report_token_record_available_entry_count
           << " parser_admission_report_visible="
           << summary.parser_admission_report_visible_count
           << " parser_admission_report_query_reusable="
           << summary.parser_admission_report_query_reusable_count
           << " parser_admission_report_unordered_anchors="
           << summary.parser_admission_report_unordered_anchor_count
           << " parser_admission_report_parser_consumable="
           << summary.parser_admission_report_parser_consumable_count
           << " parser_readiness_preflight_entries="
           << summary.parser_readiness_preflight_entry_count
           << " parser_readiness_preflight_blocked="
           << summary.parser_readiness_preflight_blocked_count
           << " derive_parser_readiness_preflight_entries="
           << summary.parser_readiness_preflight_derive_entry_count
           << " empty_parser_readiness_preflight_entries="
           << summary.parser_readiness_preflight_empty_entry_count
           << " parser_readiness_preflight_contiguous_indices="
           << summary.parser_readiness_preflight_contiguous_index_count
           << " parser_readiness_preflight_delimiter_balanced="
           << summary.parser_readiness_preflight_delimiter_balanced_count
           << " parser_readiness_preflight_source_anchor_covered="
           << summary.parser_readiness_preflight_source_anchor_covered_count
           << " parser_readiness_preflight_parse_config_compatible="
           << summary.parser_readiness_preflight_parse_config_compatible_count
           << " parser_readiness_preflight_parser_consumable="
           << summary.parser_readiness_preflight_parser_consumable_count
           << " parser_consumption_contract_gates="
           << summary.parser_consumption_contract_gate_count
           << " parser_consumption_contract_blocked_gates="
           << summary.parser_consumption_contract_blocked_gate_count
           << " parser_consumption_contract_visible="
           << summary.parser_consumption_contract_visible_count
           << " parser_consumption_contract_query_reusable="
           << summary.parser_consumption_contract_query_reusable_count
           << " parser_consumption_contract_parser_consumable="
           << summary.parser_consumption_contract_parser_consumable_count
           << " macro_boundary_closure_reports="
           << summary.macro_boundary_closure_report_count
           << " macro_boundary_closure_visible="
           << summary.macro_boundary_closure_visible_count
           << " macro_boundary_closure_query_reusable="
           << summary.macro_boundary_closure_query_reusable_count
           << " macro_boundary_closure_complete="
           << summary.macro_boundary_closure_complete_count
           << " macro_boundary_closure_parser_consumption_enabled="
           << summary.macro_boundary_closure_parser_consumption_enabled_count
           << " builtin_derive_expansion_admissions="
           << summary.builtin_derive_expansion_admission_gate_count
           << " builtin_derive_expansion_derive_admissions="
           << summary.builtin_derive_expansion_derive_admission_count
           << " builtin_derive_expansion_non_derive_blocked="
           << summary.builtin_derive_expansion_non_derive_blocked_count
           << " builtin_derive_expansion_visible="
           << summary.builtin_derive_expansion_visible_count
           << " builtin_derive_expansion_query_reusable="
           << summary.builtin_derive_expansion_query_reusable_count
           << " builtin_derive_expansion_capability_candidates="
           << summary.builtin_derive_expansion_capability_candidate_count
           << " builtin_derive_semantic_plans="
           << summary.builtin_derive_semantic_plan_count
           << " builtin_derive_semantic_plan_visible="
           << summary.builtin_derive_semantic_plan_visible_count
           << " builtin_derive_semantic_plan_query_reusable="
           << summary.builtin_derive_semantic_plan_query_reusable_count
           << " builtin_derive_semantic_capabilities="
           << summary.builtin_derive_semantic_capability_count
           << " builtin_derive_semantic_copy_capabilities="
           << summary.builtin_derive_semantic_copy_capability_count
           << " builtin_derive_semantic_eq_capabilities="
           << summary.builtin_derive_semantic_eq_capability_count
           << " builtin_derive_semantic_hash_capabilities="
           << summary.builtin_derive_semantic_hash_capability_count
           << " builtin_derive_parser_release_gates="
           << summary.builtin_derive_parser_release_gate_count
           << " builtin_derive_parser_release_visible="
           << summary.builtin_derive_parser_release_visible_count
           << " builtin_derive_parser_release_query_reusable="
           << summary.builtin_derive_parser_release_query_reusable_count
           << " builtin_derive_parser_release_parser_consumable="
           << summary.builtin_derive_parser_release_parser_consumable_count
           << " builtin_derive_release_hardening_matrices="
           << summary.builtin_derive_release_hardening_matrix_count
           << " builtin_derive_release_hardening_visible="
           << summary.builtin_derive_release_hardening_visible_count
           << " builtin_derive_release_hardening_query_reusable="
           << summary.builtin_derive_release_hardening_query_reusable_count
           << " builtin_derive_release_hardening_negative_matrix_complete="
           << summary.builtin_derive_release_hardening_negative_matrix_complete_count
           << " builtin_derive_release_hardening_parser_consumable="
           << summary.builtin_derive_release_hardening_parser_consumable_count
           << " builtin_derive_debug_dump_contracts="
           << summary.builtin_derive_debug_dump_contract_count
           << " builtin_derive_debug_dump_visible="
           << summary.builtin_derive_debug_dump_contract_visible_count
           << " builtin_derive_debug_dump_query_reusable="
           << summary.builtin_derive_debug_dump_query_reusable_count
           << " builtin_derive_debug_dump_complete="
           << summary.builtin_derive_debug_dump_complete_count
           << " builtin_derive_debug_dump_parser_consumable="
           << summary.builtin_derive_debug_dump_parser_consumable_count
           << " builtin_derive_rollback_diagnostic_gates="
           << summary.builtin_derive_rollback_diagnostic_gate_count
           << " builtin_derive_rollback_diagnostic_visible="
           << summary.builtin_derive_rollback_diagnostic_visible_count
           << " builtin_derive_rollback_diagnostic_query_reusable="
           << summary.builtin_derive_rollback_diagnostic_query_reusable_count
           << " builtin_derive_rollback_diagnostic_design_complete="
           << summary.builtin_derive_rollback_diagnostic_design_complete_count
           << " builtin_derive_rollback_diagnostic_parser_consumable="
           << summary.builtin_derive_rollback_diagnostic_parser_consumable_count
           << " builtin_derive_parser_consumption_admission_protocols="
           << summary.builtin_derive_parser_consumption_admission_protocol_count
           << " builtin_derive_parser_consumption_admission_visible="
           << summary.builtin_derive_parser_consumption_admission_visible_count
           << " builtin_derive_parser_consumption_admission_query_reusable="
           << summary.builtin_derive_parser_consumption_admission_query_reusable_count
           << " builtin_derive_parser_consumption_admission_complete="
           << summary.builtin_derive_parser_consumption_admission_complete_count
           << " builtin_derive_parser_consumption_admission_parser_consumable="
           << summary.builtin_derive_parser_consumption_admission_parser_consumable_count
           << " builtin_derive_checkpoint_rollback_protocols="
           << summary.builtin_derive_checkpoint_rollback_protocol_count
           << " builtin_derive_checkpoint_rollback_visible="
           << summary.builtin_derive_checkpoint_rollback_visible_count
           << " builtin_derive_checkpoint_rollback_query_reusable="
           << summary.builtin_derive_checkpoint_rollback_query_reusable_count
           << " builtin_derive_checkpoint_rollback_complete="
           << summary.builtin_derive_checkpoint_rollback_complete_count
           << " builtin_derive_checkpoint_rollback_parser_consumable="
           << summary.builtin_derive_checkpoint_rollback_parser_consumable_count
           << " builtin_derive_preconsumption_verification_closures="
           << summary.builtin_derive_preconsumption_verification_closure_count
           << " builtin_derive_preconsumption_verification_visible="
           << summary.builtin_derive_preconsumption_verification_visible_count
           << " builtin_derive_preconsumption_verification_query_reusable="
           << summary.builtin_derive_preconsumption_verification_query_reusable_count
           << " builtin_derive_preconsumption_verification_complete="
           << summary.builtin_derive_preconsumption_verification_complete_count
           << " builtin_derive_preconsumption_verification_parser_consumable="
           << summary.builtin_derive_preconsumption_verification_parser_consumable_count
           << " builtin_derive_controlled_dry_run_adapters="
           << summary.builtin_derive_controlled_dry_run_adapter_count
           << " builtin_derive_controlled_dry_run_adapter_visible="
           << summary.builtin_derive_controlled_dry_run_adapter_visible_count
           << " builtin_derive_controlled_dry_run_adapter_query_reusable="
           << summary.builtin_derive_controlled_dry_run_adapter_query_reusable_count
           << " builtin_derive_controlled_dry_run_adapter_complete="
           << summary.builtin_derive_controlled_dry_run_adapter_complete_count
           << " builtin_derive_controlled_dry_run_adapter_executed="
           << summary.builtin_derive_controlled_dry_run_adapter_executed_count
           << " builtin_derive_dry_run_rollback_replays="
           << summary.builtin_derive_dry_run_rollback_replay_count
           << " builtin_derive_dry_run_rollback_replay_visible="
           << summary.builtin_derive_dry_run_rollback_replay_visible_count
           << " builtin_derive_dry_run_rollback_replay_query_reusable="
           << summary.builtin_derive_dry_run_rollback_replay_query_reusable_count
           << " builtin_derive_dry_run_rollback_replay_complete="
           << summary.builtin_derive_dry_run_rollback_replay_complete_count
           << " builtin_derive_dry_run_rollback_replay_executed="
           << summary.builtin_derive_dry_run_rollback_replay_executed_count
           << " builtin_derive_dry_run_negative_matrices="
           << summary.builtin_derive_dry_run_negative_matrix_count
           << " builtin_derive_dry_run_negative_matrix_visible="
           << summary.builtin_derive_dry_run_negative_matrix_visible_count
           << " builtin_derive_dry_run_negative_matrix_query_reusable="
           << summary.builtin_derive_dry_run_negative_matrix_query_reusable_count
           << " builtin_derive_dry_run_negative_matrix_complete="
           << summary.builtin_derive_dry_run_negative_matrix_complete_count
           << " builtin_derive_dry_run_negative_matrix_parser_consumable="
           << summary.builtin_derive_dry_run_negative_matrix_parser_consumable_count
           << " builtin_derive_parser_dry_run_sessions="
           << summary.builtin_derive_parser_dry_run_session_count
           << " builtin_derive_parser_dry_run_session_visible="
           << summary.builtin_derive_parser_dry_run_session_visible_count
           << " builtin_derive_parser_dry_run_session_query_reusable="
           << summary.builtin_derive_parser_dry_run_session_query_reusable_count
           << " builtin_derive_parser_dry_run_session_complete="
           << summary.builtin_derive_parser_dry_run_session_complete_count
           << " builtin_derive_parser_dry_run_session_executed="
           << summary.builtin_derive_parser_dry_run_session_executed_count
           << " builtin_derive_parser_dry_run_session_committed="
           << summary.builtin_derive_parser_dry_run_session_committed_count
           << " builtin_derive_token_cursor_snapshot_proofs="
           << summary.builtin_derive_token_cursor_snapshot_proof_count
           << " builtin_derive_token_cursor_snapshot_proof_visible="
           << summary.builtin_derive_token_cursor_snapshot_proof_visible_count
           << " builtin_derive_token_cursor_snapshot_proof_query_reusable="
           << summary.builtin_derive_token_cursor_snapshot_proof_query_reusable_count
           << " builtin_derive_token_cursor_snapshot_proof_complete="
           << summary.builtin_derive_token_cursor_snapshot_proof_complete_count
           << " builtin_derive_token_cursor_snapshot_proof_cursor_advanced="
           << summary.builtin_derive_token_cursor_snapshot_proof_cursor_advanced_count
           << " builtin_derive_token_cursor_snapshot_proof_committed="
           << summary.builtin_derive_token_cursor_snapshot_proof_committed_count
           << " builtin_derive_diagnostic_shadow_no_ast_mutation_closures="
           << summary.builtin_derive_diagnostic_shadow_no_ast_mutation_closure_count
           << " builtin_derive_diagnostic_shadow_no_ast_mutation_visible="
           << summary.builtin_derive_diagnostic_shadow_no_ast_mutation_visible_count
           << " builtin_derive_diagnostic_shadow_no_ast_mutation_query_reusable="
           << summary.builtin_derive_diagnostic_shadow_no_ast_mutation_query_reusable_count
           << " builtin_derive_diagnostic_shadow_no_ast_mutation_complete="
           << summary.builtin_derive_diagnostic_shadow_no_ast_mutation_complete_count
           << " builtin_derive_diagnostic_shadow_no_ast_mutation_executed="
           << summary.builtin_derive_diagnostic_shadow_no_ast_mutation_executed_count
           << " builtin_derive_diagnostic_shadow_no_ast_mutation_ast_mutation="
           << summary.builtin_derive_diagnostic_shadow_no_ast_mutation_ast_mutation_count
           << " builtin_derive_diagnostic_shadow_no_ast_mutation_parser_consumable="
           << summary.builtin_derive_diagnostic_shadow_no_ast_mutation_parser_consumable_count
           << " builtin_derive_parser_dry_run_admission_gates="
           << summary.builtin_derive_parser_dry_run_admission_gate_count
           << " builtin_derive_parser_dry_run_admission_gate_visible="
           << summary.builtin_derive_parser_dry_run_admission_gate_visible_count
           << " builtin_derive_parser_dry_run_admission_gate_query_reusable="
           << summary.builtin_derive_parser_dry_run_admission_gate_query_reusable_count
           << " builtin_derive_parser_dry_run_admission_gate_complete="
           << summary.builtin_derive_parser_dry_run_admission_gate_complete_count
           << " builtin_derive_parser_dry_run_admission_gate_execution_admitted="
           << summary.builtin_derive_parser_dry_run_admission_gate_execution_admitted_count
           << " builtin_derive_parser_dry_run_admission_gate_executed="
           << summary.builtin_derive_parser_dry_run_admission_gate_executed_count
           << " builtin_derive_parser_dry_run_admission_gate_parser_consumable="
           << summary.builtin_derive_parser_dry_run_admission_gate_parser_consumable_count
           << " builtin_derive_error_recovery_shadow_diagnostic_gates="
           << summary.builtin_derive_error_recovery_shadow_diagnostic_gate_count
           << " builtin_derive_error_recovery_shadow_diagnostic_gate_visible="
           << summary.builtin_derive_error_recovery_shadow_diagnostic_gate_visible_count
           << " builtin_derive_error_recovery_shadow_diagnostic_gate_query_reusable="
           << summary.builtin_derive_error_recovery_shadow_diagnostic_gate_query_reusable_count
           << " builtin_derive_error_recovery_shadow_diagnostic_gate_complete="
           << summary.builtin_derive_error_recovery_shadow_diagnostic_gate_complete_count
           << " builtin_derive_error_recovery_shadow_diagnostic_gate_recovery_executed="
           << summary.builtin_derive_error_recovery_shadow_diagnostic_gate_recovery_executed_count
           << " builtin_derive_error_recovery_shadow_diagnostic_gate_diagnostic_emitted="
           << summary.builtin_derive_error_recovery_shadow_diagnostic_gate_diagnostic_emitted_count
           << " builtin_derive_error_recovery_shadow_diagnostic_gate_parser_consumable="
           << summary.builtin_derive_error_recovery_shadow_diagnostic_gate_parser_consumable_count
           << " builtin_derive_cursor_rollback_ast_mutation_verifier_closures="
           << summary.builtin_derive_cursor_rollback_ast_mutation_verifier_closure_count
           << " builtin_derive_cursor_rollback_ast_mutation_verifier_visible="
           << summary.builtin_derive_cursor_rollback_ast_mutation_verifier_visible_count
           << " builtin_derive_cursor_rollback_ast_mutation_verifier_query_reusable="
           << summary.builtin_derive_cursor_rollback_ast_mutation_verifier_query_reusable_count
           << " builtin_derive_cursor_rollback_ast_mutation_verifier_complete="
           << summary.builtin_derive_cursor_rollback_ast_mutation_verifier_complete_count
           << " builtin_derive_cursor_rollback_ast_mutation_verifier_rollback_executed="
           << summary.builtin_derive_cursor_rollback_ast_mutation_verifier_rollback_executed_count
           << " builtin_derive_cursor_rollback_ast_mutation_verifier_ast_mutation="
           << summary.builtin_derive_cursor_rollback_ast_mutation_verifier_ast_mutation_count
           << " builtin_derive_cursor_rollback_ast_mutation_verifier_parser_consumable="
           << summary.builtin_derive_cursor_rollback_ast_mutation_verifier_parser_consumable_count
           << " aurex_macro_surface_source_items="
           << summary.aurex_macro_surface_source_item_count
           << " aurex_macro_surface_admissions="
           << summary.aurex_macro_surface_admission_gate_count
           << " aurex_macro_declarative_surfaces="
           << summary.aurex_macro_declarative_surface_count
           << " aurex_macro_user_derive_surfaces="
           << summary.aurex_macro_user_derive_surface_count
           << " aurex_macro_compile_time_surfaces="
           << summary.aurex_macro_compile_time_surface_count
           << " aurex_macro_surface_match_clauses="
           << summary.aurex_macro_surface_match_clause_count
           << " aurex_macro_surface_expansion_enabled="
           << summary.aurex_macro_surface_expansion_enabled_count
           << " aurex_macro_surface_compile_time_execution_enabled="
           << summary.aurex_macro_surface_compile_time_execution_enabled_count
           << " aurex_macro_surface_parser_consumable="
           << summary.aurex_macro_surface_parser_consumable_count
           << " aurex_macro_definition_site_hygiene_gates="
           << summary.aurex_macro_definition_site_hygiene_gate_count
           << " aurex_macro_definition_site_scope_available="
           << summary.aurex_macro_definition_site_scope_available_count
           << " aurex_macro_fresh_name_scopes="
           << summary.aurex_macro_fresh_name_scope_reserved_count
           << " aurex_macro_diagnostic_anchors="
           << summary.aurex_macro_diagnostic_anchor_available_count
           << " aurex_macro_hygiene_resolution_enabled="
           << summary.aurex_macro_hygiene_resolution_enabled_count
           << " aurex_macro_typed_matcher_admissions="
           << summary.aurex_macro_typed_matcher_admission_gate_count
           << " aurex_macro_typed_matchers_recognized="
           << summary.aurex_macro_typed_matcher_recognized_count
           << " aurex_macro_expr_list_matchers="
           << summary.aurex_macro_expr_list_matcher_count
           << " aurex_macro_item_matchers="
           << summary.aurex_macro_item_matcher_count
           << " aurex_macro_token_stream_matchers="
           << summary.aurex_macro_token_stream_matcher_count
           << " aurex_macro_unknown_matchers="
           << summary.aurex_macro_unknown_matcher_count
           << " aurex_macro_typed_matcher_execution_enabled="
           << summary.aurex_macro_typed_matcher_execution_enabled_count
           << " generated_source_text=" << summary.generated_source_text_count
           << " parse_ready_token_buffers=" << summary.parse_ready_token_buffer_count
           << " ast_mutations=" << summary.ast_mutation_count
           << " user_generated_code=" << summary.user_generated_code_count
           << " standard_library_required=" << summary.standard_library_required_count
           << " runtime_required=" << summary.runtime_required_count
           << " external_process_required=" << summary.external_process_required_count
           << " fingerprint=" << query::debug_string(early_item_expansion_fingerprint(result));
    return stream.str();
}

std::string dump_early_item_expansion(const EarlyItemExpansionResult& result)
{
    std::ostringstream stream;
    stream << summarize_early_item_expansion(result) << '\n';
    for (base::usize index = 0; index < result.inputs.size(); ++index) {
        const EarlyItemMacroInput& input = result.inputs[index];
        stream << "  input #" << index
               << " item=" << input.item.value
               << " module=" << input.module.value
               << " part=" << input.part_index
               << " attr=" << input.attribute_name
               << " disposition=" << early_item_expansion_disposition_name(input.disposition)
               << " token_count=" << input.token_count
               << " query_key=" << query::debug_string(input.query_key_fingerprint) << '\n';
    }
    for (base::usize index = 0; index < result.generated_parts.size(); ++index) {
        const GeneratedModulePartPlaceholder& part = result.generated_parts[index];
        stream << "  generated_part #" << index
               << " module=" << part.module.value
               << " source_part=" << part.source_part_index
               << " stable_index=" << part.generated_stable_index
               << " source_role=generated"
               << " kind=generated"
               << " parsed=" << (part.parsed ? "yes" : "no")
               << " merged=" << (part.merged ? "yes" : "no")
               << " user_generated_code=" << (part.produced_user_generated_code ? "yes" : "no") << '\n';
    }
    for (base::usize index = 0; index < result.generated_part_stubs.size(); ++index) {
        const GeneratedModulePartParseMergeStub& stub = result.generated_part_stubs[index];
        stream << "  parse_merge_stub #" << index
               << " module=" << stub.module.value
               << " source_part=" << stub.source_part_index
               << " stable_index=" << stub.generated_stable_index
               << " buffer=" << stub.generated_buffer_name
               << " lifecycle=" << generated_module_part_lifecycle_state_name(stub.lifecycle_state)
               << " materialized_buffer=" << (stub.materialized_buffer ? "yes" : "no")
               << " parsed=" << (stub.parsed ? "yes" : "no")
               << " merged=" << (stub.merged ? "yes" : "no")
               << " sema_visible=" << (stub.sema_visible ? "yes" : "no")
               << " blocker=" << stub.blocker_reason
               << " buffer_identity=" << query::debug_string(stub.generated_buffer_identity)
               << " parse_config=" << query::debug_string(stub.parse_config_fingerprint)
               << " merge_ordering=" << query::debug_string(stub.merge_ordering_key) << '\n';
    }
    for (base::usize index = 0; index < result.source_maps.size(); ++index) {
        const ExpansionSourceMapPlaceholder& source_map = result.source_maps[index];
        stream << "  source_map #" << index
               << " item=" << source_map.item.value
               << " module=" << source_map.module.value
               << " attribute_index=" << source_map.attribute_index
               << " real_source_map=" << (source_map.real_source_map ? "yes" : "no")
               << " debug_trace=" << (source_map.debug_trace_available ? "yes" : "no") << '\n';
    }
    for (base::usize index = 0; index < result.hygiene_stubs.size(); ++index) {
        const ExpansionHygieneStub& stub = result.hygiene_stubs[index];
        stream << "  hygiene_stub #" << index
               << " item=" << stub.item.value
               << " module=" << stub.module.value
               << " part=" << stub.part_index
               << " attribute_index=" << stub.attribute_index
               << " policy=" << stub.policy
               << " resolved=" << (stub.resolved ? "yes" : "no")
               << " declared_names_visible=" << (stub.declared_names_visible ? "yes" : "no")
               << " captures_call_site_locals=" << (stub.captures_call_site_locals ? "yes" : "no")
               << " origin=" << query::debug_string(stub.expansion_origin)
               << " call_site_mark=" << query::debug_string(stub.call_site_mark)
               << " definition_site_mark=" << query::debug_string(stub.definition_site_mark)
               << " generated_fresh_mark=" << query::debug_string(stub.generated_fresh_mark)
               << " declared_name_set=" << query::debug_string(stub.declared_name_set) << '\n';
    }
    for (base::usize index = 0; index < result.trace_stubs.size(); ++index) {
        const ExpansionTraceStub& stub = result.trace_stubs[index];
        stream << "  trace_stub #" << index
               << " item=" << stub.item.value
               << " module=" << stub.module.value
               << " part=" << stub.part_index
               << " attribute_index=" << stub.attribute_index
               << " policy=" << stub.trace_policy
               << " real_source_map=" << (stub.real_source_map ? "yes" : "no")
               << " debug_trace=" << (stub.debug_trace_available ? "yes" : "no")
               << " cli_emit_expanded=" << (stub.cli_emit_expanded_available ? "yes" : "no")
               << " blocker=" << stub.blocker_reason
               << " origin=" << query::debug_string(stub.expansion_origin)
               << " trace_identity=" << query::debug_string(stub.trace_identity)
               << " generated_source_map="
               << query::debug_string(stub.generated_source_map_identity)
               << " diagnostic_anchor=" << query::debug_string(stub.diagnostic_anchor) << '\n';
    }
    for (base::usize index = 0; index < result.generated_item_declarations.size(); ++index) {
        const GeneratedItemDeclarationStub& stub = result.generated_item_declarations[index];
        stream << "  generated_item_declaration_stub #" << index
               << " item=" << stub.item.value
               << " module=" << stub.module.value
               << " part=" << stub.part_index
               << " attribute_index=" << stub.attribute_index
               << " role=" << stub.declaration_role
               << " name=" << stub.generated_item_name
               << " planned=" << (stub.planned ? "yes" : "no")
               << " materialized_tokens=" << (stub.materialized_tokens ? "yes" : "no")
               << " parsed=" << (stub.parsed ? "yes" : "no")
               << " merged=" << (stub.merged ? "yes" : "no")
               << " sema_visible=" << (stub.sema_visible ? "yes" : "no")
               << " user_generated_code=" << (stub.produced_user_generated_code ? "yes" : "no")
               << " blocker=" << stub.blocker_reason
               << " origin=" << query::debug_string(stub.expansion_origin)
               << " declaration_identity=" << query::debug_string(stub.declaration_identity)
               << " declared_name_set=" << query::debug_string(stub.declared_name_set)
               << " generated_item_key=" << query::debug_string(stub.generated_item_key) << '\n';
    }
    for (base::usize index = 0; index < result.declared_generated_names.size(); ++index) {
        const DeclaredGeneratedNameStub& stub = result.declared_generated_names[index];
        stream << "  declared_generated_name_stub #" << index
               << " item=" << stub.item.value
               << " module=" << stub.module.value
               << " part=" << stub.part_index
               << " attribute_index=" << stub.attribute_index
               << " namespace=" << stub.namespace_kind
               << " name=" << stub.declared_name
               << " lookup_visible=" << (stub.lookup_visible ? "yes" : "no")
               << " export_visible=" << (stub.export_visible ? "yes" : "no")
               << " sema_visible=" << (stub.sema_visible ? "yes" : "no")
               << " user_generated_code=" << (stub.produced_user_generated_code ? "yes" : "no")
               << " blocker=" << stub.blocker_reason
               << " origin=" << query::debug_string(stub.expansion_origin)
               << " declared_name_set=" << query::debug_string(stub.declared_name_set)
               << " declared_name_identity=" << query::debug_string(stub.declared_name_identity)
               << " hygiene_mark=" << query::debug_string(stub.hygiene_mark) << '\n';
    }
    for (base::usize index = 0; index < result.token_materialization_admissions.size(); ++index) {
        const TokenMaterializationAdmissionStub& stub = result.token_materialization_admissions[index];
        stream << "  token_materialization_admission_stub #" << index
               << " item=" << stub.item.value
               << " module=" << stub.module.value
               << " part=" << stub.part_index
               << " attribute_index=" << stub.attribute_index
               << " policy=" << stub.admission_policy
               << " token_stream=" << stub.token_stream_name
               << " compiler_owned=" << (stub.compiler_owned ? "yes" : "no")
               << " admitted=" << (stub.admitted ? "yes" : "no")
               << " materialized_tokens=" << (stub.materialized_tokens ? "yes" : "no")
               << " generated_source_text=" << (stub.generated_source_text ? "yes" : "no")
               << " parse_ready=" << (stub.parse_ready ? "yes" : "no")
               << " external_process_required=" << (stub.external_process_required ? "yes" : "no")
               << " standard_library_required=" << (stub.standard_library_required ? "yes" : "no")
               << " runtime_required=" << (stub.runtime_required ? "yes" : "no")
               << " user_generated_code=" << (stub.produced_user_generated_code ? "yes" : "no")
               << " blocker=" << stub.blocker_reason
               << " origin=" << query::debug_string(stub.expansion_origin)
               << " declaration_identity=" << query::debug_string(stub.declaration_identity)
               << " generated_item_key=" << query::debug_string(stub.generated_item_key)
               << " declared_name_set=" << query::debug_string(stub.declared_name_set)
               << " declared_name_identity=" << query::debug_string(stub.declared_name_identity)
               << " hygiene_mark=" << query::debug_string(stub.hygiene_mark)
               << " source_map_identity=" << query::debug_string(stub.source_map_identity)
               << " trace_identity=" << query::debug_string(stub.trace_identity)
               << " token_plan_identity=" << query::debug_string(stub.token_plan_identity)
               << " token_buffer_identity=" << query::debug_string(stub.token_buffer_identity) << '\n';
    }
    for (base::usize index = 0; index < result.generated_token_buffers.size(); ++index) {
        const GeneratedTokenBufferStub& stub = result.generated_token_buffers[index];
        stream << "  generated_token_buffer_stub #" << index
               << " item=" << stub.item.value
               << " module=" << stub.module.value
               << " part=" << stub.part_index
               << " attribute_index=" << stub.attribute_index
               << " token_stream=" << stub.token_stream_name
               << " kind=" << stub.token_buffer_kind
               << " producer=" << stub.token_producer_policy
               << " token_count=" << stub.token_count
               << " empty=" << (stub.empty ? "yes" : "no")
               << " materialized_tokens=" << (stub.materialized_tokens ? "yes" : "no")
               << " generated_source_text=" << (stub.generated_source_text ? "yes" : "no")
               << " parser_consumable=" << (stub.parser_consumable ? "yes" : "no")
               << " user_generated_code=" << (stub.produced_user_generated_code ? "yes" : "no")
               << " blocker=" << stub.blocker_reason
               << " token_plan_identity=" << query::debug_string(stub.token_plan_identity)
               << " token_buffer_identity=" << query::debug_string(stub.token_buffer_identity)
               << " materialization_identity=" << query::debug_string(stub.materialization_identity)
               << " source_map_identity=" << query::debug_string(stub.source_map_identity)
               << " hygiene_mark=" << query::debug_string(stub.hygiene_mark) << '\n';
    }
    for (base::usize index = 0; index < result.generated_token_records.size(); ++index) {
        const GeneratedTokenRecord& record = result.generated_token_records[index];
        stream << "  generated_token_record #" << index
               << " item=" << record.item.value
               << " module=" << record.module.value
               << " part=" << record.part_index
               << " attribute_index=" << record.attribute_index
               << " token_index=" << record.token_index
               << " kind=" << syntax::token_kind_name(record.kind)
               << " text=" << record.text
               << " role=" << record.token_role
               << " compiler_owned=" << (record.compiler_owned ? "yes" : "no")
               << " parser_visible=" << (record.parser_visible ? "yes" : "no")
               << " user_generated_code=" << (record.produced_user_generated_code ? "yes" : "no")
               << " token_identity=" << query::debug_string(record.token_identity)
               << " token_buffer_identity=" << query::debug_string(record.token_buffer_identity)
               << " source_map_identity=" << query::debug_string(record.source_map_identity)
               << " hygiene_mark=" << query::debug_string(record.hygiene_mark) << '\n';
    }
    for (base::usize index = 0; index < result.parser_admission_gates.size(); ++index) {
        const GeneratedTokenParserAdmissionGateStub& stub = result.parser_admission_gates[index];
        stream << "  generated_token_parser_admission_gate_stub #" << index
               << " item=" << stub.item.value
               << " module=" << stub.module.value
               << " part=" << stub.part_index
               << " attribute_index=" << stub.attribute_index
               << " token_stream=" << stub.token_stream_name
               << " policy=" << stub.parser_gate_policy
               << " token_count=" << stub.token_count
               << " compiler_owned=" << (stub.compiler_owned ? "yes" : "no")
               << " token_buffer_materialized=" << (stub.token_buffer_materialized ? "yes" : "no")
               << " token_records_available=" << (stub.token_records_available ? "yes" : "no")
               << " parser_admitted=" << (stub.parser_admitted ? "yes" : "no")
               << " parse_ready=" << (stub.parse_ready ? "yes" : "no")
               << " parser_consumable=" << (stub.parser_consumable ? "yes" : "no")
               << " generated_source_text=" << (stub.generated_source_text ? "yes" : "no")
               << " generated_part_parsed=" << (stub.generated_part_parsed ? "yes" : "no")
               << " generated_part_merged=" << (stub.generated_part_merged ? "yes" : "no")
               << " sema_visible=" << (stub.sema_visible ? "yes" : "no")
               << " user_generated_code=" << (stub.produced_user_generated_code ? "yes" : "no")
               << " blocker=" << stub.blocker_reason
               << " token_plan_identity=" << query::debug_string(stub.token_plan_identity)
               << " token_buffer_identity=" << query::debug_string(stub.token_buffer_identity)
               << " materialization_identity=" << query::debug_string(stub.materialization_identity)
               << " source_map_identity=" << query::debug_string(stub.source_map_identity)
               << " hygiene_mark=" << query::debug_string(stub.hygiene_mark)
               << " generated_buffer_identity=" << query::debug_string(stub.generated_buffer_identity)
               << " parse_config=" << query::debug_string(stub.parse_config_fingerprint)
               << " parse_gate_identity=" << query::debug_string(stub.parse_gate_identity) << '\n';
    }
    for (base::usize index = 0; index < result.parser_admission_diagnostics.size(); ++index) {
        const ParserAdmissionDiagnosticProjectionStub& stub = result.parser_admission_diagnostics[index];
        stream << "  parser_admission_diagnostic_projection_stub #" << index
               << " item=" << stub.item.value
               << " module=" << stub.module.value
               << " part=" << stub.part_index
               << " attribute_index=" << stub.attribute_index
               << " policy=" << stub.diagnostic_policy
               << " category=" << stub.blocker_category
               << " debug_projection=" << stub.debug_projection_name
               << " primary_anchor=" << stub.primary_anchor.source.value << ':'
               << stub.primary_anchor.begin << ':' << stub.primary_anchor.end
               << " token_tree_anchor=" << stub.token_tree_anchor.source.value << ':'
               << stub.token_tree_anchor.begin << ':' << stub.token_tree_anchor.end
               << " token_count=" << stub.token_count
               << " token_buffer_materialized="
               << (stub.token_buffer_materialized ? "yes" : "no")
               << " token_records_available="
               << (stub.token_records_available ? "yes" : "no")
               << " parser_admitted=" << (stub.parser_admitted ? "yes" : "no")
               << " parse_ready=" << (stub.parse_ready ? "yes" : "no")
               << " parser_consumable=" << (stub.parser_consumable ? "yes" : "no")
               << " generated_part_parsed="
               << (stub.generated_part_parsed ? "yes" : "no")
               << " generated_part_merged="
               << (stub.generated_part_merged ? "yes" : "no")
               << " emit_expanded_available="
               << (stub.emit_expanded_available ? "yes" : "no")
               << " debug_trace_available="
               << (stub.debug_trace_available ? "yes" : "no")
               << " source_map_available="
               << (stub.source_map_available ? "yes" : "no")
               << " user_generated_code="
               << (stub.produced_user_generated_code ? "yes" : "no")
               << " token_buffer_blocker=" << stub.token_buffer_blocker
               << " generated_part_parse_blocker=" << stub.generated_part_parse_blocker
               << " message=" << stub.user_message
               << " parse_gate_identity=" << query::debug_string(stub.parse_gate_identity)
               << " diagnostic_identity=" << query::debug_string(stub.diagnostic_identity)
               << " diagnostic_anchor="
               << query::debug_string(stub.diagnostic_anchor_identity)
               << " token_plan_identity=" << query::debug_string(stub.token_plan_identity)
               << " token_buffer_identity=" << query::debug_string(stub.token_buffer_identity)
               << " materialization_identity="
               << query::debug_string(stub.materialization_identity)
               << " generated_buffer_identity="
               << query::debug_string(stub.generated_buffer_identity)
               << " parse_config=" << query::debug_string(stub.parse_config_fingerprint)
               << " source_map_identity=" << query::debug_string(stub.source_map_identity)
               << " hygiene_mark=" << query::debug_string(stub.hygiene_mark)
               << " trace_identity=" << query::debug_string(stub.trace_identity) << '\n';
    }
    for (base::usize index = 0; index < result.parser_admission_report_entries.size(); ++index) {
        const ParserAdmissionDiagnosticReportEntry& entry = result.parser_admission_report_entries[index];
        stream << "  parser_admission_report_entry #" << index
               << " item=" << entry.item.value
               << " module=" << entry.module.value
               << " part=" << entry.part_index
               << " attribute_index=" << entry.attribute_index
               << " report_index=" << entry.report_index
               << " category=" << entry.blocker_category
               << " debug_projection=" << entry.debug_projection_name
               << " query_projection=" << entry.query_projection_name
               << " primary_anchor=" << entry.primary_anchor.source.value << ':'
               << entry.primary_anchor.begin << ':' << entry.primary_anchor.end
               << " token_tree_anchor=" << entry.token_tree_anchor.source.value << ':'
               << entry.token_tree_anchor.begin << ':' << entry.token_tree_anchor.end
               << " token_count=" << entry.token_count
               << " token_records_available="
               << (entry.token_records_available ? "yes" : "no")
               << " parser_admitted=" << (entry.parser_admitted ? "yes" : "no")
               << " report_visible=" << (entry.report_visible ? "yes" : "no")
               << " query_reusable=" << (entry.query_reusable ? "yes" : "no")
               << " parser_consumable=" << (entry.parser_consumable ? "yes" : "no")
               << " emit_expanded_available="
               << (entry.emit_expanded_available ? "yes" : "no")
               << " user_generated_code="
               << (entry.produced_user_generated_code ? "yes" : "no")
               << " parse_gate_identity=" << query::debug_string(entry.parse_gate_identity)
               << " diagnostic_identity=" << query::debug_string(entry.diagnostic_identity)
               << " diagnostic_anchor="
               << query::debug_string(entry.diagnostic_anchor_identity)
               << " report_entry_identity="
               << query::debug_string(entry.report_entry_identity) << '\n';
    }
    for (base::usize index = 0; index < result.parser_admission_reports.size(); ++index) {
        const ParserAdmissionDiagnosticReport& report = result.parser_admission_reports[index];
        stream << "  parser_admission_diagnostic_report #" << index
               << " module=" << report.module.value
               << " source_part=" << report.source_part_index
               << " policy=" << report.report_policy
               << " query=" << report.report_query_name
               << " entries=" << report.entry_count
               << " blocked_entries=" << report.blocked_entry_count
               << " derive_entries=" << report.derive_entry_count
               << " empty_entries=" << report.empty_entry_count
               << " token_record_available_entries="
               << report.token_record_available_entry_count
               << " query_reusable=" << (report.query_reusable ? "yes" : "no")
               << " report_visible=" << (report.report_visible ? "yes" : "no")
               << " source_anchor_ordered="
               << (report.source_anchor_ordered ? "yes" : "no")
               << " parser_admitted=" << (report.parser_admitted ? "yes" : "no")
               << " parse_ready=" << (report.parse_ready ? "yes" : "no")
               << " parser_consumable=" << (report.parser_consumable ? "yes" : "no")
               << " emit_expanded_available="
               << (report.emit_expanded_available ? "yes" : "no")
               << " debug_trace_available="
               << (report.debug_trace_available ? "yes" : "no")
               << " source_map_available="
               << (report.source_map_available ? "yes" : "no")
               << " user_generated_code="
               << (report.produced_user_generated_code ? "yes" : "no")
               << " blocker=" << report.blocked_reason
               << " report_identity=" << query::debug_string(report.report_identity)
               << " report_anchor_identity="
               << query::debug_string(report.report_anchor_identity)
               << " report_grouping_identity="
               << query::debug_string(report.report_grouping_identity)
               << " generated_buffer_identity="
               << query::debug_string(report.generated_buffer_identity)
               << " parse_config=" << query::debug_string(report.parse_config_fingerprint)
               << '\n';
    }
    for (base::usize index = 0; index < result.parser_readiness_preflight_entries.size(); ++index) {
        const GeneratedTokenParserReadinessPreflightEntry& entry =
            result.parser_readiness_preflight_entries[index];
        stream << "  parser_readiness_preflight_entry #" << index
               << " item=" << entry.item.value
               << " module=" << entry.module.value
               << " part=" << entry.part_index
               << " attribute_index=" << entry.attribute_index
               << " preflight_index=" << entry.preflight_index
               << " token_stream=" << entry.token_stream_name
               << " shape=" << entry.token_stream_shape
               << " token_count=" << entry.token_count
               << " token_records_available="
               << (entry.token_records_available ? "yes" : "no")
               << " token_indices_contiguous="
               << (entry.token_indices_contiguous ? "yes" : "no")
               << " delimiter_balance=" << entry.delimiter_balance_state
               << " delimiter_balanced=" << (entry.delimiter_balanced ? "yes" : "no")
               << " source_anchor_coverage=" << entry.source_anchor_coverage_state
               << " source_anchors_covered=" << (entry.source_anchors_covered ? "yes" : "no")
               << " parse_config_compatible="
               << (entry.parse_config_compatible ? "yes" : "no")
               << " hygiene_prerequisite_available="
               << (entry.hygiene_prerequisite_available ? "yes" : "no")
               << " source_map_prerequisite_available="
               << (entry.source_map_prerequisite_available ? "yes" : "no")
               << " diagnostic_projection_available="
               << (entry.diagnostic_projection_available ? "yes" : "no")
               << " parser_admitted=" << (entry.parser_admitted ? "yes" : "no")
               << " parse_ready=" << (entry.parse_ready ? "yes" : "no")
               << " parser_consumable=" << (entry.parser_consumable ? "yes" : "no")
               << " generated_part_parsed="
               << (entry.generated_part_parsed ? "yes" : "no")
               << " generated_part_merged="
               << (entry.generated_part_merged ? "yes" : "no")
               << " user_generated_code="
               << (entry.produced_user_generated_code ? "yes" : "no")
               << " policy=" << entry.readiness_policy
               << " blocker=" << entry.blocker_reason
               << " token_buffer_identity="
               << query::debug_string(entry.token_buffer_identity)
               << " materialization_identity="
               << query::debug_string(entry.materialization_identity)
               << " parse_gate_identity="
               << query::debug_string(entry.parse_gate_identity)
               << " diagnostic_identity="
               << query::debug_string(entry.diagnostic_identity)
               << " report_entry_identity="
               << query::debug_string(entry.report_entry_identity)
               << " preflight_identity="
               << query::debug_string(entry.preflight_identity)
               << '\n';
    }
    for (base::usize index = 0; index < result.parser_consumption_contract_gates.size(); ++index) {
        const GeneratedTokenParserConsumptionContractGate& gate =
            result.parser_consumption_contract_gates[index];
        stream << "  parser_consumption_contract_gate #" << index
               << " module=" << gate.module.value
               << " source_part=" << gate.source_part_index
               << " policy=" << gate.contract_policy
               << " query=" << gate.contract_query_name
               << " preflight_entries=" << gate.preflight_entry_count
               << " blocked_entries=" << gate.blocked_entry_count
               << " derive_entries=" << gate.derive_entry_count
               << " empty_entries=" << gate.empty_entry_count
               << " contiguous_index_entries=" << gate.contiguous_index_entry_count
               << " delimiter_balanced_entries=" << gate.delimiter_balanced_entry_count
               << " source_anchor_covered_entries=" << gate.source_anchor_covered_entry_count
               << " parse_config_compatible_entries="
               << gate.parse_config_compatible_entry_count
               << " diagnostic_projection_entries="
               << gate.diagnostic_projection_entry_count
               << " query_reusable=" << (gate.query_reusable ? "yes" : "no")
               << " contract_visible=" << (gate.contract_visible ? "yes" : "no")
               << " all_entries_structurally_checked="
               << (gate.all_entries_structurally_checked ? "yes" : "no")
               << " parser_admitted=" << (gate.parser_admitted ? "yes" : "no")
               << " parse_ready=" << (gate.parse_ready ? "yes" : "no")
               << " parser_consumable=" << (gate.parser_consumable ? "yes" : "no")
               << " generated_part_parsed="
               << (gate.generated_part_parsed ? "yes" : "no")
               << " generated_part_merged="
               << (gate.generated_part_merged ? "yes" : "no")
               << " sema_visible=" << (gate.sema_visible ? "yes" : "no")
               << " emit_expanded_available="
               << (gate.emit_expanded_available ? "yes" : "no")
               << " debug_trace_available="
               << (gate.debug_trace_available ? "yes" : "no")
               << " source_map_available="
               << (gate.source_map_available ? "yes" : "no")
               << " user_generated_code="
               << (gate.produced_user_generated_code ? "yes" : "no")
               << " blocker=" << gate.blocked_reason
               << " contract_identity="
               << query::debug_string(gate.contract_identity)
               << " contract_grouping_identity="
               << query::debug_string(gate.contract_grouping_identity)
               << " contract_anchor_identity="
               << query::debug_string(gate.contract_anchor_identity)
               << " report_identity=" << query::debug_string(gate.report_identity)
               << " generated_buffer_identity="
               << query::debug_string(gate.generated_buffer_identity)
               << " parse_config=" << query::debug_string(gate.parse_config_fingerprint)
               << '\n';
    }
    for (base::usize index = 0; index < result.macro_boundary_closure_reports.size(); ++index) {
        const MacroExpansionBoundaryClosureReport& report =
            result.macro_boundary_closure_reports[index];
        stream << "  macro_boundary_closure_report #" << index
               << " policy=" << report.closure_policy
               << " query=" << report.closure_query_name
               << " macro_inputs=" << report.macro_input_count
               << " generated_parts=" << report.generated_part_count
               << " parser_admission_reports=" << report.parser_admission_report_count
               << " parser_readiness_preflight_entries="
               << report.parser_readiness_preflight_entry_count
               << " parser_consumption_contract_gates="
               << report.parser_consumption_contract_gate_count
               << " blocked_contract_gates=" << report.blocked_contract_gate_count
               << " parser_consumable_contract_gates="
               << report.parser_consumable_contract_gate_count
               << " m21m_preflight_available="
               << (report.m21m_preflight_available ? "yes" : "no")
               << " m21n_contract_available="
               << (report.m21n_contract_available ? "yes" : "no")
               << " release_closure_complete="
               << (report.release_closure_complete ? "yes" : "no")
               << " query_reusable=" << (report.query_reusable ? "yes" : "no")
               << " closure_visible=" << (report.closure_visible ? "yes" : "no")
               << " parser_consumption_enabled="
               << (report.parser_consumption_enabled ? "yes" : "no")
               << " emit_expanded_available="
               << (report.emit_expanded_available ? "yes" : "no")
               << " debug_trace_available="
               << (report.debug_trace_available ? "yes" : "no")
               << " source_map_available="
               << (report.source_map_available ? "yes" : "no")
               << " standard_library_required="
               << (report.standard_library_required ? "yes" : "no")
               << " runtime_required=" << (report.runtime_required ? "yes" : "no")
               << " external_process_required="
               << (report.external_process_required ? "yes" : "no")
               << " user_generated_code="
               << (report.produced_user_generated_code ? "yes" : "no")
               << " blocker=" << report.blocked_reason
               << " closure_identity="
               << query::debug_string(report.closure_identity)
               << " closure_grouping_identity="
               << query::debug_string(report.closure_grouping_identity)
               << '\n';
    }
    for (base::usize index = 0; index < result.builtin_derive_expansion_admissions.size(); ++index) {
        const BuiltinDeriveExpansionAdmissionGate& gate =
            result.builtin_derive_expansion_admissions[index];
        stream << "  builtin_derive_expansion_admission_gate #" << index
               << " item=" << gate.item.value
               << " module=" << gate.module.value
               << " part=" << gate.part_index
               << " attribute_index=" << gate.attribute_index
               << " admission_index=" << gate.admission_index
               << " policy=" << gate.admission_policy
               << " kind=" << gate.admission_kind
               << " query=" << gate.query_name
               << " token_count=" << gate.token_count
               << " capability_candidates=" << gate.capability_candidate_count
               << " unsupported_candidates=" << gate.unsupported_candidate_count
               << " duplicate_candidates=" << gate.duplicate_candidate_count
               << " builtin_derive_input=" << (gate.builtin_derive_input ? "yes" : "no")
               << " compiler_owned=" << (gate.compiler_owned ? "yes" : "no")
               << " token_records_available="
               << (gate.token_records_available ? "yes" : "no")
               << " preflight_available=" << (gate.preflight_available ? "yes" : "no")
               << " admission_visible=" << (gate.admission_visible ? "yes" : "no")
               << " query_reusable=" << (gate.query_reusable ? "yes" : "no")
               << " parser_consumption_enabled="
               << (gate.parser_consumption_enabled ? "yes" : "no")
               << " external_process_required="
               << (gate.external_process_required ? "yes" : "no")
               << " standard_library_required="
               << (gate.standard_library_required ? "yes" : "no")
               << " runtime_required=" << (gate.runtime_required ? "yes" : "no")
               << " generated_source_text=" << (gate.generated_source_text ? "yes" : "no")
               << " user_generated_code="
               << (gate.produced_user_generated_code ? "yes" : "no")
               << " blocker=" << gate.blocker_reason
               << " token_buffer_identity="
               << query::debug_string(gate.token_buffer_identity)
               << " preflight_identity=" << query::debug_string(gate.preflight_identity)
               << " parse_gate_identity=" << query::debug_string(gate.parse_gate_identity)
               << " diagnostic_identity=" << query::debug_string(gate.diagnostic_identity)
               << " closure_identity=" << query::debug_string(gate.closure_identity)
               << " admission_identity=" << query::debug_string(gate.admission_identity)
               << '\n';
    }
    for (base::usize index = 0; index < result.builtin_derive_semantic_plans.size(); ++index) {
        const BuiltinDeriveSemanticExpansionPlan& plan =
            result.builtin_derive_semantic_plans[index];
        stream << "  builtin_derive_semantic_expansion_plan #" << index
               << " item=" << plan.item.value
               << " module=" << plan.module.value
               << " part=" << plan.part_index
               << " attribute_index=" << plan.attribute_index
               << " semantic_plan_index=" << plan.semantic_plan_index
               << " policy=" << plan.semantic_policy
               << " target_kind=" << plan.target_kind
               << " semantic_model=" << plan.semantic_model
               << " capabilities=" << plan.capability_count
               << " copy=" << plan.copy_capability_count
               << " eq=" << plan.eq_capability_count
               << " hash=" << plan.hash_capability_count
               << " builtin_derive_input=" << (plan.builtin_derive_input ? "yes" : "no")
               << " target_struct_or_enum="
               << (plan.target_struct_or_enum ? "yes" : "no")
               << " existing_builtin_path="
               << (plan.uses_existing_builtin_derive_capability_path ? "yes" : "no")
               << " requires_ast_mutation=" << (plan.requires_ast_mutation ? "yes" : "no")
               << " requires_generated_items="
               << (plan.requires_generated_items ? "yes" : "no")
               << " requires_standard_library="
               << (plan.requires_standard_library ? "yes" : "no")
               << " requires_runtime=" << (plan.requires_runtime ? "yes" : "no")
               << " external_process_required="
               << (plan.external_process_required ? "yes" : "no")
               << " parser_consumption_enabled="
               << (plan.parser_consumption_enabled ? "yes" : "no")
               << " user_generated_code="
               << (plan.produced_user_generated_code ? "yes" : "no")
               << " plan_visible=" << (plan.plan_visible ? "yes" : "no")
               << " query_reusable=" << (plan.query_reusable ? "yes" : "no")
               << " blocker=" << plan.blocker_reason
               << " token_buffer_identity="
               << query::debug_string(plan.token_buffer_identity)
               << " preflight_identity=" << query::debug_string(plan.preflight_identity)
               << " admission_identity=" << query::debug_string(plan.admission_identity)
               << " capability_set_identity="
               << query::debug_string(plan.capability_set_identity)
               << " semantic_plan_identity="
               << query::debug_string(plan.semantic_plan_identity)
               << '\n';
    }
    for (base::usize index = 0; index < result.builtin_derive_parser_release_gates.size(); ++index) {
        const BuiltinDeriveParserConsumptionReleaseGate& gate =
            result.builtin_derive_parser_release_gates[index];
        stream << "  builtin_derive_parser_consumption_release_gate #" << index
               << " module=" << gate.module.value
               << " source_part=" << gate.source_part_index
               << " policy=" << gate.release_policy
               << " query=" << gate.release_query_name
               << " admissions=" << gate.admission_count
               << " derive_admissions=" << gate.derive_admission_count
               << " semantic_plans=" << gate.semantic_plan_count
               << " capabilities=" << gate.capability_total_count
               << " parser_consumable_contracts="
               << gate.parser_consumable_contract_count
               << " rollback_diagnostics_available="
               << (gate.rollback_diagnostics_available ? "yes" : "no")
               << " debug_trace_prerequisite_available="
               << (gate.debug_trace_prerequisite_available ? "yes" : "no")
               << " source_map_prerequisite_available="
               << (gate.source_map_prerequisite_available ? "yes" : "no")
               << " hygiene_prerequisite_available="
               << (gate.hygiene_prerequisite_available ? "yes" : "no")
               << " parser_consumption_enabled="
               << (gate.parser_consumption_enabled ? "yes" : "no")
               << " generated_part_parsed="
               << (gate.generated_part_parsed ? "yes" : "no")
               << " generated_part_merged="
               << (gate.generated_part_merged ? "yes" : "no")
               << " emit_expanded_available="
               << (gate.emit_expanded_available ? "yes" : "no")
               << " debug_trace_available="
               << (gate.debug_trace_available ? "yes" : "no")
               << " source_map_available="
               << (gate.source_map_available ? "yes" : "no")
               << " standard_library_required="
               << (gate.standard_library_required ? "yes" : "no")
               << " runtime_required=" << (gate.runtime_required ? "yes" : "no")
               << " external_process_required="
               << (gate.external_process_required ? "yes" : "no")
               << " user_generated_code="
               << (gate.produced_user_generated_code ? "yes" : "no")
               << " release_visible=" << (gate.release_visible ? "yes" : "no")
               << " query_reusable=" << (gate.query_reusable ? "yes" : "no")
               << " blocker=" << gate.blocked_reason
               << " contract_identity=" << query::debug_string(gate.contract_identity)
               << " closure_identity=" << query::debug_string(gate.closure_identity)
               << " admission_group_identity="
               << query::debug_string(gate.admission_group_identity)
               << " semantic_plan_group_identity="
               << query::debug_string(gate.semantic_plan_group_identity)
               << " release_gate_identity="
               << query::debug_string(gate.release_gate_identity)
               << '\n';
    }
    for (base::usize index = 0; index < result.builtin_derive_release_hardening_matrices.size(); ++index) {
        const BuiltinDeriveReleaseHardeningMatrix& matrix =
            result.builtin_derive_release_hardening_matrices[index];
        stream << "  builtin_derive_release_hardening_matrix #" << index
               << " module=" << matrix.module.value
               << " source_part=" << matrix.source_part_index
               << " policy=" << matrix.hardening_policy
               << " query=" << matrix.hardening_query_name
               << " part_local_admissions=" << matrix.part_local_admission_count
               << " part_local_derive_admissions=" << matrix.part_local_derive_admission_count
               << " part_local_semantic_plans=" << matrix.part_local_semantic_plan_count
               << " part_local_release_gates=" << matrix.part_local_release_gate_count
               << " global_admissions=" << matrix.global_admission_count
               << " global_semantic_plans=" << matrix.global_semantic_plan_count
               << " global_generated_parts=" << matrix.global_generated_part_count
               << " cross_part_admissions=" << matrix.cross_part_admission_count
               << " cross_part_semantic_plans=" << matrix.cross_part_semantic_plan_count
               << " part_locality_preserved="
               << (matrix.part_locality_preserved ? "yes" : "no")
               << " multi_item_matrix_available="
               << (matrix.multi_item_matrix_available ? "yes" : "no")
               << " negative_matrix_complete="
               << (matrix.negative_matrix_complete ? "yes" : "no")
               << " release_remains_blocked="
               << (matrix.release_remains_blocked ? "yes" : "no")
               << " parser_consumption_enabled="
               << (matrix.parser_consumption_enabled ? "yes" : "no")
               << " generated_part_parsed="
               << (matrix.generated_part_parsed ? "yes" : "no")
               << " generated_part_merged="
               << (matrix.generated_part_merged ? "yes" : "no")
               << " emit_expanded_available="
               << (matrix.emit_expanded_available ? "yes" : "no")
               << " debug_trace_available="
               << (matrix.debug_trace_available ? "yes" : "no")
               << " source_map_available="
               << (matrix.source_map_available ? "yes" : "no")
               << " standard_library_required="
               << (matrix.standard_library_required ? "yes" : "no")
               << " runtime_required=" << (matrix.runtime_required ? "yes" : "no")
               << " external_process_required="
               << (matrix.external_process_required ? "yes" : "no")
               << " user_generated_code="
               << (matrix.produced_user_generated_code ? "yes" : "no")
               << " matrix_visible=" << (matrix.matrix_visible ? "yes" : "no")
               << " query_reusable=" << (matrix.query_reusable ? "yes" : "no")
               << " blocker=" << matrix.blocked_reason
               << " release_gate_identity=" << query::debug_string(matrix.release_gate_identity)
               << " admission_group_identity="
               << query::debug_string(matrix.admission_group_identity)
               << " semantic_plan_group_identity="
               << query::debug_string(matrix.semantic_plan_group_identity)
               << " hardening_matrix_identity="
               << query::debug_string(matrix.hardening_matrix_identity)
               << '\n';
    }
    for (base::usize index = 0; index < result.builtin_derive_debug_dump_contracts.size(); ++index) {
        const BuiltinDeriveDebugDumpStabilityContract& contract =
            result.builtin_derive_debug_dump_contracts[index];
        stream << "  builtin_derive_debug_dump_stability_contract #" << index
               << " module=" << contract.module.value
               << " source_part=" << contract.source_part_index
               << " policy=" << contract.debug_dump_policy
               << " query=" << contract.debug_dump_query_name
               << " dump_sections=" << contract.dump_section_count
               << " stable_ordering_available="
               << (contract.stable_ordering_available ? "yes" : "no")
               << " identity_projection_available="
               << (contract.identity_projection_available ? "yes" : "no")
               << " summary_projection_available="
               << (contract.summary_projection_available ? "yes" : "no")
               << " drift_debuggable=" << (contract.drift_debuggable ? "yes" : "no")
               << " debug_dump_contract_complete="
               << (contract.debug_dump_contract_complete ? "yes" : "no")
               << " emit_expanded_available="
               << (contract.emit_expanded_available ? "yes" : "no")
               << " debug_trace_available="
               << (contract.debug_trace_available ? "yes" : "no")
               << " source_map_available="
               << (contract.source_map_available ? "yes" : "no")
               << " parser_consumption_enabled="
               << (contract.parser_consumption_enabled ? "yes" : "no")
               << " standard_library_required="
               << (contract.standard_library_required ? "yes" : "no")
               << " runtime_required=" << (contract.runtime_required ? "yes" : "no")
               << " external_process_required="
               << (contract.external_process_required ? "yes" : "no")
               << " user_generated_code="
               << (contract.produced_user_generated_code ? "yes" : "no")
               << " contract_visible=" << (contract.contract_visible ? "yes" : "no")
               << " query_reusable=" << (contract.query_reusable ? "yes" : "no")
               << " blocker=" << contract.blocked_reason
               << " release_gate_identity="
               << query::debug_string(contract.release_gate_identity)
               << " hardening_matrix_identity="
               << query::debug_string(contract.hardening_matrix_identity)
               << " debug_dump_contract_identity="
               << query::debug_string(contract.debug_dump_contract_identity)
               << '\n';
    }
    for (base::usize index = 0; index < result.builtin_derive_rollback_diagnostic_gates.size(); ++index) {
        const BuiltinDeriveRollbackDiagnosticDesignGate& gate =
            result.builtin_derive_rollback_diagnostic_gates[index];
        stream << "  builtin_derive_rollback_diagnostic_design_gate #" << index
               << " module=" << gate.module.value
               << " source_part=" << gate.source_part_index
               << " policy=" << gate.rollback_policy
               << " query=" << gate.rollback_query_name
               << " diagnostic_projections=" << gate.diagnostic_projection_count
               << " diagnostic_report_entries=" << gate.diagnostic_report_entry_count
               << " blocked_diagnostics=" << gate.blocked_diagnostic_count
               << " derive_diagnostics=" << gate.derive_diagnostic_count
               << " empty_diagnostics=" << gate.empty_diagnostic_count
               << " parser_consumption_contracts=" << gate.parser_consumption_contract_count
               << " rollback_diagnostic_design_available="
               << (gate.rollback_diagnostic_design_available ? "yes" : "no")
               << " diagnostic_grouping_available="
               << (gate.diagnostic_grouping_available ? "yes" : "no")
               << " source_anchor_available="
               << (gate.source_anchor_available ? "yes" : "no")
               << " token_tree_anchor_available="
               << (gate.token_tree_anchor_available ? "yes" : "no")
               << " debug_dump_contract_available="
               << (gate.debug_dump_contract_available ? "yes" : "no")
               << " release_rollback_plan_complete="
               << (gate.release_rollback_plan_complete ? "yes" : "no")
               << " rollback_execution_enabled="
               << (gate.rollback_execution_enabled ? "yes" : "no")
               << " parser_consumption_enabled="
               << (gate.parser_consumption_enabled ? "yes" : "no")
               << " generated_part_parsed="
               << (gate.generated_part_parsed ? "yes" : "no")
               << " generated_part_merged="
               << (gate.generated_part_merged ? "yes" : "no")
               << " emit_expanded_available="
               << (gate.emit_expanded_available ? "yes" : "no")
               << " debug_trace_available="
               << (gate.debug_trace_available ? "yes" : "no")
               << " source_map_available="
               << (gate.source_map_available ? "yes" : "no")
               << " standard_library_required="
               << (gate.standard_library_required ? "yes" : "no")
               << " runtime_required=" << (gate.runtime_required ? "yes" : "no")
               << " external_process_required="
               << (gate.external_process_required ? "yes" : "no")
               << " user_generated_code="
               << (gate.produced_user_generated_code ? "yes" : "no")
               << " rollback_gate_visible="
               << (gate.rollback_gate_visible ? "yes" : "no")
               << " query_reusable=" << (gate.query_reusable ? "yes" : "no")
               << " blocker=" << gate.blocked_reason
               << " parser_consumption_contract_identity="
               << query::debug_string(gate.parser_consumption_contract_identity)
               << " release_gate_identity=" << query::debug_string(gate.release_gate_identity)
               << " hardening_matrix_identity="
               << query::debug_string(gate.hardening_matrix_identity)
               << " debug_dump_contract_identity="
               << query::debug_string(gate.debug_dump_contract_identity)
               << " rollback_gate_identity="
               << query::debug_string(gate.rollback_gate_identity)
               << '\n';
    }
    for (base::usize index = 0;
         index < result.builtin_derive_parser_consumption_admission_protocols.size();
         ++index) {
        const BuiltinDeriveParserConsumptionAdmissionProtocol& protocol =
            result.builtin_derive_parser_consumption_admission_protocols[index];
        stream << "  builtin_derive_parser_consumption_admission_protocol #" << index
               << " module=" << protocol.module.value
               << " source_part=" << protocol.source_part_index
               << " policy=" << protocol.admission_policy
               << " query=" << protocol.admission_query_name
               << " token_buffers=" << protocol.token_buffer_count
               << " token_records=" << protocol.token_record_count
               << " derive_candidates=" << protocol.derive_candidate_count
               << " empty_candidates=" << protocol.empty_candidate_count
               << " blocked_diagnostics=" << protocol.blocked_diagnostic_count
               << " release_gate_available="
               << (protocol.release_gate_available ? "yes" : "no")
               << " rollback_gate_available="
               << (protocol.rollback_gate_available ? "yes" : "no")
               << " parser_contract_available="
               << (protocol.parser_contract_available ? "yes" : "no")
               << " deterministic_order_available="
               << (protocol.deterministic_order_available ? "yes" : "no")
               << " generated_tokens_checkpointed="
               << (protocol.generated_tokens_checkpointed ? "yes" : "no")
               << " admission_protocol_complete="
               << (protocol.admission_protocol_complete ? "yes" : "no")
               << " parser_consumption_enabled="
               << (protocol.parser_consumption_enabled ? "yes" : "no")
               << " parser_admitted=" << (protocol.parser_admitted ? "yes" : "no")
               << " generated_part_parsed="
               << (protocol.generated_part_parsed ? "yes" : "no")
               << " generated_part_merged="
               << (protocol.generated_part_merged ? "yes" : "no")
               << " emit_expanded_available="
               << (protocol.emit_expanded_available ? "yes" : "no")
               << " debug_trace_available="
               << (protocol.debug_trace_available ? "yes" : "no")
               << " source_map_available="
               << (protocol.source_map_available ? "yes" : "no")
               << " standard_library_required="
               << (protocol.standard_library_required ? "yes" : "no")
               << " runtime_required=" << (protocol.runtime_required ? "yes" : "no")
               << " external_process_required="
               << (protocol.external_process_required ? "yes" : "no")
               << " user_generated_code="
               << (protocol.produced_user_generated_code ? "yes" : "no")
               << " protocol_visible=" << (protocol.protocol_visible ? "yes" : "no")
               << " query_reusable=" << (protocol.query_reusable ? "yes" : "no")
               << " blocker=" << protocol.blocked_reason
               << " parser_consumption_contract_identity="
               << query::debug_string(protocol.parser_consumption_contract_identity)
               << " release_gate_identity=" << query::debug_string(protocol.release_gate_identity)
               << " rollback_gate_identity=" << query::debug_string(protocol.rollback_gate_identity)
               << " admission_protocol_identity="
               << query::debug_string(protocol.admission_protocol_identity)
               << '\n';
    }
    for (base::usize index = 0; index < result.builtin_derive_checkpoint_rollback_protocols.size();
         ++index) {
        const BuiltinDeriveParserConsumptionCheckpointRollbackProtocol& protocol =
            result.builtin_derive_checkpoint_rollback_protocols[index];
        stream << "  builtin_derive_checkpoint_rollback_protocol #" << index
               << " module=" << protocol.module.value
               << " source_part=" << protocol.source_part_index
               << " policy=" << protocol.checkpoint_policy
               << " query=" << protocol.checkpoint_query_name
               << " checkpoints=" << protocol.checkpoint_count
               << " rollback_plans=" << protocol.rollback_plan_count
               << " token_records=" << protocol.token_record_count
               << " diagnostic_anchors=" << protocol.diagnostic_anchor_count
               << " parser_state_checkpoint_available="
               << (protocol.parser_state_checkpoint_available ? "yes" : "no")
               << " token_cursor_checkpoint_available="
               << (protocol.token_cursor_checkpoint_available ? "yes" : "no")
               << " generated_part_checkpoint_available="
               << (protocol.generated_part_checkpoint_available ? "yes" : "no")
               << " diagnostic_replay_available="
               << (protocol.diagnostic_replay_available ? "yes" : "no")
               << " rollback_protocol_complete="
               << (protocol.rollback_protocol_complete ? "yes" : "no")
               << " rollback_execution_enabled="
               << (protocol.rollback_execution_enabled ? "yes" : "no")
               << " parser_consumption_enabled="
               << (protocol.parser_consumption_enabled ? "yes" : "no")
               << " generated_part_parsed="
               << (protocol.generated_part_parsed ? "yes" : "no")
               << " generated_part_merged="
               << (protocol.generated_part_merged ? "yes" : "no")
               << " emit_expanded_available="
               << (protocol.emit_expanded_available ? "yes" : "no")
               << " debug_trace_available="
               << (protocol.debug_trace_available ? "yes" : "no")
               << " source_map_available="
               << (protocol.source_map_available ? "yes" : "no")
               << " standard_library_required="
               << (protocol.standard_library_required ? "yes" : "no")
               << " runtime_required=" << (protocol.runtime_required ? "yes" : "no")
               << " external_process_required="
               << (protocol.external_process_required ? "yes" : "no")
               << " user_generated_code="
               << (protocol.produced_user_generated_code ? "yes" : "no")
               << " protocol_visible=" << (protocol.protocol_visible ? "yes" : "no")
               << " query_reusable=" << (protocol.query_reusable ? "yes" : "no")
               << " blocker=" << protocol.blocked_reason
               << " admission_protocol_identity="
               << query::debug_string(protocol.admission_protocol_identity)
               << " rollback_gate_identity=" << query::debug_string(protocol.rollback_gate_identity)
               << " checkpoint_protocol_identity="
               << query::debug_string(protocol.checkpoint_protocol_identity)
               << '\n';
    }
    for (base::usize index = 0; index < result.builtin_derive_preconsumption_verification_closures.size();
         ++index) {
        const BuiltinDeriveParserPreConsumptionVerificationClosure& closure =
            result.builtin_derive_preconsumption_verification_closures[index];
        stream << "  builtin_derive_preconsumption_verification_closure #" << index
               << " module=" << closure.module.value
               << " source_part=" << closure.source_part_index
               << " policy=" << closure.verification_policy
               << " query=" << closure.verification_query_name
               << " admission_protocols=" << closure.admission_protocol_count
               << " checkpoint_protocols=" << closure.checkpoint_protocol_count
               << " hardening_matrices=" << closure.hardening_matrix_count
               << " debug_dump_contracts=" << closure.debug_dump_contract_count
               << " rollback_gates=" << closure.rollback_gate_count
               << " admission_protocol_available="
               << (closure.admission_protocol_available ? "yes" : "no")
               << " checkpoint_protocol_available="
               << (closure.checkpoint_protocol_available ? "yes" : "no")
               << " release_hardening_available="
               << (closure.release_hardening_available ? "yes" : "no")
               << " debug_dump_contract_available="
               << (closure.debug_dump_contract_available ? "yes" : "no")
               << " rollback_gate_available="
               << (closure.rollback_gate_available ? "yes" : "no")
               << " verification_closure_complete="
               << (closure.verification_closure_complete ? "yes" : "no")
               << " parser_consumption_enabled="
               << (closure.parser_consumption_enabled ? "yes" : "no")
               << " generated_part_parsed="
               << (closure.generated_part_parsed ? "yes" : "no")
               << " generated_part_merged="
               << (closure.generated_part_merged ? "yes" : "no")
               << " sema_visible=" << (closure.sema_visible ? "yes" : "no")
               << " emit_expanded_available="
               << (closure.emit_expanded_available ? "yes" : "no")
               << " debug_trace_available="
               << (closure.debug_trace_available ? "yes" : "no")
               << " source_map_available="
               << (closure.source_map_available ? "yes" : "no")
               << " standard_library_required="
               << (closure.standard_library_required ? "yes" : "no")
               << " runtime_required=" << (closure.runtime_required ? "yes" : "no")
               << " external_process_required="
               << (closure.external_process_required ? "yes" : "no")
               << " user_generated_code="
               << (closure.produced_user_generated_code ? "yes" : "no")
               << " closure_visible=" << (closure.closure_visible ? "yes" : "no")
               << " query_reusable=" << (closure.query_reusable ? "yes" : "no")
               << " blocker=" << closure.blocked_reason
               << " admission_protocol_identity="
               << query::debug_string(closure.admission_protocol_identity)
               << " checkpoint_protocol_identity="
               << query::debug_string(closure.checkpoint_protocol_identity)
               << " debug_dump_contract_identity="
               << query::debug_string(closure.debug_dump_contract_identity)
               << " verification_closure_identity="
               << query::debug_string(closure.verification_closure_identity)
               << '\n';
    }
    for (base::usize index = 0; index < result.builtin_derive_controlled_dry_run_adapters.size();
         ++index) {
        const BuiltinDeriveControlledParserDryRunAdapter& adapter =
            result.builtin_derive_controlled_dry_run_adapters[index];
        stream << "  builtin_derive_controlled_dry_run_adapter #" << index
               << " module=" << adapter.module.value
               << " source_part=" << adapter.source_part_index
               << " policy=" << adapter.adapter_policy
               << " query=" << adapter.adapter_query_name
               << " token_records=" << adapter.token_record_count
               << " diagnostic_anchors=" << adapter.diagnostic_anchor_count
               << " prerequisites=" << adapter.prerequisite_count
               << " verification_closure_available="
               << (adapter.verification_closure_available ? "yes" : "no")
               << " admission_protocol_available="
               << (adapter.admission_protocol_available ? "yes" : "no")
               << " checkpoint_protocol_available="
               << (adapter.checkpoint_protocol_available ? "yes" : "no")
               << " compiler_owned_tokens_available="
               << (adapter.compiler_owned_tokens_available ? "yes" : "no")
               << " diagnostic_replay_available="
               << (adapter.diagnostic_replay_available ? "yes" : "no")
               << " dry_run_adapter_complete="
               << (adapter.dry_run_adapter_complete ? "yes" : "no")
               << " dry_run_executed=" << (adapter.dry_run_executed ? "yes" : "no")
               << " parser_consumption_enabled="
               << (adapter.parser_consumption_enabled ? "yes" : "no")
               << " parser_admitted=" << (adapter.parser_admitted ? "yes" : "no")
               << " generated_part_parsed="
               << (adapter.generated_part_parsed ? "yes" : "no")
               << " generated_part_merged="
               << (adapter.generated_part_merged ? "yes" : "no")
               << " sema_visible=" << (adapter.sema_visible ? "yes" : "no")
               << " emit_expanded_available="
               << (adapter.emit_expanded_available ? "yes" : "no")
               << " debug_trace_available="
               << (adapter.debug_trace_available ? "yes" : "no")
               << " source_map_available="
               << (adapter.source_map_available ? "yes" : "no")
               << " standard_library_required="
               << (adapter.standard_library_required ? "yes" : "no")
               << " runtime_required=" << (adapter.runtime_required ? "yes" : "no")
               << " external_process_required="
               << (adapter.external_process_required ? "yes" : "no")
               << " user_generated_code="
               << (adapter.produced_user_generated_code ? "yes" : "no")
               << " adapter_visible=" << (adapter.adapter_visible ? "yes" : "no")
               << " query_reusable=" << (adapter.query_reusable ? "yes" : "no")
               << " blocker=" << adapter.blocked_reason
               << " verification_closure_identity="
               << query::debug_string(adapter.verification_closure_identity)
               << " admission_protocol_identity="
               << query::debug_string(adapter.admission_protocol_identity)
               << " checkpoint_protocol_identity="
               << query::debug_string(adapter.checkpoint_protocol_identity)
               << " dry_run_adapter_identity="
               << query::debug_string(adapter.dry_run_adapter_identity)
               << '\n';
    }
    for (base::usize index = 0; index < result.builtin_derive_dry_run_rollback_replays.size();
         ++index) {
        const BuiltinDeriveDryRunRollbackDiagnosticReplay& replay =
            result.builtin_derive_dry_run_rollback_replays[index];
        stream << "  builtin_derive_dry_run_rollback_replay #" << index
               << " module=" << replay.module.value
               << " source_part=" << replay.source_part_index
               << " policy=" << replay.replay_policy
               << " query=" << replay.replay_query_name
               << " diagnostic_anchors=" << replay.diagnostic_anchor_count
               << " report_entries=" << replay.report_entry_count
               << " planned_replays=" << replay.planned_replay_count
               << " executed_replays=" << replay.executed_replay_count
               << " dry_run_adapter_available="
               << (replay.dry_run_adapter_available ? "yes" : "no")
               << " checkpoint_protocol_available="
               << (replay.checkpoint_protocol_available ? "yes" : "no")
               << " rollback_gate_available="
               << (replay.rollback_gate_available ? "yes" : "no")
               << " diagnostic_replay_plan_available="
               << (replay.diagnostic_replay_plan_available ? "yes" : "no")
               << " replay_protocol_complete="
               << (replay.replay_protocol_complete ? "yes" : "no")
               << " replay_execution_enabled="
               << (replay.replay_execution_enabled ? "yes" : "no")
               << " dry_run_executed=" << (replay.dry_run_executed ? "yes" : "no")
               << " parser_consumption_enabled="
               << (replay.parser_consumption_enabled ? "yes" : "no")
               << " generated_part_parsed="
               << (replay.generated_part_parsed ? "yes" : "no")
               << " generated_part_merged="
               << (replay.generated_part_merged ? "yes" : "no")
               << " sema_visible=" << (replay.sema_visible ? "yes" : "no")
               << " emit_expanded_available="
               << (replay.emit_expanded_available ? "yes" : "no")
               << " debug_trace_available="
               << (replay.debug_trace_available ? "yes" : "no")
               << " source_map_available="
               << (replay.source_map_available ? "yes" : "no")
               << " standard_library_required="
               << (replay.standard_library_required ? "yes" : "no")
               << " runtime_required=" << (replay.runtime_required ? "yes" : "no")
               << " external_process_required="
               << (replay.external_process_required ? "yes" : "no")
               << " user_generated_code="
               << (replay.produced_user_generated_code ? "yes" : "no")
               << " replay_visible=" << (replay.replay_visible ? "yes" : "no")
               << " query_reusable=" << (replay.query_reusable ? "yes" : "no")
               << " blocker=" << replay.blocked_reason
               << " dry_run_adapter_identity="
               << query::debug_string(replay.dry_run_adapter_identity)
               << " checkpoint_protocol_identity="
               << query::debug_string(replay.checkpoint_protocol_identity)
               << " rollback_gate_identity="
               << query::debug_string(replay.rollback_gate_identity)
               << " replay_protocol_identity="
               << query::debug_string(replay.replay_protocol_identity)
               << '\n';
    }
    for (base::usize index = 0; index < result.builtin_derive_dry_run_negative_matrices.size();
         ++index) {
        const BuiltinDeriveDryRunNegativeMatrixClosure& matrix =
            result.builtin_derive_dry_run_negative_matrices[index];
        stream << "  builtin_derive_dry_run_negative_matrix #" << index
               << " module=" << matrix.module.value
               << " source_part=" << matrix.source_part_index
               << " policy=" << matrix.matrix_policy
               << " query=" << matrix.matrix_query_name
               << " dry_run_adapters=" << matrix.dry_run_adapter_count
               << " rollback_replays=" << matrix.rollback_replay_count
               << " verification_closures=" << matrix.verification_closure_count
               << " negative_cases=" << matrix.negative_case_count
               << " parser_consumable_cases=" << matrix.parser_consumable_case_count
               << " dry_run_adapter_available="
               << (matrix.dry_run_adapter_available ? "yes" : "no")
               << " rollback_replay_available="
               << (matrix.rollback_replay_available ? "yes" : "no")
               << " verification_closure_available="
               << (matrix.verification_closure_available ? "yes" : "no")
               << " negative_matrix_complete="
               << (matrix.negative_matrix_complete ? "yes" : "no")
               << " dry_run_executed=" << (matrix.dry_run_executed ? "yes" : "no")
               << " parser_consumption_enabled="
               << (matrix.parser_consumption_enabled ? "yes" : "no")
               << " parser_admitted=" << (matrix.parser_admitted ? "yes" : "no")
               << " generated_part_parsed="
               << (matrix.generated_part_parsed ? "yes" : "no")
               << " generated_part_merged="
               << (matrix.generated_part_merged ? "yes" : "no")
               << " sema_visible=" << (matrix.sema_visible ? "yes" : "no")
               << " emit_expanded_available="
               << (matrix.emit_expanded_available ? "yes" : "no")
               << " debug_trace_available="
               << (matrix.debug_trace_available ? "yes" : "no")
               << " source_map_available="
               << (matrix.source_map_available ? "yes" : "no")
               << " standard_library_required="
               << (matrix.standard_library_required ? "yes" : "no")
               << " runtime_required=" << (matrix.runtime_required ? "yes" : "no")
               << " external_process_required="
               << (matrix.external_process_required ? "yes" : "no")
               << " user_generated_code="
               << (matrix.produced_user_generated_code ? "yes" : "no")
               << " matrix_visible=" << (matrix.matrix_visible ? "yes" : "no")
               << " query_reusable=" << (matrix.query_reusable ? "yes" : "no")
               << " blocker=" << matrix.blocked_reason
               << " dry_run_adapter_identity="
               << query::debug_string(matrix.dry_run_adapter_identity)
               << " rollback_replay_identity="
               << query::debug_string(matrix.rollback_replay_identity)
               << " verification_closure_identity="
               << query::debug_string(matrix.verification_closure_identity)
               << " negative_matrix_identity="
               << query::debug_string(matrix.negative_matrix_identity)
               << '\n';
    }
    for (base::usize index = 0; index < result.builtin_derive_parser_dry_run_sessions.size();
         ++index) {
        const BuiltinDeriveParserDryRunSessionBoundary& session =
            result.builtin_derive_parser_dry_run_sessions[index];
        stream << "  builtin_derive_parser_dry_run_session #" << index
               << " module=" << session.module.value
               << " source_part=" << session.source_part_index
               << " policy=" << session.session_policy
               << " query=" << session.session_query_name
               << " token_buffer_candidates=" << session.token_buffer_candidate_count
               << " token_records=" << session.token_record_count
               << " diagnostic_anchors=" << session.diagnostic_anchor_count
               << " parser_state_snapshots=" << session.parser_state_snapshot_count
               << " committed_parses=" << session.committed_parse_count
               << " dry_run_adapter_available="
               << (session.dry_run_adapter_available ? "yes" : "no")
               << " negative_matrix_available="
               << (session.negative_matrix_available ? "yes" : "no")
               << " compiler_owned_token_stream_available="
               << (session.compiler_owned_token_stream_available ? "yes" : "no")
               << " sandbox_available=" << (session.sandbox_available ? "yes" : "no")
               << " check_only=" << (session.check_only ? "yes" : "no")
               << " dry_run_session_complete="
               << (session.dry_run_session_complete ? "yes" : "no")
               << " dry_run_executed=" << (session.dry_run_executed ? "yes" : "no")
               << " session_committed=" << (session.session_committed ? "yes" : "no")
               << " parser_consumption_enabled="
               << (session.parser_consumption_enabled ? "yes" : "no")
               << " parser_admitted=" << (session.parser_admitted ? "yes" : "no")
               << " parser_cursor_advanced="
               << (session.parser_cursor_advanced ? "yes" : "no")
               << " generated_part_parsed="
               << (session.generated_part_parsed ? "yes" : "no")
               << " generated_part_merged="
               << (session.generated_part_merged ? "yes" : "no")
               << " ast_mutated=" << (session.ast_mutated ? "yes" : "no")
               << " sema_visible=" << (session.sema_visible ? "yes" : "no")
               << " emit_expanded_available="
               << (session.emit_expanded_available ? "yes" : "no")
               << " debug_trace_available="
               << (session.debug_trace_available ? "yes" : "no")
               << " source_map_available="
               << (session.source_map_available ? "yes" : "no")
               << " standard_library_required="
               << (session.standard_library_required ? "yes" : "no")
               << " runtime_required=" << (session.runtime_required ? "yes" : "no")
               << " external_process_required="
               << (session.external_process_required ? "yes" : "no")
               << " user_generated_code="
               << (session.produced_user_generated_code ? "yes" : "no")
               << " session_visible=" << (session.session_visible ? "yes" : "no")
               << " query_reusable=" << (session.query_reusable ? "yes" : "no")
               << " blocker=" << session.blocked_reason
               << " dry_run_adapter_identity="
               << query::debug_string(session.dry_run_adapter_identity)
               << " negative_matrix_identity="
               << query::debug_string(session.negative_matrix_identity)
               << " generated_buffer_identity="
               << query::debug_string(session.generated_buffer_identity)
               << " parse_config_fingerprint="
               << query::debug_string(session.parse_config_fingerprint)
               << " dry_run_session_identity="
               << query::debug_string(session.dry_run_session_identity)
               << '\n';
    }
    for (base::usize index = 0; index < result.builtin_derive_token_cursor_snapshot_proofs.size();
         ++index) {
        const BuiltinDeriveTokenCursorSnapshotRollbackProof& proof =
            result.builtin_derive_token_cursor_snapshot_proofs[index];
        stream << "  builtin_derive_token_cursor_snapshot_proof #" << index
               << " module=" << proof.module.value
               << " source_part=" << proof.source_part_index
               << " policy=" << proof.snapshot_policy
               << " query=" << proof.snapshot_query_name
               << " token_records=" << proof.token_record_count
               << " checkpoints=" << proof.checkpoint_count
               << " cursor_snapshots=" << proof.cursor_snapshot_count
               << " parser_state_snapshots=" << proof.parser_state_snapshot_count
               << " rollback_proofs=" << proof.rollback_proof_count
               << " cursor_commits=" << proof.cursor_commit_count
               << " dry_run_session_available="
               << (proof.dry_run_session_available ? "yes" : "no")
               << " checkpoint_protocol_available="
               << (proof.checkpoint_protocol_available ? "yes" : "no")
               << " rollback_replay_available="
               << (proof.rollback_replay_available ? "yes" : "no")
               << " token_cursor_snapshot_available="
               << (proof.token_cursor_snapshot_available ? "yes" : "no")
               << " parser_state_snapshot_available="
               << (proof.parser_state_snapshot_available ? "yes" : "no")
               << " rollback_proof_complete="
               << (proof.rollback_proof_complete ? "yes" : "no")
               << " replay_execution_enabled="
               << (proof.replay_execution_enabled ? "yes" : "no")
               << " rollback_execution_enabled="
               << (proof.rollback_execution_enabled ? "yes" : "no")
               << " dry_run_executed=" << (proof.dry_run_executed ? "yes" : "no")
               << " parser_cursor_advanced="
               << (proof.parser_cursor_advanced ? "yes" : "no")
               << " session_committed=" << (proof.session_committed ? "yes" : "no")
               << " parser_consumption_enabled="
               << (proof.parser_consumption_enabled ? "yes" : "no")
               << " parser_admitted=" << (proof.parser_admitted ? "yes" : "no")
               << " generated_part_parsed="
               << (proof.generated_part_parsed ? "yes" : "no")
               << " generated_part_merged="
               << (proof.generated_part_merged ? "yes" : "no")
               << " ast_mutated=" << (proof.ast_mutated ? "yes" : "no")
               << " sema_visible=" << (proof.sema_visible ? "yes" : "no")
               << " standard_library_required="
               << (proof.standard_library_required ? "yes" : "no")
               << " runtime_required=" << (proof.runtime_required ? "yes" : "no")
               << " external_process_required="
               << (proof.external_process_required ? "yes" : "no")
               << " user_generated_code="
               << (proof.produced_user_generated_code ? "yes" : "no")
               << " proof_visible=" << (proof.proof_visible ? "yes" : "no")
               << " query_reusable=" << (proof.query_reusable ? "yes" : "no")
               << " blocker=" << proof.blocked_reason
               << " dry_run_session_identity="
               << query::debug_string(proof.dry_run_session_identity)
               << " checkpoint_protocol_identity="
               << query::debug_string(proof.checkpoint_protocol_identity)
               << " rollback_replay_identity="
               << query::debug_string(proof.rollback_replay_identity)
               << " cursor_snapshot_identity="
               << query::debug_string(proof.cursor_snapshot_identity)
               << '\n';
    }
    for (base::usize index = 0;
         index < result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures.size();
         ++index) {
        const BuiltinDeriveDiagnosticShadowNoAstMutationClosure& closure =
            result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures[index];
        stream << "  builtin_derive_diagnostic_shadow_no_ast_mutation_closure #" << index
               << " module=" << closure.module.value
               << " source_part=" << closure.source_part_index
               << " policy=" << closure.closure_policy
               << " query=" << closure.closure_query_name
               << " dry_run_sessions=" << closure.dry_run_session_count
               << " cursor_snapshot_proofs=" << closure.cursor_snapshot_proof_count
               << " rollback_replays=" << closure.rollback_replay_count
               << " negative_matrices=" << closure.negative_matrix_count
               << " diagnostic_shadows=" << closure.diagnostic_shadow_count
               << " executed_shadows=" << closure.executed_shadow_count
               << " ast_mutations=" << closure.ast_mutation_count
               << " parser_consumable_cases=" << closure.parser_consumable_case_count
               << " dry_run_session_available="
               << (closure.dry_run_session_available ? "yes" : "no")
               << " cursor_snapshot_proof_available="
               << (closure.cursor_snapshot_proof_available ? "yes" : "no")
               << " rollback_replay_available="
               << (closure.rollback_replay_available ? "yes" : "no")
               << " negative_matrix_available="
               << (closure.negative_matrix_available ? "yes" : "no")
               << " diagnostic_shadow_available="
               << (closure.diagnostic_shadow_available ? "yes" : "no")
               << " no_ast_mutation_verified="
               << (closure.no_ast_mutation_verified ? "yes" : "no")
               << " closure_complete=" << (closure.closure_complete ? "yes" : "no")
               << " dry_run_executed=" << (closure.dry_run_executed ? "yes" : "no")
               << " replay_execution_enabled="
               << (closure.replay_execution_enabled ? "yes" : "no")
               << " session_committed=" << (closure.session_committed ? "yes" : "no")
               << " parser_cursor_advanced="
               << (closure.parser_cursor_advanced ? "yes" : "no")
               << " parser_consumption_enabled="
               << (closure.parser_consumption_enabled ? "yes" : "no")
               << " parser_admitted=" << (closure.parser_admitted ? "yes" : "no")
               << " generated_part_parsed="
               << (closure.generated_part_parsed ? "yes" : "no")
               << " generated_part_merged="
               << (closure.generated_part_merged ? "yes" : "no")
               << " ast_mutated=" << (closure.ast_mutated ? "yes" : "no")
               << " sema_visible=" << (closure.sema_visible ? "yes" : "no")
               << " emit_expanded_available="
               << (closure.emit_expanded_available ? "yes" : "no")
               << " debug_trace_available="
               << (closure.debug_trace_available ? "yes" : "no")
               << " source_map_available="
               << (closure.source_map_available ? "yes" : "no")
               << " standard_library_required="
               << (closure.standard_library_required ? "yes" : "no")
               << " runtime_required=" << (closure.runtime_required ? "yes" : "no")
               << " external_process_required="
               << (closure.external_process_required ? "yes" : "no")
               << " user_generated_code="
               << (closure.produced_user_generated_code ? "yes" : "no")
               << " closure_visible=" << (closure.closure_visible ? "yes" : "no")
               << " query_reusable=" << (closure.query_reusable ? "yes" : "no")
               << " blocker=" << closure.blocked_reason
               << " dry_run_session_identity="
               << query::debug_string(closure.dry_run_session_identity)
               << " cursor_snapshot_identity="
               << query::debug_string(closure.cursor_snapshot_identity)
               << " rollback_replay_identity="
               << query::debug_string(closure.rollback_replay_identity)
               << " negative_matrix_identity="
               << query::debug_string(closure.negative_matrix_identity)
               << " closure_identity="
               << query::debug_string(closure.closure_identity)
               << '\n';
    }
    for (base::usize index = 0; index < result.builtin_derive_parser_dry_run_admission_gates.size();
         ++index) {
        const BuiltinDeriveParserDryRunAdmissionGate& gate =
            result.builtin_derive_parser_dry_run_admission_gates[index];
        stream << "  builtin_derive_parser_dry_run_admission_gate #" << index
               << " module=" << gate.module.value
               << " source_part=" << gate.source_part_index
               << " policy=" << gate.admission_policy
               << " query=" << gate.admission_query_name
               << " dry_run_sessions=" << gate.dry_run_session_count
               << " cursor_snapshot_proofs=" << gate.cursor_snapshot_proof_count
               << " diagnostic_shadow_closures=" << gate.diagnostic_shadow_closure_count
               << " admission_prerequisites=" << gate.admission_prerequisite_count
               << " token_buffer_candidates=" << gate.token_buffer_candidate_count
               << " token_records=" << gate.token_record_count
               << " dry_run_execution_admitted_count="
               << gate.dry_run_execution_admitted_count
               << " parser_consumable_cases=" << gate.parser_consumable_case_count
               << " dry_run_session_available="
               << (gate.dry_run_session_available ? "yes" : "no")
               << " cursor_snapshot_proof_available="
               << (gate.cursor_snapshot_proof_available ? "yes" : "no")
               << " diagnostic_shadow_closure_available="
               << (gate.diagnostic_shadow_closure_available ? "yes" : "no")
               << " generated_buffer_available="
               << (gate.generated_buffer_available ? "yes" : "no")
               << " parse_config_available=" << (gate.parse_config_available ? "yes" : "no")
               << " admission_gate_complete=" << (gate.admission_gate_complete ? "yes" : "no")
               << " dry_run_execution_admitted="
               << (gate.dry_run_execution_admitted ? "yes" : "no")
               << " dry_run_executed=" << (gate.dry_run_executed ? "yes" : "no")
               << " diagnostic_shadow_executed="
               << (gate.diagnostic_shadow_executed ? "yes" : "no")
               << " rollback_execution_enabled="
               << (gate.rollback_execution_enabled ? "yes" : "no")
               << " session_committed=" << (gate.session_committed ? "yes" : "no")
               << " parser_cursor_advanced="
               << (gate.parser_cursor_advanced ? "yes" : "no")
               << " parser_consumption_enabled="
               << (gate.parser_consumption_enabled ? "yes" : "no")
               << " parser_admitted=" << (gate.parser_admitted ? "yes" : "no")
               << " generated_part_parsed="
               << (gate.generated_part_parsed ? "yes" : "no")
               << " generated_part_merged="
               << (gate.generated_part_merged ? "yes" : "no")
               << " ast_mutated=" << (gate.ast_mutated ? "yes" : "no")
               << " sema_visible=" << (gate.sema_visible ? "yes" : "no")
               << " emit_expanded_available="
               << (gate.emit_expanded_available ? "yes" : "no")
               << " debug_trace_available="
               << (gate.debug_trace_available ? "yes" : "no")
               << " source_map_available=" << (gate.source_map_available ? "yes" : "no")
               << " standard_library_required="
               << (gate.standard_library_required ? "yes" : "no")
               << " runtime_required=" << (gate.runtime_required ? "yes" : "no")
               << " external_process_required="
               << (gate.external_process_required ? "yes" : "no")
               << " user_generated_code="
               << (gate.produced_user_generated_code ? "yes" : "no")
               << " gate_visible=" << (gate.gate_visible ? "yes" : "no")
               << " query_reusable=" << (gate.query_reusable ? "yes" : "no")
               << " blocker=" << gate.blocked_reason
               << " dry_run_session_identity="
               << query::debug_string(gate.dry_run_session_identity)
               << " cursor_snapshot_identity="
               << query::debug_string(gate.cursor_snapshot_identity)
               << " diagnostic_shadow_closure_identity="
               << query::debug_string(gate.diagnostic_shadow_closure_identity)
               << " generated_buffer_identity="
               << query::debug_string(gate.generated_buffer_identity)
               << " parse_config_fingerprint="
               << query::debug_string(gate.parse_config_fingerprint)
               << " admission_gate_identity="
               << query::debug_string(gate.admission_gate_identity)
               << '\n';
    }
    for (base::usize index = 0;
         index < result.builtin_derive_error_recovery_shadow_diagnostic_gates.size();
         ++index) {
        const BuiltinDeriveErrorRecoveryShadowDiagnosticGate& gate =
            result.builtin_derive_error_recovery_shadow_diagnostic_gates[index];
        stream << "  builtin_derive_error_recovery_shadow_diagnostic_gate #" << index
               << " module=" << gate.module.value
               << " source_part=" << gate.source_part_index
               << " policy=" << gate.recovery_policy
               << " query=" << gate.recovery_query_name
               << " diagnostic_shadows=" << gate.diagnostic_shadow_count
               << " report_entries=" << gate.report_entry_count
               << " planned_recoveries=" << gate.planned_recovery_count
               << " executed_recoveries=" << gate.executed_recovery_count
               << " emitted_diagnostics=" << gate.emitted_diagnostic_count
               << " dry_run_admission_gate_available="
               << (gate.dry_run_admission_gate_available ? "yes" : "no")
               << " diagnostic_shadow_closure_available="
               << (gate.diagnostic_shadow_closure_available ? "yes" : "no")
               << " rollback_replay_available="
               << (gate.rollback_replay_available ? "yes" : "no")
               << " parser_report_available="
               << (gate.parser_report_available ? "yes" : "no")
               << " recovery_shadow_plan_available="
               << (gate.recovery_shadow_plan_available ? "yes" : "no")
               << " recovery_shadow_complete="
               << (gate.recovery_shadow_complete ? "yes" : "no")
               << " recovery_execution_enabled="
               << (gate.recovery_execution_enabled ? "yes" : "no")
               << " diagnostic_emission_enabled="
               << (gate.diagnostic_emission_enabled ? "yes" : "no")
               << " dry_run_execution_admitted="
               << (gate.dry_run_execution_admitted ? "yes" : "no")
               << " dry_run_executed=" << (gate.dry_run_executed ? "yes" : "no")
               << " rollback_execution_enabled="
               << (gate.rollback_execution_enabled ? "yes" : "no")
               << " session_committed=" << (gate.session_committed ? "yes" : "no")
               << " parser_cursor_advanced="
               << (gate.parser_cursor_advanced ? "yes" : "no")
               << " parser_consumption_enabled="
               << (gate.parser_consumption_enabled ? "yes" : "no")
               << " parser_admitted=" << (gate.parser_admitted ? "yes" : "no")
               << " generated_part_parsed="
               << (gate.generated_part_parsed ? "yes" : "no")
               << " generated_part_merged="
               << (gate.generated_part_merged ? "yes" : "no")
               << " ast_mutated=" << (gate.ast_mutated ? "yes" : "no")
               << " sema_visible=" << (gate.sema_visible ? "yes" : "no")
               << " emit_expanded_available="
               << (gate.emit_expanded_available ? "yes" : "no")
               << " debug_trace_available="
               << (gate.debug_trace_available ? "yes" : "no")
               << " source_map_available=" << (gate.source_map_available ? "yes" : "no")
               << " standard_library_required="
               << (gate.standard_library_required ? "yes" : "no")
               << " runtime_required=" << (gate.runtime_required ? "yes" : "no")
               << " external_process_required="
               << (gate.external_process_required ? "yes" : "no")
               << " user_generated_code="
               << (gate.produced_user_generated_code ? "yes" : "no")
               << " gate_visible=" << (gate.gate_visible ? "yes" : "no")
               << " query_reusable=" << (gate.query_reusable ? "yes" : "no")
               << " blocker=" << gate.blocked_reason
               << " dry_run_admission_gate_identity="
               << query::debug_string(gate.dry_run_admission_gate_identity)
               << " diagnostic_shadow_closure_identity="
               << query::debug_string(gate.diagnostic_shadow_closure_identity)
               << " rollback_replay_identity="
               << query::debug_string(gate.rollback_replay_identity)
               << " parser_report_identity="
               << query::debug_string(gate.parser_report_identity)
               << " recovery_shadow_identity="
               << query::debug_string(gate.recovery_shadow_identity)
               << '\n';
    }
    for (base::usize index = 0;
         index < result.builtin_derive_cursor_rollback_ast_mutation_verifier_closures.size();
         ++index) {
        const BuiltinDeriveCursorRollbackAstMutationVerifierClosure& closure =
            result.builtin_derive_cursor_rollback_ast_mutation_verifier_closures[index];
        stream << "  builtin_derive_cursor_rollback_ast_mutation_verifier_closure #" << index
               << " module=" << closure.module.value
               << " source_part=" << closure.source_part_index
               << " policy=" << closure.verifier_policy
               << " query=" << closure.verifier_query_name
               << " cursor_snapshots=" << closure.cursor_snapshot_count
               << " rollback_proofs=" << closure.rollback_proof_count
               << " recovery_shadows=" << closure.recovery_shadow_count
               << " ast_baseline_snapshots=" << closure.ast_baseline_snapshot_count
               << " ast_mutations=" << closure.ast_mutation_count
               << " cursor_commits=" << closure.cursor_commit_count
               << " session_commits=" << closure.session_commit_count
               << " parser_consumable_cases=" << closure.parser_consumable_case_count
               << " dry_run_admission_gate_available="
               << (closure.dry_run_admission_gate_available ? "yes" : "no")
               << " recovery_shadow_available="
               << (closure.recovery_shadow_available ? "yes" : "no")
               << " cursor_snapshot_proof_available="
               << (closure.cursor_snapshot_proof_available ? "yes" : "no")
               << " dry_run_session_available="
               << (closure.dry_run_session_available ? "yes" : "no")
               << " diagnostic_shadow_closure_available="
               << (closure.diagnostic_shadow_closure_available ? "yes" : "no")
               << " ast_baseline_available="
               << (closure.ast_baseline_available ? "yes" : "no")
               << " rollback_execution_guard_available="
               << (closure.rollback_execution_guard_available ? "yes" : "no")
               << " ast_mutation_verifier_complete="
               << (closure.ast_mutation_verifier_complete ? "yes" : "no")
               << " rollback_execution_enabled="
               << (closure.rollback_execution_enabled ? "yes" : "no")
               << " recovery_execution_enabled="
               << (closure.recovery_execution_enabled ? "yes" : "no")
               << " diagnostic_emission_enabled="
               << (closure.diagnostic_emission_enabled ? "yes" : "no")
               << " dry_run_execution_admitted="
               << (closure.dry_run_execution_admitted ? "yes" : "no")
               << " dry_run_executed=" << (closure.dry_run_executed ? "yes" : "no")
               << " session_committed=" << (closure.session_committed ? "yes" : "no")
               << " parser_cursor_advanced="
               << (closure.parser_cursor_advanced ? "yes" : "no")
               << " parser_consumption_enabled="
               << (closure.parser_consumption_enabled ? "yes" : "no")
               << " parser_admitted=" << (closure.parser_admitted ? "yes" : "no")
               << " generated_part_parsed="
               << (closure.generated_part_parsed ? "yes" : "no")
               << " generated_part_merged="
               << (closure.generated_part_merged ? "yes" : "no")
               << " ast_mutated=" << (closure.ast_mutated ? "yes" : "no")
               << " sema_visible=" << (closure.sema_visible ? "yes" : "no")
               << " emit_expanded_available="
               << (closure.emit_expanded_available ? "yes" : "no")
               << " debug_trace_available="
               << (closure.debug_trace_available ? "yes" : "no")
               << " source_map_available=" << (closure.source_map_available ? "yes" : "no")
               << " standard_library_required="
               << (closure.standard_library_required ? "yes" : "no")
               << " runtime_required=" << (closure.runtime_required ? "yes" : "no")
               << " external_process_required="
               << (closure.external_process_required ? "yes" : "no")
               << " user_generated_code="
               << (closure.produced_user_generated_code ? "yes" : "no")
               << " closure_visible=" << (closure.closure_visible ? "yes" : "no")
               << " query_reusable=" << (closure.query_reusable ? "yes" : "no")
               << " blocker=" << closure.blocked_reason
               << " dry_run_admission_gate_identity="
               << query::debug_string(closure.dry_run_admission_gate_identity)
               << " recovery_shadow_identity="
               << query::debug_string(closure.recovery_shadow_identity)
               << " cursor_snapshot_identity="
               << query::debug_string(closure.cursor_snapshot_identity)
               << " dry_run_session_identity="
               << query::debug_string(closure.dry_run_session_identity)
               << " diagnostic_shadow_closure_identity="
               << query::debug_string(closure.diagnostic_shadow_closure_identity)
               << " verifier_closure_identity="
               << query::debug_string(closure.verifier_closure_identity)
               << '\n';
    }
    for (base::usize index = 0; index < result.aurex_macro_surface_admission_gates.size(); ++index) {
        const AurexMacroSurfaceAdmissionGate& gate = result.aurex_macro_surface_admission_gates[index];
        stream << "  aurex_macro_surface_admission_gate #" << index
               << " item=" << gate.item.value
               << " module=" << gate.module.value
               << " part=" << gate.part_index
               << " name=" << gate.macro_name
               << " kind=" << macro_decl_kind_name(gate.macro_kind)
               << " policy=" << gate.admission_policy
               << " query=" << gate.query_name
               << " body_tokens=" << gate.body_token_count
               << " match_clauses=" << gate.match_clause_count
               << " body_balanced=" << (gate.body_balanced ? "yes" : "no")
               << " declarative_surface=" << (gate.declarative_surface ? "yes" : "no")
               << " user_derive_surface=" << (gate.user_derive_surface ? "yes" : "no")
               << " compile_time_execution_surface="
               << (gate.compile_time_execution_surface ? "yes" : "no")
               << " expansion_enabled=" << (gate.expansion_enabled ? "yes" : "no")
               << " compile_time_execution_enabled="
               << (gate.compile_time_execution_enabled ? "yes" : "no")
               << " parser_consumption_enabled="
               << (gate.parser_consumption_enabled ? "yes" : "no")
               << " ast_mutated=" << (gate.ast_mutated ? "yes" : "no")
               << " sema_visible_generated_items="
               << (gate.sema_visible_generated_items ? "yes" : "no")
               << " standard_library_required="
               << (gate.standard_library_required ? "yes" : "no")
               << " runtime_required=" << (gate.runtime_required ? "yes" : "no")
               << " external_process_required="
               << (gate.external_process_required ? "yes" : "no")
               << " user_generated_code="
               << (gate.produced_user_generated_code ? "yes" : "no")
               << " gate_visible=" << (gate.gate_visible ? "yes" : "no")
               << " query_reusable=" << (gate.query_reusable ? "yes" : "no")
               << " blocker=" << gate.blocker_reason
               << " body_fingerprint=" << query::debug_string(gate.body_fingerprint)
               << " admission_identity=" << query::debug_string(gate.admission_identity)
               << '\n';
    }
    for (base::usize index = 0; index < result.aurex_macro_definition_site_hygiene_gates.size();
         ++index) {
        const AurexMacroDefinitionSiteHygieneAdmissionGate& gate =
            result.aurex_macro_definition_site_hygiene_gates[index];
        stream << "  aurex_macro_definition_site_hygiene_gate #" << index
               << " item=" << gate.item.value
               << " module=" << gate.module.value
               << " part=" << gate.part_index
               << " name=" << gate.macro_name
               << " kind=" << macro_decl_kind_name(gate.macro_kind)
               << " policy=" << gate.hygiene_policy
               << " query=" << gate.query_name
               << " definition_site_scope_available="
               << (gate.definition_site_scope_available ? "yes" : "no")
               << " fresh_name_scope_reserved="
               << (gate.fresh_name_scope_reserved ? "yes" : "no")
               << " diagnostic_anchor_available="
               << (gate.diagnostic_anchor_available ? "yes" : "no")
               << " hygiene_resolution_enabled="
               << (gate.hygiene_resolution_enabled ? "yes" : "no")
               << " declared_names_visible="
               << (gate.declared_names_visible ? "yes" : "no")
               << " call_site_capture="
               << (gate.captures_call_site_locals ? "yes" : "no")
               << " user_generated_code="
               << (gate.produced_user_generated_code ? "yes" : "no")
               << " gate_visible=" << (gate.gate_visible ? "yes" : "no")
               << " query_reusable=" << (gate.query_reusable ? "yes" : "no")
               << " blocker=" << gate.blocker_reason
               << " definition_site_mark="
               << query::debug_string(gate.definition_site_mark)
               << " fresh_name_scope=" << query::debug_string(gate.fresh_name_scope)
               << " diagnostic_anchor="
               << query::debug_string(gate.diagnostic_anchor_identity)
               << " hygiene_identity=" << query::debug_string(gate.hygiene_identity)
               << '\n';
    }
    for (base::usize index = 0; index < result.aurex_macro_typed_matcher_admission_gates.size();
         ++index) {
        const AurexMacroTypedMatcherAdmissionGate& gate =
            result.aurex_macro_typed_matcher_admission_gates[index];
        stream << "  aurex_macro_typed_matcher_admission_gate #" << index
               << " item=" << gate.item.value
               << " module=" << gate.module.value
               << " part=" << gate.part_index
               << " matcher=" << gate.matcher_index
               << " name=" << gate.macro_name
               << " macro_kind=" << macro_decl_kind_name(gate.macro_kind)
               << " matcher_kind=" << typed_matcher_kind_name(gate.matcher_kind)
               << " head=" << gate.matcher_head
               << " binding=" << gate.binding_name
               << " policy=" << gate.matcher_policy
               << " query=" << gate.query_name
               << " recognized=" << (gate.matcher_shape_recognized ? "yes" : "no")
               << " expr_list=" << (gate.expr_list_matcher ? "yes" : "no")
               << " item_matcher=" << (gate.item_matcher ? "yes" : "no")
               << " token_stream=" << (gate.token_stream_matcher ? "yes" : "no")
               << " unknown=" << (gate.unknown_matcher ? "yes" : "no")
               << " definition_site_hygiene_available="
               << (gate.definition_site_hygiene_available ? "yes" : "no")
               << " fresh_name_scope_available="
               << (gate.fresh_name_scope_available ? "yes" : "no")
               << " diagnostic_anchor_available="
               << (gate.diagnostic_anchor_available ? "yes" : "no")
               << " matcher_execution_enabled="
               << (gate.matcher_execution_enabled ? "yes" : "no")
               << " expansion_enabled=" << (gate.expansion_enabled ? "yes" : "no")
               << " compile_time_execution_enabled="
               << (gate.compile_time_execution_enabled ? "yes" : "no")
               << " parser_consumption_enabled="
               << (gate.parser_consumption_enabled ? "yes" : "no")
               << " ast_mutated=" << (gate.ast_mutated ? "yes" : "no")
               << " sema_visible_generated_items="
               << (gate.sema_visible_generated_items ? "yes" : "no")
               << " user_generated_code="
               << (gate.produced_user_generated_code ? "yes" : "no")
               << " gate_visible=" << (gate.gate_visible ? "yes" : "no")
               << " query_reusable=" << (gate.query_reusable ? "yes" : "no")
               << " blocker=" << gate.blocker_reason
               << " matcher_fingerprint=" << query::debug_string(gate.matcher_fingerprint)
               << " matcher_identity=" << query::debug_string(gate.matcher_identity)
               << " hygiene_identity="
               << query::debug_string(gate.definition_site_hygiene_identity)
               << " diagnostic_anchor="
               << query::debug_string(gate.diagnostic_anchor_identity)
               << '\n';
    }
    return stream.str();
}

base::Result<EarlyItemExpansionResult> expand_early_item_macros_noop(const syntax::AstModule& ast,
    const std::span<const std::vector<query::ModulePartKey>> module_part_keys,
    const query::MacroExpansionPlan& plan)
{
    if (!query::is_valid_m21c_macro_expansion_plan(plan)) {
        return base::Result<EarlyItemExpansionResult>::fail(internal_error(FRONTEND_MACRO_M21D_INVALID_PLAN));
    }
    if (ast.item_modules.size() != ast.items.size()) {
        return base::Result<EarlyItemExpansionResult>::fail(
            internal_error(FRONTEND_MACRO_M21D_ITEM_MODULES_MISMATCH));
    }
    if (ast.item_part_indices.size() != ast.items.size()) {
        return base::Result<EarlyItemExpansionResult>::fail(
            internal_error(FRONTEND_MACRO_M21D_ITEM_PARTS_MISMATCH));
    }

    EarlyItemExpansionResult result;
    result.name = std::string(FRONTEND_MACRO_M26C_EXPANSION_NAME);
    result.plan = plan;
    const base::usize attribute_count = count_item_attributes(ast);
    std::vector<base::usize> input_item_indices;
    std::vector<base::usize> input_attribute_indices;
    input_item_indices.reserve(attribute_count);
    input_attribute_indices.reserve(attribute_count);
    result.inputs.reserve(attribute_count);
    result.generated_parts.reserve(ast.items.size());
    result.generated_part_stubs.reserve(ast.items.size());
    result.source_maps.reserve(attribute_count);
    result.hygiene_stubs.reserve(attribute_count);
    result.trace_stubs.reserve(attribute_count);
    result.generated_item_declarations.reserve(attribute_count);
    result.declared_generated_names.reserve(attribute_count);
    result.token_materialization_admissions.reserve(attribute_count);
    result.generated_token_buffers.reserve(attribute_count);
    result.generated_token_records.reserve(count_compiler_owned_generated_token_records(ast));
    result.parser_admission_gates.reserve(attribute_count);
    result.parser_admission_diagnostics.reserve(attribute_count);
    result.parser_admission_report_entries.reserve(attribute_count);
    result.parser_admission_reports.reserve(ast.items.size());
    result.parser_readiness_preflight_entries.reserve(attribute_count);
    result.parser_consumption_contract_gates.reserve(ast.items.size());
    result.macro_boundary_closure_reports.reserve(1U);
    result.builtin_derive_expansion_admissions.reserve(attribute_count);
    result.builtin_derive_semantic_plans.reserve(attribute_count);
    result.builtin_derive_parser_release_gates.reserve(ast.items.size());
    result.builtin_derive_release_hardening_matrices.reserve(ast.items.size());
    result.builtin_derive_debug_dump_contracts.reserve(ast.items.size());
    result.builtin_derive_rollback_diagnostic_gates.reserve(ast.items.size());
    result.builtin_derive_parser_consumption_admission_protocols.reserve(ast.items.size());
    result.builtin_derive_checkpoint_rollback_protocols.reserve(ast.items.size());
    result.builtin_derive_preconsumption_verification_closures.reserve(ast.items.size());
    result.builtin_derive_controlled_dry_run_adapters.reserve(ast.items.size());
    result.builtin_derive_dry_run_rollback_replays.reserve(ast.items.size());
    result.builtin_derive_dry_run_negative_matrices.reserve(ast.items.size());
    result.builtin_derive_parser_dry_run_sessions.reserve(ast.items.size());
    result.builtin_derive_token_cursor_snapshot_proofs.reserve(ast.items.size());
    result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures.reserve(ast.items.size());
    result.builtin_derive_parser_dry_run_admission_gates.reserve(ast.items.size());
    result.builtin_derive_error_recovery_shadow_diagnostic_gates.reserve(ast.items.size());
    result.builtin_derive_cursor_rollback_ast_mutation_verifier_closures.reserve(ast.items.size());
    result.aurex_macro_surface_admission_gates.reserve(ast.items.size());
    result.aurex_macro_definition_site_hygiene_gates.reserve(ast.items.size());
    result.aurex_macro_typed_matcher_admission_gates.reserve(count_aurex_macro_matcher_candidates(ast));

    for (base::usize item_index = 0; item_index < ast.items.size(); ++item_index) {
        const syntax::ItemId item_id{base::checked_u32(item_index, syntax::SYNTAX_ITEM_NODE_ID_CONTEXT)};
        const syntax::ItemNode& item = ast.items[item_index];
        if (item.kind == syntax::ItemKind::macro_decl) {
            ++result.aurex_macro_surface_source_item_count;
            auto attached_part_result = module_part_key_for_item(ast, module_part_keys, item_id);
            if (!attached_part_result) {
                return base::Result<EarlyItemExpansionResult>::fail(attached_part_result.error());
            }
            AurexMacroSurfaceAdmissionGate surface_gate =
                make_aurex_macro_surface_admission_gate(ast, item_id, attached_part_result.value());
            AurexMacroDefinitionSiteHygieneAdmissionGate hygiene_gate =
                make_aurex_macro_definition_site_hygiene_gate(surface_gate);
            for (const TypedMatcherCandidate& candidate : collect_typed_matcher_candidates(item)) {
                result.aurex_macro_typed_matcher_admission_gates.push_back(
                    make_aurex_macro_typed_matcher_gate(surface_gate, hygiene_gate, candidate));
            }
            result.aurex_macro_definition_site_hygiene_gates.push_back(std::move(hygiene_gate));
            result.aurex_macro_surface_admission_gates.push_back(std::move(surface_gate));
        }
        if (item.attributes.empty()) {
            continue;
        }
        auto attached_part_result = module_part_key_for_item(ast, module_part_keys, item_id);
        if (!attached_part_result) {
            return base::Result<EarlyItemExpansionResult>::fail(attached_part_result.error());
        }
        const query::ModulePartKey attached_part = attached_part_result.value();
        const syntax::ModuleId module = ast.item_modules[item_index];
        const base::u32 part_index = ast.item_part_indices[item_index];
        if (!generated_part_exists_for(result.generated_parts, module, part_index)) {
            GeneratedModulePartPlaceholder placeholder =
                make_generated_part_placeholder(attached_part, module, part_index);
            result.generated_part_stubs.push_back(make_parse_merge_stub(placeholder));
            result.generated_parts.push_back(std::move(placeholder));
        }
        const GeneratedModulePartPlaceholder* const generated_part =
            find_generated_part_for(result.generated_parts, module, part_index);
        if (generated_part == nullptr) {
            return base::Result<EarlyItemExpansionResult>::fail(
                internal_error(FRONTEND_MACRO_M21G_MISSING_GENERATED_PART));
        }
        const GeneratedModulePartParseMergeStub* const parse_merge_stub =
            find_parse_merge_stub_for(result.generated_part_stubs, module, part_index);
        if (parse_merge_stub == nullptr) {
            return base::Result<EarlyItemExpansionResult>::fail(
                internal_error(FRONTEND_MACRO_M21J_MISSING_PARSE_MERGE_STUB));
        }

        for (base::usize attribute_index = 0; attribute_index < item.attributes.size(); ++attribute_index) {
            const syntax::AttributeDecl& attribute = item.attributes[attribute_index];
            EarlyItemMacroInput input = make_macro_input(ast, item_id,
                base::checked_u32(attribute_index, "early item macro attribute index"), attribute, attached_part);
            ExpansionHygieneStub hygiene = make_hygiene_stub(input);
            ExpansionTraceStub trace = make_trace_stub(input);
            GeneratedItemDeclarationStub declaration =
                make_generated_item_declaration_stub(input, *generated_part, hygiene);
            DeclaredGeneratedNameStub declared_name =
                make_declared_generated_name_stub(input, *generated_part, hygiene, declaration);
            TokenMaterializationAdmissionStub admission = make_token_materialization_admission_stub(
                input, *generated_part, hygiene, trace, declaration, declared_name);
            GeneratedTokenBufferStub token_buffer = make_generated_token_buffer_stub(
                input, *generated_part, hygiene, trace, admission);
            GeneratedTokenParserAdmissionGateStub parser_admission_gate =
                make_generated_token_parser_admission_gate_stub(input, *generated_part, *parse_merge_stub,
                    token_buffer);
            ParserAdmissionDiagnosticProjectionStub parser_admission_diagnostic =
                make_parser_admission_diagnostic_projection_stub(input, *generated_part, *parse_merge_stub,
                    trace, token_buffer, parser_admission_gate);
            append_generated_token_records_for_attribute(result.generated_token_records, input, attribute,
                token_buffer);
            result.source_maps.push_back(make_source_map_placeholder(input));
            result.hygiene_stubs.push_back(std::move(hygiene));
            result.trace_stubs.push_back(std::move(trace));
            result.generated_item_declarations.push_back(std::move(declaration));
            result.declared_generated_names.push_back(std::move(declared_name));
            result.token_materialization_admissions.push_back(std::move(admission));
            result.generated_token_buffers.push_back(std::move(token_buffer));
            result.parser_admission_gates.push_back(std::move(parser_admission_gate));
            result.parser_admission_diagnostics.push_back(std::move(parser_admission_diagnostic));
            input_item_indices.push_back(item_index);
            input_attribute_indices.push_back(attribute_index);
            result.inputs.push_back(std::move(input));
        }
    }

    for (base::usize index = 0; index < result.parser_admission_diagnostics.size(); ++index) {
        result.parser_admission_report_entries.push_back(make_parser_admission_report_entry(
            result.parser_admission_diagnostics[index],
            base::checked_u32(index, "parser admission diagnostic report entry index")));
    }
    for (base::usize index = 0; index < result.inputs.size(); ++index) {
        result.parser_readiness_preflight_entries.push_back(make_parser_readiness_preflight_entry(
            result.inputs[index],
            result.generated_token_buffers[index],
            result.parser_admission_gates[index],
            result.parser_admission_diagnostics[index],
            result.parser_admission_report_entries[index],
            result.generated_token_records,
            base::checked_u32(index, "parser readiness preflight entry index")));
    }
    for (const GeneratedModulePartPlaceholder& placeholder : result.generated_parts) {
        const GeneratedModulePartParseMergeStub* const parse_merge_stub =
            find_parse_merge_stub_for(result.generated_part_stubs, placeholder.module,
                placeholder.source_part_index);
        if (parse_merge_stub == nullptr) {
            return base::Result<EarlyItemExpansionResult>::fail(
                internal_error(FRONTEND_MACRO_M21J_MISSING_PARSE_MERGE_STUB));
        }
        result.parser_admission_reports.push_back(
            make_parser_admission_report(placeholder, *parse_merge_stub,
                result.parser_admission_report_entries));
    }
    for (base::usize index = 0; index < result.generated_parts.size(); ++index) {
        const GeneratedModulePartPlaceholder& placeholder = result.generated_parts[index];
        const GeneratedModulePartParseMergeStub* const parse_merge_stub =
            find_parse_merge_stub_for(result.generated_part_stubs, placeholder.module,
                placeholder.source_part_index);
        if (parse_merge_stub == nullptr) {
            return base::Result<EarlyItemExpansionResult>::fail(
                internal_error(FRONTEND_MACRO_M21J_MISSING_PARSE_MERGE_STUB));
        }
        result.parser_consumption_contract_gates.push_back(
            make_parser_consumption_contract_gate(placeholder, *parse_merge_stub,
                result.parser_admission_reports[index],
                result.parser_readiness_preflight_entries));
    }
    result.macro_boundary_closure_reports.push_back(make_macro_boundary_closure_report(result));
    const MacroExpansionBoundaryClosureReport& closure = result.macro_boundary_closure_reports.front();
    for (base::usize index = 0; index < result.inputs.size(); ++index) {
        if (input_item_indices[index] >= ast.items.size()
            || input_attribute_indices[index] >= ast.items[input_item_indices[index]].attributes.size()) {
            return base::Result<EarlyItemExpansionResult>::fail(
                internal_error(FRONTEND_MACRO_M22_MISSING_INPUT_AST_VIEW));
        }
        const syntax::ItemNode& item = ast.items[input_item_indices[index]];
        const syntax::AttributeDecl& attribute = item.attributes[input_attribute_indices[index]];
        result.builtin_derive_expansion_admissions.push_back(
            make_builtin_derive_expansion_admission_gate(result.inputs[index],
                attribute,
                result.generated_token_buffers[index],
                result.parser_admission_gates[index],
                result.parser_readiness_preflight_entries[index],
                result.parser_admission_diagnostics[index],
                closure,
                base::checked_u32(index, "builtin derive expansion admission index")));
        result.builtin_derive_semantic_plans.push_back(
            make_builtin_derive_semantic_expansion_plan(result.inputs[index],
                item,
                result.builtin_derive_expansion_admissions[index],
                base::checked_u32(index, "builtin derive semantic expansion plan index")));
    }
    for (base::usize index = 0; index < result.generated_parts.size(); ++index) {
        result.builtin_derive_parser_release_gates.push_back(
            make_builtin_derive_parser_consumption_release_gate(result.generated_parts[index],
                result.parser_consumption_contract_gates[index],
                closure,
                result.builtin_derive_expansion_admissions,
                result.builtin_derive_semantic_plans));
    }
    for (base::usize index = 0; index < result.generated_parts.size(); ++index) {
        result.builtin_derive_release_hardening_matrices.push_back(
            make_builtin_derive_release_hardening_matrix(result.generated_parts[index],
                result.builtin_derive_parser_release_gates[index],
                result.generated_parts,
                result.builtin_derive_expansion_admissions,
                result.builtin_derive_semantic_plans,
                result.builtin_derive_parser_release_gates));
        result.builtin_derive_debug_dump_contracts.push_back(
            make_builtin_derive_debug_dump_stability_contract(result.generated_parts[index],
                result.builtin_derive_parser_release_gates[index],
                result.builtin_derive_release_hardening_matrices[index]));
        result.builtin_derive_rollback_diagnostic_gates.push_back(
            make_builtin_derive_rollback_diagnostic_design_gate(result.generated_parts[index],
                result.parser_consumption_contract_gates[index],
                result.builtin_derive_parser_release_gates[index],
                result.builtin_derive_release_hardening_matrices[index],
                result.builtin_derive_debug_dump_contracts[index],
                result.parser_admission_diagnostics,
                result.parser_admission_report_entries));
        result.builtin_derive_parser_consumption_admission_protocols.push_back(
            make_builtin_derive_parser_consumption_admission_protocol(result.generated_parts[index],
                result.parser_consumption_contract_gates[index],
                result.builtin_derive_parser_release_gates[index],
                result.builtin_derive_rollback_diagnostic_gates[index],
                result.generated_token_buffers,
                result.generated_token_records,
                result.parser_admission_diagnostics));
        result.builtin_derive_checkpoint_rollback_protocols.push_back(
            make_builtin_derive_checkpoint_rollback_protocol(result.generated_parts[index],
                result.builtin_derive_parser_consumption_admission_protocols[index],
                result.builtin_derive_rollback_diagnostic_gates[index]));
        result.builtin_derive_preconsumption_verification_closures.push_back(
            make_builtin_derive_preconsumption_verification_closure(result.generated_parts[index],
                result.builtin_derive_parser_consumption_admission_protocols[index],
                result.builtin_derive_checkpoint_rollback_protocols[index],
                result.builtin_derive_release_hardening_matrices[index],
                result.builtin_derive_debug_dump_contracts[index],
                result.builtin_derive_rollback_diagnostic_gates[index]));
        result.builtin_derive_controlled_dry_run_adapters.push_back(
            make_builtin_derive_controlled_dry_run_adapter(result.generated_parts[index],
                result.builtin_derive_preconsumption_verification_closures[index],
                result.builtin_derive_parser_consumption_admission_protocols[index],
                result.builtin_derive_checkpoint_rollback_protocols[index]));
        result.builtin_derive_dry_run_rollback_replays.push_back(
            make_builtin_derive_dry_run_rollback_replay(result.generated_parts[index],
                result.builtin_derive_controlled_dry_run_adapters[index],
                result.builtin_derive_checkpoint_rollback_protocols[index],
                result.builtin_derive_rollback_diagnostic_gates[index]));
        result.builtin_derive_dry_run_negative_matrices.push_back(
            make_builtin_derive_dry_run_negative_matrix(result.generated_parts[index],
                result.builtin_derive_controlled_dry_run_adapters[index],
                result.builtin_derive_dry_run_rollback_replays[index],
                result.builtin_derive_preconsumption_verification_closures[index]));
        result.builtin_derive_parser_dry_run_sessions.push_back(
            make_builtin_derive_parser_dry_run_session(result.generated_parts[index],
                result.builtin_derive_controlled_dry_run_adapters[index],
                result.builtin_derive_dry_run_negative_matrices[index],
                result.generated_part_stubs[index]));
        result.builtin_derive_token_cursor_snapshot_proofs.push_back(
            make_builtin_derive_token_cursor_snapshot_proof(result.generated_parts[index],
                result.builtin_derive_parser_dry_run_sessions[index],
                result.builtin_derive_checkpoint_rollback_protocols[index],
                result.builtin_derive_dry_run_rollback_replays[index]));
        result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures.push_back(
            make_builtin_derive_diagnostic_shadow_no_ast_mutation_closure(result.generated_parts[index],
                result.builtin_derive_parser_dry_run_sessions[index],
                result.builtin_derive_token_cursor_snapshot_proofs[index],
                result.builtin_derive_dry_run_rollback_replays[index],
                result.builtin_derive_dry_run_negative_matrices[index]));
        result.builtin_derive_parser_dry_run_admission_gates.push_back(
            make_builtin_derive_parser_dry_run_admission_gate(result.generated_parts[index],
                result.builtin_derive_parser_dry_run_sessions[index],
                result.builtin_derive_token_cursor_snapshot_proofs[index],
                result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures[index],
                result.generated_part_stubs[index]));
        result.builtin_derive_error_recovery_shadow_diagnostic_gates.push_back(
            make_builtin_derive_error_recovery_shadow_diagnostic_gate(result.generated_parts[index],
                result.builtin_derive_parser_dry_run_admission_gates[index],
                result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures[index],
                result.builtin_derive_dry_run_rollback_replays[index],
                result.parser_admission_reports[index]));
        result.builtin_derive_cursor_rollback_ast_mutation_verifier_closures.push_back(
            make_builtin_derive_cursor_rollback_ast_mutation_verifier_closure(result.generated_parts[index],
                result.builtin_derive_parser_dry_run_admission_gates[index],
                result.builtin_derive_error_recovery_shadow_diagnostic_gates[index],
                result.builtin_derive_token_cursor_snapshot_proofs[index],
                result.builtin_derive_parser_dry_run_sessions[index],
                result.builtin_derive_diagnostic_shadow_no_ast_mutation_closures[index]));
    }

    result.summary = summarize_early_item_expansion_counts(result);
    result.fingerprint = early_item_expansion_fingerprint(result);
    return base::Result<EarlyItemExpansionResult>::ok(std::move(result));
}

} // namespace aurex::frontend::macro
