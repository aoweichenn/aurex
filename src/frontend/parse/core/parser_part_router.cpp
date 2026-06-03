#include <aurex/frontend/parse/parser_block_part.hpp>
#include <aurex/frontend/parse/parser_expr_part.hpp>
#include <aurex/frontend/parse/parser_part_router.hpp>
#include <aurex/frontend/parse/parser_pattern_part.hpp>
#include <aurex/frontend/parse/parser_stmt_part.hpp>
#include <aurex/frontend/parse/parser_type_part.hpp>

namespace aurex::parse {

syntax::TypeId ParserPartRouter::parse_type() const
{
    return TypeParser(this->parser_).parse_type();
}

syntax::StmtId ParserPartRouter::parse_block() const
{
    return BlockParser(this->parser_).parse_block();
}

syntax::ExprId ParserPartRouter::parse_block_expr(const ExprContext context) const
{
    return BlockParser(this->parser_).parse_block_expr(context);
}

syntax::StmtId ParserPartRouter::parse_stmt() const
{
    return StmtParser(this->parser_).parse_stmt();
}

syntax::ExprId ParserPartRouter::parse_expr(const ExprContext context) const
{
    return ExprParser(this->parser_).parse_expr(context);
}

syntax::PatternId ParserPartRouter::parse_pattern() const
{
    return PatternParser(this->parser_).parse_pattern();
}

} // namespace aurex::parse
