#include "bracket_suffix_classifier.hpp"

#include <string_view>
#include <vector>

namespace aurex::parse {
namespace {

using syntax::TokenKind;

constexpr char PARSER_TYPE_LIKE_FIRST_UPPER = 'A';
constexpr char PARSER_TYPE_LIKE_LAST_UPPER = 'Z';

[[nodiscard]] bool identifier_text_is_type_like(const std::string_view text) noexcept
{
    if (text.empty()) {
        return false;
    }
    const char first = text.front();
    return first >= PARSER_TYPE_LIKE_FIRST_UPPER && first <= PARSER_TYPE_LIKE_LAST_UPPER;
}

[[nodiscard]] bool token_is_generic_call_or_literal_continuation(
    const TokenKind kind, const ExprContext context) noexcept
{
    return kind == TokenKind::l_paren || (context == ExprContext::normal && kind == TokenKind::l_brace);
}

[[nodiscard]] bool is_primitive_type_token(const TokenKind kind) noexcept
{
    switch (kind) {
        case TokenKind::kw_void:
        case TokenKind::kw_bool:
        case TokenKind::kw_i8:
        case TokenKind::kw_u8:
        case TokenKind::kw_i16:
        case TokenKind::kw_u16:
        case TokenKind::kw_i32:
        case TokenKind::kw_u32:
        case TokenKind::kw_i64:
        case TokenKind::kw_u64:
        case TokenKind::kw_isize:
        case TokenKind::kw_usize:
        case TokenKind::kw_f32:
        case TokenKind::kw_f64:
        case TokenKind::kw_str:
        case TokenKind::kw_char:
            return true;
        default:
            return false;
    }
}

} // namespace

BracketSuffixDecision BracketSuffixClassifier::classify_empty_suffix() const noexcept
{
    BracketSuffixDecision decision;
    decision.kind = this->check(TokenKind::l_paren) || this->check(TokenKind::l_brace)
        ? BracketSuffixKind::generic_apply
        : BracketSuffixKind::index;
    return decision;
}

BracketSuffixDecision BracketSuffixClassifier::classify_slice_suffix(const bool start_is_type_only) const noexcept
{
    BracketSuffixDecision decision;
    decision.kind = BracketSuffixKind::slice;
    decision.has_type_only_arg = start_is_type_only;
    return decision;
}

BracketSuffixDecision BracketSuffixClassifier::classify_after_expr(BracketSuffixClassificationInput input) const
{
    BracketSuffixDecision decision;
    decision.base_is_type_like = this->arg_expr_is_type_like(input.base);
    decision.args_are_type_like = input.args_are_type_like;
    decision.has_type_only_arg = input.has_type_only_arg;

    if (input.has_type_only_arg) {
        decision.kind = BracketSuffixKind::generic_apply;
        decision.report_type_arg_errors = true;
        return decision;
    }

    const TokenKind continuation = this->peek().kind;
    if (token_is_generic_call_or_literal_continuation(continuation, input.context)) {
        decision.kind = BracketSuffixKind::generic_apply;
        decision.report_type_arg_errors = !decision.base_is_type_like || !decision.args_are_type_like;
        return decision;
    }
    if (continuation == TokenKind::dot) {
        decision.kind = decision.base_is_type_like && decision.args_are_type_like ? BracketSuffixKind::generic_apply
                                                                                  : BracketSuffixKind::index;
        return decision;
    }
    if (continuation == TokenKind::r_bracket && this->suffix_is_inside_generic_continuation()
        && decision.base_is_type_like && decision.args_are_type_like) {
        decision.kind = BracketSuffixKind::generic_apply;
        return decision;
    }

    decision.kind = BracketSuffixKind::index;
    return decision;
}

bool BracketSuffixClassifier::arg_starts_type_only() const noexcept
{
    const TokenKind kind = this->peek().kind;
    if (is_primitive_type_token(kind) || kind == TokenKind::kw_fn || kind == TokenKind::kw_extern
        || kind == TokenKind::kw_unsafe) {
        return true;
    }
    if (kind == TokenKind::star) {
        const TokenKind next = this->peek_at(1).kind;
        return next == TokenKind::kw_mut || next == TokenKind::kw_const;
    }
    if (kind == TokenKind::l_bracket) {
        const TokenKind next = this->peek_at(1).kind;
        return next == TokenKind::r_bracket || next == TokenKind::integer_literal;
    }
    if (kind == TokenKind::l_paren) {
        return this->parenthesized_arg_is_type();
    }
    return false;
}

bool BracketSuffixClassifier::arg_expr_is_type_like(const syntax::ExprId expr) const
{
    std::vector<syntax::ExprId> pending;
    pending.push_back(expr);
    while (!pending.empty()) {
        const syntax::ExprId current = pending.back();
        pending.pop_back();
        if (!syntax::is_valid(current) || current.value >= this->session_.module.exprs.size()) {
            return false;
        }
        const syntax::ExprKind kind = this->session_.module.exprs.kind(current.value);
        switch (kind) {
            case syntax::ExprKind::name: {
                const syntax::NameExprPayload* const payload = this->session_.module.exprs.name_payload(current.value);
                if (payload == nullptr || !identifier_text_is_type_like(payload->text)) {
                    return false;
                }
                break;
            }
            case syntax::ExprKind::field: {
                const syntax::FieldExprPayload* const payload =
                    this->session_.module.exprs.field_payload(current.value);
                if (payload == nullptr || !identifier_text_is_type_like(payload->field_name)) {
                    return false;
                }
                break;
            }
            case syntax::ExprKind::generic_apply: {
                const syntax::GenericApplyExprPayload* const payload =
                    this->session_.module.exprs.generic_apply_payload(current.value);
                if (payload == nullptr) {
                    return false;
                }
                pending.push_back(payload->callee);
                break;
            }
            case syntax::ExprKind::unary: {
                const syntax::UnaryExprPayload* const payload =
                    this->session_.module.exprs.unary_payload(current.value);
                if (payload == nullptr
                    || (payload->op != syntax::UnaryOp::address_of && payload->op != syntax::UnaryOp::address_of_mut)) {
                    return false;
                }
                pending.push_back(payload->operand);
                break;
            }
            default:
                return false;
        }
    }
    return true;
}

bool BracketSuffixClassifier::parenthesized_arg_is_type() const noexcept
{
    base::usize depth = 0;
    bool saw_comma_at_outer_depth = false;
    for (base::usize offset = 0; true; ++offset) {
        const syntax::Token& token = this->peek_at(offset);
        if (token.kind == TokenKind::eof) {
            return false;
        }
        if (token.kind == TokenKind::l_paren) {
            ++depth;
            continue;
        }
        if (token.kind == TokenKind::r_paren) {
            if (depth == 0) {
                return false;
            }
            --depth;
            if (depth == 0) {
                return saw_comma_at_outer_depth;
            }
            continue;
        }
        if (token.kind == TokenKind::comma && depth == 1) {
            saw_comma_at_outer_depth = true;
        }
    }
}

bool BracketSuffixClassifier::suffix_is_inside_generic_continuation() const noexcept
{
    base::usize offset = 0;
    while (this->peek_at(offset).kind == TokenKind::r_bracket) {
        ++offset;
    }
    const TokenKind continuation = this->peek_at(offset).kind;
    return continuation == TokenKind::l_paren || continuation == TokenKind::l_brace || continuation == TokenKind::dot;
}

} // namespace aurex::parse
