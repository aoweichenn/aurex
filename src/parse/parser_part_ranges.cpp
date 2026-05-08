#include "aurex/parse/parser_part_ranges.hpp"

#include "aurex/parse/parser.hpp"

namespace aurex::parse {

base::SourceRange ParserPartRangeReader::merge(
    const base::SourceRange begin,
    const base::SourceRange end
) const noexcept {
    return this->parser_.merge(begin, end);
}

base::SourceRange ParserPartRangeReader::expr_range_or(
    const syntax::ExprId id,
    const base::SourceRange fallback
) const noexcept {
    if (!syntax::is_valid(id) || id.value >= this->session_.module.exprs.size()) {
        return fallback;
    }
    return this->session_.module.exprs[id.value].range;
}

base::SourceRange ParserPartRangeReader::stmt_range_or(
    const syntax::StmtId id,
    const base::SourceRange fallback
) const noexcept {
    if (!syntax::is_valid(id) || id.value >= this->session_.module.stmts.size()) {
        return fallback;
    }
    return this->session_.module.stmts[id.value].range;
}

base::SourceRange ParserPartRangeReader::type_range_or(
    const syntax::TypeId id,
    const base::SourceRange fallback
) const noexcept {
    if (!syntax::is_valid(id) || id.value >= this->session_.module.types.size()) {
        return fallback;
    }
    return this->session_.module.types[id.value].range;
}

base::SourceRange ParserPartRangeReader::pattern_range_or(
    const syntax::PatternId id,
    const base::SourceRange fallback
) const noexcept {
    if (!syntax::is_valid(id) || id.value >= this->session_.module.patterns.size()) {
        return fallback;
    }
    return this->session_.module.patterns[id.value].range;
}

} // namespace aurex::parse
