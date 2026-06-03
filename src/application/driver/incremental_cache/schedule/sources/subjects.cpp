#include <application/driver/incremental_cache/subjects/private/detail.hpp>

namespace aurex::driver::incremental_cache_detail {

[[nodiscard]] QuerySubjectCollection collect_query_subjects(const std::span<const ModuleRecord> modules,
    const sema::CheckedModule& checked, const base::SourceManager& sources, const syntax::AstModule* const ast,
    const project::ProjectModel& project_model, const ir::Module* const lowered_ir,
    const bool include_lowering_subjects)
{
    QuerySubjectCollection collection;
    collect_source_file_query_subjects(collection, sources, modules);
    collection.module_parts = collect_module_part_query_subjects(modules);
    collection.module_graphs = collect_module_graph_query_subjects(modules);
    collect_project_graph_query_subjects(collection, project_model, modules);
    collection.item_lists = collect_item_list_query_subjects(modules, checked, ast);
    collection.module_exports = collect_module_exports_query_subjects(modules, checked, ast);
    collection.module_package_exports = collect_module_package_exports_query_subjects(modules, checked, ast);
    collection.item_signatures = collect_item_signature_query_subjects(checked, modules);
    collect_function_body_query_subjects(
        checked, sources, ast, modules, collection.function_body_syntaxes, collection.type_check_bodies);
    collection.generic_template_signatures = collect_generic_template_signature_query_subjects(checked, modules);
    collection.generic_instance_signatures = collect_generic_instance_signature_query_subjects(checked);
    collection.generic_instance_bodies = collect_generic_instance_body_query_subjects(checked, sources, ast);
    if (include_lowering_subjects && lowered_ir != nullptr) {
        collection.lower_function_irs = collect_lower_function_ir_query_subjects(
            collection.type_check_bodies, collection.generic_instance_bodies, checked, modules, *lowered_ir);
    }
    build_ordered_query_subjects(collection);
    return collection;
}

} // namespace aurex::driver::incremental_cache_detail
