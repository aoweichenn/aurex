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
    [[nodiscard]] bool bracketed_type_args_are_postfix_generic_apply(syntax::ExprId expr) const noexcept;
    [[nodiscard]] syntax::ExprId parse_generic_apply_suffix(syntax::ExprId expr);
    void parse_generic_type_args(std::vector<syntax::TypeId>& args);
    [[nodiscard]] bool recover_generic_type_arg_separator();
    [[nodiscard]] syntax::ExprId parse_field_suffix(syntax::ExprId expr);
    [[nodiscard]] syntax::ExprId parse_rejected_legacy_scope_suffix(syntax::ExprId expr);
    [[nodiscard]] syntax::ExprId parse_struct_literal_suffix(syntax::ExprId type_expr, ExprContext context);
    void parse_struct_fields(std::vector<syntax::FieldInit>& fields, ExprContext context);
    [[nodiscard]] syntax::FieldInit parse_struct_field(ExprContext context);
    [[nodiscard]] bool recover_struct_field_separator();
    [[nodiscard]] syntax::ExprId parse_rejected_numeric_tuple_field_suffix(syntax::ExprId expr);
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
