#pragma once

#include <aurex/application/driver/invocation.hpp>
#include <aurex/infrastructure/query/query_key.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace aurex::driver {

inline constexpr std::string_view DRIVER_IMPORT_ROOT_PACKAGE_IDENTITY_KIND = "import-root";
inline constexpr std::string_view DRIVER_MANIFEST_PACKAGE_IDENTITY_KIND = "manifest";
inline constexpr std::string_view DRIVER_PACKAGE_MANIFEST_FILE_NAME = "aurex.toml";
inline constexpr std::string_view DRIVER_PACKAGE_MANIFEST_DEFAULT_VERSION = "0";
inline constexpr std::string_view DRIVER_PACKAGE_MANIFEST_SOURCE_ROOT_KEY = "source-root";
inline constexpr char DRIVER_PACKAGE_IDENTITY_FIELD_SEPARATOR = '\x1f';

struct PackageManifestInfo {
    std::filesystem::path manifest_path;
    std::filesystem::path manifest_root;
    std::optional<std::filesystem::path> source_root;
    std::string identity;
};

[[nodiscard]] query::PackageKey package_key_from_identity(std::string_view identity);
[[nodiscard]] std::optional<std::filesystem::path> find_package_manifest_for_path(const std::filesystem::path& anchor);
[[nodiscard]] std::optional<PackageManifestInfo> package_manifest_info_for_path(const std::filesystem::path& anchor);
[[nodiscard]] std::optional<std::string> package_manifest_identity_for_path(const std::filesystem::path& anchor);
[[nodiscard]] std::optional<std::filesystem::path> package_manifest_source_root_for_path(
    const std::filesystem::path& anchor);
[[nodiscard]] std::string package_identity_for_invocation(const CompilerInvocation& invocation);
[[nodiscard]] std::string package_identity_for_import_root(std::string_view canonical_import_root);
[[nodiscard]] std::optional<std::filesystem::path> package_source_root_for_invocation(
    const CompilerInvocation& invocation);
[[nodiscard]] std::optional<std::filesystem::path> package_source_root_for_import_root(
    std::string_view canonical_import_root);
[[nodiscard]] query::PackageKey package_key_for_invocation(const CompilerInvocation& invocation);
[[nodiscard]] query::PackageKey package_key_for_import_root(std::string_view canonical_import_root);

} // namespace aurex::driver
