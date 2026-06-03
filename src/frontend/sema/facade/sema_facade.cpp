#include <aurex/frontend/sema/sema.hpp>

#include <utility>

#include <frontend/sema/internal/core/private/sema_core.hpp>

namespace aurex::sema {

class SemanticAnalyzer::Impl final {
public:
    Impl(syntax::AstModule& module, base::DiagnosticSink& diagnostics, const SemanticOptions options) noexcept
        : core_(module, diagnostics, options)
    {
    }

    Impl(syntax::AstModule&& module, base::DiagnosticSink& diagnostics, const SemanticOptions options) noexcept
        : core_(std::move(module), diagnostics, options)
    {
    }

    [[nodiscard]] base::Result<CheckedModule> analyze()
    {
        return this->core_.analyze();
    }

private:
    SemanticAnalyzerCore core_;
};

SemanticAnalyzer::SemanticAnalyzer(
    syntax::AstModule& module, base::DiagnosticSink& diagnostics, const SemanticOptions options) noexcept
    : impl_(std::make_unique<Impl>(module, diagnostics, options))
{
}

SemanticAnalyzer::SemanticAnalyzer(
    syntax::AstModule&& module, base::DiagnosticSink& diagnostics, const SemanticOptions options) noexcept
    : impl_(std::make_unique<Impl>(std::move(module), diagnostics, options))
{
}

SemanticAnalyzer::~SemanticAnalyzer() = default;

base::Result<CheckedModule> SemanticAnalyzer::analyze()
{
    return this->impl_->analyze();
}

} // namespace aurex::sema
