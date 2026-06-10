#include <aurex/infrastructure/query/query_edge_verifier.hpp>
#include <aurex/infrastructure/query/stable_key_decoder.hpp>

#include <optional>
#include <string_view>

namespace aurex::query {
namespace {

[[nodiscard]] bool stable_module_keys_match(
    const std::string_view dependent_key, const std::string_view dependency_key) noexcept
{
    return dependent_key == dependency_key && stable_key_has_module_key_layout(dependent_key);
}

[[nodiscard]] bool stable_body_keys_match(
    const std::string_view dependent_key, const std::string_view dependency_key) noexcept
{
    return dependent_key == dependency_key && stable_key_has_body_key_layout(dependent_key);
}

[[nodiscard]] bool stable_generic_instance_keys_match(
    const std::string_view dependent_key, const std::string_view dependency_key) noexcept
{
    return dependent_key == dependency_key && stable_key_has_generic_instance_key_layout(dependent_key);
}

[[nodiscard]] bool stable_project_keys_match(
    const std::string_view dependent_key, const std::string_view dependency_key) noexcept
{
    return dependent_key == dependency_key && stable_key_has_project_key_layout(dependent_key);
}

[[nodiscard]] bool stable_module_part_depends_on_parse_file(
    const std::string_view dependent_key, const std::string_view dependency_key) noexcept
{
    const std::optional<DecodedModulePartKeyIdentity> dependent_identity =
        decode_module_part_key_identity(dependent_key);
    const std::optional<DecodedParseFileKeyIdentity> dependency_identity =
        decode_parse_file_key_identity(dependency_key);
    return dependent_identity.has_value() && dependency_identity.has_value()
        && dependent_identity->file == dependency_identity->file;
}

[[nodiscard]] bool stable_module_graph_depends_on_module_part(
    const std::string_view dependent_key, const std::string_view dependency_key) noexcept
{
    const std::optional<DecodedModulePartKeyIdentity> dependency_identity =
        decode_module_part_key_identity(dependency_key);
    return dependency_identity.has_value() && dependent_key == dependency_identity->module
        && stable_key_has_module_key_layout(dependent_key);
}

[[nodiscard]] bool lower_function_ir_dependency_identity_is_valid(
    const QueryRecord& dependent, const QueryRecord& dependency) noexcept
{
    if (dependency.key.kind == QueryKind::type_check_body) {
        return stable_body_keys_match(dependent.stable_key_bytes, dependency.stable_key_bytes);
    }
    return stable_generic_instance_keys_match(dependent.stable_key_bytes, dependency.stable_key_bytes);
}

[[nodiscard]] bool query_dependency_edge_stable_identity_is_valid(
    const QueryRecord& dependent, const QueryRecord& dependency)
{
    const std::string_view dependent_key = dependent.stable_key_bytes;
    const std::string_view dependency_key = dependency.stable_key_bytes;
    switch (dependent.key.kind) {
        case QueryKind::lex_file: {
            const std::optional<DecodedLexFileKeyIdentity> identity = decode_lex_file_key_identity(dependent_key);
            return identity.has_value() && identity->file == dependency_key;
        }
        case QueryKind::parse_file: {
            const std::optional<DecodedParseFileKeyIdentity> dependent_identity =
                decode_parse_file_key_identity(dependent_key);
            const std::optional<DecodedLexFileKeyIdentity> dependency_identity =
                decode_lex_file_key_identity(dependency_key);
            return dependent_identity.has_value() && dependency_identity.has_value()
                && dependent_identity->file == dependency_identity->file
                && dependent_identity->lex_config == dependency_identity->lex_config;
        }
        case QueryKind::module_part:
            return stable_module_part_depends_on_parse_file(dependent_key, dependency_key);
        case QueryKind::module_graph:
            if (dependency.key.kind == QueryKind::project_graph) {
                return stable_key_has_module_key_layout(dependent_key)
                    && stable_key_has_project_key_layout(dependency_key);
            }
            return stable_module_graph_depends_on_module_part(dependent_key, dependency_key);
        case QueryKind::dyn_ownership_runtime_boundary_gate:
            return stable_project_keys_match(dependent_key, dependency_key);
        case QueryKind::item_list:
            return stable_module_keys_match(dependent_key, dependency_key);
        case QueryKind::module_exports:
            if (dependency.key.kind == QueryKind::item_list) {
                return stable_module_keys_match(dependent_key, dependency_key);
            }
            return stable_key_has_module_key_layout(dependent_key)
                && stable_key_has_module_key_layout(dependency_key);
        case QueryKind::module_package_exports:
            if (dependency.key.kind == QueryKind::item_list) {
                return stable_module_keys_match(dependent_key, dependency_key);
            }
            return stable_key_has_module_key_layout(dependent_key)
                && stable_key_has_module_key_layout(dependency_key);
        case QueryKind::generic_instance_body:
            return stable_generic_instance_keys_match(dependent_key, dependency_key);
        case QueryKind::item_signature:
        case QueryKind::generic_template_signature: {
            const std::optional<DecodedDefKeyIdentity> identity = decode_def_key_identity(dependent_key);
            return identity.has_value() && identity->module == dependency_key;
        }
        case QueryKind::generic_instance_signature: {
            const std::optional<DecodedGenericInstanceKeyIdentity> identity =
                decode_generic_instance_key_identity(dependent_key);
            return identity.has_value() && identity->template_def == dependency_key;
        }
        case QueryKind::type_check_body:
            if (dependency.key.kind == QueryKind::function_body_syntax) {
                return stable_body_keys_match(dependent_key, dependency_key);
            }
            {
                const std::optional<DecodedBodyKeyIdentity> identity = decode_body_key_identity(dependent_key);
                return identity.has_value() && identity->owner == dependency_key;
            }
        case QueryKind::lower_function_ir:
            return lower_function_ir_dependency_identity_is_valid(dependent, dependency);
        case QueryKind::diagnostics:
            return dependent_key == stable_serialize(dependency.key) && stable_key_has_query_key_layout(dependent_key);
        default:
            return false;
    }
}

} // namespace

QueryDependencyEdgeValidationStatus validate_query_dependency_edge_records(
    const QueryRecord& dependent, const QueryRecord& dependency)
{
    if (!is_valid(dependent) || !is_valid(dependency)) {
        return QueryDependencyEdgeValidationStatus::invalid_record;
    }

    const QueryDependencyEdge edge{
        dependent.key,
        dependency.key,
    };
    if (!query_dependency_edge_kind_is_expected(edge)) {
        return QueryDependencyEdgeValidationStatus::invalid_kind;
    }
    if (!query_dependency_edge_stable_identity_is_valid(dependent, dependency)) {
        return QueryDependencyEdgeValidationStatus::invalid_identity;
    }
    return QueryDependencyEdgeValidationStatus::valid;
}

bool query_dependency_edge_records_are_valid(const QueryRecord& dependent, const QueryRecord& dependency)
{
    return validate_query_dependency_edge_records(dependent, dependency) == QueryDependencyEdgeValidationStatus::valid;
}

} // namespace aurex::query
