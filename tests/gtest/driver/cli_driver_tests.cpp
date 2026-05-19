#include <aurex/base/config.hpp>
#include <aurex/driver/cli.hpp>
#include <aurex/driver/cli_llvm.hpp>
#include <aurex/driver/compiler.hpp>
#include <aurex/driver/file_cache.hpp>
#include <aurex/driver/incremental_cache.hpp>
#include <aurex/driver/profile.hpp>
#include <aurex/query/generic_instance_key.hpp>
#include <aurex/query/query_context.hpp>
#include <aurex/query/query_result.hpp>

#include <support/test_support.hpp>

#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <thread>
#include <vector>

namespace aurex::test {
namespace {

constexpr std::string_view DRIVER_FILE_CACHE_FIRST_TEXT = "cached-one";
constexpr std::string_view DRIVER_FILE_CACHE_SECOND_TEXT = "cached-two";
constexpr std::string_view DRIVER_FILE_CACHE_FIFO_TEXT = "fifo-stream";
constexpr std::string_view DRIVER_INCREMENTAL_CACHE_FIRST_SOURCE =
    "module incremental_cache_driver;\n"
    "pub type Count = i32;\n"
    "pub struct Box { pub value: Count; }\n"
    "pub enum Mode: u8 { fast = 1, slow = 2, }\n"
    "impl Box { pub fn read(self: &Box) -> Count { return self.value; } }\n"
    "fn id[T](value: T) -> T { return value; }\n"
    "fn main() -> i32 { let box: Box = Box { value: id[Count](0) }; return box.read(); }\n";
constexpr std::string_view DRIVER_INCREMENTAL_CACHE_SECOND_SOURCE =
    "module incremental_cache_driver;\n"
    "pub type Count = i32;\n"
    "pub struct Box { pub value: Count; }\n"
    "pub enum Mode: u8 { fast = 1, slow = 2, }\n"
    "impl Box { pub fn read(self: &Box) -> Count { return self.value; } }\n"
    "fn id[T](value: T) -> T { return value; }\n"
    "fn main() -> i32 { let box: Box = Box { value: id[Count](1) }; return box.read(); }\n";
constexpr std::string_view DRIVER_INCREMENTAL_CACHE_INVALID_SOURCE =
    "module incremental_cache_driver;\nfn main() -> i32 { return ; }\n";
constexpr std::string_view DRIVER_INCREMENTAL_CACHE_IMPORT_ROOT_SOURCE =
    "module incremental_cache_import_root;\n"
    "import shared.util as util;\n"
    "fn main() -> i32 { return util.twice(21) - 42; }\n";
constexpr std::string_view DRIVER_INCREMENTAL_CACHE_IMPORT_FIRST_SOURCE =
    "module shared.util;\n"
    "pub fn twice(value: i32) -> i32 { return value + value; }\n";
constexpr std::string_view DRIVER_INCREMENTAL_CACHE_IMPORT_SECOND_SOURCE =
    "module shared.util;\n"
    "pub fn twice(value: i32) -> i32 { return value + 21; }\n";
constexpr std::string_view DRIVER_INCREMENTAL_CACHE_SIGNATURE_FIRST_SOURCE =
    "module incremental_cache_signature;\n"
    "fn helper(value: i32) -> i32 { return value + 1; }\n"
    "fn main() -> i32 { return helper(41); }\n";
constexpr std::string_view DRIVER_INCREMENTAL_CACHE_SIGNATURE_SECOND_SOURCE =
    "module incremental_cache_signature;\n"
    "fn helper(value: i32) -> i32 { return value + 2; }\n"
    "fn main() -> i32 { return helper(40); }\n";
constexpr std::string_view DRIVER_INCREMENTAL_CACHE_SIGNATURE_CHANGE_FIRST_SOURCE =
    "module incremental_cache_signature_change;\n"
    "fn helper(value: i32) -> i32 { return value; }\n"
    "fn main() -> i32 { return helper(41); }\n";
constexpr std::string_view DRIVER_INCREMENTAL_CACHE_SIGNATURE_CHANGE_SECOND_SOURCE =
    "module incremental_cache_signature_change;\n"
    "fn helper(value: i32, extra: i32) -> i32 { return value + extra; }\n"
    "fn main() -> i32 { return helper(40, 1); }\n";
constexpr base::usize CACHE_TEST_QUERY_ROW_KIND_FIELD = 0;
constexpr base::usize CACHE_TEST_QUERY_KIND_FIELD = 1;
constexpr base::usize CACHE_TEST_QUERY_RESULT_GLOBAL_FIELD = 7;
constexpr base::usize CACHE_TEST_QUERY_RESULT_PRIMARY_FIELD = 8;
constexpr base::usize CACHE_TEST_QUERY_RESULT_SECONDARY_FIELD = 9;
constexpr base::usize CACHE_TEST_QUERY_RESULT_BYTES_FIELD = 10;
constexpr base::usize CACHE_TEST_QUERY_STABLE_KEY_FIELD = 11;
constexpr base::usize CACHE_TEST_QUERY_FIELD_COUNT = 12;
constexpr std::string_view CACHE_TEST_QUERY_ROW_KIND = "query";
constexpr std::string_view CACHE_TEST_QUERY_EDGE_ROW_KIND = "query_edge";
constexpr std::string_view CACHE_TEST_QUERY_FILE_CONTENT = "file_content";
constexpr std::string_view CACHE_TEST_QUERY_LEX_FILE = "lex_file";
constexpr std::string_view CACHE_TEST_QUERY_PARSE_FILE = "parse_file";
constexpr std::string_view CACHE_TEST_QUERY_MODULE_EXPORTS = "module_exports";
constexpr std::string_view CACHE_TEST_QUERY_ITEM_SIGNATURE = "item_signature";
constexpr std::string_view CACHE_TEST_QUERY_FUNCTION_BODY_SYNTAX = "function_body_syntax";
constexpr std::string_view CACHE_TEST_QUERY_TYPE_CHECK_BODY = "type_check_body";
constexpr std::string_view CACHE_TEST_QUERY_GENERIC_INSTANCE_SIGNATURE = "generic_instance_signature";
constexpr std::string_view CACHE_TEST_QUERY_GENERIC_INSTANCE_BODY = "generic_instance_body";
constexpr std::string_view CACHE_TEST_QUERY_LOWER_FUNCTION_IR = "lower_function_ir";
constexpr std::string_view CACHE_TEST_QUERY_DIAGNOSTICS = "diagnostics";
constexpr std::string_view CACHE_TEST_QUERY_DIFF_PROFILE_PHASE = "incremental_cache.query_diff";
constexpr std::string_view CACHE_TEST_QUERY_PLAN_PROFILE_PHASE = "incremental_cache.query_plan";
constexpr std::string_view CACHE_TEST_QUERY_PRUNING_PROFILE_PHASE = "incremental_cache.query_pruning";
constexpr std::string_view CACHE_TEST_QUERY_PROVIDER_EVAL_PROFILE_PHASE = "incremental_cache.query_provider_eval";
constexpr std::string_view CACHE_TEST_QUERY_DIFF_MISSING_DETAIL =
    "total=14,missing=14,unchanged=0,changed=0,malformed=0";
constexpr std::string_view CACHE_TEST_QUERY_DIFF_UNCHANGED_DETAIL =
    "total=14,missing=0,unchanged=14,changed=0,malformed=0";
constexpr std::string_view CACHE_TEST_QUERY_DIFF_CHANGED_DETAIL =
    "total=14,missing=0,unchanged=13,changed=1,malformed=0";
constexpr std::string_view CACHE_TEST_QUERY_PLAN_MISSING_DETAIL =
    "reusable=0,recompute_roots=14,propagated_recompute=0,recompute=14";
constexpr std::string_view CACHE_TEST_QUERY_PLAN_UNCHANGED_DETAIL =
    "reusable=14,recompute_roots=0,propagated_recompute=0,recompute=0";
constexpr std::string_view CACHE_TEST_QUERY_PLAN_CHANGED_DETAIL =
    "reusable=13,recompute_roots=1,propagated_recompute=0,recompute=1";
constexpr std::string_view CACHE_TEST_QUERY_PRUNING_NO_CACHE_DETAIL =
    "enabled=1,applied=0,reused=0,recomputed=14,reused_file_contents=0,reused_lex_files=0,"
    "reused_parse_files=0,reused_module_exports=0,reused_item_signatures=0,"
    "reused_function_body_syntaxes=0,reused_type_check_bodies=0,reused_generic_instance_signatures=0,"
    "reused_generic_instance_bodies=0,reused_lower_function_irs=0,reused_diagnostics=0,"
    "recomputed_file_contents=1,recomputed_lex_files=1,recomputed_parse_files=1,recomputed_module_exports=1,"
    "recomputed_item_signatures=2,recomputed_function_body_syntaxes=0,recomputed_type_check_bodies=0,"
    "recomputed_generic_instance_signatures=1,recomputed_generic_instance_bodies=0,"
    "recomputed_lower_function_irs=0,recomputed_diagnostics=7,fallback=no_cache";
constexpr std::string_view CACHE_TEST_QUERY_PRUNING_REUSE_ALL_DETAIL =
    "enabled=1,applied=1,reused=14,recomputed=0,reused_file_contents=1,reused_lex_files=1,"
    "reused_parse_files=1,reused_module_exports=1,reused_item_signatures=2,"
    "reused_function_body_syntaxes=0,reused_type_check_bodies=0,reused_generic_instance_signatures=1,"
    "reused_generic_instance_bodies=0,reused_lower_function_irs=0,reused_diagnostics=7,"
    "recomputed_file_contents=0,recomputed_lex_files=0,recomputed_parse_files=0,recomputed_module_exports=0,"
    "recomputed_item_signatures=0,recomputed_function_body_syntaxes=0,recomputed_type_check_bodies=0,"
    "recomputed_generic_instance_signatures=0,recomputed_generic_instance_bodies=0,"
    "recomputed_lower_function_irs=0,recomputed_diagnostics=0,fallback=none";
constexpr std::string_view CACHE_TEST_QUERY_PRUNING_CHANGED_DETAIL =
    "enabled=1,applied=1,reused=13,recomputed=1,reused_file_contents=1,reused_lex_files=1,"
    "reused_parse_files=1,reused_module_exports=1,reused_item_signatures=2,"
    "reused_function_body_syntaxes=0,reused_type_check_bodies=0,reused_generic_instance_signatures=0,"
    "reused_generic_instance_bodies=0,reused_lower_function_irs=0,reused_diagnostics=7,"
    "recomputed_file_contents=0,recomputed_lex_files=0,recomputed_parse_files=0,recomputed_module_exports=0,"
    "recomputed_item_signatures=0,recomputed_function_body_syntaxes=0,recomputed_type_check_bodies=0,"
    "recomputed_generic_instance_signatures=1,recomputed_generic_instance_bodies=0,"
    "recomputed_lower_function_irs=0,recomputed_diagnostics=0,fallback=none";
constexpr std::string_view CACHE_TEST_QUERY_PROVIDER_EVAL_REUSE_ALL_DETAIL =
    "mode=pruned,seeded=14,evaluated=0,seeded_file_contents=1,seeded_lex_files=1,seeded_parse_files=1,"
    "seeded_module_exports=1,seeded_item_signatures=2,"
    "seeded_function_body_syntaxes=0,seeded_type_check_bodies=0,seeded_generic_instance_signatures=1,"
    "seeded_generic_instance_bodies=0,seeded_lower_function_irs=0,seeded_diagnostics=7,"
    "evaluated_file_contents=0,evaluated_lex_files=0,evaluated_parse_files=0,evaluated_module_exports=0,"
    "evaluated_item_signatures=0,evaluated_function_body_syntaxes=0,evaluated_type_check_bodies=0,"
    "evaluated_generic_instance_signatures=0,evaluated_generic_instance_bodies=0,"
    "evaluated_lower_function_irs=0,evaluated_diagnostics=0";
constexpr std::string_view CACHE_TEST_QUERY_PROVIDER_EVAL_CHANGED_DETAIL =
    "mode=pruned,seeded=13,evaluated=1,seeded_file_contents=1,seeded_lex_files=1,seeded_parse_files=1,"
    "seeded_module_exports=1,seeded_item_signatures=2,"
    "seeded_function_body_syntaxes=0,seeded_type_check_bodies=0,seeded_generic_instance_signatures=0,"
    "seeded_generic_instance_bodies=0,seeded_lower_function_irs=0,seeded_diagnostics=7,"
    "evaluated_file_contents=0,evaluated_lex_files=0,evaluated_parse_files=0,evaluated_module_exports=0,"
    "evaluated_item_signatures=0,evaluated_function_body_syntaxes=0,evaluated_type_check_bodies=0,"
    "evaluated_generic_instance_signatures=1,evaluated_generic_instance_bodies=0,"
    "evaluated_lower_function_irs=0,evaluated_diagnostics=0";

struct CacheTestQueryResultFingerprint {
    std::string global_id;
    std::string primary;
    std::string secondary;
    std::string byte_count;

