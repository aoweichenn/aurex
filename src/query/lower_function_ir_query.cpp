#include <aurex/query/generic_instance_body_query.hpp>
#include <aurex/query/lower_function_ir_query.hpp>
#include <aurex/query/type_check_body_query.hpp>

#include <utility>

namespace aurex::query {
namespace {

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

std::optional<LowerFunctionIRProviderOutput> provide_lower_function_ir_query(
    const LowerFunctionIRProviderInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }

    std::optional<QueryRecord> record = lower_function_ir_query_record(input.key, input.ir);
    std::vector<QueryKey> dependencies;
    if (const std::optional<QueryKey> type_check_key = type_check_body_query_key(input.key)) {
        dependencies.push_back(*type_check_key);
    }
    return LowerFunctionIRProviderOutput{
        std::move(*record),
        input.ir,
        std::move(dependencies),
    };
}

std::optional<LowerGenericInstanceIRProviderOutput> provide_lower_generic_instance_ir_query(
    const LowerGenericInstanceIRProviderInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }

    std::optional<QueryRecord> record = lower_generic_instance_ir_query_record(*input.key, input.ir);
    std::vector<QueryKey> dependencies;
    if (const std::optional<QueryKey> generic_body_key = generic_instance_body_query_key(*input.key)) {
        dependencies.push_back(*generic_body_key);
    }
    return LowerGenericInstanceIRProviderOutput{
        std::move(*record),
        input.ir,
        std::move(dependencies),
    };
}

} // namespace aurex::query
