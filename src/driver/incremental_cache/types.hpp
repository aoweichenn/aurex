#pragma once

#include <aurex/driver/module_loader.hpp>
#include <aurex/query/query_context.hpp>
#include <aurex/query/query_result.hpp>
#include <aurex/query/query_reuse.hpp>
#include <aurex/sema/identifier.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "common.hpp"
#include "format.hpp"

namespace aurex::driver::incremental_cache_detail {

using incremental_cache_format::DefinitionRecord;
using incremental_cache_format::ParsedCache;
using incremental_cache_format::ParsedCacheReadResult;
using incremental_cache_format::ParsedCacheValidationStatus;
using incremental_cache_format::SourceFingerprintRecord;

struct QueryCollection {
    std::vector<query::QueryRecord> records;
    std::vector<query::QueryDependencyEdge> dependency_edges;
};

struct QueryKindExecutionCounts {
    base::usize total = 0;
    base::usize file_contents = 0;
    base::usize lex_files = 0;
    base::usize parse_files = 0;
    base::usize module_graphs = 0;
    base::usize module_exports = 0;
    base::usize item_lists = 0;
    base::usize item_signatures = 0;
    base::usize function_body_syntaxes = 0;
    base::usize type_check_bodies = 0;
    base::usize generic_template_signatures = 0;
    base::usize generic_instance_signatures = 0;
    base::usize generic_instance_bodies = 0;
    base::usize lower_function_irs = 0;
    base::usize diagnostics = 0;
};

struct QueryProviderEvaluationStats {
    bool pruned = false;
    QueryKindExecutionCounts seeded;
    QueryKindExecutionCounts evaluated;
};

struct QueryCollectionResult {
    QueryCollection collection;
    QueryProviderEvaluationStats stats;
};

struct QueryReuseEvaluation {
    query::QueryReusePlan plan;
    query::QueryContext cached_context;
    bool cache_loaded = false;
    std::string_view fallback = INCREMENTAL_CACHE_PRUNING_FALLBACK_NO_CACHE;
};

struct QueryPruningGateResult {
    bool enabled = false;
    bool applied = false;
    QueryKindExecutionCounts reused;
    QueryKindExecutionCounts recomputed;
    std::string_view fallback = INCREMENTAL_CACHE_PRUNING_FALLBACK_DISABLED;
};

using QueryDependenciesByDependent =
    std::unordered_map<query::QueryKey, std::vector<query::QueryKey>, query::QueryKeyHash>;

struct SourceStageQueryRecords {
    query::QueryRecord lex_file;
    query::QueryRecord parse_file;
};

struct SourceStageReuseSummary {
    bool reusable = true;
    std::string_view reason = INCREMENTAL_CACHE_PROFILE_REASON_NONE;
    base::usize sources = 0;
    base::usize queries = 0;
    base::usize unchanged = 0;
    base::usize missing = 0;
    base::usize changed = 0;
    base::usize malformed = 0;
    base::usize source_failures = 0;
};

struct FileContentQuerySubject {
    query::FileKey key;
    query::QueryResultFingerprint result;
};

struct LexFileQuerySubject {
    query::LexFileKey key;
    query::QueryResultFingerprint result;
};

struct ParseFileQuerySubject {
    query::ParseFileKey key;
    query::QueryResultFingerprint result;
};

struct ModuleExportsQuerySubject {
    query::ModuleKey key;
    query::QueryResultFingerprint result;
    std::vector<query::ModuleKey> reexport_dependencies;
};

struct ModuleGraphQuerySubject {
    query::ModuleKey key;
    query::QueryResultFingerprint result;
};

struct ModuleExportsSignatureEntry {
    std::string category;
    std::string name;
    std::string identity;
    std::string signature;
};

struct ItemListSignatureEntry {
    std::string category;
    std::string name;
    std::string identity;
};

struct ItemListQuerySubject {
    query::ModuleKey key;
    query::QueryResultFingerprint result;
};

struct ItemSignatureQuerySubject {
    sema::StableDefId stable_id;
    sema::IncrementalKey incremental_key;
    query::DefNamespace name_space = query::DefNamespace::value;
    query::DefKind kind = query::DefKind::invalid;
};

struct GenericTemplateSignatureQuerySubject {
    sema::StableDefId stable_id;
    sema::IncrementalKey incremental_key;
    query::DefNamespace name_space = query::DefNamespace::value;
};

struct GenericInstanceSignatureQuerySubject {
    const query::GenericInstanceKey* key = nullptr;
    sema::IncrementalKey incremental_key;
};

struct GenericInstanceBodyQuerySubject {
    const query::GenericInstanceKey* key = nullptr;
    query::QueryResultFingerprint result;
};

struct FunctionBodySyntaxQuerySubject {
    query::BodyKey key;
    query::QueryResultFingerprint result;
};

struct TypeCheckBodyQuerySubject {
    query::BodyKey key;
    query::QueryResultFingerprint result;
};

enum class LowerFunctionIRSubjectKind : base::u8 {
    body,
    generic_instance,
};

struct LowerFunctionIRQuerySubject {
    LowerFunctionIRSubjectKind kind = LowerFunctionIRSubjectKind::body;
    query::BodyKey body;
    const query::GenericInstanceKey* generic_instance = nullptr;
    query::QueryResultFingerprint result;
};

struct DiagnosticsQuerySubject {
    query::QueryKey producer;
    query::QueryResultFingerprint result;
};

enum class QuerySubjectKind : base::u8 {
    file_content,
    lex_file,
    parse_file,
    module_graph,
    module_exports,
    item_list,
    item_signature,
    function_body_syntax,
    type_check_body,
    generic_template_signature,
    generic_instance_signature,
    generic_instance_body,
    lower_function_ir,
    diagnostics,
};

struct QuerySubject {
    QuerySubjectKind kind = QuerySubjectKind::module_exports;
    base::usize index = 0;
    query::QueryRecord record;
};

struct QuerySubjectCollection {
    std::vector<ModuleGraphQuerySubject> module_graphs;
    std::vector<ModuleExportsQuerySubject> module_exports;
    std::vector<ItemListQuerySubject> item_lists;
    std::vector<ItemSignatureQuerySubject> item_signatures;
    std::vector<FunctionBodySyntaxQuerySubject> function_body_syntaxes;
    std::vector<TypeCheckBodyQuerySubject> type_check_bodies;
    std::vector<GenericTemplateSignatureQuerySubject> generic_template_signatures;
    std::vector<GenericInstanceSignatureQuerySubject> generic_instance_signatures;
    std::vector<GenericInstanceBodyQuerySubject> generic_instance_bodies;
    std::vector<LowerFunctionIRQuerySubject> lower_function_irs;
    std::vector<FileContentQuerySubject> file_contents;
    std::vector<LexFileQuerySubject> lex_files;
    std::vector<ParseFileQuerySubject> parse_files;
    std::vector<DiagnosticsQuerySubject> diagnostics;
    std::vector<QuerySubject> subjects;
    std::vector<query::QueryRecord> records;
};

} // namespace aurex::driver::incremental_cache_detail
