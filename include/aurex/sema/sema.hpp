#pragma once

#include <aurex/base/result.hpp>
#include <aurex/sema/checked_module.hpp>

#include <memory>

namespace aurex::base {
class DiagnosticSink;
} // namespace aurex::base

namespace aurex::syntax {
struct AstModule;
} // namespace aurex::syntax

namespace aurex::sema {

struct SemanticOptions {
    bool retain_generic_side_tables = true;
};

class SemanticAnalyzer final {
public:
    SemanticAnalyzer(
        syntax::AstModule& module, base::DiagnosticSink& diagnostics, SemanticOptions options = {}) noexcept;
    SemanticAnalyzer(
        const syntax::AstModule& module, base::DiagnosticSink& diagnostics, SemanticOptions options = {}) = delete;
    SemanticAnalyzer(
        syntax::AstModule&& module, base::DiagnosticSink& diagnostics, SemanticOptions options = {}) noexcept;
    ~SemanticAnalyzer();

    [[nodiscard]] base::Result<CheckedModule> analyze();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace aurex::sema
