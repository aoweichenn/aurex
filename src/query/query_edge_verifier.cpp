#include <aurex/query/query_edge_verifier.hpp>

#include <string_view>

namespace aurex::query {
namespace {

constexpr base::usize QUERY_EDGE_STABLE_U8_WIDTH = 1;
constexpr base::usize QUERY_EDGE_STABLE_U32_WIDTH = 4;
constexpr base::usize QUERY_EDGE_STABLE_U64_WIDTH = 8;
constexpr base::usize QUERY_EDGE_STABLE_BOOL_WIDTH = 1;
constexpr base::usize QUERY_EDGE_STABLE_FINGERPRINT_WIDTH = 20;
constexpr base::usize QUERY_EDGE_STABLE_PACKAGE_KEY_WIDTH =
    QUERY_EDGE_STABLE_U64_WIDTH + QUERY_EDGE_STABLE_FINGERPRINT_WIDTH + QUERY_EDGE_STABLE_U64_WIDTH;
constexpr base::usize QUERY_EDGE_STABLE_FILE_KEY_WIDTH = QUERY_EDGE_STABLE_U64_WIDTH
    + QUERY_EDGE_STABLE_PACKAGE_KEY_WIDTH + QUERY_EDGE_STABLE_FINGERPRINT_WIDTH + QUERY_EDGE_STABLE_FINGERPRINT_WIDTH
    + QUERY_EDGE_STABLE_U8_WIDTH + QUERY_EDGE_STABLE_U64_WIDTH;
constexpr base::usize QUERY_EDGE_STABLE_LEX_CONFIG_KEY_WIDTH = QUERY_EDGE_STABLE_U64_WIDTH + QUERY_EDGE_STABLE_U32_WIDTH
    + QUERY_EDGE_STABLE_BOOL_WIDTH + QUERY_EDGE_STABLE_U64_WIDTH;
constexpr base::usize QUERY_EDGE_STABLE_LEX_FILE_KEY_FILE_OFFSET = QUERY_EDGE_STABLE_U64_WIDTH;
constexpr base::usize QUERY_EDGE_STABLE_LEX_FILE_KEY_CONFIG_OFFSET =
    QUERY_EDGE_STABLE_LEX_FILE_KEY_FILE_OFFSET + QUERY_EDGE_STABLE_FILE_KEY_WIDTH;
constexpr base::usize QUERY_EDGE_STABLE_PARSE_FILE_KEY_FILE_OFFSET = QUERY_EDGE_STABLE_U64_WIDTH;
constexpr base::usize QUERY_EDGE_STABLE_PARSE_FILE_KEY_PARSER_CONFIG_OFFSET =
    QUERY_EDGE_STABLE_PARSE_FILE_KEY_FILE_OFFSET + QUERY_EDGE_STABLE_FILE_KEY_WIDTH;
constexpr base::usize QUERY_EDGE_STABLE_PARSE_FILE_KEY_LEX_CONFIG_OFFSET =
    QUERY_EDGE_STABLE_PARSE_FILE_KEY_PARSER_CONFIG_OFFSET + QUERY_EDGE_STABLE_U64_WIDTH;
constexpr base::usize QUERY_EDGE_STABLE_DEF_KEY_MODULE_OFFSET = QUERY_EDGE_STABLE_U64_WIDTH;
constexpr base::usize QUERY_EDGE_STABLE_BODY_KEY_OWNER_OFFSET = QUERY_EDGE_STABLE_U64_WIDTH;
constexpr base::usize QUERY_EDGE_STABLE_GENERIC_INSTANCE_KEY_TEMPLATE_OFFSET = QUERY_EDGE_STABLE_U64_WIDTH;

[[nodiscard]] bool stable_key_slice_matches(
    const std::string_view bytes, const base::usize offset, const std::string_view expected) noexcept
{
    return offset <= bytes.size() && expected.size() <= bytes.size() - offset
        && bytes.substr(offset, expected.size()) == expected;
}

[[nodiscard]] bool stable_key_slices_match(const std::string_view lhs, const base::usize lhs_offset,
    const std::string_view rhs, const base::usize rhs_offset, const base::usize width) noexcept
{
    return lhs_offset <= lhs.size() && width <= lhs.size() - lhs_offset && rhs_offset <= rhs.size()
        && width <= rhs.size() - rhs_offset && lhs.substr(lhs_offset, width) == rhs.substr(rhs_offset, width);
}

[[nodiscard]] bool query_dependency_edge_stable_identity_is_valid(
    const QueryRecord& dependent, const QueryRecord& dependency)
{
    const std::string_view dependent_key = dependent.stable_key_bytes;
    const std::string_view dependency_key = dependency.stable_key_bytes;
    switch (dependent.key.kind) {
        case QueryKind::lex_file:
            return stable_key_slice_matches(dependent_key, QUERY_EDGE_STABLE_LEX_FILE_KEY_FILE_OFFSET, dependency_key);
        case QueryKind::parse_file:
            return stable_key_slices_match(dependent_key, QUERY_EDGE_STABLE_PARSE_FILE_KEY_FILE_OFFSET, dependency_key,
                       QUERY_EDGE_STABLE_LEX_FILE_KEY_FILE_OFFSET, QUERY_EDGE_STABLE_FILE_KEY_WIDTH)
                && stable_key_slices_match(dependent_key, QUERY_EDGE_STABLE_PARSE_FILE_KEY_LEX_CONFIG_OFFSET,
                    dependency_key, QUERY_EDGE_STABLE_LEX_FILE_KEY_CONFIG_OFFSET,
                    QUERY_EDGE_STABLE_LEX_CONFIG_KEY_WIDTH);
        case QueryKind::item_list:
        case QueryKind::module_exports:
        case QueryKind::generic_instance_body:
            return dependent_key == dependency_key;
        case QueryKind::item_signature:
        case QueryKind::generic_template_signature:
            return stable_key_slice_matches(dependent_key, QUERY_EDGE_STABLE_DEF_KEY_MODULE_OFFSET, dependency_key);
        case QueryKind::generic_instance_signature:
            return stable_key_slice_matches(
                dependent_key, QUERY_EDGE_STABLE_GENERIC_INSTANCE_KEY_TEMPLATE_OFFSET, dependency_key);
        case QueryKind::type_check_body:
            if (dependency.key.kind == QueryKind::function_body_syntax) {
                return dependent_key == dependency_key;
            }
            return stable_key_slice_matches(dependent_key, QUERY_EDGE_STABLE_BODY_KEY_OWNER_OFFSET, dependency_key);
        case QueryKind::lower_function_ir:
            return dependent_key == dependency_key;
        case QueryKind::diagnostics:
            return dependent_key == stable_serialize(dependency.key);
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
