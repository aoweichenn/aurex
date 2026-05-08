#pragma once

#include "aurex/base/diagnostic.hpp"
#include "aurex/parse/parse_diagnostics.hpp"
#include "aurex/parse/token_cursor.hpp"
#include "aurex/syntax/ast.hpp"

#include <span>

namespace aurex::parse {

struct ParseSession final {
    TokenCursor cursor;
    ParseDiagnostics diagnostics;
    syntax::AstModule module;

    ParseSession(
        std::span<const syntax::Token> tokens,
        base::DiagnosticSink& sink
    ) noexcept
        : cursor(tokens), diagnostics(sink) {}
};

} // namespace aurex::parse
