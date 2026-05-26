#pragma once

#include <aurex/driver/invocation.hpp>
#include <aurex/query/query_key.hpp>

#include <array>
#include <span>
#include <string_view>

namespace aurex::driver {

inline constexpr std::string_view DRIVER_IMPORT_ROOT_PACKAGE_IDENTITY_KIND = "import-root";

[[nodiscard]] inline query::PackageKey package_key_from_identity(const std::string_view identity) noexcept
{
    if (identity.empty()) {
        return query::package_key(std::span<const std::string_view>{});
    }
    const std::array<std::string_view, 1> parts{identity};
    return query::package_key(parts);
}

[[nodiscard]] inline query::PackageKey package_key_for_invocation(const CompilerInvocation& invocation) noexcept
{
    return package_key_from_identity(invocation.package_identity);
}

[[nodiscard]] inline query::PackageKey package_key_for_import_root(
    const std::string_view canonical_import_root) noexcept
{
    const std::array<std::string_view, 2> parts{
        DRIVER_IMPORT_ROOT_PACKAGE_IDENTITY_KIND,
        canonical_import_root,
    };
    return query::package_key(parts);
}

} // namespace aurex::driver
