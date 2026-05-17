#include <aurex/driver/incremental_cache.hpp>

#include <aurex/base/config.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace aurex::driver {
namespace {

constexpr std::string_view INCREMENTAL_CACHE_MAGIC = "aurex-incremental-cache-v1";
constexpr base::u64 INCREMENTAL_CACHE_SCHEMA_VERSION = 1;
constexpr std::string_view INCREMENTAL_CACHE_MODE_SEMANTIC_OK = "semantic-ok";
constexpr char INCREMENTAL_CACHE_SEPARATOR = '\t';
constexpr char INCREMENTAL_CACHE_HEX_DIGIT_PREFIX = '0';
constexpr char INCREMENTAL_CACHE_HEX_ALPHA_PREFIX = 'a';
constexpr int INCREMENTAL_CACHE_DECIMAL_BASE = 10;
constexpr int INCREMENTAL_CACHE_HEX_LETTER_OFFSET = 10;
constexpr int INCREMENTAL_CACHE_HEX_BYTE_SHIFT = 4;
constexpr int INCREMENTAL_CACHE_INVALID_HEX_NIBBLE = -1;
constexpr unsigned char INCREMENTAL_CACHE_HEX_LOW_MASK = 0x0fU;
constexpr base::usize INCREMENTAL_CACHE_HEX_BYTE_WIDTH = 2;
constexpr base::usize INCREMENTAL_CACHE_KIND_FIELD = 0;
constexpr base::usize INCREMENTAL_CACHE_FIRST_VALUE_FIELD = 1;
constexpr base::usize INCREMENTAL_CACHE_HEADER_FIELD_COUNT = 2;
constexpr base::usize INCREMENTAL_CACHE_SOURCE_FIELD_COUNT = 6;
constexpr base::usize INCREMENTAL_CACHE_MODULE_FIELD_COUNT = 3;
constexpr base::usize INCREMENTAL_CACHE_DEF_FIELD_COUNT = 12;
constexpr base::usize INCREMENTAL_CACHE_SOURCE_SIZE_FIELD = 1;
constexpr base::usize INCREMENTAL_CACHE_SOURCE_PRIMARY_FIELD = 2;
constexpr base::usize INCREMENTAL_CACHE_SOURCE_SECONDARY_FIELD = 3;
constexpr base::usize INCREMENTAL_CACHE_SOURCE_BYTES_FIELD = 4;
constexpr base::usize INCREMENTAL_CACHE_SOURCE_PATH_FIELD = 5;
constexpr base::usize INCREMENTAL_CACHE_MODULE_NAME_FIELD = 1;
constexpr base::usize INCREMENTAL_CACHE_MODULE_PATH_FIELD = 2;
constexpr base::usize INCREMENTAL_CACHE_DEF_CATEGORY_FIELD = 1;
constexpr base::usize INCREMENTAL_CACHE_DEF_STABLE_KIND_FIELD = 2;
constexpr base::usize INCREMENTAL_CACHE_DEF_STABLE_GLOBAL_FIELD = 3;
constexpr base::usize INCREMENTAL_CACHE_DEF_STABLE_PRIMARY_FIELD = 4;
constexpr base::usize INCREMENTAL_CACHE_DEF_STABLE_SECONDARY_FIELD = 5;
constexpr base::usize INCREMENTAL_CACHE_DEF_STABLE_BYTES_FIELD = 6;
constexpr base::usize INCREMENTAL_CACHE_DEF_INCREMENTAL_GLOBAL_FIELD = 7;
constexpr base::usize INCREMENTAL_CACHE_DEF_INCREMENTAL_PRIMARY_FIELD = 8;
constexpr base::usize INCREMENTAL_CACHE_DEF_INCREMENTAL_SECONDARY_FIELD = 9;
constexpr base::usize INCREMENTAL_CACHE_DEF_INCREMENTAL_BYTES_FIELD = 10;
constexpr base::usize INCREMENTAL_CACHE_DEF_NAME_FIELD = 11;

constexpr std::string_view INCREMENTAL_CACHE_FIELD_SCHEMA = "schema";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_COMPILER = "compiler";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_MODE = "mode";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_ROOT = "root";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_IMPORT_PATHS = "import_paths";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_IMPORT_PATH = "import_path";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_SOURCES = "sources";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_SOURCE = "source";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_MODULES = "modules";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_MODULE = "module";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_DEFINITIONS = "definitions";
constexpr std::string_view INCREMENTAL_CACHE_FIELD_DEF = "def";

constexpr std::string_view INCREMENTAL_CACHE_CATEGORY_FUNCTION = "function";
constexpr std::string_view INCREMENTAL_CACHE_CATEGORY_GENERIC_FUNCTION_INSTANCE = "generic_function_instance";
constexpr std::string_view INCREMENTAL_CACHE_CATEGORY_STRUCT = "struct";
constexpr std::string_view INCREMENTAL_CACHE_CATEGORY_ENUM_CASE = "enum_case";
constexpr std::string_view INCREMENTAL_CACHE_CATEGORY_TYPE_ALIAS = "type_alias";

constexpr std::string_view INCREMENTAL_CACHE_WRITE_OPEN_FAILED =
    "failed to open incremental cache file for writing";
constexpr std::string_view INCREMENTAL_CACHE_WRITE_FAILED =
    "failed to write incremental cache file";
constexpr std::string_view INCREMENTAL_CACHE_RENAME_FAILED =
    "failed to publish incremental cache file";
constexpr std::string_view INCREMENTAL_CACHE_DIRECTORY_FAILED =
    "failed to create incremental cache directory";

struct SourceFingerprintRecord {
    std::filesystem::path path;
    base::usize size = 0;
    sema::StableFingerprint128 fingerprint;
};

struct DefinitionRecord {
    std::string category;
    std::string name;
    sema::StableDefId stable_id;
    sema::IncrementalKey incremental_key;
};

struct ParsedCache {
    base::u64 schema = 0;
    std::string compiler_version;
    std::string mode;
    std::filesystem::path root_path;
    std::vector<std::filesystem::path> import_paths;
    std::vector<SourceFingerprintRecord> sources;
    base::usize module_count = 0;
    base::usize definition_count = 0;
    std::optional<base::usize> expected_import_paths;
    std::optional<base::usize> expected_sources;
    std::optional<base::usize> expected_modules;
    std::optional<base::usize> expected_definitions;
};

[[nodiscard]] std::filesystem::path canonical_or_absolute(const std::filesystem::path& path) {
    std::error_code canonical_error;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(path, canonical_error);
    if (!canonical_error) {
        return canonical;
    }

    std::error_code absolute_error;
    std::filesystem::path absolute = std::filesystem::absolute(path, absolute_error);
    return absolute_error ? path : absolute;
}

[[nodiscard]] sema::StableFingerprint128 fingerprint_text(const std::string_view text) noexcept {
    return sema::stable_fingerprint(text);
}

[[nodiscard]] std::optional<std::string> read_file_for_fingerprint(
    const std::filesystem::path& path
) {
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

[[nodiscard]] std::optional<SourceFingerprintRecord> fingerprint_file(
    const std::filesystem::path& path
) {
    const std::optional<std::string> text = read_file_for_fingerprint(path);
    if (!text) {
        return std::nullopt;
    }
    return SourceFingerprintRecord {
        canonical_or_absolute(path),
        text->size(),
        fingerprint_text(*text),
    };
}

[[nodiscard]] bool same_fingerprint(
    const SourceFingerprintRecord& lhs,
    const SourceFingerprintRecord& rhs
) noexcept {
    return lhs.size == rhs.size &&
           lhs.fingerprint.primary == rhs.fingerprint.primary &&
           lhs.fingerprint.secondary == rhs.fingerprint.secondary &&
           lhs.fingerprint.byte_count == rhs.fingerprint.byte_count;
}

[[nodiscard]] bool parse_u64(const std::string_view text, base::u64& value) noexcept {
    base::u64 parsed = 0;
    const char* const begin = text.data();
    const char* const end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, parsed, INCREMENTAL_CACHE_DECIMAL_BASE);
    if (result.ec != std::errc {} || result.ptr != end) {
        return false;
    }
    value = parsed;
    return true;
}

[[nodiscard]] bool parse_usize(const std::string_view text, base::usize& value) noexcept {
    base::u64 parsed = 0;
    if (!parse_u64(text, parsed) || parsed > std::numeric_limits<base::usize>::max()) {
        return false;
    }
    value = static_cast<base::usize>(parsed);
    return true;
}

[[nodiscard]] bool parse_u32(const std::string_view text, base::u32& value) noexcept {
    base::u64 parsed = 0;
    if (!parse_u64(text, parsed) || parsed > std::numeric_limits<base::u32>::max()) {
        return false;
    }
    value = static_cast<base::u32>(parsed);
    return true;
}

[[nodiscard]] int hex_nibble(const char value) noexcept {
    if (value >= '0' && value <= '9') {
        return value - INCREMENTAL_CACHE_HEX_DIGIT_PREFIX;
    }
    if (value >= 'a' && value <= 'f') {
        return value - INCREMENTAL_CACHE_HEX_ALPHA_PREFIX + INCREMENTAL_CACHE_HEX_LETTER_OFFSET;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + INCREMENTAL_CACHE_HEX_LETTER_OFFSET;
    }
    return INCREMENTAL_CACHE_INVALID_HEX_NIBBLE;
}

void append_hex_string(std::ostream& out, const std::string_view value) {
    static constexpr char DIGITS[] = "0123456789abcdef";
    for (const unsigned char byte : value) {
        out << DIGITS[byte >> INCREMENTAL_CACHE_HEX_BYTE_SHIFT]
            << DIGITS[byte & INCREMENTAL_CACHE_HEX_LOW_MASK];
    }
}

[[nodiscard]] std::optional<std::string> decode_hex_string(const std::string_view encoded) {
    if (encoded.size() % INCREMENTAL_CACHE_HEX_BYTE_WIDTH != 0) {
        return std::nullopt;
    }

    std::string decoded;
    decoded.reserve(encoded.size() / INCREMENTAL_CACHE_HEX_BYTE_WIDTH);
    for (base::usize index = 0; index < encoded.size(); index += INCREMENTAL_CACHE_HEX_BYTE_WIDTH) {
        const int high = hex_nibble(encoded[index]);
        const int low = hex_nibble(encoded[index + 1]);
        if (high == INCREMENTAL_CACHE_INVALID_HEX_NIBBLE ||
            low == INCREMENTAL_CACHE_INVALID_HEX_NIBBLE) {
            return std::nullopt;
        }
        decoded.push_back(static_cast<char>((high << INCREMENTAL_CACHE_HEX_BYTE_SHIFT) | low));
    }
    return decoded;
}

[[nodiscard]] std::optional<std::filesystem::path> decode_path(const std::string_view encoded) {
    const std::optional<std::string> decoded = decode_hex_string(encoded);
    if (!decoded) {
        return std::nullopt;
    }
    return std::filesystem::path(*decoded);
}

[[nodiscard]] std::vector<std::string_view> split_cache_fields(const std::string_view line) {
    std::vector<std::string_view> fields;
    base::usize begin = 0;
    for (;;) {
        const base::usize end = line.find(INCREMENTAL_CACHE_SEPARATOR, begin);
        if (end == std::string_view::npos) {
            fields.push_back(line.substr(begin));
            return fields;
        }
        fields.push_back(line.substr(begin, end - begin));
        begin = end + 1;
    }
}

[[nodiscard]] bool assign_count(
    std::optional<base::usize>& target,
    const std::string_view value
) noexcept {
    if (target) {
        return false;
    }
    base::usize parsed = 0;
    if (!parse_usize(value, parsed)) {
        return false;
    }
    target = parsed;
    return true;
}

[[nodiscard]] bool parse_header_line(
    ParsedCache& cache,
    const std::vector<std::string_view>& fields
) {
    if (fields.size() != INCREMENTAL_CACHE_HEADER_FIELD_COUNT) {
        return false;
    }

    const std::string_view kind = fields[INCREMENTAL_CACHE_KIND_FIELD];
    const std::string_view value = fields[INCREMENTAL_CACHE_FIRST_VALUE_FIELD];
    if (kind == INCREMENTAL_CACHE_FIELD_SCHEMA) {
        return cache.schema == 0 && parse_u64(value, cache.schema);
    }
    if (kind == INCREMENTAL_CACHE_FIELD_COMPILER) {
        std::optional<std::string> compiler = decode_hex_string(value);
        if (!compiler || !cache.compiler_version.empty()) {
            return false;
        }
        cache.compiler_version = std::move(*compiler);
        return true;
    }
    if (kind == INCREMENTAL_CACHE_FIELD_MODE) {
        std::optional<std::string> mode = decode_hex_string(value);
        if (!mode || !cache.mode.empty()) {
            return false;
        }
        cache.mode = std::move(*mode);
        return true;
    }
    if (kind == INCREMENTAL_CACHE_FIELD_ROOT) {
        const std::optional<std::filesystem::path> path = decode_path(value);
        if (!path || !cache.root_path.empty()) {
            return false;
        }
        cache.root_path = *path;
        return true;
    }
    if (kind == INCREMENTAL_CACHE_FIELD_IMPORT_PATHS) {
        return assign_count(cache.expected_import_paths, value);
    }
    if (kind == INCREMENTAL_CACHE_FIELD_SOURCES) {
        return assign_count(cache.expected_sources, value);
    }
    if (kind == INCREMENTAL_CACHE_FIELD_MODULES) {
        return assign_count(cache.expected_modules, value);
    }
    if (kind == INCREMENTAL_CACHE_FIELD_DEFINITIONS) {
        return assign_count(cache.expected_definitions, value);
    }
    return false;
}

[[nodiscard]] bool parse_import_path_line(
    ParsedCache& cache,
    const std::vector<std::string_view>& fields
) {
    if (fields.size() != INCREMENTAL_CACHE_HEADER_FIELD_COUNT ||
        fields[INCREMENTAL_CACHE_KIND_FIELD] != INCREMENTAL_CACHE_FIELD_IMPORT_PATH) {
        return false;
    }
    const std::optional<std::filesystem::path> path = decode_path(fields[INCREMENTAL_CACHE_FIRST_VALUE_FIELD]);
    if (!path) {
        return false;
    }
    cache.import_paths.push_back(*path);
    return true;
}

[[nodiscard]] bool parse_source_line(
    ParsedCache& cache,
    const std::vector<std::string_view>& fields
) {
    if (fields.size() != INCREMENTAL_CACHE_SOURCE_FIELD_COUNT ||
        fields[INCREMENTAL_CACHE_KIND_FIELD] != INCREMENTAL_CACHE_FIELD_SOURCE) {
        return false;
    }

    SourceFingerprintRecord record;
    base::u64 size = 0;
    if (!parse_u64(fields[INCREMENTAL_CACHE_SOURCE_SIZE_FIELD], size) ||
        size > std::numeric_limits<base::usize>::max() ||
        !parse_u64(fields[INCREMENTAL_CACHE_SOURCE_PRIMARY_FIELD], record.fingerprint.primary) ||
        !parse_u64(fields[INCREMENTAL_CACHE_SOURCE_SECONDARY_FIELD], record.fingerprint.secondary) ||
        !parse_u32(fields[INCREMENTAL_CACHE_SOURCE_BYTES_FIELD], record.fingerprint.byte_count)) {
        return false;
    }
    record.size = static_cast<base::usize>(size);

    const std::optional<std::filesystem::path> path = decode_path(fields[INCREMENTAL_CACHE_SOURCE_PATH_FIELD]);
    if (!path) {
        return false;
    }
    record.path = *path;
    cache.sources.push_back(std::move(record));
    return true;
}

[[nodiscard]] bool parse_module_line(
    ParsedCache& cache,
    const std::vector<std::string_view>& fields
) {
    if (fields.size() != INCREMENTAL_CACHE_MODULE_FIELD_COUNT ||
        fields[INCREMENTAL_CACHE_KIND_FIELD] != INCREMENTAL_CACHE_FIELD_MODULE ||
        !decode_hex_string(fields[INCREMENTAL_CACHE_MODULE_NAME_FIELD]) ||
        !decode_path(fields[INCREMENTAL_CACHE_MODULE_PATH_FIELD])) {
        return false;
    }
    cache.module_count += 1;
    return true;
}

[[nodiscard]] bool parse_definition_line(
    ParsedCache& cache,
    const std::vector<std::string_view>& fields
) {
    if (fields.size() != INCREMENTAL_CACHE_DEF_FIELD_COUNT ||
        fields[INCREMENTAL_CACHE_KIND_FIELD] != INCREMENTAL_CACHE_FIELD_DEF ||
        fields[INCREMENTAL_CACHE_DEF_CATEGORY_FIELD].empty() ||
        fields[INCREMENTAL_CACHE_DEF_STABLE_KIND_FIELD].empty() ||
        !decode_hex_string(fields[INCREMENTAL_CACHE_DEF_NAME_FIELD])) {
        return false;
    }

    base::u64 stable_global = 0;
    base::u64 stable_primary = 0;
    base::u64 stable_secondary = 0;
    base::u32 stable_bytes = 0;
    base::u64 incremental_global = 0;
    base::u64 incremental_primary = 0;
    base::u64 incremental_secondary = 0;
    base::u32 incremental_bytes = 0;
    if (!parse_u64(fields[INCREMENTAL_CACHE_DEF_STABLE_GLOBAL_FIELD], stable_global) ||
        !parse_u64(fields[INCREMENTAL_CACHE_DEF_STABLE_PRIMARY_FIELD], stable_primary) ||
        !parse_u64(fields[INCREMENTAL_CACHE_DEF_STABLE_SECONDARY_FIELD], stable_secondary) ||
        !parse_u32(fields[INCREMENTAL_CACHE_DEF_STABLE_BYTES_FIELD], stable_bytes) ||
        !parse_u64(fields[INCREMENTAL_CACHE_DEF_INCREMENTAL_GLOBAL_FIELD], incremental_global) ||
        !parse_u64(fields[INCREMENTAL_CACHE_DEF_INCREMENTAL_PRIMARY_FIELD], incremental_primary) ||
        !parse_u64(fields[INCREMENTAL_CACHE_DEF_INCREMENTAL_SECONDARY_FIELD], incremental_secondary) ||
        !parse_u32(fields[INCREMENTAL_CACHE_DEF_INCREMENTAL_BYTES_FIELD], incremental_bytes)) {
        return false;
    }
    cache.definition_count += 1;
    return true;
}

[[nodiscard]] bool parse_cache_line(
    ParsedCache& cache,
    const std::string_view line
) {
    if (line.empty()) {
        return true;
    }

    const std::vector<std::string_view> fields = split_cache_fields(line);
    const std::string_view kind = fields[INCREMENTAL_CACHE_KIND_FIELD];
    if (kind == INCREMENTAL_CACHE_FIELD_IMPORT_PATH) {
        return parse_import_path_line(cache, fields);
    }
    if (kind == INCREMENTAL_CACHE_FIELD_SOURCE) {
        return parse_source_line(cache, fields);
    }
    if (kind == INCREMENTAL_CACHE_FIELD_MODULE) {
        return parse_module_line(cache, fields);
    }
    if (kind == INCREMENTAL_CACHE_FIELD_DEF) {
        return parse_definition_line(cache, fields);
    }
    return parse_header_line(cache, fields);
}

[[nodiscard]] bool parsed_cache_counts_match(const ParsedCache& cache) noexcept {
    return cache.expected_import_paths &&
           cache.expected_sources &&
           cache.expected_modules &&
           cache.expected_definitions &&
           cache.import_paths.size() == *cache.expected_import_paths &&
           cache.sources.size() == *cache.expected_sources &&
           cache.module_count == *cache.expected_modules &&
           cache.definition_count == *cache.expected_definitions;
}

[[nodiscard]] bool parsed_cache_header_matches(
    const ParsedCache& cache,
    const CompilerInvocation& invocation
) {
    if (cache.schema != INCREMENTAL_CACHE_SCHEMA_VERSION ||
        cache.compiler_version != base::config::AUREX_VERSION_STRING ||
        cache.mode != INCREMENTAL_CACHE_MODE_SEMANTIC_OK ||
        cache.root_path != canonical_or_absolute(invocation.input_path) ||
        !parsed_cache_counts_match(cache)) {
        return false;
    }

    if (cache.import_paths.size() != invocation.import_paths.size()) {
        return false;
    }
    for (base::usize index = 0; index < cache.import_paths.size(); ++index) {
        if (cache.import_paths[index] != canonical_or_absolute(invocation.import_paths[index])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::optional<ParsedCache> read_incremental_cache(
    const std::filesystem::path& cache_path
) {
    std::error_code exists_error;
    if (!std::filesystem::exists(cache_path, exists_error) || exists_error) {
        return std::nullopt;
    }

    std::ifstream input(cache_path, std::ios::binary);
    if (!input) {
        return std::nullopt;
    }

    std::string line;
    if (!std::getline(input, line) || line != INCREMENTAL_CACHE_MAGIC) {
        return std::nullopt;
    }

    ParsedCache cache;
    while (std::getline(input, line)) {
        if (!parse_cache_line(cache, line)) {
            return std::nullopt;
        }
    }
    if (input.bad()) {
        return std::nullopt;
    }
    return cache;
}

[[nodiscard]] bool cache_sources_match(const ParsedCache& cache) {
    bool has_root_source = false;
    for (const SourceFingerprintRecord& cached_source : cache.sources) {
        if (cached_source.path == cache.root_path) {
            has_root_source = true;
        }
        std::optional<SourceFingerprintRecord> current = fingerprint_file(cached_source.path);
        if (!current || !same_fingerprint(cached_source, *current)) {
            return false;
        }
    }
    return has_root_source;
}

[[nodiscard]] std::vector<std::filesystem::path> normalized_import_paths(
    const CompilerInvocation& invocation
) {
    std::vector<std::filesystem::path> paths;
    paths.reserve(invocation.import_paths.size());
    for (const std::filesystem::path& path : invocation.import_paths) {
        paths.push_back(canonical_or_absolute(path));
    }
    return paths;
}

[[nodiscard]] std::vector<SourceFingerprintRecord> collect_source_fingerprints(
    const base::SourceManager& sources
) {
    std::vector<SourceFingerprintRecord> records;
    records.reserve(sources.files().size());
    for (const base::SourceFile& file : sources.files()) {
        records.push_back(SourceFingerprintRecord {
            canonical_or_absolute(std::filesystem::path(file.path())),
            file.text().size(),
            fingerprint_text(file.text()),
        });
    }
    std::sort(records.begin(), records.end(), [](const SourceFingerprintRecord& lhs, const SourceFingerprintRecord& rhs) {
        return lhs.path.string() < rhs.path.string();
    });
    return records;
}

[[nodiscard]] std::vector<ModuleRecord> sorted_modules(
    const std::span<const ModuleRecord> modules
) {
    std::vector<ModuleRecord> sorted;
    sorted.reserve(modules.size());
    for (const ModuleRecord& module : modules) {
        sorted.push_back(ModuleRecord {module.name, canonical_or_absolute(module.path)});
    }
    std::sort(sorted.begin(), sorted.end(), [](const ModuleRecord& lhs, const ModuleRecord& rhs) {
        if (lhs.name != rhs.name) {
            return lhs.name < rhs.name;
        }
        return lhs.path.string() < rhs.path.string();
    });
    return sorted;
}

[[nodiscard]] std::string_view stable_symbol_kind_name(const sema::StableSymbolKind kind) noexcept {
    static constexpr auto NAMES = std::to_array<std::string_view>({
        "invalid",
        "type",
        "function",
        "method",
        "value",
        "enum_case",
        "struct_field",
        "generic_template",
        "synthetic",
    });
    const base::usize index = static_cast<base::usize>(kind);
    return index < NAMES.size() ? NAMES[index] : NAMES.front();
}

void push_definition(
    std::vector<DefinitionRecord>& records,
    const std::string_view category,
    const std::string_view name,
    const sema::StableDefId& stable_id,
    const sema::IncrementalKey& incremental_key
) {
    records.push_back(DefinitionRecord {
        std::string(category),
        std::string(name),
        stable_id,
        incremental_key,
    });
}

[[nodiscard]] std::vector<DefinitionRecord> collect_definitions(
    const sema::CheckedModule& checked
) {
    std::vector<DefinitionRecord> records;
    records.reserve(
        checked.functions.size() +
        checked.generic_function_instances.size() +
        checked.structs.size() +
        checked.enum_cases.size() +
        checked.type_aliases.size()
    );

    for (const auto& entry : checked.functions) {
        const sema::FunctionSignature& signature = entry.second;
        push_definition(
            records,
            INCREMENTAL_CACHE_CATEGORY_FUNCTION,
            signature.name.view(),
            signature.stable_id,
            signature.incremental_key
        );
    }
    for (const sema::GenericFunctionInstanceInfo& instance : checked.generic_function_instances) {
        push_definition(
            records,
            INCREMENTAL_CACHE_CATEGORY_GENERIC_FUNCTION_INSTANCE,
            instance.signature.name.view(),
            instance.signature.stable_id,
            instance.signature.incremental_key
        );
    }
    for (const auto& entry : checked.structs) {
        const sema::StructInfo& info = entry.second;
        push_definition(
            records,
            INCREMENTAL_CACHE_CATEGORY_STRUCT,
            info.name.view(),
            info.stable_id,
            info.incremental_key
        );
    }
    for (const auto& entry : checked.enum_cases) {
        const sema::EnumCaseInfo& info = entry.second;
        push_definition(
            records,
            INCREMENTAL_CACHE_CATEGORY_ENUM_CASE,
            info.name.view(),
            info.stable_id,
            info.incremental_key
        );
    }
    for (const auto& entry : checked.type_aliases) {
        const sema::TypeAliasInfo& info = entry.second;
        push_definition(
            records,
            INCREMENTAL_CACHE_CATEGORY_TYPE_ALIAS,
            info.name.view(),
            info.stable_id,
            info.incremental_key
        );
    }

    std::sort(records.begin(), records.end(), [](const DefinitionRecord& lhs, const DefinitionRecord& rhs) {
        if (lhs.category != rhs.category) {
            return lhs.category < rhs.category;
        }
        if (lhs.name != rhs.name) {
            return lhs.name < rhs.name;
        }
        if (lhs.stable_id.global_id != rhs.stable_id.global_id) {
            return lhs.stable_id.global_id < rhs.stable_id.global_id;
        }
        return lhs.incremental_key.global_id < rhs.incremental_key.global_id;
    });
    return records;
}

void write_hex_field(std::ostream& out, const std::string_view value) {
    append_hex_string(out, value);
}

void write_header_field(
    std::ostream& out,
    const std::string_view name,
    const std::string_view encoded_value
) {
    out << name << INCREMENTAL_CACHE_SEPARATOR << encoded_value << '\n';
}

void write_encoded_header_field(
    std::ostream& out,
    const std::string_view name,
    const std::string_view value
) {
    out << name << INCREMENTAL_CACHE_SEPARATOR;
    write_hex_field(out, value);
    out << '\n';
}

void write_source_record(std::ostream& out, const SourceFingerprintRecord& record) {
    out << INCREMENTAL_CACHE_FIELD_SOURCE
        << INCREMENTAL_CACHE_SEPARATOR << record.size
        << INCREMENTAL_CACHE_SEPARATOR << record.fingerprint.primary
        << INCREMENTAL_CACHE_SEPARATOR << record.fingerprint.secondary
        << INCREMENTAL_CACHE_SEPARATOR << record.fingerprint.byte_count
        << INCREMENTAL_CACHE_SEPARATOR;
    write_hex_field(out, record.path.string());
    out << '\n';
}

void write_module_record(std::ostream& out, const ModuleRecord& record) {
    out << INCREMENTAL_CACHE_FIELD_MODULE << INCREMENTAL_CACHE_SEPARATOR;
    write_hex_field(out, record.name);
    out << INCREMENTAL_CACHE_SEPARATOR;
    write_hex_field(out, record.path.string());
    out << '\n';
}

void write_definition_record(std::ostream& out, const DefinitionRecord& record) {
    out << INCREMENTAL_CACHE_FIELD_DEF
        << INCREMENTAL_CACHE_SEPARATOR << record.category
        << INCREMENTAL_CACHE_SEPARATOR << stable_symbol_kind_name(record.stable_id.kind)
        << INCREMENTAL_CACHE_SEPARATOR << record.stable_id.global_id
        << INCREMENTAL_CACHE_SEPARATOR << record.stable_id.name.primary
        << INCREMENTAL_CACHE_SEPARATOR << record.stable_id.name.secondary
        << INCREMENTAL_CACHE_SEPARATOR << record.stable_id.name.byte_count
        << INCREMENTAL_CACHE_SEPARATOR << record.incremental_key.global_id
        << INCREMENTAL_CACHE_SEPARATOR << record.incremental_key.fingerprint.primary
        << INCREMENTAL_CACHE_SEPARATOR << record.incremental_key.fingerprint.secondary
        << INCREMENTAL_CACHE_SEPARATOR << record.incremental_key.fingerprint.byte_count
        << INCREMENTAL_CACHE_SEPARATOR;
    write_hex_field(out, record.name);
    out << '\n';
}

[[nodiscard]] std::filesystem::path temporary_cache_path(const std::filesystem::path& cache_path) {
    std::filesystem::path temporary = cache_path;
    temporary += ".tmp.";
    temporary += std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    return temporary;
}

[[nodiscard]] base::Result<void> publish_cache_file(
    const std::filesystem::path& temporary_path,
    const std::filesystem::path& cache_path
) {
    std::error_code rename_error;
    std::filesystem::rename(temporary_path, cache_path, rename_error);
    if (rename_error) {
        std::error_code remove_error;
        std::filesystem::remove(temporary_path, remove_error);
        return base::Result<void>::fail({
            base::ErrorCode::io_error,
            std::string(INCREMENTAL_CACHE_RENAME_FAILED)
        });
    }
    return base::Result<void>::ok();
}

} // namespace

base::Result<bool> try_reuse_incremental_check_cache(
    const CompilerInvocation& invocation
) {
    if (invocation.incremental_cache_path.empty() ||
        invocation.emit_kind != EmitKind::check) {
        return base::Result<bool>::ok(false);
    }

    const std::optional<ParsedCache> cache = read_incremental_cache(invocation.incremental_cache_path);
    if (!cache) {
        return base::Result<bool>::ok(false);
    }
    if (!parsed_cache_header_matches(*cache, invocation) ||
        !cache_sources_match(*cache)) {
        return base::Result<bool>::ok(false);
    }
    return base::Result<bool>::ok(true);
}

base::Result<void> write_incremental_cache(
    const CompilerInvocation& invocation,
    const base::SourceManager& sources,
    const std::span<const ModuleRecord> modules,
    const sema::CheckedModule& checked
) {
    if (invocation.incremental_cache_path.empty()) {
        return base::Result<void>::ok();
    }

    const std::filesystem::path cache_path = invocation.incremental_cache_path;
    const std::filesystem::path parent = cache_path.parent_path();
    if (!parent.empty()) {
        std::error_code directory_error;
        std::filesystem::create_directories(parent, directory_error);
        if (directory_error) {
            return base::Result<void>::fail({
                base::ErrorCode::io_error,
                std::string(INCREMENTAL_CACHE_DIRECTORY_FAILED)
            });
        }
    }

    const std::vector<std::filesystem::path> imports = normalized_import_paths(invocation);
    const std::vector<SourceFingerprintRecord> source_records = collect_source_fingerprints(sources);
    const std::vector<ModuleRecord> module_records = sorted_modules(modules);
    const std::vector<DefinitionRecord> definition_records = collect_definitions(checked);

    const std::filesystem::path temporary_path = temporary_cache_path(cache_path);
    {
        std::ofstream out(temporary_path, std::ios::binary | std::ios::trunc);
        if (!out) {
            return base::Result<void>::fail({
                base::ErrorCode::io_error,
                std::string(INCREMENTAL_CACHE_WRITE_OPEN_FAILED)
            });
        }

        out << INCREMENTAL_CACHE_MAGIC << '\n';
        write_header_field(out, INCREMENTAL_CACHE_FIELD_SCHEMA, std::to_string(INCREMENTAL_CACHE_SCHEMA_VERSION));
        write_encoded_header_field(out, INCREMENTAL_CACHE_FIELD_COMPILER, base::config::AUREX_VERSION_STRING);
        write_encoded_header_field(out, INCREMENTAL_CACHE_FIELD_MODE, INCREMENTAL_CACHE_MODE_SEMANTIC_OK);
        write_encoded_header_field(out, INCREMENTAL_CACHE_FIELD_ROOT, canonical_or_absolute(invocation.input_path).string());
        write_header_field(out, INCREMENTAL_CACHE_FIELD_IMPORT_PATHS, std::to_string(imports.size()));
        for (const std::filesystem::path& import_path : imports) {
            out << INCREMENTAL_CACHE_FIELD_IMPORT_PATH << INCREMENTAL_CACHE_SEPARATOR;
            write_hex_field(out, import_path.string());
            out << '\n';
        }
        write_header_field(out, INCREMENTAL_CACHE_FIELD_SOURCES, std::to_string(source_records.size()));
        for (const SourceFingerprintRecord& record : source_records) {
            write_source_record(out, record);
        }
        write_header_field(out, INCREMENTAL_CACHE_FIELD_MODULES, std::to_string(module_records.size()));
        for (const ModuleRecord& record : module_records) {
            write_module_record(out, record);
        }
        write_header_field(out, INCREMENTAL_CACHE_FIELD_DEFINITIONS, std::to_string(definition_records.size()));
        for (const DefinitionRecord& record : definition_records) {
            write_definition_record(out, record);
        }

        if (!out) {
            return base::Result<void>::fail({
                base::ErrorCode::io_error,
                std::string(INCREMENTAL_CACHE_WRITE_FAILED)
            });
        }
    }

    return publish_cache_file(temporary_path, cache_path);
}

} // namespace aurex::driver
