#pragma once

#include <aurex/base/source.hpp>

#include <string_view>

namespace aurex::syntax {

enum class TokenKind {
    eof,
    invalid,
    identifier,
    integer_literal,
    float_literal,
    string_literal,
    c_string_literal,
    raw_string_literal,
    byte_string_literal,
    byte_literal,
    char_literal,
    kw_module,
    kw_import,
    kw_as,
    kw_pub,
    kw_priv,
    kw_extern,
    kw_export,
    kw_unsafe,
    kw_fn,
    kw_struct,
    kw_opaque,
    kw_enum,
    kw_const,
    kw_type,
    kw_impl,
    kw_where,
    kw_match,
    kw_let,
    kw_var,
    kw_if,
    kw_else,
    kw_for,
    kw_in,
    kw_is,
    kw_while,
    kw_break,
    kw_continue,
    kw_defer,
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
    kw_char,
    kw_mut,
    kw_cast,
    kw_ptrcast,
    kw_bitcast,
    kw_sizeof,
    kw_alignof,
    kw_ptraddr,
    kw_ptrat,
    kw_strptr,
    kw_strblen,
    kw_strvalid,
    kw_strfromutf8,
    kw_strraw,
    l_paren,
    r_paren,
    l_brace,
    r_brace,
    l_bracket,
    r_bracket,
    comma,
    dot,
    ellipsis,
    semicolon,
    colon,
    colon_colon,
    arrow,
    fat_arrow,
    plus,
    plus_plus,
    plus_equal,
    minus,
    minus_minus,
    minus_equal,
    star,
    star_equal,
    slash,
    slash_equal,
    percent,
    percent_equal,
    amp,
    amp_equal,
    pipe,
    pipe_equal,
    caret,
    caret_equal,
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
    less_less_equal,
    greater_greater,
    greater_greater_equal,
    amp_amp,
    pipe_pipe,
    question,
    at,
};

struct Token {
    TokenKind kind = TokenKind::invalid;
    base::SourceRange range {};

    Token() = default;

    Token(
        const TokenKind token_kind,
        const base::SourceRange token_range,
        const std::string_view token_text
    ) noexcept
        : kind(token_kind),
          range(token_range),
          text_data_(token_text.empty() ? nullptr : token_text.data()) {}

    [[nodiscard]] std::string_view text() const noexcept {
        if (this->text_data_ == nullptr) {
            return {};
        }
        return std::string_view {this->text_data_, this->range.length()};
    }

private:
    const char* text_data_ = nullptr;
};

[[nodiscard]] std::string_view token_kind_name(TokenKind kind) noexcept;

} // namespace aurex::syntax
