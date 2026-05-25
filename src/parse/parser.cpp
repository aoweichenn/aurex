#include <aurex/parse/parser.hpp>
#include <aurex/parse/parser_item_part.hpp>
#include <aurex/parse/parser_messages.hpp>

#include <string_view>
#include <utility>

namespace aurex::parse {

namespace {

using syntax::TokenKind;

inline constexpr std::string_view PARSER_CONTEXTUAL_PART_KEYWORD_TEXT = "part";

[[nodiscard]] bool token_is_contextual_part_keyword(const syntax::Token& token) noexcept
{
    return token.kind == TokenKind::identifier && token.text() == PARSER_CONTEXTUAL_PART_KEYWORD_TEXT;
}

} // namespace

Parser::Parser(const std::span<const syntax::Token> tokens, base::DiagnosticSink& diagnostics)
    : session_(tokens, diagnostics)
{
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

    while (this->check(TokenKind::kw_import)
        || ((this->check(TokenKind::kw_pub) || this->check(TokenKind::kw_priv))
            && this->check_next(TokenKind::kw_import))) {
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
