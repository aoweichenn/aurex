#include <algorithm>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "detail.hpp"

namespace aurex::driver::incremental_cache_detail {
namespace cache_format = incremental_cache_format;
using namespace cache_format;

namespace {

constexpr std::string_view INCREMENTAL_CACHE_CATEGORY_CONST = "const";
constexpr std::string_view INCREMENTAL_CACHE_CATEGORY_REEXPORT = "reexport";
constexpr std::string_view INCREMENTAL_CACHE_CATEGORY_STRUCT_FIELD = "struct_field";
constexpr std::string_view INCREMENTAL_CACHE_EXPORT_EXTERN_C_TAG = "extern_c";
constexpr std::string_view INCREMENTAL_CACHE_EXPORT_AUREX_TAG = "aurex";
constexpr std::string_view INCREMENTAL_CACHE_EXPORT_VARIADIC_TAG = "variadic";
constexpr std::string_view INCREMENTAL_CACHE_EXPORT_FIXED_TAG = "fixed";
constexpr std::string_view INCREMENTAL_CACHE_GRAPH_PRIMARY_PART_TAG = "primary";
constexpr std::string_view INCREMENTAL_CACHE_GRAPH_NAMED_PART_TAG = "named";
constexpr std::string_view INCREMENTAL_CACHE_GRAPH_PART_IDENTITY_TAG = "module-part:v1";
constexpr std::string_view INCREMENTAL_CACHE_GRAPH_PRIMARY_PART_KEY_NAME = "<primary>";
constexpr std::string_view INCREMENTAL_CACHE_GRAPH_SOURCE_ROOT_TOPOLOGY_TAG = "source-root";
constexpr std::string_view INCREMENTAL_CACHE_INVALID_TYPE_TAG = "<invalid>";
constexpr base::usize INCREMENTAL_CACHE_EXPORT_PARAM_TEXT_BUDGET = 16;
constexpr base::usize INCREMENTAL_CACHE_EXPORT_BASE_TEXT_BUDGET = 64;

[[nodiscard]] bool module_exports_signature_entry_less(
    const ModuleExportsSignatureEntry& lhs, const ModuleExportsSignatureEntry& rhs)
{
    return std::tie(lhs.category, lhs.name, lhs.visibility_rank, lhs.identity, lhs.signature)
        < std::tie(rhs.category, rhs.name, rhs.visibility_rank, rhs.identity, rhs.signature);
}

[[nodiscard]] bool item_list_signature_entry_less(const ItemListSignatureEntry& lhs, const ItemListSignatureEntry& rhs)
{
    return std::tie(lhs.category, lhs.name, lhs.identity) < std::tie(rhs.category, rhs.name, rhs.identity);
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

[[nodiscard]] query::PackageKey module_package_or_default(const query::PackageKey package) noexcept
{
    if (query::is_valid(package)) {
        return package;
    }
    return query::package_key(std::span<const std::string_view>{});
}

[[nodiscard]] query::ModuleKey module_key_from_record(const ModuleRecord& module)
{
    return query::module_key_from_stable_id(
        module_package_or_default(module.package), stable_module_id_from_record(module));
}

[[nodiscard]] query::ModulePartKind module_part_key_kind(const ModulePartRecordKind kind) noexcept
{
    switch (kind) {
        case ModulePartRecordKind::primary:
            return query::ModulePartKind::primary;
        case ModulePartRecordKind::named:
            return query::ModulePartKind::fragment;
    }
    return query::ModulePartKind::fragment;
}

[[nodiscard]] std::string_view module_part_key_name(const ModulePartRecord& part) noexcept
{
    return part.kind == ModulePartRecordKind::primary ? INCREMENTAL_CACHE_GRAPH_PRIMARY_PART_KEY_NAME : part.name;
}

[[nodiscard]] query::ModulePartKey module_part_key_from_record(
    const query::ModuleKey module_key, const ModuleRecord& module, const ModulePartRecord& part)
{
    if (query::is_valid(part.key)) {
        return part.key;
    }
    const query::FileKey file_key =
        query::file_key(module_package_or_default(module.package), part.path.string(), query::SourceRole::source);
    return query::module_part_key(
        module_key, file_key, module_part_key_kind(part.kind), module_part_key_name(part), part.stable_index);
}

[[nodiscard]] query::StableFingerprint128 module_part_graph_identity_fingerprint(const query::ModulePartKey key)
{
    query::StableHashBuilder builder;
    builder.mix_string(INCREMENTAL_CACHE_GRAPH_PART_IDENTITY_TAG);
    builder.mix_fingerprint(query::stable_key_fingerprint(key.module));
    builder.mix_fingerprint(query::stable_key_fingerprint(key.file));
    builder.mix_fingerprint(key.name);
    builder.mix_u64(static_cast<base::u64>(key.kind));
    return builder.finish();
}

[[nodiscard]] query::ModuleKey module_key_from_import(const ModuleImportRecord& import)
{
    const std::vector<std::string_view> parts = module_name_parts(import.module_name);
    return query::module_key_from_stable_id(module_package_or_default(import.module_package),
        sema::stable_module_id(std::span<const std::string_view>{parts.data(), parts.size()}));
}

[[nodiscard]] std::string stable_identity_string(const sema::StableDefId& stable_id)
{
    return query::is_valid(stable_id) ? query::stable_serialize(stable_id) : std::string{};
}

[[nodiscard]] std::string stable_identity_string(const sema::StableMemberKey& stable_key)
{
    return query::is_valid(stable_key) ? query::stable_serialize(stable_key) : std::string{};
}

[[nodiscard]] std::string type_display_name(const sema::CheckedModule& checked, const sema::TypeHandle type)
{
    return sema::is_valid(type) ? checked.types.display_name(type) : std::string(INCREMENTAL_CACHE_INVALID_TYPE_TAG);
}

[[nodiscard]] std::string checked_syntax_type_display_name(
    const sema::CheckedModule& checked, const syntax::TypeId type)
{
    if (!syntax::is_valid(type) || type.value >= checked.syntax_type_handles.size()) {
        return std::string(INCREMENTAL_CACHE_INVALID_TYPE_TAG);
    }
    return type_display_name(checked, checked.syntax_type_handles[type.value]);
}

[[nodiscard]] std::string module_part_kind_name(const ModulePartRecordKind kind)
{
    switch (kind) {
        case ModulePartRecordKind::primary:
            return std::string(INCREMENTAL_CACHE_GRAPH_PRIMARY_PART_TAG);
        case ModulePartRecordKind::named:
            return std::string(INCREMENTAL_CACHE_GRAPH_NAMED_PART_TAG);
    }
    return {};
}

[[nodiscard]] bool module_key_less(const query::ModuleKey lhs, const query::ModuleKey rhs)
{
    return query::stable_serialize(lhs) < query::stable_serialize(rhs);
}

[[nodiscard]] bool package_key_less(const query::PackageKey lhs, const query::PackageKey rhs) noexcept
{
    return std::tie(lhs.global_id, lhs.identity.primary, lhs.identity.secondary, lhs.identity.byte_count)
        < std::tie(rhs.global_id, rhs.identity.primary, rhs.identity.secondary, rhs.identity.byte_count);
}

[[nodiscard]] std::vector<query::ModuleKey> primary_public_reexport_dependencies(const ModuleRecord& module)
{
    std::vector<query::ModuleKey> dependencies;
    dependencies.reserve(module.imports.size());
    for (const ModuleImportRecord& import : module.imports) {
        if (!import.owner_is_primary || !syntax::visibility_is_public(import.visibility)) {
            continue;
        }
        const query::ModuleKey key = module_key_from_import(import);
        if (query::is_valid(key)) {
            dependencies.push_back(key);
        }
    }
    std::sort(dependencies.begin(), dependencies.end(), module_key_less);
    dependencies.erase(std::unique(dependencies.begin(), dependencies.end()), dependencies.end());
    return dependencies;
}

struct ModulePackageReexportDependencies {
    std::vector<query::ModuleKey> public_surface;
    std::vector<query::ModuleKey> package_surface;
};

[[nodiscard]] std::string module_key_identity(const query::ModuleKey key)
{
    return query::is_valid(key) ? query::stable_serialize(key) : std::string{};
}

[[nodiscard]] std::unordered_map<std::string, base::usize> module_key_index(const std::span<const ModuleRecord> modules)
{
    std::unordered_map<std::string, base::usize> index;
    index.reserve(modules.size());
    for (base::usize module_index = 0; module_index < modules.size(); ++module_index) {
        const query::ModuleKey key = module_key_from_record(modules[module_index]);
        if (query::is_valid(key)) {
            index.emplace(module_key_identity(key), module_index);
        }
    }
    return index;
}

void sort_unique_module_keys(std::vector<query::ModuleKey>& modules)
{
    std::sort(modules.begin(), modules.end(), module_key_less);
    modules.erase(std::unique(modules.begin(), modules.end()), modules.end());
}

[[nodiscard]] query::QueryResultFingerprint module_graph_result_fingerprint(
    const query::ModuleKey key, const ModuleRecord& module)
{
    query::StableHashBuilder builder;
    builder.mix_string(INCREMENTAL_CACHE_MODULE_GRAPH_RESULT_MARKER);
    builder.mix_fingerprint(query::stable_key_fingerprint(key));
    builder.mix_string(module.name);
    builder.mix_string(module.path.string());
    if (module.source_root_topology.has_value()) {
        builder.mix_string(INCREMENTAL_CACHE_GRAPH_SOURCE_ROOT_TOPOLOGY_TAG);
        builder.mix_string(module.source_root_topology->source_root.string());
        builder.mix_string(module.source_root_topology->source_relative_path.string());
    }

    std::vector<const ModulePartRecord*> parts;
    parts.reserve(module.parts.size());
    for (const ModulePartRecord& part : module.parts) {
        parts.push_back(&part);
    }
    std::sort(parts.begin(), parts.end(), [](const ModulePartRecord* lhs, const ModulePartRecord* rhs) {
        return std::tie(lhs->kind, lhs->name, lhs->path) < std::tie(rhs->kind, rhs->name, rhs->path);
    });
    builder.mix_u64(static_cast<base::u64>(parts.size()));
    for (base::usize index = 0; index < parts.size(); ++index) {
        const ModulePartRecord& part = *parts[index];
        const query::ModulePartKey part_key = module_part_key_from_record(key, module, part);
        builder.mix_u64(static_cast<base::u64>(index));
        builder.mix_fingerprint(module_part_graph_identity_fingerprint(part_key));
        builder.mix_string(module_part_kind_name(part.kind));
        builder.mix_string(part.name);
        builder.mix_string(part.path.string());
    }

    std::vector<const ModuleImportRecord*> imports;
    imports.reserve(module.imports.size());
    for (const ModuleImportRecord& import : module.imports) {
        imports.push_back(&import);
    }
    std::sort(imports.begin(), imports.end(), [](const ModuleImportRecord* lhs, const ModuleImportRecord* rhs) {
        const auto lhs_key = std::tie(lhs->owner_is_primary, lhs->owner_part, lhs->module_name, lhs->alias);
        const auto rhs_key = std::tie(rhs->owner_is_primary, rhs->owner_part, rhs->module_name, rhs->alias);
        if (lhs_key != rhs_key) {
            return lhs_key < rhs_key;
        }
        const query::PackageKey lhs_package = module_package_or_default(lhs->module_package);
        const query::PackageKey rhs_package = module_package_or_default(rhs->module_package);
        if (lhs_package != rhs_package) {
            return package_key_less(lhs_package, rhs_package);
        }
        return syntax::visibility_rank(lhs->visibility) < syntax::visibility_rank(rhs->visibility);
    });
    builder.mix_u64(static_cast<base::u64>(imports.size()));
    for (base::usize index = 0; index < imports.size(); ++index) {
        const ModuleImportRecord& import = *imports[index];
        builder.mix_u64(static_cast<base::u64>(index));
        builder.mix_bool(import.owner_is_primary);
        builder.mix_string(import.owner_part);
        builder.mix_string(import.module_name);
        builder.mix_fingerprint(query::stable_key_fingerprint(module_package_or_default(import.module_package)));
        builder.mix_string(import.alias);
        builder.mix_u64(syntax::visibility_rank(import.visibility));
    }
    return query::query_result_fingerprint(builder.finish());
}

void push_module_exports_signature_entry(std::vector<ModuleExportsSignatureEntry>& entries,
    const std::string_view category, const std::string_view name, const syntax::Visibility visibility,
    std::string identity, std::string signature)
{
    if (identity.empty()) {
        return;
    }
    entries.push_back(ModuleExportsSignatureEntry{
        std::string(category),
        std::string(name),
        syntax::visibility_rank(visibility),
        std::move(identity),
        std::move(signature),
    });
}

void push_item_list_signature_entry(std::vector<ItemListSignatureEntry>& entries, const std::string_view category,
    const std::string_view name, std::string identity)
{
    if (identity.empty()) {
        return;
    }
    entries.push_back(ItemListSignatureEntry{
        std::string(category),
        std::string(name),
        std::move(identity),
    });
}

[[nodiscard]] bool stable_id_belongs_to_module(
    const sema::StableDefId& stable_id, const sema::StableModuleId& module) noexcept
{
    return query::is_valid(stable_id) && stable_id.module == module;
}

[[nodiscard]] syntax::Visibility function_signature_surface_visibility(
    const sema::CheckedModule& checked, const sema::FunctionSignature& signature)
{
    if (!signature.is_method) {
        return signature.visibility;
    }
    for (const auto& entry : checked.structs) {
        const sema::StructInfo& info = entry.second;
        if (info.type.value == signature.method_owner_type.value) {
            return syntax::effective_visibility(info.visibility, signature.visibility);
        }
    }
    for (const auto& entry : checked.enum_cases) {
        const sema::EnumCaseInfo& info = entry.second;
        if (info.type.value == signature.method_owner_type.value) {
            return syntax::effective_visibility(info.visibility, signature.visibility);
        }
    }
    return signature.visibility;
}

[[nodiscard]] std::string function_export_name(
    const sema::CheckedModule& checked, const sema::FunctionSignature& signature)
{
    if (signature.is_method) {
        return checked.types.display_name(signature.method_owner_type) + "."
            + sema::function_display_name(checked.types, signature);
    }
    return std::string(signature.name.view());
}

[[nodiscard]] std::string function_export_signature(
    const sema::CheckedModule& checked, const sema::FunctionSignature& signature)
{
    std::string value;
    value.reserve(signature.param_types.size() * INCREMENTAL_CACHE_EXPORT_PARAM_TEXT_BUDGET
        + INCREMENTAL_CACHE_EXPORT_BASE_TEXT_BUDGET);
    value += signature.is_extern_c ? INCREMENTAL_CACHE_EXPORT_EXTERN_C_TAG : INCREMENTAL_CACHE_EXPORT_AUREX_TAG;
    value.push_back('|');
    value += signature.is_variadic ? INCREMENTAL_CACHE_EXPORT_VARIADIC_TAG : INCREMENTAL_CACHE_EXPORT_FIXED_TAG;
    value.push_back('|');
    value += type_display_name(checked, signature.return_type);
    for (const sema::TypeHandle param : signature.param_types) {
        value.push_back('|');
        value += type_display_name(checked, param);
    }
    return value;
}

[[nodiscard]] std::string enum_case_export_signature(const sema::CheckedModule& checked, const sema::EnumCaseInfo& info)
{
    std::string value;
    value += type_display_name(checked, info.type);
    for (const sema::TypeHandle payload : info.payload_types) {
        value.push_back('|');
        value += type_display_name(checked, payload);
    }
    if (info.payload_types.empty()) {
        value.push_back('|');
        value += type_display_name(checked, info.payload_type);
    }
    return value;
}

[[nodiscard]] sema::StableDefId stable_const_id(
    const syntax::AstModule& ast, const syntax::ItemNode& item, const syntax::ModuleId module)
{
    if (!syntax::is_valid(module) || module.value >= ast.modules.size()) {
        return {};
    }
    return sema::stable_definition_id(
        sema::stable_module_id(ast.modules[module.value].path.parts), sema::StableSymbolKind::value, item.name);
}

void collect_const_item_list_entries(std::vector<ItemListSignatureEntry>& entries, const syntax::AstModule* const ast,
    const sema::StableModuleId& module)
{
    if (ast == nullptr) {
        return;
    }
    for (base::u32 index = 0; index < ast->items.size(); ++index) {
        if (ast->items.kind(index) != syntax::ItemKind::const_decl || index >= ast->item_modules.size()) {
            continue;
        }
        const syntax::ItemNode item = ast->items[index];
        const sema::StableDefId stable_id = stable_const_id(*ast, item, ast->item_modules[index]);
        if (!stable_id_belongs_to_module(stable_id, module)) {
            continue;
        }
        push_item_list_signature_entry(
            entries, INCREMENTAL_CACHE_CATEGORY_CONST, item.name, stable_identity_string(stable_id));
    }
}

void collect_const_module_export_entries(std::vector<ModuleExportsSignatureEntry>& entries,
    const sema::CheckedModule& checked, const syntax::AstModule* const ast, const sema::StableModuleId& module,
    const syntax::Visibility minimum_visibility)
{
    if (ast == nullptr) {
        return;
    }
    for (base::u32 index = 0; index < ast->items.size(); ++index) {
        if (ast->items.kind(index) != syntax::ItemKind::const_decl || index >= ast->item_modules.size()) {
            continue;
        }
        const syntax::ItemNode item = ast->items[index];
        const sema::StableDefId stable_id = stable_const_id(*ast, item, ast->item_modules[index]);
        if (!syntax::visibility_at_least(item.visibility, minimum_visibility)
            || !stable_id_belongs_to_module(stable_id, module)) {
            continue;
        }
        push_module_exports_signature_entry(entries, INCREMENTAL_CACHE_CATEGORY_CONST, item.name, item.visibility,
            stable_identity_string(stable_id), checked_syntax_type_display_name(checked, item.const_type));
    }
}

void collect_primary_reexport_entries(std::vector<ModuleExportsSignatureEntry>& entries, const ModuleRecord& module,
    const syntax::Visibility minimum_visibility)
{
    for (const ModuleImportRecord& import : module.imports) {
        if (!import.owner_is_primary || !syntax::visibility_at_least(import.visibility, minimum_visibility)) {
            continue;
        }
        const query::ModuleKey reexported = module_key_from_import(import);
        if (!query::is_valid(reexported)) {
            continue;
        }
        push_module_exports_signature_entry(entries, INCREMENTAL_CACHE_CATEGORY_REEXPORT, import.alias,
            import.visibility, query::stable_serialize(reexported), import.module_name);
    }
}

[[nodiscard]] std::vector<ModuleExportsSignatureEntry> collect_module_exports_signature_entries(
    const ModuleRecord& module_record, const sema::CheckedModule& checked, const syntax::AstModule* const ast,
    const sema::StableModuleId& module, const syntax::Visibility minimum_visibility)
{
    std::vector<ModuleExportsSignatureEntry> entries;
    entries.reserve(checked.functions.size() + checked.structs.size() + checked.enum_cases.size()
        + checked.type_aliases.size() + checked.generic_template_signatures.size() + module_record.imports.size());

    for (const auto& entry : checked.functions) {
        const sema::FunctionSignature& signature = entry.second;
        const syntax::Visibility surface_visibility = function_signature_surface_visibility(checked, signature);
        if (!syntax::visibility_at_least(surface_visibility, minimum_visibility) || signature.has_conflict
            || !stable_id_belongs_to_module(signature.stable_id, module)
            || !syntax::visibility_at_least(signature.visibility, minimum_visibility)) {
            continue;
        }
        push_module_exports_signature_entry(entries, INCREMENTAL_CACHE_CATEGORY_FUNCTION,
            function_export_name(checked, signature), surface_visibility, stable_identity_string(signature.stable_id),
            function_export_signature(checked, signature));
    }
    for (const auto& entry : checked.structs) {
        const sema::StructInfo& info = entry.second;
        if (!syntax::visibility_at_least(info.visibility, minimum_visibility)
            || !stable_id_belongs_to_module(info.stable_id, module)) {
            continue;
        }
        push_module_exports_signature_entry(entries, INCREMENTAL_CACHE_CATEGORY_STRUCT, info.name.view(),
            info.visibility, stable_identity_string(info.stable_id), info.is_opaque ? "opaque" : "struct");
        const std::string struct_name = sema::struct_display_name(checked.types, info);
        for (const sema::StructFieldInfo& field : info.fields) {
            const syntax::Visibility field_surface = syntax::effective_visibility(info.visibility, field.visibility);
            if (!syntax::visibility_at_least(field_surface, minimum_visibility)) {
                continue;
            }
            push_module_exports_signature_entry(entries, INCREMENTAL_CACHE_CATEGORY_STRUCT_FIELD,
                struct_name + "." + std::string(field.name.view()), field_surface,
                stable_identity_string(field.stable_key), type_display_name(checked, field.type));
        }
    }
    for (const auto& entry : checked.enum_cases) {
        const sema::EnumCaseInfo& info = entry.second;
        if (!syntax::visibility_at_least(info.visibility, minimum_visibility)
            || !stable_id_belongs_to_module(info.stable_id, module)) {
            continue;
        }
        push_module_exports_signature_entry(entries, INCREMENTAL_CACHE_CATEGORY_ENUM_CASE,
            sema::enum_case_display_name(checked.types, info), info.visibility, stable_identity_string(info.stable_id),
            enum_case_export_signature(checked, info));
    }
    for (const auto& entry : checked.type_aliases) {
        const sema::TypeAliasInfo& info = entry.second;
        if (!syntax::visibility_at_least(info.visibility, minimum_visibility)
            || !stable_id_belongs_to_module(info.stable_id, module)) {
            continue;
        }
        push_module_exports_signature_entry(entries, INCREMENTAL_CACHE_CATEGORY_TYPE_ALIAS, info.name.view(),
            info.visibility, stable_identity_string(info.stable_id),
            checked_syntax_type_display_name(checked, info.target));
    }
    for (const sema::GenericTemplateSignatureInfo& info : checked.generic_template_signatures) {
        if (!syntax::visibility_at_least(info.visibility, minimum_visibility)
            || !stable_id_belongs_to_module(info.stable_id, module) || !query::is_valid(info.incremental_key)) {
            continue;
        }
        push_module_exports_signature_entry(entries, INCREMENTAL_CACHE_CATEGORY_GENERIC_TEMPLATE, info.name.view(),
            info.visibility, stable_identity_string(info.stable_id), query::stable_serialize(info.incremental_key));
    }
    collect_const_module_export_entries(entries, checked, ast, module, minimum_visibility);
    collect_primary_reexport_entries(entries, module_record, minimum_visibility);

    std::sort(entries.begin(), entries.end(), module_exports_signature_entry_less);
    return entries;
}

[[nodiscard]] std::vector<ItemListSignatureEntry> collect_item_list_signature_entries(
    const sema::CheckedModule& checked, const syntax::AstModule* const ast, const sema::StableModuleId& module)
{
    std::vector<ItemListSignatureEntry> entries;
    entries.reserve(checked.functions.size() + checked.generic_template_signatures.size() + checked.structs.size()
        + checked.enum_cases.size() + checked.type_aliases.size());

    for (const sema::GenericTemplateSignatureInfo& info : checked.generic_template_signatures) {
        if (!stable_id_belongs_to_module(info.stable_id, module)) {
            continue;
        }
        push_item_list_signature_entry(entries, INCREMENTAL_CACHE_CATEGORY_GENERIC_TEMPLATE, info.name.view(),
            stable_identity_string(info.stable_id));
    }
    for (const auto& entry : checked.functions) {
        const sema::FunctionSignature& signature = entry.second;
        if (!stable_id_belongs_to_module(signature.stable_id, module)) {
            continue;
        }
        push_item_list_signature_entry(entries, INCREMENTAL_CACHE_CATEGORY_FUNCTION,
            function_export_name(checked, signature), stable_identity_string(signature.stable_id));
    }
    for (const auto& entry : checked.structs) {
        const sema::StructInfo& info = entry.second;
        if (!stable_id_belongs_to_module(info.stable_id, module)) {
            continue;
        }
        push_item_list_signature_entry(
            entries, INCREMENTAL_CACHE_CATEGORY_STRUCT, info.name.view(), stable_identity_string(info.stable_id));
    }
    for (const auto& entry : checked.enum_cases) {
        const sema::EnumCaseInfo& info = entry.second;
        if (!stable_id_belongs_to_module(info.stable_id, module)) {
            continue;
        }
        push_item_list_signature_entry(entries, INCREMENTAL_CACHE_CATEGORY_ENUM_CASE,
            sema::enum_case_display_name(checked.types, info), stable_identity_string(info.stable_id));
    }
    for (const auto& entry : checked.type_aliases) {
        const sema::TypeAliasInfo& info = entry.second;
        if (!stable_id_belongs_to_module(info.stable_id, module)) {
            continue;
        }
        push_item_list_signature_entry(
            entries, INCREMENTAL_CACHE_CATEGORY_TYPE_ALIAS, info.name.view(), stable_identity_string(info.stable_id));
    }
    collect_const_item_list_entries(entries, ast, module);

    std::sort(entries.begin(), entries.end(), item_list_signature_entry_less);
    return entries;
}

[[nodiscard]] query::QueryResultFingerprint module_exports_result_fingerprint(const query::ModuleKey key,
    const std::vector<ModuleExportsSignatureEntry>& entries,
    const std::string_view marker = INCREMENTAL_CACHE_MODULE_EXPORTS_RESULT_MARKER)
{
    query::StableHashBuilder builder;
    builder.mix_string(marker);
    builder.mix_fingerprint(query::stable_key_fingerprint(key));
    builder.mix_u64(static_cast<base::u64>(entries.size()));
    for (base::usize index = 0; index < entries.size(); ++index) {
        const ModuleExportsSignatureEntry& entry = entries[index];
        builder.mix_u64(static_cast<base::u64>(index));
        builder.mix_string(entry.category);
        builder.mix_string(entry.name);
        builder.mix_u64(entry.visibility_rank);
        builder.mix_string(entry.identity);
        builder.mix_string(entry.signature);
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
        builder.mix_string(entry.identity);
    }
    return query::query_result_fingerprint(builder.finish());
}

void push_module_graph_query_subject(std::vector<ModuleGraphQuerySubject>& subjects, const ModuleRecord& module)
{
    const sema::StableModuleId stable_module = stable_module_id_from_record(module);
    const query::ModuleKey key = module_key_from_record(module);
    if (!query::is_valid(stable_module) || !query::is_valid(key)) {
        return;
    }
    subjects.push_back(ModuleGraphQuerySubject{
        key,
        module_graph_result_fingerprint(key, module),
    });
}

void push_module_exports_query_subject(std::vector<ModuleExportsQuerySubject>& subjects, const ModuleRecord& module,
    const sema::CheckedModule& checked, const syntax::AstModule* const ast)
{
    const sema::StableModuleId stable_module = stable_module_id_from_record(module);
    const query::ModuleKey key = module_key_from_record(module);
    if (!query::is_valid(stable_module) || !query::is_valid(key)) {
        return;
    }

    const std::vector<ModuleExportsSignatureEntry> entries =
        collect_module_exports_signature_entries(module, checked, ast, stable_module, syntax::Visibility::public_);
    subjects.push_back(ModuleExportsQuerySubject{
        key,
        module_exports_result_fingerprint(key, entries),
        primary_public_reexport_dependencies(module),
    });
}

[[nodiscard]] bool module_has_direct_package_export_entries(
    const ModuleRecord& module, const sema::CheckedModule& checked, const syntax::AstModule* const ast)
{
    const sema::StableModuleId stable_module = stable_module_id_from_record(module);
    if (!query::is_valid(stable_module)) {
        return false;
    }
    const std::vector<ModuleExportsSignatureEntry> entries =
        collect_module_exports_signature_entries(module, checked, ast, stable_module, syntax::Visibility::package_);
    return std::any_of(entries.begin(), entries.end(), [](const ModuleExportsSignatureEntry& entry) {
        return entry.visibility_rank == syntax::VISIBILITY_RANK_PACKAGE;
    });
}

[[nodiscard]] bool import_can_propagate_package_exports(const ModuleImportRecord& import) noexcept
{
    return import.owner_is_primary && syntax::visibility_at_least(import.visibility, syntax::Visibility::package_);
}

struct ModulePackageExportPlan {
    std::vector<bool> enabled;
    std::unordered_set<std::string> enabled_keys;
};

[[nodiscard]] ModulePackageExportPlan module_package_export_plan(
    const std::span<const ModuleRecord> modules, const sema::CheckedModule& checked, const syntax::AstModule* const ast)
{
    const std::unordered_map<std::string, base::usize> index_by_key = module_key_index(modules);
    std::vector<bool> enabled(modules.size(), false);
    for (base::usize index = 0; index < modules.size(); ++index) {
        enabled[index] = module_has_direct_package_export_entries(modules[index], checked, ast);
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (base::usize index = 0; index < modules.size(); ++index) {
            if (enabled[index]) {
                continue;
            }
            const query::ModuleKey module_key = module_key_from_record(modules[index]);
            for (const ModuleImportRecord& import : modules[index].imports) {
                if (!import_can_propagate_package_exports(import)) {
                    continue;
                }
                const query::ModuleKey target = module_key_from_import(import);
                const auto target_index = index_by_key.find(module_key_identity(target));
                if (target_index == index_by_key.end() || target.package != module_key.package
                    || !enabled[target_index->second]) {
                    continue;
                }
                enabled[index] = true;
                changed = true;
                break;
            }
        }
    }

    std::unordered_set<std::string> enabled_keys;
    enabled_keys.reserve(modules.size());
    for (base::usize index = 0; index < modules.size(); ++index) {
        if (!enabled[index]) {
            continue;
        }
        const query::ModuleKey key = module_key_from_record(modules[index]);
        if (query::is_valid(key)) {
            enabled_keys.insert(module_key_identity(key));
        }
    }
    return ModulePackageExportPlan{
        std::move(enabled),
        std::move(enabled_keys),
    };
}

[[nodiscard]] ModulePackageReexportDependencies primary_package_reexport_dependencies(
    const ModuleRecord& module, const std::unordered_set<std::string>& package_export_keys)
{
    ModulePackageReexportDependencies dependencies;
    dependencies.public_surface.reserve(module.imports.size());
    dependencies.package_surface.reserve(module.imports.size());

    const query::ModuleKey module_key = module_key_from_record(module);
    for (const ModuleImportRecord& import : module.imports) {
        if (!import_can_propagate_package_exports(import)) {
            continue;
        }
        const query::ModuleKey target = module_key_from_import(import);
        if (!query::is_valid(target)) {
            continue;
        }
        if (target.package == module_key.package && package_export_keys.contains(module_key_identity(target))) {
            dependencies.package_surface.push_back(target);
            continue;
        }
        dependencies.public_surface.push_back(target);
    }
    sort_unique_module_keys(dependencies.public_surface);
    sort_unique_module_keys(dependencies.package_surface);
    return dependencies;
}

void push_module_package_exports_query_subject(std::vector<ModulePackageExportsQuerySubject>& subjects,
    const ModuleRecord& module, const sema::CheckedModule& checked, const syntax::AstModule* const ast,
    const std::unordered_set<std::string>& package_export_keys)
{
    const sema::StableModuleId stable_module = stable_module_id_from_record(module);
    const query::ModuleKey key = module_key_from_record(module);
    if (!query::is_valid(stable_module) || !query::is_valid(key)) {
        return;
    }

    const std::vector<ModuleExportsSignatureEntry> entries =
        collect_module_exports_signature_entries(module, checked, ast, stable_module, syntax::Visibility::package_);
    ModulePackageReexportDependencies dependencies = primary_package_reexport_dependencies(module, package_export_keys);
    subjects.push_back(ModulePackageExportsQuerySubject{
        key,
        module_exports_result_fingerprint(key, entries, INCREMENTAL_CACHE_MODULE_PACKAGE_EXPORTS_RESULT_MARKER),
        std::move(dependencies.public_surface),
        std::move(dependencies.package_surface),
    });
}

void push_item_list_query_subject(std::vector<ItemListQuerySubject>& subjects, const ModuleRecord& module,
    const sema::CheckedModule& checked, const syntax::AstModule* const ast)
{
    const sema::StableModuleId stable_module = stable_module_id_from_record(module);
    const query::ModuleKey key = module_key_from_record(module);
    if (!query::is_valid(stable_module) || !query::is_valid(key)) {
        return;
    }

    const std::vector<ItemListSignatureEntry> entries =
        collect_item_list_signature_entries(checked, ast, stable_module);
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
        push_module_graph_query_subject(subjects, module);
    }
    return subjects;
}

[[nodiscard]] std::vector<ModuleExportsQuerySubject> collect_module_exports_query_subjects(
    const std::span<const ModuleRecord> modules, const sema::CheckedModule& checked, const syntax::AstModule* const ast)
{
    std::vector<ModuleExportsQuerySubject> subjects;
    subjects.reserve(modules.size());
    for (const ModuleRecord& module : modules) {
        push_module_exports_query_subject(subjects, module, checked, ast);
    }
    return subjects;
}

[[nodiscard]] std::vector<ModulePackageExportsQuerySubject> collect_module_package_exports_query_subjects(
    const std::span<const ModuleRecord> modules, const sema::CheckedModule& checked, const syntax::AstModule* const ast)
{
    const ModulePackageExportPlan plan = module_package_export_plan(modules, checked, ast);
    std::vector<ModulePackageExportsQuerySubject> subjects;
    subjects.reserve(plan.enabled_keys.size());
    for (base::usize index = 0; index < modules.size(); ++index) {
        if (!plan.enabled[index]) {
            continue;
        }
        push_module_package_exports_query_subject(subjects, modules[index], checked, ast, plan.enabled_keys);
    }
    return subjects;
}

[[nodiscard]] std::vector<ItemListQuerySubject> collect_item_list_query_subjects(
    const std::span<const ModuleRecord> modules, const sema::CheckedModule& checked, const syntax::AstModule* const ast)
{
    std::vector<ItemListQuerySubject> subjects;
    subjects.reserve(modules.size());
    for (const ModuleRecord& module : modules) {
        push_item_list_query_subject(subjects, module, checked, ast);
    }
    return subjects;
}

} // namespace aurex::driver::incremental_cache_detail
