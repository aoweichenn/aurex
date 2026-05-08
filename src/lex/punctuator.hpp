#pragma once

#include "aurex/base/integer.hpp"
#include "aurex/syntax/token.hpp"

#include <array>
#include <optional>
#include <string_view>

namespace aurex::lex {

struct PunctuatorMatch final {
    std::string_view text;
    syntax::TokenKind kind;
};

namespace detail {

struct PunctuatorEntry final {
    std::string_view text;
    syntax::TokenKind kind;
};

inline constexpr std::array dot_punctuators {
    PunctuatorEntry {"...", syntax::TokenKind::ellipsis},
    PunctuatorEntry {".", syntax::TokenKind::dot},
};
inline constexpr std::array colon_punctuators {
    PunctuatorEntry {"::", syntax::TokenKind::colon_colon},
    PunctuatorEntry {":", syntax::TokenKind::colon},
};
inline constexpr std::array minus_punctuators {
    PunctuatorEntry {"->", syntax::TokenKind::arrow},
    PunctuatorEntry {"-", syntax::TokenKind::minus},
};
inline constexpr std::array equal_punctuators {
    PunctuatorEntry {"=>", syntax::TokenKind::fat_arrow},
    PunctuatorEntry {"==", syntax::TokenKind::equal_equal},
    PunctuatorEntry {"=", syntax::TokenKind::equal},
};
inline constexpr std::array bang_punctuators {
    PunctuatorEntry {"!=", syntax::TokenKind::bang_equal},
    PunctuatorEntry {"!", syntax::TokenKind::bang},
};
inline constexpr std::array less_punctuators {
    PunctuatorEntry {"<=", syntax::TokenKind::less_equal},
    PunctuatorEntry {"<<", syntax::TokenKind::less_less},
    PunctuatorEntry {"<", syntax::TokenKind::less},
};
inline constexpr std::array greater_punctuators {
    PunctuatorEntry {">=", syntax::TokenKind::greater_equal},
    PunctuatorEntry {">>", syntax::TokenKind::greater_greater},
    PunctuatorEntry {">", syntax::TokenKind::greater},
};
inline constexpr std::array amp_punctuators {
    PunctuatorEntry {"&&", syntax::TokenKind::amp_amp},
    PunctuatorEntry {"&", syntax::TokenKind::amp},
};
inline constexpr std::array pipe_punctuators {
    PunctuatorEntry {"||", syntax::TokenKind::pipe_pipe},
    PunctuatorEntry {"|", syntax::TokenKind::pipe},
};
inline constexpr std::array single_char_punctuators {
    PunctuatorEntry {"(", syntax::TokenKind::l_paren},
    PunctuatorEntry {")", syntax::TokenKind::r_paren},
    PunctuatorEntry {"{", syntax::TokenKind::l_brace},
    PunctuatorEntry {"}", syntax::TokenKind::r_brace},
    PunctuatorEntry {"[", syntax::TokenKind::l_bracket},
    PunctuatorEntry {"]", syntax::TokenKind::r_bracket},
    PunctuatorEntry {",", syntax::TokenKind::comma},
    PunctuatorEntry {";", syntax::TokenKind::semicolon},
    PunctuatorEntry {"+", syntax::TokenKind::plus},
    PunctuatorEntry {"*", syntax::TokenKind::star},
    PunctuatorEntry {"/", syntax::TokenKind::slash},
    PunctuatorEntry {"%", syntax::TokenKind::percent},
    PunctuatorEntry {"^", syntax::TokenKind::caret},
    PunctuatorEntry {"~", syntax::TokenKind::tilde},
    PunctuatorEntry {"@", syntax::TokenKind::at},
    PunctuatorEntry {"?", syntax::TokenKind::question},
};

template <base::usize entry_count>
[[nodiscard]] consteval bool punctuator_entries_are_longest_first(
    const std::array<PunctuatorEntry, entry_count>& entries
) noexcept {
    for (base::usize index = 1; index < entries.size(); ++index) {
        if (entries[index - 1].text.empty() || entries[index].text.empty()) {
            return false;
        }
        if (entries[index - 1].text.size() < entries[index].text.size()) {
            return false;
        }
    }
    return true;
}

static_assert(punctuator_entries_are_longest_first(dot_punctuators));
static_assert(punctuator_entries_are_longest_first(colon_punctuators));
static_assert(punctuator_entries_are_longest_first(minus_punctuators));
static_assert(punctuator_entries_are_longest_first(equal_punctuators));
static_assert(punctuator_entries_are_longest_first(bang_punctuators));
static_assert(punctuator_entries_are_longest_first(less_punctuators));
static_assert(punctuator_entries_are_longest_first(greater_punctuators));
static_assert(punctuator_entries_are_longest_first(amp_punctuators));
static_assert(punctuator_entries_are_longest_first(pipe_punctuators));
static_assert(punctuator_entries_are_longest_first(single_char_punctuators));

template <base::usize entry_count>
[[nodiscard]] inline std::optional<PunctuatorMatch> match_bucket(
    const std::string_view text,
    const std::array<PunctuatorEntry, entry_count>& entries
) noexcept {
    for (const PunctuatorEntry& entry : entries) {
        if (!text.starts_with(entry.text)) {
            continue;
        }
        return PunctuatorMatch {entry.text, entry.kind};
    }
    return std::nullopt;
}

} // namespace detail

[[nodiscard]] inline std::optional<PunctuatorMatch> match_punctuator(const std::string_view text) noexcept {
    if (text.empty()) {
        return std::nullopt;
    }

    const char first = text.front();
    switch (first) {
    case '.':
        return detail::match_bucket(text, detail::dot_punctuators);
    case ':':
        return detail::match_bucket(text, detail::colon_punctuators);
    case '-':
        return detail::match_bucket(text, detail::minus_punctuators);
    case '=':
        return detail::match_bucket(text, detail::equal_punctuators);
    case '!':
        return detail::match_bucket(text, detail::bang_punctuators);
    case '<':
        return detail::match_bucket(text, detail::less_punctuators);
    case '>':
        return detail::match_bucket(text, detail::greater_punctuators);
    case '&':
        return detail::match_bucket(text, detail::amp_punctuators);
    case '|':
        return detail::match_bucket(text, detail::pipe_punctuators);
    case '(':
    case ')':
    case '{':
    case '}':
    case '[':
    case ']':
    case ',':
    case ';':
    case '+':
    case '*':
    case '/':
    case '%':
    case '^':
    case '~':
    case '@':
    case '?':
        return detail::match_bucket(text, detail::single_char_punctuators);
    default:
        return std::nullopt;
    }
}

} // namespace aurex::lex
