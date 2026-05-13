#pragma once

#include <aurex/parse/parser_part_base.hpp>

#include <optional>
#include <vector>

namespace aurex::parse {

class PostfixExprParser final : private ParserPartBase {
public:
    explicit PostfixExprParser(Parser& parser) noexcept
        : ParserPartBase(parser) {}

    [[nodiscard]] syntax::ExprId parse_postfix(ExprContext context);

private:
    [[nodiscard]] std::optional<syntax::ExprId> parse_next_suffix(syntax::ExprId expr, ExprContext context);
    [[nodiscard]] syntax::ExprId parse_generic_apply_suffix(syntax::ExprId expr);
    void parse_generic_type_args(std::vector<syntax::TypeId>& args);
    [[nodiscard]] bool recover_generic_type_arg_separator();
    [[nodiscard]] syntax::ExprId parse_field_suffix(syntax::ExprId expr);
    [[nodiscard]] syntax::ExprId parse_leading_dot_numeric_field_suffix(syntax::ExprId expr);
    [[nodiscard]] syntax::ExprId parse_index_suffix(syntax::ExprId expr, ExprContext context);
    [[nodiscard]] const syntax::Token& expect_index_suffix_end();
    [[nodiscard]] const syntax::Token& expect_slice_suffix_end();
    [[nodiscard]] syntax::ExprId parse_call_suffix(syntax::ExprId expr, ExprContext context);
    void parse_call_args(std::vector<syntax::ExprId>& args, ExprContext context);
    [[nodiscard]] bool recover_call_arg_separator();
    [[nodiscard]] syntax::ExprId parse_try_suffix(syntax::ExprId expr);
    [[nodiscard]] syntax::ExprId parse_rejected_update_suffix(
        syntax::ExprId expr,
        syntax::TokenKind kind,
        std::string message
    );
};

} // namespace aurex::parse
