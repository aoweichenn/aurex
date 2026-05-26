#include <aurex/driver/package_identity.hpp>

#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "module_loader_support.hpp"

namespace aurex::driver {
namespace {

inline constexpr std::string_view PACKAGE_MANIFEST_SECTION_PACKAGE = "[package]";
inline constexpr std::string_view PACKAGE_MANIFEST_KEY_NAME = "name";
inline constexpr std::string_view PACKAGE_MANIFEST_KEY_VERSION = "version";
inline constexpr char PACKAGE_MANIFEST_ASSIGNMENT = '=';
inline constexpr char PACKAGE_MANIFEST_COMMENT = '#';
inline constexpr char PACKAGE_MANIFEST_ESCAPE = '\\';
inline constexpr char PACKAGE_MANIFEST_QUOTE = '"';
inline constexpr base::usize PACKAGE_IDENTITY_MANIFEST_FIELD_SEPARATOR_COUNT = 3U;
inline constexpr base::usize PACKAGE_IDENTITY_MANIFEST_SOURCE_ROOT_FIELD_SEPARATOR_COUNT = 4U;
inline constexpr base::usize PACKAGE_IDENTITY_IMPORT_ROOT_FIELD_SEPARATOR_COUNT = 1U;

struct PackageManifestIdentityFields {
    std::string name;
    std::string version;
    std::filesystem::path manifest_path;
    std::filesystem::path manifest_root;
    std::optional<std::filesystem::path> source_root;
};

[[nodiscard]] bool path_exists(const std::filesystem::path& path) noexcept
{
    std::error_code error;
    return std::filesystem::exists(path, error) && !error;
}

[[nodiscard]] bool path_is_directory(const std::filesystem::path& path) noexcept
{
    std::error_code error;
    return std::filesystem::is_directory(path, error) && !error;
}

[[nodiscard]] bool path_is_within_or_equal(
    const std::filesystem::path& child, const std::filesystem::path& parent) noexcept
{
    auto child_part = child.begin();
    auto parent_part = parent.begin();
    while (parent_part != parent.end()) {
        if (child_part == child.end() || *child_part != *parent_part) {
            return false;
        }
        ++child_part;
        ++parent_part;
    }
    return true;
}

[[nodiscard]] std::filesystem::path manifest_search_start(const std::filesystem::path& anchor)
{
    if (anchor.empty()) {
        return {};
    }
    const std::filesystem::path canonical = module_loader_canonical_or_absolute(anchor);
    return path_is_directory(canonical) ? canonical : canonical.parent_path();
}

[[nodiscard]] std::string_view trim_manifest_space(const std::string_view text) noexcept
{
    base::usize begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }
    base::usize end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }
    return text.substr(begin, end - begin);
}

[[nodiscard]] std::string_view strip_manifest_comment(const std::string_view line) noexcept
{
    bool in_string = false;
    bool escaped = false;
    for (base::usize index = 0; index < line.size(); ++index) {
        const char ch = line[index];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (in_string && ch == PACKAGE_MANIFEST_ESCAPE) {
            escaped = true;
            continue;
        }
        if (ch == PACKAGE_MANIFEST_QUOTE) {
            in_string = !in_string;
            continue;
        }
        if (!in_string && ch == PACKAGE_MANIFEST_COMMENT) {
            return trim_manifest_space(line.substr(0, index));
        }
    }
    return trim_manifest_space(line);
}

[[nodiscard]] std::optional<std::string> parse_manifest_string_value(const std::string_view value)
{
    const std::string_view trimmed = trim_manifest_space(value);
    if (trimmed.empty() || trimmed.front() != PACKAGE_MANIFEST_QUOTE) {
        return std::nullopt;
    }

    std::string parsed;
    parsed.reserve(trimmed.size());
    bool escaped = false;
    for (base::usize index = 1; index < trimmed.size(); ++index) {
        const char ch = trimmed[index];
        if (escaped) {
            parsed.push_back(ch);
            escaped = false;
            continue;
        }
        if (ch == PACKAGE_MANIFEST_ESCAPE) {
            escaped = true;
            continue;
        }
        if (ch == PACKAGE_MANIFEST_QUOTE) {
            const std::string_view rest = trim_manifest_space(trimmed.substr(index + 1));
            if (!rest.empty()) {
                return std::nullopt;
            }
            return parsed;
        }
        parsed.push_back(ch);
    }
    return std::nullopt;
}