    [[nodiscard]] friend bool operator==(
        const CacheTestQueryResultFingerprint& lhs, const CacheTestQueryResultFingerprint& rhs) = default;
};

[[nodiscard]] driver::CliParseResult require_parse_cli(const std::vector<std::string_view>& args)
{
    auto result = driver::parse_cli_arguments(args);
    EXPECT_TRUE(result) << result.error().message;
    return result.value();
}

[[nodiscard]] std::string hex_encode_cache_test_field(const std::string_view value)
{
    constexpr char DIGITS[] = "0123456789abcdef";
    constexpr unsigned int HIGH_NIBBLE_SHIFT = 4;
    constexpr unsigned int LOW_NIBBLE_MASK = 0x0fU;
    constexpr std::size_t CACHE_TEST_HEX_CHARS_PER_BYTE = 2U;
    std::string encoded;
    encoded.reserve(value.size() * CACHE_TEST_HEX_CHARS_PER_BYTE);
    for (const unsigned char byte : value) {
        encoded.push_back(DIGITS[byte >> HIGH_NIBBLE_SHIFT]);
        encoded.push_back(DIGITS[byte & LOW_NIBBLE_MASK]);
    }
    return encoded;
}

[[nodiscard]] std::string uppercase_hex_cache_test_field(std::string encoded)
{
    constexpr char LOWER_HEX_FIRST = 'a';
    constexpr char LOWER_HEX_LAST = 'f';
    constexpr char HEX_CASE_OFFSET = 'A' - 'a';
    for (char& ch : encoded) {
        if (ch >= LOWER_HEX_FIRST && ch <= LOWER_HEX_LAST) {
            ch = static_cast<char>(ch + HEX_CASE_OFFSET);
        }
    }
    return encoded;
}

[[nodiscard]] std::string cache_test_header(const fs::path& root, const std::vector<fs::path>& import_paths = {})
{
    std::string cache;
    cache += "aurex-incremental-cache-v1\n";
    cache += "schema\t1\n";
    cache += "compiler\t" + hex_encode_cache_test_field(base::config::AUREX_VERSION_STRING) + "\n";
    cache += "mode\t" + hex_encode_cache_test_field("semantic-ok") + "\n";
    cache += "root\t" + hex_encode_cache_test_field(root.string()) + "\n";
    cache += "import_paths\t" + std::to_string(import_paths.size()) + "\n";
    for (const fs::path& import_path : import_paths) {
        cache += "import_path\t" + hex_encode_cache_test_field(import_path.string()) + "\n";
    }
    return cache;
}

[[nodiscard]] std::string cache_test_header_with_compiler_and_mode(
    const fs::path& root, const std::string_view compiler_version, const std::string_view mode)
{
    std::string cache;
    cache += "aurex-incremental-cache-v1\n";
    cache += "schema\t1\n";
    cache += "compiler\t" + hex_encode_cache_test_field(compiler_version) + "\n";
    cache += "mode\t" + hex_encode_cache_test_field(mode) + "\n";
    cache += "root\t" + hex_encode_cache_test_field(root.string()) + "\n";
    cache += "import_paths\t0\n";
    return cache;
}

[[nodiscard]] std::string minimal_cache_without_sources(
    const fs::path& root, const std::vector<fs::path>& import_paths = {})
{
    std::string cache = cache_test_header(root, import_paths);
    cache += "sources\t0\n";
    cache += "modules\t0\n";
    cache += "definitions\t0\n";
    return cache;
}

[[nodiscard]] std::string cache_test_source_row(const fs::path& path, const std::string_view size,
    const std::string_view primary, const std::string_view secondary, const std::string_view byte_count)
{
    std::string row;
    row += "source\t";
    row += size;
    row += "\t";
    row += primary;
    row += "\t";
    row += secondary;
    row += "\t";
    row += byte_count;
    row += "\t";
    row += hex_encode_cache_test_field(path.string());
    row += "\n";
    return row;
}

[[nodiscard]] std::string cache_test_source_row(const fs::path& path, const std::string_view text)
{
    const query::StableFingerprint128 fingerprint = query::stable_fingerprint(text);
    return cache_test_source_row(path, std::to_string(text.size()), std::to_string(fingerprint.primary),
        std::to_string(fingerprint.secondary), std::to_string(fingerprint.byte_count));
}

[[nodiscard]] std::string cache_test_def_row(const std::string_view category, const std::string_view stable_kind,
    const std::string_view stable_global, const std::string_view stable_primary,
    const std::string_view stable_secondary, const std::string_view stable_bytes,
    const std::string_view incremental_global, const std::string_view incremental_primary,
    const std::string_view incremental_secondary, const std::string_view incremental_bytes,
    const std::string_view encoded_name)
{
    std::string row;
    row += "def\t";
    row += category;
    row += "\t";
    row += stable_kind;
    row += "\t";
    row += stable_global;
    row += "\t";
    row += stable_primary;
    row += "\t";
    row += stable_secondary;
    row += "\t";
    row += stable_bytes;
    row += "\t";
    row += incremental_global;
    row += "\t";
    row += incremental_primary;
    row += "\t";
    row += incremental_secondary;
    row += "\t";
    row += incremental_bytes;
    row += "\t";
    row += encoded_name;
    row += "\n";
    return row;
}

[[nodiscard]] std::string cache_test_query_row(const std::string_view kind, const std::string_view schema,
    const std::string_view query_global, const std::string_view payload_primary,
    const std::string_view payload_secondary, const std::string_view payload_bytes,
    const std::string_view result_global, const std::string_view result_primary,
    const std::string_view result_secondary, const std::string_view result_bytes,
    const std::string_view encoded_stable_key)
{
    std::string row;
    row += "query\t";
    row += kind;
    row += "\t";
    row += schema;
    row += "\t";
    row += query_global;
    row += "\t";
    row += payload_primary;
    row += "\t";
    row += payload_secondary;
    row += "\t";
    row += payload_bytes;
    row += "\t";
    row += result_global;
    row += "\t";
    row += result_primary;
    row += "\t";
    row += result_secondary;
    row += "\t";
    row += result_bytes;
    row += "\t";
    row += encoded_stable_key;
    row += "\n";
    return row;
}

[[nodiscard]] std::string_view cache_test_query_kind_name(const query::QueryKind kind) noexcept
{
    if (kind == query::QueryKind::file_content) {
        return CACHE_TEST_QUERY_FILE_CONTENT;
    }
    if (kind == query::QueryKind::lex_file) {
        return CACHE_TEST_QUERY_LEX_FILE;
    }
    if (kind == query::QueryKind::parse_file) {
        return CACHE_TEST_QUERY_PARSE_FILE;
    }
    if (kind == query::QueryKind::module_exports) {
        return CACHE_TEST_QUERY_MODULE_EXPORTS;
    }
    if (kind == query::QueryKind::item_signature) {
        return CACHE_TEST_QUERY_ITEM_SIGNATURE;
    }
    if (kind == query::QueryKind::generic_instance_signature) {
        return CACHE_TEST_QUERY_GENERIC_INSTANCE_SIGNATURE;
    }
    if (kind == query::QueryKind::generic_instance_body) {
        return CACHE_TEST_QUERY_GENERIC_INSTANCE_BODY;
    }
    if (kind == query::QueryKind::lower_function_ir) {
        return CACHE_TEST_QUERY_LOWER_FUNCTION_IR;
    }
    if (kind == query::QueryKind::diagnostics) {
        return CACHE_TEST_QUERY_DIAGNOSTICS;
    }
    return {};
}

[[nodiscard]] std::string cache_test_query_row(const query::QueryRecord& record)
{
    return cache_test_query_row(cache_test_query_kind_name(record.key.kind), std::to_string(record.key.schema),
        std::to_string(record.key.global_id), std::to_string(record.key.payload.primary),
        std::to_string(record.key.payload.secondary), std::to_string(record.key.payload.byte_count),
        std::to_string(record.result.global_id), std::to_string(record.result.fingerprint.primary),
        std::to_string(record.result.fingerprint.secondary), std::to_string(record.result.fingerprint.byte_count),
        hex_encode_cache_test_field(record.stable_key_bytes));
}

[[nodiscard]] std::string cache_test_query_key_fields(const query::QueryKey key)
{
    std::string fields;
    fields += cache_test_query_kind_name(key.kind);
    fields += "\t";
    fields += std::to_string(key.schema);
    fields += "\t";
    fields += std::to_string(key.global_id);
    fields += "\t";
    fields += std::to_string(key.payload.primary);
    fields += "\t";
    fields += std::to_string(key.payload.secondary);
    fields += "\t";
    fields += std::to_string(key.payload.byte_count);
    return fields;
}

[[nodiscard]] std::string cache_test_query_edge_row(const query::QueryDependencyEdge& edge)
{
    std::string row;
    row += CACHE_TEST_QUERY_EDGE_ROW_KIND;
    row += "\t";
    row += cache_test_query_key_fields(edge.dependent);
    row += "\t";
    row += cache_test_query_key_fields(edge.dependency);
    row += "\n";
    return row;
}

[[nodiscard]] std::vector<std::string_view> split_cache_test_fields(const std::string_view line)
{
    std::vector<std::string_view> fields;
    fields.reserve(CACHE_TEST_QUERY_FIELD_COUNT);
    base::usize start = 0;
    while (start <= line.size()) {
        const base::usize end = line.find('\t', start);
        if (end == std::string_view::npos) {
            fields.push_back(line.substr(start));
            break;
        }
        fields.push_back(line.substr(start, end - start));
        start = end + 1;
    }
    return fields;
}

[[nodiscard]] std::optional<CacheTestQueryResultFingerprint> cache_test_query_result(
    const std::string_view cache_text, const std::string_view query_kind, const std::string_view encoded_stable_key)
{
    base::usize line_start = 0;
    while (line_start < cache_text.size()) {
        const base::usize line_end = cache_text.find('\n', line_start);
        const std::string_view line = line_end == std::string_view::npos
            ? cache_text.substr(line_start)
            : cache_text.substr(line_start, line_end - line_start);
        const std::vector<std::string_view> fields = split_cache_test_fields(line);
        if (fields.size() == CACHE_TEST_QUERY_FIELD_COUNT
            && fields[CACHE_TEST_QUERY_ROW_KIND_FIELD] == CACHE_TEST_QUERY_ROW_KIND
            && fields[CACHE_TEST_QUERY_KIND_FIELD] == query_kind
            && fields[CACHE_TEST_QUERY_STABLE_KEY_FIELD] == encoded_stable_key) {
            return CacheTestQueryResultFingerprint{
                std::string(fields[CACHE_TEST_QUERY_RESULT_GLOBAL_FIELD]),
                std::string(fields[CACHE_TEST_QUERY_RESULT_PRIMARY_FIELD]),
                std::string(fields[CACHE_TEST_QUERY_RESULT_SECONDARY_FIELD]),
                std::string(fields[CACHE_TEST_QUERY_RESULT_BYTES_FIELD]),
            };
        }
        if (line_end == std::string_view::npos) {
            break;
        }
        line_start = line_end + 1;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<CacheTestQueryResultFingerprint> cache_test_item_signature_result(
    const std::string_view cache_text, const std::string_view encoded_stable_key)
{
    return cache_test_query_result(cache_text, CACHE_TEST_QUERY_ITEM_SIGNATURE, encoded_stable_key);
}

[[nodiscard]] std::optional<CacheTestQueryResultFingerprint> cache_test_function_body_syntax_result(
    const std::string_view cache_text, const std::string_view encoded_stable_key)
{
    return cache_test_query_result(cache_text, CACHE_TEST_QUERY_FUNCTION_BODY_SYNTAX, encoded_stable_key);
}

[[nodiscard]] std::optional<CacheTestQueryResultFingerprint> cache_test_type_check_body_result(
    const std::string_view cache_text, const std::string_view encoded_stable_key)
{
    return cache_test_query_result(cache_text, CACHE_TEST_QUERY_TYPE_CHECK_BODY, encoded_stable_key);
}

void expect_query_profile_phases(
    const driver::CompilationProfiler& profiler, const std::string_view diff_detail, const std::string_view plan_detail)
{
    ASSERT_EQ(profiler.phases().size(), 2U);
    EXPECT_EQ(profiler.phases()[0].name, CACHE_TEST_QUERY_DIFF_PROFILE_PHASE);
    EXPECT_EQ(profiler.phases()[0].detail, diff_detail);
    EXPECT_EQ(profiler.phases()[1].name, CACHE_TEST_QUERY_PLAN_PROFILE_PHASE);
    EXPECT_EQ(profiler.phases()[1].detail, plan_detail);
}

void expect_query_profile_phases_with_pruning(const driver::CompilationProfiler& profiler,
    const std::string_view diff_detail, const std::string_view plan_detail, const std::string_view pruning_detail)
{
    ASSERT_EQ(profiler.phases().size(), 3U);
    EXPECT_EQ(profiler.phases()[0].name, CACHE_TEST_QUERY_DIFF_PROFILE_PHASE);
    EXPECT_EQ(profiler.phases()[0].detail, diff_detail);
    EXPECT_EQ(profiler.phases()[1].name, CACHE_TEST_QUERY_PLAN_PROFILE_PHASE);
    EXPECT_EQ(profiler.phases()[1].detail, plan_detail);
    EXPECT_EQ(profiler.phases()[2].name, CACHE_TEST_QUERY_PRUNING_PROFILE_PHASE);
    EXPECT_EQ(profiler.phases()[2].detail, pruning_detail);
}

void expect_query_profile_phases_with_pruned_provider_eval(const driver::CompilationProfiler& profiler,
    const std::string_view diff_detail, const std::string_view plan_detail, const std::string_view pruning_detail,
    const std::string_view provider_eval_detail)
{
    ASSERT_EQ(profiler.phases().size(), 4U);
    EXPECT_EQ(profiler.phases()[0].name, CACHE_TEST_QUERY_DIFF_PROFILE_PHASE);
    EXPECT_EQ(profiler.phases()[0].detail, diff_detail);
    EXPECT_EQ(profiler.phases()[1].name, CACHE_TEST_QUERY_PLAN_PROFILE_PHASE);
    EXPECT_EQ(profiler.phases()[1].detail, plan_detail);
    EXPECT_EQ(profiler.phases()[2].name, CACHE_TEST_QUERY_PRUNING_PROFILE_PHASE);
    EXPECT_EQ(profiler.phases()[2].detail, pruning_detail);
    EXPECT_EQ(profiler.phases()[3].name, CACHE_TEST_QUERY_PROVIDER_EVAL_PROFILE_PHASE);
    EXPECT_EQ(profiler.phases()[3].detail, provider_eval_detail);
}

} // namespace

TEST(CoreUnit, CliParserIsTableDrivenAndSupportsModernDriverForms)
{
    const std::vector<std::string_view> object_args{
        "aurexc",
        "-c",
        "-Itests/samples/imports",
        "--clang=/usr/bin/clang",
        "--clang-arg=-fno-color-diagnostics",
        "--opt-level=O2",
        "--incremental-cache",
        "build/hello.axic",
        "--experimental-query-pruning",
        "--profile-output",
        "build/hello.profile.json",
        "examples/hello.ax",
    };
    const driver::CliParseResult object_parse = require_parse_cli(object_args);
    EXPECT_EQ(object_parse.action, driver::CliAction::compile);
    EXPECT_EQ(object_parse.invocation.emit_kind, driver::EmitKind::object);
    EXPECT_EQ(object_parse.invocation.output_path, fs::path("hello.o"));
    ASSERT_EQ(object_parse.invocation.import_paths.size(), 1U);
    EXPECT_EQ(object_parse.invocation.import_paths.front(), fs::path("tests/samples/imports"));
    EXPECT_EQ(object_parse.invocation.clang_path, "/usr/bin/clang");
    ASSERT_EQ(object_parse.invocation.clang_args.size(), 1U);
    EXPECT_EQ(object_parse.invocation.clang_args.front(), "-fno-color-diagnostics");
    EXPECT_EQ(object_parse.invocation.optimization_level, ir::OptimizationLevel::standard);
    EXPECT_EQ(object_parse.invocation.incremental_cache_path, fs::path("build/hello.axic"));
    EXPECT_TRUE(object_parse.invocation.experimental_query_pruning);
    EXPECT_EQ(object_parse.invocation.profile_output_path, fs::path("build/hello.profile.json"));

    const std::vector<std::string_view> assembly_args{
        "aurexc",
        "-S",
        "examples/hello.ax",
    };
    const driver::CliParseResult assembly_parse = require_parse_cli(assembly_args);
    EXPECT_EQ(assembly_parse.invocation.emit_kind, driver::EmitKind::assembly);
    EXPECT_EQ(assembly_parse.invocation.output_path, fs::path("hello.s"));
    EXPECT_FALSE(assembly_parse.invocation.experimental_query_pruning);

    const std::vector<std::string_view> separate_emit_args{
        "aurexc",
        "--emit",
        "llvm-ir",
        "-fsyntax-only",
        "examples/hello.ax",
    };
    const driver::CliParseResult separate_emit_parse = require_parse_cli(separate_emit_args);
    EXPECT_EQ(separate_emit_parse.invocation.emit_kind, driver::EmitKind::check);

    const std::vector<std::string_view> typed_emit_args{
        "aurexc",
        "--emit=typed",
        "examples/hello.ax",
    };
    const driver::CliParseResult typed_emit_parse = require_parse_cli(typed_emit_args);
    EXPECT_EQ(typed_emit_parse.invocation.emit_kind, driver::EmitKind::typed);

    const std::vector<std::string_view> json_diagnostic_args{
        "aurexc",
        "--check",
        "--diagnostics=json",
        "examples/hello.ax",
    };
    const driver::CliParseResult json_diagnostic_parse = require_parse_cli(json_diagnostic_args);
    EXPECT_EQ(json_diagnostic_parse.invocation.emit_kind, driver::EmitKind::check);
    EXPECT_EQ(json_diagnostic_parse.invocation.diagnostic_format, driver::DiagnosticOutputFormat::json);

    const std::vector<std::string_view> text_diagnostic_args{
        "aurexc",
        "--check",
        "--diagnostics",
        "text",
        "examples/hello.ax",
    };
    const driver::CliParseResult text_diagnostic_parse = require_parse_cli(text_diagnostic_args);
    EXPECT_EQ(text_diagnostic_parse.invocation.diagnostic_format, driver::DiagnosticOutputFormat::text);

    const std::vector<std::string_view> inference_reset_args{
        "aurexc",
        "-S",
        "--emit=ir",
        "examples/hello.ax",
    };
    const driver::CliParseResult inference_reset_parse = require_parse_cli(inference_reset_args);
    EXPECT_EQ(inference_reset_parse.invocation.emit_kind, driver::EmitKind::ir);
    EXPECT_TRUE(inference_reset_parse.invocation.output_path.empty());

    const std::vector<std::string_view> dump_llvm_ir_args{
        "aurexc",
        "--dump-llvm-ir",
        "examples/hello.ax",
    };
    const driver::CliParseResult dump_llvm_ir_parse = require_parse_cli(dump_llvm_ir_args);
    EXPECT_EQ(dump_llvm_ir_parse.invocation.emit_kind, driver::EmitKind::llvm_ir);

    const std::vector<std::string_view> option_end_args{
        "aurexc",
        "--",
        "-strange.ax",
    };
    const driver::CliParseResult option_end_parse = require_parse_cli(option_end_args);
    EXPECT_EQ(option_end_parse.invocation.input_path, fs::path("-strange.ax"));
}

TEST_F(AurexIntegrationTest, CompilerWritesPhaseProfileOutput)
{
    {
        const fs::path profile = tmp_root() / "hello.profile.json";
        driver::CompilerInvocation invocation;
        invocation.input_path = source_root() / "examples" / "hello.ax";
        invocation.emit_kind = driver::EmitKind::check;
        invocation.incremental_cache_path = tmp_root() / "hello.profile.axic";
        invocation.profile_output_path = profile;

        driver::Compiler compiler;
        const auto result = compiler.run(invocation);
        ASSERT_TRUE(result) << result.error().message;

        const std::string profile_text = read_text(profile);
        expect_contains_all(profile_text,
            {
                "\"format\": \"aurex-profile-v1\"",
                "\"phases\"",
                "\"name\": \"module.read\"",
                "\"name\": \"module.lex\"",
                "\"name\": \"module.parse\"",
                "\"name\": \"sema.analyze\"",
                "\"name\": \"incremental_cache.query_diff\"",
                "\"name\": \"incremental_cache.query_plan\"",
                "total=",
                "missing=",
                "unchanged=",
                "changed=",
                "malformed=",
                "reusable=",
                "recompute_roots=",
                "propagated_recompute=",
                "recompute=",
                "\"rss_mib_after\"",
                "\"rss_delta_mib\"",
            });
    }

    {
        const fs::path invalid = tmp_root() / "invalid_profile.ax";
        const fs::path profile = tmp_root() / "invalid.profile.json";
        std::ofstream out(invalid);
        out << "module invalid_profile;\nfn main() -> i32 { let value: i32 = true; return value; }\n";
        out.close();

        driver::CompilerInvocation invocation;
        invocation.input_path = invalid;
        invocation.emit_kind = driver::EmitKind::check;
        invocation.profile_output_path = profile;

        driver::Compiler compiler;
        testing::internal::CaptureStderr();
        const auto result = compiler.run(invocation);
        static_cast<void>(testing::internal::GetCapturedStderr());
        ASSERT_FALSE(result);

        const std::string profile_text = read_text(profile);
        expect_contains_all(profile_text,
            {
                "\"format\": \"aurex-profile-v1\"",
                "\"name\": \"module.parse\"",
                "\"name\": \"sema.analyze\"",
            });
    }
}

TEST_F(AurexIntegrationTest, CompilerProfileCoversJsonAndErrorPaths)
{
    {
        driver::CompilationProfiler disabled;
        EXPECT_FALSE(disabled.enabled());
        disabled.record("ignored", std::chrono::milliseconds(1));
        EXPECT_TRUE(disabled.phases().empty());
        const fs::path disabled_path = tmp_root() / "disabled.profile.json";
        const auto result = disabled.write_json(disabled_path);
        ASSERT_TRUE(result) << result.error().message;
        EXPECT_FALSE(fs::exists(disabled_path));
    }

    {
        driver::CompilationProfiler profiler(true);
        ASSERT_TRUE(profiler.enabled());
        profiler.record("phase\n\"\\", std::string_view{"detail\t\r\x01", 9}, std::chrono::milliseconds(2));
        ASSERT_EQ(profiler.phases().size(), 1U);
        EXPECT_EQ(profiler.phases().front().name, "phase\n\"\\");

        const fs::path profile = tmp_root() / "nested" / "phase.profile.json";
        const auto result = profiler.write_json(profile);
        ASSERT_TRUE(result) << result.error().message;
        const std::string profile_text = read_text(profile);
        expect_contains_all(profile_text,
            {
                "\"format\": \"aurex-profile-v1\"",
                "\\n",
                "\\\"",
                "\\\\",
                "\\t",
                "\\r",
                "\\u0001",
                "\"elapsed_ms\":",
                "\"rss_mib_after\":",
                "\"rss_delta_mib\":",
            });

        const fs::path relative_profile = "profile-no-parent-test.json";
        std::error_code remove_error;
        fs::remove(relative_profile, remove_error);
        const auto relative_result = profiler.write_json(relative_profile);
        ASSERT_TRUE(relative_result) << relative_result.error().message;
        EXPECT_TRUE(fs::exists(relative_profile));
        fs::remove(relative_profile, remove_error);
    }

    {
        driver::CompilationProfiler profiler(true);
        profiler.record("phase", std::chrono::milliseconds(1));
        const fs::path file_parent = tmp_root() / "profile-parent-file";
        {
            std::ofstream out(file_parent);
            out << "not a directory";
        }
        const auto result = profiler.write_json(file_parent / "profile.json");
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code, base::ErrorCode::io_error);
        expect_contains(result.error().message, "profile output directory");
    }

    {
        driver::CompilationProfiler profiler(true);
        profiler.record("phase", std::chrono::milliseconds(1));
        const fs::path directory_target = tmp_root() / "profile-directory-target";
        fs::create_directories(directory_target);
        const auto result = profiler.write_json(directory_target);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code, base::ErrorCode::io_error);
        expect_contains(result.error().message, "profile output file");
    }

