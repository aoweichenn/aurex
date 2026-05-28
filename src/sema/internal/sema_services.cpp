#include <algorithm>
#include <array>

#include <sema/internal/sema_expression_analyzer.hpp>
#include <sema/internal/sema_generic_analyzer.hpp>
#include <sema/internal/sema_lookup_resolver.hpp>
#include <sema/internal/sema_services.hpp>
#include <sema/internal/sema_statement_analyzer.hpp>
#include <sema/internal/sema_type_services.hpp>

namespace aurex::sema {
namespace {

constexpr std::string_view SEMA_LOOKUP_SERVICE_NAME = "lookup";
constexpr std::string_view SEMA_TYPE_SERVICE_NAME = "type";
constexpr std::string_view SEMA_GENERIC_SERVICE_NAME = "generic";
constexpr std::string_view SEMA_BODY_CHECK_SERVICE_NAME = "body_check";

constexpr std::array<query::QueryKind, 6> SEMA_LOOKUP_SERVICE_AUTHORITIES{
    query::QueryKind::module_graph,
    query::QueryKind::module_part,
    query::QueryKind::item_list,
    query::QueryKind::module_exports,
    query::QueryKind::module_package_exports,
    query::QueryKind::item_signature,
};

constexpr std::array<query::QueryKind, 3> SEMA_TYPE_SERVICE_AUTHORITIES{
    query::QueryKind::item_signature,
    query::QueryKind::generic_instance_signature,
    query::QueryKind::type_check_body,
};

constexpr std::array<query::QueryKind, 3> SEMA_GENERIC_SERVICE_AUTHORITIES{
    query::QueryKind::generic_template_signature,
    query::QueryKind::generic_instance_signature,
    query::QueryKind::generic_instance_body,
};

constexpr std::array<query::QueryKind, 3> SEMA_BODY_CHECK_SERVICE_AUTHORITIES{
    query::QueryKind::function_body_syntax,
    query::QueryKind::type_check_body,
    query::QueryKind::item_signature,
};

[[nodiscard]] bool supports_authority(
    const std::span<const query::QueryKind> authorities, const query::QueryKind kind) noexcept
{
    return std::ranges::find(authorities, kind) != authorities.end();
}

} // namespace

SemanticLookupService::SemanticLookupService(SemanticAnalyzerCore& core) noexcept : core_(core)
{
}

SemanticServiceBoundary SemanticLookupService::boundary() const noexcept
{
    return SemanticServiceBoundary{
        SemanticServiceDomain::lookup,
        SEMA_LOOKUP_SERVICE_NAME,
        SEMA_LOOKUP_SERVICE_AUTHORITIES,
        false,
    };
}

bool SemanticLookupService::accepts_authority(const query::QueryKind kind) const noexcept
{
    return supports_authority(this->boundary().authority_kinds, kind);
}

SemanticAnalyzerCore::LookupResolver SemanticLookupService::resolver() noexcept
{
    return this->core_.lookup_resolver();
}

SemanticAnalyzerCore::LookupResolver SemanticLookupService::resolver() const noexcept
{
    const SemanticAnalyzerCore& core = this->core_;
    return core.lookup_resolver();
}

const SemanticAnalyzerCore::ModuleIdList& SemanticLookupService::visible_modules(const syntax::ModuleId module) const
{
    return this->resolver().visible_modules(module);
}

const FunctionSignature* SemanticLookupService::find_function_in_visible_modules(
    const IdentId name_id, const std::string_view name, const base::SourceRange& range, const bool report_unknown)
{
    return this->resolver().find_function_in_visible_modules(name_id, name, range, report_unknown);
}

TypeHandle SemanticLookupService::find_type_in_visible_modules(const IdentId name_id, const std::string_view name,
    const base::SourceRange& range, const bool opaque_allowed_as_pointee, const bool report_unknown)
{
    return this->resolver().find_type_in_visible_modules(
        name_id, name, range, opaque_allowed_as_pointee, report_unknown);
}

SemanticTypeService::SemanticTypeService(SemanticAnalyzerCore& core) noexcept : core_(core)
{
}

SemanticServiceBoundary SemanticTypeService::boundary() const noexcept
{
    return SemanticServiceBoundary{
        SemanticServiceDomain::type,
        SEMA_TYPE_SERVICE_NAME,
        SEMA_TYPE_SERVICE_AUTHORITIES,
        false,
    };
}

bool SemanticTypeService::accepts_authority(const query::QueryKind kind) const noexcept
{
    return supports_authority(this->boundary().authority_kinds, kind);
}

SemanticTypeResolver SemanticTypeService::resolver() noexcept
{
    return this->core_.type_resolver();
}

SemanticTypeValidator SemanticTypeService::validator() const noexcept
{
    return this->core_.type_validator();
}

SemanticAbiChecker SemanticTypeService::abi_checker() const noexcept
{
    return this->core_.abi_checker();
}

TypeHandle SemanticTypeService::resolve_type(const syntax::TypeId type)
{
    return this->resolver().resolve_type(type);
}

bool SemanticTypeService::can_assign(
    const TypeHandle dst, const TypeHandle src, const syntax::ExprId value) const noexcept
{
    return this->validator().can_assign(dst, src, value);
}

bool SemanticTypeService::is_valid_storage_type(const TypeHandle type) const
{
    return this->validator().is_valid_storage_type(type);
}

void SemanticTypeService::validate_type_layouts() const
{
    this->abi_checker().validate_type_layouts();
}

SemanticGenericService::SemanticGenericService(SemanticAnalyzerCore& core) noexcept : core_(core)
{
}

SemanticServiceBoundary SemanticGenericService::boundary() const noexcept
{
    return SemanticServiceBoundary{
        SemanticServiceDomain::generic,
        SEMA_GENERIC_SERVICE_NAME,
        SEMA_GENERIC_SERVICE_AUTHORITIES,
        false,
    };
}

bool SemanticGenericService::accepts_authority(const query::QueryKind kind) const noexcept
{
    return supports_authority(this->boundary().authority_kinds, kind);
}

SemanticAnalyzerCore::GenericAnalyzer SemanticGenericService::analyzer() noexcept
{
    return SemanticAnalyzerCore::GenericAnalyzer(this->core_);
}

bool SemanticGenericService::has_generic_params(const syntax::ItemNode& item) const noexcept
{
    return SemanticAnalyzerCore::GenericAnalyzer(this->core_).has_generic_params(item);
}

void SemanticGenericService::analyze_function_definition(const SemanticAnalyzerCore::GenericTemplateInfo& info)
{
    this->analyzer().analyze_generic_function_definition(info);
}

SemanticBodyCheckService::SemanticBodyCheckService(SemanticAnalyzerCore& core) noexcept : core_(core)
{
}

SemanticServiceBoundary SemanticBodyCheckService::boundary() const noexcept
{
    return SemanticServiceBoundary{
        SemanticServiceDomain::body_check,
        SEMA_BODY_CHECK_SERVICE_NAME,
        SEMA_BODY_CHECK_SERVICE_AUTHORITIES,
        false,
    };
}

bool SemanticBodyCheckService::accepts_authority(const query::QueryKind kind) const noexcept
{
    return supports_authority(this->boundary().authority_kinds, kind);
}

SemanticAnalyzerCore::ExpressionAnalyzer SemanticBodyCheckService::expression_analyzer() noexcept
{
    return this->core_.expression_analyzer();
}

SemanticAnalyzerCore::StatementAnalyzer SemanticBodyCheckService::statement_analyzer() noexcept
{
    return SemanticAnalyzerCore::StatementAnalyzer(this->core_);
}

void SemanticBodyCheckService::analyze_function_body(const syntax::ItemNode& function, const syntax::ItemId function_id)
{
    this->statement_analyzer().analyze_function_body(function, function_id);
}

void SemanticBodyCheckService::analyze_function_body_with_signature(const syntax::ItemNode& function,
    const FunctionLookupKey& key, const FunctionSignature& signature, SemanticAnalyzerCore::FunctionBodyState& state)
{
    this->statement_analyzer().analyze_function_body_with_signature(function, key, signature, state);
}

SemanticServiceBundle::SemanticServiceBundle(SemanticAnalyzerCore& core) noexcept : core_(core)
{
}

SemaContext& SemanticServiceBundle::context() noexcept
{
    return this->core_.ctx_;
}

SemanticLookupService SemanticServiceBundle::lookup() noexcept
{
    return SemanticLookupService(this->core_);
}

SemanticTypeService SemanticServiceBundle::type() noexcept
{
    return SemanticTypeService(this->core_);
}

SemanticGenericService SemanticServiceBundle::generic() noexcept
{
    return SemanticGenericService(this->core_);
}

SemanticBodyCheckService SemanticServiceBundle::body_check() noexcept
{
    return SemanticBodyCheckService(this->core_);
}

SemanticServiceBundle SemanticAnalyzerCore::services() noexcept
{
    return SemanticServiceBundle(*this);
}

SemanticLookupService SemanticAnalyzerCore::lookup_service() noexcept
{
    return SemanticLookupService(*this);
}

SemanticTypeService SemanticAnalyzerCore::type_service() noexcept
{
    return SemanticTypeService(*this);
}

SemanticGenericService SemanticAnalyzerCore::generic_service() noexcept
{
    return SemanticGenericService(*this);
}

SemanticBodyCheckService SemanticAnalyzerCore::body_check_service() noexcept
{
    return SemanticBodyCheckService(*this);
}

} // namespace aurex::sema
