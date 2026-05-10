#pragma once

#include <aurex/parse/parser_part_router.hpp>
#include <aurex/syntax/ast.hpp>

namespace aurex::parse {

class ParserPartRangeReader : protected ParserPartRouter {
protected:
    explicit ParserPartRangeReader(Parser& parser) noexcept
        : ParserPartRouter(parser) {}

    [[nodiscard]] base::SourceRange merge(base::SourceRange begin, base::SourceRange end) const noexcept;
    [[nodiscard]] base::SourceRange expr_range_or(syntax::ExprId id, base::SourceRange fallback) const noexcept;
    [[nodiscard]] base::SourceRange stmt_range_or(syntax::StmtId id, base::SourceRange fallback) const noexcept;
    [[nodiscard]] base::SourceRange type_range_or(syntax::TypeId id, base::SourceRange fallback) const noexcept;
    [[nodiscard]] base::SourceRange pattern_range_or(syntax::PatternId id, base::SourceRange fallback) const noexcept;
};

} // namespace aurex::parse