[[nodiscard]] std::filesystem::path resolve_manifest_source_root(
    const std::filesystem::path& manifest_root, const std::string_view source_root)
{
    std::filesystem::path root{std::string(source_root)};
    if (root.is_relative()) {
        root = manifest_root / root;
    }
    return module_loader_canonical_or_absolute(root);
}

[[nodiscard]] std::optional<PackageManifestIdentityFields> read_manifest_identity_fields(
    const std::filesystem::path& manifest_path)
{
    std::ifstream in(manifest_path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }

    PackageManifestIdentityFields fields;
    fields.version = DRIVER_PACKAGE_MANIFEST_DEFAULT_VERSION;
    fields.manifest_path = module_loader_canonical_or_absolute(manifest_path);
    fields.manifest_root = fields.manifest_path.parent_path();

    bool in_package_section = false;
    std::string line;
    while (std::getline(in, line)) {
        const std::string_view stripped = strip_manifest_comment(line);
        if (stripped.empty()) {
            continue;
        }
        if (stripped.front() == '[') {
            in_package_section = stripped == PACKAGE_MANIFEST_SECTION_PACKAGE;
            continue;
        }
        if (!in_package_section) {
            continue;
        }

        const base::usize assignment = stripped.find(PACKAGE_MANIFEST_ASSIGNMENT);
        if (assignment == std::string_view::npos) {
            continue;
        }
        const std::string_view key = trim_manifest_space(stripped.substr(0, assignment));
        const std::string_view value = stripped.substr(assignment + 1);
        std::optional<std::string> parsed_value = parse_manifest_string_value(value);
        if (!parsed_value.has_value()) {
            continue;
        }
        if (key == PACKAGE_MANIFEST_KEY_NAME) {
            fields.name = std::move(*parsed_value);
        } else if (key == PACKAGE_MANIFEST_KEY_VERSION) {
            fields.version = std::move(*parsed_value);
        } else if (key == DRIVER_PACKAGE_MANIFEST_SOURCE_ROOT_KEY) {
            const std::filesystem::path source_root = resolve_manifest_source_root(fields.manifest_root, *parsed_value);
            if (path_is_within_or_equal(source_root, fields.manifest_root)) {
                fields.source_root = source_root;
            }
        }
    }

    if (fields.name.empty()) {
        return std::nullopt;
    }
    return fields;
}

[[nodiscard]] std::string package_identity_from_manifest_fields(const PackageManifestIdentityFields& fields)
{
    const std::string manifest_root = fields.manifest_root.string();
    const std::string source_root = fields.source_root.has_value() ? fields.source_root->string() : std::string{};
    const base::usize separator_count = fields.source_root.has_value()
        ? PACKAGE_IDENTITY_MANIFEST_SOURCE_ROOT_FIELD_SEPARATOR_COUNT
        : PACKAGE_IDENTITY_MANIFEST_FIELD_SEPARATOR_COUNT;
    std::string identity;
    identity.reserve(DRIVER_MANIFEST_PACKAGE_IDENTITY_KIND.size() + fields.name.size() + fields.version.size()
        + manifest_root.size() + source_root.size() + separator_count);
    identity.append(DRIVER_MANIFEST_PACKAGE_IDENTITY_KIND);
    identity.push_back(DRIVER_PACKAGE_IDENTITY_FIELD_SEPARATOR);
    identity.append(fields.name);
    identity.push_back(DRIVER_PACKAGE_IDENTITY_FIELD_SEPARATOR);
    identity.append(fields.version);
    identity.push_back(DRIVER_PACKAGE_IDENTITY_FIELD_SEPARATOR);
    identity.append(manifest_root);
    if (fields.source_root.has_value()) {
        identity.push_back(DRIVER_PACKAGE_IDENTITY_FIELD_SEPARATOR);
        identity.append(source_root);
    }
    return identity;
}

