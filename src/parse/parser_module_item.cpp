#include "aurex/parse/parser_parts.hpp"

namespace aurex::parse {

namespace {

using syntax::TokenKind;

[[nodiscard]] bool is_path_segment_token(const TokenKind kind) noexcept {
    return kind == TokenKind::identifier || kind == TokenKind::kw_c || kind == TokenKind::kw_str;
}

} // namespace

syntax::ModulePath ItemParser::parse_path() {
    syntax::ModulePath path;
    const syntax::Token& first = is_path_segment_token(this->peek().kind)
        ? this->advance()
        : this->expect(TokenKind::identifier, "expected identifier in path");
    base::SourceRange range = first.range;
    if (is_path_segment_token(first.kind)) {
        path.parts.push_back(first.text);
    }
    while (this->match(TokenKind::dot)) {
        const syntax::Token& part = is_path_segment_token(this->peek().kind)
            ? this->advance()
            : this->expect(TokenKind::identifier, "expected identifier after '.'");
        if (is_path_segment_token(part.kind)) {
            path.parts.push_back(part.text);
            range = this->merge(range, part.range);
        }
    }
    path.range = range;
    this->reset_panic();
    return path;
}

syntax::ImportDecl ItemParser::parse_import_decl() {
    syntax::ImportDecl import;
    if (this->match(TokenKind::kw_pub)) {
        import.visibility = syntax::Visibility::public_;
        import.explicit_visibility = true;
    } else if (this->match(TokenKind::kw_priv)) {
        import.visibility = syntax::Visibility::private_;
        import.explicit_visibility = true;
    }
    this->expect(TokenKind::kw_import, "expected 'import'");
    import.path = this->parse_path();
    if (this->match(TokenKind::kw_as)) {
        const syntax::Token& alias = this->expect(TokenKind::identifier, "expected import alias after 'as'");
        if (alias.kind == TokenKind::identifier) {
            import.alias = alias.text;
            import.alias_range = alias.range;
        }
    }
    this->expect(TokenKind::semicolon, "expected ';' after import declaration");
    this->reset_panic();
    return import;
}

syntax::Visibility ItemParser::parse_visibility() {
    if (this->match(TokenKind::kw_pub)) {
        return syntax::Visibility::public_;
    }
    if (this->match(TokenKind::kw_priv)) {
        return syntax::Visibility::private_;
    }
    return syntax::Visibility::public_;
}

} // namespace aurex::parse
