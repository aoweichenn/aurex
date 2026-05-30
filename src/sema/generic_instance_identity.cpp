#include <aurex/sema/canonical_type_builder.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <sema/internal/sema_core.hpp>

namespace aurex::sema {
namespace {

constexpr std::string_view SEMA_GENERIC_INSTANCE_FINGERPRINT_PREFIX = "generic-instance:";
constexpr std::string_view SEMA_GENERIC_SIGNATURE_FINGERPRINT_PREFIX = "generic-signature:";
constexpr std::string_view SEMA_GENERIC_STRUCT_SIGNATURE_FINGERPRINT_PREFIX = "generic-struct-signature:";
constexpr std::string_view SEMA_GENERIC_ENUM_SIGNATURE_FINGERPRINT_PREFIX = "generic-enum-signature:";
constexpr std::string_view SEMA_GENERIC_TYPE_ALIAS_SIGNATURE_FINGERPRINT_PREFIX = "generic-alias-signature:";
constexpr std::string_view SEMA_GENERIC_PARAM_ENV_SEPARATOR = ":";
constexpr std::string_view SEMA_GENERIC_INSTANCE_QUERY_KEY_ERROR = "failed to build generic instance query key: ";
constexpr std::string_view SEMA_GENERIC_SIGNATURE_QUERY_KEY_ERROR =
    "failed to build generic instance signature fingerprint: ";
constexpr std::string_view SEMA_GENERIC_STRUCT_SIGNATURE_QUERY_KEY_ERROR =
    "failed to build generic struct instance signature fingerprint: ";
constexpr std::string_view SEMA_GENERIC_ENUM_SIGNATURE_QUERY_KEY_ERROR =
    "failed to build generic enum instance signature fingerprint: ";
constexpr std::string_view SEMA_GENERIC_TYPE_ALIAS_SIGNATURE_QUERY_KEY_ERROR =
    "failed to build generic type alias instance signature fingerprint: ";
constexpr base::u64 SEMA_GENERIC_INSTANCE_SIGNATURE_KEY_MARKER = 0x53454d4147495347ULL;
constexpr base::u64 SEMA_GENERIC_STRUCT_SIGNATURE_KEY_MARKER = 0x53454d4147535453ULL;
constexpr base::u64 SEMA_GENERIC_ENUM_SIGNATURE_KEY_MARKER = 0x53454d4147454e53ULL;
constexpr base::u64 SEMA_GENERIC_TYPE_ALIAS_SIGNATURE_KEY_MARKER = 0x53454d4147414c49ULL;
constexpr base::usize SEMA_GENERIC_PARAM_ENV_PREDICATE_SIZE_ESTIMATE = 16;
constexpr char SEMA_GENERIC_TEMPLATE_KEY_SEPARATOR = ':';

[[nodiscard]] base::Result<void> append_canonical_type_key(query::StableKeyWriter& writer, const TypeTable& types,
    const TypeHandle type, const CanonicalTypeKeyResolver& resolver)
{
    writer.write_bool(is_valid(type));
    if (!is_valid(type)) {
        return base::Result<void>::ok();
    }
    base::Result<query::CanonicalTypeKey> canonical_type = build_canonical_type_key(types, type, resolver);
    if (!canonical_type) {
        return base::Result<void>::fail(canonical_type.error());
    }
    query::append_stable_key(writer, canonical_type.value());
    return base::Result<void>::ok();
}

[[nodiscard]] std::optional<std::pair<syntax::ModuleId, std::string_view>> parse_generic_origin_key(
    const std::string_view origin_key) noexcept
{
    const base::usize separator = origin_key.find(SEMA_GENERIC_TEMPLATE_KEY_SEPARATOR);
    if (separator == std::string_view::npos || separator == 0 || separator + 1 >= origin_key.size()) {
        return std::nullopt;
    }
    base::u32 module_value = 0;
    const char* const begin = origin_key.data();
    const char* const end = origin_key.data() + separator;
    const auto [parsed_end, error] = std::from_chars(begin, end, module_value);
    if (error != std::errc() || parsed_end != end) {
        return std::nullopt;
    }
    return std::pair{syntax::ModuleId{module_value}, origin_key.substr(separator + 1)};
}

} // namespace

class SemanticAnalyzerCore::GenericInstanceCanonicalResolver final : public CanonicalTypeKeyResolver {
public:
    GenericInstanceCanonicalResolver(
        const SemanticAnalyzerCore& analyzer, const GenericTemplateInfo& owner, const query::DefKey owner_key)
        : analyzer_(analyzer), owner_(owner), owner_key_(owner_key)
    {
    }

