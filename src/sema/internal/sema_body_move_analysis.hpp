#pragma once

#include <sema/internal/sema_core.hpp>

namespace aurex::sema {

class SemanticAnalyzerCore::BodyMoveAnalyzer final {
public:
    explicit BodyMoveAnalyzer(SemanticAnalyzerCore& core) noexcept;

    void analyze(const syntax::ItemNode& function, const FunctionSignature& signature);

private:
    SemanticAnalyzerCore& core_;
};

} // namespace aurex::sema
