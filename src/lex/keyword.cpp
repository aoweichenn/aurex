#include <lex/keyword.hpp>

#include <array>

namespace aurex::lex {

namespace {

struct KeywordEntry final {
    std::string_view text;
    syntax::TokenKind kind;
};

inline constexpr std::array KEYWORD_ENTRIES {
    KeywordEntry {"c", syntax::TokenKind::kw_c},

    KeywordEntry {"as", syntax::TokenKind::kw_as},
    KeywordEntry {"fn", syntax::TokenKind::kw_fn},
    KeywordEntry {"if", syntax::TokenKind::kw_if},
    KeywordEntry {"in", syntax::TokenKind::kw_in},
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
    KeywordEntry {"true", syntax::TokenKind::kw_true},
    KeywordEntry {"null", syntax::TokenKind::kw_null},
    KeywordEntry {"void", syntax::TokenKind::kw_void},
    KeywordEntry {"bool", syntax::TokenKind::kw_bool},
    KeywordEntry {"cast", syntax::TokenKind::kw_cast},
    KeywordEntry {"pcast", syntax::TokenKind::kw_pcast},

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

    KeywordEntry {"size_of", syntax::TokenKind::kw_size_of},

    KeywordEntry {"continue", syntax::TokenKind::kw_continue},
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

inline constexpr base::usize KEYWORD_ASCII_FIRST_CHAR_BUCKET_COUNT = 128;

[[nodiscard]] consteval base::usize compute_max_keyword_text_length() noexcept {
    base::usize max_length = 0;
    for (const KeywordEntry& entry : KEYWORD_ENTRIES) {
        if (entry.text.size() > max_length) {
            max_length = entry.text.size();
        }
    }
    return max_length;
}

[[nodiscard]] constexpr base::usize first_char_bucket(const char c) noexcept {
    return static_cast<unsigned char>(c);
}

[[nodiscard]] constexpr bool keyword_entry_less(const KeywordEntry& lhs, const KeywordEntry& rhs) noexcept {
    if (lhs.text.size() != rhs.text.size()) {
        return lhs.text.size() < rhs.text.size();
    }
    const base::usize lhs_bucket = first_char_bucket(lhs.text.front());
    const base::usize rhs_bucket = first_char_bucket(rhs.text.front());
    if (lhs_bucket != rhs_bucket) {
        return lhs_bucket < rhs_bucket;
    }
    return lhs.text < rhs.text;
}

[[nodiscard]] consteval bool keyword_entries_are_nonempty() noexcept {
    for (const KeywordEntry& entry : KEYWORD_ENTRIES) {
        if (entry.text.empty()) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] consteval bool keyword_first_chars_are_ascii() noexcept {
    for (const KeywordEntry& entry : KEYWORD_ENTRIES) {
        if (first_char_bucket(entry.text.front()) >= KEYWORD_ASCII_FIRST_CHAR_BUCKET_COUNT) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] consteval bool keyword_entries_are_unique() noexcept {
    for (base::usize lhs = 0; lhs < KEYWORD_ENTRIES.size(); ++lhs) {
        for (base::usize rhs = lhs + 1; rhs < KEYWORD_ENTRIES.size(); ++rhs) {
            if (KEYWORD_ENTRIES[lhs].text == KEYWORD_ENTRIES[rhs].text) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] consteval std::array<KeywordEntry, KEYWORD_ENTRIES.size()> build_sorted_keyword_entries() noexcept {
    std::array<KeywordEntry, KEYWORD_ENTRIES.size()> sorted = KEYWORD_ENTRIES;
    for (base::usize cursor = 0; cursor < sorted.size(); ++cursor) {
        base::usize selected = cursor;
        for (base::usize candidate = cursor + 1; candidate < sorted.size(); ++candidate) {
            if (keyword_entry_less(sorted[candidate], sorted[selected])) {
                selected = candidate;
            }
        }
        if (selected != cursor) {
            const KeywordEntry selected_entry = sorted[selected];
            sorted[selected] = sorted[cursor];
            sorted[cursor] = selected_entry;
        }
    }
    return sorted;
}

[[nodiscard]] consteval bool keyword_entries_are_sorted_by_bucket(
    const std::array<KeywordEntry, KEYWORD_ENTRIES.size()>& entries
) noexcept {
    for (base::usize index = 1; index < entries.size(); ++index) {
        if (keyword_entry_less(entries[index], entries[index - 1])) {
            return false;
        }
    }
    return true;
}

inline constexpr base::usize KEYWORD_MAX_TEXT_LENGTH = compute_max_keyword_text_length();
inline constexpr base::usize KEYWORD_EMPTY_BUCKET_COUNT = 1;
inline constexpr base::usize KEYWORD_BUCKET_COUNT = KEYWORD_MAX_TEXT_LENGTH + KEYWORD_EMPTY_BUCKET_COUNT;
inline constexpr std::array SORTED_KEYWORD_ENTRIES = build_sorted_keyword_entries();

using KeywordFirstCharBuckets = std::array<KeywordBucket, KEYWORD_ASCII_FIRST_CHAR_BUCKET_COUNT>;
using KeywordBuckets = std::array<KeywordFirstCharBuckets, KEYWORD_BUCKET_COUNT>;

static_assert(keyword_entries_are_nonempty(), "keyword entries must not contain empty keyword text");
static_assert(keyword_first_chars_are_ascii(), "keyword entries must start with an ASCII character");
static_assert(keyword_entries_are_unique(), "keyword entries contain duplicate keyword text");
static_assert(
    keyword_entries_are_sorted_by_bucket(SORTED_KEYWORD_ENTRIES),
    "sorted keyword entries must stay sorted by keyword text length and first character"
);

[[nodiscard]] consteval KeywordBuckets build_keyword_buckets() noexcept {
    KeywordBuckets buckets {};
    base::usize cursor = 0;
    for (base::usize length = 0; length < buckets.size(); ++length) {
        for (base::usize first = 0; first < KEYWORD_ASCII_FIRST_CHAR_BUCKET_COUNT; ++first) {
            while (cursor < SORTED_KEYWORD_ENTRIES.size() &&
                   (SORTED_KEYWORD_ENTRIES[cursor].text.size() < length ||
                    (SORTED_KEYWORD_ENTRIES[cursor].text.size() == length &&
                     first_char_bucket(SORTED_KEYWORD_ENTRIES[cursor].text.front()) < first))) {
                ++cursor;
            }

            base::usize end = cursor;
            while (end < SORTED_KEYWORD_ENTRIES.size() &&
                   SORTED_KEYWORD_ENTRIES[end].text.size() == length &&
                   first_char_bucket(SORTED_KEYWORD_ENTRIES[end].text.front()) == first) {
                ++end;
            }

            buckets[length][first] = KeywordBucket {cursor, end};
            cursor = end;
        }
    }
    return buckets;
}

inline constexpr std::array KEYWORD_BUCKETS = build_keyword_buckets();

} // namespace

syntax::TokenKind keyword_kind(const std::string_view text) noexcept {
    if (text.empty() || text.size() >= KEYWORD_BUCKETS.size()) {
        return syntax::TokenKind::identifier;
    }

    const base::usize first = first_char_bucket(text.front());
    if (first >= KEYWORD_ASCII_FIRST_CHAR_BUCKET_COUNT) {
        return syntax::TokenKind::identifier;
    }

    const KeywordBucket bucket = KEYWORD_BUCKETS[text.size()][first];
    for (base::usize index = bucket.begin; index < bucket.end; ++index) {
        const KeywordEntry& keyword = SORTED_KEYWORD_ENTRIES[index];
        if (text == keyword.text) {
            return keyword.kind;
        }
    }
    return syntax::TokenKind::identifier;
}

} // namespace aurex::lex
