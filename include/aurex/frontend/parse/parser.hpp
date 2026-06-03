#pragma once

#include <aurex/frontend/parse/expr_context.hpp>
#include <aurex/frontend/parse/parse_session.hpp>
#include <aurex/frontend/parse/recovery_context.hpp>
#include <aurex/frontend/syntax/core/ast.hpp>
#include <aurex/frontend/syntax/core/token.hpp>
#include <aurex/infrastructure/base/diagnostic.hpp>
#include <aurex/infrastructure/base/result.hpp>

#include <span>
#include <string>

namespace aurex::parse {

class ExprParser;
class ItemParser;
class ParserPartBase;
class ParserPartCore;
class ParserPartRangeReader;
class PatternParser;
class StmtParser;
class TypeParser;

class Parser final {
public:
    // Parser depends only on tokens, not on Lexer. This keeps syntax tests and
    // future parser replacements independent from the scanning implementation.
    Parser(std::span<const syntax::Token> tokens, base::DiagnosticSink& diagnostics);

    [[nodiscard]] base::Result<syntax::AstModule> parse_module();

private:
    [[nodiscard]] bool is_eof() const noexcept;
    [[nodiscard]] const syntax::Token& peek() const noexcept;
    [[nodiscard]] const syntax::Token& previous() const noexcept;
    [[nodiscard]] bool check(syntax::TokenKind kind) const noexcept;
    [[nodiscard]] bool check_next(syntax::TokenKind kind) const noexcept;
    [[nodiscard]] bool check_visibility_import_prefix() const noexcept;
    [[nodiscard]] bool check_visibility_use_prefix() const noexcept;
    [[nodiscard]] bool check_import_or_use_decl_prefix() const noexcept;
    [[nodiscard]] const syntax::Token& peek_at(base::usize offset) const noexcept;
    [[nodiscard]] base::usize mark() const noexcept;
    void rewind(base::usize position) noexcept;
    bool match(syntax::TokenKind kind) noexcept;
    const syntax::Token& advance() noexcept;
    [[nodiscard]] bool check_contextual_c_keyword() const noexcept;
    const syntax::Token& expect(syntax::TokenKind kind, std::string message);
    const syntax::Token& expect_contextual_c_keyword(std::string message);
    const syntax::Token& expect_contextual_c_keyword_recovered(std::string message, RecoveryContext context);
    const syntax::Token& expect_recovered(syntax::TokenKind kind, std::string message, RecoveryContext context);
    const syntax::Token& expect_recovered_after(
        syntax::TokenKind kind, std::string message, RecoveryContext context, const syntax::Token& opening);
    void synchronize(RecoveryContext context);
    void report_here(std::string message);
    void report_at(const syntax::Token& token, std::string message);
    void report_note_at(const syntax::Token& token, std::string message);

    [[nodiscard]] base::SourceRange merge(const base::SourceRange& begin, const base::SourceRange& end) const noexcept;
    void reset_panic() noexcept;

    friend class ExprParser;
    friend class ItemParser;
    friend class ParserPartBase;
    friend class ParserPartCore;
    friend class ParserPartRangeReader;
    friend class PatternParser;
    friend class StmtParser;
    friend class TypeParser;

    ParseSession session_;
};

} // namespace aurex::parse
