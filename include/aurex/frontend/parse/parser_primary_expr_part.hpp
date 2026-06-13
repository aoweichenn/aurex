#pragma once

#include <aurex/frontend/parse/parser_part_base.hpp>

#include <optional>
#include <vector>

namespace aurex::parse {

class PrimaryExprParser final : private ParserPartBase {
public:
    explicit PrimaryExprParser(Parser& parser) noexcept : ParserPartBase(parser)
    {
    }

    [[nodiscard]] syntax::ExprId parse_primary(ExprContext context);

private:
    struct LambdaHead final {
        const syntax::Token& begin;
        std::vector<syntax::ParamDecl> params;
    };

    [[nodiscard]] syntax::ExprId parse_lambda_expr(ExprContext context);
    [[nodiscard]] syntax::ExprId parse_legacy_lambda_expr(ExprContext context);
    [[nodiscard]] syntax::ExprId finish_lambda_expr(LambdaHead head, ExprContext context);
    [[nodiscard]] std::vector<syntax::ParamDecl> parse_lambda_param_list(syntax::TokenKind terminator);
    [[nodiscard]] std::optional<syntax::ParamDecl> parse_lambda_param();
    bool recover_lambda_param_separator(syntax::TokenKind terminator);
    [[nodiscard]] syntax::StmtId make_lambda_return_body(
        const syntax::Token& begin, syntax::ExprId value, const base::SourceRange& body_range);
    [[nodiscard]] syntax::ExprId parse_unsafe_block_expr(ExprContext context);
    [[nodiscard]] syntax::ExprId parse_array_literal(ExprContext context);
    bool recover_array_literal_separator();
    const syntax::Token& expect_array_literal_end(const syntax::Token& opening);
    [[nodiscard]] syntax::ExprId parse_tuple_or_grouped_expr(ExprContext context);
    bool recover_tuple_literal_separator();
    const syntax::Token& expect_tuple_literal_end(const syntax::Token& opening);
    [[nodiscard]] syntax::ExprId parse_builtin_expr(ExprContext context);
    void expect_grouped_expression_end(const syntax::Token& opening);
    void skip_grouped_expression_remainder();
    [[nodiscard]] syntax::ExprId parse_literal(syntax::ExprKind kind) const;
    [[nodiscard]] syntax::ExprId make_invalid_expr();
};

} // namespace aurex::parse