    {
        driver::ScopedCompilationPhase phase(nullptr, "ignored");
    }
}

TEST_F(AurexIntegrationTest, CompilerProfileCoversCacheHitWriteFailureAndJsonSuppression)
{
    driver::clear_file_cache();

    {
        const fs::path cache_dir = tmp_root() / "profile-cache-hit";
        fs::create_directories(cache_dir);
        const fs::path source = cache_dir / "main.ax";
        const fs::path cache = cache_dir / "main.axic";
        {
            std::ofstream out(source, std::ios::binary | std::ios::trunc);
            out << DRIVER_INCREMENTAL_CACHE_FIRST_SOURCE;
        }

        driver::CompilerInvocation invocation;
        invocation.input_path = source;
        invocation.emit_kind = driver::EmitKind::check;
        invocation.incremental_cache_path = cache;

        driver::Compiler compiler;
        auto first = compiler.run(invocation);
        ASSERT_TRUE(first) << first.error().message;

        invocation.profile_output_path = cache_dir / "cache-hit.profile.json";
        auto second = compiler.run(invocation);
        ASSERT_TRUE(second) << second.error().message;
        const std::string profile_text = read_text(invocation.profile_output_path);
        expect_contains(profile_text, "\"name\": \"incremental_cache.lookup\"");
        EXPECT_EQ(profile_text.find("\"name\": \"module.read\""), std::string::npos);
    }

    {
        const fs::path file_parent = tmp_root() / "compiler-profile-parent-file";
        {
            std::ofstream out(file_parent);
            out << "not a directory";
        }

        driver::CompilerInvocation invocation;
        invocation.input_path = source_root() / "examples" / "hello.ax";
        invocation.emit_kind = driver::EmitKind::check;
        invocation.profile_output_path = file_parent / "profile.json";

        driver::Compiler compiler;
        auto result = compiler.run(invocation);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code, base::ErrorCode::io_error);
        expect_contains(result.error().message, "profile output directory");
    }

