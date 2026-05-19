#pragma once

#include <aurex/query/generic_instance_key.hpp>
#include <aurex/query/stable_identity.hpp>

#include <optional>
#include <string>

namespace aurex::query {

struct QueryResultFingerprint {
    StableFingerprint128 fingerprint;
    base::u64 global_id = 0;

    [[nodiscard]] friend constexpr bool operator==(
        QueryResultFingerprint lhs, QueryResultFingerprint rhs) noexcept = default;
};

struct QueryRecord {
    QueryKey key;
    QueryResultFingerprint result;
    std::string stable_key_bytes;
};

struct ModuleExportsQueryInput {
    ModuleKey key;
    QueryResultFingerprint result;
};

struct ItemSignatureQueryInput {
    DefKey key;
    QueryResultFingerprint result;
};

struct GenericInstanceSignatureQueryInput {
    GenericInstanceKey key;
    QueryResultFingerprint result;
};

struct GenericInstanceBodyQueryInput {
    GenericInstanceKey key;
    QueryResultFingerprint result;
};

struct FunctionBodySyntaxQueryInput {
    BodyKey key;
    QueryResultFingerprint result;
};

struct TypeCheckBodyQueryInput {
    BodyKey key;
    QueryResultFingerprint result;
};

struct DiagnosticsQueryInput {
    QueryKey producer;
    QueryResultFingerprint result;
};

enum class QueryRecordChangeStatus : base::u8 {
    missing,
    unchanged,
    changed,
    malformed,
};

[[nodiscard]] bool is_valid(QueryResultFingerprint result) noexcept;
[[nodiscard]] bool is_valid(const QueryRecord& record) noexcept;
[[nodiscard]] bool is_valid(const ModuleExportsQueryInput& input) noexcept;
[[nodiscard]] bool is_valid(const ItemSignatureQueryInput& input) noexcept;
[[nodiscard]] bool is_valid(const GenericInstanceSignatureQueryInput& input) noexcept;
[[nodiscard]] bool is_valid(const GenericInstanceBodyQueryInput& input) noexcept;
[[nodiscard]] bool is_valid(const FunctionBodySyntaxQueryInput& input) noexcept;
[[nodiscard]] bool is_valid(const TypeCheckBodyQueryInput& input) noexcept;
[[nodiscard]] bool is_valid(const DiagnosticsQueryInput& input) noexcept;

[[nodiscard]] QueryResultFingerprint query_result_fingerprint(StableFingerprint128 fingerprint) noexcept;
[[nodiscard]] QueryResultFingerprint query_result_fingerprint(IncrementalKey incremental_key) noexcept;
[[nodiscard]] QueryRecordChangeStatus query_record_change_status(
    const QueryRecord* cached, const QueryRecord& current) noexcept;

[[nodiscard]] std::optional<QueryRecord> query_record(QueryKind kind, StableFingerprint128 key_payload,
    std::string stable_key_bytes, QueryResultFingerprint result);

[[nodiscard]] std::optional<QueryRecord> module_exports_query_record(const ModuleExportsQueryInput& input);
[[nodiscard]] std::optional<QueryRecord> module_exports_query_record(ModuleKey key, QueryResultFingerprint result);

[[nodiscard]] std::optional<QueryRecord> item_signature_query_record(const ItemSignatureQueryInput& input);
[[nodiscard]] std::optional<QueryRecord> item_signature_query_record(DefKey key, QueryResultFingerprint result);

[[nodiscard]] std::optional<QueryRecord> generic_instance_signature_query_record(
    const GenericInstanceSignatureQueryInput& input);
[[nodiscard]] std::optional<QueryRecord> generic_instance_signature_query_record(
    const GenericInstanceKey& key, QueryResultFingerprint result);

[[nodiscard]] std::optional<QueryRecord> generic_instance_body_query_record(const GenericInstanceBodyQueryInput& input);
[[nodiscard]] std::optional<QueryRecord> generic_instance_body_query_record(
    const GenericInstanceKey& key, QueryResultFingerprint result);

[[nodiscard]] std::optional<QueryRecord> function_body_syntax_query_record(const FunctionBodySyntaxQueryInput& input);
[[nodiscard]] std::optional<QueryRecord> function_body_syntax_query_record(BodyKey key, QueryResultFingerprint result);

[[nodiscard]] std::optional<QueryRecord> type_check_body_query_record(const TypeCheckBodyQueryInput& input);
[[nodiscard]] std::optional<QueryRecord> type_check_body_query_record(BodyKey key, QueryResultFingerprint result);

[[nodiscard]] std::optional<QueryRecord> diagnostics_query_record(const DiagnosticsQueryInput& input);
[[nodiscard]] std::optional<QueryRecord> diagnostics_query_record(QueryKey producer, QueryResultFingerprint result);

} // namespace aurex::query
