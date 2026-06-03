#include <aurex/infrastructure/query/module_part_query.hpp>
#include <aurex/infrastructure/query/source_file_query.hpp>

#include <utility>

namespace aurex::query {

std::optional<QueryKey> module_part_query_key(const ModulePartKey key) noexcept
{
    if (!is_valid(key)) {
        return std::nullopt;
    }
    return query_key(QueryKind::module_part, stable_key_fingerprint(key));
}

bool is_valid(const ModulePartProviderInput& input) noexcept
{
    return is_valid(input.key) && is_valid(input.part);
}

bool is_valid(const ModulePartProviderOutput& output) noexcept
{
    if (!is_valid(output.record) || !is_valid(output.result) || output.record.key.kind != QueryKind::module_part
        || output.record.result != output.result) {
        return false;
    }
    for (const QueryKey dependency : output.dependencies) {
        if (!is_valid(dependency)) {
            return false;
        }
    }
    return true;
}

std::optional<ModulePartProviderOutput> provide_module_part_query(const ModulePartProviderInput& input)
{
    if (!is_valid(input)) {
        return std::nullopt;
    }

    std::optional<QueryRecord> record = module_part_query_record(input.key, input.part);
    std::vector<QueryKey> dependencies;
    if (const std::optional<QuerySourceStageKeys> source_keys = query_source_stage_keys(input.key.file);
        source_keys.has_value()) {
        if (const std::optional<QueryKey> parse_key = parse_file_query_key(source_keys->parse_file);
            parse_key.has_value()) {
            dependencies.push_back(*parse_key);
        }
    }
    return ModulePartProviderOutput{
        std::move(*record),
        input.part,
        std::move(dependencies),
    };
}

} // namespace aurex::query