    {
        const fs::path source = tmp_root() / "json_suppressed_diagnostics.ax";
        std::ofstream out(source);
        out << "module json_suppressed_diagnostics;\nfn main() -> i32 {\n";
        for (int index = 0; index < 150; ++index) {
            out << "    let value_" << index << ": i32 = true;\n";
        }
        out << "    return 0;\n}\n";
        out.close();

        driver::CompilerInvocation invocation;
        invocation.input_path = source;
        invocation.emit_kind = driver::EmitKind::check;
        invocation.diagnostic_format = driver::DiagnosticOutputFormat::json;

        driver::Compiler compiler;
        testing::internal::CaptureStderr();
        const auto result = compiler.run(invocation);
        const std::string diagnostics = testing::internal::GetCapturedStderr();
        ASSERT_FALSE(result);
        expect_contains_all(diagnostics,
            {
                "\"format\": \"aurex-diagnostics-v1\"",
                "\"suppressed\": ",
            });
        EXPECT_EQ(diagnostics.find("\"suppressed\": 0"), std::string::npos);
    }

    driver::clear_file_cache();
}

TEST(CoreUnit, CliParserReportsTableDrivenArgumentErrors)
{
    const std::vector<std::string_view> invalid_emit_args{
        "aurexc",
        "--emit=not-a-kind",
        "examples/hello.ax",
    };
    const auto invalid_emit = driver::parse_cli_arguments(invalid_emit_args);
    ASSERT_FALSE(invalid_emit);
    expect_contains(invalid_emit.error().message, "invalid emit kind");

    const std::vector<std::string_view> unexpected_value_args{
        "aurexc",
        "--help=true",
    };
    const auto unexpected_value = driver::parse_cli_arguments(unexpected_value_args);
    ASSERT_FALSE(unexpected_value);
    expect_contains(unexpected_value.error().message, "does not take a value");

    const std::vector<std::string_view> unknown_equal_args{
        "aurexc",
        "--unknown=value",
    };
    const auto unknown_equal = driver::parse_cli_arguments(unknown_equal_args);
    ASSERT_FALSE(unknown_equal);
    expect_contains(unknown_equal.error().message, "unknown option: --unknown");

    const std::vector<std::string_view> inapplicable_native_backend_args{
        "aurexc",
        "--clang=/usr/bin/clang",
        "--emit=ir",
        "examples/hello.ax",
    };
    const auto inapplicable_native_backend = driver::parse_cli_arguments(inapplicable_native_backend_args);
    ASSERT_FALSE(inapplicable_native_backend);
    expect_contains(inapplicable_native_backend.error().message, "option requires native output: --clang");

    const std::vector<std::string_view> invalid_diagnostic_format_args{
        "aurexc",
        "--diagnostics=xml",
        "examples/hello.ax",
    };
    const auto invalid_diagnostic_format = driver::parse_cli_arguments(invalid_diagnostic_format_args);
    ASSERT_FALSE(invalid_diagnostic_format);
    expect_contains(invalid_diagnostic_format.error().message, "invalid diagnostic output format");

    std::ostringstream out;
    std::ostringstream err;
    const std::vector<std::string_view> missing_file_args{
        "aurexc",
        "--check",
        "missing.ax",
    };
    EXPECT_EQ(driver::run_cli(missing_file_args, out, err), 1);
    expect_contains(err.str(), "aurexc: failed to open input file");

    std::ostringstream empty_out;
    std::ostringstream empty_err;
    const std::vector<std::string_view> empty_args;
    EXPECT_EQ(driver::run_cli(empty_args, empty_out, empty_err), 2);
    expect_contains(empty_err.str(), "usage: aurexc");
}

TEST_F(AurexIntegrationTest, CliAndFrontendDumps)
{
    expect_contains(require_success(aurexc() + " --version").output, "0.1.2");

    const std::string help = require_success(aurexc() + " --help").output;
    expect_contains_all(help,
        {
            "primary options:",
            "secondary options:",
            "frontend and debug output:",
            "native backend:",
            "--check",
            "--emit=ast",
            "--emit=typed",
            "--emit=ir",
            "--emit=llvm-ir",
            "--emit=asm",
            "--emit=obj",
            "--emit=exe",
            "--dump-modules",
            "--incremental-cache",
            "--diagnostics",
            "--opt-level",
        });

    const fs::path hello = source_root() / "examples" / "hello.ax";
    require_success(aurexc() + " --check " + q(hello));
    require_success(aurexc() + " --emit=check " + q(hello));
    require_success(aurexc() + " --emit=typed " + q(hello));

    const std::string tokens = require_success(aurexc() + " --dump-tokens " + q(hello)).output;
    const std::string ast = require_success(aurexc() + " --emit=ast " + q(hello)).output;
    const std::string checked = require_success(aurexc() + " --emit=checked " + q(hello)).output;
    const std::string ir = require_success(aurexc() + " --emit=ir " + q(hello)).output;
    const std::string llvm_ir = require_success(aurexc() + " --emit=llvm-ir " + q(hello)).output;

    EXPECT_EQ(read_text(golden_root() / "hello.tokens"), tokens);
    expect_contains(tokens, "c_string_literal");
    expect_contains(ast, "extern_block");
    expect_contains(checked, "checked_module");
    expect_contains(ir, "aurex_ir v0");
    expect_contains(ir, "fn puts(s: *const u8) @puts linkage(extern_c) abi(c) -> i32");
    expect_contains(ir, "call puts");
    expect_contains(llvm_ir, "define i32 @main");

    const std::string eval_order = require_success(
        aurexc() + " --emit=ir --opt-level O1 " + q(positive_sample("evaluation", "eval_order_assign.ax")))
                                       .output;
    expect_contains(eval_order, "call m0_eval_order_assign_next(%");

    const std::string pointer_field =
        require_success(aurexc() + " --emit=ir " + q(positive_sample("pointers", "pointer_field_write.ax"))).output;
    expect_contains(pointer_field, "field_addr ");
    expect_contains(pointer_field, ".value");

    const fs::path json_diagnostics = tmp_root() / "json\"\\diagnostics.ax";
    {
        std::ofstream out(json_diagnostics, std::ios::binary);
        out << "module json_diagnostics;\nfn main() -> i32 { let value: i32 = true; return value; }\n";
    }
    const std::string json_output =
        require_failure(aurexc() + " --check --diagnostics=json " + q(json_diagnostics)).output;
    expect_contains_all(json_output,
        {
            "\"format\": \"aurex-diagnostics-v1\"",
            "\"severity\": \"error\"",
            "\"category\": \"type\"",
            "\"code\": \"SEM0100\"",
            "\"message\": \"initializer type does not match declared type\"",
            "\"message\": \"expected type: i32\"",
            "\"message\": \"actual type: bool\"",
            "json\\\"\\\\diagnostics.ax",
            "\"range\": {",
            "\"suppressed\": 0",
        });
    EXPECT_EQ(json_output.find("aurexc:"), std::string::npos);
}

TEST_F(AurexIntegrationTest, CompilerDriverErrorBranches)
{
    {
        const fs::path invalid = tmp_root() / "invalid_json_cli.ax";
        std::ofstream out(invalid);
        out << "module invalid_json_cli;\nfn main() -> i32 { let value: i32 = true; return value; }\n";
        out.close();

        std::ostringstream stdout_capture;
        std::ostringstream stderr_capture;
        const std::string invalid_path = invalid.string();
        const std::vector<std::string_view> args{
            "aurexc",
            "--check",
            "--diagnostics=json",
            invalid_path,
        };
        testing::internal::CaptureStderr();
        const int exit_code = driver::run_cli(args, stdout_capture, stderr_capture);
        const std::string diagnostics = testing::internal::GetCapturedStderr();
        EXPECT_EQ(exit_code, 1);
        expect_contains(diagnostics, "\"format\": \"aurex-diagnostics-v1\"");
        EXPECT_TRUE(stderr_capture.str().empty());
    }

    {
        const fs::path escaped_path = tmp_root() / std::string("json\n\r\t\x01\"\\diagnostics.ax");
        std::ofstream out(escaped_path, std::ios::binary);
        out << "module escaped_json_diagnostics;\n"
               "fn main() -> i32 { let value: i32 = true; return value; }\n";
        out.close();

        driver::CompilerInvocation invocation;
        invocation.input_path = escaped_path;
        invocation.emit_kind = driver::EmitKind::check;
        invocation.diagnostic_format = driver::DiagnosticOutputFormat::json;
        driver::Compiler compiler;
        testing::internal::CaptureStderr();
        const auto result = compiler.run(invocation);
        const std::string diagnostics = testing::internal::GetCapturedStderr();
        ASSERT_FALSE(result);
        expect_contains_all(diagnostics,
            {
                "\\n",
                "\\r",
                "\\t",
                "\\u0001",
                "\\\"",
                "\\\\",
            });
    }

    {
        std::ostringstream stdout_capture;
        std::ostringstream stderr_capture;
        const std::string missing_path = (tmp_root() / "definitely_missing.ax").string();
        const std::vector<std::string_view> args{
            "aurexc",
            "--check",
            "--diagnostics=json",
            missing_path,
        };
        testing::internal::CaptureStderr();
        const int exit_code = driver::run_cli(args, stdout_capture, stderr_capture);
        const std::string diagnostics = testing::internal::GetCapturedStderr();
        EXPECT_EQ(exit_code, 1);
        EXPECT_TRUE(diagnostics.empty());
        expect_contains_all(stderr_capture.str(),
            {
                "\"format\": \"aurex-diagnostics-v1\"",
                "\"severity\": \"fatal\"",
                "\"message\": \"failed to open input file\"",
                "\"range\": null",
            });
    }

    {
        const fs::path invalid = tmp_root() / "invalid_tokens.ax";
        std::ofstream out(invalid);
        out << "module invalid_tokens;\nfn main() -> i32 { return \"unterminated; }\n";
        out.close();

        driver::CompilerInvocation invocation;
        invocation.input_path = invalid;
        invocation.emit_kind = driver::EmitKind::tokens;
        driver::Compiler compiler;
        testing::internal::CaptureStderr();
        const auto result = compiler.run(invocation);
        static_cast<void>(testing::internal::GetCapturedStderr());
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code, base::ErrorCode::lex_error);
    }

    {
        driver::CompilerInvocation invocation;
        invocation.input_path = source_root() / "examples" / "hello.ax";
        invocation.emit_kind = static_cast<driver::EmitKind>(999);
        driver::Compiler compiler;
        const auto result = compiler.run(invocation);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code, base::ErrorCode::codegen_error);
        expect_contains(result.error().message, "unsupported emission mode");
    }

    {
        driver::CompilerInvocation invocation;
        invocation.input_path = source_root() / "examples" / "hello.ax";
        invocation.emit_kind = driver::EmitKind::object;
        invocation.output_path = tmp_root() / "bad_clang.o";
        invocation.clang_path = "/definitely/not/a/real/clang";
        driver::Compiler compiler(driver::llvm_backend_ir_emitter());
        const auto result = compiler.run(invocation);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code, base::ErrorCode::codegen_error);
        expect_contains(result.error().message, "exit code 127");
    }

    {
        driver::CompilerInvocation invocation;
        invocation.input_path = source_root() / "examples" / "hello.ax";
        invocation.emit_kind = driver::EmitKind::llvm_ir;
        driver::Compiler compiler;
        const auto result = compiler.run(invocation);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code, base::ErrorCode::codegen_error);
        expect_contains(result.error().message, "LLVM backend is unavailable");
    }
}

