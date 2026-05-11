#pragma once

#include <aurex/base/diagnostic.hpp>
#include <aurex/base/result.hpp>
#include <aurex/parse/expr_context.hpp>
#include <aurex/parse/parse_session.hpp>
#include <aurex/parse/recovery_context.hpp>
#include <aurex/syntax/ast.hpp>
#include <aurex/syntax/token.hpp>

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
    Parser(
        std::span<const syntax::Token> tokens,
        base::DiagnosticSink& diagnostics
    ) noexcept;

    [[nodiscard]] base::Result<syntax::AstModule> parse_module();

private:
    [[nodiscard]] bool is_eof() const noexcept;
    [[nodiscard]] const syntax::Token& peek() const noexcept;
    [[nodiscard]] const syntax::Token& previous() const noexcept;
    [[nodiscard]] bool check(syntax::TokenKind kind) const noexcept;
    [[nodiscard]] bool check_next(syntax::TokenKind kind) const noexcept;
    bool match(syntax::TokenKind kind) noexcept;
    const syntax::Token& advance() noexcept;
    const syntax::Token& expect(syntax::TokenKind kind, std::string message);
    const syntax::Token& expect_recovered(
        syntax::TokenKind kind,
        std::string message,
        RecoveryContext context
    );
    void synchronize(RecoveryContext context);
    void report_here(std::string message);
    void report_at(const syntax::Token& token, std::string message);

    [[nodiscard]] base::SourceRange merge(base::SourceRange begin, base::SourceRange end) const noexcept;
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