[[nodiscard]] PackageManifestInfo package_manifest_info_from_fields(const PackageManifestIdentityFields& fields)
{
    return PackageManifestInfo{
        fields.manifest_path,
        fields.manifest_root,
        fields.source_root,
        package_identity_from_manifest_fields(fields),
    };
}

} // namespace

query::PackageKey package_key_from_identity(const std::string_view identity)
{
    if (identity.empty()) {
        return query::package_key(std::span<const std::string_view>{});
    }

    std::vector<std::string_view> parts;
    base::usize begin = 0;
    while (begin <= identity.size()) {
        const base::usize end = identity.find(DRIVER_PACKAGE_IDENTITY_FIELD_SEPARATOR, begin);
        if (end == std::string_view::npos) {
            parts.push_back(identity.substr(begin));
            break;
        }
        parts.push_back(identity.substr(begin, end - begin));
        begin = end + 1;
    }
    return query::package_key(parts);
}

std::optional<std::filesystem::path> find_package_manifest_for_path(const std::filesystem::path& anchor)
{
    std::filesystem::path current = manifest_search_start(anchor);
    while (!current.empty()) {
        const std::filesystem::path candidate = current / DRIVER_PACKAGE_MANIFEST_FILE_NAME;
        if (path_exists(candidate)) {
            return module_loader_canonical_or_absolute(candidate);
        }
        const std::filesystem::path parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }
    return std::nullopt;
}

std::optional<PackageManifestInfo> package_manifest_info_for_path(const std::filesystem::path& anchor)
{
    const std::optional<std::filesystem::path> manifest = find_package_manifest_for_path(anchor);
    if (!manifest.has_value()) {
        return std::nullopt;
    }
    const std::optional<PackageManifestIdentityFields> fields = read_manifest_identity_fields(*manifest);
    if (!fields.has_value()) {
        return std::nullopt;
    }
    return package_manifest_info_from_fields(*fields);
}

std::optional<std::string> package_manifest_identity_for_path(const std::filesystem::path& anchor)
{
    const std::optional<PackageManifestInfo> manifest = package_manifest_info_for_path(anchor);
    if (!manifest.has_value()) {
        return std::nullopt;
    }
    return manifest->identity;
}

std::optional<std::filesystem::path> package_manifest_source_root_for_path(const std::filesystem::path& anchor)
{
    const std::optional<PackageManifestInfo> manifest = package_manifest_info_for_path(anchor);
    if (!manifest.has_value()) {
        return std::nullopt;
    }
    return manifest->source_root;
}

std::string package_identity_for_invocation(const CompilerInvocation& invocation)
{
    if (!invocation.package_identity.empty()) {
        return invocation.package_identity;
    }
    return package_manifest_identity_for_path(invocation.input_path).value_or(std::string{});
}

std::string package_identity_for_import_root(const std::string_view canonical_import_root)
{
    if (const std::optional<std::string> manifest_identity =
            package_manifest_identity_for_path(std::filesystem::path(canonical_import_root));
        manifest_identity.has_value()) {
        return *manifest_identity;
    }
    std::string identity;
    identity.reserve(DRIVER_IMPORT_ROOT_PACKAGE_IDENTITY_KIND.size() + canonical_import_root.size()
        + PACKAGE_IDENTITY_IMPORT_ROOT_FIELD_SEPARATOR_COUNT);
    identity.append(DRIVER_IMPORT_ROOT_PACKAGE_IDENTITY_KIND);
    identity.push_back(DRIVER_PACKAGE_IDENTITY_FIELD_SEPARATOR);
    identity.append(canonical_import_root);
    return identity;
}

std::optional<std::filesystem::path> package_source_root_for_invocation(const CompilerInvocation& invocation)
{
    return package_manifest_source_root_for_path(invocation.input_path);
}

std::optional<std::filesystem::path> package_source_root_for_import_root(const std::string_view canonical_import_root)
{
    return package_manifest_source_root_for_path(std::filesystem::path(canonical_import_root));
}

query::PackageKey package_key_for_invocation(const CompilerInvocation& invocation)
{
    return package_key_from_identity(package_identity_for_invocation(invocation));
}

query::PackageKey package_key_for_import_root(const std::string_view canonical_import_root)
{
    return package_key_from_identity(package_identity_for_import_root(canonical_import_root));
}

} // namespace aurex::driver