TEST_F(AurexIntegrationTest, CliFileCacheCoversMissHitClearAndEmptyFiles)
{
    driver::clear_file_cache();

    const fs::path missing = tmp_root() / "missing.ax";
    const auto missing_result = driver::read_text_file(missing);
    ASSERT_FALSE(missing_result);
    EXPECT_EQ(missing_result.error().code, base::ErrorCode::io_error);

    const fs::path empty = tmp_root() / "empty.ax";
    {
        std::ofstream out(empty, std::ios::binary);
    }
    const auto empty_result = driver::read_text_file(empty);
    ASSERT_TRUE(empty_result) << empty_result.error().message;
    EXPECT_TRUE(empty_result.value().empty());

    const fs::path cached = tmp_root() / "cached.ax";
    {
        std::ofstream out(cached, std::ios::binary);
        out << DRIVER_FILE_CACHE_FIRST_TEXT;
    }
    const auto first = driver::read_text_file(cached);
    ASSERT_TRUE(first) << first.error().message;
    EXPECT_EQ(first.value(), DRIVER_FILE_CACHE_FIRST_TEXT);

    const auto cached_hit = driver::read_text_file(cached);
    ASSERT_TRUE(cached_hit) << cached_hit.error().message;
    EXPECT_EQ(cached_hit.value(), DRIVER_FILE_CACHE_FIRST_TEXT);

    driver::clear_file_cache();
    {
        std::ofstream out(cached, std::ios::binary | std::ios::trunc);
        out << DRIVER_FILE_CACHE_SECOND_TEXT;
    }
    const auto after_clear = driver::read_text_file(cached);
    ASSERT_TRUE(after_clear) << after_clear.error().message;
    EXPECT_EQ(after_clear.value(), DRIVER_FILE_CACHE_SECOND_TEXT);

    const fs::path fifo = tmp_root() / "fifo.ax";
    std::error_code remove_error;
    fs::remove(fifo, remove_error);
    ASSERT_EQ(::mkfifo(fifo.c_str(), 0600), 0) << std::strerror(errno);

    std::thread writer([fifo] {
        std::ofstream out(fifo, std::ios::binary);
        out << DRIVER_FILE_CACHE_FIFO_TEXT;
    });

    const auto fifo_result = driver::read_text_file(fifo);
    writer.join();
    ASSERT_TRUE(fifo_result) << fifo_result.error().message;
    EXPECT_EQ(fifo_result.value(), DRIVER_FILE_CACHE_FIFO_TEXT);
    fs::remove(fifo, remove_error);

    driver::clear_file_cache();
}

