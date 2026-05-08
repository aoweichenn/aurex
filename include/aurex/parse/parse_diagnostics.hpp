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
        this->report_at(cursor.peek(), std::move(message));
    }

    void report_at(const syntax::Token& token, std::string message) {
        if (this->panic_) {
            return;
        }
        this->panic_ = true;
        this->sink_.push(base::Diagnostic {
            base::Severity::error,
            token.range,
            std::move(message),
        });
    }

    void reset_panic() noexcept {
        this->panic_ = false;
    }

    [[nodiscard]] bool has_error() const noexcept {
        return this->sink_.has_error();
    }

private:
    base::DiagnosticSink& sink_;
    bool panic_ = false;
};

} // namespace aurex::parse
