#pragma once

#include <aurex/query/query_key.hpp>
#include <aurex/syntax/ast/nodes.hpp>

namespace aurex::sema {

struct DeclContext {
    query::PackageKey package;
    query::ModuleKey module;
    query::ModulePartKey part;
};

struct AccessContext {
    query::PackageKey package;
    query::ModuleKey module;
    query::ModulePartKey part;
};

[[nodiscard]] inline DeclContext decl_context_from_module_key(
    const query::ModuleKey& module, const query::ModulePartKey& part = {}) noexcept
{
    return DeclContext{
        module.package,
        module,
        part,
    };
}

[[nodiscard]] inline AccessContext access_context_from_module_key(
    const query::ModuleKey& module, const query::ModulePartKey& part = {}) noexcept
{
    return AccessContext{
        module.package,
        module,
        part,
    };
}

[[nodiscard]] inline bool access_context_same_module(
    const DeclContext& declaration, const AccessContext& access) noexcept
{
    return query::is_valid(declaration.module) && query::is_valid(access.module) && declaration.module == access.module;
}

[[nodiscard]] inline bool access_context_same_package(
    const DeclContext& declaration, const AccessContext& access) noexcept
{
    return query::is_valid(declaration.module) && query::is_valid(access.module)
        && declaration.package == access.package;
}

class VisibilityPolicy final {
public:
    [[nodiscard]] bool can_access(
        const syntax::Visibility visibility, const DeclContext& declaration, const AccessContext& access) const noexcept
    {
        if (syntax::visibility_is_public(visibility)) {
            return true;
        }
        if (syntax::visibility_at_least(visibility, syntax::Visibility::package_)) {
            return access_context_same_package(declaration, access);
        }
        return access_context_same_module(declaration, access);
    }

    [[nodiscard]] bool can_expose_type(
        const syntax::Visibility surface_visibility, const syntax::Visibility type_visibility) const noexcept
    {
        return syntax::visibility_at_least(type_visibility, surface_visibility);
    }
};

} // namespace aurex::sema
