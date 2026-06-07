#pragma once

#include <aurex/infrastructure/query/query_key.hpp>

#include <optional>
#include <string_view>

namespace aurex::query {

struct DecodedLexFileKeyIdentity {
    std::string_view file;
    std::string_view lex_config;
};

struct DecodedParseFileKeyIdentity {
    std::string_view file;
    std::string_view lex_config;
};

struct DecodedDefKeyIdentity {
    std::string_view module;
};

struct DecodedModulePartKeyIdentity {
    std::string_view module;
    std::string_view file;
};

struct DecodedBodyKeyIdentity {
    std::string_view owner;
};

struct DecodedGenericInstanceKeyIdentity {
    std::string_view template_def;
};

struct DecodedTraitObjectTypeKeyIdentity {
    std::string_view principal_trait;
};

struct DecodedVTableLayoutKeyIdentity {
    std::string_view concrete_type;
    std::string_view object_type;
};

struct DecodedTraitObjectCoercionKeyIdentity {
    std::string_view source_type;
    std::string_view target_object_type;
    std::string_view vtable_layout;
};

[[nodiscard]] bool stable_key_has_file_key_layout(std::string_view bytes) noexcept;
[[nodiscard]] bool stable_key_has_project_key_layout(std::string_view bytes) noexcept;
[[nodiscard]] bool stable_key_has_module_key_layout(std::string_view bytes) noexcept;
[[nodiscard]] bool stable_key_has_module_part_key_layout(std::string_view bytes) noexcept;
[[nodiscard]] bool stable_key_has_body_key_layout(std::string_view bytes) noexcept;
[[nodiscard]] bool stable_key_has_generic_instance_key_layout(std::string_view bytes) noexcept;
[[nodiscard]] bool stable_key_has_trait_object_type_key_layout(std::string_view bytes) noexcept;
[[nodiscard]] bool stable_key_has_vtable_layout_key_layout(std::string_view bytes) noexcept;
[[nodiscard]] bool stable_key_has_trait_object_coercion_key_layout(std::string_view bytes) noexcept;
[[nodiscard]] bool stable_key_has_query_key_layout(std::string_view bytes) noexcept;
[[nodiscard]] bool stable_key_layout_matches_query_kind(QueryKind kind, std::string_view bytes) noexcept;
[[nodiscard]] std::optional<DecodedLexFileKeyIdentity> decode_lex_file_key_identity(std::string_view bytes) noexcept;
[[nodiscard]] std::optional<DecodedParseFileKeyIdentity> decode_parse_file_key_identity(
    std::string_view bytes) noexcept;
[[nodiscard]] std::optional<DecodedModulePartKeyIdentity> decode_module_part_key_identity(
    std::string_view bytes) noexcept;
[[nodiscard]] std::optional<DecodedDefKeyIdentity> decode_def_key_identity(std::string_view bytes) noexcept;
[[nodiscard]] std::optional<DecodedBodyKeyIdentity> decode_body_key_identity(std::string_view bytes) noexcept;
[[nodiscard]] std::optional<DecodedGenericInstanceKeyIdentity> decode_generic_instance_key_identity(
    std::string_view bytes) noexcept;
[[nodiscard]] std::optional<DecodedTraitObjectTypeKeyIdentity> decode_trait_object_type_key_identity(
    std::string_view bytes) noexcept;
[[nodiscard]] std::optional<DecodedVTableLayoutKeyIdentity> decode_vtable_layout_key_identity(
    std::string_view bytes) noexcept;
[[nodiscard]] std::optional<DecodedTraitObjectCoercionKeyIdentity> decode_trait_object_coercion_key_identity(
    std::string_view bytes) noexcept;

} // namespace aurex::query
