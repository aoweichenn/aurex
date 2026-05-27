#include <aurex/query/query_result.hpp>
#include <aurex/query/stable_key_decoder.hpp>

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

bool query_record_stable_identity_is_valid(const QueryRecord& record) noexcept
{
    return is_valid(record) && record.key.payload == stable_fingerprint(record.stable_key_bytes)
        && stable_key_layout_matches_query_kind(record.key.kind, record.stable_key_bytes);
}

bool is_valid(const FileContentQueryInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.result);
}

bool is_valid(const LexFileQueryInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.result);
}

bool is_valid(const ParseFileQueryInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.result);
}

bool is_valid(const ModuleGraphQueryInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.result);
}

bool is_valid(const ModulePartQueryInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.result);
}

bool is_valid(const ModuleExportsQueryInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.result);
}

bool is_valid(const ModulePackageExportsQueryInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.result);
}

bool is_valid(const ItemListQueryInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.result);
}

bool is_valid(const ItemSignatureQueryInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.result);
}

bool is_valid(const GenericTemplateSignatureQueryInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.result);
}

bool is_valid(const GenericInstanceSignatureQueryInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.result);
}

bool is_valid(const GenericInstanceBodyQueryInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.result);
}

bool is_valid(const LowerFunctionIRQueryInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.result);
}

bool is_valid(const LowerGenericInstanceIRQueryInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.result);
}

bool is_valid(const FunctionBodySyntaxQueryInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.result);
}

bool is_valid(const TypeCheckBodyQueryInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.result);
}

bool is_valid(const DiagnosticsQueryInput& input) noexcept
{
    return is_valid(input.producer) && input.producer.kind != QueryKind::diagnostics && is_valid(input.result);
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
    if (!query_record_stable_identity_is_valid(current)) {
        return QueryRecordChangeStatus::malformed;
    }
    if (cached == nullptr) {
        return QueryRecordChangeStatus::missing;
    }
    if (!query_record_stable_identity_is_valid(*cached) || cached->key.kind != current.key.kind
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

std::optional<QueryRecord> file_content_query_record(const FileContentQueryInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }
    return query_record(
        QueryKind::file_content, stable_key_fingerprint(input.key), stable_serialize(input.key), input.result);
}

std::optional<QueryRecord> file_content_query_record(const FileKey key, const QueryResultFingerprint result)
{
    return file_content_query_record(FileContentQueryInput{
        key,
        result,
    });
}

std::optional<QueryRecord> lex_file_query_record(const LexFileQueryInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }
    return query_record(
        QueryKind::lex_file, stable_key_fingerprint(input.key), stable_serialize(input.key), input.result);
}

std::optional<QueryRecord> lex_file_query_record(const LexFileKey key, const QueryResultFingerprint result)
{
    return lex_file_query_record(LexFileQueryInput{
        key,
        result,
    });
}

std::optional<QueryRecord> parse_file_query_record(const ParseFileQueryInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }
    return query_record(
        QueryKind::parse_file, stable_key_fingerprint(input.key), stable_serialize(input.key), input.result);
}

std::optional<QueryRecord> parse_file_query_record(const ParseFileKey key, const QueryResultFingerprint result)
{
    return parse_file_query_record(ParseFileQueryInput{
        key,
        result,
    });
}

std::optional<QueryRecord> module_graph_query_record(const ModuleGraphQueryInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }
    return query_record(
        QueryKind::module_graph, stable_key_fingerprint(input.key), stable_serialize(input.key), input.result);
}

std::optional<QueryRecord> module_graph_query_record(const ModuleKey key, const QueryResultFingerprint result)
{
    return module_graph_query_record(ModuleGraphQueryInput{
        key,
        result,
    });
}

std::optional<QueryRecord> module_part_query_record(const ModulePartQueryInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }
    return query_record(
        QueryKind::module_part, stable_key_fingerprint(input.key), stable_serialize(input.key), input.result);
}

std::optional<QueryRecord> module_part_query_record(const ModulePartKey key, const QueryResultFingerprint result)
{
    return module_part_query_record(ModulePartQueryInput{
        key,
        result,
    });
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

std::optional<QueryRecord> module_package_exports_query_record(const ModulePackageExportsQueryInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }
    return query_record(QueryKind::module_package_exports, stable_key_fingerprint(input.key),
        stable_serialize(input.key), input.result);
}

