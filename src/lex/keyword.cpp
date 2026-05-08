#include "keyword.hpp"

#include "lexeme.hpp"

namespace aurex::lex {

syntax::TokenKind keyword_kind(const std::string_view text) noexcept {
    using syntax::TokenKind;

    switch (text.size()) {
    case token_text_length("c"):
        if (text == "c") return TokenKind::kw_c;
        break;
    case token_text_length("as"):
        if (text == "as") return TokenKind::kw_as;
        if (text == "fn") return TokenKind::kw_fn;
        if (text == "if") return TokenKind::kw_if;
        if (text == "i8") return TokenKind::kw_i8;
        if (text == "u8") return TokenKind::kw_u8;
        break;
    case token_text_length("pub"):
        if (text == "pub") return TokenKind::kw_pub;
        if (text == "let") return TokenKind::kw_let;
        if (text == "var") return TokenKind::kw_var;
        if (text == "for") return TokenKind::kw_for;
        if (text == "i16") return TokenKind::kw_i16;
        if (text == "u16") return TokenKind::kw_u16;
        if (text == "i32") return TokenKind::kw_i32;
        if (text == "u32") return TokenKind::kw_u32;
        if (text == "i64") return TokenKind::kw_i64;
        if (text == "u64") return TokenKind::kw_u64;
        if (text == "f32") return TokenKind::kw_f32;
        if (text == "f64") return TokenKind::kw_f64;
        if (text == "str") return TokenKind::kw_str;
        if (text == "mut") return TokenKind::kw_mut;
        break;
    case token_text_length("priv"):
        if (text == "priv") return TokenKind::kw_priv;
        if (text == "enum") return TokenKind::kw_enum;
        if (text == "type") return TokenKind::kw_type;
        if (text == "impl") return TokenKind::kw_impl;
        if (text == "else") return TokenKind::kw_else;
        if (text == "move") return TokenKind::kw_move;
        if (text == "true") return TokenKind::kw_true;
        if (text == "null") return TokenKind::kw_null;
        if (text == "void") return TokenKind::kw_void;
        if (text == "bool") return TokenKind::kw_bool;
        if (text == "cast") return TokenKind::kw_cast;
        break;
    case token_text_length("const"):
        if (text == "const") return TokenKind::kw_const;
        if (text == "match") return TokenKind::kw_match;
        if (text == "while") return TokenKind::kw_while;
        if (text == "break") return TokenKind::kw_break;
        if (text == "defer") return TokenKind::kw_defer;
        if (text == "false") return TokenKind::kw_false;
        if (text == "isize") return TokenKind::kw_isize;
        if (text == "usize") return TokenKind::kw_usize;
        break;
    case token_text_length("module"):
        if (text == "module") return TokenKind::kw_module;
        if (text == "import") return TokenKind::kw_import;
        if (text == "extern") return TokenKind::kw_extern;
        if (text == "export") return TokenKind::kw_export;
        if (text == "struct") return TokenKind::kw_struct;
        if (text == "opaque") return TokenKind::kw_opaque;
        if (text == "return") return TokenKind::kw_return;
        break;
    case token_text_length("noncopy"):
        if (text == "noncopy") return TokenKind::kw_noncopy;
        if (text == "size_of") return TokenKind::kw_size_of;
        break;
    case token_text_length("continue"):
        if (text == "continue") return TokenKind::kw_continue;
        if (text == "ptr_cast") return TokenKind::kw_ptr_cast;
        if (text == "bit_cast") return TokenKind::kw_bit_cast;
        if (text == "align_of") return TokenKind::kw_align_of;
        if (text == "ptr_addr") return TokenKind::kw_ptr_addr;
        if (text == "str_data") return TokenKind::kw_str_data;
        break;
    case token_text_length("str_byte_len"):
        if (text == "str_byte_len") return TokenKind::kw_str_byte_len;
        break;
    case token_text_length("ptr_from_addr"):
        if (text == "ptr_from_addr") return TokenKind::kw_ptr_from_addr;
        break;
    case token_text_length("str_from_bytes_unchecked"):
        if (text == "str_from_bytes_unchecked") return TokenKind::kw_str_from_bytes_unchecked;
        break;
    default:
        break;
    }
    return TokenKind::identifier;
}

} // namespace aurex::lex
