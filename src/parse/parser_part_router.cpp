#include <aurex/parse/parser_part_router.hpp>

#include <aurex/parse/parser_block_part.hpp>
#include <aurex/parse/parser_expr_part.hpp>
#include <aurex/parse/parser_pattern_part.hpp>
#include <aurex/parse/parser_stmt_part.hpp>
#include <aurex/parse/parser_type_part.hpp>

namespace aurex::parse {

syntax::TypeId ParserPartRouter::parse_type() {
    return TypeParser(this->parser_).parse_type();
}

std::vector<syntax::TypeId> ParserPartRouter::parse_type_arg_list() {
    return TypeParser(this->parser_).parse_type_arg_list();
}

syntax::StmtId ParserPartRouter::parse_block() {
    return BlockParser(this->parser_).parse_block();
}

syntax::ExprId ParserPartRouter::parse_block_expr(const ExprContext context) {
    return BlockParser(this->parser_).parse_block_expr(context);
}

syntax::StmtId ParserPartRouter::parse_stmt() {
    return StmtParser(this->parser_).parse_stmt();
}

syntax::ExprId ParserPartRouter::parse_expr(const ExprContext context) {
    return ExprParser(this->parser_).parse_expr(context);
}

syntax::PatternId ParserPartRouter::parse_pattern() {
    return PatternParser(this->parser_).parse_pattern();
}

} // namespace aurex::parse
