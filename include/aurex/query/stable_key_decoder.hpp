#pragma once

#include <aurex/query/query_key.hpp>

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

[[nodiscard]] bool stable_key_has_file_key_layout(std::string_view bytes) noexcept;
[[nodiscard]] bool stable_key_has_project_key_layout(std::string_view bytes) noexcept;
[[nodiscard]] bool stable_key_has_module_key_layout(std::string_view bytes) noexcept;
[[nodiscard]] bool stable_key_has_module_part_key_layout(std::string_view bytes) noexcept;
[[nodiscard]] bool stable_key_has_body_key_layout(std::string_view bytes) noexcept;
[[nodiscard]] bool stable_key_has_generic_instance_key_layout(std::string_view bytes) noexcept;
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

} // namespace aurex::query
