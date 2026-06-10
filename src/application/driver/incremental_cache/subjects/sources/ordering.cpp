#include <algorithm>
#include <optional>
#include <unordered_set>
#include <utility>
#include <vector>

#include <application/driver/incremental_cache/schedule/private/schedule.hpp>
#include <application/driver/incremental_cache/subjects/private/detail.hpp>

namespace aurex::driver::incremental_cache_detail {
namespace cache_format = incremental_cache_format;
using namespace cache_format;

namespace {

[[nodiscard]] query::QueryResultFingerprint diagnostics_result_fingerprint(const query::QueryKey producer)
{
    query::StableHashBuilder builder;
    builder.mix_string(INCREMENTAL_CACHE_DIAGNOSTICS_RESULT_MARKER);
    builder.mix_fingerprint(query::stable_key_fingerprint(producer));
    return query::query_result_fingerprint(builder.finish());
}

void evaluate_file_content_query_subject(query::QueryContext& context, const FileContentQuerySubject& subject)
{
    const query::FileContentProviderInput input{
        subject.key,
        subject.result,
    };
    static_cast<void>(context.evaluate_file_content(input));
}

void evaluate_project_graph_query_subject(query::QueryContext& context, const ProjectGraphQuerySubject& subject)
{
    const query::ProjectGraphProviderInput input{
        subject.key,
        subject.result,
    };
    static_cast<void>(context.evaluate_project_graph(input));
}

void evaluate_dyn_ownership_runtime_boundary_gate_query_subject(
    query::QueryContext& context, const DynOwnershipRuntimeBoundaryGateQuerySubject& subject)
{
    const query::DynOwnershipRuntimeBoundaryGateProviderInput input{
        subject.key,
        subject.gate,
    };
    static_cast<void>(context.evaluate_dyn_ownership_runtime_boundary_gate(input));
}

void evaluate_lex_file_query_subject(query::QueryContext& context, const LexFileQuerySubject& subject)
{
    const query::LexFileProviderInput input{
        subject.key,
        subject.result,
    };
    static_cast<void>(context.evaluate_lex_file(input));
}

void evaluate_parse_file_query_subject(query::QueryContext& context, const ParseFileQuerySubject& subject)
{
    const query::ParseFileProviderInput input{
        subject.key,
        subject.result,
    };
    static_cast<void>(context.evaluate_parse_file(input));
}

void evaluate_module_graph_query_subject(query::QueryContext& context, const ModuleGraphQuerySubject& subject)
{
    std::vector<query::QueryKey> dependencies;
    dependencies.reserve(subject.part_dependencies.size() + 1U);
    if (const std::optional<query::QueryKey> project_graph = query::project_graph_query_key(subject.project)) {
        dependencies.push_back(*project_graph);
    }
    for (const query::ModulePartKey& part : subject.part_dependencies) {
        if (const std::optional<query::QueryKey> part_key = query::module_part_query_key(part)) {
            dependencies.push_back(*part_key);
        }
    }
    const query::ModuleGraphProviderInput input{
        subject.key,
        subject.result,
        std::move(dependencies),
    };
    static_cast<void>(context.evaluate_module_graph(input));
}

void evaluate_module_part_query_subject(query::QueryContext& context, const ModulePartQuerySubject& subject)
{
    const query::ModulePartProviderInput input{
        subject.key,
        subject.result,
    };
    static_cast<void>(context.evaluate_module_part(input));
}

void evaluate_module_exports_query_subject(query::QueryContext& context, const ModuleExportsQuerySubject& subject)
{
    const query::ModuleExportsProviderInput input{
        subject.key,
        subject.result,
        subject.reexport_dependencies,
    };
    static_cast<void>(context.evaluate_module_exports(input));
}

void evaluate_module_package_exports_query_subject(
    query::QueryContext& context, const ModulePackageExportsQuerySubject& subject)
{
    const query::ModulePackageExportsProviderInput input{
        subject.key,
        subject.result,
        subject.public_surface_dependencies,
        subject.package_surface_dependencies,
    };
    static_cast<void>(context.evaluate_module_package_exports(input));
}

void evaluate_item_list_query_subject(query::QueryContext& context, const ItemListQuerySubject& subject)
{
    const query::ItemListProviderInput input{
        subject.key,
        subject.result,
    };
    static_cast<void>(context.evaluate_item_list(input));
}

void evaluate_item_signature_query_subject(query::QueryContext& context, const ItemSignatureQuerySubject& subject)
{
    const query::ItemSignatureProviderInput input{
        subject.key,
        subject.authority,
    };
    static_cast<void>(context.evaluate_item_signature(input));
}

void evaluate_generic_template_signature_query_subject(
    query::QueryContext& context, const GenericTemplateSignatureQuerySubject& subject)
{
    const query::GenericTemplateSignatureProviderInput input{
        subject.key,
        subject.authority,
    };
    static_cast<void>(context.evaluate_generic_template_signature(input));
}

void evaluate_generic_instance_signature_query_subject(
    query::QueryContext& context, const GenericInstanceSignatureQuerySubject& subject)
{
    const query::GenericInstanceSignatureProviderInput input{
        subject.key,
        subject.authority,
    };
    static_cast<void>(context.evaluate_generic_instance_signature(input));
}

void evaluate_generic_instance_body_query_subject(
    query::QueryContext& context, const GenericInstanceBodyQuerySubject& subject)
{
    const query::GenericInstanceBodyProviderInput input{
        subject.key,
        subject.authority,
    };
    static_cast<void>(context.evaluate_generic_instance_body(input));
}

void evaluate_lower_function_ir_query_subject(query::QueryContext& context, const LowerFunctionIRQuerySubject& subject)
{
    switch (subject.kind) {
        case LowerFunctionIRSubjectKind::body: {
            const query::LowerFunctionIRProviderInput input{
                subject.body,
                subject.ir,
                subject.cleanup_markers,
                subject.dyn_abi,
            };
            static_cast<void>(context.evaluate_lower_function_ir(input));
            return;
        }
        case LowerFunctionIRSubjectKind::generic_instance: {
            const query::LowerGenericInstanceIRProviderInput input{
                subject.generic_instance,
                subject.ir,
                subject.cleanup_markers,
                subject.dyn_abi,
            };
            static_cast<void>(context.evaluate_lower_generic_instance_ir(input));
            return;
        }
    }
}

void evaluate_function_body_syntax_query_subject(
    query::QueryContext& context, const FunctionBodySyntaxQuerySubject& subject)
{
    const query::FunctionBodySyntaxProviderInput input{
        subject.key,
        subject.authority,
    };
    static_cast<void>(context.evaluate_function_body_syntax(input));
}

void evaluate_type_check_body_query_subject(query::QueryContext& context, const TypeCheckBodyQuerySubject& subject)
{
    const query::TypeCheckBodyProviderInput input{
        subject.key,
        subject.authority,
    };
    static_cast<void>(context.evaluate_type_check_body(input));
}

void evaluate_diagnostics_query_subject(query::QueryContext& context, const DiagnosticsQuerySubject& subject)
{
    const query::DiagnosticsProviderInput input{
        subject.producer,
        subject.result,
    };
    static_cast<void>(context.evaluate_diagnostics(input));
}

[[nodiscard]] std::optional<query::QueryRecord> query_record_for_subject(const FileContentQuerySubject& subject)
{
    return query::file_content_query_record(subject.key, subject.result);
}

[[nodiscard]] std::optional<query::QueryRecord> query_record_for_subject(const ProjectGraphQuerySubject& subject)
{
    return query::project_graph_query_record(subject.key, subject.result);
}

[[nodiscard]] std::optional<query::QueryRecord> query_record_for_subject(
    const DynOwnershipRuntimeBoundaryGateQuerySubject& subject)
{
    return query::dyn_ownership_runtime_boundary_gate_query_record(
        subject.key, query::dyn_ownership_runtime_boundary_gate_result_fingerprint(subject.gate));
}

[[nodiscard]] std::optional<query::QueryRecord> query_record_for_subject(const LexFileQuerySubject& subject)
{
    return query::lex_file_query_record(subject.key, subject.result);
}

[[nodiscard]] std::optional<query::QueryRecord> query_record_for_subject(const ParseFileQuerySubject& subject)
{
    return query::parse_file_query_record(subject.key, subject.result);
}

[[nodiscard]] std::optional<query::QueryRecord> query_record_for_subject(const ModuleGraphQuerySubject& subject)
{
    return query::module_graph_query_record(subject.key, subject.result);
}

[[nodiscard]] std::optional<query::QueryRecord> query_record_for_subject(const ModulePartQuerySubject& subject)
{
    return query::module_part_query_record(subject.key, subject.result);
}

[[nodiscard]] std::optional<query::QueryRecord> query_record_for_subject(const ModuleExportsQuerySubject& subject)
{
    return query::module_exports_query_record(subject.key, subject.result);
}

[[nodiscard]] std::optional<query::QueryRecord> query_record_for_subject(
    const ModulePackageExportsQuerySubject& subject)
{
    return query::module_package_exports_query_record(subject.key, subject.result);
}

[[nodiscard]] std::optional<query::QueryRecord> query_record_for_subject(const ItemListQuerySubject& subject)
{
    return query::item_list_query_record(subject.key, subject.result);
}

[[nodiscard]] std::optional<query::QueryRecord> query_record_for_subject(const ItemSignatureQuerySubject& subject)
{
    return query::item_signature_query_record(subject.key, query::item_signature_result_fingerprint(subject.authority));
}

[[nodiscard]] std::optional<query::QueryRecord> query_record_for_subject(
    const GenericTemplateSignatureQuerySubject& subject)
{
    return query::generic_template_signature_query_record(
        subject.key, query::generic_template_signature_result_fingerprint(subject.authority));
}

[[nodiscard]] std::optional<query::QueryRecord> query_record_for_subject(
    const GenericInstanceSignatureQuerySubject& subject)
{
    if (subject.key == nullptr) {
        return std::nullopt;
    }
    return query::generic_instance_signature_query_record(
        *subject.key, query::generic_instance_signature_result_fingerprint(subject.authority));
}

[[nodiscard]] std::optional<query::QueryRecord> query_record_for_subject(const GenericInstanceBodyQuerySubject& subject)
{
    if (subject.key == nullptr) {
        return std::nullopt;
    }
    return query::generic_instance_body_query_record(
        *subject.key, query::generic_instance_body_result_fingerprint(subject.authority));
}

[[nodiscard]] std::optional<query::QueryRecord> query_record_for_subject(const LowerFunctionIRQuerySubject& subject)
{
    switch (subject.kind) {
        case LowerFunctionIRSubjectKind::body:
            return query::lower_function_ir_query_record(subject.body, subject.result);
        case LowerFunctionIRSubjectKind::generic_instance:
            if (subject.generic_instance == nullptr) {
                return std::nullopt;
            }
            return query::lower_generic_instance_ir_query_record(*subject.generic_instance, subject.result);
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<query::QueryRecord> query_record_for_subject(const FunctionBodySyntaxQuerySubject& subject)
{
    return query::function_body_syntax_query_record(
        subject.key, query::function_body_syntax_result_fingerprint(subject.authority));
}

[[nodiscard]] std::optional<query::QueryRecord> query_record_for_subject(const TypeCheckBodyQuerySubject& subject)
{
    return query::type_check_body_query_record(
        subject.key, query::type_check_body_result_fingerprint(subject.authority));
}

[[nodiscard]] std::optional<query::QueryRecord> query_record_for_subject(const DiagnosticsQuerySubject& subject)
{
    return query::diagnostics_query_record(subject.producer, subject.result);
}

void push_query_subject(std::vector<QuerySubject>& subjects,
    std::unordered_set<query::QueryKey, query::QueryKeyHash>& keys, const QuerySubjectKind kind,
    const base::usize index, std::optional<query::QueryRecord> record)
{
    if (!record) {
        return;
    }
    const auto inserted = keys.insert(record->key);
    if (!inserted.second) {
        return;
    }
    subjects.push_back(QuerySubject{
        kind,
        index,
        std::move(*record),
    });
}

void collect_diagnostics_query_subjects(QuerySubjectCollection& collection)
{
    collection.diagnostics.reserve(collection.subjects.size());
    for (const QuerySubject& subject : collection.subjects) {
        const query::QueryResultFingerprint result = diagnostics_result_fingerprint(subject.record.key);
        if (!query::is_valid(result)) {
            continue;
        }
        collection.diagnostics.push_back(DiagnosticsQuerySubject{
            subject.record.key,
            result,
        });
    }
}

} // namespace

void build_ordered_query_subjects(QuerySubjectCollection& collection)
{
    collection.subjects.reserve(collection.project_graphs.size() + collection.file_contents.size()
        + collection.dyn_ownership_runtime_boundary_gates.size() + collection.lex_files.size()
        + collection.parse_files.size() + collection.module_parts.size() + collection.module_graphs.size()
        + collection.module_exports.size() + collection.module_package_exports.size() + collection.item_lists.size()
        + collection.item_signatures.size() + collection.function_body_syntaxes.size()
        + collection.type_check_bodies.size() + collection.generic_template_signatures.size()
        + collection.generic_instance_signatures.size() + collection.generic_instance_bodies.size()
        + collection.lower_function_irs.size());
    std::unordered_set<query::QueryKey, query::QueryKeyHash> keys;
    keys.reserve(collection.project_graphs.size() + collection.file_contents.size() + collection.lex_files.size()
        + collection.dyn_ownership_runtime_boundary_gates.size() + collection.parse_files.size()
        + collection.module_parts.size() + collection.module_graphs.size() + collection.module_exports.size()
        + collection.module_package_exports.size() + collection.item_lists.size() + collection.item_signatures.size()
        + collection.function_body_syntaxes.size() + collection.type_check_bodies.size()
        + collection.generic_template_signatures.size()
        + collection.generic_instance_signatures.size() + collection.generic_instance_bodies.size()
        + collection.lower_function_irs.size());

    for (base::usize index = 0; index < collection.project_graphs.size(); ++index) {
        push_query_subject(collection.subjects, keys, QuerySubjectKind::project_graph, index,
            query_record_for_subject(collection.project_graphs[index]));
    }
    for (base::usize index = 0; index < collection.dyn_ownership_runtime_boundary_gates.size(); ++index) {
        push_query_subject(collection.subjects, keys, QuerySubjectKind::dyn_ownership_runtime_boundary_gate, index,
            query_record_for_subject(collection.dyn_ownership_runtime_boundary_gates[index]));
    }
    for (base::usize index = 0; index < collection.file_contents.size(); ++index) {
        push_query_subject(collection.subjects, keys, QuerySubjectKind::file_content, index,
            query_record_for_subject(collection.file_contents[index]));
    }
    for (base::usize index = 0; index < collection.lex_files.size(); ++index) {
        push_query_subject(collection.subjects, keys, QuerySubjectKind::lex_file, index,
            query_record_for_subject(collection.lex_files[index]));
    }
    for (base::usize index = 0; index < collection.parse_files.size(); ++index) {
        push_query_subject(collection.subjects, keys, QuerySubjectKind::parse_file, index,
            query_record_for_subject(collection.parse_files[index]));
    }
    for (base::usize index = 0; index < collection.module_parts.size(); ++index) {
        push_query_subject(collection.subjects, keys, QuerySubjectKind::module_part, index,
            query_record_for_subject(collection.module_parts[index]));
    }
    for (base::usize index = 0; index < collection.module_graphs.size(); ++index) {
        push_query_subject(collection.subjects, keys, QuerySubjectKind::module_graph, index,
            query_record_for_subject(collection.module_graphs[index]));
    }
    for (base::usize index = 0; index < collection.module_exports.size(); ++index) {
        push_query_subject(collection.subjects, keys, QuerySubjectKind::module_exports, index,
            query_record_for_subject(collection.module_exports[index]));
    }
    for (base::usize index = 0; index < collection.module_package_exports.size(); ++index) {
        push_query_subject(collection.subjects, keys, QuerySubjectKind::module_package_exports, index,
            query_record_for_subject(collection.module_package_exports[index]));
    }
    for (base::usize index = 0; index < collection.item_lists.size(); ++index) {
        push_query_subject(collection.subjects, keys, QuerySubjectKind::item_list, index,
            query_record_for_subject(collection.item_lists[index]));
    }
    for (base::usize index = 0; index < collection.item_signatures.size(); ++index) {
        push_query_subject(collection.subjects, keys, QuerySubjectKind::item_signature, index,
            query_record_for_subject(collection.item_signatures[index]));
    }
    for (base::usize index = 0; index < collection.function_body_syntaxes.size(); ++index) {
        push_query_subject(collection.subjects, keys, QuerySubjectKind::function_body_syntax, index,
            query_record_for_subject(collection.function_body_syntaxes[index]));
    }
    for (base::usize index = 0; index < collection.type_check_bodies.size(); ++index) {
        push_query_subject(collection.subjects, keys, QuerySubjectKind::type_check_body, index,
            query_record_for_subject(collection.type_check_bodies[index]));
    }
    for (base::usize index = 0; index < collection.generic_template_signatures.size(); ++index) {
        push_query_subject(collection.subjects, keys, QuerySubjectKind::generic_template_signature, index,
            query_record_for_subject(collection.generic_template_signatures[index]));
    }
    for (base::usize index = 0; index < collection.generic_instance_signatures.size(); ++index) {
        push_query_subject(collection.subjects, keys, QuerySubjectKind::generic_instance_signature, index,
            query_record_for_subject(collection.generic_instance_signatures[index]));
    }
    for (base::usize index = 0; index < collection.generic_instance_bodies.size(); ++index) {
        push_query_subject(collection.subjects, keys, QuerySubjectKind::generic_instance_body, index,
            query_record_for_subject(collection.generic_instance_bodies[index]));
    }
    for (base::usize index = 0; index < collection.lower_function_irs.size(); ++index) {
        push_query_subject(collection.subjects, keys, QuerySubjectKind::lower_function_ir, index,
            query_record_for_subject(collection.lower_function_irs[index]));
    }

    collect_diagnostics_query_subjects(collection);
    collection.subjects.reserve(collection.subjects.size() + collection.diagnostics.size());
    keys.reserve(keys.size() + collection.diagnostics.size());
    for (base::usize index = 0; index < collection.diagnostics.size(); ++index) {
        push_query_subject(collection.subjects, keys, QuerySubjectKind::diagnostics, index,
            query_record_for_subject(collection.diagnostics[index]));
    }

    std::sort(collection.subjects.begin(), collection.subjects.end(), query_subject_schedule_less);

    collection.records.reserve(collection.subjects.size());
    for (const QuerySubject& subject : collection.subjects) {
        collection.records.push_back(subject.record);
    }
}

void evaluate_query_subject(
    query::QueryContext& context, const QuerySubjectCollection& collection, const QuerySubject& subject)
{
    switch (subject.kind) {
        case QuerySubjectKind::project_graph:
            evaluate_project_graph_query_subject(context, collection.project_graphs[subject.index]);
            return;
        case QuerySubjectKind::dyn_ownership_runtime_boundary_gate:
            evaluate_dyn_ownership_runtime_boundary_gate_query_subject(
                context, collection.dyn_ownership_runtime_boundary_gates[subject.index]);
            return;
        case QuerySubjectKind::file_content:
            evaluate_file_content_query_subject(context, collection.file_contents[subject.index]);
            return;
        case QuerySubjectKind::lex_file:
            evaluate_lex_file_query_subject(context, collection.lex_files[subject.index]);
            return;
        case QuerySubjectKind::parse_file:
            evaluate_parse_file_query_subject(context, collection.parse_files[subject.index]);
            return;
        case QuerySubjectKind::module_part:
            evaluate_module_part_query_subject(context, collection.module_parts[subject.index]);
            return;
        case QuerySubjectKind::module_graph:
            evaluate_module_graph_query_subject(context, collection.module_graphs[subject.index]);
            return;
        case QuerySubjectKind::module_exports:
            evaluate_module_exports_query_subject(context, collection.module_exports[subject.index]);
            return;
        case QuerySubjectKind::module_package_exports:
            evaluate_module_package_exports_query_subject(context, collection.module_package_exports[subject.index]);
            return;
        case QuerySubjectKind::item_list:
            evaluate_item_list_query_subject(context, collection.item_lists[subject.index]);
            return;
        case QuerySubjectKind::item_signature:
            evaluate_item_signature_query_subject(context, collection.item_signatures[subject.index]);
            return;
        case QuerySubjectKind::function_body_syntax:
            evaluate_function_body_syntax_query_subject(context, collection.function_body_syntaxes[subject.index]);
            return;
        case QuerySubjectKind::type_check_body:
            evaluate_type_check_body_query_subject(context, collection.type_check_bodies[subject.index]);
            return;
        case QuerySubjectKind::generic_template_signature:
            evaluate_generic_template_signature_query_subject(
                context, collection.generic_template_signatures[subject.index]);
            return;
        case QuerySubjectKind::generic_instance_signature:
            evaluate_generic_instance_signature_query_subject(
                context, collection.generic_instance_signatures[subject.index]);
            return;
        case QuerySubjectKind::generic_instance_body:
            evaluate_generic_instance_body_query_subject(context, collection.generic_instance_bodies[subject.index]);
            return;
        case QuerySubjectKind::lower_function_ir:
            evaluate_lower_function_ir_query_subject(context, collection.lower_function_irs[subject.index]);
            return;
        case QuerySubjectKind::diagnostics:
            evaluate_diagnostics_query_subject(context, collection.diagnostics[subject.index]);
            return;
    }
}

} // namespace aurex::driver::incremental_cache_detail
