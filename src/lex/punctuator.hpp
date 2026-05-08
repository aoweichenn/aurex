#pragma once

#include "aurex/base/integer.hpp"
#include "aurex/syntax/token.hpp"

#include "lexeme.hpp"

namespace aurex::lex {

struct PunctuatorMatch final {
    base::usize width {};
    syntax::TokenKind kind {syntax::TokenKind::invalid};

    [[nodiscard]] bool matched() const noexcept {
        return kind != syntax::TokenKind::invalid;
    }
};

namespace detail {

inline constexpr base::usize punctuator_second_byte_offset = 1;
inline constexpr base::usize punctuator_third_byte_offset = 2;
inline constexpr base::usize double_byte_punctuator_width = 2;
inline constexpr base::usize ellipsis_punctuator_width = 3;

[[nodiscard]] inline PunctuatorMatch single_char_match(const syntax::TokenKind kind) noexcept {
    return PunctuatorMatch {single_byte_lexeme_width, kind};
}

[[nodiscard]] inline PunctuatorMatch double_char_match(const syntax::TokenKind kind) noexcept {
    return PunctuatorMatch {double_byte_punctuator_width, kind};
}

[[nodiscard]] inline PunctuatorMatch match_two_or_one(
    const char actual_second,
    const char second,
    const syntax::TokenKind double_kind,
    const syntax::TokenKind single_kind
) noexcept {
    if (actual_second == second) {
        return double_char_match(double_kind);
    }
    return single_char_match(single_kind);
}

} // namespace detail

[[nodiscard]] inline PunctuatorMatch match_punctuator(
    const char first,
    const char second,
    const char third
) noexcept {
    switch (first) {
    case lexeme_dot:
        if (second == lexeme_dot && third == lexeme_dot) {
            return PunctuatorMatch {detail::ellipsis_punctuator_width, syntax::TokenKind::ellipsis};
        }
        return detail::single_char_match(syntax::TokenKind::dot);
    case lexeme_colon:
        return detail::match_two_or_one(
            second,
            lexeme_colon,
            syntax::TokenKind::colon_colon,
            syntax::TokenKind::colon
        );
    case lexeme_minus:
        return detail::match_two_or_one(
            second,
            lexeme_greater,
            syntax::TokenKind::arrow,
            syntax::TokenKind::minus
        );
    case lexeme_equal:
        if (second == lexeme_greater) {
            return detail::double_char_match(syntax::TokenKind::fat_arrow);
        }
        if (second == lexeme_equal) {
            return detail::double_char_match(syntax::TokenKind::equal_equal);
        }
        return detail::single_char_match(syntax::TokenKind::equal);
    case lexeme_bang:
        return detail::match_two_or_one(
            second,
            lexeme_equal,
            syntax::TokenKind::bang_equal,
            syntax::TokenKind::bang
        );
    case lexeme_less:
        if (second == lexeme_equal) {
            return detail::double_char_match(syntax::TokenKind::less_equal);
        }
        if (second == lexeme_less) {
            return detail::double_char_match(syntax::TokenKind::less_less);
        }
        return detail::single_char_match(syntax::TokenKind::less);
    case lexeme_greater:
        if (second == lexeme_equal) {
            return detail::double_char_match(syntax::TokenKind::greater_equal);
        }
        if (second == lexeme_greater) {
            return detail::double_char_match(syntax::TokenKind::greater_greater);
        }
        return detail::single_char_match(syntax::TokenKind::greater);
    case lexeme_amp:
        return detail::match_two_or_one(
            second,
            lexeme_amp,
            syntax::TokenKind::amp_amp,
            syntax::TokenKind::amp
        );
    case lexeme_pipe:
        return detail::match_two_or_one(
            second,
            lexeme_pipe,
            syntax::TokenKind::pipe_pipe,
            syntax::TokenKind::pipe
        );
    case lexeme_l_paren:
        return detail::single_char_match(syntax::TokenKind::l_paren);
    case lexeme_r_paren:
        return detail::single_char_match(syntax::TokenKind::r_paren);
    case lexeme_l_brace:
        return detail::single_char_match(syntax::TokenKind::l_brace);
    case lexeme_r_brace:
        return detail::single_char_match(syntax::TokenKind::r_brace);
    case lexeme_l_bracket:
        return detail::single_char_match(syntax::TokenKind::l_bracket);
    case lexeme_r_bracket:
        return detail::single_char_match(syntax::TokenKind::r_bracket);
    case lexeme_comma:
        return detail::single_char_match(syntax::TokenKind::comma);
    case lexeme_semicolon:
        return detail::single_char_match(syntax::TokenKind::semicolon);
    case lexeme_plus:
        return detail::single_char_match(syntax::TokenKind::plus);
    case lexeme_star:
        return detail::single_char_match(syntax::TokenKind::star);
    case lexeme_slash:
        return detail::single_char_match(syntax::TokenKind::slash);
    case lexeme_percent:
        return detail::single_char_match(syntax::TokenKind::percent);
    case lexeme_caret:
        return detail::single_char_match(syntax::TokenKind::caret);
    case lexeme_tilde:
        return detail::single_char_match(syntax::TokenKind::tilde);
    case lexeme_at:
        return detail::single_char_match(syntax::TokenKind::at);
    case lexeme_question:
        return detail::single_char_match(syntax::TokenKind::question);
    default:
        return {};
    }
}

} // namespace aurex::lex
