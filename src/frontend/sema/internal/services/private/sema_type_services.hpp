#pragma once

#include <frontend/sema/internal/core/private/sema_core.hpp>

namespace aurex::sema {

class SemanticTypeResolver final {
public:
    explicit SemanticTypeResolver(SemanticAnalyzerCore& core) noexcept;

    [[nodiscard]] TypeHandle resolve_type(syntax::TypeId type);
    [[nodiscard]] TypeHandle resolve_type(syntax::TypeId type, bool opaque_allowed_as_pointee);
    [[nodiscard]] TypeHandle resolve_named_type(
        syntax::TypeId type_id, const syntax::TypeNode& type, bool opaque_allowed_as_pointee);
    [[nodiscard]] TypeHandle resolve_dyn_trait_type(
        syntax::TypeId type_id,
        const syntax::TypeNode& type,
        std::span<const TypeHandle> resolved_args,
        std::span<const TypeHandle> associated_value_types);
    [[nodiscard]] TypeHandle resolve_type_alias(const TypeAliasInfo& alias, bool opaque_allowed_as_pointee);

private:
    SemanticAnalyzerCore& core_;
};

class SemanticTypeValidator final {
public:
    explicit SemanticTypeValidator(const SemanticAnalyzerCore& core) noexcept;

    [[nodiscard]] bool can_assign(TypeHandle dst, TypeHandle src, syntax::ExprId value) const;
    [[nodiscard]] bool is_valid_storage_type(TypeHandle type) const;
    [[nodiscard]] bool check_m2_value_abi(
        TypeHandle type, ValueAbiContext context, const base::SourceRange& range) const;
    [[nodiscard]] bool is_valid_cast(syntax::ExprKind kind, TypeHandle dst, TypeHandle src) const;
    [[nodiscard]] bool is_array_containing_value_type(TypeHandle type) const noexcept;
    [[nodiscard]] const StructInfo* find_struct(TypeHandle type) const noexcept;

private:
    const SemanticAnalyzerCore& core_;
};

class SemanticAbiChecker final {
public:
    explicit SemanticAbiChecker(const SemanticAnalyzerCore& core) noexcept;

    void validate_type_layouts() const;
    [[nodiscard]] SemanticAnalyzerCore::TypeAbiLayout abi_layout(TypeHandle type) const;
    [[nodiscard]] base::u64 abi_size(TypeHandle type) const;
    [[nodiscard]] base::u64 abi_align(TypeHandle type) const;

private:
    const SemanticAnalyzerCore& core_;
};

} // namespace aurex::sema
