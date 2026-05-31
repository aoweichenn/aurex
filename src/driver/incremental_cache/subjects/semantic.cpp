#include <aurex/ir/ir_fingerprint.hpp>

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

constexpr std::string_view INCREMENTAL_CACHE_PRIMARY_MODULE_PART_KEY_NAME = "<primary>";
constexpr std::string_view INCREMENTAL_CACHE_LOWER_FUNCTION_IR_BODY_KIND = "body";
constexpr std::string_view INCREMENTAL_CACHE_LOWER_FUNCTION_IR_GENERIC_INSTANCE_KIND = "generic-instance";

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
    std::unordered_map<base::u32, const ModuleRecord*> record_by_module_id;
    std::unordered_map<std::string, const ModuleRecord*> record_by_stable_module;
};

[[nodiscard]] PackageIndex package_index_for_modules(const std::span<const ModuleRecord> modules)
{
    PackageIndex packages;
    packages.by_module_id.reserve(modules.size());
    packages.by_stable_module.reserve(modules.size());
    packages.record_by_module_id.reserve(modules.size());
    packages.record_by_stable_module.reserve(modules.size());
    for (const ModuleRecord& module : modules) {
        const query::PackageKey package = cache_package_key_or_default(module.package);
        const std::string stable_module_key = query::stable_serialize(stable_module_id_from_record(module));
        if (syntax::is_valid(module.id)) {
            packages.by_module_id.emplace(module.id.value, package);
            packages.record_by_module_id.emplace(module.id.value, &module);
        }
        packages.by_stable_module.emplace(stable_module_key, package);
        packages.record_by_stable_module.emplace(stable_module_key, &module);
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

[[nodiscard]] const ModuleRecord* module_record_for_definition(
    const PackageIndex& packages, const sema::StableModuleId stable_module, const syntax::ModuleId module_id)
{
    if (syntax::is_valid(module_id)) {
        const auto found_by_id = packages.record_by_module_id.find(module_id.value);
        if (found_by_id != packages.record_by_module_id.end()) {
            return found_by_id->second;
        }
    }
    const auto found_by_stable_id = packages.record_by_stable_module.find(query::stable_serialize(stable_module));
    return found_by_stable_id == packages.record_by_stable_module.end() ? nullptr : found_by_stable_id->second;
}

[[nodiscard]] query::ModuleKey module_key_from_record(const ModuleRecord& module)
{
    return query::module_key_from_stable_id(
        cache_package_key_or_default(module.package), stable_module_id_from_record(module));
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
    return part.kind == ModulePartRecordKind::primary ? INCREMENTAL_CACHE_PRIMARY_MODULE_PART_KEY_NAME : part.name;
}

[[nodiscard]] query::ModulePartKey module_part_key_from_record(
    const query::ModuleKey module_key, const ModuleRecord& module, const ModulePartRecord& part)
{
    if (query::is_valid(part.key)) {
        return part.key;
    }
    const query::FileKey file_key =
        query::file_key(cache_package_key_or_default(module.package), part.path.string(), query::SourceRole::source);
    return query::module_part_key(
        module_key, file_key, module_part_key_kind(part.kind), module_part_key_name(part), part.stable_index);
}

[[nodiscard]] query::ModulePartKey module_part_key_for_definition(const PackageIndex& packages,
    const sema::StableModuleId stable_module, const syntax::ModuleId module_id, const base::u32 part_index)
{
    const ModuleRecord* const module = module_record_for_definition(packages, stable_module, module_id);
    if (module == nullptr) {
        return {};
    }
    const query::ModuleKey module_key = module_key_from_record(*module);
    if (!query::is_valid(module_key)) {
        return {};
    }
    for (const ModulePartRecord& part : module->parts) {
        if (part.stable_index == part_index) {
            return module_part_key_from_record(module_key, *module, part);
        }
    }
    return {};
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

[[nodiscard]] query::ItemSignatureAuthority item_signature_authority_base(const sema::StableDefId& stable_id,
    const sema::IncrementalKey& incremental_key, const syntax::ModuleId module_id, const base::u32 part_index,
    const query::DefNamespace name_space, const query::DefKind kind, const syntax::Visibility visibility,
    const PackageIndex& packages)
{
    return query::ItemSignatureAuthority{
        incremental_key,
        module_part_key_for_definition(packages, stable_id.module, module_id, part_index),
        name_space,
        kind,
        syntax::visibility_rank(visibility),
    };
}

[[nodiscard]] query::ItemSignatureAuthority item_signature_authority(
    const sema::FunctionSignature& signature, const PackageIndex& packages)
{
    query::ItemSignatureAuthority authority = item_signature_authority_base(signature.stable_id,
        signature.incremental_key, signature.module, signature.part_index, query::DefNamespace::value,
        function_signature_def_kind(signature), signature.visibility, packages);
    authority.value_component_count = static_cast<base::u32>(signature.param_types.size());
    authority.generic_param_count = static_cast<base::u32>(signature.generic_args.size());
    authority.has_return_type = sema::is_valid(signature.return_type);
    authority.has_receiver_type = sema::is_valid(signature.method_owner_type) || signature.has_self_param;
    authority.is_unsafe = signature.is_unsafe;
    authority.is_variadic = signature.is_variadic;
    authority.has_definition = signature.has_definition;
    return authority;
}

[[nodiscard]] query::ItemSignatureAuthority item_signature_authority(
    const sema::StructInfo& info, const PackageIndex& packages)
{
    query::ItemSignatureAuthority authority = item_signature_authority_base(info.stable_id, info.incremental_key,
        info.module, info.part_index, query::DefNamespace::type, query::DefKind::struct_, info.visibility, packages);
    authority.value_component_count = static_cast<base::u32>(info.fields.size());
    authority.generic_param_count = static_cast<base::u32>(info.generic_instance_key.type_args.size());
    authority.has_return_type = sema::is_valid(info.type);
    authority.has_definition = !info.is_opaque;
    return authority;
}

[[nodiscard]] query::ItemSignatureAuthority item_signature_authority(
    const sema::EnumCaseInfo& info, const PackageIndex& packages)
{
    query::ItemSignatureAuthority authority = item_signature_authority_base(info.stable_id, info.incremental_key,
        info.module, info.part_index, query::DefNamespace::value, query::DefKind::enum_case, info.visibility, packages);
    authority.value_component_count = static_cast<base::u32>(info.payload_types.size());
    authority.generic_param_count = static_cast<base::u32>(info.generic_instance_key.type_args.size());
    authority.has_return_type = sema::is_valid(info.type);
    authority.has_definition = true;
    return authority;
}

[[nodiscard]] query::ItemSignatureAuthority item_signature_authority(
    const sema::TypeAliasInfo& info, const PackageIndex& packages)
{
    query::ItemSignatureAuthority authority = item_signature_authority_base(info.stable_id, info.incremental_key,
        info.module, info.part_index, query::DefNamespace::type, query::DefKind::type_alias, info.visibility, packages);
    authority.value_component_count = syntax::is_valid(info.target) ? 1U : 0U;
    authority.has_return_type = syntax::is_valid(info.target);
    authority.has_definition = true;
    return authority;
}

[[nodiscard]] query::GenericTemplateSignatureAuthority generic_template_signature_authority(
    const sema::GenericTemplateSignatureInfo& info, const PackageIndex& packages)
{
    return query::GenericTemplateSignatureAuthority{
        info.incremental_key,
        module_part_key_for_definition(packages, info.stable_id.module, info.module, info.part_index),
        info.name_space,
        syntax::visibility_rank(info.visibility),
        info.param_count,
        info.constraint_count,
    };
}

[[nodiscard]] query::GenericInstanceSignatureKind generic_function_signature_kind(
    const sema::FunctionSignature& signature) noexcept
{
    return signature.is_method ? query::GenericInstanceSignatureKind::method
                               : query::GenericInstanceSignatureKind::function;
}

[[nodiscard]] query::GenericInstanceSignatureAuthority generic_instance_signature_authority_base(
    const query::GenericInstanceKey& key, const sema::IncrementalKey& incremental_key,
    const query::GenericInstanceSignatureKind kind)
{
    return query::GenericInstanceSignatureAuthority{
        incremental_key,
        kind,
        0,
        static_cast<base::u32>(key.type_args.size()),
        static_cast<base::u32>(key.const_args.size()),
        key.param_env.predicate_count,
    };
}

[[nodiscard]] query::GenericInstanceSignatureAuthority generic_instance_signature_authority(
    const query::GenericInstanceKey& key, const sema::FunctionSignature& signature)
{
    query::GenericInstanceSignatureAuthority authority = generic_instance_signature_authority_base(
        key, signature.incremental_key, generic_function_signature_kind(signature));
    authority.visibility_rank = syntax::visibility_rank(signature.visibility);
    authority.value_param_count = static_cast<base::u32>(signature.param_types.size());
    authority.generic_param_count = static_cast<base::u32>(signature.generic_args.size());
    authority.has_return_type = sema::is_valid(signature.return_type);
    authority.has_receiver_type = sema::is_valid(signature.method_owner_type) || signature.has_self_param;
    authority.is_unsafe = signature.is_unsafe;
    authority.is_variadic = signature.is_variadic;
    authority.has_definition = signature.has_definition;
    return authority;
}

[[nodiscard]] query::GenericInstanceSignatureAuthority generic_instance_signature_authority(
    const query::GenericInstanceKey& key, const sema::StructInfo& info)
{
    query::GenericInstanceSignatureAuthority authority = generic_instance_signature_authority_base(
        key, info.incremental_key, query::GenericInstanceSignatureKind::struct_);
    authority.visibility_rank = syntax::visibility_rank(info.visibility);
    authority.value_param_count = static_cast<base::u32>(info.fields.size());
    authority.generic_param_count = static_cast<base::u32>(key.type_args.size());
    authority.has_return_type = sema::is_valid(info.type);
    return authority;
}

[[nodiscard]] query::GenericInstanceSignatureAuthority generic_instance_signature_authority(
    const sema::GenericEnumInstanceInfo& instance)
{
    query::GenericInstanceSignatureAuthority authority = generic_instance_signature_authority_base(
        instance.generic_instance_key, instance.incremental_key, query::GenericInstanceSignatureKind::enum_);
    authority.generic_param_count = static_cast<base::u32>(instance.generic_instance_key.type_args.size());
    authority.has_return_type = sema::is_valid(instance.type);
    return authority;
}

[[nodiscard]] query::GenericInstanceSignatureAuthority generic_instance_signature_authority(
    const sema::GenericTypeAliasInstanceInfo& instance)
{
    query::GenericInstanceSignatureAuthority authority = generic_instance_signature_authority_base(
        instance.generic_instance_key, instance.incremental_key, query::GenericInstanceSignatureKind::type_alias);
    authority.generic_param_count = static_cast<base::u32>(instance.generic_instance_key.type_args.size());
    authority.has_return_type = sema::is_valid(instance.resolved_type);
    return authority;
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
    const query::BodySlotKind slot = signature.is_trait_default_method_instance
        ? query::BodySlotKind::trait_default_method
        : query::BodySlotKind::function_body;
    return query::body_key(def_key_from_stable_id(packages, signature.stable_id, signature.module,
                               query::DefNamespace::value, function_signature_def_kind(signature)),
        slot);
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

[[nodiscard]] query::FunctionBodySyntaxAuthority function_body_syntax_authority(const query::BodyKey key,
    const query::ModulePartKey module_part, const base::SourceRange body_range, const std::string_view body_text)
{
    return query::FunctionBodySyntaxAuthority{
        function_body_syntax_result_fingerprint(key, body_text),
        key.owner,
        module_part,
        static_cast<base::u64>(body_range.begin),
        static_cast<base::u64>(body_range.end),
        key.slot,
        key.ordinal,
    };
}

[[nodiscard]] query::QueryResultFingerprint type_check_body_checked_result_fingerprint(const query::BodyKey key,
    const query::QueryResultFingerprint body_syntax_result, const query::QueryResultFingerprint signature_result)
{
    query::StableHashBuilder builder;
    builder.mix_string(INCREMENTAL_CACHE_TYPE_CHECK_BODY_RESULT_MARKER);
    builder.mix_fingerprint(query::stable_key_fingerprint(key));
    builder.mix_u64(body_syntax_result.global_id);
    builder.mix_fingerprint(body_syntax_result.fingerprint);
    builder.mix_u64(signature_result.global_id);
    builder.mix_fingerprint(signature_result.fingerprint);
    return query::query_result_fingerprint(builder.finish());
}

[[nodiscard]] query::TypeCheckBodyAuthority type_check_body_authority(const query::BodyKey key,
    const query::QueryResultFingerprint body_syntax_result, const query::QueryResultFingerprint signature_result)
{
    return query::TypeCheckBodyAuthority{
        type_check_body_checked_result_fingerprint(key, body_syntax_result, signature_result),
        body_syntax_result,
        signature_result,
    };
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

[[nodiscard]] std::optional<base::SourceRange> generic_function_instance_body_range(const sema::CheckedModule& checked,
    const sema::GenericFunctionInstanceInfo& instance, const syntax::AstModule& ast) noexcept
{
    const sema::GenericFunctionInstanceBodyView body = checked.generic_function_instance_body_view(ast, instance);
    if (!sema::is_valid(body) || body.body.value >= ast.stmts.size()) {
        return std::nullopt;
    }
    return ast.stmts.range(body.body.value);
}

[[nodiscard]] std::optional<std::string_view> generic_function_instance_body_text(const sema::CheckedModule& checked,
    const base::SourceManager& sources, const sema::GenericFunctionInstanceInfo& instance,
    const syntax::AstModule* const ast) noexcept
{
    if (ast != nullptr) {
        if (const std::optional<base::SourceRange> body_range =
                generic_function_instance_body_range(checked, instance, *ast)) {
            return source_range_text(sources, *body_range);
        }
    }
    return source_range_text(sources, instance.signature.range);
}

[[nodiscard]] query::QueryResultFingerprint generic_instance_body_checked_result_fingerprint(
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

[[nodiscard]] base::u32 side_table_entry_count(const sema::GenericNodeSpan span, const sema::SemaIndexTable& ids)
{
    return ids.empty() ? span.count : static_cast<base::u32>(ids.size());
}

[[nodiscard]] query::GenericInstanceBodyAuthority generic_instance_body_authority(
    const sema::GenericFunctionInstanceInfo& instance, const query::GenericInstanceKey& instance_key,
    const query::QueryResultFingerprint checked_body)
{
    const query::GenericInstanceSignatureAuthority signature_authority =
        generic_instance_signature_authority(instance_key, instance.signature);
    const query::QueryResultFingerprint signature_result =
        query::generic_instance_signature_result_fingerprint(signature_authority);
    const sema::GenericSideTables& side_tables = instance.side_tables;
    const sema::GenericSideTableLayout* const layout = side_tables.layout;
    const base::u32 expr_count = layout == nullptr
        ? side_table_entry_count(side_tables.expr_span, side_tables.expr_node_ids)
        : side_table_entry_count(layout->expr_span, layout->expr_node_ids);
    const base::u32 pattern_count = layout == nullptr
        ? side_table_entry_count(side_tables.pattern_span, side_tables.pattern_node_ids)
        : side_table_entry_count(layout->pattern_span, layout->pattern_node_ids);
    const base::u32 type_count = layout == nullptr
        ? side_table_entry_count(side_tables.type_span, side_tables.type_node_ids)
        : side_table_entry_count(layout->type_span, layout->type_node_ids);
    const base::u32 stmt_count = layout == nullptr
        ? side_table_entry_count(side_tables.stmt_span, side_tables.stmt_node_ids)
        : side_table_entry_count(layout->stmt_span, layout->stmt_node_ids);
    const base::u32 fallback_count = static_cast<base::u32>(side_tables.sparse_fallbacks.total());
    return query::GenericInstanceBodyAuthority{
        checked_body,
        signature_result,
        expr_count,
        pattern_count,
        type_count,
        stmt_count,
        fallback_count,
        side_tables.local_dense || side_tables.sparse || expr_count != 0 || pattern_count != 0 || type_count != 0
            || stmt_count != 0 || fallback_count != 0,
        side_tables.local_dense,
        side_tables.sparse,
    };
}

[[nodiscard]] query::QueryResultFingerprint lower_function_ir_result_fingerprint(const query::BodyKey key,
    const query::QueryResultFingerprint type_check_result, const ir::FunctionIRUnitFingerprint& ir_unit)
{
    query::StableHashBuilder builder;
    builder.mix_string(INCREMENTAL_CACHE_LOWER_FUNCTION_IR_RESULT_MARKER);
    builder.mix_string(INCREMENTAL_CACHE_LOWER_FUNCTION_IR_BODY_KIND);
    builder.mix_fingerprint(query::stable_key_fingerprint(key));
    builder.mix_u64(type_check_result.global_id);
    builder.mix_fingerprint(type_check_result.fingerprint);
    builder.mix_string(ir_unit.symbol);
    builder.mix_u64(ir_unit.target_independent_ir.global_id);
    builder.mix_fingerprint(ir_unit.target_independent_ir.fingerprint);
    builder.mix_u64(ir_unit.llvm_emission_unit.global_id);
    builder.mix_fingerprint(ir_unit.llvm_emission_unit.fingerprint);
    return query::query_result_fingerprint(builder.finish());
}

[[nodiscard]] query::QueryResultFingerprint lower_generic_instance_ir_result_fingerprint(
    const query::GenericInstanceKey& key, const query::QueryResultFingerprint generic_body_result,
    const ir::FunctionIRUnitFingerprint& ir_unit)
{
    query::StableHashBuilder builder;
    builder.mix_string(INCREMENTAL_CACHE_LOWER_FUNCTION_IR_RESULT_MARKER);
    builder.mix_string(INCREMENTAL_CACHE_LOWER_FUNCTION_IR_GENERIC_INSTANCE_KIND);
    builder.mix_fingerprint(query::stable_key_fingerprint(key));
    builder.mix_u64(generic_body_result.global_id);
    builder.mix_fingerprint(generic_body_result.fingerprint);
    builder.mix_string(ir_unit.symbol);
    builder.mix_u64(ir_unit.target_independent_ir.global_id);
    builder.mix_fingerprint(ir_unit.target_independent_ir.fingerprint);
    builder.mix_u64(ir_unit.llvm_emission_unit.global_id);
    builder.mix_fingerprint(ir_unit.llvm_emission_unit.fingerprint);
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

void push_item_signature_query_subject(std::vector<ItemSignatureQuerySubject>& subjects, const query::DefKey key,
    const query::ItemSignatureAuthority& authority)
{
    if (!query::is_valid(key) || !query::is_valid(authority)) {
        return;
    }
    subjects.push_back(ItemSignatureQuerySubject{
        key,
        authority,
    });
}

void push_generic_template_signature_query_subject(std::vector<GenericTemplateSignatureQuerySubject>& subjects,
    const sema::GenericTemplateSignatureInfo& info, const PackageIndex& packages)
{
    const query::DefKey key = def_key_from_stable_id(
        packages, info.stable_id, info.module, info.name_space, query::DefKind::generic_template);
    const query::GenericTemplateSignatureAuthority authority = generic_template_signature_authority(info, packages);
    if (!query::is_valid(key) || !query::is_valid(authority)) {
        return;
    }
    subjects.push_back(GenericTemplateSignatureQuerySubject{
        key,
        authority,
    });
}

void push_generic_instance_signature_query_subject(std::vector<GenericInstanceSignatureQuerySubject>& subjects,
    const query::GenericInstanceKey& key, const query::GenericInstanceSignatureAuthority& authority)
{
    if (!query::is_valid(key) || !query::is_valid(authority)) {
        return;
    }
    subjects.push_back(GenericInstanceSignatureQuerySubject{
        &key,
        authority,
    });
}

void push_unique_generic_instance_signature_query_subject(std::vector<GenericInstanceSignatureQuerySubject>& subjects,
    std::unordered_set<query::GenericInstanceKey, query::GenericInstanceKeyHash>& seen,
    const query::GenericInstanceKey& key, const query::GenericInstanceSignatureAuthority& authority)
{
    if (!query::is_valid(key) || !query::is_valid(authority)) {
        return;
    }
    if (!seen.insert(key).second) {
        return;
    }
    push_generic_instance_signature_query_subject(subjects, key, authority);
}

void push_generic_instance_body_query_subject(std::vector<GenericInstanceBodyQuerySubject>& subjects,
    const sema::CheckedModule& checked, const sema::GenericFunctionInstanceInfo& instance,
    const base::SourceManager& sources, const syntax::AstModule* const ast)
{
    const query::GenericInstanceKey& instance_key = query::is_valid(instance.signature.generic_instance_key)
        ? instance.signature.generic_instance_key
        : instance.generic_instance_key;
    if (!query::is_valid(instance_key) || !query::is_valid(instance.signature.incremental_key)
        || !instance.signature.has_definition || instance.signature.has_conflict) {
        return;
    }
    const std::optional<std::string_view> body_text =
        generic_function_instance_body_text(checked, sources, instance, ast);
    if (!body_text) {
        return;
    }
    const query::QueryResultFingerprint checked_body =
        generic_instance_body_checked_result_fingerprint(instance_key, instance.signature.incremental_key, *body_text);
    const query::GenericInstanceBodyAuthority authority =
        generic_instance_body_authority(instance, instance_key, checked_body);
    if (!query::is_valid(authority)) {
        return;
    }
    subjects.push_back(GenericInstanceBodyQuerySubject{
        &instance_key,
        authority,
    });
}

void push_lower_function_ir_query_subject(std::vector<LowerFunctionIRQuerySubject>& subjects,
    const TypeCheckBodyQuerySubject& type_check_subject, const ir::FunctionIRUnitFingerprint& ir_unit)
{
    const query::QueryResultFingerprint result = lower_function_ir_result_fingerprint(
        type_check_subject.key, query::type_check_body_result_fingerprint(type_check_subject.authority), ir_unit);
    subjects.push_back(LowerFunctionIRQuerySubject{
        LowerFunctionIRSubjectKind::body,
        type_check_subject.key,
        nullptr,
        result,
    });
}

void push_lower_generic_instance_ir_query_subject(std::vector<LowerFunctionIRQuerySubject>& subjects,
    const GenericInstanceBodyQuerySubject& generic_body_subject, const ir::FunctionIRUnitFingerprint& ir_unit)
{
    if (generic_body_subject.key == nullptr) {
        return;
    }
    const query::QueryResultFingerprint result = lower_generic_instance_ir_result_fingerprint(*generic_body_subject.key,
        query::generic_instance_body_result_fingerprint(generic_body_subject.authority), ir_unit);
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
    const std::optional<base::SourceRange> body_range = ast == nullptr
        ? std::optional<base::SourceRange>{signature.range}
        : function_signature_body_range(signature, *ast);
    if (!body_range) {
        return;
    }
    const std::optional<std::string_view> body_text = source_range_text(sources, *body_range);
    if (!body_text) {
        return;
    }
    const query::ModulePartKey module_part =
        module_part_key_for_definition(packages, signature.stable_id.module, signature.module, signature.part_index);
    const query::FunctionBodySyntaxAuthority syntax_authority =
        function_body_syntax_authority(key, module_part, *body_range, *body_text);
    if (!query::is_valid(syntax_authority)) {
        return;
    }
    const query::ItemSignatureAuthority signature_authority = item_signature_authority(signature, packages);
    const query::QueryResultFingerprint signature_result =
        query::item_signature_result_fingerprint(signature_authority);
    if (!query::is_valid(signature_result)) {
        return;
    }
    const query::QueryResultFingerprint syntax_result =
        query::function_body_syntax_result_fingerprint(syntax_authority);
    const query::TypeCheckBodyAuthority type_check_authority =
        type_check_body_authority(key, syntax_result, signature_result);
    if (!query::is_valid(type_check_authority)) {
        return;
    }
    syntax_subjects.push_back(FunctionBodySyntaxQuerySubject{
        key,
        syntax_authority,
    });
    type_check_subjects.push_back(TypeCheckBodyQuerySubject{
        key,
        type_check_authority,
    });
}

using SymbolByStableKey = std::unordered_map<std::string, std::string>;
using IRUnitBySymbol = std::unordered_map<std::string, ir::FunctionIRUnitFingerprint>;

[[nodiscard]] SymbolByStableKey function_body_symbols_by_stable_key(
    const sema::CheckedModule& checked, const PackageIndex& packages)
{
    SymbolByStableKey symbols;
    symbols.reserve(checked.functions.size());
    for (const auto& entry : checked.functions) {
        const sema::FunctionSignature& signature = entry.second;
        if (!signature.has_definition || signature.has_conflict) {
            continue;
        }
        const query::BodyKey key = function_body_key(signature, packages);
        if (!query::is_valid(key) || signature.c_name.empty()) {
            continue;
        }
        symbols.emplace(query::stable_serialize(key), std::string(signature.c_name.view()));
    }
    return symbols;
}

[[nodiscard]] SymbolByStableKey generic_instance_symbols_by_stable_key(const sema::CheckedModule& checked)
{
    SymbolByStableKey symbols;
    symbols.reserve(checked.generic_function_instances.size());
    for (const sema::GenericFunctionInstanceInfo& instance : checked.generic_function_instances) {
        const query::GenericInstanceKey& instance_key = query::is_valid(instance.signature.generic_instance_key)
            ? instance.signature.generic_instance_key
            : instance.generic_instance_key;
        if (!query::is_valid(instance_key) || instance.signature.c_name.empty()) {
            continue;
        }
        symbols.emplace(query::stable_serialize(instance_key), std::string(instance.signature.c_name.view()));
    }
    return symbols;
}

[[nodiscard]] IRUnitBySymbol ir_units_by_symbol(const ir::Module& lowered_ir)
{
    IRUnitBySymbol units;
    std::vector<ir::FunctionIRUnitFingerprint> fingerprints = ir::function_ir_unit_fingerprints(lowered_ir);
    units.reserve(fingerprints.size());
    for (ir::FunctionIRUnitFingerprint& unit : fingerprints) {
        std::string symbol = unit.symbol;
        units.emplace(std::move(symbol), std::move(unit));
    }
    return units;
}

void push_lower_function_ir_query_subject_for_symbol(std::vector<LowerFunctionIRQuerySubject>& subjects,
    const TypeCheckBodyQuerySubject& type_check_subject, const SymbolByStableKey& symbols, const IRUnitBySymbol& units)
{
    const auto symbol = symbols.find(query::stable_serialize(type_check_subject.key));
    if (symbol == symbols.end()) {
        return;
    }
    const auto unit = units.find(symbol->second);
    if (unit == units.end()) {
        return;
    }
    push_lower_function_ir_query_subject(subjects, type_check_subject, unit->second);
}

void push_lower_generic_instance_ir_query_subject_for_symbol(std::vector<LowerFunctionIRQuerySubject>& subjects,
    const GenericInstanceBodyQuerySubject& generic_body_subject, const SymbolByStableKey& symbols,
    const IRUnitBySymbol& units)
{
    if (generic_body_subject.key == nullptr) {
        return;
    }
    const auto symbol = symbols.find(query::stable_serialize(*generic_body_subject.key));
    if (symbol == symbols.end()) {
        return;
    }
    const auto unit = units.find(symbol->second);
    if (unit == units.end()) {
        return;
    }
    push_lower_generic_instance_ir_query_subject(subjects, generic_body_subject, unit->second);
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
        const query::DefKey key = def_key_from_stable_id(packages, signature.stable_id, signature.module,
            query::DefNamespace::value, function_signature_def_kind(signature));
        push_item_signature_query_subject(subjects, key, item_signature_authority(signature, packages));
    }
    for (const auto& entry : checked.structs) {
        const sema::StructInfo& info = entry.second;
        const query::DefKey key = def_key_from_stable_id(
            packages, info.stable_id, info.module, query::DefNamespace::type, query::DefKind::struct_);
        push_item_signature_query_subject(subjects, key, item_signature_authority(info, packages));
    }
    for (const auto& entry : checked.enum_cases) {
        const sema::EnumCaseInfo& info = entry.second;
        const query::DefKey key = def_key_from_stable_id(
            packages, info.stable_id, info.module, query::DefNamespace::value, query::DefKind::enum_case);
        push_item_signature_query_subject(subjects, key, item_signature_authority(info, packages));
    }
    for (const auto& entry : checked.type_aliases) {
        const sema::TypeAliasInfo& info = entry.second;
        const query::DefKey key = def_key_from_stable_id(
            packages, info.stable_id, info.module, query::DefNamespace::type, query::DefKind::type_alias);
        push_item_signature_query_subject(subjects, key, item_signature_authority(info, packages));
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
        push_unique_generic_instance_signature_query_subject(subjects, seen, signature.generic_instance_key,
            generic_instance_signature_authority(signature.generic_instance_key, signature));
    }
    for (const sema::GenericFunctionInstanceInfo& instance : checked.generic_function_instances) {
        const query::GenericInstanceKey& instance_key = query::is_valid(instance.signature.generic_instance_key)
            ? instance.signature.generic_instance_key
            : instance.generic_instance_key;
        push_unique_generic_instance_signature_query_subject(
            subjects, seen, instance_key, generic_instance_signature_authority(instance_key, instance.signature));
    }
    for (const auto& entry : checked.structs) {
        const sema::StructInfo& info = entry.second;
        push_unique_generic_instance_signature_query_subject(subjects, seen, info.generic_instance_key,
            generic_instance_signature_authority(info.generic_instance_key, info));
    }
    for (const sema::GenericEnumInstanceInfo& instance : checked.generic_enum_instances) {
        push_unique_generic_instance_signature_query_subject(
            subjects, seen, instance.generic_instance_key, generic_instance_signature_authority(instance));
    }
    for (const sema::GenericTypeAliasInstanceInfo& instance : checked.generic_type_alias_instances) {
        push_unique_generic_instance_signature_query_subject(
            subjects, seen, instance.generic_instance_key, generic_instance_signature_authority(instance));
    }
    return subjects;
}

[[nodiscard]] std::vector<GenericInstanceBodyQuerySubject> collect_generic_instance_body_query_subjects(
    const sema::CheckedModule& checked, const base::SourceManager& sources, const syntax::AstModule* const ast)
{
    std::vector<GenericInstanceBodyQuerySubject> subjects;
    subjects.reserve(checked.generic_function_instances.size());
    for (const sema::GenericFunctionInstanceInfo& instance : checked.generic_function_instances) {
        push_generic_instance_body_query_subject(subjects, checked, instance, sources, ast);
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
    const std::vector<GenericInstanceBodyQuerySubject>& generic_body_subjects, const sema::CheckedModule& checked,
    const std::span<const ModuleRecord> modules, const ir::Module& lowered_ir)
{
    const PackageIndex packages = package_index_for_modules(modules);
    const SymbolByStableKey function_symbols = function_body_symbols_by_stable_key(checked, packages);
    const SymbolByStableKey generic_symbols = generic_instance_symbols_by_stable_key(checked);
    const IRUnitBySymbol units = ir_units_by_symbol(lowered_ir);

    std::vector<LowerFunctionIRQuerySubject> subjects;
    subjects.reserve(type_check_subjects.size() + generic_body_subjects.size());
    for (const TypeCheckBodyQuerySubject& type_check_subject : type_check_subjects) {
        push_lower_function_ir_query_subject_for_symbol(subjects, type_check_subject, function_symbols, units);
    }
    for (const GenericInstanceBodyQuerySubject& generic_body_subject : generic_body_subjects) {
        push_lower_generic_instance_ir_query_subject_for_symbol(subjects, generic_body_subject, generic_symbols, units);
    }
    return subjects;
}

[[nodiscard]] std::vector<DefinitionRecord> collect_definitions(const sema::CheckedModule& checked)
{
    std::vector<DefinitionRecord> records;
    records.reserve(checked.functions.size() + checked.generic_template_signatures.size()
        + checked.generic_function_instances.size() + checked.trait_default_method_instances.size()
        + checked.structs.size() + checked.enum_cases.size() + checked.type_aliases.size());

    for (const sema::GenericTemplateSignatureInfo& info : checked.generic_template_signatures) {
        push_definition(records, INCREMENTAL_CACHE_CATEGORY_GENERIC_TEMPLATE, info.name.view(), info.stable_id,
            info.incremental_key);
    }
    for (const auto& entry : checked.functions) {
        const sema::FunctionSignature& signature = entry.second;
        if (signature.is_trait_default_method_instance) {
            continue;
        }
        push_definition(records, INCREMENTAL_CACHE_CATEGORY_FUNCTION, signature.name.view(), signature.stable_id,
            signature.incremental_key);
    }
    for (const sema::GenericFunctionInstanceInfo& instance : checked.generic_function_instances) {
        push_definition(records, INCREMENTAL_CACHE_CATEGORY_GENERIC_FUNCTION_INSTANCE, instance.signature.name.view(),
            instance.signature.stable_id, instance.signature.incremental_key);
    }
    for (const sema::TraitDefaultMethodInstanceInfo& instance : checked.trait_default_method_instances) {
        push_definition(records, INCREMENTAL_CACHE_CATEGORY_TRAIT_DEFAULT_METHOD_INSTANCE,
            instance.signature.name.view(), instance.signature.stable_id, instance.signature.incremental_key);
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
