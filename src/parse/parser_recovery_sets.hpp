#pragma once

#include <aurex/syntax/token.hpp>

namespace aurex::parse::detail {

[[nodiscard]] bool token_starts_item(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_starts_expression(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_starts_statement(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_starts_non_expression_statement(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_starts_type(syntax::TokenKind kind) noexcept;

[[nodiscard]] bool token_matches_identifier_boundary(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_ends_type_argument(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_ends_match_arm(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_ends_call_argument(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_ends_struct_field(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_ends_parameter(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_ends_struct_decl_field(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_ends_enum_case(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_ends_generic_parameter(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_matches_parameter_list_start_boundary(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_matches_abi_attribute_argument(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_matches_abi_attribute_start_boundary(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_ends_builtin_argument(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_matches_builtin_argument_list_start_boundary(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_matches_path_segment_boundary(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_matches_import_alias_boundary(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_matches_module_terminator_boundary(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_matches_statement_terminator_boundary(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_matches_for_clause_separator_boundary(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_matches_block_start_boundary(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_matches_type_annotation_boundary(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_matches_initializer_boundary(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_matches_match_arm_arrow_boundary(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_matches_if_else_boundary(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_matches_grouped_expression_boundary(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_matches_index_expression_boundary(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_matches_array_type_length_boundary(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_matches_pattern_payload_boundary(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_matches_enum_case_payload_boundary(syntax::TokenKind kind) noexcept;
[[nodiscard]] bool token_matches_item_terminator_boundary(syntax::TokenKind kind) noexcept;

} // namespace aurex::parse::detail
