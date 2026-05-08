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

inline constexpr std::array punctuator_entries {
    PunctuatorEntry {"...", syntax::TokenKind::ellipsis},
    PunctuatorEntry {"::", syntax::TokenKind::colon_colon},
    PunctuatorEntry {"->", syntax::TokenKind::arrow},
    PunctuatorEntry {"=>", syntax::TokenKind::fat_arrow},
    PunctuatorEntry {"==", syntax::TokenKind::equal_equal},
    PunctuatorEntry {"!=", syntax::TokenKind::bang_equal},
    PunctuatorEntry {"<=", syntax::TokenKind::less_equal},
    PunctuatorEntry {"<<", syntax::TokenKind::less_less},
    PunctuatorEntry {">=", syntax::TokenKind::greater_equal},
    PunctuatorEntry {">>", syntax::TokenKind::greater_greater},
    PunctuatorEntry {"&&", syntax::TokenKind::amp_amp},
    PunctuatorEntry {"||", syntax::TokenKind::pipe_pipe},

    PunctuatorEntry {"(", syntax::TokenKind::l_paren},
    PunctuatorEntry {")", syntax::TokenKind::r_paren},
    PunctuatorEntry {"{", syntax::TokenKind::l_brace},
    PunctuatorEntry {"}", syntax::TokenKind::r_brace},
    PunctuatorEntry {"[", syntax::TokenKind::l_bracket},
    PunctuatorEntry {"]", syntax::TokenKind::r_bracket},
    PunctuatorEntry {",", syntax::TokenKind::comma},
    PunctuatorEntry {".", syntax::TokenKind::dot},
    PunctuatorEntry {";", syntax::TokenKind::semicolon},
    PunctuatorEntry {":", syntax::TokenKind::colon},
    PunctuatorEntry {"+", syntax::TokenKind::plus},
    PunctuatorEntry {"-", syntax::TokenKind::minus},
    PunctuatorEntry {"*", syntax::TokenKind::star},
    PunctuatorEntry {"/", syntax::TokenKind::slash},
    PunctuatorEntry {"%", syntax::TokenKind::percent},
    PunctuatorEntry {"&", syntax::TokenKind::amp},
    PunctuatorEntry {"|", syntax::TokenKind::pipe},
    PunctuatorEntry {"^", syntax::TokenKind::caret},
    PunctuatorEntry {"~", syntax::TokenKind::tilde},
    PunctuatorEntry {"!", syntax::TokenKind::bang},
    PunctuatorEntry {"=", syntax::TokenKind::equal},
    PunctuatorEntry {"<", syntax::TokenKind::less},
    PunctuatorEntry {">", syntax::TokenKind::greater},
    PunctuatorEntry {"@", syntax::TokenKind::at},
    PunctuatorEntry {"?", syntax::TokenKind::question},
};

[[nodiscard]] consteval bool punctuator_entries_are_longest_first() noexcept {
    for (base::usize index = 1; index < punctuator_entries.size(); ++index) {
        if (punctuator_entries[index - 1].text.empty() || punctuator_entries[index].text.empty()) {
            return false;
        }
        if (punctuator_entries[index - 1].text.size() < punctuator_entries[index].text.size()) {
            return false;
        }
    }
    return true;
}

static_assert(punctuator_entries_are_longest_first(), "punctuator entries must stay sorted longest-first");

} // namespace detail

[[nodiscard]] inline std::optional<PunctuatorMatch> match_punctuator(const std::string_view text) noexcept {
    if (text.empty()) {
        return std::nullopt;
    }

    const char first = text.front();
    for (const detail::PunctuatorEntry& entry : detail::punctuator_entries) {
        if (entry.text.front() != first) {
            continue;
        }
        if (!text.starts_with(entry.text)) {
            continue;
        }
        return PunctuatorMatch {entry.text, entry.kind};
    }
    return std::nullopt;
}

} // namespace aurex::lex