    [[nodiscard]] std::optional<query::DefKey> nominal_type_key(
        const TypeHandle handle, const TypeInfo& info) const override
    {
        return this->analyzer_.canonical_nominal_type_query_key(handle, info);
    }

    [[nodiscard]] std::optional<query::GenericParamKey> generic_param_key(
        const TypeHandle, const TypeInfo& info) const override
    {
        return this->analyzer_.canonical_generic_param_query_key(this->owner_, this->owner_key_, info);
    }

private:
    const SemanticAnalyzerCore& analyzer_;
    const GenericTemplateInfo& owner_;
    query::DefKey owner_key_;
};

query::ModuleKey SemanticAnalyzerCore::query_module_key(const syntax::ModuleId module) const noexcept
{
    const query::PackageKey package = this->query_package_key(module);
    if (!syntax::is_valid(module) || module.value >= this->ctx_.module.modules.size()) {
        return query::module_key_from_stable_id(package, sema::stable_module_id(std::span<const std::string_view>{}));
    }
    return query::module_key_from_stable_id(
        package, sema::stable_module_id(this->ctx_.module.modules[module.value].path.parts));
}

query::ModulePartKey SemanticAnalyzerCore::query_module_part_key(
    const syntax::ModuleId module, const base::u32 part_index) const noexcept
{
    if (!syntax::is_valid(module) || module.value >= this->ctx_.options.module_part_keys.size()) {
        return {};
    }
    const std::vector<query::ModulePartKey>& part_keys = this->ctx_.options.module_part_keys[module.value];
    if (part_index >= part_keys.size()) {
        return {};
    }
    return part_keys[part_index];
}

query::ModulePartKey SemanticAnalyzerCore::query_module_part_key(const syntax::ItemId item) const noexcept
{
    if (!syntax::is_valid(item) || item.value >= this->ctx_.module.items.size()) {
        return {};
    }
    return this->query_module_part_key(this->item_module(item), this->item_part_index(item));
}

query::PackageKey SemanticAnalyzerCore::query_package_key(const syntax::ModuleId module) const noexcept
{
    if (syntax::is_valid(module) && module.value < this->ctx_.options.module_packages.size()
        && query::is_valid(this->ctx_.options.module_packages[module.value])) {
        return this->ctx_.options.module_packages[module.value];
    }
    if (query::is_valid(this->ctx_.options.default_package)) {
        return this->ctx_.options.default_package;
    }
    return query::package_key(std::span<const std::string_view>{});
}

query::DefKey SemanticAnalyzerCore::generic_template_query_key(
    const GenericTemplateInfo& info, const query::DefNamespace name_space) const noexcept
{
    return query::def_key_from_stable_id(
        this->query_package_key(info.module), info.stable_id, name_space, query::DefKind::generic_template);
}

void SemanticAnalyzerCore::index_generic_param_query_keys(
    const GenericTemplateInfo& info, const query::DefNamespace name_space)
{
    const query::DefKey owner_key = this->generic_template_query_key(info, name_space);
    for (base::usize index = 0; index < info.param_identities.size(); ++index) {
        const GenericParamIdentity identity = info.param_identities[index];
        if (!is_valid(identity)) {
            continue;
        }
        this->state_.generics.param_query_keys[identity] =
            query::generic_param_key(owner_key, static_cast<base::u32>(index));
    }
}

std::optional<query::DefKey> SemanticAnalyzerCore::canonical_nominal_type_query_key(
    const TypeHandle handle, const TypeInfo& info) const
{
    const auto make_type_key = [&](const syntax::ModuleId module, const std::string_view name,
                                   const query::DefKind kind) -> std::optional<query::DefKey> {
        if (!syntax::is_valid(module) || module.value >= this->ctx_.module.modules.size() || name.empty()) {
            return std::nullopt;
        }
        const std::array<std::string_view, 1> path{name};
        return query::def_key(this->query_module_key(module), query::DefNamespace::type, kind, path);
    };

    if (!info.generic_origin_key.empty()) {
        const std::optional<std::pair<syntax::ModuleId, std::string_view>> origin =
            parse_generic_origin_key(info.generic_origin_key.view());
        if (origin.has_value()) {
            return make_type_key(origin->first, origin->second, query::DefKind::generic_template);
        }
    }

    if (const auto found = this->state_.types.struct_infos_by_type.find(handle.value);
        found != this->state_.types.struct_infos_by_type.end()) {
        const StructInfo* const struct_info = found->second;
        if (struct_info != nullptr) {
            return make_type_key(struct_info->module, struct_info->name.view(), query::DefKind::struct_);
        }
    }

    if (info.kind == TypeKind::enum_) {
        for (const auto& entry : this->state_.types.named_types) {
            if (entry.second.value == handle.value) {
                const std::string_view enum_name = this->ctx_.module.identifier_text(entry.first.name);
                return make_type_key(syntax::ModuleId{entry.first.module}, enum_name, query::DefKind::enum_);
            }
        }
        for (const auto& entry : this->state_.checked.enum_cases) {
            if (entry.second.type.value == handle.value) {
                return make_type_key(entry.second.module, entry.second.enum_name.view(), query::DefKind::enum_);
            }
        }
    }

    return std::nullopt;
}

std::optional<query::GenericParamKey> SemanticAnalyzerCore::canonical_generic_param_query_key(
    const GenericTemplateInfo& owner, const query::DefKey owner_key, const TypeInfo& info) const
{
    if (!is_valid(info.generic_identity)) {
        return std::nullopt;
    }
    for (base::usize index = 0; index < owner.param_identities.size(); ++index) {
        if (owner.param_identities[index] == info.generic_identity) {
            return query::generic_param_key(owner_key, static_cast<base::u32>(index));
        }
    }
    if (const auto found = this->state_.generics.param_query_keys.find(info.generic_identity);
        found != this->state_.generics.param_query_keys.end()) {
        return found->second;
    }
    return std::nullopt;
}

query::ParamEnvKey SemanticAnalyzerCore::generic_param_env_key(const GenericTemplateInfo& info) const
{
    std::vector<std::string> predicates;
    predicates.reserve(info.predicate_indices.empty() ? info.constraints.size() : info.predicate_indices.size());
    if (!info.predicate_indices.empty()) {
        for (const base::u32 predicate_index : info.predicate_indices) {
            if (predicate_index >= this->state_.checked.trait_predicates.size()) {
                continue;
            }
            predicates.push_back(
                query::debug_string(this->state_.checked.trait_predicates[predicate_index].canonical_fingerprint));
        }
        std::ranges::sort(predicates);
    } else {
        for (base::usize param_index = 0; param_index < info.params.size(); ++param_index) {
            const auto found = info.constraints.find(info.params[param_index]);
            if (found == info.constraints.end()) {
                continue;
            }
            std::vector<CapabilityKind> capabilities(found->second.begin(), found->second.end());
            std::ranges::sort(capabilities, [](const CapabilityKind lhs, const CapabilityKind rhs) {
                return static_cast<base::u8>(lhs) < static_cast<base::u8>(rhs);
            });
            for (const CapabilityKind capability : capabilities) {
                std::string predicate;
                predicate.reserve(SEMA_GENERIC_PARAM_ENV_PREDICATE_SIZE_ESTIMATE);
                predicate += std::to_string(param_index);
                predicate += SEMA_GENERIC_PARAM_ENV_SEPARATOR;
                predicate += capability_name(capability);
                predicates.push_back(std::move(predicate));
            }
        }
    }

    std::vector<std::string_view> predicate_views;
    predicate_views.reserve(predicates.size());
    for (const std::string& predicate : predicates) {
        predicate_views.push_back(predicate);
    }
    return query::param_env_key(predicate_views);
}

base::Result<SemanticAnalyzerCore::GenericInstanceIdentity> SemanticAnalyzerCore::generic_instance_identity(
    const GenericTemplateInfo& info, const std::span<const TypeHandle> args, const query::DefNamespace name_space) const
{
    const query::DefKey template_key = this->generic_template_query_key(info, name_space);
    GenericInstanceCanonicalResolver resolver(*this, info, template_key);
    std::vector<query::CanonicalTypeKey> canonical_args;
    canonical_args.reserve(args.size());
    for (const TypeHandle arg : args) {
        base::Result<query::CanonicalTypeKey> canonical_arg =
            build_canonical_type_key(this->state_.checked.types, arg, resolver);
        if (!canonical_arg) {
            return base::Result<GenericInstanceIdentity>::fail({
                base::ErrorCode::internal_error,
                std::string(SEMA_GENERIC_INSTANCE_QUERY_KEY_ERROR) + canonical_arg.error().message,
            });
        }
        canonical_args.push_back(canonical_arg.take_value());
    }

    query::GenericInstanceKey key = query::generic_instance_key(template_key, canonical_args,
        std::span<const query::StableFingerprint128>{}, this->generic_param_env_key(info));
    std::string fingerprint_text(SEMA_GENERIC_INSTANCE_FINGERPRINT_PREFIX);
    fingerprint_text += query::debug_string(query::stable_key_fingerprint(key));
    return base::Result<GenericInstanceIdentity>::ok(GenericInstanceIdentity{
        std::move(key),
        std::move(fingerprint_text),
    });
}

base::Result<std::string> SemanticAnalyzerCore::generic_instance_signature_fingerprint(const GenericTemplateInfo& info,
    const GenericInstanceIdentity& identity, const TypeHandle return_type,
    const std::span<const TypeHandle> param_types, const bool is_method, const bool is_variadic) const
{
    GenericInstanceCanonicalResolver resolver(*this, info, identity.key.template_def);
    query::StableKeyWriter writer;
    writer.write_u64(SEMA_GENERIC_INSTANCE_SIGNATURE_KEY_MARKER);
    query::append_stable_key(writer, identity.key);
    writer.write_bool(is_method);
    writer.write_bool(is_variadic);

    const auto append_type = [&](const TypeHandle type) -> base::Result<void> {
        return append_canonical_type_key(writer, this->state_.checked.types, type, resolver);
    };

    base::Result<void> return_result = append_type(return_type);
    if (!return_result) {
        return base::Result<std::string>::fail({
            base::ErrorCode::internal_error,
            std::string(SEMA_GENERIC_SIGNATURE_QUERY_KEY_ERROR) + return_result.error().message,
        });
    }

    writer.write_u64(static_cast<base::u64>(param_types.size()));
    for (const TypeHandle param_type : param_types) {
        base::Result<void> param_result = append_type(param_type);
        if (!param_result) {
            return base::Result<std::string>::fail({
                base::ErrorCode::internal_error,
                std::string(SEMA_GENERIC_SIGNATURE_QUERY_KEY_ERROR) + param_result.error().message,
            });
        }
    }

    std::string fingerprint(SEMA_GENERIC_SIGNATURE_FINGERPRINT_PREFIX);
    fingerprint += query::debug_string(writer.fingerprint());
    return base::Result<std::string>::ok(std::move(fingerprint));
}

base::Result<std::string> SemanticAnalyzerCore::generic_struct_instance_signature_fingerprint(
    const GenericTemplateInfo& info, const GenericInstanceIdentity& identity, const StructInfo& struct_info) const
{
    GenericInstanceCanonicalResolver resolver(*this, info, identity.key.template_def);
    query::StableKeyWriter writer;
    writer.write_u64(SEMA_GENERIC_STRUCT_SIGNATURE_KEY_MARKER);
    query::append_stable_key(writer, identity.key);
    writer.write_u8(static_cast<base::u8>(struct_info.visibility));
    writer.write_bool(struct_info.is_opaque);
    writer.write_bool(is_valid(struct_info.type));
    writer.write_u64(static_cast<base::u64>(struct_info.fields.size()));

    for (const StructFieldInfo& field : struct_info.fields) {
        writer.write_bool(query::is_valid(field.stable_key));
        if (query::is_valid(field.stable_key)) {
            query::append_stable_key(writer, field.stable_key);
        }
        writer.write_u8(static_cast<base::u8>(field.visibility));
        base::Result<void> field_type_result =
            append_canonical_type_key(writer, this->state_.checked.types, field.type, resolver);
        if (!field_type_result) {
            return base::Result<std::string>::fail({
                base::ErrorCode::internal_error,
                std::string(SEMA_GENERIC_STRUCT_SIGNATURE_QUERY_KEY_ERROR) + field_type_result.error().message,
            });
        }
    }

    std::string fingerprint(SEMA_GENERIC_STRUCT_SIGNATURE_FINGERPRINT_PREFIX);
    fingerprint += query::debug_string(writer.fingerprint());
    return base::Result<std::string>::ok(std::move(fingerprint));
}

base::Result<std::string> SemanticAnalyzerCore::generic_enum_instance_signature_fingerprint(
    const GenericTemplateInfo& info, const GenericInstanceIdentity& identity, const TypeHandle enum_type) const
{
    GenericInstanceCanonicalResolver resolver(*this, info, identity.key.template_def);
    query::StableKeyWriter writer;
    writer.write_u64(SEMA_GENERIC_ENUM_SIGNATURE_KEY_MARKER);
    query::append_stable_key(writer, identity.key);

    if (is_valid(enum_type) && enum_type.value >= this->state_.checked.types.size()) {
        return base::Result<std::string>::fail({
            base::ErrorCode::internal_error,
            std::string(SEMA_GENERIC_ENUM_SIGNATURE_QUERY_KEY_ERROR) + "unknown type handle",
        });
    }
    if (is_valid(enum_type)) {
        const TypeInfo& enum_info = this->state_.checked.types.get(enum_type);
        writer.write_bool(true);
        writer.write_u64(enum_info.enum_payload_size);
        writer.write_u64(enum_info.enum_payload_align);
        writer.write_bool(enum_info.contains_array);
        base::Result<void> underlying_result =
            append_canonical_type_key(writer, this->state_.checked.types, enum_info.enum_underlying, resolver);
        if (!underlying_result) {
            return base::Result<std::string>::fail({
                base::ErrorCode::internal_error,
                std::string(SEMA_GENERIC_ENUM_SIGNATURE_QUERY_KEY_ERROR) + underlying_result.error().message,
            });
        }
        base::Result<void> storage_result =
            append_canonical_type_key(writer, this->state_.checked.types, enum_info.enum_payload_storage, resolver);
        if (!storage_result) {
            return base::Result<std::string>::fail({
                base::ErrorCode::internal_error,
                std::string(SEMA_GENERIC_ENUM_SIGNATURE_QUERY_KEY_ERROR) + storage_result.error().message,
            });
        }
    } else {
        writer.write_bool(false);
    }

    std::vector<const EnumCaseInfo*> cases;
    cases.reserve(this->state_.checked.enum_cases.size());
    for (const auto& entry : this->state_.checked.enum_cases) {
        const EnumCaseInfo& enum_case = entry.second;
        if (enum_case.type.value == enum_type.value && enum_case.generic_instance_key == identity.key) {
            cases.push_back(&enum_case);
        }
    }
    std::ranges::sort(cases, [](const EnumCaseInfo* lhs, const EnumCaseInfo* rhs) {
        if (lhs->stable_id.global_id != rhs->stable_id.global_id) {
            return lhs->stable_id.global_id < rhs->stable_id.global_id;
        }
        return lhs->case_name.view() < rhs->case_name.view();
    });

    writer.write_u64(static_cast<base::u64>(cases.size()));
    for (const EnumCaseInfo* const enum_case : cases) {
        writer.write_bool(query::is_valid(enum_case->stable_id));
        if (query::is_valid(enum_case->stable_id)) {
            query::append_stable_key(writer, enum_case->stable_id);
        }
        writer.write_bool(query::is_valid(enum_case->stable_case_key));
        if (query::is_valid(enum_case->stable_case_key)) {
            query::append_stable_key(writer, enum_case->stable_case_key);
        }
        writer.write_u8(static_cast<base::u8>(enum_case->visibility));
        writer.write_string(enum_case->value_text.view());
        writer.write_u64(static_cast<base::u64>(enum_case->payload_types.size()));
        for (const TypeHandle payload_type : enum_case->payload_types) {
            base::Result<void> payload_result =
                append_canonical_type_key(writer, this->state_.checked.types, payload_type, resolver);
            if (!payload_result) {
                return base::Result<std::string>::fail({
                    base::ErrorCode::internal_error,
                    std::string(SEMA_GENERIC_ENUM_SIGNATURE_QUERY_KEY_ERROR) + payload_result.error().message,
                });
            }
        }
        base::Result<void> payload_storage_result =
            append_canonical_type_key(writer, this->state_.checked.types, enum_case->payload_type, resolver);
        if (!payload_storage_result) {
            return base::Result<std::string>::fail({
                base::ErrorCode::internal_error,
                std::string(SEMA_GENERIC_ENUM_SIGNATURE_QUERY_KEY_ERROR) + payload_storage_result.error().message,
            });
        }
    }

    std::string fingerprint(SEMA_GENERIC_ENUM_SIGNATURE_FINGERPRINT_PREFIX);
    fingerprint += query::debug_string(writer.fingerprint());
    return base::Result<std::string>::ok(std::move(fingerprint));
}

base::Result<std::string> SemanticAnalyzerCore::generic_type_alias_instance_signature_fingerprint(
    const GenericTemplateInfo& info, const GenericInstanceIdentity& identity, const TypeHandle target_type) const
{
    GenericInstanceCanonicalResolver resolver(*this, info, identity.key.template_def);
    query::StableKeyWriter writer;
    writer.write_u64(SEMA_GENERIC_TYPE_ALIAS_SIGNATURE_KEY_MARKER);
    query::append_stable_key(writer, identity.key);
    writer.write_bool(is_valid(target_type));
    if (is_valid(target_type)) {
        base::Result<query::CanonicalTypeKey> canonical_target =
            build_canonical_type_key(this->state_.checked.types, target_type, resolver);
        if (!canonical_target) {
            return base::Result<std::string>::fail({
                base::ErrorCode::internal_error,
                std::string(SEMA_GENERIC_TYPE_ALIAS_SIGNATURE_QUERY_KEY_ERROR) + canonical_target.error().message,
            });
        }
        query::append_stable_key(writer, canonical_target.value());
    }

    std::string fingerprint(SEMA_GENERIC_TYPE_ALIAS_SIGNATURE_FINGERPRINT_PREFIX);
    fingerprint += query::debug_string(writer.fingerprint());
    return base::Result<std::string>::ok(std::move(fingerprint));
}

} // namespace aurex::sema
