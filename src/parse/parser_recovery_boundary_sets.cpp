#include "parser_recovery_sets.hpp"

namespace aurex::parse::detail {

using syntax::TokenKind;

bool token_ends_type_argument(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::greater:
    case TokenKind::greater_greater:
    case TokenKind::comma:
    case TokenKind::semicolon:
    case TokenKind::r_paren:
    case TokenKind::r_bracket:
    case TokenKind::r_brace:
        return true;
    default:
        return false;
    }
}

bool token_ends_match_arm(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::comma:
    case TokenKind::r_brace:
        return true;
    default:
        return false;
    }
}

bool token_ends_call_argument(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::comma:
    case TokenKind::r_paren:
    case TokenKind::semicolon:
    case TokenKind::r_bracket:
    case TokenKind::r_brace:
        return true;
    default:
        return false;
    }
}

bool token_ends_struct_field(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::comma:
    case TokenKind::r_brace:
    case TokenKind::semicolon:
    case TokenKind::r_paren:
    case TokenKind::r_bracket:
        return true;
    default:
        return false;
    }
}

bool token_ends_parameter(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::comma:
    case TokenKind::r_paren:
    case TokenKind::arrow:
    case TokenKind::l_brace:
    case TokenKind::semicolon:
    case TokenKind::r_brace:
        return true;
    default:
        return false;
    }
}

bool token_ends_struct_decl_field(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::semicolon:
    case TokenKind::comma:
    case TokenKind::r_brace:
    case TokenKind::kw_fn:
    case TokenKind::kw_struct:
    case TokenKind::kw_enum:
    case TokenKind::kw_impl:
    case TokenKind::kw_extern:
    case TokenKind::kw_export:
        return true;
    default:
        return false;
    }
}

bool token_ends_enum_case(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::comma:
    case TokenKind::semicolon:
    case TokenKind::r_brace:
    case TokenKind::kw_fn:
    case TokenKind::kw_struct:
    case TokenKind::kw_enum:
    case TokenKind::kw_impl:
    case TokenKind::kw_extern:
    case TokenKind::kw_export:
        return true;
    default:
        return false;
    }
}

bool token_ends_generic_parameter(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::greater:
    case TokenKind::comma:
    case TokenKind::l_paren:
    case TokenKind::r_paren:
    case TokenKind::l_brace:
    case TokenKind::colon:
    case TokenKind::semicolon:
    case TokenKind::r_brace:
        return true;
    default:
        return false;
    }
}

bool token_matches_abi_attribute_argument(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::string_literal:
    case TokenKind::r_paren:
    case TokenKind::l_brace:
    case TokenKind::semicolon:
    case TokenKind::r_brace:
        return true;
    default:
        return false;
    }
}

bool token_ends_builtin_argument(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::comma:
    case TokenKind::r_paren:
    case TokenKind::semicolon:
    case TokenKind::r_bracket:
    case TokenKind::r_brace:
        return true;
    default:
        return false;
    }
}

bool token_matches_path_segment_boundary(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::dot:
    case TokenKind::semicolon:
    case TokenKind::kw_as:
    case TokenKind::l_brace:
    case TokenKind::r_brace:
        return true;
    default:
        return false;
    }
}

bool token_matches_import_alias_boundary(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::semicolon:
    case TokenKind::r_brace:
        return true;
    default:
        return false;
    }
}

bool token_matches_statement_terminator_boundary(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::semicolon:
    case TokenKind::r_brace:
        return true;
    default:
        return false;
    }
}

bool token_matches_for_clause_separator_boundary(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::semicolon:
    case TokenKind::l_brace:
    case TokenKind::r_brace:
        return true;
    default:
        return false;
    }
}

bool token_matches_block_start_boundary(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::l_brace:
    case TokenKind::r_brace:
        return true;
    default:
        return false;
    }
}

} // namespace aurex::parse::detail
