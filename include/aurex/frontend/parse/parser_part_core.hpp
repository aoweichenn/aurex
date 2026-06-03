#pragma once

#include <aurex/frontend/parse/parse_session.hpp>
#include <aurex/frontend/parse/recovery_context.hpp>
#include <aurex/frontend/syntax/core/ast.hpp>

#include <string>

namespace aurex::parse {

class Parser;

class ParserPartCore {
protected:
    explicit ParserPartCore(Parser& parser) noexcept;

    [[nodiscard]] bool is_eof() const noexcept;
    [[nodiscard]] const syntax::Token& peek() const noexcept;
    [[nodiscard]] const syntax::Token& previous() const noexcept;
    [[nodiscard]] bool check(syntax::TokenKind kind) const noexcept;
    [[nodiscard]] bool check_next(syntax::TokenKind kind) const noexcept;
    [[nodiscard]] const syntax::Token& peek_at(base::usize offset) const noexcept;
    [[nodiscard]] base::usize mark() const noexcept;
    void rewind(base::usize position) const noexcept;
    bool match(syntax::TokenKind kind) const noexcept;
    const syntax::Token& advance() const noexcept;
    const syntax::Token& expect(syntax::TokenKind kind, std::string message) const;
    const syntax::Token& expect_contextual_c_keyword(std::string message) const;
    const syntax::Token& expect_contextual_c_keyword_recovered(std::string message, RecoveryContext context) const;
    const syntax::Token& expect_recovered(syntax::TokenKind kind, std::string message, RecoveryContext context) const;
    const syntax::Token& expect_recovered_after(
        syntax::TokenKind kind, std::string message, RecoveryContext context, const syntax::Token& opening) const;
    const syntax::Token& expect_identifier_recovered(std::string message);
    const syntax::Token& expect_type_annotation_colon(std::string message);
    const syntax::Token& expect_initializer_equal(std::string message);
    void synchronize(RecoveryContext context) const;
    void report_here(std::string message) const;
    void report_at(const syntax::Token& token, std::string message) const;
    void report_note_at(const syntax::Token& token, std::string message) const;
    void reset_panic() const noexcept;

    Parser& parser_;
    ParseSession& session_;
};

} // namespace aurex::parse
