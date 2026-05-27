#include <aurex/parse/parser.hpp>
#include <aurex/parse/parser_item_part.hpp>
#include <aurex/parse/parser_messages.hpp>

#include <string_view>
#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

inline constexpr std::string_view PARSER_CONTEXTUAL_PART_KEYWORD_TEXT = "part";
inline constexpr std::string_view PARSER_CONTEXTUAL_USE_KEYWORD_TEXT = "use";
inline constexpr base::usize PARSER_SCOPED_VISIBILITY_SCOPE_OFFSET = 2;
inline constexpr base::usize PARSER_SCOPED_VISIBILITY_CLOSE_OFFSET = 3;
inline constexpr base::usize PARSER_SCOPED_VISIBILITY_IMPORT_OFFSET = 4;
inline constexpr base::usize PARSER_SCOPED_VISIBILITY_USE_OFFSET = 4;

[[nodiscard]] bool token_is_contextual_part_keyword(const syntax::Token& token) noexcept
{
    return token.kind == TokenKind::identifier && token.text() == PARSER_CONTEXTUAL_PART_KEYWORD_TEXT;
}

[[nodiscard]] bool token_is_contextual_use_keyword(const syntax::Token& token) noexcept
{
    return token.kind == TokenKind::identifier && token.text() == PARSER_CONTEXTUAL_USE_KEYWORD_TEXT;
}

} // namespace

Parser::Parser(const std::span<const syntax::Token> tokens, base::DiagnosticSink& diagnostics)
    : session_(tokens, diagnostics)
{
}

bool Parser::check_visibility_import_prefix() const noexcept
{
    if (this->check(TokenKind::kw_priv)) {
        return this->check_next(TokenKind::kw_import);
    }
    if (!this->check(TokenKind::kw_pub)) {
        return false;
    }
    if (this->check_next(TokenKind::kw_import)) {
        return true;
    }
    return this->check_next(TokenKind::l_paren)
        && this->peek_at(PARSER_SCOPED_VISIBILITY_SCOPE_OFFSET).kind == TokenKind::identifier
        && this->peek_at(PARSER_SCOPED_VISIBILITY_SCOPE_OFFSET).text() == PARSER_VISIBILITY_PACKAGE_SCOPE_TEXT
        && this->peek_at(PARSER_SCOPED_VISIBILITY_CLOSE_OFFSET).kind == TokenKind::r_paren
        && this->peek_at(PARSER_SCOPED_VISIBILITY_IMPORT_OFFSET).kind == TokenKind::kw_import;
}

bool Parser::check_visibility_use_prefix() const noexcept
{
    if (this->check(TokenKind::kw_priv)) {
        return token_is_contextual_use_keyword(this->peek_at(1));
    }
    if (!this->check(TokenKind::kw_pub)) {
        return false;
    }
    if (token_is_contextual_use_keyword(this->peek_at(1))) {
        return true;
    }
    return this->check_next(TokenKind::l_paren)
        && this->peek_at(PARSER_SCOPED_VISIBILITY_SCOPE_OFFSET).kind == TokenKind::identifier
        && this->peek_at(PARSER_SCOPED_VISIBILITY_SCOPE_OFFSET).text() == PARSER_VISIBILITY_PACKAGE_SCOPE_TEXT
        && this->peek_at(PARSER_SCOPED_VISIBILITY_CLOSE_OFFSET).kind == TokenKind::r_paren
        && token_is_contextual_use_keyword(this->peek_at(PARSER_SCOPED_VISIBILITY_USE_OFFSET));
}

bool Parser::check_import_or_use_decl_prefix() const noexcept
{
    return this->check(TokenKind::kw_import) || this->check_visibility_import_prefix()
        || this->check_visibility_use_prefix();
}

base::Result<syntax::AstModule> Parser::parse_module()
{
    ItemParser items(*this);
    bool has_module_declaration = false;

    if (this->match(TokenKind::kw_module)) {
        has_module_declaration = true;
        this->session_.module.module_path = items.parse_path();
        if (token_is_contextual_part_keyword(this->peek())) {
            this->session_.module.file_kind = syntax::ModuleFileKind::part;
            this->session_.module.part_header = items.parse_module_part_header();
        } else {
            this->expect_recovered(
                TokenKind::semicolon, std::string(PARSER_EXPECT_MODULE_TERMINATOR), RecoveryContext::module_terminator);
        }
    }

    while (has_module_declaration && token_is_contextual_part_keyword(this->peek())) {
        if (this->session_.module.file_kind == syntax::ModuleFileKind::part) {
            this->report_at(this->peek(), std::string(PARSER_MODULE_PART_LIST_UNSUPPORTED));
            static_cast<void>(items.parse_module_part_decl());
            continue;
        }
        this->session_.module.part_declarations.push_back(items.parse_module_part_decl());
    }

    while (this->check_import_or_use_decl_prefix()) {
        if (this->check_visibility_use_prefix()) {
            if (this->session_.module.file_kind == syntax::ModuleFileKind::part) {
                this->report_at(this->peek(), std::string(PARSER_MODULE_PART_USE_UNSUPPORTED));
            }
            this->session_.module.reexports.push_back(items.parse_use_decl());
            continue;
        }
        this->session_.module.imports.push_back(items.parse_import_decl());
    }

    while (!this->is_eof()) {
        if (token_is_contextual_part_keyword(this->peek())) {
            if (!has_module_declaration) {
                this->report_at(this->peek(), std::string(PARSER_PART_DECL_REQUIRES_MODULE));
            } else if (this->session_.module.file_kind == syntax::ModuleFileKind::part) {
                this->report_at(this->peek(), std::string(PARSER_MODULE_PART_LIST_UNSUPPORTED));
            } else {
                this->report_at(this->peek(), std::string(PARSER_PART_DECL_AFTER_IMPORT_OR_ITEM));
            }
            static_cast<void>(items.parse_module_part_decl());
            continue;
        }
        if (this->check_visibility_use_prefix()) {
            if (this->session_.module.file_kind == syntax::ModuleFileKind::part) {
                this->report_at(this->peek(), std::string(PARSER_MODULE_PART_USE_UNSUPPORTED));
            } else {
                this->report_at(this->peek(), std::string(PARSER_USE_AFTER_ITEM));
            }
            this->session_.module.reexports.push_back(items.parse_use_decl());
            continue;
        }
        const syntax::ItemId item = items.parse_item();
        if (!syntax::is_valid(item)) {
            this->synchronize(RecoveryContext::item);
        }
    }

    if (this->session_.diagnostics.has_error()) {
        return base::Result<syntax::AstModule>::fail({base::ErrorCode::parse_error, std::string(PARSER_PARSE_FAILED)});
    }
    this->session_.module.finalize_identifiers();
    return base::Result<syntax::AstModule>::ok(std::move(this->session_.module));
}

} // namespace aurex::parse
