#include "../io.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <sstream>

namespace aurex::driver::incremental_cache_detail {

[[nodiscard]] std::filesystem::path canonical_or_absolute(const std::filesystem::path& path)
{
    std::error_code canonical_error;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(path, canonical_error);
    if (!canonical_error) {
        return canonical;
    }

    std::error_code absolute_error;
    std::filesystem::path absolute = std::filesystem::absolute(path, absolute_error);
    return absolute_error ? path : absolute;
}

[[nodiscard]] sema::StableFingerprint128 fingerprint_text(const std::string_view text) noexcept
{
    return sema::stable_fingerprint(text);
}

[[nodiscard]] std::optional<std::string> read_file_for_fingerprint(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return std::nullopt;
    }

    std::string text;
    std::error_code size_error;
    const std::uintmax_t size = std::filesystem::file_size(path, size_error);
    if (!size_error) {
        text.resize(static_cast<std::size_t>(size));
        if (!text.empty()) {
            input.read(text.data(), static_cast<std::streamsize>(text.size()));
            if (!input) {
                return std::nullopt;
            }
        }
        return text;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (input.bad()) {
        return std::nullopt;
    }
    return buffer.str();
}

[[nodiscard]] bool same_fingerprint(const SourceFingerprintRecord& lhs, const SourceFingerprintRecord& rhs) noexcept
{
    return lhs.size == rhs.size && lhs.fingerprint.primary == rhs.fingerprint.primary
        && lhs.fingerprint.secondary == rhs.fingerprint.secondary
        && lhs.fingerprint.byte_count == rhs.fingerprint.byte_count;
}

[[nodiscard]] std::optional<SourceFingerprintRecord> fingerprint_file(const std::filesystem::path& path)
{
    const std::optional<std::string> text = read_file_for_fingerprint(path);
    if (!text) {
        return std::nullopt;
    }
    return SourceFingerprintRecord{
        canonical_or_absolute(path),
        text->size(),
        fingerprint_text(*text),
    };
}

[[nodiscard]] std::vector<std::filesystem::path> normalized_import_paths(const CompilerInvocation& invocation)
{
    std::vector<std::filesystem::path> paths;
    paths.reserve(invocation.import_paths.size());
    for (const std::filesystem::path& path : invocation.import_paths) {
        paths.push_back(canonical_or_absolute(path));
    }
    return paths;
}

[[nodiscard]] std::vector<SourceFingerprintRecord> collect_source_fingerprints(const base::SourceManager& sources)
{
    std::vector<SourceFingerprintRecord> records;
    records.reserve(sources.files().size());
    for (const base::SourceFile& file : sources.files()) {
        records.push_back(SourceFingerprintRecord{
            canonical_or_absolute(std::filesystem::path(file.path())),
            file.text().size(),
            fingerprint_text(file.text()),
        });
    }
    std::sort(
        records.begin(), records.end(), [](const SourceFingerprintRecord& lhs, const SourceFingerprintRecord& rhs) {
            return lhs.path.string() < rhs.path.string();
        });
    return records;
}

[[nodiscard]] std::vector<ModuleRecord> sorted_modules(const std::span<const ModuleRecord> modules)
{
    std::vector<ModuleRecord> sorted;
    sorted.reserve(modules.size());
    for (const ModuleRecord& module : modules) {
        sorted.push_back(ModuleRecord{module.name, canonical_or_absolute(module.path)});
    }
    std::sort(sorted.begin(), sorted.end(), [](const ModuleRecord& lhs, const ModuleRecord& rhs) {
        if (lhs.name != rhs.name) {
            return lhs.name < rhs.name;
        }
        return lhs.path.string() < rhs.path.string();
    });
    return sorted;
}

} // namespace aurex::driver::incremental_cache_detail