TEST_F(AurexIntegrationTest, IncrementalCacheWritesValidatesInvalidatesAndReusesCheck)
{
    driver::clear_file_cache();

    const fs::path cache_dir = tmp_root() / "incremental-cache";
    fs::create_directories(cache_dir);
    const fs::path source = cache_dir / "main.ax";
    const fs::path cache = cache_dir / "main.axic";

    const auto write_source = [&](const std::string_view text) {
        std::ofstream out(source, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << text;
    };

    write_source(DRIVER_INCREMENTAL_CACHE_FIRST_SOURCE);

    driver::CompilerInvocation invocation;
    invocation.input_path = source;
    invocation.emit_kind = driver::EmitKind::check;
    invocation.incremental_cache_path = cache;

    driver::Compiler compiler;
    auto first = compiler.run(invocation);
    ASSERT_TRUE(first) << first.error().message;
    ASSERT_TRUE(fs::exists(cache));

    const std::string first_cache = read_text(cache);
    expect_contains(first_cache, "aurex-incremental-cache-v1");
    expect_contains(first_cache, "source\t");
    expect_contains(first_cache, "def\tfunction");
    expect_contains(first_cache, "def\tstruct");
    expect_contains(first_cache, "def\tenum_case");
    expect_contains(first_cache, "def\ttype_alias");
    expect_contains(first_cache, "queries\t");
    expect_contains(first_cache, "query\tfile_content");
    expect_contains(first_cache, "query\tlex_file");
    expect_contains(first_cache, "query\tparse_file");
    expect_contains(first_cache, "query\tmodule_exports");
    expect_contains(first_cache, "query\titem_signature");
    expect_contains(first_cache, "query\tdiagnostics");
    expect_contains(first_cache, "query_edges\t");
    expect_contains(first_cache, "query_edge\tlex_file");
    expect_contains(first_cache, "query_edge\tparse_file");

    auto first_reuse = driver::try_reuse_incremental_check_cache(invocation);
    ASSERT_TRUE(first_reuse) << first_reuse.error().message;
    EXPECT_TRUE(first_reuse.value());

    write_source(DRIVER_INCREMENTAL_CACHE_INVALID_SOURCE);
    driver::clear_file_cache();
    auto stale_reuse = driver::try_reuse_incremental_check_cache(invocation);
    ASSERT_TRUE(stale_reuse) << stale_reuse.error().message;
    EXPECT_FALSE(stale_reuse.value());

    testing::internal::CaptureStderr();
    auto invalid = compiler.run(invocation);
    static_cast<void>(testing::internal::GetCapturedStderr());
    EXPECT_FALSE(invalid);

    write_source(DRIVER_INCREMENTAL_CACHE_SECOND_SOURCE);
    driver::clear_file_cache();
    auto second = compiler.run(invocation);
    ASSERT_TRUE(second) << second.error().message;
    auto second_reuse = driver::try_reuse_incremental_check_cache(invocation);
    ASSERT_TRUE(second_reuse) << second_reuse.error().message;
    EXPECT_TRUE(second_reuse.value());
    EXPECT_NE(first_cache, read_text(cache));

    driver::clear_file_cache();
}

TEST_F(AurexIntegrationTest, IncrementalCacheWritesGenericInstanceQueryRowsWhenAvailable)
{
    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_SYNTHETIC_SOURCE = "module cache_query_rows;\n";
    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_SYNTHETIC_MODULE = "cache_query_rows";
    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_SYNTHETIC_FUNCTION = "id";
    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_SYNTHETIC_STABLE_ID = "generic-instance:manual";
    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_SYNTHETIC_SIGNATURE = "generic-signature:manual";
    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_SYNTHETIC_CHANGED_SIGNATURE =
        "generic-signature:manual:changed";

    driver::clear_file_cache();

    const fs::path cache_dir = tmp_root() / "incremental-cache-query-rows";
    fs::create_directories(cache_dir);
    const fs::path source = cache_dir / "main.ax";
    const fs::path cache = cache_dir / "main.axic";
    {
        std::ofstream out(source, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << DRIVER_INCREMENTAL_CACHE_SYNTHETIC_SOURCE;
    }
    const auto write_cache_text = [&](const std::string_view text) {
        std::ofstream out(cache, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << text;
    };

    driver::CompilerInvocation invocation;
    invocation.input_path = source;
    invocation.emit_kind = driver::EmitKind::check;
    invocation.incremental_cache_path = cache;

    base::SourceManager sources;
    static_cast<void>(sources.add_source(source.string(), std::string(DRIVER_INCREMENTAL_CACHE_SYNTHETIC_SOURCE)));
    const std::array<driver::ModuleRecord, 1> modules{{
        driver::ModuleRecord{std::string(DRIVER_INCREMENTAL_CACHE_SYNTHETIC_MODULE), source},
    }};

    sema::CheckedModule checked;
    const std::array<std::string_view, 1> module_parts{DRIVER_INCREMENTAL_CACHE_SYNTHETIC_MODULE};
    const query::ModuleKey module_key =
        query::module_key(query::package_key(std::span<const std::string_view>{}), module_parts);
    const std::array<std::string_view, 1> template_path{DRIVER_INCREMENTAL_CACHE_SYNTHETIC_FUNCTION};
    const query::DefKey template_def =
        query::def_key(module_key, query::DefNamespace::value, query::DefKind::generic_template, template_path);
    const query::CanonicalTypeKey i32_arg = query::canonical_builtin(query::BuiltinTypeKey::i32);
    const std::array<query::CanonicalTypeKey, 1> type_args{i32_arg};
    const query::GenericInstanceKey generic_instance_key = query::generic_instance_key(
        template_def, type_args, std::span<const query::StableFingerprint128>{}, query::param_env_key({}));
    const sema::StableModuleId stable_module = sema::stable_module_id(module_parts);

    const sema::InternedText duplicate_name = checked.intern_text(DRIVER_INCREMENTAL_CACHE_SYNTHETIC_FUNCTION);
    const sema::StableDefId duplicate_stable_id = sema::stable_definition_id(
        stable_module, sema::StableSymbolKind::function, DRIVER_INCREMENTAL_CACHE_SYNTHETIC_STABLE_ID);
    for (base::u32 owner_type = 0; owner_type < 2; ++owner_type) {
        sema::FunctionSignature signature = checked.make_function_signature();
        signature.name = duplicate_name;
        signature.name_id = duplicate_name.id;
        signature.stable_id = duplicate_stable_id;
        signature.incremental_key =
            sema::stable_incremental_key(duplicate_stable_id, "duplicate-signature-" + std::to_string(owner_type));
        checked.functions.emplace(sema::FunctionLookupKey{0, owner_type, duplicate_name.id}, std::move(signature));
    }
    sema::FunctionSignature second_stable_signature = checked.make_function_signature();
    second_stable_signature.name = duplicate_name;
    second_stable_signature.name_id = duplicate_name.id;
    second_stable_signature.stable_id =
        sema::stable_definition_id(stable_module, sema::StableSymbolKind::function, "generic-instance:manual:second");
    second_stable_signature.incremental_key =
        sema::stable_incremental_key(second_stable_signature.stable_id, "duplicate-signature-second-stable");
    checked.functions.emplace(sema::FunctionLookupKey{0, 2, duplicate_name.id}, std::move(second_stable_signature));
    sema::FunctionSignature invalid_signature = checked.make_function_signature();
    invalid_signature.name = checked.intern_text("skip_invalid_query_key");
    invalid_signature.name_id = invalid_signature.name.id;
    checked.functions.emplace(sema::FunctionLookupKey{0, 3, invalid_signature.name_id}, std::move(invalid_signature));

    sema::GenericFunctionInstanceInfo instance;
    instance.generic_instance_key = generic_instance_key;
    instance.signature = checked.make_function_signature();
    instance.signature.name = checked.intern_text(DRIVER_INCREMENTAL_CACHE_SYNTHETIC_FUNCTION);
    instance.signature.stable_id = sema::stable_definition_id(
        stable_module, sema::StableSymbolKind::function, DRIVER_INCREMENTAL_CACHE_SYNTHETIC_STABLE_ID);
    instance.signature.incremental_key =
        sema::stable_incremental_key(instance.signature.stable_id, DRIVER_INCREMENTAL_CACHE_SYNTHETIC_SIGNATURE);
    checked.generic_function_instances.push_back(std::move(instance));

    driver::CompilerInvocation no_cache_pruning_invocation = invocation;
    no_cache_pruning_invocation.incremental_cache_path = cache_dir / "first-pruning.axic";
    no_cache_pruning_invocation.experimental_query_pruning = true;
    driver::CompilationProfiler no_cache_pruning_profiler(true);
    auto no_cache_pruning_result = driver::write_incremental_cache(
        no_cache_pruning_invocation, sources, modules, checked, &no_cache_pruning_profiler);
    ASSERT_TRUE(no_cache_pruning_result) << no_cache_pruning_result.error().message;
    expect_query_profile_phases_with_pruning(no_cache_pruning_profiler, CACHE_TEST_QUERY_DIFF_MISSING_DETAIL,
        CACHE_TEST_QUERY_PLAN_MISSING_DETAIL, CACHE_TEST_QUERY_PRUNING_NO_CACHE_DETAIL);

    driver::CompilationProfiler first_write_profiler(true);
    auto write_result = driver::write_incremental_cache(invocation, sources, modules, checked, &first_write_profiler);
    ASSERT_TRUE(write_result) << write_result.error().message;
    expect_query_profile_phases(
        first_write_profiler, CACHE_TEST_QUERY_DIFF_MISSING_DETAIL, CACHE_TEST_QUERY_PLAN_MISSING_DETAIL);

    const std::string cache_text = read_text(cache);
    expect_contains(cache_text, "queries\t14");
    expect_contains(cache_text, "query_edges\t12");
    expect_contains(cache_text, "query\tfile_content");
    expect_contains(cache_text, "query\tlex_file");
    expect_contains(cache_text, "query\tparse_file");
    expect_contains(cache_text, "query\tmodule_exports");
    expect_contains(cache_text, "query\titem_signature");
    expect_contains(cache_text, "query\tgeneric_instance_signature");
    expect_contains(cache_text, "query\tdiagnostics");
    expect_contains(cache_text, "query_edge\tlex_file");
    expect_contains(cache_text, "query_edge\tparse_file");
    const query::DefKey expected_item_signature_key =
        query::def_key_from_stable_id(duplicate_stable_id, query::DefNamespace::value, query::DefKind::function);
    expect_contains(cache_text, hex_encode_cache_test_field(query::stable_serialize(expected_item_signature_key)));
    const std::string encoded_generic_instance_key =
        hex_encode_cache_test_field(query::stable_serialize(generic_instance_key));
    expect_contains(cache_text, encoded_generic_instance_key);
    EXPECT_TRUE(
        cache_test_query_result(cache_text, CACHE_TEST_QUERY_GENERIC_INSTANCE_SIGNATURE, encoded_generic_instance_key)
            .has_value());
    EXPECT_EQ(
        cache_text.find(hex_encode_cache_test_field(query::stable_serialize(duplicate_stable_id))), std::string::npos);

    driver::CompilationProfiler second_write_profiler(true);
    auto rewrite_result =
        driver::write_incremental_cache(invocation, sources, modules, checked, &second_write_profiler);
    ASSERT_TRUE(rewrite_result) << rewrite_result.error().message;
    expect_query_profile_phases(
        second_write_profiler, CACHE_TEST_QUERY_DIFF_UNCHANGED_DETAIL, CACHE_TEST_QUERY_PLAN_UNCHANGED_DETAIL);

    const sema::IncrementalKey expected_generic_incremental_key =
        sema::stable_incremental_key(duplicate_stable_id, DRIVER_INCREMENTAL_CACHE_SYNTHETIC_SIGNATURE);
    const std::optional<query::QueryRecord> expected_generic_record = query::generic_instance_signature_query_record(
        generic_instance_key, query::query_result_fingerprint(expected_generic_incremental_key));
    ASSERT_TRUE(expected_generic_record.has_value());
    const std::string cached_query_text = read_text(cache);
    const std::optional<CacheTestQueryResultFingerprint> expected_item_result =
        cache_test_query_result(cached_query_text, CACHE_TEST_QUERY_ITEM_SIGNATURE,
            hex_encode_cache_test_field(query::stable_serialize(expected_item_signature_key)));
    ASSERT_TRUE(expected_item_result.has_value());
    const query::StableFingerprint128 expected_item_result_fingerprint{
        std::stoull(expected_item_result->primary),
        std::stoull(expected_item_result->secondary),
        static_cast<base::u32>(std::stoul(expected_item_result->byte_count)),
    };
    const std::optional<query::QueryRecord> expected_item_record =
        query::item_signature_query_record(expected_item_signature_key,
            query::QueryResultFingerprint{
                expected_item_result_fingerprint,
                std::stoull(expected_item_result->global_id),
            });
    ASSERT_TRUE(expected_item_record.has_value());
    const query::QueryDependencyEdge cached_only_edge{
        expected_generic_record->key,
        expected_item_record->key,
    };
    const std::string cached_only_edge_row = cache_test_query_edge_row(cached_only_edge);
    std::string cache_with_cached_only_edge = cached_query_text;
    const std::string original_query_edge_count = "query_edges\t12\n";
    const std::size_t query_edge_count_pos = cache_with_cached_only_edge.find(original_query_edge_count);
    ASSERT_NE(query_edge_count_pos, std::string::npos);
    cache_with_cached_only_edge.replace(query_edge_count_pos, original_query_edge_count.size(), "query_edges\t13\n");
    cache_with_cached_only_edge += cached_only_edge_row;
    write_cache_text(cache_with_cached_only_edge);

    driver::CompilerInvocation pruning_invocation = invocation;
    pruning_invocation.experimental_query_pruning = true;
    driver::CompilationProfiler pruning_write_profiler(true);
    auto pruning_write_result =
        driver::write_incremental_cache(pruning_invocation, sources, modules, checked, &pruning_write_profiler);
    ASSERT_TRUE(pruning_write_result) << pruning_write_result.error().message;
    expect_query_profile_phases_with_pruned_provider_eval(pruning_write_profiler,
        CACHE_TEST_QUERY_DIFF_UNCHANGED_DETAIL, CACHE_TEST_QUERY_PLAN_UNCHANGED_DETAIL,
        CACHE_TEST_QUERY_PRUNING_REUSE_ALL_DETAIL, CACHE_TEST_QUERY_PROVIDER_EVAL_REUSE_ALL_DETAIL);
    const std::string pruned_cache_text = read_text(cache);
    expect_contains(pruned_cache_text, "query_edges\t13");
    expect_contains(pruned_cache_text, cached_only_edge_row);

    checked.generic_function_instances.front().signature.incremental_key =
        sema::stable_incremental_key(checked.generic_function_instances.front().signature.stable_id,
            DRIVER_INCREMENTAL_CACHE_SYNTHETIC_CHANGED_SIGNATURE);
    driver::CompilationProfiler changed_write_profiler(true);
    auto changed_write_result =
        driver::write_incremental_cache(pruning_invocation, sources, modules, checked, &changed_write_profiler);
    ASSERT_TRUE(changed_write_result) << changed_write_result.error().message;
    expect_query_profile_phases_with_pruned_provider_eval(changed_write_profiler, CACHE_TEST_QUERY_DIFF_CHANGED_DETAIL,
        CACHE_TEST_QUERY_PLAN_CHANGED_DETAIL, CACHE_TEST_QUERY_PRUNING_CHANGED_DETAIL,
        CACHE_TEST_QUERY_PROVIDER_EVAL_CHANGED_DETAIL);

    auto reuse = driver::try_reuse_incremental_check_cache(invocation);
    ASSERT_TRUE(reuse) << reuse.error().message;
    EXPECT_TRUE(reuse.value());

    driver::clear_file_cache();
}

TEST_F(AurexIntegrationTest, IncrementalCacheWritesGenericInstanceBodyRowsForTypedGenericFunctions)
{
    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_GENERIC_BODY_SOURCE =
        "module incremental_cache_generic_body;\n"
        "fn id[T](value: T) -> T { return value; }\n"
        "fn main() -> i32 { return id[i32](7); }\n";

    driver::clear_file_cache();

    const fs::path cache_dir = tmp_root() / "incremental-cache-generic-body";
    fs::create_directories(cache_dir);
    const fs::path source = cache_dir / "main.ax";
    const fs::path cache = cache_dir / "main.axic";
    {
        std::ofstream out(source, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << DRIVER_INCREMENTAL_CACHE_GENERIC_BODY_SOURCE;
    }

    driver::CompilerInvocation invocation;
    invocation.input_path = source;
    invocation.emit_kind = driver::EmitKind::typed;
    invocation.incremental_cache_path = cache;

    driver::Compiler compiler;
    auto result = compiler.run(invocation);
    ASSERT_TRUE(result) << result.error().message;

    const std::string cache_text = read_text(cache);
    expect_contains(cache_text, "query\tgeneric_instance_signature");
    expect_contains(cache_text, "query\tgeneric_instance_body");
    expect_contains(cache_text, "query\tlower_function_ir");
    expect_contains(cache_text, "query\tdiagnostics");
    expect_contains(cache_text, "query_edge\tgeneric_instance_body");
    expect_contains(cache_text, "query_edge\tlower_function_ir");

    driver::clear_file_cache();
}

TEST_F(AurexIntegrationTest, IncrementalCacheParsesQueryDependencyEdgeRows)
{
    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_EDGE_SOURCE = "module incremental_cache_query_edges;\n";
    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_EDGE_MODULE = "incremental_cache_query_edges";
    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_EDGE_FUNCTION = "helper";
    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_EDGE_EXPORTS_SIGNATURE = "edge-exports:v1";
    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_EDGE_ITEM_SIGNATURE = "edge-item-signature:v1";
    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_EDGE_UNKNOWN_KIND = "unknown_query";
    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_EDGE_CHANGED_DETAIL =
        "total=10,missing=8,unchanged=1,changed=1,malformed=0";
    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_EDGE_PLAN_DETAIL =
        "reusable=1,recompute_roots=9,propagated_recompute=0,recompute=9";
    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_EDGE_PRUNING_DETAIL =
        "enabled=1,applied=1,reused=1,recomputed=9,reused_file_contents=0,reused_lex_files=0,"
        "reused_parse_files=0,reused_module_exports=0,reused_item_signatures=1,"
        "reused_function_body_syntaxes=0,reused_type_check_bodies=0,reused_generic_instance_signatures=0,"
        "reused_generic_instance_bodies=0,reused_lower_function_irs=0,reused_diagnostics=0,"
        "recomputed_file_contents=1,recomputed_lex_files=1,recomputed_parse_files=1,recomputed_module_exports=1,"
        "recomputed_item_signatures=0,recomputed_function_body_syntaxes=0,recomputed_type_check_bodies=0,"
        "recomputed_generic_instance_signatures=0,recomputed_generic_instance_bodies=0,"
        "recomputed_lower_function_irs=0,recomputed_diagnostics=5,fallback=none";
    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_EDGE_PROVIDER_EVAL_DETAIL =
        "mode=pruned,seeded=1,evaluated=9,seeded_file_contents=0,seeded_lex_files=0,seeded_parse_files=0,"
        "seeded_module_exports=0,seeded_item_signatures=1,"
        "seeded_function_body_syntaxes=0,seeded_type_check_bodies=0,seeded_generic_instance_signatures=0,"
        "seeded_generic_instance_bodies=0,seeded_lower_function_irs=0,seeded_diagnostics=0,"
        "evaluated_file_contents=1,evaluated_lex_files=1,evaluated_parse_files=1,evaluated_module_exports=1,"
        "evaluated_item_signatures=0,evaluated_function_body_syntaxes=0,evaluated_type_check_bodies=0,"
        "evaluated_generic_instance_signatures=0,evaluated_generic_instance_bodies=0,"
        "evaluated_lower_function_irs=0,evaluated_diagnostics=5";

    driver::clear_file_cache();

    const fs::path cache_dir = tmp_root() / "incremental-cache-query-edges";
    fs::create_directories(cache_dir);
    const fs::path source = cache_dir / "main.ax";
    const fs::path cache = cache_dir / "main.axic";
    {
        std::ofstream out(source, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << DRIVER_INCREMENTAL_CACHE_EDGE_SOURCE;
    }

    driver::CompilerInvocation invocation;
    invocation.input_path = source;
    invocation.emit_kind = driver::EmitKind::check;
    invocation.incremental_cache_path = cache;
    invocation.experimental_query_pruning = true;
    const fs::path canonical_source = fs::weakly_canonical(source);

    const auto write_cache = [&](const std::string_view text) {
        std::ofstream out(cache, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << text;
    };
    const auto expect_reused = [&] {
        auto reuse = driver::try_reuse_incremental_check_cache(invocation);
        ASSERT_TRUE(reuse) << reuse.error().message;
        EXPECT_TRUE(reuse.value());
    };
    const auto expect_not_reused = [&] {
        auto reuse = driver::try_reuse_incremental_check_cache(invocation);
        ASSERT_TRUE(reuse) << reuse.error().message;
        EXPECT_FALSE(reuse.value());
    };

    const std::array<std::string_view, 1> module_parts{DRIVER_INCREMENTAL_CACHE_EDGE_MODULE};
    const sema::StableModuleId stable_module = sema::stable_module_id(module_parts);
    const query::ModuleKey module_key = query::module_key_from_stable_id(stable_module);
    const sema::StableDefId stable_id = sema::stable_definition_id(
        stable_module, sema::StableSymbolKind::function, DRIVER_INCREMENTAL_CACHE_EDGE_FUNCTION);
    const query::DefKey def_key =
        query::def_key_from_stable_id(stable_id, query::DefNamespace::value, query::DefKind::function);
    const query::QueryResultFingerprint exports_result = query::query_result_fingerprint(
        sema::stable_incremental_key(stable_id, DRIVER_INCREMENTAL_CACHE_EDGE_EXPORTS_SIGNATURE));
    const query::QueryResultFingerprint item_result = query::query_result_fingerprint(
        sema::stable_incremental_key(stable_id, DRIVER_INCREMENTAL_CACHE_EDGE_ITEM_SIGNATURE));
    const std::optional<query::QueryRecord> exports_record = query::query_record(query::QueryKind::module_exports,
        query::stable_key_fingerprint(module_key), query::stable_serialize(module_key), exports_result);
    const std::optional<query::QueryRecord> item_record = query::item_signature_query_record(def_key, item_result);
    ASSERT_TRUE(exports_record.has_value());
    ASSERT_TRUE(item_record.has_value());

    const query::QueryDependencyEdge edge{
        item_record->key,
        exports_record->key,
    };

    std::string old_cache = cache_test_header(canonical_source);
    old_cache += "sources\t1\n";
    old_cache += cache_test_source_row(canonical_source, DRIVER_INCREMENTAL_CACHE_EDGE_SOURCE);
    old_cache += "modules\t0\n";
    old_cache += "definitions\t0\n";
    old_cache += "queries\t0\n";
    write_cache(old_cache);
    expect_reused();

    std::string edge_cache = cache_test_header(canonical_source);
    edge_cache += "sources\t1\n";
    edge_cache += cache_test_source_row(canonical_source, DRIVER_INCREMENTAL_CACHE_EDGE_SOURCE);
    edge_cache += "modules\t0\n";
    edge_cache += "definitions\t0\n";
    edge_cache += "queries\t2\n";
    edge_cache += cache_test_query_row(*exports_record);
    edge_cache += cache_test_query_row(*item_record);
    edge_cache += "query_edges\t1\n";
    edge_cache += cache_test_query_edge_row(edge);
    write_cache(edge_cache);
    expect_reused();

    base::SourceManager sources;
    static_cast<void>(sources.add_source(source.string(), std::string(DRIVER_INCREMENTAL_CACHE_EDGE_SOURCE)));
    const std::array<driver::ModuleRecord, 1> modules{{
        driver::ModuleRecord{std::string(DRIVER_INCREMENTAL_CACHE_EDGE_MODULE), source},
    }};
    sema::CheckedModule checked;
    const sema::InternedText function_name = checked.intern_text(DRIVER_INCREMENTAL_CACHE_EDGE_FUNCTION);
    sema::FunctionSignature signature = checked.make_function_signature();
    signature.name = function_name;
    signature.name_id = function_name.id;
    signature.stable_id = stable_id;
    signature.incremental_key = sema::stable_incremental_key(stable_id, DRIVER_INCREMENTAL_CACHE_EDGE_ITEM_SIGNATURE);
    checked.functions.emplace(sema::FunctionLookupKey{0, 0, function_name.id}, std::move(signature));

    driver::CompilationProfiler profiler(true);
    auto write_result = driver::write_incremental_cache(invocation, sources, modules, checked, &profiler);
    ASSERT_TRUE(write_result) << write_result.error().message;
    expect_query_profile_phases_with_pruned_provider_eval(profiler, DRIVER_INCREMENTAL_CACHE_EDGE_CHANGED_DETAIL,
        DRIVER_INCREMENTAL_CACHE_EDGE_PLAN_DETAIL, DRIVER_INCREMENTAL_CACHE_EDGE_PRUNING_DETAIL,
        DRIVER_INCREMENTAL_CACHE_EDGE_PROVIDER_EVAL_DETAIL);

    std::string missing_edge_count_cache = edge_cache;
    const std::string edge_count_header = "query_edges\t1\n";
    const std::size_t edge_count_pos = missing_edge_count_cache.find(edge_count_header);
    ASSERT_NE(edge_count_pos, std::string::npos);
    missing_edge_count_cache.erase(edge_count_pos, edge_count_header.size());
    write_cache(missing_edge_count_cache);
    expect_not_reused();

    std::string duplicate_edge_cache = edge_cache;
    const std::string duplicate_edge_count_header = "query_edges\t1\n";
    const std::size_t duplicate_edge_count_pos = duplicate_edge_cache.find(duplicate_edge_count_header);
    ASSERT_NE(duplicate_edge_count_pos, std::string::npos);
    duplicate_edge_cache.replace(duplicate_edge_count_pos, duplicate_edge_count_header.size(), "query_edges\t2\n");
    duplicate_edge_cache += cache_test_query_edge_row(edge);
    write_cache(duplicate_edge_cache);
    expect_not_reused();

    std::string invalid_edge_kind_cache = edge_cache;
    const std::string valid_edge_kind_prefix = "query_edge\titem_signature";
    const std::size_t edge_kind_pos = invalid_edge_kind_cache.find(valid_edge_kind_prefix);
    ASSERT_NE(edge_kind_pos, std::string::npos);
    invalid_edge_kind_cache.replace(edge_kind_pos, valid_edge_kind_prefix.size(),
        "query_edge\t" + std::string(DRIVER_INCREMENTAL_CACHE_EDGE_UNKNOWN_KIND));
    write_cache(invalid_edge_kind_cache);
    expect_not_reused();

    driver::clear_file_cache();
}

TEST_F(AurexIntegrationTest, IncrementalCacheItemSignatureIgnoresBodyOnlyChanges)
{
    driver::clear_file_cache();

    const fs::path cache_dir = tmp_root() / "incremental-cache-signature";
    fs::create_directories(cache_dir);
    const fs::path source = cache_dir / "main.ax";
    const fs::path cache = cache_dir / "main.axic";
    const auto write_source = [&](const std::string_view text) {
        std::ofstream out(source, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << text;
    };

    const std::array<std::string_view, 1> module_parts{"incremental_cache_signature"};
    const sema::StableModuleId stable_module = sema::stable_module_id(module_parts);
    const sema::StableDefId helper_stable_id =
        sema::stable_definition_id(stable_module, sema::StableSymbolKind::function, "helper");
    const query::DefKey helper_def_key =
        query::def_key_from_stable_id(helper_stable_id, query::DefNamespace::value, query::DefKind::function);
    const query::BodyKey helper_body_key = query::body_key(helper_def_key, query::BodySlotKind::function_body);
    const std::string encoded_helper_key = hex_encode_cache_test_field(query::stable_serialize(helper_def_key));
    const std::string encoded_helper_body_key = hex_encode_cache_test_field(query::stable_serialize(helper_body_key));

    driver::CompilerInvocation invocation;
    invocation.input_path = source;
    invocation.emit_kind = driver::EmitKind::check;
    invocation.incremental_cache_path = cache;

    write_source(DRIVER_INCREMENTAL_CACHE_SIGNATURE_FIRST_SOURCE);
    driver::Compiler compiler;
    auto first = compiler.run(invocation);
    ASSERT_TRUE(first) << first.error().message;
    const std::string first_cache = read_text(cache);
    const std::optional<CacheTestQueryResultFingerprint> first_result =
        cache_test_item_signature_result(first_cache, encoded_helper_key);
    ASSERT_TRUE(first_result.has_value());
    const std::optional<CacheTestQueryResultFingerprint> first_body_result =
        cache_test_function_body_syntax_result(first_cache, encoded_helper_body_key);
    ASSERT_TRUE(first_body_result.has_value());
    const std::optional<CacheTestQueryResultFingerprint> first_type_check_result =
        cache_test_type_check_body_result(first_cache, encoded_helper_body_key);
    ASSERT_TRUE(first_type_check_result.has_value());

    write_source(DRIVER_INCREMENTAL_CACHE_SIGNATURE_SECOND_SOURCE);
    driver::clear_file_cache();
    auto second = compiler.run(invocation);
    ASSERT_TRUE(second) << second.error().message;
    const std::string second_cache = read_text(cache);
    const std::optional<CacheTestQueryResultFingerprint> second_result =
        cache_test_item_signature_result(second_cache, encoded_helper_key);
    ASSERT_TRUE(second_result.has_value());
    const std::optional<CacheTestQueryResultFingerprint> second_body_result =
        cache_test_function_body_syntax_result(second_cache, encoded_helper_body_key);
    ASSERT_TRUE(second_body_result.has_value());
    const std::optional<CacheTestQueryResultFingerprint> second_type_check_result =
        cache_test_type_check_body_result(second_cache, encoded_helper_body_key);
    ASSERT_TRUE(second_type_check_result.has_value());

    EXPECT_EQ(*first_result, *second_result);
    EXPECT_FALSE(*first_body_result == *second_body_result);
    EXPECT_FALSE(*first_type_check_result == *second_type_check_result);

    driver::clear_file_cache();
}

TEST_F(AurexIntegrationTest, IncrementalCacheItemSignatureChangesWhenSignatureChanges)
{
    driver::clear_file_cache();

    const fs::path cache_dir = tmp_root() / "incremental-cache-signature-change";
    fs::create_directories(cache_dir);
    const fs::path source = cache_dir / "main.ax";
    const fs::path cache = cache_dir / "main.axic";
    const auto write_source = [&](const std::string_view text) {
        std::ofstream out(source, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << text;
    };

    const std::array<std::string_view, 1> module_parts{"incremental_cache_signature_change"};
    const sema::StableModuleId stable_module = sema::stable_module_id(module_parts);
    const query::ModuleKey module_key = query::module_key_from_stable_id(stable_module);
    const sema::StableDefId helper_stable_id =
        sema::stable_definition_id(stable_module, sema::StableSymbolKind::function, "helper");
    const query::DefKey helper_def_key =
        query::def_key_from_stable_id(helper_stable_id, query::DefNamespace::value, query::DefKind::function);
    const std::string encoded_helper_key = hex_encode_cache_test_field(query::stable_serialize(helper_def_key));
    const std::string encoded_module_key = hex_encode_cache_test_field(query::stable_serialize(module_key));

    driver::CompilerInvocation invocation;
    invocation.input_path = source;
    invocation.emit_kind = driver::EmitKind::check;
    invocation.incremental_cache_path = cache;

    write_source(DRIVER_INCREMENTAL_CACHE_SIGNATURE_CHANGE_FIRST_SOURCE);
    driver::Compiler compiler;
    auto first = compiler.run(invocation);
    ASSERT_TRUE(first) << first.error().message;
    const std::optional<CacheTestQueryResultFingerprint> first_result =
        cache_test_item_signature_result(read_text(cache), encoded_helper_key);
    ASSERT_TRUE(first_result.has_value());
    const std::optional<CacheTestQueryResultFingerprint> first_exports_result =
        cache_test_query_result(read_text(cache), CACHE_TEST_QUERY_MODULE_EXPORTS, encoded_module_key);
    ASSERT_TRUE(first_exports_result.has_value());

    write_source(DRIVER_INCREMENTAL_CACHE_SIGNATURE_CHANGE_SECOND_SOURCE);
    driver::clear_file_cache();
    auto second = compiler.run(invocation);
    ASSERT_TRUE(second) << second.error().message;
    const std::optional<CacheTestQueryResultFingerprint> second_result =
        cache_test_item_signature_result(read_text(cache), encoded_helper_key);
    ASSERT_TRUE(second_result.has_value());
    const std::optional<CacheTestQueryResultFingerprint> second_exports_result =
        cache_test_query_result(read_text(cache), CACHE_TEST_QUERY_MODULE_EXPORTS, encoded_module_key);
    ASSERT_TRUE(second_exports_result.has_value());

    EXPECT_FALSE(*first_result == *second_result);
    EXPECT_EQ(*first_exports_result, *second_exports_result);

    driver::clear_file_cache();
}

TEST_F(AurexIntegrationTest, IncrementalCacheRejectsMalformedMismatchedAndBlockedCacheFiles)
{
    driver::clear_file_cache();

    const fs::path cache_dir = tmp_root() / "incremental-cache-malformed";
    fs::create_directories(cache_dir);
    const fs::path source = cache_dir / "main.ax";
    const fs::path cache = cache_dir / "main.axic";
    {
        std::ofstream out(source, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << DRIVER_INCREMENTAL_CACHE_FIRST_SOURCE;
    }

    driver::CompilerInvocation invocation;
    invocation.input_path = source;
    invocation.emit_kind = driver::EmitKind::check;
    invocation.incremental_cache_path = cache;
    const fs::path canonical_source = fs::weakly_canonical(source);

    const auto write_cache = [&](const std::string_view text) {
        std::ofstream out(cache, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << text;
    };
    const auto expect_not_reused = [&] {
        auto reuse = driver::try_reuse_incremental_check_cache(invocation);
        ASSERT_TRUE(reuse) << reuse.error().message;
        EXPECT_FALSE(reuse.value());
    };
    const auto valid_query_row = [&]() -> std::string {
        const std::array<std::string_view, 1> module_parts{"incremental_cache_malformed"};
        const sema::StableModuleId stable_module = sema::stable_module_id(module_parts);
        const sema::StableDefId stable_id =
            sema::stable_definition_id(stable_module, sema::StableSymbolKind::function, "helper");
        const query::DefKey def_key =
            query::def_key_from_stable_id(stable_id, query::DefNamespace::value, query::DefKind::function);
        const query::QueryResultFingerprint result =
            query::query_result_fingerprint(sema::stable_incremental_key(stable_id, "signature:i32"));
        const std::optional<query::QueryRecord> record = query::item_signature_query_record(def_key, result);
        EXPECT_TRUE(record.has_value());
        return record.has_value() ? cache_test_query_row(*record) : std::string{};
    };

    expect_not_reused();
    driver::CompilerInvocation typed_probe = invocation;
    typed_probe.emit_kind = driver::EmitKind::typed;
    auto typed_reuse = driver::try_reuse_incremental_check_cache(typed_probe);
    ASSERT_TRUE(typed_reuse) << typed_reuse.error().message;
    EXPECT_FALSE(typed_reuse.value());

    write_cache("not-a-cache\n");
    expect_not_reused();

    write_cache("aurex-incremental-cache-v1\nschema\tnope\n");
    expect_not_reused();

    write_cache("aurex-incremental-cache-v1\nschema\t1\ncompiler\t0\n");
    expect_not_reused();

    write_cache("aurex-incremental-cache-v1\nschema\t1\ncompiler\tzz\n");
    expect_not_reused();

    write_cache("aurex-incremental-cache-v1\nschema\t1\nschema\t1\n");
    expect_not_reused();

    write_cache("aurex-incremental-cache-v1\nschema\t1\nimport_paths\tnope\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source, {cache_dir / "imports-a"})
        + "import_path\nsources\t0\nmodules\t0\ndefinitions\t0\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source, {cache_dir / "imports-a"})
        + "import_path\t0\nsources\t0\nmodules\t0\ndefinitions\t0\n");
    expect_not_reused();

    write_cache("aurex-incremental-cache-v1\nunknown\t1\n");
    expect_not_reused();

    write_cache("aurex-incremental-cache-v1\nschema\t1\textra\n");
    expect_not_reused();

    {
        std::string uppercase_cache = cache_test_header(canonical_source);
        const std::string lower_version = hex_encode_cache_test_field(base::config::AUREX_VERSION_STRING);
        const std::string upper_version = uppercase_hex_cache_test_field(lower_version);
        const std::size_t version_pos = uppercase_cache.find(lower_version);
        ASSERT_NE(version_pos, std::string::npos);
        uppercase_cache.replace(version_pos, lower_version.size(), upper_version);
        uppercase_cache += "sources\t0\nmodules\t0\ndefinitions\t0\n";
        write_cache(uppercase_cache);
        expect_not_reused();
    }

    write_cache(cache_test_header_with_compiler_and_mode(canonical_source, "bad-version", "semantic-ok")
        + "sources\t0\nmodules\t0\ndefinitions\t0\n");
    expect_not_reused();

    write_cache(cache_test_header_with_compiler_and_mode(canonical_source, base::config::AUREX_VERSION_STRING, "not-ok")
        + "sources\t0\nmodules\t0\ndefinitions\t0\n");
    expect_not_reused();

    write_cache(cache_test_header(cache_dir / "other.ax") + "sources\t0\nmodules\t0\ndefinitions\t0\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "\n" + "sources\t0\nmodules\t0\ndefinitions\t0\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t1\nmodules\t0\ndefinitions\t0\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "import_paths\t0\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "mode\t" + hex_encode_cache_test_field("semantic-ok") + "\n");
    expect_not_reused();

    write_cache(
        cache_test_header(canonical_source) + "root\t" + hex_encode_cache_test_field(canonical_source.string()) + "\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t1\nsource\tbad\t0\t0\t0\t"
        + hex_encode_cache_test_field(canonical_source.string()) + "\nmodules\t0\ndefinitions\t0\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t1\nsource\t1\t0\t0\tbad\t"
        + hex_encode_cache_test_field(canonical_source.string()) + "\nmodules\t0\ndefinitions\t0\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t1\n"
        + cache_test_source_row(canonical_source, "1", "bad", "0", "0") + "modules\t0\ndefinitions\t0\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t1\n"
        + cache_test_source_row(canonical_source, "1", "0", "bad", "0") + "modules\t0\ndefinitions\t0\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t1\n"
        + cache_test_source_row(canonical_source, "1", "0", "0", "4294967296") + "modules\t0\ndefinitions\t0\n");
    expect_not_reused();

    write_cache(
        cache_test_header(canonical_source) + "sources\t1\nsource\t1\t0\t0\t1\t0\n" + "modules\t0\ndefinitions\t0\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t1\nsource\ttoo-few\n" + "modules\t0\ndefinitions\t0\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t1\nmodule\tbad\n" + "definitions\t0\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t1\nmodule\t00\t0\n" + "definitions\t0\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t1\nmodule\t0\t00\n" + "definitions\t0\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t1\n" + "def\ttoo-few\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t1\n"
        + "def\t\tfunction\t0\t0\t0\t0\t0\t0\t0\t0\t00\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t1\n"
        + "def\tfunction\tfunction\tbad\t0\t0\t0\t0\t0\t0\t0\t00\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t1\n"
        + cache_test_def_row("function", "", "0", "0", "0", "0", "0", "0", "0", "0", "00"));
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t1\n"
        + cache_test_def_row("function", "function", "0", "bad", "0", "0", "0", "0", "0", "0", "00"));
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t1\n"
        + cache_test_def_row("function", "function", "0", "0", "bad", "0", "0", "0", "0", "0", "00"));
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t1\n"
        + cache_test_def_row("function", "function", "0", "0", "0", "4294967296", "0", "0", "0", "0", "00"));
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t1\n"
        + cache_test_def_row("function", "function", "0", "0", "0", "0", "bad", "0", "0", "0", "00"));
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t1\n"
        + cache_test_def_row("function", "function", "0", "0", "0", "0", "0", "bad", "0", "0", "00"));
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t1\n"
        + cache_test_def_row("function", "function", "0", "0", "0", "0", "0", "0", "bad", "0", "00"));
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t1\n"
        + cache_test_def_row("function", "function", "0", "0", "0", "0", "0", "0", "0", "4294967296", "00"));
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t1\n"
        + cache_test_def_row("function", "function", "0", "0", "0", "0", "0", "0", "0", "0", "0"));
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t0\nqueries\tbad\n");
    expect_not_reused();

    write_cache(
        cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t0\nqueries\t0\nquery_edges\tbad\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source)
        + "sources\t0\nmodules\t0\ndefinitions\t0\nqueries\t0\nquery_edges\t1\nquery_edge\ttoo-few\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t0\n"
        + cache_test_query_row("item_signature", "1", "1", "0", "0", "0", "1", "0", "0", "0", "00"));
    expect_not_reused();

    write_cache(
        cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t0\nqueries\t1\nquery\ttoo-few\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t0\nqueries\t1\n"
        + cache_test_query_row("unknown_query", "1", "1", "0", "0", "0", "1", "0", "0", "0", "00"));
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t0\nqueries\t1\n"
        + cache_test_query_row("item_signature", "2", "1", "0", "0", "0", "1", "0", "0", "0", "00"));
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t0\nqueries\t1\n"
        + cache_test_query_row("item_signature", "bad", "1", "0", "0", "0", "1", "0", "0", "0", "00"));
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t0\nqueries\t1\n"
        + cache_test_query_row("item_signature", "1", "bad", "0", "0", "0", "1", "0", "0", "0", "00"));
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t0\nqueries\t1\n"
        + cache_test_query_row("item_signature", "1", "0", "0", "0", "0", "1", "0", "0", "0", "00"));
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t0\nqueries\t1\n"
        + cache_test_query_row("item_signature", "1", "1", "bad", "0", "0", "1", "0", "0", "0", "00"));
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t0\nqueries\t1\n"
        + cache_test_query_row("item_signature", "1", "1", "0", "bad", "0", "1", "0", "0", "0", "00"));
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t0\nqueries\t1\n"
        + cache_test_query_row("item_signature", "1", "1", "0", "0", "4294967296", "1", "0", "0", "0", "00"));
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t0\nqueries\t1\n"
        + cache_test_query_row("item_signature", "1", "1", "0", "0", "0", "bad", "0", "0", "0", "00"));
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t0\nqueries\t1\n"
        + cache_test_query_row("item_signature", "1", "1", "0", "0", "0", "0", "0", "0", "0", "00"));
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t0\nqueries\t1\n"
        + cache_test_query_row("item_signature", "1", "1", "0", "0", "0", "1", "bad", "0", "0", "00"));
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t0\nqueries\t1\n"
        + cache_test_query_row("item_signature", "1", "1", "0", "0", "0", "1", "0", "bad", "0", "00"));
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t0\nqueries\t1\n"
        + cache_test_query_row("item_signature", "1", "1", "0", "0", "0", "1", "0", "0", "4294967296", "00"));
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t0\nqueries\t1\n"
        + cache_test_query_row("item_signature", "1", "1", "0", "0", "0", "1", "0", "0", "0", "zz"));
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t0\nqueries\t1\n"
        + cache_test_query_row("item_signature", "1", "1", "0", "0", "0", "1", "0", "0", "0", ""));
    expect_not_reused();

    {
        const std::string row = valid_query_row();
        ASSERT_FALSE(row.empty());
        write_cache(
            cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t0\nqueries\t2\n" + row + row);
        expect_not_reused();

        write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t0\nqueries\t2\n" + row);
        expect_not_reused();
    }

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t0\nqueries\t2\n"
        + cache_test_query_row("item_signature", "1", "1", "0", "0", "0", "1", "0", "0", "0", "00"));
    expect_not_reused();

    const fs::path null_device("/dev/null");
    if (fs::exists(null_device)) {
        write_cache(cache_test_header(canonical_source) + "sources\t1\n"
            + cache_test_source_row(null_device, "1", "0", "0", "1") + "modules\t0\ndefinitions\t0\n");
        expect_not_reused();
    }

    write_cache(
        cache_test_header(canonical_source, {cache_dir / "imports-a"}) + "sources\t0\nmodules\t0\ndefinitions\t0\n");
    driver::CompilerInvocation wrong_import_value = invocation;
    wrong_import_value.import_paths.push_back(cache_dir / "imports-b");
    auto wrong_import_value_reuse = driver::try_reuse_incremental_check_cache(wrong_import_value);
    ASSERT_TRUE(wrong_import_value_reuse) << wrong_import_value_reuse.error().message;
    EXPECT_FALSE(wrong_import_value_reuse.value());

    write_cache(minimal_cache_without_sources(canonical_source));
    expect_not_reused();

    driver::Compiler compiler;
    auto first = compiler.run(invocation);
    ASSERT_TRUE(first) << first.error().message;

    driver::CompilerInvocation wrong_imports = invocation;
    wrong_imports.import_paths.push_back(cache_dir / "imports");
    auto wrong_import_reuse = driver::try_reuse_incremental_check_cache(wrong_imports);
    ASSERT_TRUE(wrong_import_reuse) << wrong_import_reuse.error().message;
    EXPECT_FALSE(wrong_import_reuse.value());

    fs::remove(source);
    driver::clear_file_cache();
    auto missing_source_reuse = driver::try_reuse_incremental_check_cache(invocation);
    ASSERT_TRUE(missing_source_reuse) << missing_source_reuse.error().message;
    EXPECT_FALSE(missing_source_reuse.value());

    {
        std::ofstream out(source, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << DRIVER_INCREMENTAL_CACHE_FIRST_SOURCE;
    }
    driver::clear_file_cache();

    const fs::path blocked_parent = cache_dir / "blocked-parent";
    {
        std::ofstream out(blocked_parent, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << "not a directory";
    }
    driver::CompilerInvocation blocked = invocation;
    blocked.incremental_cache_path = blocked_parent / "cache.axic";
    auto blocked_result = compiler.run(blocked);
    ASSERT_FALSE(blocked_result);
    EXPECT_EQ(blocked_result.error().code, base::ErrorCode::io_error);
    expect_contains(blocked_result.error().message, "incremental cache directory");

    const fs::path directory_cache = cache_dir / "directory-cache";
    fs::create_directories(directory_cache);
    driver::CompilerInvocation directory_target = invocation;
    directory_target.incremental_cache_path = directory_cache;
    auto directory_target_result = compiler.run(directory_target);
    ASSERT_FALSE(directory_target_result);
    EXPECT_EQ(directory_target_result.error().code, base::ErrorCode::io_error);
    expect_contains(directory_target_result.error().message, "incremental cache file");

    driver::clear_file_cache();
}

TEST_F(AurexIntegrationTest, IncrementalCacheTracksImportPathAndDependencyFingerprints)
{
    driver::clear_file_cache();

    const fs::path cache_dir = tmp_root() / "incremental-cache-imports";
    const fs::path import_dir = cache_dir / "imports";
    const fs::path import_file = import_dir / "shared" / "util.ax";
    const fs::path root = cache_dir / "root.ax";
    const fs::path cache = cache_dir / "root.axic";
    fs::create_directories(import_file.parent_path());

    {
        std::ofstream out(root, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << DRIVER_INCREMENTAL_CACHE_IMPORT_ROOT_SOURCE;
    }
    {
        std::ofstream out(import_file, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << DRIVER_INCREMENTAL_CACHE_IMPORT_FIRST_SOURCE;
    }

    driver::CompilerInvocation invocation;
    invocation.input_path = root;
    invocation.emit_kind = driver::EmitKind::check;
    invocation.incremental_cache_path = cache;
    invocation.import_paths.push_back(import_dir);

    driver::Compiler compiler;
    auto first = compiler.run(invocation);
    ASSERT_TRUE(first) << first.error().message;

    const std::string first_cache = read_text(cache);
    expect_contains(first_cache, "import_paths\t1");
    expect_contains(first_cache, "sources\t2");

    auto first_reuse = driver::try_reuse_incremental_check_cache(invocation);
    ASSERT_TRUE(first_reuse) << first_reuse.error().message;
    EXPECT_TRUE(first_reuse.value());

    {
        std::ofstream out(import_file, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << DRIVER_INCREMENTAL_CACHE_IMPORT_SECOND_SOURCE;
    }
    driver::clear_file_cache();

    auto stale = driver::try_reuse_incremental_check_cache(invocation);
    ASSERT_TRUE(stale) << stale.error().message;
    EXPECT_FALSE(stale.value());

    auto second = compiler.run(invocation);
    ASSERT_TRUE(second) << second.error().message;
    auto second_reuse = driver::try_reuse_incremental_check_cache(invocation);
    ASSERT_TRUE(second_reuse) << second_reuse.error().message;
    EXPECT_TRUE(second_reuse.value());
    EXPECT_NE(first_cache, read_text(cache));

    driver::clear_file_cache();
}

} // namespace aurex::test
