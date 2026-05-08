#pragma once

#include "aurex/base/integer.hpp"
#include "aurex/syntax/token.hpp"

#include "lexeme.hpp"

#include <array>
#include <optional>
#include <string_view>

namespace aurex::lex {

struct PunctuatorMatch final {
    base::usize width;
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

template <base::usize entry_count>
[[nodiscard]] inline std::optional<PunctuatorMatch> match_bucket(
    const std::string_view text,
    const std::array<PunctuatorEntry, entry_count>& entries
) noexcept {
    for (const PunctuatorEntry& entry : entries) {
        if (!text.starts_with(entry.text)) {
            continue;
        }
        return PunctuatorMatch {entry.text.size(), entry.kind};
    }
    return std::nullopt;
}

[[nodiscard]] inline PunctuatorMatch single_char_match(const syntax::TokenKind kind) noexcept {
    return PunctuatorMatch {single_byte_lexeme_width, kind};
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
        return detail::single_char_match(syntax::TokenKind::l_paren);
    case ')':
        return detail::single_char_match(syntax::TokenKind::r_paren);
    case '{':
        return detail::single_char_match(syntax::TokenKind::l_brace);
    case '}':
        return detail::single_char_match(syntax::TokenKind::r_brace);
    case '[':
        return detail::single_char_match(syntax::TokenKind::l_bracket);
    case ']':
        return detail::single_char_match(syntax::TokenKind::r_bracket);
    case ',':
        return detail::single_char_match(syntax::TokenKind::comma);
    case ';':
        return detail::single_char_match(syntax::TokenKind::semicolon);
    case '+':
        return detail::single_char_match(syntax::TokenKind::plus);
    case '*':
        return detail::single_char_match(syntax::TokenKind::star);
    case '/':
        return detail::single_char_match(syntax::TokenKind::slash);
    case '%':
        return detail::single_char_match(syntax::TokenKind::percent);
    case '^':
        return detail::single_char_match(syntax::TokenKind::caret);
    case '~':
        return detail::single_char_match(syntax::TokenKind::tilde);
    case '@':
        return detail::single_char_match(syntax::TokenKind::at);
    case '?':
        return detail::single_char_match(syntax::TokenKind::question);
    default:
        return std::nullopt;
    }
}

} // namespace aurex::lex
