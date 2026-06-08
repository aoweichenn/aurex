#pragma once

#include <aurex/infrastructure/query/query_key.hpp>

#include <span>
#include <string_view>

#include <frontend/sema/internal/core/private/sema_core.hpp>

namespace aurex::sema {

enum class SemanticServiceDomain : base::u8 {
    lookup,
    type,
    generic,
    body_check,
};

struct SemanticServiceBoundary {
    SemanticServiceDomain domain = SemanticServiceDomain::lookup;
    std::string_view name;
    std::span<const query::QueryKind> authority_kinds;
    bool owns_analyzer_state = false;
};

class SemanticLookupService final {
public:
    explicit SemanticLookupService(SemanticAnalyzerCore& core) noexcept;

    [[nodiscard]] SemanticServiceBoundary boundary() const noexcept;
    [[nodiscard]] bool accepts_authority(query::QueryKind kind) const noexcept;
    [[nodiscard]] SemanticAnalyzerCore::LookupResolver resolver() noexcept;
    [[nodiscard]] SemanticAnalyzerCore::LookupResolver resolver() const noexcept;
    [[nodiscard]] const SemanticAnalyzerCore::ModuleIdList& visible_modules(syntax::ModuleId module) const;
    [[nodiscard]] const FunctionSignature* find_function_in_visible_modules(
        IdentId name_id, std::string_view name, const base::SourceRange& range, bool report_unknown = true);
    [[nodiscard]] TypeHandle find_type_in_visible_modules(IdentId name_id, std::string_view name,
        const base::SourceRange& range, bool opaque_allowed_as_pointee, bool report_unknown = true);

private:
    SemanticAnalyzerCore& core_;
};

class SemanticTypeService final {
public:
    explicit SemanticTypeService(SemanticAnalyzerCore& core) noexcept;

    [[nodiscard]] SemanticServiceBoundary boundary() const noexcept;
    [[nodiscard]] bool accepts_authority(query::QueryKind kind) const noexcept;
    [[nodiscard]] SemanticTypeResolver resolver() noexcept;
    [[nodiscard]] SemanticTypeValidator validator() const noexcept;
    [[nodiscard]] SemanticAbiChecker abi_checker() const noexcept;
    [[nodiscard]] TypeHandle resolve_type(syntax::TypeId type);
    [[nodiscard]] bool can_assign(TypeHandle dst, TypeHandle src, syntax::ExprId value) const;
    [[nodiscard]] bool is_valid_storage_type(TypeHandle type) const;
    void validate_type_layouts() const;

private:
    SemanticAnalyzerCore& core_;
};

class SemanticGenericService final {
public:
    explicit SemanticGenericService(SemanticAnalyzerCore& core) noexcept;

    [[nodiscard]] SemanticServiceBoundary boundary() const noexcept;
    [[nodiscard]] bool accepts_authority(query::QueryKind kind) const noexcept;
    [[nodiscard]] SemanticAnalyzerCore::GenericAnalyzer analyzer() noexcept;
    [[nodiscard]] bool has_generic_params(const syntax::ItemNode& item) const noexcept;
    void analyze_function_definition(const SemanticAnalyzerCore::GenericTemplateInfo& info);

private:
    SemanticAnalyzerCore& core_;
};

class SemanticBodyCheckService final {
public:
    explicit SemanticBodyCheckService(SemanticAnalyzerCore& core) noexcept;

    [[nodiscard]] SemanticServiceBoundary boundary() const noexcept;
    [[nodiscard]] bool accepts_authority(query::QueryKind kind) const noexcept;
    [[nodiscard]] SemanticAnalyzerCore::ExpressionAnalyzer expression_analyzer() noexcept;
    [[nodiscard]] SemanticAnalyzerCore::StatementAnalyzer statement_analyzer() noexcept;
    void analyze_function_body(const syntax::ItemNode& function, syntax::ItemId function_id);
    void analyze_function_body_with_signature(const syntax::ItemNode& function, const FunctionLookupKey& key,
        const FunctionSignature& signature, SemanticAnalyzerCore::FunctionBodyState& state);

private:
    SemanticAnalyzerCore& core_;
};

class SemanticServiceBundle final {
public:
    explicit SemanticServiceBundle(SemanticAnalyzerCore& core) noexcept;

    [[nodiscard]] SemaContext& context() noexcept;
    [[nodiscard]] SemanticLookupService lookup() noexcept;
    [[nodiscard]] SemanticTypeService type() noexcept;
    [[nodiscard]] SemanticGenericService generic() noexcept;
    [[nodiscard]] SemanticBodyCheckService body_check() noexcept;

private:
    SemanticAnalyzerCore& core_;
};

} // namespace aurex::sema
