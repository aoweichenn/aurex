#pragma once

#include <aurex/frontend/parse/parser_part_core.hpp>
#include <aurex/frontend/syntax/core/ast.hpp>

#include <string_view>

namespace aurex::parse {

class TypeArgExprConverter final : private ParserPartCore {
public:
    explicit TypeArgExprConverter(Parser& parser) noexcept : ParserPartCore(parser)
    {
    }

    [[nodiscard]] syntax::TypeId convert(syntax::ExprId expr, bool report_errors);
    [[nodiscard]] syntax::TypeId append_selector(
        syntax::TypeId base, std::string_view name, const base::SourceRange& range, bool report_errors);
};

} // namespace aurex::parse
