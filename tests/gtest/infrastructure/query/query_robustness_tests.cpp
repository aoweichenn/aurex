#include <aurex/infrastructure/query/diagnostics_query.hpp>
#include <aurex/infrastructure/query/function_body_syntax_query.hpp>
#include <aurex/infrastructure/query/generic_instance_body_query.hpp>
#include <aurex/infrastructure/query/generic_instance_key.hpp>
#include <aurex/infrastructure/query/generic_instance_signature_query.hpp>
#include <aurex/infrastructure/query/generic_template_signature_query.hpp>
#include <aurex/infrastructure/query/item_list_query.hpp>
#include <aurex/infrastructure/query/item_signature_query.hpp>
#include <aurex/infrastructure/query/lower_function_ir_query.hpp>
#include <aurex/infrastructure/query/module_exports_query.hpp>
#include <aurex/infrastructure/query/module_graph_query.hpp>
#include <aurex/infrastructure/query/query_context.hpp>
#include <aurex/infrastructure/query/query_executor.hpp>
#include <aurex/infrastructure/query/query_interner.hpp>
#include <aurex/infrastructure/query/query_replay.hpp>
#include <aurex/infrastructure/query/query_result.hpp>
#include <aurex/infrastructure/query/query_reuse.hpp>
#include <aurex/infrastructure/query/source_file_query.hpp>
#include <aurex/infrastructure/query/stable_identity.hpp>
#include <aurex/infrastructure/query/type_check_body_query.hpp>

