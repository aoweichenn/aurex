#pragma once

#include "aurex/parse/parse_session.hpp"
#include "aurex/parse/recovery_context.hpp"
#include "aurex/syntax/ast.hpp"

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
    [[nodiscard]] bool check_type_arg_list_end() const noexcept;
    [[nodiscard]] bool next_angle_list_is_type_scope() const noexcept;
    [[nodiscard]] bool next_angle_list_is_struct_literal() const noexcept;
    bool match(syntax::TokenKind kind) noexcept;
    const syntax::Token& advance() noexcept;
    const syntax::Token& expect(syntax::TokenKind kind, std::string message);
    const syntax::Token& expect_recovered(
        syntax::TokenKind kind,
        std::string message,
        RecoveryContext context
    );
    const syntax::Token& expect_identifier_recovered(std::string message);
    const syntax::Token& expect_type_annotation_colon(std::string message);
    const syntax::Token& expect_initializer_equal(std::string message);
    const syntax::Token& expect_type_arg_list_end(std::string message);
    void synchronize(RecoveryContext context);
    void report_here(std::string message);
    void report_at(const syntax::Token& token, std::string message);
    void reset_panic() noexcept;

    Parser& parser_;
    ParseSession& session_;
};

} // namespace aurex::parse
