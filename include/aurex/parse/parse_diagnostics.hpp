#pragma once

#include "aurex/base/diagnostic.hpp"
#include "aurex/parse/token_cursor.hpp"

#include <string>
#include <utility>

namespace aurex::parse {

class ParseDiagnostics final {
public:
    explicit ParseDiagnostics(base::DiagnosticSink& sink) noexcept
        : sink_(sink) {}

    void report_here(const TokenCursor& cursor, std::string message) {
        report_at(cursor.peek(), std::move(message));
    }

    void report_at(const syntax::Token& token, std::string message) {
        if (panic_) {
            return;
        }
        panic_ = true;
        sink_.push(base::Diagnostic {
            base::Severity::error,
            token.range,
            std::move(message),
        });
    }

    void reset_panic() noexcept {
        panic_ = false;
    }

    [[nodiscard]] bool has_error() const noexcept {
        return sink_.has_error();
    }

private:
    base::DiagnosticSink& sink_;
    bool panic_ = false;
};

} // namespace aurex::parse
