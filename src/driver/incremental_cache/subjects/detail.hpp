#pragma once

#include <aurex/base/source.hpp>
#include <aurex/sema/checked_module.hpp>
#include <aurex/syntax/ast.hpp>

#include <span>
#include <vector>

#include "../subjects.hpp"

namespace aurex::driver::incremental_cache_detail {

void collect_source_file_query_subjects(
    QuerySubjectCollection& collection, const base::SourceManager& sources, std::span<const ModuleRecord> modules);

[[nodiscard]] std::vector<ModuleGraphQuerySubject> collect_module_graph_query_subjects(
    std::span<const ModuleRecord> modules);
[[nodiscard]] std::vector<ModulePartQuerySubject> collect_module_part_query_subjects(
    std::span<const ModuleRecord> modules);
[[nodiscard]] std::vector<ModuleExportsQuerySubject> collect_module_exports_query_subjects(
    std::span<const ModuleRecord> modules, const sema::CheckedModule& checked, const syntax::AstModule* ast);
[[nodiscard]] std::vector<ModulePackageExportsQuerySubject> collect_module_package_exports_query_subjects(
    std::span<const ModuleRecord> modules, const sema::CheckedModule& checked, const syntax::AstModule* ast);
[[nodiscard]] std::vector<ItemListQuerySubject> collect_item_list_query_subjects(
    std::span<const ModuleRecord> modules, const sema::CheckedModule& checked, const syntax::AstModule* ast);

[[nodiscard]] std::vector<ItemSignatureQuerySubject> collect_item_signature_query_subjects(
    const sema::CheckedModule& checked, std::span<const ModuleRecord> modules);
[[nodiscard]] std::vector<GenericTemplateSignatureQuerySubject> collect_generic_template_signature_query_subjects(
    const sema::CheckedModule& checked, std::span<const ModuleRecord> modules);
[[nodiscard]] std::vector<GenericInstanceSignatureQuerySubject> collect_generic_instance_signature_query_subjects(
    const sema::CheckedModule& checked);
[[nodiscard]] std::vector<GenericInstanceBodyQuerySubject> collect_generic_instance_body_query_subjects(
    const sema::CheckedModule& checked, const base::SourceManager& sources);
void collect_function_body_query_subjects(const sema::CheckedModule& checked, const base::SourceManager& sources,
    const syntax::AstModule* ast, std::span<const ModuleRecord> modules,
    std::vector<FunctionBodySyntaxQuerySubject>& syntax_subjects,
    std::vector<TypeCheckBodyQuerySubject>& type_check_subjects);
[[nodiscard]] std::vector<LowerFunctionIRQuerySubject> collect_lower_function_ir_query_subjects(
    const std::vector<TypeCheckBodyQuerySubject>& type_check_subjects,
    const std::vector<GenericInstanceBodyQuerySubject>& generic_body_subjects);

void build_ordered_query_subjects(QuerySubjectCollection& collection);

} // namespace aurex::driver::incremental_cache_detail
