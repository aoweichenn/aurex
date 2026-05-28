#include <algorithm>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "detail.hpp"

namespace aurex::driver::incremental_cache_detail {
namespace cache_format = incremental_cache_format;
using namespace cache_format;

namespace {

[[nodiscard]] query::PackageKey cache_package_key_or_default(const query::PackageKey package) noexcept
{
    if (query::is_valid(package)) {
        return package;
    }
    return query::package_key(std::span<const std::string_view>{});
}

[[nodiscard]] std::vector<std::string_view> module_name_parts(const std::string_view module_name)
{
    std::vector<std::string_view> parts;
    base::usize begin = 0;
    while (begin < module_name.size()) {
        const base::usize end = module_name.find(INCREMENTAL_CACHE_MODULE_NAME_SEPARATOR, begin);
        if (end == std::string_view::npos) {
            parts.push_back(module_name.substr(begin));
            break;
        }
        parts.push_back(module_name.substr(begin, end - begin));
        begin = end + 1;
    }
    return parts;
}

[[nodiscard]] sema::StableModuleId stable_module_id_from_record(const ModuleRecord& module)
{
    const std::vector<std::string_view> parts = module_name_parts(module.name);
    return sema::stable_module_id(std::span<const std::string_view>{parts.data(), parts.size()});
}

struct PackageIndex {
    std::unordered_map<base::u32, query::PackageKey> by_module_id;
    std::unordered_map<std::string, query::PackageKey> by_stable_module;
};

[[nodiscard]] PackageIndex package_index_for_modules(const std::span<const ModuleRecord> modules)
{
    PackageIndex packages;
    packages.by_module_id.reserve(modules.size());
    packages.by_stable_module.reserve(modules.size());
    for (const ModuleRecord& module : modules) {
        const query::PackageKey package = cache_package_key_or_default(module.package);
        if (syntax::is_valid(module.id)) {
            packages.by_module_id.emplace(module.id.value, package);
        }
        packages.by_stable_module.emplace(query::stable_serialize(stable_module_id_from_record(module)), package);
    }
    return packages;
}

[[nodiscard]] query::PackageKey package_for_definition(
    const PackageIndex& packages, const sema::StableModuleId stable_module, const syntax::ModuleId module_id)
{
    if (syntax::is_valid(module_id)) {
        const auto found_by_id = packages.by_module_id.find(module_id.value);
        if (found_by_id != packages.by_module_id.end()) {
            return cache_package_key_or_default(found_by_id->second);
        }
    }
    const auto found_by_stable_id = packages.by_stable_module.find(query::stable_serialize(stable_module));
    if (found_by_stable_id != packages.by_stable_module.end()) {
        return cache_package_key_or_default(found_by_stable_id->second);
    }
    return cache_package_key_or_default({});
}

[[nodiscard]] query::DefKey def_key_from_stable_id(const PackageIndex& packages, const sema::StableDefId& stable_id,
    const syntax::ModuleId module_id, const query::DefNamespace name_space, const query::DefKind kind)
{
    return query::def_key_from_stable_id(
        package_for_definition(packages, stable_id.module, module_id), stable_id, name_space, kind);
}

[[nodiscard]] query::DefKind function_signature_def_kind(const sema::FunctionSignature& signature) noexcept
{
    return signature.is_method ? query::DefKind::method : query::DefKind::function;
}

[[nodiscard]] std::optional<std::string_view> source_range_text(
    const base::SourceManager& sources, const base::SourceRange& range) noexcept
{
    const std::span<const base::SourceFile> files = sources.files();
    if (range.source.value >= files.size()) {
        return std::nullopt;
    }
    const std::string_view text = files[range.source.value].text();
    if (range.begin > range.end || range.end > text.size()) {
        return std::nullopt;
    }
    return std::string_view{text.data() + range.begin, range.end - range.begin};
}

[[nodiscard]] query::BodyKey function_body_key(const sema::FunctionSignature& signature, const PackageIndex& packages)
{
    return query::body_key(def_key_from_stable_id(packages, signature.stable_id, signature.module,
                               query::DefNamespace::value, function_signature_def_kind(signature)),
        query::BodySlotKind::function_body);
}

[[nodiscard]] query::QueryResultFingerprint function_body_syntax_result_fingerprint(
    const query::BodyKey key, const std::string_view body_text)
{
    query::StableHashBuilder builder;
    builder.mix_string(INCREMENTAL_CACHE_FUNCTION_BODY_SYNTAX_RESULT_MARKER);
    builder.mix_fingerprint(query::stable_key_fingerprint(key));
    builder.mix_string(body_text);
    return query::query_result_fingerprint(builder.finish());
}

[[nodiscard]] query::QueryResultFingerprint type_check_body_result_fingerprint(const query::BodyKey key,
    const query::QueryResultFingerprint body_syntax_result, const sema::IncrementalKey& signature_key)
{
    query::StableHashBuilder builder;
    builder.mix_string(INCREMENTAL_CACHE_TYPE_CHECK_BODY_RESULT_MARKER);
    builder.mix_fingerprint(query::stable_key_fingerprint(key));
    builder.mix_u64(body_syntax_result.global_id);
    builder.mix_fingerprint(body_syntax_result.fingerprint);
    builder.mix_u64(signature_key.global_id);
    builder.mix_fingerprint(signature_key.fingerprint);
    return query::query_result_fingerprint(builder.finish());
}

[[nodiscard]] std::optional<base::SourceRange> function_signature_body_range(
    const sema::FunctionSignature& signature, const syntax::AstModule& ast) noexcept
{
    const syntax::ItemId item =
        syntax::is_valid(signature.definition_item) ? signature.definition_item : signature.prototype_item;
    if (!syntax::is_valid(item) || item.value >= ast.items.size()) {
        return std::nullopt;
    }
    const syntax::ItemNode* const node = ast.items.ptr(item.value);
    if (node == nullptr || node->kind != syntax::ItemKind::fn_decl || !syntax::is_valid(node->body)
        || node->body.value >= ast.stmts.size()) {
        return std::nullopt;
    }
    return ast.stmts.range(node->body.value);
}

[[nodiscard]] std::optional<std::string_view> function_body_text(
    const base::SourceManager& sources, const sema::FunctionSignature& signature, const syntax::AstModule& ast) noexcept
{
    if (const std::optional<base::SourceRange> body_range = function_signature_body_range(signature, ast)) {
        return source_range_text(sources, *body_range);
    }
    return source_range_text(sources, signature.range);
}

[[nodiscard]] query::QueryResultFingerprint generic_instance_body_result_fingerprint(
    const query::GenericInstanceKey& key, const sema::IncrementalKey& signature_key, const std::string_view body_text)
{
    query::StableHashBuilder builder;
    builder.mix_string(INCREMENTAL_CACHE_GENERIC_INSTANCE_BODY_RESULT_MARKER);
    builder.mix_fingerprint(query::stable_key_fingerprint(key));
    builder.mix_u64(signature_key.global_id);
    builder.mix_fingerprint(signature_key.fingerprint);
    builder.mix_string(body_text);
    return query::query_result_fingerprint(builder.finish());
}

[[nodiscard]] query::QueryResultFingerprint lower_function_ir_result_fingerprint(
    const query::BodyKey key, const query::QueryResultFingerprint type_check_result)
{
    query::StableHashBuilder builder;
    builder.mix_string(INCREMENTAL_CACHE_LOWER_FUNCTION_IR_RESULT_MARKER);
    builder.mix_string("body");
    builder.mix_fingerprint(query::stable_key_fingerprint(key));
    builder.mix_u64(type_check_result.global_id);
    builder.mix_fingerprint(type_check_result.fingerprint);
    return query::query_result_fingerprint(builder.finish());
}

[[nodiscard]] query::QueryResultFingerprint lower_generic_instance_ir_result_fingerprint(
    const query::GenericInstanceKey& key, const query::QueryResultFingerprint generic_body_result)
{
    query::StableHashBuilder builder;
    builder.mix_string(INCREMENTAL_CACHE_LOWER_FUNCTION_IR_RESULT_MARKER);
    builder.mix_string("generic-instance");
    builder.mix_fingerprint(query::stable_key_fingerprint(key));
    builder.mix_u64(generic_body_result.global_id);
    builder.mix_fingerprint(generic_body_result.fingerprint);
    return query::query_result_fingerprint(builder.finish());
}

void push_definition(std::vector<DefinitionRecord>& records, const std::string_view category,
    const std::string_view name, const sema::StableDefId& stable_id, const sema::IncrementalKey& incremental_key)
{
    records.push_back(DefinitionRecord{
        std::string(category),
        std::string(name),
        stable_id,
        incremental_key,
    });
}

void push_item_signature_query_subject(std::vector<ItemSignatureQuerySubject>& subjects,
    const sema::StableDefId& stable_id, const sema::IncrementalKey& incremental_key, const syntax::ModuleId module_id,
    const query::DefNamespace name_space, const query::DefKind kind, const PackageIndex& packages)
{
    subjects.push_back(ItemSignatureQuerySubject{
        def_key_from_stable_id(packages, stable_id, module_id, name_space, kind),
        incremental_key,
    });
}

void push_generic_template_signature_query_subject(std::vector<GenericTemplateSignatureQuerySubject>& subjects,
    const sema::GenericTemplateSignatureInfo& info, const PackageIndex& packages)
{
    subjects.push_back(GenericTemplateSignatureQuerySubject{
        def_key_from_stable_id(
            packages, info.stable_id, info.module, info.name_space, query::DefKind::generic_template),
        info.incremental_key,
    });
}

void push_generic_instance_signature_query_subject(std::vector<GenericInstanceSignatureQuerySubject>& subjects,
    const query::GenericInstanceKey& key, const sema::IncrementalKey& incremental_key)
{
    if (!query::is_valid(key) || !query::is_valid(incremental_key)) {
        return;
    }
    subjects.push_back(GenericInstanceSignatureQuerySubject{
        &key,
        incremental_key,
    });
}

void push_unique_generic_instance_signature_query_subject(std::vector<GenericInstanceSignatureQuerySubject>& subjects,
    std::unordered_set<query::GenericInstanceKey, query::GenericInstanceKeyHash>& seen,
    const query::GenericInstanceKey& key, const sema::IncrementalKey& incremental_key)
{
    if (!query::is_valid(key) || !query::is_valid(incremental_key)) {
        return;
    }
    if (!seen.insert(key).second) {
        return;
    }
    push_generic_instance_signature_query_subject(subjects, key, incremental_key);
}

void push_generic_instance_body_query_subject(std::vector<GenericInstanceBodyQuerySubject>& subjects,
    const sema::GenericFunctionInstanceInfo& instance, const base::SourceManager& sources)
{
    const query::GenericInstanceKey& instance_key = query::is_valid(instance.signature.generic_instance_key)
        ? instance.signature.generic_instance_key
        : instance.generic_instance_key;
    if (!query::is_valid(instance_key) || !query::is_valid(instance.signature.incremental_key)
        || !instance.signature.has_definition || instance.signature.has_conflict) {
        return;
    }
    const std::optional<std::string_view> body_text = source_range_text(sources, instance.signature.range);
    if (!body_text) {
        return;
    }
    const query::QueryResultFingerprint result =
        generic_instance_body_result_fingerprint(instance_key, instance.signature.incremental_key, *body_text);
    if (!query::is_valid(result)) {
        return;
    }
    subjects.push_back(GenericInstanceBodyQuerySubject{
        &instance_key,
        result,
    });
}

void push_lower_function_ir_query_subject(
    std::vector<LowerFunctionIRQuerySubject>& subjects, const TypeCheckBodyQuerySubject& type_check_subject)
{
    const query::QueryResultFingerprint result =
        lower_function_ir_result_fingerprint(type_check_subject.key, type_check_subject.result);
    subjects.push_back(LowerFunctionIRQuerySubject{
        LowerFunctionIRSubjectKind::body,
        type_check_subject.key,
        nullptr,
        result,
    });
}

void push_lower_generic_instance_ir_query_subject(
    std::vector<LowerFunctionIRQuerySubject>& subjects, const GenericInstanceBodyQuerySubject& generic_body_subject)
{
    if (generic_body_subject.key == nullptr) {
        return;
    }
    const query::QueryResultFingerprint result =
        lower_generic_instance_ir_result_fingerprint(*generic_body_subject.key, generic_body_subject.result);
    subjects.push_back(LowerFunctionIRQuerySubject{
        LowerFunctionIRSubjectKind::generic_instance,
        {},
        generic_body_subject.key,
        result,
    });
}

void push_function_body_query_subjects(std::vector<FunctionBodySyntaxQuerySubject>& syntax_subjects,
    std::vector<TypeCheckBodyQuerySubject>& type_check_subjects, const sema::FunctionSignature& signature,
    const base::SourceManager& sources, const syntax::AstModule* const ast, const PackageIndex& packages)
{
    if (!signature.has_definition || signature.has_conflict || !query::is_valid(signature.stable_id)
        || !query::is_valid(signature.incremental_key)) {
        return;
    }
    const query::BodyKey key = function_body_key(signature, packages);
    if (!query::is_valid(key)) {
        return;
    }
    const std::optional<std::string_view> body_text =
        ast == nullptr ? source_range_text(sources, signature.range) : function_body_text(sources, signature, *ast);
    if (!body_text) {
        return;
    }
    const query::QueryResultFingerprint syntax_result = function_body_syntax_result_fingerprint(key, *body_text);
    if (!query::is_valid(syntax_result)) {
        return;
    }
    syntax_subjects.push_back(FunctionBodySyntaxQuerySubject{
        key,
        syntax_result,
    });
    type_check_subjects.push_back(TypeCheckBodyQuerySubject{
        key,
        type_check_body_result_fingerprint(key, syntax_result, signature.incremental_key),
    });
}

} // namespace

[[nodiscard]] std::vector<ItemSignatureQuerySubject> collect_item_signature_query_subjects(
    const sema::CheckedModule& checked, const std::span<const ModuleRecord> modules)
{
    const PackageIndex packages = package_index_for_modules(modules);
    std::vector<ItemSignatureQuerySubject> subjects;
    subjects.reserve(
        checked.functions.size() + checked.structs.size() + checked.enum_cases.size() + checked.type_aliases.size());

    for (const auto& entry : checked.functions) {
        const sema::FunctionSignature& signature = entry.second;
        push_item_signature_query_subject(subjects, signature.stable_id, signature.incremental_key, signature.module,
            query::DefNamespace::value, function_signature_def_kind(signature), packages);
    }
    for (const auto& entry : checked.structs) {
        const sema::StructInfo& info = entry.second;
        push_item_signature_query_subject(subjects, info.stable_id, info.incremental_key, info.module,
            query::DefNamespace::type, query::DefKind::struct_, packages);
    }
    for (const auto& entry : checked.enum_cases) {
        const sema::EnumCaseInfo& info = entry.second;
        push_item_signature_query_subject(subjects, info.stable_id, info.incremental_key, info.module,
            query::DefNamespace::value, query::DefKind::enum_case, packages);
    }
    for (const auto& entry : checked.type_aliases) {
        const sema::TypeAliasInfo& info = entry.second;
        push_item_signature_query_subject(subjects, info.stable_id, info.incremental_key, info.module,
            query::DefNamespace::type, query::DefKind::type_alias, packages);
    }
    return subjects;
}

[[nodiscard]] std::vector<GenericTemplateSignatureQuerySubject> collect_generic_template_signature_query_subjects(
    const sema::CheckedModule& checked, const std::span<const ModuleRecord> modules)
{
    const PackageIndex packages = package_index_for_modules(modules);
    std::vector<GenericTemplateSignatureQuerySubject> subjects;
    subjects.reserve(checked.generic_template_signatures.size());
    for (const sema::GenericTemplateSignatureInfo& info : checked.generic_template_signatures) {
        push_generic_template_signature_query_subject(subjects, info, packages);
    }
    return subjects;
}

[[nodiscard]] std::vector<GenericInstanceSignatureQuerySubject> collect_generic_instance_signature_query_subjects(
    const sema::CheckedModule& checked)
{
    std::vector<GenericInstanceSignatureQuerySubject> subjects;
    const base::usize generic_instance_capacity = checked.functions.size() + checked.generic_function_instances.size()
        + checked.structs.size() + checked.generic_enum_instances.size() + checked.generic_type_alias_instances.size();
    subjects.reserve(generic_instance_capacity);
    std::unordered_set<query::GenericInstanceKey, query::GenericInstanceKeyHash> seen;
    seen.reserve(generic_instance_capacity);

    for (const auto& entry : checked.functions) {
        const sema::FunctionSignature& signature = entry.second;
        push_unique_generic_instance_signature_query_subject(
            subjects, seen, signature.generic_instance_key, signature.incremental_key);
    }
    for (const sema::GenericFunctionInstanceInfo& instance : checked.generic_function_instances) {
        push_unique_generic_instance_signature_query_subject(subjects, seen,
            query::is_valid(instance.signature.generic_instance_key) ? instance.signature.generic_instance_key
                                                                     : instance.generic_instance_key,
            instance.signature.incremental_key);
    }
    for (const auto& entry : checked.structs) {
        const sema::StructInfo& info = entry.second;
        push_unique_generic_instance_signature_query_subject(
            subjects, seen, info.generic_instance_key, info.incremental_key);
    }
    for (const sema::GenericEnumInstanceInfo& instance : checked.generic_enum_instances) {
        push_unique_generic_instance_signature_query_subject(
            subjects, seen, instance.generic_instance_key, instance.incremental_key);
    }
    for (const sema::GenericTypeAliasInstanceInfo& instance : checked.generic_type_alias_instances) {
        push_unique_generic_instance_signature_query_subject(
            subjects, seen, instance.generic_instance_key, instance.incremental_key);
    }
    return subjects;
}

[[nodiscard]] std::vector<GenericInstanceBodyQuerySubject> collect_generic_instance_body_query_subjects(
    const sema::CheckedModule& checked, const base::SourceManager& sources)
{
    std::vector<GenericInstanceBodyQuerySubject> subjects;
    subjects.reserve(checked.generic_function_instances.size());
    for (const sema::GenericFunctionInstanceInfo& instance : checked.generic_function_instances) {
        push_generic_instance_body_query_subject(subjects, instance, sources);
    }
    return subjects;
}

void collect_function_body_query_subjects(const sema::CheckedModule& checked, const base::SourceManager& sources,
    const syntax::AstModule* const ast, const std::span<const ModuleRecord> modules,
    std::vector<FunctionBodySyntaxQuerySubject>& syntax_subjects,
    std::vector<TypeCheckBodyQuerySubject>& type_check_subjects)
{
    const PackageIndex packages = package_index_for_modules(modules);
    syntax_subjects.reserve(checked.functions.size());
    type_check_subjects.reserve(checked.functions.size());
    for (const auto& entry : checked.functions) {
        push_function_body_query_subjects(syntax_subjects, type_check_subjects, entry.second, sources, ast, packages);
    }
}

[[nodiscard]] std::vector<LowerFunctionIRQuerySubject> collect_lower_function_ir_query_subjects(
    const std::vector<TypeCheckBodyQuerySubject>& type_check_subjects,
    const std::vector<GenericInstanceBodyQuerySubject>& generic_body_subjects)
{
    std::vector<LowerFunctionIRQuerySubject> subjects;
    subjects.reserve(type_check_subjects.size() + generic_body_subjects.size());
    for (const TypeCheckBodyQuerySubject& type_check_subject : type_check_subjects) {
        push_lower_function_ir_query_subject(subjects, type_check_subject);
    }
    for (const GenericInstanceBodyQuerySubject& generic_body_subject : generic_body_subjects) {
        push_lower_generic_instance_ir_query_subject(subjects, generic_body_subject);
    }
    return subjects;
}

[[nodiscard]] std::vector<DefinitionRecord> collect_definitions(const sema::CheckedModule& checked)
{
    std::vector<DefinitionRecord> records;
    records.reserve(checked.functions.size() + checked.generic_template_signatures.size()
        + checked.generic_function_instances.size() + checked.structs.size() + checked.enum_cases.size()
        + checked.type_aliases.size());

    for (const sema::GenericTemplateSignatureInfo& info : checked.generic_template_signatures) {
        push_definition(records, INCREMENTAL_CACHE_CATEGORY_GENERIC_TEMPLATE, info.name.view(), info.stable_id,
            info.incremental_key);
    }
    for (const auto& entry : checked.functions) {
        const sema::FunctionSignature& signature = entry.second;
        push_definition(records, INCREMENTAL_CACHE_CATEGORY_FUNCTION, signature.name.view(), signature.stable_id,
            signature.incremental_key);
    }
    for (const sema::GenericFunctionInstanceInfo& instance : checked.generic_function_instances) {
        push_definition(records, INCREMENTAL_CACHE_CATEGORY_GENERIC_FUNCTION_INSTANCE, instance.signature.name.view(),
            instance.signature.stable_id, instance.signature.incremental_key);
    }
    for (const auto& entry : checked.structs) {
        const sema::StructInfo& info = entry.second;
        push_definition(
            records, INCREMENTAL_CACHE_CATEGORY_STRUCT, info.name.view(), info.stable_id, info.incremental_key);
    }
    for (const auto& entry : checked.enum_cases) {
        const sema::EnumCaseInfo& info = entry.second;
        push_definition(
            records, INCREMENTAL_CACHE_CATEGORY_ENUM_CASE, info.name.view(), info.stable_id, info.incremental_key);
    }
    for (const auto& entry : checked.type_aliases) {
        const sema::TypeAliasInfo& info = entry.second;
        push_definition(
            records, INCREMENTAL_CACHE_CATEGORY_TYPE_ALIAS, info.name.view(), info.stable_id, info.incremental_key);
    }

    std::sort(records.begin(), records.end(), [](const DefinitionRecord& lhs, const DefinitionRecord& rhs) {
        if (lhs.category != rhs.category) {
            return lhs.category < rhs.category;
        }
        if (lhs.name != rhs.name) {
            return lhs.name < rhs.name;
        }
        if (lhs.stable_id.global_id != rhs.stable_id.global_id) {
            return lhs.stable_id.global_id < rhs.stable_id.global_id;
        }
        return lhs.incremental_key.global_id < rhs.incremental_key.global_id;
    });
    return records;
}

} // namespace aurex::driver::incremental_cache_detail
