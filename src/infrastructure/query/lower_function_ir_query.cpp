#include <aurex/infrastructure/query/generic_instance_body_query.hpp>
#include <aurex/infrastructure/query/lower_function_ir_query.hpp>
#include <aurex/infrastructure/query/type_check_body_query.hpp>

#include <string_view>
#include <utility>

namespace aurex::query {
namespace {

constexpr std::string_view QUERY_LOWER_FUNCTION_IR_RESULT_MARKER = "query.lower_function_ir.result.v3";

[[nodiscard]] bool is_valid_lower_function_ir_output(
    const QueryRecord& record, const QueryResultFingerprint result) noexcept
{
    return is_valid(record) && is_valid(result) && record.key.kind == QueryKind::lower_function_ir
        && record.result == result;
}

[[nodiscard]] bool dependencies_are_valid(const std::vector<QueryKey>& dependencies) noexcept
{
    for (const QueryKey dependency : dependencies) {
        if (!is_valid(dependency)) {
            return false;
        }
    }
    return true;
}

} // namespace

std::optional<QueryKey> lower_function_ir_query_key(const BodyKey key) noexcept
{
    if (!is_valid(key)) {
        return std::nullopt;
    }
    return query_key(QueryKind::lower_function_ir, stable_key_fingerprint(key));
}

std::optional<QueryKey> lower_generic_instance_ir_query_key(const GenericInstanceKey& key) noexcept
{
    if (!is_valid(key)) {
        return std::nullopt;
    }
    return query_key(QueryKind::lower_function_ir, stable_key_fingerprint(key));
}

bool is_valid(const LowerFunctionIRProviderInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.ir);
}

bool is_valid(const LowerFunctionIRProviderOutput& output) noexcept
{
    return is_valid_lower_function_ir_output(output.record, output.result)
        && dependencies_are_valid(output.dependencies);
}

bool is_valid(const LowerGenericInstanceIRProviderInput& input) noexcept
{
    return input.key != nullptr && is_valid(*input.key) && is_valid(input.ir);
}

bool is_valid(const LowerGenericInstanceIRProviderOutput& output) noexcept
{
    return is_valid_lower_function_ir_output(output.record, output.result)
        && dependencies_are_valid(output.dependencies);
}

QueryResultFingerprint lower_function_ir_result_fingerprint(
    const QueryResultFingerprint ir, const FunctionCleanupMarkerFacts& cleanup_markers,
    const FunctionDynAbiFacts& dyn_abi) noexcept
{
    if (!is_valid(ir)) {
        return {};
    }
    StableHashBuilder builder;
    builder.mix_string(QUERY_LOWER_FUNCTION_IR_RESULT_MARKER);
    builder.mix_u64(ir.global_id);
    builder.mix_fingerprint(ir.fingerprint);
    builder.mix_fingerprint(function_cleanup_marker_facts_fingerprint(cleanup_markers));
    builder.mix_u64(cleanup_markers.markers.size());
    builder.mix_u64(cleanup_markers.summary.drop_count);
    builder.mix_u64(cleanup_markers.summary.drop_if_count);
    builder.mix_u64(cleanup_markers.summary.structural_static_count);
    builder.mix_u64(cleanup_markers.summary.generic_marker_only_count);
    builder.mix_u64(cleanup_markers.summary.associated_projection_marker_only_count);
    builder.mix_u64(cleanup_markers.summary.opaque_marker_only_count);
    builder.mix_u64(cleanup_markers.summary.unknown_marker_only_count);
    builder.mix_u64(cleanup_markers.summary.static_custom_destructor_count);
    builder.mix_fingerprint(function_dyn_abi_facts_fingerprint(dyn_abi));
    builder.mix_u64(dyn_abi.objects.size());
    builder.mix_u64(dyn_abi.vtables.size());
    builder.mix_u64(dyn_abi.coercions.size());
    builder.mix_u64(dyn_abi.dispatches.size());
    builder.mix_u64(dyn_abi.summary.slot_count);
    return query_result_fingerprint(builder.finish());
}

std::optional<LowerFunctionIRProviderOutput> provide_lower_function_ir_query(const LowerFunctionIRProviderInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }

    const QueryResultFingerprint result =
        lower_function_ir_result_fingerprint(input.ir, input.cleanup_markers, input.dyn_abi);
    std::optional<QueryRecord> record = lower_function_ir_query_record(input.key, result);
    std::vector<QueryKey> dependencies;
    if (const std::optional<QueryKey> type_check_key = type_check_body_query_key(input.key)) {
        dependencies.push_back(*type_check_key);
    }
    return LowerFunctionIRProviderOutput{
        std::move(*record),
        result,
        std::move(dependencies),
        input.cleanup_markers,
        input.dyn_abi,
    };
}

std::optional<LowerGenericInstanceIRProviderOutput> provide_lower_generic_instance_ir_query(
    const LowerGenericInstanceIRProviderInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }

    const QueryResultFingerprint result =
        lower_function_ir_result_fingerprint(input.ir, input.cleanup_markers, input.dyn_abi);
    std::optional<QueryRecord> record = lower_generic_instance_ir_query_record(*input.key, result);
    std::vector<QueryKey> dependencies;
    if (const std::optional<QueryKey> generic_body_key = generic_instance_body_query_key(*input.key)) {
        dependencies.push_back(*generic_body_key);
    }
    return LowerGenericInstanceIRProviderOutput{
        std::move(*record),
        result,
        std::move(dependencies),
        input.cleanup_markers,
        input.dyn_abi,
    };
}

} // namespace aurex::query
