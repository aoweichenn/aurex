#include "parser_recovery_sets.hpp"

namespace aurex::parse::detail {

using syntax::TokenKind;

bool token_matches_identifier_boundary(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::identifier:
    case TokenKind::colon:
    case TokenKind::equal:
    case TokenKind::comma:
    case TokenKind::semicolon:
    case TokenKind::dot:
    case TokenKind::l_paren:
    case TokenKind::r_paren:
    case TokenKind::l_brace:
    case TokenKind::r_brace:
    case TokenKind::arrow:
    case TokenKind::fat_arrow:
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

bool token_matches_parameter_list_start_boundary(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::l_paren:
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

bool token_matches_abi_attribute_start_boundary(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::l_paren:
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

bool token_matches_builtin_argument_list_start_boundary(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::l_paren:
    case TokenKind::r_paren:
    case TokenKind::comma:
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

bool token_matches_module_terminator_boundary(const TokenKind kind) noexcept {
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

bool token_matches_type_annotation_boundary(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::colon:
    case TokenKind::equal:
    case TokenKind::comma:
    case TokenKind::semicolon:
    case TokenKind::r_paren:
    case TokenKind::l_brace:
    case TokenKind::r_brace:
    case TokenKind::arrow:
    case TokenKind::fat_arrow:
        return true;
    default:
        return false;
    }
}

bool token_matches_initializer_boundary(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::equal:
    case TokenKind::comma:
    case TokenKind::semicolon:
    case TokenKind::r_paren:
    case TokenKind::l_brace:
    case TokenKind::r_brace:
    case TokenKind::arrow:
    case TokenKind::fat_arrow:
        return true;
    default:
        return false;
    }
}

bool token_matches_match_arm_arrow_boundary(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::fat_arrow:
    case TokenKind::comma:
    case TokenKind::r_brace:
        return true;
    default:
        return false;
    }
}

bool token_matches_if_else_boundary(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::kw_else:
    case TokenKind::l_brace:
    case TokenKind::semicolon:
    case TokenKind::comma:
    case TokenKind::r_paren:
    case TokenKind::r_bracket:
    case TokenKind::r_brace:
        return true;
    default:
        return false;
    }
}

bool token_matches_grouped_expression_boundary(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::r_paren:
    case TokenKind::semicolon:
    case TokenKind::comma:
    case TokenKind::r_bracket:
    case TokenKind::r_brace:
        return true;
    default:
        return false;
    }
}

bool token_matches_index_expression_boundary(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::r_bracket:
    case TokenKind::semicolon:
    case TokenKind::comma:
    case TokenKind::r_paren:
    case TokenKind::r_brace:
        return true;
    default:
        return false;
    }
}

bool token_matches_array_type_length_boundary(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::r_bracket:
    case TokenKind::semicolon:
    case TokenKind::comma:
    case TokenKind::r_paren:
    case TokenKind::r_brace:
        return true;
    default:
        return false;
    }
}

bool token_matches_pattern_payload_boundary(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::r_paren:
    case TokenKind::comma:
    case TokenKind::fat_arrow:
    case TokenKind::r_brace:
        return true;
    default:
        return false;
    }
}

bool token_matches_enum_case_payload_boundary(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::r_paren:
    case TokenKind::equal:
    case TokenKind::comma:
    case TokenKind::semicolon:
    case TokenKind::r_brace:
        return true;
    default:
        return false;
    }
}

bool token_matches_item_terminator_boundary(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::semicolon:
    case TokenKind::r_brace:
        return true;
    default:
        return false;
    }
}

} // namespace aurex::parse::detail
