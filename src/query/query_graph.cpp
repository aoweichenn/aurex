#include <aurex/query/query_graph.hpp>

namespace aurex::query {

bool query_dependency_edge_kind_is_expected(const QueryDependencyEdge edge) noexcept
{
    if (edge.dependent.global_id == 0 || edge.dependency.global_id == 0 || edge.dependent == edge.dependency) {
        return false;
    }

    switch (edge.dependent.kind) {
        case QueryKind::project_graph:
        case QueryKind::file_content:
        case QueryKind::function_body_syntax:
            return false;
        case QueryKind::module_graph:
            return edge.dependency.kind == QueryKind::project_graph || edge.dependency.kind == QueryKind::module_part;
        case QueryKind::lex_file:
            return edge.dependency.kind == QueryKind::file_content;
        case QueryKind::parse_file:
            return edge.dependency.kind == QueryKind::lex_file;
        case QueryKind::module_part:
            return edge.dependency.kind == QueryKind::parse_file;
        case QueryKind::item_list:
            return edge.dependency.kind == QueryKind::module_graph;
        case QueryKind::module_exports:
            return edge.dependency.kind == QueryKind::item_list || edge.dependency.kind == QueryKind::module_exports;
        case QueryKind::module_package_exports:
            return edge.dependency.kind == QueryKind::item_list || edge.dependency.kind == QueryKind::module_exports
                || edge.dependency.kind == QueryKind::module_package_exports;
        case QueryKind::item_signature:
            return edge.dependency.kind == QueryKind::module_exports;
        case QueryKind::generic_template_signature:
            return edge.dependency.kind == QueryKind::item_list;
        case QueryKind::generic_instance_signature:
            return edge.dependency.kind == QueryKind::generic_template_signature;
        case QueryKind::type_check_body:
            return edge.dependency.kind == QueryKind::function_body_syntax
                || edge.dependency.kind == QueryKind::item_signature;
        case QueryKind::generic_instance_body:
            return edge.dependency.kind == QueryKind::generic_instance_signature;
        case QueryKind::lower_function_ir:
            return edge.dependency.kind == QueryKind::type_check_body
                || edge.dependency.kind == QueryKind::generic_instance_body;
        case QueryKind::diagnostics:
            return edge.dependency.kind != QueryKind::diagnostics && edge.dependency.kind != QueryKind::invalid;
        case QueryKind::invalid:
            return false;
    }
}

} // namespace aurex::query
