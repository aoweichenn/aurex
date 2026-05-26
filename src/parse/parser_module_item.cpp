#include <aurex/parse/parser_item_part.hpp>
#include <aurex/parse/parser_messages.hpp>
#include <aurex/parse/recovery.hpp>

#include <optional>
#include <string_view>
#include <utility>

namespace aurex::parse {
namespace {

using syntax::TokenKind;

inline constexpr std::string_view PARSER_CONTEXTUAL_PART_KEYWORD_TEXT = "part";

} // namespace

syntax::ModulePath ItemParser::parse_path() const
{
    syntax::ModulePath path;
    const std::optional<syntax::Token> first = this->parse_path_segment(std::string(PARSER_EXPECT_PATH_IDENTIFIER));
    base::SourceRange range = first.has_value() ? first->range : this->peek().range;
    if (first.has_value()) {
        path.parts.push_back(first->text());
    }
    while (this->match(TokenKind::dot)) {
        std::optional<syntax::Token> part =
            this->parse_path_segment(std::string(PARSER_EXPECT_PATH_IDENTIFIER_AFTER_DOT));
        if (part.has_value()) {
            path.parts.push_back(part->text());
            range = this->merge(range, part->range);
        }
    }
    path.range = range;
    this->reset_panic();
    return path;
}

syntax::ImportDecl ItemParser::parse_import_decl()
{
    syntax::ImportDecl import;
    const ParsedVisibility visibility = this->parse_visibility();
    import.visibility = visibility.visibility;
    import.explicit_visibility = visibility.explicit_visibility;
    this->expect(TokenKind::kw_import, std::string(PARSER_EXPECT_IMPORT_KEYWORD));
    import.path = this->parse_path();
    if (this->match(TokenKind::kw_as)) {
        this->parse_import_alias(import);
    } else if (!import.path.parts.empty()) {
        import.alias = import.path.parts.back();
        import.alias_range = import.path.range;
    }
    [[maybe_unused]] const syntax::Token& terminator =
        this->expect_item_terminator(std::string(PARSER_EXPECT_IMPORT_TERMINATOR));
    this->reset_panic();
    return import;
}

syntax::ModulePartDecl ItemParser::parse_module_part_decl()
{
    syntax::ModulePartDecl part;
    const syntax::Token& keyword = this->expect(TokenKind::identifier, std::string(PARSER_EXPECT_PRIMARY_PART_NAME));
    if (keyword.kind == TokenKind::identifier && keyword.text() != PARSER_CONTEXTUAL_PART_KEYWORD_TEXT) {
        this->report_at(keyword, std::string(PARSER_EXPECT_PRIMARY_PART_NAME));
    }
    const syntax::Token& name = this->expect_identifier_recovered(std::string(PARSER_EXPECT_PRIMARY_PART_NAME));
    if (name.kind == TokenKind::identifier) {
        part.name = name.text();
        part.range = keyword.kind == TokenKind::identifier ? this->merge(keyword.range, name.range) : name.range;
    }
    [[maybe_unused]] const syntax::Token& terminator =
        this->expect_item_terminator(std::string(PARSER_EXPECT_PRIMARY_PART_TERMINATOR));
    this->reset_panic();
    return part;
}

syntax::ModulePartHeader ItemParser::parse_module_part_header()
{
    syntax::ModulePartHeader part;
    const syntax::Token& keyword = this->expect(TokenKind::identifier, std::string(PARSER_EXPECT_MODULE_PART_NAME));
    if (keyword.kind == TokenKind::identifier && keyword.text() != PARSER_CONTEXTUAL_PART_KEYWORD_TEXT) {
        this->report_at(keyword, std::string(PARSER_EXPECT_MODULE_PART_NAME));
    }
    const syntax::Token& name = this->expect_identifier_recovered(std::string(PARSER_EXPECT_MODULE_PART_NAME));
    if (name.kind == TokenKind::identifier) {
        part.name = name.text();
        part.range = keyword.kind == TokenKind::identifier ? this->merge(keyword.range, name.range) : name.range;
    }
    [[maybe_unused]] const syntax::Token& terminator =
        this->expect_item_terminator(std::string(PARSER_EXPECT_MODULE_PART_TERMINATOR));
    this->reset_panic();
    return part;
}

const syntax::Token& ItemParser::expect_item_terminator(std::string message) const
{
    return this->expect_recovered(TokenKind::semicolon, std::move(message), RecoveryContext::item_terminator);
}

void ItemParser::expect_item_container_start(std::string message) const
{
    this->expect_recovered(TokenKind::l_brace, std::move(message), RecoveryContext::block_start);
}

const syntax::Token& ItemParser::expect_item_container_end(std::string message) const
{
    return this->expect_recovered(TokenKind::r_brace, std::move(message), RecoveryContext::block_end);
}

std::optional<syntax::Token> ItemParser::parse_path_segment(std::string message) const
{
    if (token_starts_path_segment(this->peek().kind)) {
        return this->advance();
    }

    const syntax::Token& segment =
        this->expect_recovered(syntax::TokenKind::identifier, std::move(message), RecoveryContext::path_segment);
    if (segment.kind == TokenKind::identifier) {
        return segment;
    }
    if (token_starts_path_segment(this->peek().kind)) {
        return this->advance();
    }
    return std::nullopt;
}

void ItemParser::parse_import_alias(syntax::ImportDecl& import)
{
    const syntax::Token& alias = this->expect_identifier_recovered(std::string(PARSER_EXPECT_IMPORT_ALIAS));
    if (alias.kind == TokenKind::identifier) {
        import.alias = alias.text();
        import.alias_range = alias.range;
        return;
    }
    this->recover_import_alias();
}

void ItemParser::recover_import_alias() const
{
    if (!token_matches_recovery_context(this->peek().kind, RecoveryContext::import_alias)) {
        this->synchronize(RecoveryContext::import_alias);
    }
}

ParsedVisibility ItemParser::parse_visibility() const
{
    if (this->match(syntax::TokenKind::kw_pub)) {
        if (this->match(TokenKind::l_paren)) {
            const syntax::Token& opening = this->previous();
            const syntax::Token& scope = this->expect_recovered(TokenKind::identifier,
                std::string(PARSER_EXPECT_VISIBILITY_SCOPE), RecoveryContext::grouped_expression);
            syntax::Visibility visibility = syntax::Visibility::public_;
            if (scope.kind == TokenKind::identifier) {
                if (scope.text() == PARSER_VISIBILITY_PACKAGE_SCOPE_TEXT) {
                    visibility = syntax::Visibility::package_;
                } else {
                    this->report_at(scope, std::string(PARSER_UNSUPPORTED_VISIBILITY_SCOPE));
                }
            }
            this->expect_recovered_after(TokenKind::r_paren, std::string(PARSER_EXPECT_VISIBILITY_SCOPE_END),
                RecoveryContext::grouped_expression, opening);
            return ParsedVisibility{visibility, true};
        }
        return ParsedVisibility{syntax::Visibility::public_, true};
    }
    if (this->match(syntax::TokenKind::kw_priv)) {
        return ParsedVisibility{syntax::Visibility::private_, true};
    }
    return {};
}

} // namespace aurex::parse
