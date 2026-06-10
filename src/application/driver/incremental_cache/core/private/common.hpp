#pragma once

#include <aurex/infrastructure/base/integer.hpp>

#include <limits>
#include <string_view>

namespace aurex::driver::incremental_cache_detail {

inline constexpr std::string_view INCREMENTAL_CACHE_WRITE_OPEN_FAILED =
    "failed to open incremental cache file for writing";
inline constexpr std::string_view INCREMENTAL_CACHE_WRITE_FAILED = "failed to write incremental cache file";
inline constexpr std::string_view INCREMENTAL_CACHE_RENAME_FAILED = "failed to publish incremental cache file";
inline constexpr std::string_view INCREMENTAL_CACHE_DIRECTORY_FAILED = "failed to create incremental cache directory";
inline constexpr std::string_view INCREMENTAL_CACHE_QUERY_GRAPH_INVALID =
    "invalid incremental cache query dependency graph";

inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_QUERY_DIFF = "incremental_cache.query_diff";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_QUERY_PLAN = "incremental_cache.query_plan";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_QUERY_PRUNING = "incremental_cache.query_pruning";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_QUERY_PROVIDER_EVAL =
    "incremental_cache.query_provider_eval";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_SOURCE_STAGE_REUSE = "incremental_cache.source_stage_reuse";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROJECT_INPUTS = "incremental_cache.project_inputs";

inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_TOTAL = "total=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_MISSING = ",missing=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_UNCHANGED = ",unchanged=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_CHANGED = ",changed=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_MALFORMED = ",malformed=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_RESULT = "result=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_REUSE = "reuse";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_REJECT = "reject";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_REASON = ",reason=";
inline constexpr std::string_view INCREMENTAL_CACHE_PRUNING_FALLBACK_DISABLED = "disabled";
inline constexpr std::string_view INCREMENTAL_CACHE_PRUNING_FALLBACK_NONE = "none";
inline constexpr std::string_view INCREMENTAL_CACHE_PRUNING_FALLBACK_NO_CACHE = "no_cache";
inline constexpr std::string_view INCREMENTAL_CACHE_PRUNING_FALLBACK_MALFORMED_CACHE = "malformed_cache";
inline constexpr std::string_view INCREMENTAL_CACHE_PRUNING_FALLBACK_MALFORMED_QUERY_GRAPH = "malformed_query_graph";
inline constexpr std::string_view INCREMENTAL_CACHE_PRUNING_FALLBACK_MALFORMED_QUERY_IDENTITY =
    "malformed_query_identity";
inline constexpr std::string_view INCREMENTAL_CACHE_PRUNING_FALLBACK_INCOMPLETE_PLAN = "incomplete_plan";
inline constexpr std::string_view INCREMENTAL_CACHE_PRUNING_FALLBACK_MISSING_REUSABLE_RECORD =
    "missing_reusable_record";

inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_REASON_NONE = "none";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_REASON_MISSING_ROOT_SOURCE = "missing_root_source";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_REASON_SOURCE_FAILURE = "source_failure";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_REASON_MISSING_QUERY = "missing_query";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_REASON_CHANGED_QUERY = "changed_query";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_REASON_MALFORMED_QUERY = "malformed_query";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_SOURCES = ",sources=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_QUERIES = ",queries=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_SOURCE_FAILURES = ",source_failures=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROJECT_CHANGED_INPUTS = ",changed_inputs=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_REUSABLE = "reusable=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_RECOMPUTE_ROOTS = ",recompute_roots=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROPAGATED_RECOMPUTE = ",propagated_recompute=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_RECOMPUTE = ",recompute=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_ENABLED = "enabled=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_APPLIED = ",applied=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED = ",reused=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED = ",recomputed=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_PROJECT_GRAPHS = ",reused_project_graphs=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_DYN_OWNERSHIP_RUNTIME_BOUNDARY_GATES =
    ",reused_dyn_ownership_runtime_boundary_gates=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_FILE_CONTENTS = ",reused_file_contents=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_LEX_FILES = ",reused_lex_files=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_PARSE_FILES = ",reused_parse_files=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_MODULE_PARTS = ",reused_module_parts=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_MODULE_GRAPHS = ",reused_module_graphs=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_MODULE_EXPORTS = ",reused_module_exports=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_ITEM_LISTS = ",reused_item_lists=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_ITEM_SIGNATURES = ",reused_item_signatures=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_FUNCTION_BODY_SYNTAXES =
    ",reused_function_body_syntaxes=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_TYPE_CHECK_BODIES =
    ",reused_type_check_bodies=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_GENERIC_TEMPLATE_SIGNATURES =
    ",reused_generic_template_signatures=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_GENERIC_INSTANCE_SIGNATURES =
    ",reused_generic_instance_signatures=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_GENERIC_INSTANCE_BODIES =
    ",reused_generic_instance_bodies=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_LOWER_FUNCTION_IRS =
    ",reused_lower_function_irs=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_REUSED_DIAGNOSTICS = ",reused_diagnostics=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_FILE_CONTENTS =
    ",recomputed_file_contents=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_PROJECT_GRAPHS =
    ",recomputed_project_graphs=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_DYN_OWNERSHIP_RUNTIME_BOUNDARY_GATES =
    ",recomputed_dyn_ownership_runtime_boundary_gates=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_LEX_FILES = ",recomputed_lex_files=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_PARSE_FILES = ",recomputed_parse_files=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_MODULE_PARTS =
    ",recomputed_module_parts=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_MODULE_GRAPHS =
    ",recomputed_module_graphs=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_MODULE_EXPORTS =
    ",recomputed_module_exports=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_ITEM_LISTS = ",recomputed_item_lists=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_ITEM_SIGNATURES =
    ",recomputed_item_signatures=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_FUNCTION_BODY_SYNTAXES =
    ",recomputed_function_body_syntaxes=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_TYPE_CHECK_BODIES =
    ",recomputed_type_check_bodies=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_GENERIC_TEMPLATE_SIGNATURES =
    ",recomputed_generic_template_signatures=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_GENERIC_INSTANCE_SIGNATURES =
    ",recomputed_generic_instance_signatures=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_GENERIC_INSTANCE_BODIES =
    ",recomputed_generic_instance_bodies=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_LOWER_FUNCTION_IRS =
    ",recomputed_lower_function_irs=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_RECOMPUTED_DIAGNOSTICS = ",recomputed_diagnostics=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PRUNING_FALLBACK = ",fallback=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_MODE = "mode=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_MODE_FULL = "full";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_MODE_PRUNED = "pruned";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED = ",seeded=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED = ",evaluated=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_PROJECT_GRAPHS =
    ",seeded_project_graphs=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_DYN_OWNERSHIP_RUNTIME_BOUNDARY_GATES =
    ",seeded_dyn_ownership_runtime_boundary_gates=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_FILE_CONTENTS =
    ",seeded_file_contents=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_LEX_FILES = ",seeded_lex_files=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_PARSE_FILES = ",seeded_parse_files=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_MODULE_PARTS = ",seeded_module_parts=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_MODULE_GRAPHS =
    ",seeded_module_graphs=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_MODULE_EXPORTS =
    ",seeded_module_exports=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_ITEM_LISTS = ",seeded_item_lists=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_ITEM_SIGNATURES =
    ",seeded_item_signatures=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_FUNCTION_BODY_SYNTAXES =
    ",seeded_function_body_syntaxes=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_TYPE_CHECK_BODIES =
    ",seeded_type_check_bodies=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_GENERIC_TEMPLATE_SIGNATURES =
    ",seeded_generic_template_signatures=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_GENERIC_INSTANCE_SIGNATURES =
    ",seeded_generic_instance_signatures=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_GENERIC_INSTANCE_BODIES =
    ",seeded_generic_instance_bodies=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_LOWER_FUNCTION_IRS =
    ",seeded_lower_function_irs=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_SEEDED_DIAGNOSTICS = ",seeded_diagnostics=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_FILE_CONTENTS =
    ",evaluated_file_contents=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_PROJECT_GRAPHS =
    ",evaluated_project_graphs=";
inline constexpr std::string_view
    INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_DYN_OWNERSHIP_RUNTIME_BOUNDARY_GATES =
        ",evaluated_dyn_ownership_runtime_boundary_gates=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_LEX_FILES = ",evaluated_lex_files=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_PARSE_FILES =
    ",evaluated_parse_files=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_MODULE_PARTS =
    ",evaluated_module_parts=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_MODULE_GRAPHS =
    ",evaluated_module_graphs=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_MODULE_EXPORTS =
    ",evaluated_module_exports=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_ITEM_LISTS =
    ",evaluated_item_lists=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_ITEM_SIGNATURES =
    ",evaluated_item_signatures=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_FUNCTION_BODY_SYNTAXES =
    ",evaluated_function_body_syntaxes=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_TYPE_CHECK_BODIES =
    ",evaluated_type_check_bodies=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_GENERIC_TEMPLATE_SIGNATURES =
    ",evaluated_generic_template_signatures=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_GENERIC_INSTANCE_SIGNATURES =
    ",evaluated_generic_instance_signatures=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_GENERIC_INSTANCE_BODIES =
    ",evaluated_generic_instance_bodies=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_LOWER_FUNCTION_IRS =
    ",evaluated_lower_function_irs=";
inline constexpr std::string_view INCREMENTAL_CACHE_PROFILE_PROVIDER_EVAL_EVALUATED_DIAGNOSTICS =
    ",evaluated_diagnostics=";

inline constexpr base::usize INCREMENTAL_CACHE_QUERY_SCHEDULE_PROJECT_GRAPH_RANK = 0;
inline constexpr base::usize INCREMENTAL_CACHE_QUERY_SCHEDULE_DYN_OWNERSHIP_RUNTIME_BOUNDARY_GATE_RANK = 1;
inline constexpr base::usize INCREMENTAL_CACHE_QUERY_SCHEDULE_FILE_CONTENT_RANK = 2;
inline constexpr base::usize INCREMENTAL_CACHE_QUERY_SCHEDULE_LEX_FILE_RANK = 3;
inline constexpr base::usize INCREMENTAL_CACHE_QUERY_SCHEDULE_PARSE_FILE_RANK = 4;
inline constexpr base::usize INCREMENTAL_CACHE_QUERY_SCHEDULE_MODULE_PART_RANK = 5;
inline constexpr base::usize INCREMENTAL_CACHE_QUERY_SCHEDULE_MODULE_GRAPH_RANK = 6;
inline constexpr base::usize INCREMENTAL_CACHE_QUERY_SCHEDULE_ITEM_LIST_RANK = 7;
inline constexpr base::usize INCREMENTAL_CACHE_QUERY_SCHEDULE_MODULE_EXPORTS_RANK = 8;
inline constexpr base::usize INCREMENTAL_CACHE_QUERY_SCHEDULE_ITEM_SIGNATURE_RANK = 9;
inline constexpr base::usize INCREMENTAL_CACHE_QUERY_SCHEDULE_GENERIC_TEMPLATE_SIGNATURE_RANK = 10;
inline constexpr base::usize INCREMENTAL_CACHE_QUERY_SCHEDULE_GENERIC_INSTANCE_SIGNATURE_RANK = 11;
inline constexpr base::usize INCREMENTAL_CACHE_QUERY_SCHEDULE_FUNCTION_BODY_SYNTAX_RANK = 12;
inline constexpr base::usize INCREMENTAL_CACHE_QUERY_SCHEDULE_TYPE_CHECK_BODY_RANK = 13;
inline constexpr base::usize INCREMENTAL_CACHE_QUERY_SCHEDULE_GENERIC_INSTANCE_BODY_RANK = 14;
inline constexpr base::usize INCREMENTAL_CACHE_QUERY_SCHEDULE_LOWER_FUNCTION_IR_RANK = 15;
inline constexpr base::usize INCREMENTAL_CACHE_QUERY_SCHEDULE_DIAGNOSTICS_RANK = 16;
inline constexpr base::usize INCREMENTAL_CACHE_QUERY_SCHEDULE_INVALID_RANK = std::numeric_limits<base::usize>::max();

} // namespace aurex::driver::incremental_cache_detail
