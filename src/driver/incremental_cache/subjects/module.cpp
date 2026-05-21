#include "detail.hpp"

#include <algorithm>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace aurex::driver::incremental_cache_detail {
namespace cache_format = incremental_cache_format;
using namespace cache_format;

namespace {

[[nodiscard]] bool module_exports_signature_entry_less(
    const ModuleExportsSignatureEntry& lhs, const ModuleExportsSignatureEntry& rhs)
{
    return std::tie(lhs.category, lhs.name, lhs.stable_id.global_id, lhs.stable_id.name.primary,
               lhs.stable_id.name.secondary, lhs.stable_id.name.byte_count, lhs.stable_id.disambiguator,
               lhs.stable_id.kind, lhs.incremental_key.global_id, lhs.incremental_key.fingerprint.primary,
               lhs.incremental_key.fingerprint.secondary, lhs.incremental_key.fingerprint.byte_count)
        < std::tie(rhs.category, rhs.name, rhs.stable_id.global_id, rhs.stable_id.name.primary,
            rhs.stable_id.name.secondary, rhs.stable_id.name.byte_count, rhs.stable_id.disambiguator,
            rhs.stable_id.kind, rhs.incremental_key.global_id, rhs.incremental_key.fingerprint.primary,
            rhs.incremental_key.fingerprint.secondary, rhs.incremental_key.fingerprint.byte_count);
}

[[nodiscard]] bool item_list_signature_entry_less(const ItemListSignatureEntry& lhs, const ItemListSignatureEntry& rhs)
{
    return std::tie(lhs.category, lhs.name, lhs.stable_id.global_id, lhs.stable_id.name.primary,
               lhs.stable_id.name.secondary, lhs.stable_id.name.byte_count, lhs.stable_id.disambiguator,
               lhs.stable_id.kind, lhs.incremental_key.global_id, lhs.incremental_key.fingerprint.primary,
               lhs.incremental_key.fingerprint.secondary, lhs.incremental_key.fingerprint.byte_count)
        < std::tie(rhs.category, rhs.name, rhs.stable_id.global_id, rhs.stable_id.name.primary,
            rhs.stable_id.name.secondary, rhs.stable_id.name.byte_count, rhs.stable_id.disambiguator,
            rhs.stable_id.kind, rhs.incremental_key.global_id, rhs.incremental_key.fingerprint.primary,
            rhs.incremental_key.fingerprint.secondary, rhs.incremental_key.fingerprint.byte_count);
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

[[nodiscard]] query::QueryResultFingerprint module_graph_result_fingerprint(
    const query::ModuleKey key, const std::span<const ModuleRecord> modules)
{
    query::StableHashBuilder builder;
    builder.mix_string(INCREMENTAL_CACHE_MODULE_GRAPH_RESULT_MARKER);
    builder.mix_fingerprint(query::stable_key_fingerprint(key));
    builder.mix_u64(static_cast<base::u64>(modules.size()));
    for (base::usize index = 0; index < modules.size(); ++index) {
        const ModuleRecord& module = modules[index];
        builder.mix_u64(static_cast<base::u64>(index));
        builder.mix_string(module.name);
        builder.mix_string(module.path.string());
    }
    return query::query_result_fingerprint(builder.finish());
}

void push_module_exports_signature_entry(std::vector<ModuleExportsSignatureEntry>& entries,
    const std::string_view category, const std::string_view name, const sema::StableDefId& stable_id,
    const sema::IncrementalKey& incremental_key, const syntax::Visibility visibility,
    const sema::StableModuleId& module)
{
    if (visibility != syntax::Visibility::public_ || stable_id.module != module || !query::is_valid(stable_id)
        || !query::is_valid(incremental_key)) {
        return;
    }
    entries.push_back(ModuleExportsSignatureEntry{
        std::string(category),
        std::string(name),
        stable_id,
        incremental_key,
    });
}

void push_item_list_signature_entry(std::vector<ItemListSignatureEntry>& entries, const std::string_view category,
    const std::string_view name, const sema::StableDefId& stable_id, const sema::IncrementalKey& incremental_key,
    const sema::StableModuleId& module)
{
    if (stable_id.module != module || !query::is_valid(stable_id) || !query::is_valid(incremental_key)) {
        return;
    }
    entries.push_back(ItemListSignatureEntry{
        std::string(category),
        std::string(name),
        stable_id,
        incremental_key,
    });
}

[[nodiscard]] std::vector<ModuleExportsSignatureEntry> collect_module_exports_signature_entries(
    const sema::CheckedModule& checked, const sema::StableModuleId& module)
{
    std::vector<ModuleExportsSignatureEntry> entries;
    entries.reserve(
        checked.functions.size() + checked.structs.size() + checked.enum_cases.size() + checked.type_aliases.size());

    for (const auto& entry : checked.functions) {
        const sema::FunctionSignature& signature = entry.second;
        push_module_exports_signature_entry(entries, INCREMENTAL_CACHE_CATEGORY_FUNCTION, signature.name.view(),
            signature.stable_id, signature.incremental_key, signature.visibility, module);
    }
    for (const auto& entry : checked.structs) {
        const sema::StructInfo& info = entry.second;
        push_module_exports_signature_entry(entries, INCREMENTAL_CACHE_CATEGORY_STRUCT, info.name.view(),
            info.stable_id, info.incremental_key, info.visibility, module);
    }
    for (const auto& entry : checked.enum_cases) {
        const sema::EnumCaseInfo& info = entry.second;
        push_module_exports_signature_entry(entries, INCREMENTAL_CACHE_CATEGORY_ENUM_CASE, info.name.view(),
            info.stable_id, info.incremental_key, info.visibility, module);
    }
    for (const auto& entry : checked.type_aliases) {
        const sema::TypeAliasInfo& info = entry.second;
        push_module_exports_signature_entry(entries, INCREMENTAL_CACHE_CATEGORY_TYPE_ALIAS, info.name.view(),
            info.stable_id, info.incremental_key, info.visibility, module);
    }

    std::sort(entries.begin(), entries.end(), module_exports_signature_entry_less);
    return entries;
}

[[nodiscard]] std::vector<ItemListSignatureEntry> collect_item_list_signature_entries(
    const sema::CheckedModule& checked, const sema::StableModuleId& module)
{
    std::vector<ItemListSignatureEntry> entries;
    entries.reserve(checked.functions.size() + checked.generic_template_signatures.size() + checked.structs.size()
        + checked.enum_cases.size() + checked.type_aliases.size());

    for (const sema::GenericTemplateSignatureInfo& info : checked.generic_template_signatures) {
        push_item_list_signature_entry(entries, INCREMENTAL_CACHE_CATEGORY_GENERIC_TEMPLATE, info.name.view(),
            info.stable_id, info.incremental_key, module);
    }
    for (const auto& entry : checked.functions) {
        const sema::FunctionSignature& signature = entry.second;
        push_item_list_signature_entry(entries, INCREMENTAL_CACHE_CATEGORY_FUNCTION, signature.name.view(),
            signature.stable_id, signature.incremental_key, module);
    }
    for (const auto& entry : checked.structs) {
        const sema::StructInfo& info = entry.second;
        push_item_list_signature_entry(
            entries, INCREMENTAL_CACHE_CATEGORY_STRUCT, info.name.view(), info.stable_id, info.incremental_key, module);
    }
    for (const auto& entry : checked.enum_cases) {
        const sema::EnumCaseInfo& info = entry.second;
        push_item_list_signature_entry(entries, INCREMENTAL_CACHE_CATEGORY_ENUM_CASE, info.name.view(), info.stable_id,
            info.incremental_key, module);
    }
    for (const auto& entry : checked.type_aliases) {
        const sema::TypeAliasInfo& info = entry.second;
        push_item_list_signature_entry(entries, INCREMENTAL_CACHE_CATEGORY_TYPE_ALIAS, info.name.view(), info.stable_id,
            info.incremental_key, module);
    }

    std::sort(entries.begin(), entries.end(), item_list_signature_entry_less);
    return entries;
}

[[nodiscard]] query::QueryResultFingerprint module_exports_result_fingerprint(
    const query::ModuleKey key, const std::vector<ModuleExportsSignatureEntry>& entries)
{
    query::StableHashBuilder builder;
    builder.mix_string(INCREMENTAL_CACHE_MODULE_EXPORTS_RESULT_MARKER);
    builder.mix_fingerprint(query::stable_key_fingerprint(key));
    builder.mix_u64(static_cast<base::u64>(entries.size()));
    for (base::usize index = 0; index < entries.size(); ++index) {
        const ModuleExportsSignatureEntry& entry = entries[index];
        builder.mix_u64(static_cast<base::u64>(index));
        builder.mix_string(entry.category);
        builder.mix_string(entry.name);
        builder.mix_fingerprint(query::stable_key_fingerprint(entry.stable_id));
        builder.mix_u64(entry.incremental_key.global_id);
        builder.mix_fingerprint(entry.incremental_key.fingerprint);
    }
    return query::query_result_fingerprint(builder.finish());
}

[[nodiscard]] query::QueryResultFingerprint item_list_result_fingerprint(
    const query::ModuleKey key, const std::vector<ItemListSignatureEntry>& entries)
{
    query::StableHashBuilder builder;
    builder.mix_string(INCREMENTAL_CACHE_ITEM_LIST_RESULT_MARKER);
    builder.mix_fingerprint(query::stable_key_fingerprint(key));
    builder.mix_u64(static_cast<base::u64>(entries.size()));
    for (base::usize index = 0; index < entries.size(); ++index) {
        const ItemListSignatureEntry& entry = entries[index];
        builder.mix_u64(static_cast<base::u64>(index));
        builder.mix_string(entry.category);
        builder.mix_string(entry.name);
        builder.mix_fingerprint(query::stable_key_fingerprint(entry.stable_id));
        builder.mix_u64(entry.incremental_key.global_id);
        builder.mix_fingerprint(entry.incremental_key.fingerprint);
    }
    return query::query_result_fingerprint(builder.finish());
}

void push_module_graph_query_subject(std::vector<ModuleGraphQuerySubject>& subjects, const ModuleRecord& module,
    const std::span<const ModuleRecord> modules)
{
    const sema::StableModuleId stable_module = stable_module_id_from_record(module);
    const query::ModuleKey key = query::module_key_from_stable_id(stable_module);
    if (!query::is_valid(stable_module) || !query::is_valid(key)) {
        return;
    }
    subjects.push_back(ModuleGraphQuerySubject{
        key,
        module_graph_result_fingerprint(key, modules),
    });
}

void push_module_exports_query_subject(
    std::vector<ModuleExportsQuerySubject>& subjects, const ModuleRecord& module, const sema::CheckedModule& checked)
{
    const sema::StableModuleId stable_module = stable_module_id_from_record(module);
    const query::ModuleKey key = query::module_key_from_stable_id(stable_module);
    if (!query::is_valid(stable_module) || !query::is_valid(key)) {
        return;
    }

    const std::vector<ModuleExportsSignatureEntry> entries =
        collect_module_exports_signature_entries(checked, stable_module);
    subjects.push_back(ModuleExportsQuerySubject{
        key,
        module_exports_result_fingerprint(key, entries),
    });
}

void push_item_list_query_subject(
    std::vector<ItemListQuerySubject>& subjects, const ModuleRecord& module, const sema::CheckedModule& checked)
{
    const sema::StableModuleId stable_module = stable_module_id_from_record(module);
    const query::ModuleKey key = query::module_key_from_stable_id(stable_module);
    if (!query::is_valid(stable_module) || !query::is_valid(key)) {
        return;
    }

    const std::vector<ItemListSignatureEntry> entries = collect_item_list_signature_entries(checked, stable_module);
    subjects.push_back(ItemListQuerySubject{
        key,
        item_list_result_fingerprint(key, entries),
    });
}

} // namespace

[[nodiscard]] std::vector<ModuleGraphQuerySubject> collect_module_graph_query_subjects(
    const std::span<const ModuleRecord> modules)
{
    std::vector<ModuleGraphQuerySubject> subjects;
    subjects.reserve(modules.size());
    for (const ModuleRecord& module : modules) {
        push_module_graph_query_subject(subjects, module, modules);
    }
    return subjects;
}

[[nodiscard]] std::vector<ModuleExportsQuerySubject> collect_module_exports_query_subjects(
    const std::span<const ModuleRecord> modules, const sema::CheckedModule& checked)
{
    std::vector<ModuleExportsQuerySubject> subjects;
    subjects.reserve(modules.size());
    for (const ModuleRecord& module : modules) {
        push_module_exports_query_subject(subjects, module, checked);
    }
    return subjects;
}

[[nodiscard]] std::vector<ItemListQuerySubject> collect_item_list_query_subjects(
    const std::span<const ModuleRecord> modules, const sema::CheckedModule& checked)
{
    std::vector<ItemListQuerySubject> subjects;
    subjects.reserve(modules.size());
    for (const ModuleRecord& module : modules) {
        push_item_list_query_subject(subjects, module, checked);
    }
    return subjects;
}

} // namespace aurex::driver::incremental_cache_detail
