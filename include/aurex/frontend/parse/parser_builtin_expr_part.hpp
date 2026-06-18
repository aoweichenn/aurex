#pragma once

#include <aurex/frontend/parse/parser_part_base.hpp>

namespace aurex::parse {

class BuiltinExprParser final : private ParserPartBase {
public:
    explicit BuiltinExprParser(Parser& parser) noexcept : ParserPartBase(parser)
    {
    }

    [[nodiscard]] syntax::ExprId parse_cast(syntax::ExprKind kind, ExprContext context);
    [[nodiscard]] syntax::ExprId parse_ptraddr(ExprContext context);
    [[nodiscard]] syntax::ExprId parse_ptrat(ExprContext context);
    [[nodiscard]] syntax::ExprId parse_str_slice_unary(syntax::ExprKind kind, ExprContext context);
    [[nodiscard]] syntax::ExprId parse_strraw(ExprContext context);

private:
    void expect_builtin_arg_list_start(std::string message);
    [[nodiscard]] const syntax::Token& expect_builtin_type_arg_list_start(std::string message);
    void recover_builtin_arg_separator(std::string message);
    [[nodiscard]] const syntax::Token& expect_builtin_arg_list_end(std::string message);
    [[nodiscard]] const syntax::Token& expect_builtin_type_arg_list_end(
        std::string message, const syntax::Token& opening);
};

} // namespace aurex::parse
