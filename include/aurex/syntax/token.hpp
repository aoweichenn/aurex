#pragma once

#include "aurex/base/source.hpp"

#include <string_view>

namespace aurex::syntax {

enum class TokenKind {
    eof,
    invalid,
    identifier,
    integer_literal,
    string_literal,
    c_string_literal,
    byte_literal,
    kw_module,
    kw_import,
    kw_extern,
    kw_export,
    kw_c,
    kw_fn,
    kw_struct,
    kw_opaque,
    kw_enum,
    kw_const,
    kw_let,
    kw_var,
    kw_if,
    kw_else,
    kw_while,
    kw_break,
    kw_continue,
    kw_return,
    kw_true,
    kw_false,
    kw_null,
    kw_void,
    kw_bool,
    kw_i8,
    kw_u8,
    kw_i16,
    kw_u16,
    kw_i32,
    kw_u32,
    kw_i64,
    kw_u64,
    kw_isize,
    kw_usize,
    kw_f32,
    kw_f64,
    kw_str,
    kw_mut,
    kw_cast,
    kw_ptr_cast,
    kw_bit_cast,
    l_paren,
    r_paren,
    l_brace,
    r_brace,
    l_bracket,
    r_bracket,
    comma,
    dot,
    semicolon,
    colon,
    arrow,
    plus,
    minus,
    star,
    slash,
    percent,
    amp,
    pipe,
    caret,
    tilde,
    bang,
    equal,
    equal_equal,
    bang_equal,
    less,
    less_equal,
    greater,
    greater_equal,
    less_less,
    greater_greater,
    amp_amp,
    pipe_pipe,
    at,
};

struct Token {
    TokenKind kind = TokenKind::invalid;
    base::SourceRange range {};
    std::string_view text;
};

[[nodiscard]] std::string_view token_kind_name(TokenKind kind) noexcept;

} // namespace aurex::syntax