std::optional<QueryRecord> module_package_exports_query_record(const ModuleKey key, const QueryResultFingerprint result)
{
    return module_package_exports_query_record(ModulePackageExportsQueryInput{
        key,
        result,
    });
}

std::optional<QueryRecord> item_list_query_record(const ItemListQueryInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }
    return query_record(
        QueryKind::item_list, stable_key_fingerprint(input.key), stable_serialize(input.key), input.result);
}

std::optional<QueryRecord> item_list_query_record(const ModuleKey key, const QueryResultFingerprint result)
{
    return item_list_query_record(ItemListQueryInput{
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

std::optional<QueryRecord> generic_template_signature_query_record(const GenericTemplateSignatureQueryInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }
    return query_record(QueryKind::generic_template_signature, stable_key_fingerprint(input.key),
        stable_serialize(input.key), input.result);
}

std::optional<QueryRecord> generic_template_signature_query_record(
    const DefKey key, const QueryResultFingerprint result)
{
    return generic_template_signature_query_record(GenericTemplateSignatureQueryInput{
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

std::optional<QueryRecord> generic_instance_body_query_record(const GenericInstanceBodyQueryInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }
    return query_record(
        QueryKind::generic_instance_body, stable_key_fingerprint(input.key), stable_serialize(input.key), input.result);
}

std::optional<QueryRecord> generic_instance_body_query_record(
    const GenericInstanceKey& key, const QueryResultFingerprint result)
{
    if (!is_valid(key) || !is_valid(result)) {
        return std::nullopt;
    }
    return query_record(QueryKind::generic_instance_body, stable_key_fingerprint(key), stable_serialize(key), result);
}

std::optional<QueryRecord> lower_function_ir_query_record(const LowerFunctionIRQueryInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }
    return query_record(
        QueryKind::lower_function_ir, stable_key_fingerprint(input.key), stable_serialize(input.key), input.result);
}

std::optional<QueryRecord> lower_function_ir_query_record(const BodyKey key, const QueryResultFingerprint result)
{
    return lower_function_ir_query_record(LowerFunctionIRQueryInput{
        key,
        result,
    });
}

std::optional<QueryRecord> lower_generic_instance_ir_query_record(const LowerGenericInstanceIRQueryInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }
    return query_record(
        QueryKind::lower_function_ir, stable_key_fingerprint(input.key), stable_serialize(input.key), input.result);
}

std::optional<QueryRecord> lower_generic_instance_ir_query_record(
    const GenericInstanceKey& key, const QueryResultFingerprint result)
{
    if (!is_valid(key) || !is_valid(result)) {
        return std::nullopt;
    }
    return query_record(QueryKind::lower_function_ir, stable_key_fingerprint(key), stable_serialize(key), result);
}

std::optional<QueryRecord> function_body_syntax_query_record(const FunctionBodySyntaxQueryInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }
    return query_record(
        QueryKind::function_body_syntax, stable_key_fingerprint(input.key), stable_serialize(input.key), input.result);
}

std::optional<QueryRecord> function_body_syntax_query_record(const BodyKey key, const QueryResultFingerprint result)
{
    return function_body_syntax_query_record(FunctionBodySyntaxQueryInput{
        key,
        result,
    });
}

std::optional<QueryRecord> type_check_body_query_record(const TypeCheckBodyQueryInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }
    return query_record(
        QueryKind::type_check_body, stable_key_fingerprint(input.key), stable_serialize(input.key), input.result);
}

std::optional<QueryRecord> type_check_body_query_record(const BodyKey key, const QueryResultFingerprint result)
{
    return type_check_body_query_record(TypeCheckBodyQueryInput{
        key,
        result,
    });
}

std::optional<QueryRecord> diagnostics_query_record(const DiagnosticsQueryInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }
    return query_record(
        QueryKind::diagnostics, stable_key_fingerprint(input.producer), stable_serialize(input.producer), input.result);
}

std::optional<QueryRecord> diagnostics_query_record(const QueryKey producer, const QueryResultFingerprint result)
{
    return diagnostics_query_record(DiagnosticsQueryInput{
        producer,
        result,
    });
}

} // namespace aurex::query
