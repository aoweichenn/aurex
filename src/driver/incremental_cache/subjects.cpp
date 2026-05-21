#include "subjects/detail.hpp"

namespace aurex::driver::incremental_cache_detail {

[[nodiscard]] QuerySubjectCollection collect_query_subjects(
    const std::span<const ModuleRecord> modules, const sema::CheckedModule& checked, const base::SourceManager& sources,
    const syntax::AstModule* const ast)
{
    QuerySubjectCollection collection;
    collect_source_file_query_subjects(collection, sources);
    collection.module_graphs = collect_module_graph_query_subjects(modules);
    collection.item_lists = collect_item_list_query_subjects(modules, checked);
    collection.module_exports = collect_module_exports_query_subjects(modules, checked);
    collection.item_signatures = collect_item_signature_query_subjects(checked);
    collect_function_body_query_subjects(
        checked, sources, ast, collection.function_body_syntaxes, collection.type_check_bodies);
    collection.generic_template_signatures = collect_generic_template_signature_query_subjects(checked);
    collection.generic_instance_signatures = collect_generic_instance_signature_query_subjects(checked);
    collection.generic_instance_bodies = collect_generic_instance_body_query_subjects(checked, sources);
    collection.lower_function_irs =
        collect_lower_function_ir_query_subjects(collection.type_check_bodies, collection.generic_instance_bodies);
    build_ordered_query_subjects(collection);
    return collection;
}

} // namespace aurex::driver::incremental_cache_detail
