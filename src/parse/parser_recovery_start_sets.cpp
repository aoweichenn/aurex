#include "parser_recovery_sets.hpp"

#include "aurex/parse/recovery.hpp"

namespace aurex::parse::detail {

using syntax::TokenKind;

bool token_starts_item(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::r_brace:
    case TokenKind::kw_fn:
    case TokenKind::kw_struct:
    case TokenKind::kw_enum:
    case TokenKind::kw_impl:
    case TokenKind::kw_opaque:
    case TokenKind::kw_const:
    case TokenKind::kw_type:
    case TokenKind::kw_pub:
    case TokenKind::kw_priv:
    case TokenKind::kw_extern:
    case TokenKind::kw_export:
    case TokenKind::kw_noncopy:
    case TokenKind::kw_import:
        return true;
    default:
        return false;
    }
}

bool token_starts_expression(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::identifier:
    case TokenKind::integer_literal:
    case TokenKind::float_literal:
    case TokenKind::string_literal:
    case TokenKind::c_string_literal:
    case TokenKind::byte_literal:
    case TokenKind::kw_if:
    case TokenKind::kw_match:
    case TokenKind::kw_move:
    case TokenKind::kw_true:
    case TokenKind::kw_false:
    case TokenKind::kw_null:
    case TokenKind::kw_cast:
    case TokenKind::kw_ptr_cast:
    case TokenKind::kw_bit_cast:
    case TokenKind::kw_size_of:
    case TokenKind::kw_align_of:
    case TokenKind::kw_ptr_addr:
    case TokenKind::kw_ptr_from_addr:
    case TokenKind::kw_str_data:
    case TokenKind::kw_str_byte_len:
    case TokenKind::kw_str_from_bytes_unchecked:
    case TokenKind::l_paren:
    case TokenKind::l_brace:
    case TokenKind::minus:
    case TokenKind::star:
    case TokenKind::amp:
    case TokenKind::tilde:
    case TokenKind::bang:
        return true;
    default:
        return false;
    }
}

bool token_starts_statement(const TokenKind kind) noexcept {
    if (token_starts_expression(kind)) {
        return true;
    }

    switch (kind) {
    case TokenKind::r_brace:
    case TokenKind::kw_let:
    case TokenKind::kw_var:
    case TokenKind::kw_for:
    case TokenKind::kw_while:
    case TokenKind::kw_break:
    case TokenKind::kw_continue:
    case TokenKind::kw_defer:
    case TokenKind::kw_return:
        return true;
    default:
        return false;
    }
}

bool token_starts_non_expression_statement(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::kw_let:
    case TokenKind::kw_var:
    case TokenKind::kw_if:
    case TokenKind::kw_for:
    case TokenKind::kw_while:
    case TokenKind::kw_break:
    case TokenKind::kw_continue:
    case TokenKind::kw_defer:
    case TokenKind::kw_return:
        return true;
    default:
        return false;
    }
}

bool token_starts_type(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::identifier:
    case TokenKind::star:
    case TokenKind::l_bracket:
    case TokenKind::kw_void:
    case TokenKind::kw_bool:
    case TokenKind::kw_i8:
    case TokenKind::kw_u8:
    case TokenKind::kw_i16:
    case TokenKind::kw_u16:
    case TokenKind::kw_i32:
    case TokenKind::kw_u32:
    case TokenKind::kw_i64:
    case TokenKind::kw_u64:
    case TokenKind::kw_isize:
    case TokenKind::kw_usize:
    case TokenKind::kw_f32:
    case TokenKind::kw_f64:
    case TokenKind::kw_str:
        return true;
    default:
        return false;
    }
}

} // namespace aurex::parse::detail

namespace aurex::parse {

using syntax::TokenKind;

bool token_starts_match_arm(const TokenKind kind) noexcept {
    switch (kind) {
    case TokenKind::identifier:
    case TokenKind::integer_literal:
    case TokenKind::kw_true:
    case TokenKind::kw_false:
    case TokenKind::dot:
        return true;
    default:
        return false;
    }
}

bool token_starts_struct_field(const TokenKind kind) noexcept {
    return kind == TokenKind::identifier;
}

bool token_starts_parameter(const TokenKind kind) noexcept {
    return kind == TokenKind::identifier || kind == TokenKind::ellipsis;
}

bool token_starts_struct_decl_field(const TokenKind kind) noexcept {
    return kind == TokenKind::identifier ||
           kind == TokenKind::kw_pub ||
           kind == TokenKind::kw_priv;
}

bool token_starts_enum_case(const TokenKind kind) noexcept {
    return kind == TokenKind::identifier;
}

bool token_starts_generic_parameter(const TokenKind kind) noexcept {
    return kind == TokenKind::identifier;
}

bool token_starts_path_segment(const TokenKind kind) noexcept {
    return kind == TokenKind::identifier ||
           kind == TokenKind::kw_c ||
           kind == TokenKind::kw_str;
}

} // namespace aurex::parse
