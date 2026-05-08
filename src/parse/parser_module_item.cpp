#include "aurex/parse/parser_item_part.hpp"

#include "aurex/parse/recovery.hpp"

#include <optional>
#include <utility>

namespace aurex::parse {
namespace {

using syntax::TokenKind;

} // namespace

syntax::ModulePath ItemParser::parse_path() {
    syntax::ModulePath path;
    std::optional<syntax::Token> first = this->parse_path_segment("expected identifier in path");
    base::SourceRange range = first.has_value() ? first->range : this->peek().range;
    if (first.has_value()) {
        path.parts.push_back(first->text);
    }
    while (this->match(TokenKind::dot)) {
        std::optional<syntax::Token> part = this->parse_path_segment("expected identifier after '.'");
        if (part.has_value()) {
            path.parts.push_back(part->text);
            range = this->merge(range, part->range);
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
        this->parse_import_alias(import);
    }
    [[maybe_unused]] const syntax::Token& terminator =
        this->expect_item_terminator("expected ';' after import declaration");
    this->reset_panic();
    return import;
}

const syntax::Token& ItemParser::expect_item_terminator(std::string message) {
    return this->expect_recovered(
        TokenKind::semicolon,
        std::move(message),
        RecoveryContext::item_terminator
    );
}

void ItemParser::expect_item_container_start(std::string message) {
    this->expect_recovered(
        TokenKind::l_brace,
        std::move(message),
        RecoveryContext::block_start
    );
}

const syntax::Token& ItemParser::expect_item_container_end(std::string message) {
    return this->expect_recovered(
        TokenKind::r_brace,
        std::move(message),
        RecoveryContext::block_end
    );
}

std::optional<syntax::Token> ItemParser::parse_path_segment(std::string message) {
    if (token_starts_path_segment(this->peek().kind)) {
        return this->advance();
    }

    const syntax::Token& segment = this->expect_recovered(
        syntax::TokenKind::identifier,
        std::move(message),
        RecoveryContext::path_segment
    );
    if (segment.kind == TokenKind::identifier) {
        return segment;
    }
    if (token_starts_path_segment(this->peek().kind)) {
        return this->advance();
    }
    return std::nullopt;
}

void ItemParser::parse_import_alias(syntax::ImportDecl& import) {
    const syntax::Token& alias =
        this->expect_identifier_recovered("expected import alias after 'as'");
    if (alias.kind == TokenKind::identifier) {
        import.alias = alias.text;
        import.alias_range = alias.range;
        return;
    }
    this->recover_import_alias();
}

void ItemParser::recover_import_alias() {
    if (!token_matches_recovery_context(this->peek().kind, RecoveryContext::import_alias)) {
        this->synchronize(RecoveryContext::import_alias);
    }
}

syntax::Visibility ItemParser::parse_visibility() {
    if (this->match(syntax::TokenKind::kw_pub)) {
        return syntax::Visibility::public_;
    }
    if (this->match(syntax::TokenKind::kw_priv)) {
        return syntax::Visibility::private_;
    }
    return syntax::Visibility::public_;
}

} // namespace aurex::parse
