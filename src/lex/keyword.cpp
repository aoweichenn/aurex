#include "keyword.hpp"

#include <array>

namespace aurex::lex {

namespace {

struct KeywordEntry final {
    std::string_view text;
    syntax::TokenKind kind;
};

inline constexpr std::array keyword_entries {
    KeywordEntry {"c", syntax::TokenKind::kw_c},

    KeywordEntry {"as", syntax::TokenKind::kw_as},
    KeywordEntry {"fn", syntax::TokenKind::kw_fn},
    KeywordEntry {"if", syntax::TokenKind::kw_if},
    KeywordEntry {"i8", syntax::TokenKind::kw_i8},
    KeywordEntry {"u8", syntax::TokenKind::kw_u8},

    KeywordEntry {"pub", syntax::TokenKind::kw_pub},
    KeywordEntry {"let", syntax::TokenKind::kw_let},
    KeywordEntry {"var", syntax::TokenKind::kw_var},
    KeywordEntry {"for", syntax::TokenKind::kw_for},
    KeywordEntry {"i16", syntax::TokenKind::kw_i16},
    KeywordEntry {"u16", syntax::TokenKind::kw_u16},
    KeywordEntry {"i32", syntax::TokenKind::kw_i32},
    KeywordEntry {"u32", syntax::TokenKind::kw_u32},
    KeywordEntry {"i64", syntax::TokenKind::kw_i64},
    KeywordEntry {"u64", syntax::TokenKind::kw_u64},
    KeywordEntry {"f32", syntax::TokenKind::kw_f32},
    KeywordEntry {"f64", syntax::TokenKind::kw_f64},
    KeywordEntry {"str", syntax::TokenKind::kw_str},
    KeywordEntry {"mut", syntax::TokenKind::kw_mut},

    KeywordEntry {"priv", syntax::TokenKind::kw_priv},
    KeywordEntry {"enum", syntax::TokenKind::kw_enum},
    KeywordEntry {"type", syntax::TokenKind::kw_type},
    KeywordEntry {"impl", syntax::TokenKind::kw_impl},
    KeywordEntry {"else", syntax::TokenKind::kw_else},
    KeywordEntry {"move", syntax::TokenKind::kw_move},
    KeywordEntry {"true", syntax::TokenKind::kw_true},
    KeywordEntry {"null", syntax::TokenKind::kw_null},
    KeywordEntry {"void", syntax::TokenKind::kw_void},
    KeywordEntry {"bool", syntax::TokenKind::kw_bool},
    KeywordEntry {"cast", syntax::TokenKind::kw_cast},

    KeywordEntry {"const", syntax::TokenKind::kw_const},
    KeywordEntry {"match", syntax::TokenKind::kw_match},
    KeywordEntry {"while", syntax::TokenKind::kw_while},
    KeywordEntry {"break", syntax::TokenKind::kw_break},
    KeywordEntry {"defer", syntax::TokenKind::kw_defer},
    KeywordEntry {"false", syntax::TokenKind::kw_false},
    KeywordEntry {"isize", syntax::TokenKind::kw_isize},
    KeywordEntry {"usize", syntax::TokenKind::kw_usize},

    KeywordEntry {"module", syntax::TokenKind::kw_module},
    KeywordEntry {"import", syntax::TokenKind::kw_import},
    KeywordEntry {"extern", syntax::TokenKind::kw_extern},
    KeywordEntry {"export", syntax::TokenKind::kw_export},
    KeywordEntry {"struct", syntax::TokenKind::kw_struct},
    KeywordEntry {"opaque", syntax::TokenKind::kw_opaque},
    KeywordEntry {"return", syntax::TokenKind::kw_return},

    KeywordEntry {"noncopy", syntax::TokenKind::kw_noncopy},
    KeywordEntry {"size_of", syntax::TokenKind::kw_size_of},

    KeywordEntry {"continue", syntax::TokenKind::kw_continue},
    KeywordEntry {"ptr_cast", syntax::TokenKind::kw_ptr_cast},
    KeywordEntry {"bit_cast", syntax::TokenKind::kw_bit_cast},
    KeywordEntry {"align_of", syntax::TokenKind::kw_align_of},
    KeywordEntry {"ptr_addr", syntax::TokenKind::kw_ptr_addr},
    KeywordEntry {"str_data", syntax::TokenKind::kw_str_data},

    KeywordEntry {"str_byte_len", syntax::TokenKind::kw_str_byte_len},

    KeywordEntry {"ptr_from_addr", syntax::TokenKind::kw_ptr_from_addr},

    KeywordEntry {"str_from_bytes_unchecked", syntax::TokenKind::kw_str_from_bytes_unchecked},
};

struct KeywordBucket final {
    base::usize begin {};
    base::usize end {};
};

[[nodiscard]] consteval base::usize compute_max_keyword_text_length() noexcept {
    base::usize max_length = 0;
    for (const KeywordEntry& entry : keyword_entries) {
        if (entry.text.size() > max_length) {
            max_length = entry.text.size();
        }
    }
    return max_length;
}

[[nodiscard]] consteval bool keyword_entries_are_sorted_by_length() noexcept {
    for (base::usize index = 1; index < keyword_entries.size(); ++index) {
        if (keyword_entries[index - 1].text.size() > keyword_entries[index].text.size()) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] consteval bool keyword_entries_are_unique() noexcept {
    for (base::usize lhs = 0; lhs < keyword_entries.size(); ++lhs) {
        for (base::usize rhs = lhs + 1; rhs < keyword_entries.size(); ++rhs) {
            if (keyword_entries[lhs].text == keyword_entries[rhs].text) {
                return false;
            }
        }
    }
    return true;
}

inline constexpr base::usize max_keyword_text_length = compute_max_keyword_text_length();
inline constexpr base::usize empty_keyword_bucket_count = 1;
inline constexpr base::usize keyword_bucket_count = max_keyword_text_length + empty_keyword_bucket_count;

static_assert(keyword_entries_are_sorted_by_length(), "keyword entries must stay sorted by keyword text length");
static_assert(keyword_entries_are_unique(), "keyword entries contain duplicate keyword text");

[[nodiscard]] consteval std::array<KeywordBucket, keyword_bucket_count> build_keyword_buckets() noexcept {
    std::array<KeywordBucket, keyword_bucket_count> buckets {};
    base::usize cursor = 0;
    for (base::usize length = 0; length < buckets.size(); ++length) {
        while (cursor < keyword_entries.size() && keyword_entries[cursor].text.size() < length) {
            ++cursor;
        }

        base::usize end = cursor;
        while (end < keyword_entries.size() && keyword_entries[end].text.size() == length) {
            ++end;
        }

        buckets[length] = KeywordBucket {cursor, end};
    }
    return buckets;
}

inline constexpr std::array keyword_buckets = build_keyword_buckets();

} // namespace

syntax::TokenKind keyword_kind(const std::string_view text) noexcept {
    if (text.size() >= keyword_buckets.size()) {
        return syntax::TokenKind::identifier;
    }

    const KeywordBucket bucket = keyword_buckets[text.size()];
    for (base::usize index = bucket.begin; index < bucket.end; ++index) {
        const KeywordEntry& keyword = keyword_entries[index];
        if (text == keyword.text) {
            return keyword.kind;
        }
    }
    return syntax::TokenKind::identifier;
}

} // namespace aurex::lex
