#pragma once

#include <frontend/sema/internal/core/private/sema_core.hpp>

#include <optional>

namespace aurex::sema {

class SemanticAnalyzerCore::RaiiAnalyzer final {
public:
    explicit RaiiAnalyzer(SemanticAnalyzerCore& core) noexcept;

    void validate_destructor_impls();
    [[nodiscard]] bool is_destructor_impl_block(const syntax::ItemNode& item) const;

private:
    class AnalysisScope;

    void validate_destructor_impl_block(const syntax::ItemNode& impl_block, syntax::ItemId impl_id);
    [[nodiscard]] bool validate_impl_header(const syntax::ItemNode& impl_block, syntax::ItemId impl_id,
        TypeHandle& self_type) const;
    [[nodiscard]] bool validate_method_surface(
        const syntax::ItemNode& method, TypeHandle self_type, const FunctionSignature* signature) const;
    [[nodiscard]] std::optional<syntax::ItemId> single_drop_method(const syntax::ItemNode& impl_block) const;
    [[nodiscard]] FunctionSignature* destructor_signature(const syntax::ItemNode& method, syntax::ItemId method_id);
    void record_destructor(const syntax::ItemNode& impl_block, syntax::ItemId impl_id, const syntax::ItemNode& method,
        syntax::ItemId method_id, TypeHandle self_type, FunctionLookupKey function_key);

    SemanticAnalyzerCore& core_;
};

} // namespace aurex::sema
