#include "aurex/parse/parser.hpp"

#include "aurex/parse/parser_item_part.hpp"

#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

} // namespace

Parser::Parser(
    const std::span<const syntax::Token> tokens,
    base::DiagnosticSink& diagnostics
) noexcept
    : session_(tokens, diagnostics) {}

base::Result<syntax::AstModule> Parser::parse_module() {
    ItemParser items(*this);

    if (this->match(TokenKind::kw_module)) {
        this->session_.module.module_path = items.parse_path();
        this->expect_recovered(
            TokenKind::semicolon,
            "expected ';' after module declaration",
            RecoveryContext::module_terminator
        );
    }

    while (this->check(TokenKind::kw_import) ||
           ((this->check(TokenKind::kw_pub) || this->check(TokenKind::kw_priv)) && this->check_next(TokenKind::kw_import))) {
        this->session_.module.imports.push_back(items.parse_import_decl());
    }

    while (!this->is_eof()) {
        const syntax::ItemId item = items.parse_item();
        if (!syntax::is_valid(item)) {
            this->synchronize(RecoveryContext::item);
        }
    }

    if (this->session_.diagnostics.has_error()) {
        return base::Result<syntax::AstModule>::fail(
            {base::ErrorCode::parse_error, "parsing failed"}
        );
    }
    return base::Result<syntax::AstModule>::ok(std::move(this->session_.module));
}

} // namespace aurex::parse
