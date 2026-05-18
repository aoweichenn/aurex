#pragma once

#include <aurex/parse/parser_part_router.hpp>
#include <aurex/syntax/ast.hpp>

namespace aurex::parse {

class ParserPartRangeReader : protected ParserPartRouter {
protected:
    explicit ParserPartRangeReader(Parser& parser) noexcept : ParserPartRouter(parser)
    {
    }

    [[nodiscard]] base::SourceRange merge(const base::SourceRange& begin, const base::SourceRange& end) const noexcept;
    [[nodiscard]] base::SourceRange expr_range_or(syntax::ExprId id, const base::SourceRange& fallback) const noexcept;
    [[nodiscard]] base::SourceRange stmt_range_or(syntax::StmtId id, const base::SourceRange& fallback) const noexcept;
    [[nodiscard]] base::SourceRange type_range_or(syntax::TypeId id, const base::SourceRange& fallback) const noexcept;
    [[nodiscard]] base::SourceRange pattern_range_or(
        syntax::PatternId id, const base::SourceRange& fallback) const noexcept;
};

} // namespace aurex::parse
