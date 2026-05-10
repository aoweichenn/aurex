#pragma once

#include <aurex/parse/recovery_context.hpp>
#include <aurex/syntax/token.hpp>

namespace aurex::parse {

[[nodiscard]] bool token_starts_match_arm(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_starts_struct_field(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_starts_parameter(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_starts_struct_decl_field(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_starts_enum_case(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_starts_generic_parameter(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_starts_path_segment(syntax::TokenKind kind) noexcept;

[[nodiscard]] bool token_matches_recovery_context(
    syntax::TokenKind kind,
    RecoveryContext context
) noexcept;

} // namespace aurex::parse
