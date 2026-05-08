#pragma once

#include "aurex/parse/parser_part_base.hpp"

#include <string_view>
#include <vector>

namespace aurex::parse {

class NameExprParser final : private ParserPartBase {
public:
    explicit NameExprParser(Parser& parser) noexcept
        : ParserPartBase(parser) {}

    [[nodiscard]] syntax::ExprId parse_name_or_struct_literal(ExprContext context);

private:
    [[nodiscard]] syntax::ExprId parse_struct_literal(
        std::string_view scope_name,
        base::SourceRange scope_range,
        const syntax::Token& name,
        std::vector<syntax::TypeId> struct_type_args,
        ExprContext context
    );
    void parse_struct_fields(std::vector<syntax::FieldInit>& fields, ExprContext context);
    [[nodiscard]] syntax::FieldInit parse_struct_field(ExprContext context);
    [[nodiscard]] bool recover_struct_field_separator();
    [[nodiscard]] syntax::ExprId make_name_expr(
        std::string_view scope_name,
        base::SourceRange scope_range,
        const syntax::Token& name
    );
};

} // namespace aurex::parse
