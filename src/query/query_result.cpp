#include <aurex/query/query_result.hpp>

#include <utility>

namespace aurex::query {
namespace {

constexpr base::u64 QUERY_RESULT_FINGERPRINT_MARKER = 0x51524553554c5431ULL;

[[nodiscard]] base::u64 query_result_global_id(const StableFingerprint128 fingerprint) noexcept
{
    base::u64 global_id = stable_mix(QUERY_RESULT_FINGERPRINT_MARKER, fingerprint.primary);
    global_id = stable_mix(global_id, fingerprint.secondary);
    global_id = stable_mix(global_id, fingerprint.byte_count);
    return global_id == 0 ? QUERY_RESULT_FINGERPRINT_MARKER : global_id;
}

} // namespace

bool is_valid(const QueryResultFingerprint result) noexcept
{
    return result.global_id != 0;
}

bool is_valid(const QueryRecord& record) noexcept
{
    return is_valid(record.key) && is_valid(record.result) && !record.stable_key_bytes.empty();
}

bool is_valid(const ModuleExportsQueryInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.result);
}

bool is_valid(const ItemSignatureQueryInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.result);
}

bool is_valid(const GenericInstanceSignatureQueryInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.result);
}

QueryResultFingerprint query_result_fingerprint(const StableFingerprint128 fingerprint) noexcept
{
    if (fingerprint.byte_count == 0) {
        return {};
    }
    return QueryResultFingerprint{
        fingerprint,
        query_result_global_id(fingerprint),
    };
}

QueryResultFingerprint query_result_fingerprint(const IncrementalKey incremental_key) noexcept
{
    if (!is_valid(incremental_key)) {
        return {};
    }
    return QueryResultFingerprint{
        incremental_key.fingerprint,
        incremental_key.global_id,
    };
}

QueryRecordChangeStatus query_record_change_status(const QueryRecord* const cached, const QueryRecord& current) noexcept
{
    if (!is_valid(current)) {
        return QueryRecordChangeStatus::malformed;
    }
    if (cached == nullptr) {
        return QueryRecordChangeStatus::missing;
    }
    if (!is_valid(*cached) || cached->key.kind != current.key.kind
        || cached->stable_key_bytes != current.stable_key_bytes || cached->key != current.key) {
        return QueryRecordChangeStatus::malformed;
    }
    return cached->result == current.result ? QueryRecordChangeStatus::unchanged : QueryRecordChangeStatus::changed;
}

std::optional<QueryRecord> query_record(const QueryKind kind, const StableFingerprint128 key_payload,
    std::string stable_key_bytes, const QueryResultFingerprint result)
{
    QueryRecord record{
        query_key(kind, key_payload),
        result,
        std::move(stable_key_bytes),
    };
    if (!is_valid(record)) {
        return std::nullopt;
    }
    return record;
}

std::optional<QueryRecord> module_exports_query_record(const ModuleExportsQueryInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }
    return query_record(
        QueryKind::module_exports, stable_key_fingerprint(input.key), stable_serialize(input.key), input.result);
}

std::optional<QueryRecord> module_exports_query_record(const ModuleKey key, const QueryResultFingerprint result)
{
    return module_exports_query_record(ModuleExportsQueryInput{
        key,
        result,
    });
}

std::optional<QueryRecord> item_signature_query_record(const ItemSignatureQueryInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }
    return query_record(
        QueryKind::item_signature, stable_key_fingerprint(input.key), stable_serialize(input.key), input.result);
}

std::optional<QueryRecord> item_signature_query_record(const DefKey key, const QueryResultFingerprint result)
{
    return item_signature_query_record(ItemSignatureQueryInput{
        key,
        result,
    });
}

std::optional<QueryRecord> generic_instance_signature_query_record(const GenericInstanceSignatureQueryInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }
    return query_record(QueryKind::generic_instance_signature, stable_key_fingerprint(input.key),
        stable_serialize(input.key), input.result);
}

std::optional<QueryRecord> generic_instance_signature_query_record(
    const GenericInstanceKey& key, const QueryResultFingerprint result)
{
    if (!is_valid(key) || !is_valid(result)) {
        return std::nullopt;
    }
    return query_record(
        QueryKind::generic_instance_signature, stable_key_fingerprint(key), stable_serialize(key), result);
}

} // namespace aurex::query