#include <algorithm>
#include <array>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace aurex::test {
namespace {

constexpr base::usize QUERY_ROBUSTNESS_ZERO_CAPACITY = 0;
constexpr base::usize QUERY_ROBUSTNESS_SINGLE_NODE_CAPACITY = 1;
constexpr base::usize QUERY_ROBUSTNESS_LIMITED_CAPACITY = 4;
constexpr base::usize QUERY_ROBUSTNESS_BULK_INTERNER_RECORD_COUNT = 4096;
constexpr base::usize QUERY_ROBUSTNESS_LARGE_MODULE_COUNT = 256;
constexpr base::usize QUERY_ROBUSTNESS_CHAOS_MODULE_COUNT = 2;
constexpr base::usize QUERY_ROBUSTNESS_MODULE_A_INDEX = 0;
constexpr base::usize QUERY_ROBUSTNESS_MODULE_B_INDEX = 1;
constexpr base::usize QUERY_ROBUSTNESS_REQUESTS_PER_MODULE = 8;
constexpr base::usize QUERY_ROBUSTNESS_EDGES_PER_MODULE = 7;
constexpr base::u32 QUERY_ROBUSTNESS_SHUFFLE_SEED = 0xA17E2026U;
constexpr base::u32 QUERY_ROBUSTNESS_INVALID_REPLAY_MODE = 255;
constexpr base::u32 QUERY_ROBUSTNESS_EMPTY_STABLE_INDEX = 0;
constexpr base::u32 QUERY_ROBUSTNESS_PUBLIC_VISIBILITY_RANK = 2;
constexpr base::u64 QUERY_ROBUSTNESS_VALUE_COMPONENT_COUNT = 1;
constexpr base::u64 QUERY_ROBUSTNESS_GENERIC_PARAM_COUNT = 0;
constexpr base::u64 QUERY_ROBUSTNESS_BODY_RANGE_BASE = 16;
constexpr base::u64 QUERY_ROBUSTNESS_BODY_RANGE_STRIDE = 32;
constexpr base::u64 QUERY_ROBUSTNESS_BODY_RANGE_WIDTH = 12;
constexpr char QUERY_ROBUSTNESS_CORRUPT_STABLE_BYTE = '\x7f';
constexpr std::string_view QUERY_ROBUSTNESS_PACKAGE_PART = "robustness";
constexpr std::string_view QUERY_ROBUSTNESS_ROOT_PART = "root";
constexpr std::string_view QUERY_ROBUSTNESS_MODULE_PREFIX = "module_";
constexpr std::string_view QUERY_ROBUSTNESS_FUNCTION_PREFIX = "function_";
constexpr std::string_view QUERY_ROBUSTNESS_SOURCE_PATH_PREFIX = "/workspace/robustness/";
constexpr std::string_view QUERY_ROBUSTNESS_SOURCE_EXTENSION = ".ax";
constexpr std::string_view QUERY_ROBUSTNESS_VIRTUAL_BUFFER = "buffer://robustness/editor";
constexpr std::string_view QUERY_ROBUSTNESS_PAYLOAD_SEPARATOR = ":";
constexpr std::string_view QUERY_ROBUSTNESS_GRAPH_PAYLOAD = "graph";
constexpr std::string_view QUERY_ROBUSTNESS_ITEMS_PAYLOAD = "items";
constexpr std::string_view QUERY_ROBUSTNESS_EXPORTS_PAYLOAD = "exports";
constexpr std::string_view QUERY_ROBUSTNESS_CHANGED_EXPORTS_PAYLOAD = "exports-changed";
constexpr std::string_view QUERY_ROBUSTNESS_SIGNATURE_PAYLOAD = "signature";
constexpr std::string_view QUERY_ROBUSTNESS_SYNTAX_PAYLOAD = "syntax";
constexpr std::string_view QUERY_ROBUSTNESS_CHECKED_BODY_PAYLOAD = "checked-body";
constexpr std::string_view QUERY_ROBUSTNESS_IR_PAYLOAD = "ir";
constexpr std::string_view QUERY_ROBUSTNESS_DIAGNOSTICS_PAYLOAD = "diagnostics";

struct QueryRobustnessSubject {
    base::usize index = 0;
    query::ModuleKey module;
    query::DefKey function_def;
    query::BodyKey body;
    query::IncrementalKey signature;
    query::QueryResultFingerprint graph;
    query::QueryResultFingerprint items;
    query::QueryResultFingerprint exports;
    query::QueryResultFingerprint syntax;
    query::QueryResultFingerprint checked_body;
    query::QueryResultFingerprint ir;
    query::QueryResultFingerprint diagnostics;
};

[[nodiscard]] std::string robustness_indexed_name(const std::string_view prefix, const base::usize index)
{
    std::string name{prefix};
    name += std::to_string(index);
    return name;
}

[[nodiscard]] query::QueryResultFingerprint robustness_result(const std::string_view label, const base::usize index)
{
    std::string payload{label};
    payload += QUERY_ROBUSTNESS_PAYLOAD_SEPARATOR;
    payload += std::to_string(index);
    return query::query_result_fingerprint(query::stable_fingerprint(payload));
}

[[nodiscard]] query::PackageKey robustness_package()
{
    const std::array<std::string_view, 2> parts{
        QUERY_ROBUSTNESS_PACKAGE_PART,
        QUERY_ROBUSTNESS_ROOT_PART,
    };
    return query::package_key(parts);
}

[[nodiscard]] query::ModuleKey robustness_module(const base::usize index)
{
    const std::string module_name = robustness_indexed_name(QUERY_ROBUSTNESS_MODULE_PREFIX, index);
    const std::array<std::string_view, 2> path{
        QUERY_ROBUSTNESS_PACKAGE_PART,
        module_name,
    };
    return query::module_key(robustness_package(), path);
}

[[nodiscard]] std::string robustness_source_path(const base::usize index)
{
    std::string path{QUERY_ROBUSTNESS_SOURCE_PATH_PREFIX};
    path += QUERY_ROBUSTNESS_MODULE_PREFIX;
    path += std::to_string(index);
    path += QUERY_ROBUSTNESS_SOURCE_EXTENSION;
    return path;
}

[[nodiscard]] query::DefKey robustness_function_def(const query::ModuleKey module, const base::usize index)
{
    const std::string function_name = robustness_indexed_name(QUERY_ROBUSTNESS_FUNCTION_PREFIX, index);
    const std::array<std::string_view, 1> path{
        function_name,
    };
    return query::def_key(module, query::DefNamespace::value, query::DefKind::function, path);
}

[[nodiscard]] query::StableDefId robustness_stable_function_def(const base::usize index)
{
    const std::string module_name = robustness_indexed_name(QUERY_ROBUSTNESS_MODULE_PREFIX, index);
    const std::array<std::string_view, 2> module_path{
        QUERY_ROBUSTNESS_PACKAGE_PART,
        module_name,
    };
    const std::string function_name = robustness_indexed_name(QUERY_ROBUSTNESS_FUNCTION_PREFIX, index);
    return query::stable_definition_id(
        query::stable_module_id(module_path), query::StableSymbolKind::function, function_name);
}

[[nodiscard]] QueryRobustnessSubject robustness_subject(const base::usize index)
{
    const query::ModuleKey module = robustness_module(index);
    const query::DefKey function_def = robustness_function_def(module, index);
    return QueryRobustnessSubject{
        index,
        module,
        function_def,
        query::body_key(function_def, query::BodySlotKind::function_body),
        query::stable_incremental_key(robustness_stable_function_def(index), QUERY_ROBUSTNESS_SIGNATURE_PAYLOAD),
        robustness_result(QUERY_ROBUSTNESS_GRAPH_PAYLOAD, index),
        robustness_result(QUERY_ROBUSTNESS_ITEMS_PAYLOAD, index),
        robustness_result(QUERY_ROBUSTNESS_EXPORTS_PAYLOAD, index),
        robustness_result(QUERY_ROBUSTNESS_SYNTAX_PAYLOAD, index),
        robustness_result(QUERY_ROBUSTNESS_CHECKED_BODY_PAYLOAD, index),
        robustness_result(QUERY_ROBUSTNESS_IR_PAYLOAD, index),
        robustness_result(QUERY_ROBUSTNESS_DIAGNOSTICS_PAYLOAD, index),
    };
}

[[nodiscard]] query::FileKey robustness_file_key(
    const base::usize index, const query::SourceRole role, const std::string_view virtual_buffer)
{
    return query::file_key(robustness_package(), robustness_source_path(index), role, virtual_buffer);
}

[[nodiscard]] query::ModulePartKey robustness_module_part(const QueryRobustnessSubject& subject)
{
    return query::module_part_key(subject.module,
        robustness_file_key(subject.index, query::SourceRole::source, std::string_view{}),
        query::ModulePartKind::primary, "<primary>");
}

[[nodiscard]] query::ItemSignatureAuthority robustness_item_signature_authority(const QueryRobustnessSubject& subject)
{
    return query::ItemSignatureAuthority{
        subject.signature,
        robustness_module_part(subject),
        subject.function_def.name_space,
        subject.function_def.kind,
        QUERY_ROBUSTNESS_PUBLIC_VISIBILITY_RANK,
        QUERY_ROBUSTNESS_VALUE_COMPONENT_COUNT,
        QUERY_ROBUSTNESS_GENERIC_PARAM_COUNT,
        true,
        false,
        false,
        false,
        true,
    };
}

[[nodiscard]] query::FunctionBodySyntaxAuthority robustness_function_body_syntax_authority(
    const QueryRobustnessSubject& subject)
{
    const base::u64 range_begin =
        QUERY_ROBUSTNESS_BODY_RANGE_BASE + (static_cast<base::u64>(subject.index) * QUERY_ROBUSTNESS_BODY_RANGE_STRIDE);
    return query::FunctionBodySyntaxAuthority{
        subject.syntax,
        subject.function_def,
        robustness_module_part(subject),
        range_begin,
        range_begin + QUERY_ROBUSTNESS_BODY_RANGE_WIDTH,
        subject.body.slot,
        subject.body.ordinal,
    };
}

[[nodiscard]] query::TypeCheckBodyAuthority robustness_type_check_body_authority(const QueryRobustnessSubject& subject)
{
    const query::FunctionBodySyntaxAuthority syntax_authority = robustness_function_body_syntax_authority(subject);
    const query::ItemSignatureAuthority signature_authority = robustness_item_signature_authority(subject);
    query::TypeCheckBodyAuthority authority;
    authority.checked_body = subject.checked_body;
    authority.body_syntax_result = query::function_body_syntax_result_fingerprint(syntax_authority);
    authority.signature_result = query::item_signature_result_fingerprint(signature_authority);
    return authority;
}

void append_pipeline_requests_for_subject(
    std::vector<query::QueryRequest>& requests, const QueryRobustnessSubject& subject)
{
    requests.push_back(query::QueryRequest{query::ModuleGraphProviderInput{
        subject.module,
        subject.graph,
        {},
    }});
    requests.push_back(query::QueryRequest{query::ItemListProviderInput{
        subject.module,
        subject.items,
    }});
    requests.push_back(query::QueryRequest{query::ModuleExportsProviderInput{
        subject.module,
        subject.exports,
    }});
    requests.push_back(query::QueryRequest{query::ItemSignatureProviderInput{
        subject.function_def,
        robustness_item_signature_authority(subject),
    }});
    requests.push_back(query::QueryRequest{query::FunctionBodySyntaxProviderInput{
        subject.body,
        robustness_function_body_syntax_authority(subject),
    }});
    requests.push_back(query::QueryRequest{query::TypeCheckBodyProviderInput{
        subject.body,
        robustness_type_check_body_authority(subject),
    }});
    requests.push_back(query::QueryRequest{query::LowerFunctionIRProviderInput{
        subject.body,
        subject.ir,
    }});

    const std::optional<query::QueryKey> producer = query::lower_function_ir_query_key(subject.body);
    if (producer.has_value()) {
        requests.push_back(query::QueryRequest{query::DiagnosticsProviderInput{
            *producer,
            subject.diagnostics,
        }});
    }
}

[[nodiscard]] std::vector<query::QueryRequest> robustness_pipeline_requests(const base::usize module_count)
{
    std::vector<query::QueryRequest> requests;
    requests.reserve(module_count * QUERY_ROBUSTNESS_REQUESTS_PER_MODULE);
    for (base::usize index = 0; index < module_count; ++index) {
        append_pipeline_requests_for_subject(requests, robustness_subject(index));
    }
    return requests;
}

[[nodiscard]] bool query_key_less(const query::QueryKey lhs, const query::QueryKey rhs) noexcept
{
    return std::tie(
               lhs.kind, lhs.schema, lhs.global_id, lhs.payload.primary, lhs.payload.secondary, lhs.payload.byte_count)
        < std::tie(
            rhs.kind, rhs.schema, rhs.global_id, rhs.payload.primary, rhs.payload.secondary, rhs.payload.byte_count);
}

[[nodiscard]] bool query_records_equal(const query::QueryRecord& lhs, const query::QueryRecord& rhs)
{
    return lhs.key == rhs.key && lhs.result == rhs.result && lhs.stable_key_bytes == rhs.stable_key_bytes;
}

void expect_query_record_vectors_equal(
    const std::vector<query::QueryRecord>& lhs, const std::vector<query::QueryRecord>& rhs)
{
    ASSERT_EQ(lhs.size(), rhs.size());
    for (base::usize index = 0; index < lhs.size(); ++index) {
        EXPECT_TRUE(query_records_equal(lhs[index], rhs[index])) << "record index " << index;
    }
}

[[nodiscard]] std::optional<query::QueryReplaySnapshot> robustness_snapshot(const base::usize module_count)
{
    query::QueryContext context;
    query::QueryExecutor executor{context};
    const std::vector<query::QueryRequest> requests = robustness_pipeline_requests(module_count);
    const query::QueryBatchExecutionResult batch = executor.evaluate_all(requests);
    if (batch.summary.computed != requests.size()) {
        return std::nullopt;
    }
    return query::make_query_replay_snapshot(context);
}

[[nodiscard]] base::usize replay_node_index_for_key(
    const query::QueryReplaySnapshot& snapshot, const query::QueryKey key)
{
    const auto found =
        std::find_if(snapshot.nodes.begin(), snapshot.nodes.end(), [key](const query::QueryReplayNode& node) {
            return node.key == key;
        });
    return static_cast<base::usize>(std::distance(snapshot.nodes.begin(), found));
}

[[nodiscard]] query::QueryKey required_query_key(const std::optional<query::QueryKey> key)
{
    return key.value_or(query::QueryKey{});
}

void replace_dependency_edge(query::QueryReplaySnapshot& snapshot, const query::QueryKey old_dependent,
    const query::QueryKey old_dependency, const query::QueryKey new_dependency)
{
    for (query::QueryDependencyEdge& edge : snapshot.edges) {
        if (edge.dependent == old_dependent && edge.dependency == old_dependency) {
            edge.dependency = new_dependency;
            return;
        }
    }
}

void replace_node_dependencies(
    query::QueryReplaySnapshot& snapshot, const query::QueryKey node_key, std::vector<query::QueryKey> dependencies)
{
    const base::usize node_index = replay_node_index_for_key(snapshot, node_key);
    if (node_index < snapshot.nodes.size()) {
        std::sort(dependencies.begin(), dependencies.end(), query_key_less);
        snapshot.nodes[node_index].dependencies = std::move(dependencies);
    }
}

[[nodiscard]] query::QueryRecord robustness_item_signature_record(const base::usize index)
{
    const QueryRobustnessSubject subject = robustness_subject(index);
    const std::optional<query::QueryRecord> record = query::item_signature_query_record(
        subject.function_def, query::item_signature_result_fingerprint(robustness_item_signature_authority(subject)));
    return record.value_or(query::QueryRecord{});
}

[[nodiscard]] query::QueryRecord robustness_module_exports_record(
    const QueryRobustnessSubject& subject, const query::QueryResultFingerprint result)
{
    const std::optional<query::QueryRecord> record = query::module_exports_query_record(subject.module, result);
    return record.value_or(query::QueryRecord{});
}

[[nodiscard]] query::QueryRecord robustness_item_signature_record(const QueryRobustnessSubject& subject)
{
    const std::optional<query::QueryRecord> record = query::item_signature_query_record(
        subject.function_def, query::item_signature_result_fingerprint(robustness_item_signature_authority(subject)));
    return record.value_or(query::QueryRecord{});
}

[[nodiscard]] query::QueryRecord robustness_diagnostics_record(
    const query::QueryKey producer, const query::QueryResultFingerprint result)
{
    const std::optional<query::QueryRecord> record = query::diagnostics_query_record(producer, result);
    return record.value_or(query::QueryRecord{});
}

[[nodiscard]] query::FileKey robustness_file_key(const query::SourceRole role, const std::string_view virtual_buffer)
{
    return robustness_file_key(QUERY_ROBUSTNESS_EMPTY_STABLE_INDEX, role, virtual_buffer);
}

TEST(QueryUnit, QueryInternerHandlesCapacityBoundariesAndBulkRecords)
{
    const query::QueryRecord first_record = robustness_item_signature_record(0);
    const query::QueryRecord second_record = robustness_item_signature_record(1);
    ASSERT_TRUE(query::query_record_stable_identity_is_valid(first_record));
    ASSERT_TRUE(query::query_record_stable_identity_is_valid(second_record));

    query::QueryInterner zero_capacity{QUERY_ROBUSTNESS_ZERO_CAPACITY};
    EXPECT_FALSE(zero_capacity.intern_key(first_record.key).has_value());
    EXPECT_FALSE(zero_capacity.intern_record(first_record).has_value());
    EXPECT_EQ(zero_capacity.size(), 0U);
    EXPECT_EQ(zero_capacity.stable_identity_count(), 0U);

    query::QueryInterner full_after_unbound_key{QUERY_ROBUSTNESS_SINGLE_NODE_CAPACITY};
    const std::optional<query::QueryNodeId> unbound_id = full_after_unbound_key.intern_key(first_record.key);
    ASSERT_TRUE(unbound_id.has_value());
    EXPECT_FALSE(full_after_unbound_key.intern_key(second_record.key).has_value());
    EXPECT_EQ(full_after_unbound_key.intern_record(first_record), unbound_id);
    EXPECT_EQ(full_after_unbound_key.stable_identity_count(), QUERY_ROBUSTNESS_SINGLE_NODE_CAPACITY);

    query::QueryInterner limited{QUERY_ROBUSTNESS_LIMITED_CAPACITY};
    std::vector<query::QueryNodeId> assigned_ids;
    assigned_ids.reserve(QUERY_ROBUSTNESS_LIMITED_CAPACITY);
    for (base::usize index = 0; index < QUERY_ROBUSTNESS_LIMITED_CAPACITY; ++index) {
        const query::QueryRecord record = robustness_item_signature_record(index);
        const std::optional<query::QueryNodeId> id = limited.intern_record(record);
        ASSERT_TRUE(id.has_value());
        EXPECT_EQ(id->value, query::QUERY_NODE_ID_FIRST_VALUE + index);
        assigned_ids.push_back(*id);
    }
    EXPECT_EQ(limited.size(), QUERY_ROBUSTNESS_LIMITED_CAPACITY);
    EXPECT_EQ(limited.stable_identity_count(), QUERY_ROBUSTNESS_LIMITED_CAPACITY);
    EXPECT_EQ(limited.intern_key(first_record.key), assigned_ids.front());
    EXPECT_EQ(limited.intern_record(first_record), assigned_ids.front());
    EXPECT_FALSE(
        limited.intern_key(robustness_item_signature_record(QUERY_ROBUSTNESS_LIMITED_CAPACITY).key).has_value());

    query::QueryRecord mismatched_stable_identity = first_record;
    mismatched_stable_identity.stable_key_bytes = second_record.stable_key_bytes;
    EXPECT_FALSE(limited.bind_record(assigned_ids.front(), mismatched_stable_identity));
    EXPECT_FALSE(limited.intern_record(query::QueryRecord{}).has_value());

    query::QueryInterner bulk;
    for (base::usize index = 0; index < QUERY_ROBUSTNESS_BULK_INTERNER_RECORD_COUNT; ++index) {
        const std::optional<query::QueryNodeId> id = bulk.intern_record(robustness_item_signature_record(index));
        ASSERT_TRUE(id.has_value());
        EXPECT_EQ(id->value, query::QUERY_NODE_ID_FIRST_VALUE + index);
    }
    EXPECT_EQ(bulk.size(), QUERY_ROBUSTNESS_BULK_INTERNER_RECORD_COUNT);
    EXPECT_EQ(bulk.stable_identity_count(), QUERY_ROBUSTNESS_BULK_INTERNER_RECORD_COUNT);
    const std::optional<query::QueryNodeId> repeated_bulk_id = bulk.intern_record(robustness_item_signature_record(0));
    ASSERT_TRUE(repeated_bulk_id.has_value());
    EXPECT_EQ(repeated_bulk_id->value, query::QUERY_NODE_ID_FIRST_VALUE);
}

TEST(QueryUnit, QuerySourceStageKeysRemainStableAcrossBoundaryModes)
{
    const query::FileKey source_file = robustness_file_key(query::SourceRole::source, std::string_view{});
    const query::FileKey virtual_file =
        robustness_file_key(query::SourceRole::virtual_buffer, QUERY_ROBUSTNESS_VIRTUAL_BUFFER);
    const std::optional<query::QuerySourceStageKeys> semantic =
        query::query_source_stage_keys(source_file, query::QuerySourceStageMode::semantic);
    const std::optional<query::QuerySourceStageKeys> repeated_semantic =
        query::query_source_stage_keys(source_file, query::QuerySourceStageMode::semantic);
    const std::optional<query::QuerySourceStageKeys> lossless =
        query::query_source_stage_keys(source_file, query::QuerySourceStageMode::lossless_tooling);
    const std::optional<query::QuerySourceStageKeys> virtual_lossless =
        query::query_source_stage_keys(virtual_file, query::QuerySourceStageMode::lossless_tooling);
    ASSERT_TRUE(semantic.has_value());
    ASSERT_TRUE(repeated_semantic.has_value());
    ASSERT_TRUE(lossless.has_value());
    ASSERT_TRUE(virtual_lossless.has_value());

    EXPECT_EQ(semantic->file, source_file);
    EXPECT_EQ(semantic->file, repeated_semantic->file);
    EXPECT_EQ(semantic->lex_config, repeated_semantic->lex_config);
    EXPECT_EQ(semantic->parser_config, repeated_semantic->parser_config);
    EXPECT_EQ(semantic->lex_file, repeated_semantic->lex_file);
    EXPECT_EQ(semantic->parse_file, repeated_semantic->parse_file);
    EXPECT_NE(semantic->lex_config, lossless->lex_config);
    EXPECT_NE(semantic->parser_config, lossless->parser_config);
    EXPECT_NE(semantic->lex_file, lossless->lex_file);
    EXPECT_NE(semantic->parse_file, lossless->parse_file);
    EXPECT_NE(source_file, virtual_file);
    EXPECT_NE(lossless->file, virtual_lossless->file);
    EXPECT_NE(lossless->lex_file, virtual_lossless->lex_file);
    EXPECT_NE(lossless->parse_file, virtual_lossless->parse_file);
    EXPECT_FALSE(query::query_source_stage_keys(query::FileKey{}).has_value());
}

TEST(QueryUnit, QueryExecutorLargeScaleBatchIsDeterministicAcrossChaosOrdering)
{
    std::vector<query::QueryRequest> ordered_requests =
        robustness_pipeline_requests(QUERY_ROBUSTNESS_LARGE_MODULE_COUNT);
    ASSERT_EQ(ordered_requests.size(), QUERY_ROBUSTNESS_LARGE_MODULE_COUNT * QUERY_ROBUSTNESS_REQUESTS_PER_MODULE);

    query::QueryContext ordered_context;
    query::QueryExecutor ordered_executor{ordered_context};
    const query::QueryBatchExecutionResult ordered_batch = ordered_executor.evaluate_all(ordered_requests);
    EXPECT_EQ(ordered_batch.summary.total, ordered_requests.size());
    EXPECT_EQ(ordered_batch.summary.computed, ordered_requests.size());
    EXPECT_EQ(ordered_batch.summary.cached, 0U);
    EXPECT_EQ(ordered_batch.summary.failed, 0U);
    EXPECT_EQ(ordered_batch.summary.cycles, 0U);
    EXPECT_EQ(ordered_context.dependency_edge_count(),
        QUERY_ROBUSTNESS_LARGE_MODULE_COUNT * QUERY_ROBUSTNESS_EDGES_PER_MODULE);

    const query::QueryBatchExecutionResult ordered_cached_batch = ordered_executor.evaluate_all(ordered_requests);
    EXPECT_EQ(ordered_cached_batch.summary.cached, ordered_requests.size());
    EXPECT_EQ(ordered_cached_batch.summary.computed, 0U);

    std::vector<query::QueryRequest> shuffled_requests = ordered_requests;
    std::mt19937 rng{QUERY_ROBUSTNESS_SHUFFLE_SEED};
    std::shuffle(shuffled_requests.begin(), shuffled_requests.end(), rng);

    query::QueryContext shuffled_context;
    query::QueryExecutor shuffled_executor{shuffled_context};
    const query::QueryBatchExecutionResult shuffled_batch = shuffled_executor.evaluate_all(shuffled_requests);
    EXPECT_EQ(shuffled_batch.summary.total, shuffled_requests.size());
    EXPECT_EQ(shuffled_batch.summary.computed, shuffled_requests.size());
    EXPECT_EQ(shuffled_batch.summary.failed, 0U);
    EXPECT_EQ(shuffled_context.dependency_edge_count(), ordered_context.dependency_edge_count());
    expect_query_record_vectors_equal(ordered_context.completed_records(), shuffled_context.completed_records());

    const std::optional<query::QueryReplayIndex> ordered_index = query::QueryReplayIndex::build(ordered_context);
    const std::optional<query::QueryReplayIndex> shuffled_index = query::QueryReplayIndex::build(shuffled_context);
    ASSERT_TRUE(ordered_index.has_value());
    ASSERT_TRUE(shuffled_index.has_value());
    EXPECT_EQ(ordered_index->size(), ordered_requests.size());
    EXPECT_EQ(shuffled_index->size(), ordered_requests.size());
    EXPECT_EQ(ordered_index->snapshot().edges.size(), ordered_context.dependency_edge_count());
    EXPECT_EQ(shuffled_index->snapshot().edges.size(), shuffled_context.dependency_edge_count());

    query::QueryContext replay_seeded_context;
    for (const query::QueryReplayNode& node : ordered_index->snapshot().nodes) {
        EXPECT_TRUE(replay_seeded_context.seed_completed_record(node.record, node.dependencies));
    }
    EXPECT_EQ(replay_seeded_context.completed_records().size(), ordered_requests.size());
    EXPECT_EQ(replay_seeded_context.dependency_edge_count(), ordered_context.dependency_edge_count());

    query::QueryExecutor replay_seeded_executor{replay_seeded_context};
    const query::QueryBatchExecutionResult replay_seeded_batch = replay_seeded_executor.evaluate_all(ordered_requests);
    EXPECT_EQ(replay_seeded_batch.summary.total, ordered_requests.size());
    EXPECT_EQ(replay_seeded_batch.summary.cached, ordered_requests.size());
    EXPECT_EQ(replay_seeded_batch.summary.computed, 0U);
    EXPECT_EQ(replay_seeded_batch.summary.failed, 0U);
}

TEST(QueryUnit, QueryReplayIndexRejectsDeterministicChaosMutations)
{
    const std::optional<query::QueryReplaySnapshot> maybe_snapshot =
        robustness_snapshot(QUERY_ROBUSTNESS_CHAOS_MODULE_COUNT);
    ASSERT_TRUE(maybe_snapshot.has_value());
    const query::QueryReplaySnapshot snapshot = *maybe_snapshot;
    ASSERT_GE(snapshot.nodes.size(), QUERY_ROBUSTNESS_REQUESTS_PER_MODULE * QUERY_ROBUSTNESS_CHAOS_MODULE_COUNT);
    ASSERT_FALSE(snapshot.edges.empty());
    ASSERT_TRUE(query::QueryReplayIndex::build(snapshot).has_value());

    query::QueryReplaySnapshot invalid_mode = snapshot;
    invalid_mode.safety_mode = static_cast<query::QueryReplaySafetyMode>(QUERY_ROBUSTNESS_INVALID_REPLAY_MODE);
    EXPECT_FALSE(query::QueryReplayIndex::build(std::move(invalid_mode)).has_value());

    query::QueryReplaySnapshot duplicate_node = snapshot;
    duplicate_node.nodes.push_back(duplicate_node.nodes.front());
    EXPECT_FALSE(query::QueryReplayIndex::build(std::move(duplicate_node)).has_value());

    query::QueryReplaySnapshot duplicate_node_id = snapshot;
    duplicate_node_id.nodes.back().id = duplicate_node_id.nodes.front().id;
    EXPECT_FALSE(query::QueryReplayIndex::build(std::move(duplicate_node_id)).has_value());

    query::QueryReplaySnapshot malformed_record = snapshot;
    malformed_record.nodes.front().record.stable_key_bytes.push_back(QUERY_ROBUSTNESS_CORRUPT_STABLE_BYTE);
    EXPECT_FALSE(query::QueryReplayIndex::build(std::move(malformed_record)).has_value());

    query::QueryReplaySnapshot mismatched_record_key = snapshot;
    mismatched_record_key.nodes.back().record.key = mismatched_record_key.nodes.front().key;
    EXPECT_FALSE(query::QueryReplayIndex::build(std::move(mismatched_record_key)).has_value());

    query::QueryReplaySnapshot duplicate_edge = snapshot;
    duplicate_edge.edges.push_back(duplicate_edge.edges.front());
    EXPECT_FALSE(query::QueryReplayIndex::build(std::move(duplicate_edge)).has_value());

    query::QueryReplaySnapshot missing_edge = snapshot;
    missing_edge.edges.erase(missing_edge.edges.begin());
    EXPECT_FALSE(query::QueryReplayIndex::build(std::move(missing_edge)).has_value());

    const QueryRobustnessSubject first_subject = robustness_subject(0);
    const QueryRobustnessSubject second_subject = robustness_subject(1);
    const query::QueryKey first_graph_key = required_query_key(query::module_graph_query_key(first_subject.module));
    const query::QueryKey first_item_list_key = required_query_key(query::item_list_query_key(first_subject.module));
    const query::QueryKey first_exports_key = required_query_key(query::module_exports_query_key(first_subject.module));
    const query::QueryKey first_item_signature_key =
        required_query_key(query::item_signature_query_key(first_subject.function_def));
    const query::QueryKey second_exports_key =
        required_query_key(query::module_exports_query_key(second_subject.module));

    query::QueryReplaySnapshot missing_dependency_node = snapshot;
    const base::usize graph_node_index = replay_node_index_for_key(missing_dependency_node, first_graph_key);
    ASSERT_LT(graph_node_index, missing_dependency_node.nodes.size());
    const auto graph_node_offset = static_cast<std::vector<query::QueryReplayNode>::difference_type>(graph_node_index);
    missing_dependency_node.nodes.erase(missing_dependency_node.nodes.begin() + graph_node_offset);
    EXPECT_FALSE(query::QueryReplayIndex::build(std::move(missing_dependency_node)).has_value());

    query::QueryReplaySnapshot wrong_kind_edge = snapshot;
    replace_node_dependencies(wrong_kind_edge, first_exports_key, std::vector<query::QueryKey>{first_graph_key});
    replace_dependency_edge(wrong_kind_edge, first_exports_key, first_item_list_key, first_graph_key);
    EXPECT_FALSE(query::QueryReplayIndex::build(std::move(wrong_kind_edge)).has_value());

    query::QueryReplaySnapshot wrong_identity_edge = snapshot;
    replace_node_dependencies(
        wrong_identity_edge, first_item_signature_key, std::vector<query::QueryKey>{second_exports_key});
    replace_dependency_edge(wrong_identity_edge, first_item_signature_key, first_exports_key, second_exports_key);
    EXPECT_FALSE(query::QueryReplayIndex::build(std::move(wrong_identity_edge)).has_value());

    query::QueryReplaySnapshot duplicated_node_dependency = snapshot;
    const base::usize item_signature_node_index =
        replay_node_index_for_key(duplicated_node_dependency, first_item_signature_key);
    ASSERT_LT(item_signature_node_index, duplicated_node_dependency.nodes.size());
    duplicated_node_dependency.nodes[item_signature_node_index].dependencies.push_back(first_exports_key);
    EXPECT_FALSE(query::QueryReplayIndex::build(std::move(duplicated_node_dependency)).has_value());
}

TEST(QueryUnit, QueryReusePlanCutsCrossModulePropagationAtGreenRecords)
{
    const QueryRobustnessSubject first_subject = robustness_subject(QUERY_ROBUSTNESS_MODULE_A_INDEX);
    const QueryRobustnessSubject second_subject = robustness_subject(QUERY_ROBUSTNESS_MODULE_B_INDEX);
    const query::QueryRecord first_exports = robustness_module_exports_record(first_subject, first_subject.exports);
    const query::QueryRecord changed_first_exports = robustness_module_exports_record(
        first_subject, robustness_result(QUERY_ROBUSTNESS_CHANGED_EXPORTS_PAYLOAD, QUERY_ROBUSTNESS_MODULE_A_INDEX));
    const query::QueryRecord first_item = robustness_item_signature_record(first_subject);
    const query::QueryRecord second_exports = robustness_module_exports_record(second_subject, second_subject.exports);
    const query::QueryRecord second_item = robustness_item_signature_record(second_subject);
    const std::optional<query::QueryKey> first_item_key = query::item_signature_query_key(first_subject.function_def);
    const std::optional<query::QueryKey> second_item_key = query::item_signature_query_key(second_subject.function_def);
    ASSERT_TRUE(first_item_key.has_value());
    ASSERT_TRUE(second_item_key.has_value());
    const query::QueryRecord first_diagnostics =
        robustness_diagnostics_record(*first_item_key, first_subject.diagnostics);
    const query::QueryRecord second_diagnostics =
        robustness_diagnostics_record(*second_item_key, second_subject.diagnostics);

    query::QueryContext cached_context;
    ASSERT_TRUE(cached_context.seed_completed_record(first_exports));
    ASSERT_TRUE(cached_context.seed_completed_record(first_item, {first_exports.key}));
    ASSERT_TRUE(cached_context.seed_completed_record(first_diagnostics, {first_item.key}));
    ASSERT_TRUE(cached_context.seed_completed_record(second_exports));
    ASSERT_TRUE(cached_context.seed_completed_record(second_item, {second_exports.key}));
    ASSERT_TRUE(cached_context.seed_completed_record(second_diagnostics, {second_item.key}));

    const std::vector<query::QueryRecord> current_records{
        changed_first_exports,
        first_item,
        first_diagnostics,
        second_exports,
        second_item,
        second_diagnostics,
    };
    const query::QueryReusePlan plan = query::build_query_reuse_plan(cached_context, current_records);
    EXPECT_EQ(plan.summary.changed, 1U);
    EXPECT_EQ(plan.summary.unchanged, current_records.size() - 1U);
    EXPECT_EQ(plan.summary.malformed, 0U);
    EXPECT_EQ(plan.recompute_roots, std::vector<query::QueryKey>{changed_first_exports.key});
    EXPECT_EQ(plan.recompute, std::vector<query::QueryKey>{changed_first_exports.key});
    EXPECT_TRUE(plan.propagated_recompute.empty());

    std::vector<query::QueryKey> expected_reusable{
        first_item.key,
        first_diagnostics.key,
        second_exports.key,
        second_item.key,
        second_diagnostics.key,
    };
    std::sort(expected_reusable.begin(), expected_reusable.end(), query_key_less);
    EXPECT_EQ(plan.reusable, expected_reusable);
}

TEST(QueryUnit, QueryReusePlanPropagatesMalformedRootsWithinOnlyTheAffectedModule)
{
    const QueryRobustnessSubject first_subject = robustness_subject(QUERY_ROBUSTNESS_MODULE_A_INDEX);
    const QueryRobustnessSubject second_subject = robustness_subject(QUERY_ROBUSTNESS_MODULE_B_INDEX);
    const query::QueryRecord first_exports = robustness_module_exports_record(first_subject, first_subject.exports);
    query::QueryRecord malformed_first_exports = robustness_module_exports_record(
        first_subject, robustness_result(QUERY_ROBUSTNESS_CHANGED_EXPORTS_PAYLOAD, QUERY_ROBUSTNESS_MODULE_A_INDEX));
    malformed_first_exports.stable_key_bytes.push_back(QUERY_ROBUSTNESS_CORRUPT_STABLE_BYTE);
    const query::QueryRecord first_item = robustness_item_signature_record(first_subject);
    const query::QueryRecord second_exports = robustness_module_exports_record(second_subject, second_subject.exports);
    const query::QueryRecord second_item = robustness_item_signature_record(second_subject);
    const std::optional<query::QueryKey> first_item_key = query::item_signature_query_key(first_subject.function_def);
    ASSERT_TRUE(first_item_key.has_value());
    const query::QueryRecord first_diagnostics =
        robustness_diagnostics_record(*first_item_key, first_subject.diagnostics);

    query::QueryContext cached_context;
    ASSERT_TRUE(cached_context.seed_completed_record(first_exports));
    ASSERT_TRUE(cached_context.seed_completed_record(first_item, {first_exports.key}));
    ASSERT_TRUE(cached_context.seed_completed_record(first_diagnostics, {first_item.key}));
    ASSERT_TRUE(cached_context.seed_completed_record(second_exports));
    ASSERT_TRUE(cached_context.seed_completed_record(second_item, {second_exports.key}));

    const std::vector<query::QueryRecord> current_records{
        malformed_first_exports,
        second_item,
    };
    const query::QueryReusePlan plan = query::build_query_reuse_plan(cached_context, current_records);
    EXPECT_EQ(plan.summary.malformed, 1U);
    EXPECT_EQ(plan.summary.unchanged, 1U);
    EXPECT_EQ(plan.summary.changed, 0U);
    EXPECT_EQ(plan.recompute_roots, std::vector<query::QueryKey>{malformed_first_exports.key});
    EXPECT_EQ(plan.reusable, std::vector<query::QueryKey>{second_item.key});

    std::vector<query::QueryKey> expected_propagated{
        first_item.key,
        first_diagnostics.key,
    };
    std::sort(expected_propagated.begin(), expected_propagated.end(), query_key_less);
    EXPECT_EQ(plan.propagated_recompute, expected_propagated);

    std::vector<query::QueryKey> expected_recompute = expected_propagated;
    expected_recompute.push_back(malformed_first_exports.key);
    std::sort(expected_recompute.begin(), expected_recompute.end(), query_key_less);
    EXPECT_EQ(plan.recompute, expected_recompute);
}

} // namespace
} // namespace aurex::test
