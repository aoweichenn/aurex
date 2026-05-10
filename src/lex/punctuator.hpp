#pragma once

#include <aurex/base/integer.hpp>
#include <aurex/syntax/token.hpp>

#include <lex/lexeme.hpp>

namespace aurex::lex {

struct PunctuatorMatch final {
    base::usize width {};
    syntax::TokenKind kind {syntax::TokenKind::invalid};

    [[nodiscard]] bool matched() const noexcept {
        return kind != syntax::TokenKind::invalid;
    }
};

namespace detail {

inline constexpr base::usize PUNCTUATOR_SECOND_BYTE_OFFSET = 1;
inline constexpr base::usize PUNCTUATOR_THIRD_BYTE_OFFSET = 2;
inline constexpr base::usize PUNCTUATOR_DOUBLE_BYTE_WIDTH = 2;
inline constexpr base::usize PUNCTUATOR_ELLIPSIS_WIDTH = 3;

[[nodiscard]] inline PunctuatorMatch single_char_match(const syntax::TokenKind kind) noexcept {
    return PunctuatorMatch {LEXEME_SINGLE_BYTE_WIDTH, kind};
}

[[nodiscard]] inline PunctuatorMatch double_char_match(const syntax::TokenKind kind) noexcept {
    return PunctuatorMatch {PUNCTUATOR_DOUBLE_BYTE_WIDTH, kind};
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
    case LEXEME_DOT:
        if (second == LEXEME_DOT && third == LEXEME_DOT) {
            return PunctuatorMatch {detail::PUNCTUATOR_ELLIPSIS_WIDTH, syntax::TokenKind::ellipsis};
        }
        return detail::single_char_match(syntax::TokenKind::dot);
    case LEXEME_COLON:
        return detail::match_two_or_one(
            second,
            LEXEME_COLON,
            syntax::TokenKind::colon_colon,
            syntax::TokenKind::colon
        );
    case LEXEME_MINUS:
        if (second == LEXEME_GREATER) {
            return detail::double_char_match(syntax::TokenKind::arrow);
        }
        if (second == LEXEME_MINUS) {
            return detail::double_char_match(syntax::TokenKind::minus_minus);
        }
        if (second == LEXEME_EQUAL) {
            return detail::double_char_match(syntax::TokenKind::minus_equal);
        }
        return detail::single_char_match(syntax::TokenKind::minus);
    case LEXEME_EQUAL:
        if (second == LEXEME_GREATER) {
            return detail::double_char_match(syntax::TokenKind::fat_arrow);
        }
        if (second == LEXEME_EQUAL) {
            return detail::double_char_match(syntax::TokenKind::equal_equal);
        }
        return detail::single_char_match(syntax::TokenKind::equal);
    case LEXEME_BANG:
        return detail::match_two_or_one(
            second,
            LEXEME_EQUAL,
            syntax::TokenKind::bang_equal,
            syntax::TokenKind::bang
        );
    case LEXEME_LESS:
        if (second == LEXEME_EQUAL) {
            return detail::double_char_match(syntax::TokenKind::less_equal);
        }
        if (second == LEXEME_LESS) {
            if (third == LEXEME_EQUAL) {
                return PunctuatorMatch {detail::PUNCTUATOR_ELLIPSIS_WIDTH, syntax::TokenKind::less_less_equal};
            }
            return detail::double_char_match(syntax::TokenKind::less_less);
        }
        return detail::single_char_match(syntax::TokenKind::less);
    case LEXEME_GREATER:
        if (second == LEXEME_EQUAL) {
            return detail::double_char_match(syntax::TokenKind::greater_equal);
        }
        if (second == LEXEME_GREATER) {
            if (third == LEXEME_EQUAL) {
                return PunctuatorMatch {detail::PUNCTUATOR_ELLIPSIS_WIDTH, syntax::TokenKind::greater_greater_equal};
            }
            return detail::double_char_match(syntax::TokenKind::greater_greater);
        }
        return detail::single_char_match(syntax::TokenKind::greater);
    case LEXEME_AMP:
        if (second == LEXEME_EQUAL) {
            return detail::double_char_match(syntax::TokenKind::amp_equal);
        }
        return detail::match_two_or_one(
            second,
            LEXEME_AMP,
            syntax::TokenKind::amp_amp,
            syntax::TokenKind::amp
        );
    case LEXEME_PIPE:
        if (second == LEXEME_EQUAL) {
            return detail::double_char_match(syntax::TokenKind::pipe_equal);
        }
        return detail::match_two_or_one(
            second,
            LEXEME_PIPE,
            syntax::TokenKind::pipe_pipe,
            syntax::TokenKind::pipe
        );
    case LEXEME_L_PAREN:
        return detail::single_char_match(syntax::TokenKind::l_paren);
    case LEXEME_R_PAREN:
        return detail::single_char_match(syntax::TokenKind::r_paren);
    case LEXEME_L_BRACE:
        return detail::single_char_match(syntax::TokenKind::l_brace);
    case LEXEME_R_BRACE:
        return detail::single_char_match(syntax::TokenKind::r_brace);
    case LEXEME_L_BRACKET:
        return detail::single_char_match(syntax::TokenKind::l_bracket);
    case LEXEME_R_BRACKET:
        return detail::single_char_match(syntax::TokenKind::r_bracket);
    case LEXEME_COMMA:
        return detail::single_char_match(syntax::TokenKind::comma);
    case LEXEME_SEMICOLON:
        return detail::single_char_match(syntax::TokenKind::semicolon);
    case LEXEME_PLUS:
        if (second == LEXEME_PLUS) {
            return detail::double_char_match(syntax::TokenKind::plus_plus);
        }
        if (second == LEXEME_EQUAL) {
            return detail::double_char_match(syntax::TokenKind::plus_equal);
        }
        return detail::single_char_match(syntax::TokenKind::plus);
    case LEXEME_STAR:
        if (second == LEXEME_EQUAL) {
            return detail::double_char_match(syntax::TokenKind::star_equal);
        }
        return detail::single_char_match(syntax::TokenKind::star);
    case LEXEME_SLASH:
        if (second == LEXEME_EQUAL) {
            return detail::double_char_match(syntax::TokenKind::slash_equal);
        }
        return detail::single_char_match(syntax::TokenKind::slash);
    case LEXEME_PERCENT:
        if (second == LEXEME_EQUAL) {
            return detail::double_char_match(syntax::TokenKind::percent_equal);
        }
        return detail::single_char_match(syntax::TokenKind::percent);
    case LEXEME_CARET:
        if (second == LEXEME_EQUAL) {
            return detail::double_char_match(syntax::TokenKind::caret_equal);
        }
        return detail::single_char_match(syntax::TokenKind::caret);
    case LEXEME_TILDE:
        return detail::single_char_match(syntax::TokenKind::tilde);
    case LEXEME_AT:
        return detail::single_char_match(syntax::TokenKind::at);
    case LEXEME_QUESTION:
        return detail::single_char_match(syntax::TokenKind::question);
    default:
        return {};
    }
}

} // namespace aurex::lex
