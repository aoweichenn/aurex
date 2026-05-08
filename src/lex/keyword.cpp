#include "keyword.hpp"

#include <array>

namespace aurex::lex {

syntax::TokenKind keyword_kind(const std::string_view text) noexcept {
    using syntax::TokenKind;
    struct Entry {
        std::string_view text;
        TokenKind kind;
    };
    static constexpr auto entries = std::to_array<Entry>({
        Entry {"module", TokenKind::kw_module},
        Entry {"import", TokenKind::kw_import},
        Entry {"as", TokenKind::kw_as},
        Entry {"pub", TokenKind::kw_pub},
        Entry {"priv", TokenKind::kw_priv},
        Entry {"extern", TokenKind::kw_extern},
        Entry {"export", TokenKind::kw_export},
        Entry {"c", TokenKind::kw_c},
        Entry {"fn", TokenKind::kw_fn},
        Entry {"struct", TokenKind::kw_struct},
        Entry {"opaque", TokenKind::kw_opaque},
        Entry {"enum", TokenKind::kw_enum},
        Entry {"const", TokenKind::kw_const},
        Entry {"type", TokenKind::kw_type},
        Entry {"impl", TokenKind::kw_impl},
        Entry {"match", TokenKind::kw_match},
        Entry {"let", TokenKind::kw_let},
        Entry {"var", TokenKind::kw_var},
        Entry {"if", TokenKind::kw_if},
        Entry {"else", TokenKind::kw_else},
        Entry {"for", TokenKind::kw_for},
        Entry {"while", TokenKind::kw_while},
        Entry {"break", TokenKind::kw_break},
        Entry {"continue", TokenKind::kw_continue},
        Entry {"defer", TokenKind::kw_defer},
        Entry {"return", TokenKind::kw_return},
        Entry {"noncopy", TokenKind::kw_noncopy},
        Entry {"move", TokenKind::kw_move},
        Entry {"true", TokenKind::kw_true},
        Entry {"false", TokenKind::kw_false},
        Entry {"null", TokenKind::kw_null},
        Entry {"void", TokenKind::kw_void},
        Entry {"bool", TokenKind::kw_bool},
        Entry {"i8", TokenKind::kw_i8},
        Entry {"u8", TokenKind::kw_u8},
        Entry {"i16", TokenKind::kw_i16},
        Entry {"u16", TokenKind::kw_u16},
        Entry {"i32", TokenKind::kw_i32},
        Entry {"u32", TokenKind::kw_u32},
        Entry {"i64", TokenKind::kw_i64},
        Entry {"u64", TokenKind::kw_u64},
        Entry {"isize", TokenKind::kw_isize},
        Entry {"usize", TokenKind::kw_usize},
        Entry {"f32", TokenKind::kw_f32},
        Entry {"f64", TokenKind::kw_f64},
        Entry {"str", TokenKind::kw_str},
        Entry {"mut", TokenKind::kw_mut},
        Entry {"cast", TokenKind::kw_cast},
        Entry {"ptr_cast", TokenKind::kw_ptr_cast},
        Entry {"bit_cast", TokenKind::kw_bit_cast},
        Entry {"size_of", TokenKind::kw_size_of},
        Entry {"align_of", TokenKind::kw_align_of},
        Entry {"ptr_addr", TokenKind::kw_ptr_addr},
        Entry {"ptr_from_addr", TokenKind::kw_ptr_from_addr},
        Entry {"str_data", TokenKind::kw_str_data},
        Entry {"str_byte_len", TokenKind::kw_str_byte_len},
        Entry {"str_from_bytes_unchecked", TokenKind::kw_str_from_bytes_unchecked},
    });

    for (const Entry& entry : entries) {
        if (entry.text == text) {
            return entry.kind;
        }
    }
    return TokenKind::identifier;
}

} // namespace aurex::lex
