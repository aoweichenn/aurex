#include <aurex/application/driver/cli.hpp>
#include <aurex/application/driver/cli_llvm.hpp>
#include <aurex/application/driver/compiler.hpp>
#include <aurex/application/driver/diagnostic_renderer.hpp>
#include <aurex/application/driver/file_cache.hpp>
#include <aurex/application/driver/incremental_cache.hpp>
#include <aurex/application/driver/package_identity.hpp>
#include <aurex/application/driver/profile.hpp>
#include <aurex/application/driver/project_model.hpp>
#include <aurex/frontend/sema/checked_module.hpp>
#include <aurex/infrastructure/base/config.hpp>
#include <aurex/infrastructure/pipeline/stage.hpp>
#include <aurex/infrastructure/query/generic_instance_key.hpp>
#include <aurex/infrastructure/query/generic_instance_signature_query.hpp>
#include <aurex/infrastructure/query/item_signature_query.hpp>
#include <aurex/infrastructure/query/query_context.hpp>
#include <aurex/infrastructure/query/query_result.hpp>

#include <support/test_support.hpp>

#include <array>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <thread>
#include <utility>
#include <vector>

namespace aurex::test {
namespace {

constexpr std::string_view DRIVER_FILE_CACHE_FIRST_TEXT = "cached-one";
constexpr std::string_view DRIVER_FILE_CACHE_SECOND_TEXT = "cached-two";
constexpr std::string_view DRIVER_FILE_CACHE_FIFO_TEXT = "fifo-stream";
constexpr int DRIVER_TEST_UNKNOWN_DIAGNOSTIC_CATEGORY_VALUE = 99;
constexpr std::string_view DRIVER_TEST_NO_DIAGNOSTIC_OWNER = "none";
constexpr std::string_view DRIVER_INCREMENTAL_CACHE_FIRST_SOURCE =
    "module incremental_cache_driver;\n"
    "pub type Count = i32;\n"
    "pub struct Box { pub value: Count; }\n"
    "pub enum Mode: u8 { fast = 1, slow = 2, }\n"
    "impl Box { pub fn read(self: &Box) -> Count { return self.value; } }\n"
    "fn id[T](value: T) -> T { return value; }\n"
    "fn main() -> i32 { let box: Box = Box { value: id[Count](0) }; return box.read(); }\n";
constexpr std::string_view DRIVER_INCREMENTAL_CACHE_COMMENT_ONLY_SOURCE =
    "module incremental_cache_driver;\n"
    "// comment-only edit should preserve lossy token and parse query results\n"
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
constexpr std::string_view DRIVER_INCREMENTAL_CACHE_IMPORT_COMMENT_ONLY_SOURCE =
    "module shared.util;\n"
    "// dependency comment-only edit should preserve token and parse query results\n"
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
constexpr std::string_view DRIVER_INCREMENTAL_CACHE_PUBLIC_SIGNATURE_FIRST_SOURCE =
    "module incremental_cache_public_signature;\n"
    "pub const LIMIT: i32 = 7;\n"
    "pub opaque struct Handle;\n"
    "priv struct Secret {\n"
    "  pub value: i32;\n"
    "}\n"
    "pub struct Surface {\n"
    "  pub value: i32;\n"
    "  priv hidden: i32;\n"
    "}\n"
    "pub enum Pair: u8 {\n"
    "  both(i32, bool) = 1,\n"
    "  none = 2,\n"
    "}\n"
    "impl Pair {\n"
    "  pub fn is_none(self: Pair) -> bool {\n"
    "    return match self {\n"
    "      .none => true,\n"
    "      _ => false,\n"
    "    };\n"
    "  }\n"
    "}\n"
    "impl Secret {\n"
    "  pub fn reveal(self: &Secret) -> i32 {\n"
    "    return self.value;\n"
    "  }\n"
    "}\n"
    "extern c {\n"
    "  @name(\"native_log\")\n"
    "  fn native_log(format: *const u8, ...) -> i32;\n"
    "}\n"
    "pub fn exported(value: i32) -> i32 {\n"
    "  return value;\n"
    "}\n";
constexpr std::string_view DRIVER_INCREMENTAL_CACHE_PUBLIC_SIGNATURE_SECOND_SOURCE =
    "module incremental_cache_public_signature;\n"
    "pub const LIMIT: i32 = 7;\n"
    "pub opaque struct Handle;\n"
    "priv struct Secret {\n"
    "  pub value: i32;\n"
    "}\n"
    "pub struct Surface {\n"
    "  pub value: i32;\n"
    "  priv hidden: i32;\n"
    "}\n"
    "pub enum Pair: u8 {\n"
    "  both(i32, bool) = 1,\n"
    "  none = 2,\n"
    "}\n"
    "impl Pair {\n"
    "  pub fn is_none(self: Pair) -> bool {\n"
    "    return match self {\n"
    "      .none => true,\n"
    "      _ => false,\n"
    "    };\n"
    "  }\n"
    "}\n"
    "impl Secret {\n"
    "  pub fn reveal(self: &Secret) -> i32 {\n"
    "    return self.value;\n"
    "  }\n"
    "}\n"
    "extern c {\n"
    "  @name(\"native_log\")\n"
    "  fn native_log(format: *const u8, ...) -> i32;\n"
    "}\n"
    "pub fn exported(value: i32, extra: i32) -> i32 {\n"
    "  return value + extra;\n"
    "}\n";
constexpr base::usize CACHE_TEST_QUERY_ROW_KIND_FIELD = 0;
constexpr base::usize CACHE_TEST_QUERY_KIND_FIELD = 1;
constexpr base::usize CACHE_TEST_QUERY_RESULT_GLOBAL_FIELD = 7;
constexpr base::usize CACHE_TEST_QUERY_RESULT_PRIMARY_FIELD = 8;
constexpr base::usize CACHE_TEST_QUERY_RESULT_SECONDARY_FIELD = 9;
constexpr base::usize CACHE_TEST_QUERY_RESULT_BYTES_FIELD = 10;
constexpr base::usize CACHE_TEST_QUERY_STABLE_KEY_FIELD = 11;
constexpr base::usize CACHE_TEST_QUERY_FIELD_COUNT = 12;
constexpr base::usize CACHE_TEST_QUERY_EDGE_DEPENDENT_KIND_FIELD = 1;
constexpr base::usize CACHE_TEST_QUERY_EDGE_DEPENDENCY_KIND_FIELD = 7;
constexpr base::usize CACHE_TEST_QUERY_EDGE_FIELD_COUNT = 13;
constexpr std::string_view CACHE_TEST_QUERY_ROW_KIND = "query";
constexpr std::string_view CACHE_TEST_QUERY_EDGE_ROW_KIND = "query_edge";
constexpr std::string_view CACHE_TEST_QUERY_PROJECT_GRAPH = "project_graph";
constexpr std::string_view CACHE_TEST_QUERY_FILE_CONTENT = "file_content";
constexpr std::string_view CACHE_TEST_QUERY_LEX_FILE = "lex_file";
constexpr std::string_view CACHE_TEST_QUERY_PARSE_FILE = "parse_file";
constexpr std::string_view CACHE_TEST_QUERY_MODULE_PART = "module_part";
constexpr std::string_view CACHE_TEST_QUERY_MODULE_GRAPH = "module_graph";
constexpr std::string_view CACHE_TEST_QUERY_MODULE_EXPORTS = "module_exports";
constexpr std::string_view CACHE_TEST_QUERY_MODULE_PACKAGE_EXPORTS = "module_package_exports";
constexpr std::string_view CACHE_TEST_QUERY_ITEM_LIST = "item_list";
constexpr std::string_view CACHE_TEST_QUERY_ITEM_SIGNATURE = "item_signature";
constexpr std::string_view CACHE_TEST_QUERY_FUNCTION_BODY_SYNTAX = "function_body_syntax";
constexpr std::string_view CACHE_TEST_QUERY_TYPE_CHECK_BODY = "type_check_body";
constexpr std::string_view CACHE_TEST_QUERY_GENERIC_TEMPLATE_SIGNATURE = "generic_template_signature";
constexpr std::string_view CACHE_TEST_QUERY_GENERIC_INSTANCE_SIGNATURE = "generic_instance_signature";
constexpr std::string_view CACHE_TEST_QUERY_GENERIC_INSTANCE_BODY = "generic_instance_body";
constexpr std::string_view CACHE_TEST_QUERY_LOWER_FUNCTION_IR = "lower_function_ir";
constexpr std::string_view CACHE_TEST_QUERY_DIAGNOSTICS = "diagnostics";
constexpr auto CACHE_TEST_QUERY_SCHEDULE = std::to_array<std::string_view>({
    CACHE_TEST_QUERY_PROJECT_GRAPH,
    CACHE_TEST_QUERY_FILE_CONTENT,
    CACHE_TEST_QUERY_LEX_FILE,
    CACHE_TEST_QUERY_PARSE_FILE,
    CACHE_TEST_QUERY_MODULE_PART,
    CACHE_TEST_QUERY_MODULE_GRAPH,
    CACHE_TEST_QUERY_ITEM_LIST,
    CACHE_TEST_QUERY_MODULE_EXPORTS,
    CACHE_TEST_QUERY_MODULE_PACKAGE_EXPORTS,
    CACHE_TEST_QUERY_ITEM_SIGNATURE,
    CACHE_TEST_QUERY_GENERIC_TEMPLATE_SIGNATURE,
    CACHE_TEST_QUERY_GENERIC_INSTANCE_SIGNATURE,
    CACHE_TEST_QUERY_FUNCTION_BODY_SYNTAX,
    CACHE_TEST_QUERY_TYPE_CHECK_BODY,
    CACHE_TEST_QUERY_GENERIC_INSTANCE_BODY,
    CACHE_TEST_QUERY_LOWER_FUNCTION_IR,
    CACHE_TEST_QUERY_DIAGNOSTICS,
});
constexpr std::string_view CACHE_TEST_QUERY_DIFF_PROFILE_PHASE = "incremental_cache.query_diff";
constexpr std::string_view CACHE_TEST_QUERY_PLAN_PROFILE_PHASE = "incremental_cache.query_plan";
constexpr std::string_view CACHE_TEST_QUERY_PRUNING_PROFILE_PHASE = "incremental_cache.query_pruning";
constexpr std::string_view CACHE_TEST_QUERY_PROVIDER_EVAL_PROFILE_PHASE = "incremental_cache.query_provider_eval";
constexpr std::string_view CACHE_TEST_QUERY_PROVIDER_EVAL_MODE_FULL = "mode=full";
constexpr std::string_view CACHE_TEST_PROJECT_INPUTS_PROFILE_PHASE = "incremental_cache.project_inputs";
constexpr std::string_view CACHE_TEST_SOURCE_STAGE_REUSE_PROFILE_PHASE = "incremental_cache.source_stage_reuse";
constexpr std::string_view CACHE_TEST_PROJECT_INPUTS_REUSE_DETAIL = "result=reuse,changed_inputs=none";
constexpr std::string_view PROFILE_TEST_STAGE_FIELD = "\"stage\": {";
constexpr std::string_view PROFILE_TEST_PARENT_STAGE_FIELD = "\"parent_stage\": {";
constexpr std::string_view CACHE_TEST_SOURCE_STAGE_REUSE_SINGLE_GREEN_DETAIL =
    "result=reuse,reason=none,sources=1,queries=2,unchanged=2,missing=0,changed=0,malformed=0,source_failures=0";
constexpr std::string_view CACHE_TEST_SOURCE_STAGE_REUSE_SINGLE_CHANGED_DETAIL =
    "result=reject,reason=changed_query,sources=1,queries=2,unchanged=0,missing=0,changed=2,malformed=0,"
    "source_failures=0";
constexpr std::string_view CACHE_TEST_SOURCE_STAGE_REUSE_IMPORT_GREEN_DETAIL =
    "result=reuse,reason=none,sources=2,queries=4,unchanged=4,missing=0,changed=0,malformed=0,source_failures=0";
constexpr std::string_view CACHE_TEST_SOURCE_STAGE_REUSE_IMPORT_CHANGED_DETAIL =
    "result=reject,reason=changed_query,sources=2,queries=4,unchanged=2,missing=0,changed=2,malformed=0,"
    "source_failures=0";
constexpr std::string_view CACHE_TEST_SOURCE_STAGE_REUSE_MISSING_ROOT_DETAIL =
    "result=reject,reason=missing_root_source,sources=0,queries=0,unchanged=0,missing=0,changed=0,malformed=0,"
    "source_failures=0";
constexpr std::string_view CACHE_TEST_SOURCE_STAGE_REUSE_MISSING_QUERY_DETAIL =
    "result=reject,reason=missing_query,sources=1,queries=2,unchanged=0,missing=2,changed=0,malformed=0,"
    "source_failures=0";
constexpr std::string_view CACHE_TEST_SOURCE_STAGE_REUSE_SOURCE_FAILURE_DETAIL =
    "result=reject,reason=source_failure,sources=1,queries=0,unchanged=0,missing=0,changed=0,malformed=0,"
    "source_failures=1";
constexpr std::string_view CACHE_TEST_QUERY_DIFF_MISSING_DETAIL =
    "total=24,missing=24,unchanged=0,changed=0,malformed=0";
constexpr std::string_view CACHE_TEST_QUERY_DIFF_UNCHANGED_DETAIL =
    "total=24,missing=0,unchanged=24,changed=0,malformed=0";
constexpr std::string_view CACHE_TEST_QUERY_DIFF_CHANGED_DETAIL =
    "total=24,missing=0,unchanged=23,changed=1,malformed=0";
constexpr std::string_view CACHE_TEST_QUERY_PLAN_MISSING_DETAIL =
    "reusable=0,recompute_roots=24,propagated_recompute=0,recompute=24";
constexpr std::string_view CACHE_TEST_QUERY_PLAN_UNCHANGED_DETAIL =
    "reusable=24,recompute_roots=0,propagated_recompute=0,recompute=0";
constexpr std::string_view CACHE_TEST_QUERY_PLAN_CHANGED_DETAIL =
    "reusable=23,recompute_roots=1,propagated_recompute=0,recompute=1";
constexpr std::string_view CACHE_TEST_QUERY_PRUNING_NO_CACHE_DETAIL =
    "enabled=1,applied=0,reused=0,recomputed=24,reused_project_graphs=0,reused_file_contents=0,reused_lex_files=0,"
    "reused_parse_files=0,reused_module_parts=0,reused_module_graphs=0,reused_module_exports=0,reused_item_lists=0,"
    "reused_item_signatures=0,"
    "reused_function_body_syntaxes=0,reused_type_check_bodies=0,reused_generic_template_signatures=0,"
    "reused_generic_instance_signatures=0,reused_generic_instance_bodies=0,reused_lower_function_irs=0,"
    "reused_diagnostics=0,"
    "recomputed_project_graphs=1,recomputed_file_contents=1,recomputed_lex_files=1,recomputed_parse_files=1,"
    "recomputed_module_parts=1,"
    "recomputed_module_graphs=1,"
    "recomputed_module_exports=1,recomputed_item_lists=1,"
    "recomputed_item_signatures=2,recomputed_function_body_syntaxes=0,recomputed_type_check_bodies=0,"
    "recomputed_generic_template_signatures=1,recomputed_generic_instance_signatures=1,"
    "recomputed_generic_instance_bodies=0,"
    "recomputed_lower_function_irs=0,recomputed_diagnostics=12,fallback=no_cache";
constexpr std::string_view CACHE_TEST_QUERY_PRUNING_NO_CACHE_FALLBACK_FIELD = "fallback=no_cache";
constexpr std::string_view CACHE_TEST_QUERY_PRUNING_FALLBACK_MALFORMED_CACHE = "malformed_cache";
constexpr std::string_view CACHE_TEST_QUERY_PRUNING_FALLBACK_MALFORMED_QUERY_GRAPH = "malformed_query_graph";
constexpr std::string_view CACHE_TEST_QUERY_PRUNING_FALLBACK_MALFORMED_QUERY_IDENTITY = "malformed_query_identity";
constexpr std::string_view CACHE_TEST_QUERY_PRUNING_REUSE_ALL_DETAIL =
    "enabled=1,applied=1,reused=24,recomputed=0,reused_project_graphs=1,reused_file_contents=1,"
    "reused_lex_files=1,"
    "reused_parse_files=1,reused_module_parts=1,reused_module_graphs=1,reused_module_exports=1,reused_item_lists=1,"
    "reused_item_signatures=2,"
    "reused_function_body_syntaxes=0,reused_type_check_bodies=0,reused_generic_template_signatures=1,"
    "reused_generic_instance_signatures=1,reused_generic_instance_bodies=0,reused_lower_function_irs=0,"
    "reused_diagnostics=12,"
    "recomputed_project_graphs=0,recomputed_file_contents=0,recomputed_lex_files=0,recomputed_parse_files=0,"
    "recomputed_module_parts=0,"
    "recomputed_module_graphs=0,"
    "recomputed_module_exports=0,recomputed_item_lists=0,"
    "recomputed_item_signatures=0,recomputed_function_body_syntaxes=0,recomputed_type_check_bodies=0,"
    "recomputed_generic_template_signatures=0,recomputed_generic_instance_signatures=0,"
    "recomputed_generic_instance_bodies=0,"
    "recomputed_lower_function_irs=0,recomputed_diagnostics=0,fallback=none";
constexpr std::string_view CACHE_TEST_QUERY_PRUNING_CHANGED_DETAIL =
    "enabled=1,applied=1,reused=23,recomputed=1,reused_project_graphs=1,reused_file_contents=1,"
    "reused_lex_files=1,"
    "reused_parse_files=1,reused_module_parts=1,reused_module_graphs=1,reused_module_exports=1,reused_item_lists=1,"
    "reused_item_signatures=2,"
    "reused_function_body_syntaxes=0,reused_type_check_bodies=0,reused_generic_template_signatures=1,"
    "reused_generic_instance_signatures=0,reused_generic_instance_bodies=0,reused_lower_function_irs=0,"
    "reused_diagnostics=12,"
    "recomputed_project_graphs=0,recomputed_file_contents=0,recomputed_lex_files=0,recomputed_parse_files=0,"
    "recomputed_module_parts=0,"
    "recomputed_module_graphs=0,"
    "recomputed_module_exports=0,recomputed_item_lists=0,"
    "recomputed_item_signatures=0,recomputed_function_body_syntaxes=0,recomputed_type_check_bodies=0,"
    "recomputed_generic_template_signatures=0,recomputed_generic_instance_signatures=1,"
    "recomputed_generic_instance_bodies=0,"
    "recomputed_lower_function_irs=0,recomputed_diagnostics=0,fallback=none";
constexpr std::string_view CACHE_TEST_QUERY_PROVIDER_EVAL_REUSE_ALL_DETAIL =
    "mode=pruned,seeded=24,evaluated=0,seeded_project_graphs=1,seeded_file_contents=1,seeded_lex_files=1,"
    "seeded_parse_files=1,"
    "seeded_module_parts=1,seeded_module_graphs=1,seeded_module_exports=1,seeded_item_lists=1,seeded_item_signatures=2,"
    "seeded_function_body_syntaxes=0,seeded_type_check_bodies=0,seeded_generic_template_signatures=1,"
    "seeded_generic_instance_signatures=1,seeded_generic_instance_bodies=0,seeded_lower_function_irs=0,"
    "seeded_diagnostics=12,"
    "evaluated_project_graphs=0,evaluated_file_contents=0,evaluated_lex_files=0,evaluated_parse_files=0,"
    "evaluated_module_parts=0,"
    "evaluated_module_graphs=0,"
    "evaluated_module_exports=0,evaluated_item_lists=0,"
    "evaluated_item_signatures=0,evaluated_function_body_syntaxes=0,evaluated_type_check_bodies=0,"
    "evaluated_generic_template_signatures=0,evaluated_generic_instance_signatures=0,"
    "evaluated_generic_instance_bodies=0,"
    "evaluated_lower_function_irs=0,evaluated_diagnostics=0";
constexpr std::string_view CACHE_TEST_QUERY_PROVIDER_EVAL_CHANGED_DETAIL =
    "mode=pruned,seeded=23,evaluated=1,seeded_project_graphs=1,seeded_file_contents=1,seeded_lex_files=1,"
    "seeded_parse_files=1,"
    "seeded_module_parts=1,seeded_module_graphs=1,seeded_module_exports=1,seeded_item_lists=1,seeded_item_signatures=2,"
    "seeded_function_body_syntaxes=0,seeded_type_check_bodies=0,seeded_generic_template_signatures=1,"
    "seeded_generic_instance_signatures=0,seeded_generic_instance_bodies=0,seeded_lower_function_irs=0,"
    "seeded_diagnostics=12,"
    "evaluated_project_graphs=0,evaluated_file_contents=0,evaluated_lex_files=0,evaluated_parse_files=0,"
    "evaluated_module_parts=0,"
    "evaluated_module_graphs=0,"
    "evaluated_module_exports=0,evaluated_item_lists=0,"
    "evaluated_item_signatures=0,evaluated_function_body_syntaxes=0,evaluated_type_check_bodies=0,"
    "evaluated_generic_template_signatures=0,evaluated_generic_instance_signatures=1,"
    "evaluated_generic_instance_bodies=0,"
    "evaluated_lower_function_irs=0,evaluated_diagnostics=0";

class ScopedEnvironmentVariable final {
public:
    ScopedEnvironmentVariable(std::string name, const std::string& value) : name_(std::move(name))
    {
        const char* existing = std::getenv(this->name_.c_str());
        if (existing != nullptr) {
            this->old_value_ = existing;
            this->had_old_value_ = true;
        }
        ::setenv(this->name_.c_str(), value.c_str(), 1);
    }

    ScopedEnvironmentVariable(const ScopedEnvironmentVariable&) = delete;
    ScopedEnvironmentVariable& operator=(const ScopedEnvironmentVariable&) = delete;

    ~ScopedEnvironmentVariable()
    {
        if (this->had_old_value_) {
            ::setenv(this->name_.c_str(), this->old_value_.c_str(), 1);
        } else {
            ::unsetenv(this->name_.c_str());
        }
    }

private:
    std::string name_;
    std::string old_value_;
    bool had_old_value_ = false;
};

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

[[nodiscard]] driver::CompilerInvocation cache_test_header_invocation(
    const fs::path& root, const std::vector<fs::path>& import_paths)
{
    driver::CompilerInvocation invocation;
    invocation.input_path = root;
    invocation.emit_kind = driver::EmitKind::check;
    invocation.import_paths = import_paths;
    return invocation;
}

[[nodiscard]] fs::path cache_test_import_source_root(const fs::path& import_path)
{
    const fs::path canonical_root = project::canonical_or_absolute_project_path(import_path);
    return driver::package_source_root_for_import_root(canonical_root.string()).value_or(canonical_root);
}

[[nodiscard]] std::string cache_test_header_with_compiler_and_mode(const fs::path& root,
    const std::string_view compiler_version, const std::string_view mode,
    const std::vector<fs::path>& import_paths = {})
{
    const driver::CompilerInvocation invocation = cache_test_header_invocation(root, import_paths);
    const project::ProjectModel project_model = driver::project_model_from_invocation(invocation);
    std::string cache;
    cache += "aurex-incremental-cache-v1\n";
    cache += "schema\t2\n";
    cache += "compiler\t" + hex_encode_cache_test_field(compiler_version) + "\n";
    cache += "mode\t" + hex_encode_cache_test_field(mode) + "\n";
    cache += "root\t" + hex_encode_cache_test_field(project::canonical_or_absolute_project_path(root).string()) + "\n";
    cache += "project_identity\t" + hex_encode_cache_test_field(project::stable_serialize(project_model)) + "\n";
    cache += "package_root\t" + hex_encode_cache_test_field(project_model.package_root.string()) + "\n";
    cache += "source_root\t" + hex_encode_cache_test_field(project_model.source_root.string()) + "\n";
    cache += "target_config\t" + hex_encode_cache_test_field(project::stable_serialize(project_model.target)) + "\n";
    cache += "command_options\t" + hex_encode_cache_test_field(project::stable_serialize(project_model.command_options))
        + "\n";
    cache += "open_buffers\t0\n";
    cache += "package\t" + hex_encode_cache_test_field(driver::package_identity_for_invocation(invocation)) + "\n";
    cache += "import_paths\t" + std::to_string(import_paths.size()) + "\n";
    for (const fs::path& import_path : import_paths) {
        cache +=
            "import_path\t" + hex_encode_cache_test_field(cache_test_import_source_root(import_path).string()) + "\n";
    }
    cache += "import_packages\t" + std::to_string(import_paths.size()) + "\n";
    for (const fs::path& import_path : import_paths) {
        const fs::path canonical_root = project::canonical_or_absolute_project_path(import_path);
        cache += "import_package\t"
            + hex_encode_cache_test_field(driver::package_identity_for_import_root(canonical_root.string())) + "\n";
    }
    return cache;
}

[[nodiscard]] std::string cache_test_header(const fs::path& root, const std::vector<fs::path>& import_paths = {})
{
    return cache_test_header_with_compiler_and_mode(
        root, base::config::AUREX_VERSION_STRING, "semantic-ok", import_paths);
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
    if (kind == query::QueryKind::project_graph) {
        return CACHE_TEST_QUERY_PROJECT_GRAPH;
    }
    if (kind == query::QueryKind::file_content) {
        return CACHE_TEST_QUERY_FILE_CONTENT;
    }
    if (kind == query::QueryKind::lex_file) {
        return CACHE_TEST_QUERY_LEX_FILE;
    }
    if (kind == query::QueryKind::parse_file) {
        return CACHE_TEST_QUERY_PARSE_FILE;
    }
    if (kind == query::QueryKind::module_part) {
        return CACHE_TEST_QUERY_MODULE_PART;
    }
    if (kind == query::QueryKind::module_graph) {
        return CACHE_TEST_QUERY_MODULE_GRAPH;
    }
    if (kind == query::QueryKind::module_exports) {
        return CACHE_TEST_QUERY_MODULE_EXPORTS;
    }
    if (kind == query::QueryKind::module_package_exports) {
        return CACHE_TEST_QUERY_MODULE_PACKAGE_EXPORTS;
    }
    if (kind == query::QueryKind::item_list) {
        return CACHE_TEST_QUERY_ITEM_LIST;
    }
    if (kind == query::QueryKind::item_signature) {
        return CACHE_TEST_QUERY_ITEM_SIGNATURE;
    }
    if (kind == query::QueryKind::function_body_syntax) {
        return CACHE_TEST_QUERY_FUNCTION_BODY_SYNTAX;
    }
    if (kind == query::QueryKind::type_check_body) {
        return CACHE_TEST_QUERY_TYPE_CHECK_BODY;
    }
    if (kind == query::QueryKind::generic_template_signature) {
        return CACHE_TEST_QUERY_GENERIC_TEMPLATE_SIGNATURE;
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

[[nodiscard]] std::string cache_test_query_pruning_detail_with_fallback(const std::string_view fallback)
{
    std::string detail(CACHE_TEST_QUERY_PRUNING_NO_CACHE_DETAIL);
    const std::size_t fallback_pos = detail.rfind(CACHE_TEST_QUERY_PRUNING_NO_CACHE_FALLBACK_FIELD);
    if (fallback_pos == std::string::npos) {
        return detail;
    }
    detail.replace(
        fallback_pos, CACHE_TEST_QUERY_PRUNING_NO_CACHE_FALLBACK_FIELD.size(), "fallback=" + std::string(fallback));
    return detail;
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

[[nodiscard]] std::vector<std::string_view> cache_test_query_kinds(const std::string_view cache_text)
{
    std::vector<std::string_view> kinds;
    base::usize line_start = 0;
    while (line_start < cache_text.size()) {
        const base::usize line_end = cache_text.find('\n', line_start);
        const std::string_view line = line_end == std::string_view::npos
            ? cache_text.substr(line_start)
            : cache_text.substr(line_start, line_end - line_start);
        const std::vector<std::string_view> fields = split_cache_test_fields(line);
        if (fields.size() == CACHE_TEST_QUERY_FIELD_COUNT
            && fields[CACHE_TEST_QUERY_ROW_KIND_FIELD] == CACHE_TEST_QUERY_ROW_KIND) {
            kinds.push_back(fields[CACHE_TEST_QUERY_KIND_FIELD]);
        }
        if (line_end == std::string_view::npos) {
            break;
        }
        line_start = line_end + 1;
    }
    return kinds;
}

[[nodiscard]] std::optional<base::usize> first_cache_test_query_kind_index(
    const std::vector<std::string_view>& kinds, const std::string_view kind)
{
    for (base::usize index = 0; index < kinds.size(); ++index) {
        if (kinds[index] == kind) {
            return index;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<base::usize> last_cache_test_query_kind_index(
    const std::vector<std::string_view>& kinds, const std::string_view kind)
{
    for (base::usize reverse_index = kinds.size(); reverse_index > 0; --reverse_index) {
        const base::usize index = reverse_index - 1;
        if (kinds[index] == kind) {
            return index;
        }
    }
    return std::nullopt;
}

void expect_cache_query_kind_before(
    const std::string_view cache_text, const std::string_view before, const std::string_view after)
{
    const std::vector<std::string_view> kinds = cache_test_query_kinds(cache_text);
    const std::optional<base::usize> before_index = last_cache_test_query_kind_index(kinds, before);
    const std::optional<base::usize> after_index = first_cache_test_query_kind_index(kinds, after);
    ASSERT_TRUE(before_index.has_value()) << before;
    ASSERT_TRUE(after_index.has_value()) << after;
    EXPECT_LT(*before_index, *after_index) << before << " should be scheduled before " << after;
}

[[nodiscard]] std::optional<base::usize> cache_test_query_schedule_rank(const std::string_view kind)
{
    for (base::usize index = 0; index < CACHE_TEST_QUERY_SCHEDULE.size(); ++index) {
        if (CACHE_TEST_QUERY_SCHEDULE[index] == kind) {
            return index;
        }
    }
    return std::nullopt;
}

[[nodiscard]] bool cache_test_query_edge_dependency_kind_is_expected(
    const std::string_view dependent, const std::string_view dependency) noexcept
{
    if (dependent == CACHE_TEST_QUERY_LEX_FILE) {
        return dependency == CACHE_TEST_QUERY_FILE_CONTENT;
    }
    if (dependent == CACHE_TEST_QUERY_PARSE_FILE) {
        return dependency == CACHE_TEST_QUERY_LEX_FILE;
    }
    if (dependent == CACHE_TEST_QUERY_MODULE_PART) {
        return dependency == CACHE_TEST_QUERY_PARSE_FILE;
    }
    if (dependent == CACHE_TEST_QUERY_MODULE_GRAPH) {
        return dependency == CACHE_TEST_QUERY_PROJECT_GRAPH || dependency == CACHE_TEST_QUERY_MODULE_PART;
    }
    if (dependent == CACHE_TEST_QUERY_ITEM_LIST) {
        return dependency == CACHE_TEST_QUERY_MODULE_GRAPH;
    }
    if (dependent == CACHE_TEST_QUERY_MODULE_EXPORTS) {
        return dependency == CACHE_TEST_QUERY_ITEM_LIST || dependency == CACHE_TEST_QUERY_MODULE_EXPORTS;
    }
    if (dependent == CACHE_TEST_QUERY_MODULE_PACKAGE_EXPORTS) {
        return dependency == CACHE_TEST_QUERY_ITEM_LIST || dependency == CACHE_TEST_QUERY_MODULE_EXPORTS
            || dependency == CACHE_TEST_QUERY_MODULE_PACKAGE_EXPORTS;
    }
    if (dependent == CACHE_TEST_QUERY_ITEM_SIGNATURE) {
        return dependency == CACHE_TEST_QUERY_MODULE_EXPORTS;
    }
    if (dependent == CACHE_TEST_QUERY_GENERIC_TEMPLATE_SIGNATURE) {
        return dependency == CACHE_TEST_QUERY_ITEM_LIST;
    }
    if (dependent == CACHE_TEST_QUERY_GENERIC_INSTANCE_SIGNATURE) {
        return dependency == CACHE_TEST_QUERY_GENERIC_TEMPLATE_SIGNATURE;
    }
    if (dependent == CACHE_TEST_QUERY_TYPE_CHECK_BODY) {
        return dependency == CACHE_TEST_QUERY_FUNCTION_BODY_SYNTAX || dependency == CACHE_TEST_QUERY_ITEM_SIGNATURE;
    }
    if (dependent == CACHE_TEST_QUERY_GENERIC_INSTANCE_BODY) {
        return dependency == CACHE_TEST_QUERY_GENERIC_INSTANCE_SIGNATURE;
    }
    if (dependent == CACHE_TEST_QUERY_LOWER_FUNCTION_IR) {
        return dependency == CACHE_TEST_QUERY_TYPE_CHECK_BODY || dependency == CACHE_TEST_QUERY_GENERIC_INSTANCE_BODY;
    }
    if (dependent == CACHE_TEST_QUERY_DIAGNOSTICS) {
        return !dependency.empty() && dependency != CACHE_TEST_QUERY_DIAGNOSTICS;
    }
    return false;
}

void expect_cache_query_edges_follow_dependency_schedule(const std::string_view cache_text)
{
    base::usize edge_count = 0;
    base::usize line_start = 0;
    while (line_start < cache_text.size()) {
        const base::usize line_end = cache_text.find('\n', line_start);
        const std::string_view line = line_end == std::string_view::npos
            ? cache_text.substr(line_start)
            : cache_text.substr(line_start, line_end - line_start);
        const std::vector<std::string_view> fields = split_cache_test_fields(line);
        if (fields.size() == CACHE_TEST_QUERY_EDGE_FIELD_COUNT
            && fields[CACHE_TEST_QUERY_ROW_KIND_FIELD] == CACHE_TEST_QUERY_EDGE_ROW_KIND) {
            ++edge_count;
            const std::optional<base::usize> dependent_rank =
                cache_test_query_schedule_rank(fields[CACHE_TEST_QUERY_EDGE_DEPENDENT_KIND_FIELD]);
            const std::optional<base::usize> dependency_rank =
                cache_test_query_schedule_rank(fields[CACHE_TEST_QUERY_EDGE_DEPENDENCY_KIND_FIELD]);
            ASSERT_TRUE(dependent_rank.has_value()) << fields[CACHE_TEST_QUERY_EDGE_DEPENDENT_KIND_FIELD];
            ASSERT_TRUE(dependency_rank.has_value()) << fields[CACHE_TEST_QUERY_EDGE_DEPENDENCY_KIND_FIELD];
            EXPECT_LE(*dependency_rank, *dependent_rank)
                << fields[CACHE_TEST_QUERY_EDGE_DEPENDENCY_KIND_FIELD] << " should not be scheduled after "
                << fields[CACHE_TEST_QUERY_EDGE_DEPENDENT_KIND_FIELD];
            EXPECT_TRUE(
                cache_test_query_edge_dependency_kind_is_expected(fields[CACHE_TEST_QUERY_EDGE_DEPENDENT_KIND_FIELD],
                    fields[CACHE_TEST_QUERY_EDGE_DEPENDENCY_KIND_FIELD]))
                << fields[CACHE_TEST_QUERY_EDGE_DEPENDENT_KIND_FIELD] << " should not depend on "
                << fields[CACHE_TEST_QUERY_EDGE_DEPENDENCY_KIND_FIELD];
        }
        if (line_end == std::string_view::npos) {
            break;
        }
        line_start = line_end + 1;
    }
    ASSERT_GT(edge_count, static_cast<base::usize>(0));
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

[[nodiscard]] std::optional<CacheTestQueryResultFingerprint> first_cache_test_query_result(
    const std::string_view cache_text, const std::string_view query_kind)
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
            && fields[CACHE_TEST_QUERY_KIND_FIELD] == query_kind) {
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

[[nodiscard]] query::PackageKey cache_test_default_package() noexcept
{
    return driver::package_key_from_identity(std::string_view{});
}

template <base::usize PART_COUNT>
[[nodiscard]] query::ModuleKey cache_test_module_key(const std::array<std::string_view, PART_COUNT>& module_parts)
{
    return query::module_key_from_stable_id(cache_test_default_package(),
        sema::stable_module_id(std::span<const std::string_view>{module_parts.data(), module_parts.size()}));
}

[[nodiscard]] query::ModulePartKey cache_test_primary_module_part_key(
    const query::ModuleKey module, const query::PackageKey package, const fs::path& source)
{
    const query::FileKey file = query::file_key(package, source.string(), query::SourceRole::source);
    return query::module_part_key(module, file, query::ModulePartKind::primary, "<primary>");
}

[[nodiscard]] query::DefKey cache_test_def_key_from_stable_id(
    const sema::StableDefId& stable_id, const query::DefNamespace name_space, const query::DefKind kind)
{
    return query::def_key_from_stable_id(cache_test_default_package(), stable_id, name_space, kind);
}

template <base::usize PART_COUNT>
[[nodiscard]] std::string cache_test_encoded_module_key(const std::array<std::string_view, PART_COUNT>& module_parts)
{
    return hex_encode_cache_test_field(query::stable_serialize(cache_test_module_key(module_parts)));
}

template <base::usize PART_COUNT>
[[nodiscard]] std::string cache_test_encoded_module_key(
    const query::PackageKey package, const std::array<std::string_view, PART_COUNT>& module_parts)
{
    const query::ModuleKey module_key = query::module_key_from_stable_id(
        package, sema::stable_module_id(std::span<const std::string_view>{module_parts.data(), module_parts.size()}));
    return hex_encode_cache_test_field(query::stable_serialize(module_key));
}

template <base::usize PART_COUNT>
[[nodiscard]] std::optional<CacheTestQueryResultFingerprint> cache_test_module_query_result(
    const std::string_view cache_text, const std::string_view query_kind,
    const std::array<std::string_view, PART_COUNT>& module_parts)
{
    return cache_test_query_result(cache_text, query_kind, cache_test_encoded_module_key(module_parts));
}

[[nodiscard]] base::usize cache_test_query_edge_count(
    const std::string_view cache_text, const std::string_view dependent_kind, const std::string_view dependency_kind)
{
    base::usize count = 0;
    base::usize line_start = 0;
    while (line_start < cache_text.size()) {
        const base::usize line_end = cache_text.find('\n', line_start);
        const std::string_view line = line_end == std::string_view::npos
            ? cache_text.substr(line_start)
            : cache_text.substr(line_start, line_end - line_start);
        const std::vector<std::string_view> fields = split_cache_test_fields(line);
        if (fields.size() == CACHE_TEST_QUERY_EDGE_FIELD_COUNT
            && fields[CACHE_TEST_QUERY_ROW_KIND_FIELD] == CACHE_TEST_QUERY_EDGE_ROW_KIND
            && fields[CACHE_TEST_QUERY_EDGE_DEPENDENT_KIND_FIELD] == dependent_kind
            && fields[CACHE_TEST_QUERY_EDGE_DEPENDENCY_KIND_FIELD] == dependency_kind) {
            ++count;
        }
        if (line_end == std::string_view::npos) {
            break;
        }
        line_start = line_end + 1;
    }
    return count;
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

void expect_project_inputs_and_source_stage_profile_phases(
    const driver::CompilationProfiler& profiler, const std::string_view source_stage_detail)
{
    ASSERT_EQ(profiler.phases().size(), 2U);
    EXPECT_EQ(profiler.phases()[0].name, CACHE_TEST_PROJECT_INPUTS_PROFILE_PHASE);
    EXPECT_EQ(profiler.phases()[0].detail, CACHE_TEST_PROJECT_INPUTS_REUSE_DETAIL);
    EXPECT_EQ(profiler.phases()[1].name, CACHE_TEST_SOURCE_STAGE_REUSE_PROFILE_PHASE);
    EXPECT_EQ(profiler.phases()[1].detail, source_stage_detail);
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

[[nodiscard]] std::optional<std::string_view> profile_phase_object(
    const std::string_view profile_text, const std::string_view phase_name)
{
    std::string marker{"\"name\": \""};
    marker.append(phase_name.data(), phase_name.size());
    marker.push_back('"');

    const std::size_t phase_name_position = profile_text.find(marker);
    if (phase_name_position == std::string_view::npos) {
        return std::nullopt;
    }

    std::size_t object_begin = profile_text.rfind("    {", phase_name_position);
    if (object_begin == std::string_view::npos) {
        object_begin = phase_name_position;
    }

    const std::size_t object_end = profile_text.find("\n    }", phase_name_position);
    if (object_end == std::string_view::npos) {
        return profile_text.substr(object_begin);
    }
    return profile_text.substr(object_begin, object_end - object_begin);
}

void expect_profile_phase_stage_presence(
    const std::string_view profile_text, const std::string_view phase_name, const bool expected_stage)
{
    const std::optional<std::string_view> phase_object = profile_phase_object(profile_text, phase_name);
    ASSERT_TRUE(phase_object.has_value()) << std::string(phase_name);
    if (expected_stage) {
        EXPECT_NE(phase_object->find(PROFILE_TEST_STAGE_FIELD), std::string_view::npos) << std::string(phase_name);
        return;
    }
    EXPECT_EQ(phase_object->find(PROFILE_TEST_STAGE_FIELD), std::string_view::npos) << std::string(phase_name);
}

void expect_profile_phase_parent_stage(
    const std::string_view profile_text, const std::string_view phase_name, const std::string_view parent_profile)
{
    const std::optional<std::string_view> phase_object = profile_phase_object(profile_text, phase_name);
    ASSERT_TRUE(phase_object.has_value()) << std::string(phase_name);
    EXPECT_NE(phase_object->find(PROFILE_TEST_PARENT_STAGE_FIELD), std::string_view::npos) << std::string(phase_name);

    std::string parent_profile_field{"\"profile\": \""};
    parent_profile_field.append(parent_profile.data(), parent_profile.size());
    parent_profile_field.push_back('"');
    EXPECT_NE(phase_object->find(parent_profile_field), std::string_view::npos) << std::string(phase_name);
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
        "--package=demo.pkg",
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
    EXPECT_EQ(object_parse.invocation.package_identity, "demo.pkg");
    EXPECT_TRUE(object_parse.invocation.query_pruning_enabled);
    EXPECT_EQ(object_parse.invocation.profile_output_path, fs::path("build/hello.profile.json"));

    const std::vector<std::string_view> assembly_args{
        "aurexc",
        "-S",
        "examples/hello.ax",
    };
    const driver::CliParseResult assembly_parse = require_parse_cli(assembly_args);
    EXPECT_EQ(assembly_parse.invocation.emit_kind, driver::EmitKind::assembly);
    EXPECT_EQ(assembly_parse.invocation.output_path, fs::path("hello.s"));
    EXPECT_TRUE(assembly_parse.invocation.query_pruning_enabled);

    const std::vector<std::string_view> coarse_cache_args{
        "aurexc",
        "--incremental-cache",
        "build/hello.axic",
        "--no-query-pruning",
        "examples/hello.ax",
    };
    const driver::CliParseResult coarse_cache_parse = require_parse_cli(coarse_cache_args);
    EXPECT_FALSE(coarse_cache_parse.invocation.query_pruning_enabled);

    const std::vector<std::string_view> query_pruning_args{
        "aurexc",
        "--no-query-pruning",
        "--query-pruning",
        "examples/hello.ax",
    };
    const driver::CliParseResult query_pruning_parse = require_parse_cli(query_pruning_args);
    EXPECT_TRUE(query_pruning_parse.invocation.query_pruning_enabled);

    const std::vector<std::string_view> separate_emit_args{
        "aurexc",
        "--emit",
        "llvm-ir",
        "-fsyntax-only",
        "examples/hello.ax",
    };
    const driver::CliParseResult separate_emit_parse = require_parse_cli(separate_emit_args);
    EXPECT_EQ(separate_emit_parse.invocation.emit_kind, driver::EmitKind::check);

    const std::vector<std::string_view> lossless_emit_args{
        "aurexc",
        "--emit=lossless",
        "examples/hello.ax",
    };
    const driver::CliParseResult lossless_emit_parse = require_parse_cli(lossless_emit_args);
    EXPECT_EQ(lossless_emit_parse.invocation.emit_kind, driver::EmitKind::lossless);

    const std::vector<std::string_view> dump_lossless_args{
        "aurexc",
        "--dump-lossless",
        "examples/hello.ax",
    };
    const driver::CliParseResult dump_lossless_parse = require_parse_cli(dump_lossless_args);
    EXPECT_EQ(dump_lossless_parse.invocation.emit_kind, driver::EmitKind::lossless);

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
        const fs::path hello = source_root() / "examples" / "hello.ax";
        driver::CompilerInvocation invocation;
        invocation.input_path = hello;
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
                "\"stage\": {",
                "\"id\": \"module_parse\"",
                "\"input\": \"module token buffer\"",
                "\"output\": \"module AST\"",
                "\"diagnostic_ownership\": \"parser diagnostic sink\"",
                "\"cache_query_impact\": \"feeds module graph and query records\"",
                "\"name\": \"sema.analyze\"",
                "\"name\": \"incremental_cache.query_diff\"",
                "\"name\": \"incremental_cache.query_plan\"",
                "\"name\": \"incremental_cache.query_pruning\"",
                "total=",
                "missing=",
                "unchanged=",
                "changed=",
                "malformed=",
                "reusable=",
                "recompute_roots=",
                "propagated_recompute=",
                "recompute=",
                "fallback=no_cache",
                "\"rss_mib_after\"",
                "\"rss_delta_mib\"",
            });
        expect_profile_phase_stage_presence(profile_text, "module.parse", true);
        expect_profile_phase_stage_presence(profile_text, CACHE_TEST_QUERY_DIFF_PROFILE_PHASE, false);
        expect_profile_phase_stage_presence(profile_text, CACHE_TEST_QUERY_PLAN_PROFILE_PHASE, false);
        expect_profile_phase_stage_presence(profile_text, CACHE_TEST_QUERY_PRUNING_PROFILE_PHASE, false);
        expect_profile_phase_parent_stage(profile_text, CACHE_TEST_QUERY_DIFF_PROFILE_PHASE, "incremental_cache.write");
        expect_profile_phase_parent_stage(profile_text, CACHE_TEST_QUERY_PLAN_PROFILE_PHASE, "incremental_cache.write");
        expect_profile_phase_parent_stage(
            profile_text, CACHE_TEST_QUERY_PRUNING_PROFILE_PHASE, "incremental_cache.write");

        const auto run_profiled_emit_with = [&](driver::Compiler& selected_compiler, const driver::EmitKind emit_kind,
                                                const fs::path& profile_path) {
            driver::CompilerInvocation profiled_invocation;
            profiled_invocation.input_path = hello;
            profiled_invocation.emit_kind = emit_kind;
            profiled_invocation.profile_output_path = profile_path;
            testing::internal::CaptureStdout();
            const auto profiled_result = selected_compiler.run(profiled_invocation);
            static_cast<void>(testing::internal::GetCapturedStdout());
            if (!profiled_result) {
                ADD_FAILURE() << profiled_result.error().message;
                return std::string{};
            }
            return read_text(profile_path);
        };
        const auto run_profiled_emit = [&](const driver::EmitKind emit_kind, const fs::path& profile_path) {
            return run_profiled_emit_with(compiler, emit_kind, profile_path);
        };

        const std::string token_profile_text =
            run_profiled_emit(driver::EmitKind::tokens, tmp_root() / "hello.tokens.profile.json");
        expect_contains_all(token_profile_text,
            {
                "\"name\": \"source.read\"",
                "\"name\": \"tokens.lex\"",
                "\"name\": \"tokens.dump\"",
            });
        expect_not_contains(token_profile_text, "\"name\": \"module.read\"");

        const std::string lossless_profile_text =
            run_profiled_emit(driver::EmitKind::lossless, tmp_root() / "hello.lossless.profile.json");
        expect_contains_all(lossless_profile_text,
            {
                "\"name\": \"source.read\"",
                "\"name\": \"tokens.lex\"",
                "\"name\": \"lossless.dump\"",
            });
        expect_not_contains(lossless_profile_text, "\"name\": \"module.read\"");

        const std::string ast_profile_text =
            run_profiled_emit(driver::EmitKind::ast, tmp_root() / "hello.ast.profile.json");
        expect_contains_all(ast_profile_text,
            {
                "\"name\": \"module.read\"",
                "\"name\": \"module.lex\"",
                "\"name\": \"module.parse\"",
                "\"name\": \"ast.dump\"",
            });
        expect_not_contains(ast_profile_text, "\"name\": \"sema.analyze\"");

        const std::string checked_profile_text =
            run_profiled_emit(driver::EmitKind::checked, tmp_root() / "hello.checked.profile.json");
        expect_contains_all(checked_profile_text,
            {
                "\"name\": \"sema.analyze\"",
                "\"name\": \"checked.dump\"",
            });
        expect_not_contains(checked_profile_text, "\"name\": \"ir.lower\"");

        const std::string ir_profile_text =
            run_profiled_emit(driver::EmitKind::ir, tmp_root() / "hello.ir.profile.json");
        expect_contains_all(ir_profile_text,
            {
                "\"name\": \"ir.lower\"",
                "\"name\": \"ir.pass_pipeline\"",
                "\"name\": \"ir.dump\"",
            });
        expect_not_contains(ir_profile_text, "\"name\": \"llvm.emit_ir\"");

        driver::Compiler llvm_compiler(driver::llvm_backend_ir_emitter());
        const std::string llvm_profile_text =
            run_profiled_emit_with(llvm_compiler, driver::EmitKind::llvm_ir, tmp_root() / "hello.llvm.profile.json");
        expect_contains_all(llvm_profile_text,
            {
                "\"name\": \"ir.lower\"",
                "\"name\": \"ir.pass_pipeline\"",
                "\"name\": \"llvm.emit_ir\"",
                "\"name\": \"llvm_ir.dump\"",
            });

        const fs::path native_profile = tmp_root() / "hello.native.profile.json";
        driver::CompilerInvocation native_invocation;
        native_invocation.input_path = hello;
        native_invocation.emit_kind = driver::EmitKind::object;
        native_invocation.output_path = tmp_root() / "hello.o";
        native_invocation.profile_output_path = native_profile;
        const auto native_result = llvm_compiler.run(native_invocation);
        ASSERT_TRUE(native_result) << native_result.error().message;
        const std::string native_profile_text = read_text(native_profile);
        expect_contains_all(native_profile_text,
            {
                "\"name\": \"ir.lower\"",
                "\"name\": \"ir.pass_pipeline\"",
                "\"name\": \"llvm.emit_ir\"",
                "\"name\": \"llvm.write_temp\"",
                "\"name\": \"native.clang\"",
            });
        expect_not_contains(native_profile_text, "\"name\": \"llvm_ir.dump\"");
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

TEST_F(AurexIntegrationTest, CompilerPipelineStageRecordsCoverDriverProfileContract)
{
    const std::span<const driver::PipelineStageRecord> records = driver::pipeline_stage_records();
    ASSERT_EQ(records.size(), driver::PIPELINE_STAGE_RECORD_COUNT);

    for (std::size_t index = 0; index < records.size(); ++index) {
        const driver::PipelineStageRecord& record = records[index];
        EXPECT_EQ(static_cast<std::size_t>(record.id), index);
        EXPECT_FALSE(record.name.empty());
        EXPECT_FALSE(record.input.empty());
        EXPECT_FALSE(record.output.empty());
        EXPECT_FALSE(record.profile_name.empty());
        EXPECT_FALSE(record.diagnostic_ownership.empty());
        EXPECT_FALSE(record.cache_query_impact.empty());
        EXPECT_EQ(&driver::pipeline_stage_record(record.id), &record);
        EXPECT_EQ(driver::pipeline_stage_profile_name(record.id), record.profile_name);
        EXPECT_EQ(driver::pipeline_stage_record_for_profile_name(record.profile_name), &record);
        const driver::PipelineProfilePhaseClassification classification =
            driver::pipeline_profile_phase_classification(record.profile_name);
        EXPECT_EQ(classification.kind, driver::PipelineProfilePhaseKind::driver_stage);
        EXPECT_EQ(classification.profile_name, record.profile_name);
        EXPECT_EQ(classification.stage, &record);
        EXPECT_EQ(classification.subevent, nullptr);
        EXPECT_EQ(classification.parent_stage, nullptr);
        const driver::PipelineStageMetadata metadata = driver::pipeline_stage_metadata(record);
        EXPECT_EQ(metadata.id, record.name);
        EXPECT_EQ(metadata.profile_name, record.profile_name);
        EXPECT_EQ(metadata.input, record.input);
        EXPECT_EQ(metadata.output, record.output);
        EXPECT_EQ(metadata.diagnostic_ownership, record.diagnostic_ownership);
        EXPECT_EQ(metadata.cache_query_impact, record.cache_query_impact);
        EXPECT_EQ(driver::pipeline_stage_metadata(record.id).id, record.name);
        EXPECT_EQ(driver::pipeline_stage_metadata(record.id).profile_name, record.profile_name);
        for (std::size_t other_index = index + 1; other_index < records.size(); ++other_index) {
            EXPECT_NE(record.profile_name, records[other_index].profile_name);
        }
    }

    EXPECT_EQ(driver::pipeline_stage_profile_name(driver::PipelineStageId::incremental_cache_lookup),
        "incremental_cache.lookup");
    EXPECT_EQ(driver::pipeline_stage_profile_name(driver::PipelineStageId::source_read), "source.read");
    EXPECT_EQ(driver::pipeline_stage_profile_name(driver::PipelineStageId::tokens_lex), "tokens.lex");
    EXPECT_EQ(driver::pipeline_stage_profile_name(driver::PipelineStageId::tokens_dump), "tokens.dump");
    EXPECT_EQ(driver::pipeline_stage_profile_name(driver::PipelineStageId::lossless_dump), "lossless.dump");
    EXPECT_EQ(driver::pipeline_stage_profile_name(driver::PipelineStageId::module_read), "module.read");
    EXPECT_EQ(driver::pipeline_stage_profile_name(driver::PipelineStageId::module_lex), "module.lex");
    EXPECT_EQ(driver::pipeline_stage_profile_name(driver::PipelineStageId::module_parse), "module.parse");
    EXPECT_EQ(driver::pipeline_stage_profile_name(driver::PipelineStageId::module_append), "module.append");
    EXPECT_EQ(driver::pipeline_stage_profile_name(driver::PipelineStageId::ast_dump), "ast.dump");
    EXPECT_EQ(driver::pipeline_stage_profile_name(driver::PipelineStageId::modules_dump), "modules.dump");
    EXPECT_EQ(driver::pipeline_stage_profile_name(driver::PipelineStageId::sema_analyze), "sema.analyze");
    EXPECT_EQ(driver::pipeline_stage_profile_name(driver::PipelineStageId::incremental_cache_write),
        "incremental_cache.write");
    EXPECT_EQ(driver::pipeline_stage_profile_name(driver::PipelineStageId::checked_dump), "checked.dump");
    EXPECT_EQ(driver::pipeline_stage_profile_name(driver::PipelineStageId::ir_lower), "ir.lower");
    EXPECT_EQ(driver::pipeline_stage_profile_name(driver::PipelineStageId::ir_pass_pipeline), "ir.pass_pipeline");
    EXPECT_EQ(driver::pipeline_stage_profile_name(driver::PipelineStageId::ir_dump), "ir.dump");
    EXPECT_EQ(driver::pipeline_stage_profile_name(driver::PipelineStageId::llvm_emit_ir), "llvm.emit_ir");
    EXPECT_EQ(driver::pipeline_stage_profile_name(driver::PipelineStageId::llvm_ir_dump), "llvm_ir.dump");
    EXPECT_EQ(driver::pipeline_stage_profile_name(driver::PipelineStageId::llvm_write_temp), "llvm.write_temp");
    EXPECT_EQ(driver::pipeline_stage_profile_name(driver::PipelineStageId::native_clang), "native.clang");

    const driver::PipelineStageRecord& fallback =
        driver::pipeline_stage_record(static_cast<driver::PipelineStageId>(driver::PIPELINE_STAGE_RECORD_COUNT));
    EXPECT_EQ(&fallback, &records.front());
    EXPECT_EQ(
        driver::pipeline_stage_metadata(static_cast<driver::PipelineStageId>(driver::PIPELINE_STAGE_RECORD_COUNT)).id,
        records.front().name);
    EXPECT_EQ(driver::pipeline_stage_record_for_profile_name("unknown.phase"), nullptr);
    const driver::PipelineProfilePhaseClassification unknown_classification =
        driver::pipeline_profile_phase_classification("unknown.phase");
    EXPECT_EQ(unknown_classification.kind, driver::PipelineProfilePhaseKind::unknown);
    EXPECT_EQ(unknown_classification.profile_name, "unknown.phase");
    EXPECT_EQ(unknown_classification.stage, nullptr);
    EXPECT_EQ(unknown_classification.subevent, nullptr);
    EXPECT_EQ(unknown_classification.parent_stage, nullptr);

    const auto expect_diagnostic_owner_stages =
        [](const base::DiagnosticCategory category,
            const std::initializer_list<driver::PipelineStageId> expected_stages) {
            const std::span<const driver::PipelineStageId> actual_stages =
                driver::pipeline_stage_ids_for_diagnostic_category(category);
            ASSERT_EQ(actual_stages.size(), expected_stages.size());

            std::size_t index = 0;
            for (const driver::PipelineStageId expected_stage : expected_stages) {
                ASSERT_LT(index, actual_stages.size());
                EXPECT_EQ(actual_stages[index], expected_stage);
                const driver::PipelineStageRecord& owner = driver::pipeline_stage_record(actual_stages[index]);
                EXPECT_NE(owner.diagnostic_ownership, DRIVER_TEST_NO_DIAGNOSTIC_OWNER);
                index += 1;
            }
        };
    expect_diagnostic_owner_stages(base::DiagnosticCategory::general, {});
    expect_diagnostic_owner_stages(
        base::DiagnosticCategory::lexer, {driver::PipelineStageId::tokens_lex, driver::PipelineStageId::module_lex});
    expect_diagnostic_owner_stages(base::DiagnosticCategory::parser, {driver::PipelineStageId::module_parse});
    expect_diagnostic_owner_stages(base::DiagnosticCategory::semantic, {driver::PipelineStageId::sema_analyze});
    expect_diagnostic_owner_stages(base::DiagnosticCategory::type, {driver::PipelineStageId::sema_analyze});
    expect_diagnostic_owner_stages(base::DiagnosticCategory::name_resolution, {driver::PipelineStageId::sema_analyze});
    expect_diagnostic_owner_stages(base::DiagnosticCategory::visibility, {driver::PipelineStageId::sema_analyze});
    expect_diagnostic_owner_stages(base::DiagnosticCategory::pattern, {driver::PipelineStageId::sema_analyze});
    expect_diagnostic_owner_stages(base::DiagnosticCategory::safety, {driver::PipelineStageId::sema_analyze});
    expect_diagnostic_owner_stages(base::DiagnosticCategory::unsupported, {driver::PipelineStageId::sema_analyze});
    expect_diagnostic_owner_stages(base::DiagnosticCategory::capability, {driver::PipelineStageId::sema_analyze});
    expect_diagnostic_owner_stages(base::DiagnosticCategory::module, {driver::PipelineStageId::module_append});
    expect_diagnostic_owner_stages(base::DiagnosticCategory::internal, {driver::PipelineStageId::sema_analyze});
    expect_diagnostic_owner_stages(
        static_cast<base::DiagnosticCategory>(DRIVER_TEST_UNKNOWN_DIAGNOSTIC_CATEGORY_VALUE), {});

    const std::span<const driver::PipelineProfileSubeventRecord> subevents =
        driver::pipeline_profile_subevent_records();
    ASSERT_EQ(subevents.size(), driver::PIPELINE_PROFILE_SUBEVENT_RECORD_COUNT);
    for (std::size_t index = 0; index < subevents.size(); ++index) {
        const driver::PipelineProfileSubeventRecord& record = subevents[index];
        EXPECT_EQ(static_cast<std::size_t>(record.id), index);
        EXPECT_FALSE(record.profile_name.empty());
        EXPECT_EQ(driver::pipeline_stage_record_for_profile_name(record.profile_name), nullptr);
        EXPECT_EQ(&driver::pipeline_profile_subevent_record(record.id), &record);
        EXPECT_EQ(driver::pipeline_profile_subevent_profile_name(record.id), record.profile_name);
        EXPECT_EQ(driver::pipeline_profile_subevent_record_for_profile_name(record.profile_name), &record);
        EXPECT_LT(static_cast<std::size_t>(record.parent_stage), records.size());
        const driver::PipelineStageRecord& parent = driver::pipeline_stage_record(record.parent_stage);
        EXPECT_EQ(parent.id, record.parent_stage);
        const driver::PipelineProfilePhaseClassification classification =
            driver::pipeline_profile_phase_classification(record.profile_name);
        EXPECT_EQ(classification.kind, driver::PipelineProfilePhaseKind::profile_subevent);
        EXPECT_EQ(classification.profile_name, record.profile_name);
        EXPECT_EQ(classification.stage, nullptr);
        EXPECT_EQ(classification.subevent, &record);
        EXPECT_EQ(classification.parent_stage, &parent);
        for (std::size_t other_index = index + 1; other_index < subevents.size(); ++other_index) {
            EXPECT_NE(record.profile_name, subevents[other_index].profile_name);
        }
    }
    const driver::PipelineProfileSubeventRecord* const source_stage_reuse_subevent =
        driver::pipeline_profile_subevent_record_for_profile_name(CACHE_TEST_SOURCE_STAGE_REUSE_PROFILE_PHASE);
    ASSERT_NE(source_stage_reuse_subevent, nullptr);
    EXPECT_EQ(source_stage_reuse_subevent->parent_stage, driver::PipelineStageId::incremental_cache_lookup);

    const driver::PipelineProfileSubeventRecord* const project_inputs_subevent =
        driver::pipeline_profile_subevent_record_for_profile_name(CACHE_TEST_PROJECT_INPUTS_PROFILE_PHASE);
    ASSERT_NE(project_inputs_subevent, nullptr);
    EXPECT_EQ(project_inputs_subevent->parent_stage, driver::PipelineStageId::incremental_cache_lookup);

    const driver::PipelineProfileSubeventRecord* const query_diff_subevent =
        driver::pipeline_profile_subevent_record_for_profile_name(CACHE_TEST_QUERY_DIFF_PROFILE_PHASE);
    ASSERT_NE(query_diff_subevent, nullptr);
    EXPECT_EQ(query_diff_subevent->parent_stage, driver::PipelineStageId::incremental_cache_write);

    const driver::PipelineProfileSubeventRecord* const query_plan_subevent =
        driver::pipeline_profile_subevent_record_for_profile_name(CACHE_TEST_QUERY_PLAN_PROFILE_PHASE);
    ASSERT_NE(query_plan_subevent, nullptr);
    EXPECT_EQ(query_plan_subevent->parent_stage, driver::PipelineStageId::incremental_cache_write);

    const driver::PipelineProfileSubeventRecord* const query_pruning_subevent =
        driver::pipeline_profile_subevent_record_for_profile_name(CACHE_TEST_QUERY_PRUNING_PROFILE_PHASE);
    ASSERT_NE(query_pruning_subevent, nullptr);
    EXPECT_EQ(query_pruning_subevent->parent_stage, driver::PipelineStageId::incremental_cache_write);

    const driver::PipelineProfileSubeventRecord* const query_provider_eval_subevent =
        driver::pipeline_profile_subevent_record_for_profile_name(CACHE_TEST_QUERY_PROVIDER_EVAL_PROFILE_PHASE);
    ASSERT_NE(query_provider_eval_subevent, nullptr);
    EXPECT_EQ(query_provider_eval_subevent->parent_stage, driver::PipelineStageId::incremental_cache_write);
    const driver::PipelineProfileSubeventRecord& fallback_subevent = driver::pipeline_profile_subevent_record(
        static_cast<driver::PipelineProfileSubeventId>(driver::PIPELINE_PROFILE_SUBEVENT_RECORD_COUNT));
    EXPECT_EQ(&fallback_subevent, &subevents.front());
    EXPECT_EQ(driver::pipeline_profile_subevent_profile_name(
                  static_cast<driver::PipelineProfileSubeventId>(driver::PIPELINE_PROFILE_SUBEVENT_RECORD_COUNT)),
        subevents.front().profile_name);
    EXPECT_EQ(driver::pipeline_profile_subevent_record_for_profile_name("unknown.phase"), nullptr);
    EXPECT_EQ(driver::pipeline_profile_phase_classification(CACHE_TEST_SOURCE_STAGE_REUSE_PROFILE_PHASE).kind,
        driver::PipelineProfilePhaseKind::profile_subevent);
    EXPECT_EQ(driver::pipeline_profile_phase_classification(CACHE_TEST_PROJECT_INPUTS_PROFILE_PHASE).kind,
        driver::PipelineProfilePhaseKind::profile_subevent);
}

TEST_F(AurexIntegrationTest, CliIncrementalCacheUsesQueryKeyPruningByDefault)
{
    driver::clear_file_cache();

    const fs::path cache_dir = tmp_root() / "cli-query-key-default-cache";
    fs::create_directories(cache_dir);
    const fs::path source = cache_dir / "main.ax";
    const fs::path cache = cache_dir / "main.axic";
    const fs::path initial_profile = cache_dir / "initial.profile.json";
    const fs::path pruned_profile = cache_dir / "pruned.profile.json";

    const auto write_source = [&](const std::string_view text) {
        std::ofstream out(source, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << text;
    };

    write_source(DRIVER_INCREMENTAL_CACHE_FIRST_SOURCE);
    require_success(aurexc() + " --check --incremental-cache " + q(cache) + " --profile-output " + q(initial_profile)
        + " " + q(source));
    const std::string initial_profile_text = read_text(initial_profile);
    expect_contains_all(initial_profile_text,
        {
            "\"name\": \"incremental_cache.query_diff\"",
            "\"name\": \"incremental_cache.query_plan\"",
            "\"name\": \"incremental_cache.query_pruning\"",
            "enabled=1",
            "fallback=no_cache",
        });
    expect_not_contains(initial_profile_text, "\"name\": \"incremental_cache.query_provider_eval\"");

    write_source(DRIVER_INCREMENTAL_CACHE_SECOND_SOURCE);
    require_success(aurexc() + " --check --incremental-cache " + q(cache) + " --profile-output " + q(pruned_profile)
        + " " + q(source));
    const std::string pruned_profile_text = read_text(pruned_profile);
    expect_contains_all(pruned_profile_text,
        {
            "\"name\": \"incremental_cache.lookup\"",
            "\"name\": \"incremental_cache.source_stage_reuse\"",
            "\"name\": \"incremental_cache.query_diff\"",
            "\"name\": \"incremental_cache.query_plan\"",
            "\"name\": \"incremental_cache.query_pruning\"",
            "\"name\": \"incremental_cache.query_provider_eval\"",
            "reason=changed_query",
            "enabled=1",
            "applied=1",
            "fallback=none",
            "mode=pruned",
        });

    driver::clear_file_cache();
}

TEST_F(AurexIntegrationTest, CliIncrementalCacheProfilesProviderEvalWhenPrunedReplayFallsBack)
{
    driver::clear_file_cache();

    const fs::path cache_dir = tmp_root() / "cli-query-provider-eval-fallback-cache";
    fs::create_directories(cache_dir);
    const fs::path source = cache_dir / "main.ax";
    const fs::path cache = cache_dir / "main.axic";
    const fs::path profile = cache_dir / "checked-profile.json";

    {
        std::ofstream out(source, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << DRIVER_INCREMENTAL_CACHE_FIRST_SOURCE;
    }

    require_success(aurexc() + " --check --incremental-cache " + q(cache) + " " + q(source));
    require_success(aurexc() + " --emit=checked --incremental-cache " + q(cache) + " --profile-output " + q(profile)
        + " " + q(source));

    const std::string profile_text = read_text(profile);
    expect_contains_all(profile_text,
        {
            "\"name\": \"incremental_cache.query_pruning\"",
            "\"name\": \"incremental_cache.query_provider_eval\"",
            "applied=1",
            CACHE_TEST_QUERY_PROVIDER_EVAL_MODE_FULL,
        });

    driver::clear_file_cache();
}

TEST_F(AurexIntegrationTest, CliNoQueryPruningUsesSourceFingerprintOnlyCompatibilityPath)
{
    driver::clear_file_cache();

    const fs::path cache_dir = tmp_root() / "cli-no-query-pruning-cache";
    fs::create_directories(cache_dir);
    const fs::path source = cache_dir / "main.ax";
    const fs::path cache = cache_dir / "main.axic";
    const fs::path profile = cache_dir / "source-fingerprint-only.profile.json";

    const auto write_source = [&](const std::string_view text) {
        std::ofstream out(source, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << text;
    };

    write_source(DRIVER_INCREMENTAL_CACHE_FIRST_SOURCE);
    require_success(aurexc() + " --check --incremental-cache " + q(cache) + " " + q(source));

    write_source(DRIVER_INCREMENTAL_CACHE_SECOND_SOURCE);
    require_success(aurexc() + " --check --incremental-cache " + q(cache) + " --no-query-pruning --profile-output "
        + q(profile) + " " + q(source));
    const std::string profile_text = read_text(profile);
    expect_contains_all(profile_text,
        {
            "\"name\": \"incremental_cache.lookup\"",
            "\"name\": \"incremental_cache.query_diff\"",
            "\"name\": \"incremental_cache.query_plan\"",
        });
    expect_not_contains(profile_text, "\"name\": \"incremental_cache.source_stage_reuse\"");
    expect_not_contains(profile_text, "\"name\": \"incremental_cache.query_pruning\"");
    expect_not_contains(profile_text, "\"name\": \"incremental_cache.query_provider_eval\"");
    expect_not_contains(profile_text, "mode=pruned");

    driver::clear_file_cache();
}

TEST_F(AurexIntegrationTest, CompilerProfileCoversJsonAndErrorPaths)
{
    {
        driver::CompilationProfiler disabled;
        EXPECT_FALSE(disabled.enabled());
        disabled.record("ignored", std::chrono::milliseconds(1));
        disabled.record(driver::PipelineStageId::source_read, std::chrono::milliseconds(1));
        disabled.record(driver::PipelineProfileSubeventId::incremental_cache_query_plan, std::chrono::milliseconds(1));
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
        profiler.record(driver::PipelineStageId::module_parse, "root", std::chrono::milliseconds(1));
        profiler.record(driver::PipelineStageId::sema_analyze, std::chrono::milliseconds(1));
        profiler.record(
            driver::PipelineProfileSubeventId::incremental_cache_query_plan, "plan", std::chrono::milliseconds(1));
        ASSERT_EQ(profiler.phases().size(), 3U);
        EXPECT_EQ(
            profiler.phases()[0].name, driver::pipeline_stage_profile_name(driver::PipelineStageId::module_parse));
        EXPECT_EQ(profiler.phases()[0].detail, "root");
        EXPECT_EQ(
            profiler.phases()[1].name, driver::pipeline_stage_profile_name(driver::PipelineStageId::sema_analyze));
        EXPECT_TRUE(profiler.phases()[1].detail.empty());
        EXPECT_EQ(profiler.phases()[2].name,
            driver::pipeline_profile_subevent_profile_name(
                driver::PipelineProfileSubeventId::incremental_cache_query_plan));
        EXPECT_EQ(profiler.phases()[2].detail, "plan");

        const fs::path profile = tmp_root() / "typed-stage.profile.json";
        const auto result = profiler.write_json(profile);
        ASSERT_TRUE(result) << result.error().message;
        const std::string profile_text = read_text(profile);
        expect_profile_phase_stage_presence(profile_text, "module.parse", true);
        expect_profile_phase_parent_stage(profile_text, CACHE_TEST_QUERY_PLAN_PROFILE_PHASE, "incremental_cache.write");
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

TEST(CoreUnit, DiagnosticRendererEmitsStructuredJsonAndTextChildren)
{
    base::SourceManager sources;
    const base::SourceId source_id = sources.add_source("structured.ax", "fn main() {\n  let value = true;\n}\n");
    const base::SourceRange primary_range{source_id, 14U, 19U};
    const base::SourceRange secondary_range{source_id, 22U, 26U};

    base::DiagnosticSink diagnostics;
    diagnostics.push(base::Diagnostic{
        base::Severity::error,
        primary_range,
        "structured diagnostic",
        base::DiagnosticCategory::type,
        base::DiagnosticCode::semantic_type_mismatch,
        {
            base::primary_diagnostic_label(primary_range, "declared here"),
            base::secondary_diagnostic_label(secondary_range, "related value"),
        },
        {
            base::diagnostic_note(secondary_range, "note child", base::DiagnosticCategory::semantic,
                base::DiagnosticCode::semantic_error),
            base::diagnostic_help(primary_range, "help child"),
        },
    });

    std::ostringstream json;
    driver::render_diagnostics(json, sources, diagnostics, driver::DiagnosticOutputFormat::json);
    expect_contains_all(json.str(),
        {
            "\"format\": \"aurex-diagnostics-v1\"",
            "\"message\": \"structured diagnostic\"",
            "\"labels\": [",
            "\"style\": \"primary\"",
            "\"style\": \"secondary\"",
            "\"message\": \"declared here\"",
            "\"message\": \"related value\"",
            "\"children\": [",
            "\"severity\": \"note\"",
            "\"severity\": \"help\"",
            "\"message\": \"note child\"",
            "\"message\": \"help child\"",
        });

    std::ostringstream text;
    driver::render_diagnostics(text, sources, diagnostics, driver::DiagnosticOutputFormat::text);
    expect_contains_all(text.str(),
        {
            "structured.ax:2:3: error: structured diagnostic",
            "structured.ax:2:11: note: note child",
            "structured.ax:2:3: help: help child",
        });
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
            "--emit=lossless",
            "--dump-modules",
            "--dump-lossless",
            "--incremental-cache",
            "--package",
            "--query-pruning",
            "--no-query-pruning",
            "--diagnostics",
            "--opt-level",
        });

    const fs::path hello = source_root() / "examples" / "hello.ax";
    require_success(aurexc() + " --check " + q(hello));
    require_success(aurexc() + " --emit=check " + q(hello));
    require_success(aurexc() + " --emit=typed " + q(hello));

    const fs::path lossless_probe = tmp_root() / "lossless_cli.ax";
    {
        std::ofstream out(lossless_probe);
        out << "module lossless_cli;\n"
               "// keep me\n"
               "fn main() -> i32 { /* block */ return 0; }\n";
    }
    const std::string lossless = require_success(aurexc() + " --dump-lossless " + q(lossless_probe)).output;
    expect_contains_all(lossless,
        {
            "source_file",
            "module_decl",
            "function_decl",
            "paren_group",
            "block",
            "whitespace",
            "line_comment `// keep me`",
            "block_comment `/* block */`",
            "kw_module `module`",
        });

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
        invocation.output_path = tmp_root() / "missing_backend.o";
        driver::Compiler compiler;
        const auto result = compiler.run(invocation);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code, base::ErrorCode::codegen_error);
        expect_contains(result.error().message, "LLVM backend is unavailable");
    }

    {
        const fs::path blocked_tmp = tmp_root() / "native-temp-parent-is-file";
        {
            std::ofstream out(blocked_tmp, std::ios::binary);
            out << "not a directory";
        }
        ASSERT_TRUE(fs::is_regular_file(blocked_tmp));

        const ScopedEnvironmentVariable tmpdir_guard("TMPDIR", blocked_tmp.string());
        driver::CompilerInvocation invocation;
        invocation.input_path = source_root() / "examples" / "hello.ax";
        invocation.emit_kind = driver::EmitKind::object;
        invocation.output_path = tmp_root() / "unwritable_temp.o";
        driver::Compiler compiler(driver::llvm_backend_ir_emitter());
        const auto result = compiler.run(invocation);

        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code, base::ErrorCode::io_error);
        expect_contains(result.error().message, "failed to open output file");
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

TEST_F(AurexIntegrationTest, IncrementalCacheReportsWriteOpenFailure)
{
    constexpr std::size_t DRIVER_INCREMENTAL_CACHE_TOO_LONG_FILE_NAME_LENGTH = 300;
    constexpr char DRIVER_INCREMENTAL_CACHE_TOO_LONG_FILE_NAME_CHAR = 'x';

    driver::clear_file_cache();

    const fs::path cache_dir = tmp_root() / "incremental-cache-write-open";
    fs::create_directories(cache_dir);

    driver::CompilerInvocation invocation;
    invocation.incremental_cache_path = cache_dir
        / std::string(
            DRIVER_INCREMENTAL_CACHE_TOO_LONG_FILE_NAME_LENGTH, DRIVER_INCREMENTAL_CACHE_TOO_LONG_FILE_NAME_CHAR);

    base::SourceManager sources;
    const std::array<driver::ModuleRecord, 0> modules{};
    const sema::CheckedModule checked;
    const auto result = driver::write_incremental_cache(invocation, sources, modules, checked);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, base::ErrorCode::io_error);
}

TEST_F(AurexIntegrationTest, IncrementalCachePipelineReportsWriteFailureAtEmitStopPoints)
{
    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_PIPELINE_FAILURE_SOURCE =
        "module incremental_cache_pipeline_failure;\n"
        "fn main() -> i32 { let value: i32 = 7; return value; }\n";
    constexpr std::size_t DRIVER_INCREMENTAL_CACHE_PIPELINE_FAILURE_LONG_FILE_NAME_LENGTH = 300;
    constexpr char DRIVER_INCREMENTAL_CACHE_PIPELINE_FAILURE_LONG_FILE_NAME_CHAR = 'x';

    struct CacheFailureEmitCase {
        driver::EmitKind emit_kind;
        bool requires_llvm_backend = false;
        bool requires_output_path = false;
    };

    const std::array<CacheFailureEmitCase, 6> cases{{
        {driver::EmitKind::check, false, false},
        {driver::EmitKind::typed, false, false},
        {driver::EmitKind::checked, false, false},
        {driver::EmitKind::ir, false, false},
        {driver::EmitKind::llvm_ir, true, false},
        {driver::EmitKind::object, true, true},
    }};

    driver::clear_file_cache();

    const fs::path cache_dir = tmp_root() / "incremental-cache-pipeline-write-failure";
    fs::create_directories(cache_dir);
    const fs::path source = cache_dir / "main.ax";
    {
        std::ofstream out(source, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << DRIVER_INCREMENTAL_CACHE_PIPELINE_FAILURE_SOURCE;
    }

    for (base::usize index = 0; index < cases.size(); ++index) {
        const CacheFailureEmitCase test_case = cases[index];
        driver::CompilerInvocation invocation;
        invocation.input_path = source;
        invocation.emit_kind = test_case.emit_kind;
        invocation.incremental_cache_path = cache_dir
            / (std::string(DRIVER_INCREMENTAL_CACHE_PIPELINE_FAILURE_LONG_FILE_NAME_LENGTH,
                   DRIVER_INCREMENTAL_CACHE_PIPELINE_FAILURE_LONG_FILE_NAME_CHAR)
                + std::to_string(index));
        if (test_case.requires_output_path) {
            invocation.output_path = cache_dir / ("out" + std::to_string(index) + ".o");
        }

        testing::internal::CaptureStdout();
        base::Result<void> result = base::Result<void>::ok();
        if (test_case.requires_llvm_backend) {
            driver::Compiler compiler(driver::llvm_backend_ir_emitter());
            result = compiler.run(invocation);
        } else {
            driver::Compiler compiler;
            result = compiler.run(invocation);
        }
        static_cast<void>(testing::internal::GetCapturedStdout());
        ASSERT_FALSE(result) << index;
        EXPECT_EQ(result.error().code, base::ErrorCode::io_error) << index;
    }

    driver::clear_file_cache();
}

TEST_F(AurexIntegrationTest, IncrementalCacheWritesQueryRowsInDependencyScheduleOrder)
{
    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_QUERY_SCHEDULE_SOURCE =
        "module incremental_cache_query_schedule;\n"
        "fn id[T](value: T) -> T { return value; }\n"
        "fn main() -> i32 { return id[i32](7); }\n";

    driver::clear_file_cache();

    const fs::path cache_dir = tmp_root() / "incremental-cache-query-schedule";
    fs::create_directories(cache_dir);
    const fs::path source = cache_dir / "main.ax";
    const fs::path cache = cache_dir / "main.axic";
    {
        std::ofstream out(source, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << DRIVER_INCREMENTAL_CACHE_QUERY_SCHEDULE_SOURCE;
    }

    driver::CompilerInvocation invocation;
    invocation.input_path = source;
    invocation.emit_kind = driver::EmitKind::ir;
    invocation.incremental_cache_path = cache;

    driver::Compiler compiler;
    testing::internal::CaptureStdout();
    auto first = compiler.run(invocation);
    static_cast<void>(testing::internal::GetCapturedStdout());
    ASSERT_TRUE(first) << first.error().message;

    const std::string cache_text = read_text(cache);
    expect_cache_query_kind_before(cache_text, CACHE_TEST_QUERY_PROJECT_GRAPH, CACHE_TEST_QUERY_MODULE_GRAPH);
    expect_cache_query_kind_before(cache_text, CACHE_TEST_QUERY_FILE_CONTENT, CACHE_TEST_QUERY_LEX_FILE);
    expect_cache_query_kind_before(cache_text, CACHE_TEST_QUERY_LEX_FILE, CACHE_TEST_QUERY_PARSE_FILE);
    expect_cache_query_kind_before(cache_text, CACHE_TEST_QUERY_PARSE_FILE, CACHE_TEST_QUERY_MODULE_GRAPH);
    expect_cache_query_kind_before(cache_text, CACHE_TEST_QUERY_MODULE_GRAPH, CACHE_TEST_QUERY_ITEM_LIST);
    expect_cache_query_kind_before(cache_text, CACHE_TEST_QUERY_ITEM_LIST, CACHE_TEST_QUERY_MODULE_EXPORTS);
    expect_cache_query_kind_before(cache_text, CACHE_TEST_QUERY_MODULE_EXPORTS, CACHE_TEST_QUERY_ITEM_SIGNATURE);
    expect_cache_query_kind_before(
        cache_text, CACHE_TEST_QUERY_ITEM_SIGNATURE, CACHE_TEST_QUERY_GENERIC_TEMPLATE_SIGNATURE);
    expect_cache_query_kind_before(cache_text, CACHE_TEST_QUERY_ITEM_LIST, CACHE_TEST_QUERY_GENERIC_TEMPLATE_SIGNATURE);
    expect_cache_query_kind_before(
        cache_text, CACHE_TEST_QUERY_GENERIC_TEMPLATE_SIGNATURE, CACHE_TEST_QUERY_GENERIC_INSTANCE_SIGNATURE);
    expect_cache_query_kind_before(
        cache_text, CACHE_TEST_QUERY_GENERIC_INSTANCE_SIGNATURE, CACHE_TEST_QUERY_FUNCTION_BODY_SYNTAX);
    expect_cache_query_kind_before(cache_text, CACHE_TEST_QUERY_FUNCTION_BODY_SYNTAX, CACHE_TEST_QUERY_TYPE_CHECK_BODY);
    expect_cache_query_kind_before(cache_text, CACHE_TEST_QUERY_ITEM_SIGNATURE, CACHE_TEST_QUERY_TYPE_CHECK_BODY);
    expect_cache_query_kind_before(
        cache_text, CACHE_TEST_QUERY_TYPE_CHECK_BODY, CACHE_TEST_QUERY_GENERIC_INSTANCE_BODY);
    expect_cache_query_kind_before(
        cache_text, CACHE_TEST_QUERY_GENERIC_INSTANCE_BODY, CACHE_TEST_QUERY_LOWER_FUNCTION_IR);
    expect_cache_query_kind_before(cache_text, CACHE_TEST_QUERY_TYPE_CHECK_BODY, CACHE_TEST_QUERY_LOWER_FUNCTION_IR);
    expect_cache_query_kind_before(cache_text, CACHE_TEST_QUERY_LOWER_FUNCTION_IR, CACHE_TEST_QUERY_DIAGNOSTICS);
    expect_cache_query_edges_follow_dependency_schedule(cache_text);

    driver::clear_file_cache();
}

TEST_F(AurexIntegrationTest, IncrementalCacheQueryPruningReusesCommentOnlySourceChangesByDefault)
{
    driver::clear_file_cache();

    const fs::path cache_dir = tmp_root() / "incremental-cache-source-stage-green";
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

    write_source(DRIVER_INCREMENTAL_CACHE_COMMENT_ONLY_SOURCE);
    driver::clear_file_cache();
    driver::CompilerInvocation coarse_invocation = invocation;
    coarse_invocation.query_pruning_enabled = false;
    auto coarse_reuse = driver::try_reuse_incremental_check_cache(coarse_invocation);
    ASSERT_TRUE(coarse_reuse) << coarse_reuse.error().message;
    EXPECT_FALSE(coarse_reuse.value());

    driver::CompilationProfiler source_stage_green_profiler(true);
    auto source_stage_green_reuse = driver::try_reuse_incremental_check_cache(invocation, &source_stage_green_profiler);
    ASSERT_TRUE(source_stage_green_reuse) << source_stage_green_reuse.error().message;
    EXPECT_TRUE(source_stage_green_reuse.value());
    expect_project_inputs_and_source_stage_profile_phases(
        source_stage_green_profiler, CACHE_TEST_SOURCE_STAGE_REUSE_SINGLE_GREEN_DETAIL);

    driver::CompilerInvocation profiled_pruning_invocation = invocation;
    profiled_pruning_invocation.profile_output_path = cache_dir / "source-stage-green.profile.json";
    auto profiled_green = compiler.run(profiled_pruning_invocation);
    ASSERT_TRUE(profiled_green) << profiled_green.error().message;
    const std::string profile_text = read_text(profiled_pruning_invocation.profile_output_path);
    expect_contains(profile_text, "\"name\": \"incremental_cache.lookup\"");
    expect_contains(profile_text, "\"name\": \"incremental_cache.source_stage_reuse\"");
    expect_contains(profile_text, CACHE_TEST_SOURCE_STAGE_REUSE_SINGLE_GREEN_DETAIL);
    expect_profile_phase_stage_presence(profile_text, CACHE_TEST_SOURCE_STAGE_REUSE_PROFILE_PHASE, false);
    expect_profile_phase_parent_stage(
        profile_text, CACHE_TEST_SOURCE_STAGE_REUSE_PROFILE_PHASE, "incremental_cache.lookup");

    write_source(DRIVER_INCREMENTAL_CACHE_SECOND_SOURCE);
    driver::clear_file_cache();
    driver::CompilationProfiler token_changed_profiler(true);
    auto token_changed_reuse = driver::try_reuse_incremental_check_cache(invocation, &token_changed_profiler);
    ASSERT_TRUE(token_changed_reuse) << token_changed_reuse.error().message;
    EXPECT_FALSE(token_changed_reuse.value());
    expect_project_inputs_and_source_stage_profile_phases(
        token_changed_profiler, CACHE_TEST_SOURCE_STAGE_REUSE_SINGLE_CHANGED_DETAIL);

    driver::clear_file_cache();
}

TEST_F(AurexIntegrationTest, IncrementalCacheQueryPruningProfilesSourceStageRejectReasonsByDefault)
{
    driver::clear_file_cache();

    const fs::path cache_dir = tmp_root() / "incremental-cache-source-stage-reject-profile";
    fs::create_directories(cache_dir);
    const fs::path source = cache_dir / "main.ax";
    const fs::path cache = cache_dir / "main.axic";
    const fs::path canonical_source = fs::weakly_canonical(source);

    const auto write_source = [&](const std::string_view text) {
        std::ofstream out(source, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << text;
    };
    const auto write_cache = [&](const std::string_view text) {
        std::ofstream out(cache, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << text;
    };
    const auto expect_source_stage_reject = [&](const std::string_view expected_detail) {
        driver::CompilerInvocation pruning_invocation;
        pruning_invocation.input_path = source;
        pruning_invocation.emit_kind = driver::EmitKind::check;
        pruning_invocation.incremental_cache_path = cache;
        driver::CompilationProfiler profiler(true);
        auto reuse = driver::try_reuse_incremental_check_cache(pruning_invocation, &profiler);
        ASSERT_TRUE(reuse) << reuse.error().message;
        EXPECT_FALSE(reuse.value());
        expect_project_inputs_and_source_stage_profile_phases(profiler, expected_detail);
    };

    write_source(DRIVER_INCREMENTAL_CACHE_FIRST_SOURCE);
    driver::CompilerInvocation invocation;
    invocation.input_path = source;
    invocation.emit_kind = driver::EmitKind::check;
    invocation.incremental_cache_path = cache;

    driver::Compiler compiler;
    auto first = compiler.run(invocation);
    ASSERT_TRUE(first) << first.error().message;
    const std::string valid_cache = read_text(cache);

    write_source(DRIVER_INCREMENTAL_CACHE_COMMENT_ONLY_SOURCE);
    driver::clear_file_cache();
    write_cache(minimal_cache_without_sources(canonical_source));
    expect_source_stage_reject(CACHE_TEST_SOURCE_STAGE_REUSE_MISSING_ROOT_DETAIL);

    write_cache(cache_test_header(canonical_source) + "sources\t1\n"
        + cache_test_source_row(canonical_source, DRIVER_INCREMENTAL_CACHE_FIRST_SOURCE)
        + "modules\t0\ndefinitions\t0\nqueries\t0\n");
    expect_source_stage_reject(CACHE_TEST_SOURCE_STAGE_REUSE_MISSING_QUERY_DETAIL);

    write_cache(valid_cache);
    fs::remove(source);
    driver::clear_file_cache();
    expect_source_stage_reject(CACHE_TEST_SOURCE_STAGE_REUSE_SOURCE_FAILURE_DETAIL);

    write_source(DRIVER_INCREMENTAL_CACHE_SECOND_SOURCE);
    write_cache(valid_cache);
    driver::clear_file_cache();
    auto no_profile_reuse = driver::try_reuse_incremental_check_cache(invocation);
    ASSERT_TRUE(no_profile_reuse) << no_profile_reuse.error().message;
    EXPECT_FALSE(no_profile_reuse.value());

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
    constexpr base::u32 DRIVER_INCREMENTAL_CACHE_SYNTHETIC_PRIMARY_PART_INDEX = 0;

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
    const fs::path canonical_source = fs::weakly_canonical(source);
    const std::vector<driver::ModulePartRecord> module_part_records{driver::ModulePartRecord{
        {},
        canonical_source,
        DRIVER_INCREMENTAL_CACHE_SYNTHETIC_PRIMARY_PART_INDEX,
        driver::ModulePartRecordKind::primary,
        {},
    }};
    const std::array<driver::ModuleRecord, 1> modules{{
        driver::ModuleRecord{std::string(DRIVER_INCREMENTAL_CACHE_SYNTHETIC_MODULE), source,
            query::package_key(std::span<const std::string_view>{}), syntax::INVALID_MODULE_ID, module_part_records, {},
            std::nullopt},
    }};

    sema::CheckedModule checked;
    const std::array<std::string_view, 1> module_parts{DRIVER_INCREMENTAL_CACHE_SYNTHETIC_MODULE};
    const sema::StableModuleId stable_module = sema::stable_module_id(module_parts);
    const sema::StableDefId template_stable_id = sema::stable_definition_id(
        stable_module, sema::StableSymbolKind::generic_template, DRIVER_INCREMENTAL_CACHE_SYNTHETIC_FUNCTION);
    const query::DefKey template_def = cache_test_def_key_from_stable_id(
        template_stable_id, query::DefNamespace::value, query::DefKind::generic_template);
    const query::CanonicalTypeKey i32_arg = query::canonical_builtin(query::BuiltinTypeKey::i32);
    const std::array<query::CanonicalTypeKey, 1> type_args{i32_arg};
    const query::GenericInstanceKey generic_instance_key = query::generic_instance_key(
        template_def, type_args, std::span<const query::StableFingerprint128>{}, query::param_env_key({}));

    const sema::InternedText duplicate_name = checked.intern_text(DRIVER_INCREMENTAL_CACHE_SYNTHETIC_FUNCTION);
    checked.generic_template_signatures.push_back(sema::GenericTemplateSignatureInfo{
        duplicate_name,
        duplicate_name.id,
        syntax::ModuleId{0},
        syntax::Visibility::public_,
        template_stable_id,
        sema::stable_incremental_key(template_stable_id, DRIVER_INCREMENTAL_CACHE_SYNTHETIC_SIGNATURE),
        query::DefNamespace::value,
        1,
        0,
    });
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
    instance.signature.generic_instance_key = generic_instance_key;
    checked.generic_function_instances.push_back(std::move(instance));

    driver::CompilerInvocation no_cache_pruning_invocation = invocation;
    no_cache_pruning_invocation.incremental_cache_path = cache_dir / "first-pruning.axic";
    driver::CompilationProfiler no_cache_pruning_profiler(true);
    auto no_cache_pruning_result = driver::write_incremental_cache(
        no_cache_pruning_invocation, sources, modules, checked, &no_cache_pruning_profiler);
    ASSERT_TRUE(no_cache_pruning_result) << no_cache_pruning_result.error().message;
    expect_query_profile_phases_with_pruning(no_cache_pruning_profiler, CACHE_TEST_QUERY_DIFF_MISSING_DETAIL,
        CACHE_TEST_QUERY_PLAN_MISSING_DETAIL, CACHE_TEST_QUERY_PRUNING_NO_CACHE_DETAIL);

    driver::CompilationProfiler first_write_profiler(true);
    auto write_result = driver::write_incremental_cache(invocation, sources, modules, checked, &first_write_profiler);
    ASSERT_TRUE(write_result) << write_result.error().message;
    expect_query_profile_phases_with_pruning(first_write_profiler, CACHE_TEST_QUERY_DIFF_MISSING_DETAIL,
        CACHE_TEST_QUERY_PLAN_MISSING_DETAIL, CACHE_TEST_QUERY_PRUNING_NO_CACHE_DETAIL);

    const std::string cache_text = read_text(cache);
    expect_contains(cache_text, "queries\t24");
    expect_contains(cache_text, "query_edges\t23");
    expect_contains(cache_text, "query\tproject_graph");
    expect_contains(cache_text, "query\tfile_content");
    expect_contains(cache_text, "query\tlex_file");
    expect_contains(cache_text, "query\tparse_file");
    expect_contains(cache_text, "query\tmodule_part");
    expect_contains(cache_text, "query\tmodule_graph");
    expect_contains(cache_text, "query\tmodule_exports");
    expect_contains(cache_text, "query\titem_list");
    expect_contains(cache_text, "query\titem_signature");
    expect_contains(cache_text, "query\tgeneric_template_signature");
    expect_contains(cache_text, "query\tgeneric_instance_signature");
    expect_contains(cache_text, "query\tdiagnostics");
    expect_contains(cache_text, "query_edge\tlex_file");
    expect_contains(cache_text, "query_edge\tparse_file");
    expect_contains(cache_text, "query_edge\tmodule_part");
    const query::DefKey expected_item_signature_key =
        cache_test_def_key_from_stable_id(duplicate_stable_id, query::DefNamespace::value, query::DefKind::function);
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
    expect_query_profile_phases_with_pruned_provider_eval(second_write_profiler, CACHE_TEST_QUERY_DIFF_UNCHANGED_DETAIL,
        CACHE_TEST_QUERY_PLAN_UNCHANGED_DETAIL, CACHE_TEST_QUERY_PRUNING_REUSE_ALL_DETAIL,
        CACHE_TEST_QUERY_PROVIDER_EVAL_REUSE_ALL_DETAIL);

    const sema::IncrementalKey expected_generic_incremental_key =
        sema::stable_incremental_key(duplicate_stable_id, DRIVER_INCREMENTAL_CACHE_SYNTHETIC_SIGNATURE);
    const query::GenericInstanceSignatureAuthority expected_generic_authority{
        expected_generic_incremental_key,
        query::GenericInstanceSignatureKind::function,
        syntax::visibility_rank(syntax::Visibility::public_),
        static_cast<base::u32>(generic_instance_key.type_args.size()),
        static_cast<base::u32>(generic_instance_key.const_args.size()),
        generic_instance_key.param_env.predicate_count,
        0,
        0,
        false,
        false,
        false,
        false,
        false,
    };
    const std::optional<query::QueryRecord> expected_generic_record = query::generic_instance_signature_query_record(
        generic_instance_key, query::generic_instance_signature_result_fingerprint(expected_generic_authority));
    ASSERT_TRUE(expected_generic_record.has_value());
    const std::string cached_query_text = read_text(cache);
    const std::optional<CacheTestQueryResultFingerprint> expected_generic_diagnostics_result =
        cache_test_query_result(cached_query_text, CACHE_TEST_QUERY_DIAGNOSTICS,
            hex_encode_cache_test_field(query::stable_serialize(expected_generic_record->key)));
    ASSERT_TRUE(expected_generic_diagnostics_result.has_value());
    const std::optional<CacheTestQueryResultFingerprint> expected_item_result =
        cache_test_query_result(cached_query_text, CACHE_TEST_QUERY_ITEM_SIGNATURE,
            hex_encode_cache_test_field(query::stable_serialize(expected_item_signature_key)));
    ASSERT_TRUE(expected_item_result.has_value());
    const query::StableFingerprint128 expected_generic_diagnostics_result_fingerprint{
        std::stoull(expected_generic_diagnostics_result->primary),
        std::stoull(expected_generic_diagnostics_result->secondary),
        static_cast<base::u32>(std::stoul(expected_generic_diagnostics_result->byte_count)),
    };
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
    const std::optional<query::QueryRecord> expected_generic_diagnostics_record =
        query::diagnostics_query_record(expected_generic_record->key,
            query::QueryResultFingerprint{
                expected_generic_diagnostics_result_fingerprint,
                std::stoull(expected_generic_diagnostics_result->global_id),
            });
    ASSERT_TRUE(expected_generic_diagnostics_record.has_value());

    driver::CompilerInvocation pruning_invocation = invocation;
    const std::string malformed_cache_pruning_detail =
        cache_test_query_pruning_detail_with_fallback(CACHE_TEST_QUERY_PRUNING_FALLBACK_MALFORMED_CACHE);
    const std::string malformed_graph_pruning_detail =
        cache_test_query_pruning_detail_with_fallback(CACHE_TEST_QUERY_PRUNING_FALLBACK_MALFORMED_QUERY_GRAPH);
    const std::string malformed_identity_pruning_detail =
        cache_test_query_pruning_detail_with_fallback(CACHE_TEST_QUERY_PRUNING_FALLBACK_MALFORMED_QUERY_IDENTITY);

    write_cache_text("not-a-cache\n");
    driver::CompilationProfiler malformed_cache_write_profiler(true);
    auto malformed_cache_write_result =
        driver::write_incremental_cache(pruning_invocation, sources, modules, checked, &malformed_cache_write_profiler);
    ASSERT_TRUE(malformed_cache_write_result) << malformed_cache_write_result.error().message;
    expect_query_profile_phases_with_pruning(malformed_cache_write_profiler, CACHE_TEST_QUERY_DIFF_MISSING_DETAIL,
        CACHE_TEST_QUERY_PLAN_MISSING_DETAIL, malformed_cache_pruning_detail);

    const query::QueryDependencyEdge wrong_graph_cached_edge{
        expected_item_record->key,
        expected_generic_diagnostics_record->key,
    };
    const std::string wrong_graph_cached_edge_row = cache_test_query_edge_row(wrong_graph_cached_edge);
    std::string cache_with_wrong_graph_edge = cached_query_text;
    const std::string original_query_edge_count = "query_edges\t23\n";
    const std::size_t graph_query_edge_count_pos = cache_with_wrong_graph_edge.find(original_query_edge_count);
    ASSERT_NE(graph_query_edge_count_pos, std::string::npos);
    cache_with_wrong_graph_edge.replace(
        graph_query_edge_count_pos, original_query_edge_count.size(), "query_edges\t24\n");
    cache_with_wrong_graph_edge += wrong_graph_cached_edge_row;
    write_cache_text(cache_with_wrong_graph_edge);

    driver::CompilationProfiler malformed_graph_write_profiler(true);
    auto malformed_graph_write_result =
        driver::write_incremental_cache(pruning_invocation, sources, modules, checked, &malformed_graph_write_profiler);
    ASSERT_TRUE(malformed_graph_write_result) << malformed_graph_write_result.error().message;
    expect_query_profile_phases_with_pruning(malformed_graph_write_profiler, CACHE_TEST_QUERY_DIFF_MISSING_DETAIL,
        CACHE_TEST_QUERY_PLAN_MISSING_DETAIL, malformed_graph_pruning_detail);
    const std::string graph_repaired_cache_text = read_text(cache);
    expect_contains(graph_repaired_cache_text, "query_edges\t23");
    EXPECT_EQ(graph_repaired_cache_text.find(wrong_graph_cached_edge_row), std::string::npos);

    const query::QueryDependencyEdge wrong_identity_cached_edge{
        expected_generic_diagnostics_record->key,
        expected_item_record->key,
    };
    const std::string wrong_identity_cached_edge_row = cache_test_query_edge_row(wrong_identity_cached_edge);
    std::string cache_with_wrong_identity_edge = cached_query_text;
    const std::size_t query_edge_count_pos = cache_with_wrong_identity_edge.find(original_query_edge_count);
    ASSERT_NE(query_edge_count_pos, std::string::npos);
    cache_with_wrong_identity_edge.replace(query_edge_count_pos, original_query_edge_count.size(), "query_edges\t24\n");
    cache_with_wrong_identity_edge += wrong_identity_cached_edge_row;
    write_cache_text(cache_with_wrong_identity_edge);

    driver::CompilationProfiler malformed_edge_write_profiler(true);
    auto malformed_edge_write_result =
        driver::write_incremental_cache(pruning_invocation, sources, modules, checked, &malformed_edge_write_profiler);
    ASSERT_TRUE(malformed_edge_write_result) << malformed_edge_write_result.error().message;
    expect_query_profile_phases_with_pruning(malformed_edge_write_profiler, CACHE_TEST_QUERY_DIFF_MISSING_DETAIL,
        CACHE_TEST_QUERY_PLAN_MISSING_DETAIL, malformed_identity_pruning_detail);
    const std::string repaired_cache_text = read_text(cache);
    expect_contains(repaired_cache_text, "query_edges\t23");
    EXPECT_EQ(repaired_cache_text.find(wrong_identity_cached_edge_row), std::string::npos);

    const std::optional<query::QueryRecord> malformed_item_shape_record = query::query_record(
        query::QueryKind::item_signature, query::stable_key_fingerprint(expected_item_signature_key.module),
        query::stable_serialize(expected_item_signature_key.module), expected_item_record->result);
    ASSERT_TRUE(malformed_item_shape_record.has_value());
    ASSERT_FALSE(query::query_record_stable_identity_is_valid(*malformed_item_shape_record));
    const std::string expected_item_row = cache_test_query_row(*expected_item_record);
    const std::string malformed_item_shape_row = cache_test_query_row(*malformed_item_shape_record);
    std::string cache_with_malformed_item_shape = cached_query_text;
    const std::size_t expected_item_row_pos = cache_with_malformed_item_shape.find(expected_item_row);
    ASSERT_NE(expected_item_row_pos, std::string::npos);
    cache_with_malformed_item_shape.replace(expected_item_row_pos, expected_item_row.size(), malformed_item_shape_row);
    write_cache_text(cache_with_malformed_item_shape);

    driver::CompilationProfiler malformed_row_write_profiler(true);
    auto malformed_row_write_result =
        driver::write_incremental_cache(pruning_invocation, sources, modules, checked, &malformed_row_write_profiler);
    ASSERT_TRUE(malformed_row_write_result) << malformed_row_write_result.error().message;
    expect_query_profile_phases_with_pruning(malformed_row_write_profiler, CACHE_TEST_QUERY_DIFF_MISSING_DETAIL,
        CACHE_TEST_QUERY_PLAN_MISSING_DETAIL, malformed_identity_pruning_detail);
    const std::string row_shape_repaired_cache_text = read_text(cache);
    EXPECT_EQ(row_shape_repaired_cache_text.find(malformed_item_shape_row), std::string::npos);

    driver::CompilationProfiler pruning_write_profiler(true);
    auto pruning_write_result =
        driver::write_incremental_cache(pruning_invocation, sources, modules, checked, &pruning_write_profiler);
    ASSERT_TRUE(pruning_write_result) << pruning_write_result.error().message;
    expect_query_profile_phases_with_pruned_provider_eval(pruning_write_profiler,
        CACHE_TEST_QUERY_DIFF_UNCHANGED_DETAIL, CACHE_TEST_QUERY_PLAN_UNCHANGED_DETAIL,
        CACHE_TEST_QUERY_PRUNING_REUSE_ALL_DETAIL, CACHE_TEST_QUERY_PROVIDER_EVAL_REUSE_ALL_DETAIL);
    const std::string pruned_cache_text = read_text(cache);
    expect_contains(pruned_cache_text, "query_edges\t23");
    EXPECT_EQ(pruned_cache_text.find(wrong_identity_cached_edge_row), std::string::npos);

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

TEST_F(AurexIntegrationTest, IncrementalCacheWritesGenericBodyRowsButSkipsLowerIRRowsForTypedGenericFunctions)
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
    expect_contains(cache_text, "query\tdiagnostics");
    expect_contains(cache_text, "query_edge\tgeneric_instance_body");
    expect_not_contains(cache_text, "query\tlower_function_ir");
    expect_not_contains(cache_text, "query_edge\tlower_function_ir");

    const fs::path ir_cache = cache_dir / "main.ir.axic";
    driver::CompilerInvocation ir_invocation = invocation;
    ir_invocation.emit_kind = driver::EmitKind::ir;
    ir_invocation.incremental_cache_path = ir_cache;
    testing::internal::CaptureStdout();
    auto ir_result = compiler.run(ir_invocation);
    static_cast<void>(testing::internal::GetCapturedStdout());
    ASSERT_TRUE(ir_result) << ir_result.error().message;

    const std::string ir_cache_text = read_text(ir_cache);
    expect_contains(ir_cache_text, "query\tgeneric_instance_signature");
    expect_contains(ir_cache_text, "query\tgeneric_instance_body");
    expect_contains(ir_cache_text, "query\tlower_function_ir");
    expect_contains(ir_cache_text, "query_edge\tgeneric_instance_body");
    expect_contains(ir_cache_text, "query_edge\tlower_function_ir");

    driver::clear_file_cache();
}

TEST_F(AurexIntegrationTest, IncrementalCacheRecordsTraitDefaultMethodInstanceRows)
{
    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_TRAIT_DEFAULT_SOURCE =
        "module incremental_cache_trait_default;\n"
        "trait Reader {\n"
        "  fn read(self: &Self) -> i32;\n"
        "  fn is_empty(self: &Self) -> bool {\n"
        "    return self.read() == 0;\n"
        "  }\n"
        "}\n"
        "struct File { value: i32; }\n"
        "impl Reader for File {\n"
        "  fn read(self: &File) -> i32 { return self.value; }\n"
        "}\n"
        "fn main() -> i32 {\n"
        "  let file = File { value: 0 };\n"
        "  if file.is_empty() { return 0; }\n"
        "  return 1;\n"
        "}\n";

    driver::clear_file_cache();

    const fs::path cache_dir = tmp_root() / "incremental-cache-trait-default";
    fs::create_directories(cache_dir);
    const fs::path source = cache_dir / "main.ax";
    const fs::path cache = cache_dir / "main.axic";
    {
        std::ofstream out(source, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << DRIVER_INCREMENTAL_CACHE_TRAIT_DEFAULT_SOURCE;
    }

    driver::CompilerInvocation invocation;
    invocation.input_path = source;
    invocation.emit_kind = driver::EmitKind::ir;
    invocation.incremental_cache_path = cache;

    driver::Compiler compiler;
    testing::internal::CaptureStdout();
    auto result = compiler.run(invocation);
    static_cast<void>(testing::internal::GetCapturedStdout());
    ASSERT_TRUE(result) << result.error().message;

    const std::string cache_text = read_text(cache);
    expect_contains(cache_text, "def\ttrait_default_method_instance\tmethod");
    const std::string encoded_default_method_name = hex_encode_cache_test_field("is_empty");
    bool saw_default_as_plain_function = false;
    std::string_view remaining_cache = cache_text;
    while (!remaining_cache.empty()) {
        const base::usize newline = remaining_cache.find('\n');
        const std::string_view row = remaining_cache.substr(0U, newline);
        if (row.starts_with("def\tfunction\tmethod")
            && row.find(encoded_default_method_name) != std::string_view::npos) {
            saw_default_as_plain_function = true;
            break;
        }
        if (newline == std::string_view::npos) {
            break;
        }
        remaining_cache.remove_prefix(newline + 1U);
    }
    EXPECT_FALSE(saw_default_as_plain_function);
    expect_contains(cache_text, "query\tfunction_body_syntax");
    expect_contains(cache_text, "query\ttype_check_body");
    expect_contains(cache_text, "query\tlower_function_ir");
    expect_contains(cache_text, "query_edge\ttype_check_body");
    expect_contains(cache_text, "query_edge\tlower_function_ir");

    driver::clear_file_cache();
}

TEST_F(AurexIntegrationTest, IncrementalCacheLowerIRRowsUseOptimizedIrUnitFingerprint)
{
    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_LOWER_IR_SOURCE =
        "module incremental_cache_lower_ir;\n"
        "fn main() -> i32 { let value: i32 = 7; return value; }\n";

    driver::clear_file_cache();

    const fs::path cache_dir = tmp_root() / "incremental-cache-lower-ir-fingerprint";
    fs::create_directories(cache_dir);
    const fs::path source = cache_dir / "main.ax";
    const fs::path o0_cache = cache_dir / "main.o0.axic";
    const fs::path o1_cache = cache_dir / "main.o1.axic";
    {
        std::ofstream out(source, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << DRIVER_INCREMENTAL_CACHE_LOWER_IR_SOURCE;
    }

    driver::Compiler compiler;
    driver::CompilerInvocation o0_invocation;
    o0_invocation.input_path = source;
    o0_invocation.emit_kind = driver::EmitKind::ir;
    o0_invocation.incremental_cache_path = o0_cache;
    o0_invocation.optimization_level = ir::OptimizationLevel::none;

    testing::internal::CaptureStdout();
    auto o0_result = compiler.run(o0_invocation);
    static_cast<void>(testing::internal::GetCapturedStdout());
    ASSERT_TRUE(o0_result) << o0_result.error().message;

    driver::CompilerInvocation o1_invocation = o0_invocation;
    o1_invocation.incremental_cache_path = o1_cache;
    o1_invocation.optimization_level = ir::OptimizationLevel::basic;

    testing::internal::CaptureStdout();
    auto o1_result = compiler.run(o1_invocation);
    static_cast<void>(testing::internal::GetCapturedStdout());
    ASSERT_TRUE(o1_result) << o1_result.error().message;

    const std::optional<CacheTestQueryResultFingerprint> o0_lower =
        first_cache_test_query_result(read_text(o0_cache), CACHE_TEST_QUERY_LOWER_FUNCTION_IR);
    const std::optional<CacheTestQueryResultFingerprint> o1_lower =
        first_cache_test_query_result(read_text(o1_cache), CACHE_TEST_QUERY_LOWER_FUNCTION_IR);
    ASSERT_TRUE(o0_lower.has_value());
    ASSERT_TRUE(o1_lower.has_value());
    EXPECT_NE(*o0_lower, *o1_lower);

    driver::clear_file_cache();
}

TEST_F(AurexIntegrationTest, IncrementalCacheParsesQueryDependencyEdgeRows)
{
    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_EDGE_SOURCE = "module incremental_cache_query_edges;\n";
    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_EDGE_MODULE = "incremental_cache_query_edges";
    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_EDGE_FUNCTION = "helper";
    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_EDGE_EXPORTS_SIGNATURE = "edge-exports:v1";
    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_EDGE_ITEM_LIST = "edge-item-list:v1";
    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_EDGE_ITEM_SIGNATURE = "edge-item-signature:v1";
    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_EDGE_UNKNOWN_KIND = "unknown_query";
    constexpr base::u32 DRIVER_INCREMENTAL_CACHE_EDGE_PRIMARY_PART_INDEX = 0;
    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_EDGE_CHANGED_DETAIL =
        "total=18,missing=16,unchanged=1,changed=1,malformed=0";
    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_EDGE_PLAN_DETAIL =
        "reusable=1,recompute_roots=17,propagated_recompute=0,recompute=17";
    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_EDGE_PRUNING_DETAIL =
        "enabled=1,applied=1,reused=1,recomputed=17,reused_project_graphs=0,reused_file_contents=0,"
        "reused_lex_files=0,"
        "reused_parse_files=0,reused_module_parts=0,reused_module_graphs=0,reused_module_exports=0,"
        "reused_item_lists=0,"
        "reused_item_signatures=1,reused_function_body_syntaxes=0,reused_type_check_bodies=0,"
        "reused_generic_template_signatures=0,reused_generic_instance_signatures=0,"
        "reused_generic_instance_bodies=0,reused_lower_function_irs=0,reused_diagnostics=0,"
        "recomputed_project_graphs=1,recomputed_file_contents=1,recomputed_lex_files=1,recomputed_parse_files=1,"
        "recomputed_module_parts=1,"
        "recomputed_module_graphs=1,"
        "recomputed_module_exports=1,recomputed_item_lists=1,recomputed_item_signatures=0,"
        "recomputed_function_body_syntaxes=0,recomputed_type_check_bodies=0,"
        "recomputed_generic_template_signatures=0,recomputed_generic_instance_signatures=0,"
        "recomputed_generic_instance_bodies=0,recomputed_lower_function_irs=0,recomputed_diagnostics=9,"
        "fallback=none";
    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_EDGE_PROVIDER_EVAL_DETAIL =
        "mode=pruned,seeded=1,evaluated=17,seeded_project_graphs=0,seeded_file_contents=0,seeded_lex_files=0,"
        "seeded_parse_files=0,"
        "seeded_module_parts=0,seeded_module_graphs=0,seeded_module_exports=0,seeded_item_lists=0,"
        "seeded_item_signatures=1,"
        "seeded_function_body_syntaxes=0,seeded_type_check_bodies=0,seeded_generic_template_signatures=0,"
        "seeded_generic_instance_signatures=0,seeded_generic_instance_bodies=0,seeded_lower_function_irs=0,"
        "seeded_diagnostics=0,evaluated_project_graphs=1,evaluated_file_contents=1,evaluated_lex_files=1,"
        "evaluated_parse_files=1,"
        "evaluated_module_parts=1,evaluated_module_graphs=1,evaluated_module_exports=1,evaluated_item_lists=1,"
        "evaluated_item_signatures=0,evaluated_function_body_syntaxes=0,evaluated_type_check_bodies=0,"
        "evaluated_generic_template_signatures=0,evaluated_generic_instance_signatures=0,"
        "evaluated_generic_instance_bodies=0,evaluated_lower_function_irs=0,evaluated_diagnostics=9";

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
    const query::ModuleKey module_key = cache_test_module_key(module_parts);
    const sema::StableDefId stable_id = sema::stable_definition_id(
        stable_module, sema::StableSymbolKind::function, DRIVER_INCREMENTAL_CACHE_EDGE_FUNCTION);
    const query::DefKey def_key =
        cache_test_def_key_from_stable_id(stable_id, query::DefNamespace::value, query::DefKind::function);
    const query::QueryResultFingerprint exports_result = query::query_result_fingerprint(
        sema::stable_incremental_key(stable_id, DRIVER_INCREMENTAL_CACHE_EDGE_EXPORTS_SIGNATURE));
    const query::QueryResultFingerprint item_list_result =
        query::query_result_fingerprint(query::stable_fingerprint(DRIVER_INCREMENTAL_CACHE_EDGE_ITEM_LIST));
    const query::ItemSignatureAuthority item_authority{
        sema::stable_incremental_key(stable_id, DRIVER_INCREMENTAL_CACHE_EDGE_ITEM_SIGNATURE),
        cache_test_primary_module_part_key(module_key, cache_test_default_package(), canonical_source),
        query::DefNamespace::value,
        query::DefKind::function,
        syntax::visibility_rank(syntax::Visibility::public_),
    };
    const query::QueryResultFingerprint item_result = query::item_signature_result_fingerprint(item_authority);
    const std::optional<query::QueryRecord> item_list_record =
        query::item_list_query_record(module_key, item_list_result);
    const std::optional<query::QueryRecord> exports_record = query::query_record(query::QueryKind::module_exports,
        query::stable_key_fingerprint(module_key), query::stable_serialize(module_key), exports_result);
    const std::optional<query::QueryRecord> item_record = query::item_signature_query_record(def_key, item_result);
    ASSERT_TRUE(item_list_record.has_value());
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

    const query::QueryDependencyEdge self_edge{
        item_record->key,
        item_record->key,
    };
    std::string self_edge_cache = cache_test_header(canonical_source);
    self_edge_cache += "sources\t1\n";
    self_edge_cache += cache_test_source_row(canonical_source, DRIVER_INCREMENTAL_CACHE_EDGE_SOURCE);
    self_edge_cache += "modules\t0\n";
    self_edge_cache += "definitions\t0\n";
    self_edge_cache += "queries\t1\n";
    self_edge_cache += cache_test_query_row(*item_record);
    self_edge_cache += "query_edges\t1\n";
    self_edge_cache += cache_test_query_edge_row(self_edge);
    write_cache(self_edge_cache);
    expect_not_reused();

    std::string missing_dependency_query_cache = cache_test_header(canonical_source);
    missing_dependency_query_cache += "sources\t1\n";
    missing_dependency_query_cache += cache_test_source_row(canonical_source, DRIVER_INCREMENTAL_CACHE_EDGE_SOURCE);
    missing_dependency_query_cache += "modules\t0\n";
    missing_dependency_query_cache += "definitions\t0\n";
    missing_dependency_query_cache += "queries\t1\n";
    missing_dependency_query_cache += cache_test_query_row(*item_record);
    missing_dependency_query_cache += "query_edges\t1\n";
    missing_dependency_query_cache += cache_test_query_edge_row(edge);
    write_cache(missing_dependency_query_cache);
    expect_not_reused();

    const query::QueryDependencyEdge backward_edge{
        exports_record->key,
        item_record->key,
    };
    std::string backward_edge_cache = cache_test_header(canonical_source);
    backward_edge_cache += "sources\t1\n";
    backward_edge_cache += cache_test_source_row(canonical_source, DRIVER_INCREMENTAL_CACHE_EDGE_SOURCE);
    backward_edge_cache += "modules\t0\n";
    backward_edge_cache += "definitions\t0\n";
    backward_edge_cache += "queries\t2\n";
    backward_edge_cache += cache_test_query_row(*exports_record);
    backward_edge_cache += cache_test_query_row(*item_record);
    backward_edge_cache += "query_edges\t1\n";
    backward_edge_cache += cache_test_query_edge_row(backward_edge);
    write_cache(backward_edge_cache);
    expect_not_reused();

    const query::QueryDependencyEdge unexpected_kind_edge{
        item_record->key,
        item_list_record->key,
    };
    std::string unexpected_kind_edge_cache = cache_test_header(canonical_source);
    unexpected_kind_edge_cache += "sources\t1\n";
    unexpected_kind_edge_cache += cache_test_source_row(canonical_source, DRIVER_INCREMENTAL_CACHE_EDGE_SOURCE);
    unexpected_kind_edge_cache += "modules\t0\n";
    unexpected_kind_edge_cache += "definitions\t0\n";
    unexpected_kind_edge_cache += "queries\t3\n";
    unexpected_kind_edge_cache += cache_test_query_row(*item_list_record);
    unexpected_kind_edge_cache += cache_test_query_row(*exports_record);
    unexpected_kind_edge_cache += cache_test_query_row(*item_record);
    unexpected_kind_edge_cache += "query_edges\t1\n";
    unexpected_kind_edge_cache += cache_test_query_edge_row(unexpected_kind_edge);
    write_cache(unexpected_kind_edge_cache);
    expect_not_reused();

    const std::array<std::string_view, 1> wrong_module_parts{"incremental_cache_query_edges_wrong"};
    const sema::StableModuleId wrong_stable_module = sema::stable_module_id(wrong_module_parts);
    const query::ModuleKey wrong_module_key = query::module_key_from_stable_id(wrong_stable_module);
    const std::optional<query::QueryRecord> wrong_exports_record = query::query_record(query::QueryKind::module_exports,
        query::stable_key_fingerprint(wrong_module_key), query::stable_serialize(wrong_module_key), exports_result);
    ASSERT_TRUE(wrong_exports_record.has_value());
    const query::QueryDependencyEdge wrong_identity_edge{
        item_record->key,
        wrong_exports_record->key,
    };
    std::string wrong_identity_edge_cache = cache_test_header(canonical_source);
    wrong_identity_edge_cache += "sources\t1\n";
    wrong_identity_edge_cache += cache_test_source_row(canonical_source, DRIVER_INCREMENTAL_CACHE_EDGE_SOURCE);
    wrong_identity_edge_cache += "modules\t0\n";
    wrong_identity_edge_cache += "definitions\t0\n";
    wrong_identity_edge_cache += "queries\t2\n";
    wrong_identity_edge_cache += cache_test_query_row(*wrong_exports_record);
    wrong_identity_edge_cache += cache_test_query_row(*item_record);
    wrong_identity_edge_cache += "query_edges\t1\n";
    wrong_identity_edge_cache += cache_test_query_edge_row(wrong_identity_edge);
    write_cache(wrong_identity_edge_cache);
    expect_not_reused();

    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_EDGE_SHORT_PARSE_KEY = "short-parse";
    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_EDGE_SHORT_LEX_KEY = "short-lex";
    constexpr std::string_view DRIVER_INCREMENTAL_CACHE_EDGE_SHORT_FILE_KEY = "short-file";
    const std::optional<query::QueryRecord> short_parse_record = query::query_record(query::QueryKind::parse_file,
        query::stable_fingerprint(DRIVER_INCREMENTAL_CACHE_EDGE_SHORT_PARSE_KEY),
        std::string(DRIVER_INCREMENTAL_CACHE_EDGE_SHORT_PARSE_KEY), item_result);
    const std::optional<query::QueryRecord> short_lex_record = query::query_record(query::QueryKind::lex_file,
        query::stable_fingerprint(DRIVER_INCREMENTAL_CACHE_EDGE_SHORT_LEX_KEY),
        std::string(DRIVER_INCREMENTAL_CACHE_EDGE_SHORT_LEX_KEY), item_result);
    const std::optional<query::QueryRecord> short_file_record = query::query_record(query::QueryKind::file_content,
        query::stable_fingerprint(DRIVER_INCREMENTAL_CACHE_EDGE_SHORT_FILE_KEY),
        std::string(DRIVER_INCREMENTAL_CACHE_EDGE_SHORT_FILE_KEY), item_result);
    ASSERT_TRUE(short_parse_record.has_value());
    ASSERT_TRUE(short_lex_record.has_value());
    ASSERT_TRUE(short_file_record.has_value());
    const query::QueryDependencyEdge short_parse_edge{
        short_parse_record->key,
        short_lex_record->key,
    };
    std::string short_parse_edge_cache = cache_test_header(canonical_source);
    short_parse_edge_cache += "sources\t1\n";
    short_parse_edge_cache += cache_test_source_row(canonical_source, DRIVER_INCREMENTAL_CACHE_EDGE_SOURCE);
    short_parse_edge_cache += "modules\t0\n";
    short_parse_edge_cache += "definitions\t0\n";
    short_parse_edge_cache += "queries\t2\n";
    short_parse_edge_cache += cache_test_query_row(*short_lex_record);
    short_parse_edge_cache += cache_test_query_row(*short_parse_record);
    short_parse_edge_cache += "query_edges\t1\n";
    short_parse_edge_cache += cache_test_query_edge_row(short_parse_edge);
    write_cache(short_parse_edge_cache);
    expect_not_reused();

    const query::QueryDependencyEdge short_lex_edge{
        short_lex_record->key,
        short_file_record->key,
    };
    std::string short_lex_edge_cache = cache_test_header(canonical_source);
    short_lex_edge_cache += "sources\t1\n";
    short_lex_edge_cache += cache_test_source_row(canonical_source, DRIVER_INCREMENTAL_CACHE_EDGE_SOURCE);
    short_lex_edge_cache += "modules\t0\n";
    short_lex_edge_cache += "definitions\t0\n";
    short_lex_edge_cache += "queries\t2\n";
    short_lex_edge_cache += cache_test_query_row(*short_file_record);
    short_lex_edge_cache += cache_test_query_row(*short_lex_record);
    short_lex_edge_cache += "query_edges\t1\n";
    short_lex_edge_cache += cache_test_query_edge_row(short_lex_edge);
    write_cache(short_lex_edge_cache);
    expect_not_reused();

    write_cache(edge_cache);

    base::SourceManager sources;
    static_cast<void>(sources.add_source(source.string(), std::string(DRIVER_INCREMENTAL_CACHE_EDGE_SOURCE)));
    const std::vector<driver::ModulePartRecord> module_part_records{driver::ModulePartRecord{
        {},
        canonical_source,
        DRIVER_INCREMENTAL_CACHE_EDGE_PRIMARY_PART_INDEX,
        driver::ModulePartRecordKind::primary,
        {},
    }};
    const std::array<driver::ModuleRecord, 1> modules{{
        driver::ModuleRecord{std::string(DRIVER_INCREMENTAL_CACHE_EDGE_MODULE), source,
            query::package_key(std::span<const std::string_view>{}), syntax::INVALID_MODULE_ID, module_part_records, {},
            std::nullopt},
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

    std::string zero_dependent_global_edge_cache = edge_cache;
    const std::string valid_dependent_global_field = "\t" + std::to_string(item_record->key.global_id) + "\t";
    const std::size_t dependent_global_pos = zero_dependent_global_edge_cache.find(valid_dependent_global_field);
    ASSERT_NE(dependent_global_pos, std::string::npos);
    zero_dependent_global_edge_cache.replace(dependent_global_pos, valid_dependent_global_field.size(), "\t0\t");
    write_cache(zero_dependent_global_edge_cache);
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
    const query::ModuleKey module_key = cache_test_module_key(module_parts);
    const sema::StableDefId helper_stable_id =
        sema::stable_definition_id(stable_module, sema::StableSymbolKind::function, "helper");
    const query::DefKey helper_def_key =
        cache_test_def_key_from_stable_id(helper_stable_id, query::DefNamespace::value, query::DefKind::function);
    const query::BodyKey helper_body_key = query::body_key(helper_def_key, query::BodySlotKind::function_body);
    const std::string encoded_helper_key = hex_encode_cache_test_field(query::stable_serialize(helper_def_key));
    const std::string encoded_helper_body_key = hex_encode_cache_test_field(query::stable_serialize(helper_body_key));
    const std::string encoded_module_key = hex_encode_cache_test_field(query::stable_serialize(module_key));

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
    const std::optional<CacheTestQueryResultFingerprint> first_exports_result =
        cache_test_query_result(first_cache, CACHE_TEST_QUERY_MODULE_EXPORTS, encoded_module_key);
    ASSERT_TRUE(first_exports_result.has_value());
    const std::optional<CacheTestQueryResultFingerprint> first_item_list_result =
        cache_test_query_result(first_cache, CACHE_TEST_QUERY_ITEM_LIST, encoded_module_key);
    ASSERT_TRUE(first_item_list_result.has_value());

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
    const std::optional<CacheTestQueryResultFingerprint> second_exports_result =
        cache_test_query_result(second_cache, CACHE_TEST_QUERY_MODULE_EXPORTS, encoded_module_key);
    ASSERT_TRUE(second_exports_result.has_value());
    const std::optional<CacheTestQueryResultFingerprint> second_item_list_result =
        cache_test_query_result(second_cache, CACHE_TEST_QUERY_ITEM_LIST, encoded_module_key);
    ASSERT_TRUE(second_item_list_result.has_value());

    EXPECT_EQ(*first_result, *second_result);
    EXPECT_EQ(*first_exports_result, *second_exports_result);
    EXPECT_EQ(*first_item_list_result, *second_item_list_result);
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
    const query::ModuleKey module_key = cache_test_module_key(module_parts);
    const sema::StableDefId helper_stable_id =
        sema::stable_definition_id(stable_module, sema::StableSymbolKind::function, "helper");
    const query::DefKey helper_def_key =
        cache_test_def_key_from_stable_id(helper_stable_id, query::DefNamespace::value, query::DefKind::function);
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
    const std::optional<CacheTestQueryResultFingerprint> first_item_list_result =
        cache_test_query_result(read_text(cache), CACHE_TEST_QUERY_ITEM_LIST, encoded_module_key);
    ASSERT_TRUE(first_item_list_result.has_value());

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
    const std::optional<CacheTestQueryResultFingerprint> second_item_list_result =
        cache_test_query_result(read_text(cache), CACHE_TEST_QUERY_ITEM_LIST, encoded_module_key);
    ASSERT_TRUE(second_item_list_result.has_value());

    EXPECT_FALSE(*first_result == *second_result);
    EXPECT_EQ(*first_exports_result, *second_exports_result);
    EXPECT_EQ(*first_item_list_result, *second_item_list_result);

    driver::clear_file_cache();
}

TEST_F(AurexIntegrationTest, IncrementalCacheModuleExportsTracksPublicSignatureWhileItemListStaysGreen)
{
    driver::clear_file_cache();

    const fs::path cache_dir = tmp_root() / "incremental-cache-public-signature";
    fs::create_directories(cache_dir);
    const fs::path source = cache_dir / "main.ax";
    const fs::path cache = cache_dir / "main.axic";
    const auto write_source = [&](const std::string_view text) {
        std::ofstream out(source, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << text;
    };
    const std::array<std::string_view, 1> module_parts{"incremental_cache_public_signature"};

    driver::CompilerInvocation invocation;
    invocation.input_path = source;
    invocation.emit_kind = driver::EmitKind::check;
    invocation.incremental_cache_path = cache;

    write_source(DRIVER_INCREMENTAL_CACHE_PUBLIC_SIGNATURE_FIRST_SOURCE);
    driver::Compiler compiler;
    auto first = compiler.run(invocation);
    ASSERT_TRUE(first) << first.error().message;
    const std::string first_cache = read_text(cache);
    const std::optional<CacheTestQueryResultFingerprint> first_exports_result =
        cache_test_module_query_result(first_cache, CACHE_TEST_QUERY_MODULE_EXPORTS, module_parts);
    const std::optional<CacheTestQueryResultFingerprint> first_item_list_result =
        cache_test_module_query_result(first_cache, CACHE_TEST_QUERY_ITEM_LIST, module_parts);
    ASSERT_TRUE(first_exports_result.has_value());
    ASSERT_TRUE(first_item_list_result.has_value());

    write_source(DRIVER_INCREMENTAL_CACHE_PUBLIC_SIGNATURE_SECOND_SOURCE);
    driver::clear_file_cache();
    auto second = compiler.run(invocation);
    ASSERT_TRUE(second) << second.error().message;
    const std::string second_cache = read_text(cache);
    const std::optional<CacheTestQueryResultFingerprint> second_exports_result =
        cache_test_module_query_result(second_cache, CACHE_TEST_QUERY_MODULE_EXPORTS, module_parts);
    const std::optional<CacheTestQueryResultFingerprint> second_item_list_result =
        cache_test_module_query_result(second_cache, CACHE_TEST_QUERY_ITEM_LIST, module_parts);
    ASSERT_TRUE(second_exports_result.has_value());
    ASSERT_TRUE(second_item_list_result.has_value());

    EXPECT_FALSE(*first_exports_result == *second_exports_result);
    EXPECT_EQ(*first_item_list_result, *second_item_list_result);

    driver::clear_file_cache();
}

TEST_F(AurexIntegrationTest, IncrementalCacheKeysRootPackageIdentity)
{
    driver::clear_file_cache();

    constexpr std::string_view SOURCE = "module incremental_cache_package_identity;\n"
                                        "pub fn exported() -> i32 {\n"
                                        "  return 1;\n"
                                        "}\n";
    constexpr std::string_view PACKAGE_IDENTITY = "demo.package.identity";
    const fs::path cache_dir = tmp_root() / "incremental-cache-package-identity";
    fs::create_directories(cache_dir);
    const fs::path source = cache_dir / "main.ax";
    const fs::path cache = cache_dir / "main.axic";
    {
        std::ofstream out(source, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << SOURCE;
    }

    driver::CompilerInvocation invocation;
    invocation.input_path = source;
    invocation.emit_kind = driver::EmitKind::check;
    invocation.incremental_cache_path = cache;
    invocation.package_identity = PACKAGE_IDENTITY;

    driver::Compiler compiler;
    auto result = compiler.run(invocation);
    ASSERT_TRUE(result) << result.error().message;

    const std::string cache_text = read_text(cache);
    const std::array<std::string_view, 1> module_parts{"incremental_cache_package_identity"};
    const query::PackageKey package = driver::package_key_from_identity(PACKAGE_IDENTITY);
    EXPECT_NE(cache_text.find("package\t" + hex_encode_cache_test_field(PACKAGE_IDENTITY)), std::string::npos);
    EXPECT_TRUE(cache_test_query_result(
        cache_text, CACHE_TEST_QUERY_MODULE_GRAPH, cache_test_encoded_module_key(package, module_parts))
            .has_value());
    EXPECT_FALSE(cache_test_module_query_result(cache_text, CACHE_TEST_QUERY_MODULE_GRAPH, module_parts).has_value());

    driver::clear_file_cache();
}

TEST_F(AurexIntegrationTest, IncrementalCacheKeysRootPackageManifestIdentity)
{
    driver::clear_file_cache();

    constexpr std::string_view MANIFEST = "[package]\n"
                                          "name = \"manifest.cache.pkg\"\n"
                                          "version = \"2.0.0\"\n";
    constexpr std::string_view SOURCE = "module incremental_cache_manifest_package_identity;\n"
                                        "pub fn exported() -> i32 {\n"
                                        "  return 1;\n"
                                        "}\n";
    const fs::path cache_dir = tmp_root() / "incremental-cache-manifest-package-identity";
    fs::create_directories(cache_dir);
    const fs::path source = cache_dir / "src" / "main.ax";
    const fs::path manifest = cache_dir / std::string(driver::DRIVER_PACKAGE_MANIFEST_FILE_NAME);
    const fs::path cache = cache_dir / "main.axic";
    {
        fs::create_directories(source.parent_path());
        std::ofstream out(source, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << SOURCE;
    }
    {
        std::ofstream out(manifest, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << MANIFEST;
    }

    driver::CompilerInvocation invocation;
    invocation.input_path = source;
    invocation.emit_kind = driver::EmitKind::check;
    invocation.incremental_cache_path = cache;

    driver::Compiler compiler;
    auto result = compiler.run(invocation);
    ASSERT_TRUE(result) << result.error().message;

    const std::string cache_text = read_text(cache);
    const std::string package_identity = driver::package_identity_for_invocation(invocation);
    ASSERT_FALSE(package_identity.empty());
    const std::array<std::string_view, 1> module_parts{"incremental_cache_manifest_package_identity"};
    const query::PackageKey package = driver::package_key_for_invocation(invocation);
    EXPECT_NE(cache_text.find("package\t" + hex_encode_cache_test_field(package_identity)), std::string::npos);
    EXPECT_TRUE(cache_test_query_result(
        cache_text, CACHE_TEST_QUERY_MODULE_GRAPH, cache_test_encoded_module_key(package, module_parts))
            .has_value());
    EXPECT_FALSE(cache_test_module_query_result(cache_text, CACHE_TEST_QUERY_MODULE_GRAPH, module_parts).has_value());

    driver::clear_file_cache();
}

TEST_F(AurexIntegrationTest, IncrementalCacheKeysImportPathManifestSourceRoot)
{
    driver::clear_file_cache();

    constexpr std::string_view IMPORT_MANIFEST = "[package]\n"
                                                 "name = \"manifest.cache.import.pkg\"\n"
                                                 "version = \"1.0.0\"\n"
                                                 "source-root = \"src\"\n";
    constexpr std::string_view ROOT_SOURCE = "module incremental_cache_import_source_root;\n"
                                             "import shared.util as util;\n"
                                             "fn main() -> i32 {\n"
                                             "  return util.twice(21) - 42;\n"
                                             "}\n";
    constexpr std::string_view IMPORT_SOURCE = "module shared.util;\n"
                                               "pub fn twice(value: i32) -> i32 {\n"
                                               "  return value + value;\n"
                                               "}\n";

    const fs::path cache_dir = tmp_root() / "incremental-cache-import-source-root";
    const fs::path import_root = cache_dir / "deps" / "shared_pkg";
    const fs::path import_source_root = import_root / "src";
    const fs::path import_source = import_source_root / "shared" / "util.ax";
    const fs::path source = cache_dir / "main.ax";
    const fs::path cache = cache_dir / "main.axic";
    const auto write_source_file = [](const fs::path& path, const std::string_view text) {
        fs::create_directories(path.parent_path());
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << text;
    };
    write_source_file(import_root / std::string(driver::DRIVER_PACKAGE_MANIFEST_FILE_NAME), IMPORT_MANIFEST);
    write_source_file(import_source, IMPORT_SOURCE);
    write_source_file(source, ROOT_SOURCE);

    driver::CompilerInvocation invocation;
    invocation.input_path = source;
    invocation.emit_kind = driver::EmitKind::check;
    invocation.incremental_cache_path = cache;
    invocation.import_paths.push_back(import_root);

    const std::optional<fs::path> resolved_import_source_root =
        driver::package_source_root_for_import_root(import_root.string());
    ASSERT_TRUE(resolved_import_source_root.has_value());

    driver::Compiler compiler;
    auto result = compiler.run(invocation);
    ASSERT_TRUE(result) << result.error().message;

    const std::string cache_text = read_text(cache);
    const std::string import_package_identity = driver::package_identity_for_import_root(import_root.string());
    expect_contains(cache_text, "import_paths\t1");
    expect_contains(cache_text, "import_path\t" + hex_encode_cache_test_field(resolved_import_source_root->string()));
    expect_contains(cache_text, "import_packages\t1");
    expect_contains(cache_text, "import_package\t" + hex_encode_cache_test_field(import_package_identity));
    expect_contains(cache_text, "module_source_root_topologies\t1");
    expect_contains(cache_text,
        "module\t" + hex_encode_cache_test_field("shared.util") + "\t"
            + hex_encode_cache_test_field(fs::weakly_canonical(import_source).string()) + "\t"
            + hex_encode_cache_test_field(resolved_import_source_root->string()) + "\t"
            + hex_encode_cache_test_field((fs::path("shared") / "util.ax").string()));

    auto reuse = driver::try_reuse_incremental_check_cache(invocation);
    ASSERT_TRUE(reuse) << reuse.error().message;
    EXPECT_TRUE(reuse.value());

    driver::clear_file_cache();
}

TEST_F(AurexIntegrationTest, IncrementalCacheReusesUnusedImportPathManifestSourceRoot)
{
    driver::clear_file_cache();

    constexpr std::string_view IMPORT_MANIFEST = "[package]\n"
                                                 "name = \"unused.manifest.cache.import.pkg\"\n"
                                                 "version = \"1.0.0\"\n"
                                                 "source-root = \"src\"\n";
    constexpr std::string_view ROOT_SOURCE = "module incremental_cache_unused_import_source_root;\n";

    const fs::path cache_dir = tmp_root() / "incremental-cache-unused-import-source-root";
    const fs::path import_root = cache_dir / "deps" / "unused_pkg";
    const fs::path source = cache_dir / "main.ax";
    const fs::path cache = cache_dir / "main.axic";
    fs::create_directories(import_root);
    fs::create_directories(source.parent_path());
    {
        std::ofstream out(
            import_root / std::string(driver::DRIVER_PACKAGE_MANIFEST_FILE_NAME), std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << IMPORT_MANIFEST;
    }
    {
        std::ofstream out(source, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << ROOT_SOURCE;
    }

    driver::CompilerInvocation invocation;
    invocation.input_path = source;
    invocation.emit_kind = driver::EmitKind::check;
    invocation.incremental_cache_path = cache;
    invocation.import_paths.push_back(import_root);

    const std::optional<fs::path> resolved_import_source_root =
        driver::package_source_root_for_import_root(import_root.string());
    ASSERT_TRUE(resolved_import_source_root.has_value());

    driver::Compiler compiler;
    auto result = compiler.run(invocation);
    ASSERT_TRUE(result) << result.error().message;

    const std::string cache_text = read_text(cache);
    const std::string import_package_identity = driver::package_identity_for_import_root(import_root.string());
    expect_contains(cache_text, "import_path\t" + hex_encode_cache_test_field(resolved_import_source_root->string()));
    expect_contains(cache_text, "import_packages\t1");
    expect_contains(cache_text, "import_package\t" + hex_encode_cache_test_field(import_package_identity));
    expect_contains(cache_text, "module_source_root_topologies\t0");

    auto reuse = driver::try_reuse_incremental_check_cache(invocation);
    ASSERT_TRUE(reuse) << reuse.error().message;
    EXPECT_TRUE(reuse.value());

    driver::clear_file_cache();
}

TEST_F(AurexIntegrationTest, IncrementalCacheRejectsImportPathManifestIdentityChanges)
{
    driver::clear_file_cache();

    constexpr std::string_view ROOT_SOURCE = "module incremental_cache_import_manifest_identity;\n"
                                             "import shared.util as util;\n"
                                             "fn main() -> i32 {\n"
                                             "  return util.value();\n"
                                             "}\n";
    constexpr std::string_view IMPORT_SOURCE = "module shared.util;\n"
                                               "pub fn value() -> i32 {\n"
                                               "  return 7;\n"
                                               "}\n";
    constexpr std::string_view FIRST_MANIFEST = "[package]\n"
                                                "name = \"manifest.identity.import.pkg\"\n"
                                                "version = \"1.0.0\"\n";
    constexpr std::string_view SECOND_MANIFEST = "[package]\n"
                                                 "name = \"manifest.identity.import.pkg\"\n"
                                                 "version = \"2.0.0\"\n";

    const fs::path cache_dir = tmp_root() / "incremental-cache-import-manifest-identity";
    const fs::path import_root = cache_dir / "deps" / "shared_pkg";
    const fs::path source = cache_dir / "main.ax";
    const fs::path cache = cache_dir / "main.axic";
    const fs::path manifest = import_root / std::string(driver::DRIVER_PACKAGE_MANIFEST_FILE_NAME);
    const auto write_source_file = [](const fs::path& path, const std::string_view text) {
        fs::create_directories(path.parent_path());
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << text;
    };
    write_source_file(source, ROOT_SOURCE);
    write_source_file(import_root / "shared" / "util.ax", IMPORT_SOURCE);
    write_source_file(manifest, FIRST_MANIFEST);

    driver::CompilerInvocation invocation;
    invocation.input_path = source;
    invocation.emit_kind = driver::EmitKind::check;
    invocation.incremental_cache_path = cache;
    invocation.import_paths.push_back(import_root);

    driver::Compiler compiler;
    auto first = compiler.run(invocation);
    ASSERT_TRUE(first) << first.error().message;
    const std::string first_cache = read_text(cache);
    expect_contains(first_cache, "import_packages\t1");
    expect_contains(first_cache,
        "import_package\t"
            + hex_encode_cache_test_field(driver::package_identity_for_import_root(import_root.string())));

    write_source_file(manifest, SECOND_MANIFEST);
    driver::clear_file_cache();

    auto reuse = driver::try_reuse_incremental_check_cache(invocation);
    ASSERT_TRUE(reuse) << reuse.error().message;
    EXPECT_FALSE(reuse.value());

    driver::clear_file_cache();
}

TEST_F(AurexIntegrationTest, IncrementalCacheModuleGraphFingerprintsImportTargetPackage)
{
    driver::clear_file_cache();

    constexpr std::string_view ROOT_SOURCE = "module incremental_cache_import_target_package;\n"
                                             "import shared.util as util;\n"
                                             "fn main() -> i32 {\n"
                                             "  return util.value();\n"
                                             "}\n";
    constexpr std::string_view IMPORT_SOURCE = "module shared.util;\n"
                                               "pub fn value() -> i32 {\n"
                                               "  return 11;\n"
                                               "}\n";
    constexpr std::string_view FIRST_MANIFEST = "[package]\n"
                                                "name = \"target.package.first\"\n"
                                                "version = \"1.0.0\"\n";
    constexpr std::string_view SECOND_MANIFEST = "[package]\n"
                                                 "name = \"target.package.second\"\n"
                                                 "version = \"1.0.0\"\n";

    const fs::path cache_dir = tmp_root() / "incremental-cache-import-target-package";
    const fs::path import_root_a = cache_dir / "deps" / "first";
    const fs::path import_root_b = cache_dir / "deps" / "second";
    const fs::path source = cache_dir / "main.ax";
    const fs::path cache_a = cache_dir / "first.axic";
    const fs::path cache_b = cache_dir / "second.axic";
    const auto write_source_file = [](const fs::path& path, const std::string_view text) {
        fs::create_directories(path.parent_path());
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << text;
    };
    write_source_file(source, ROOT_SOURCE);
    write_source_file(import_root_a / std::string(driver::DRIVER_PACKAGE_MANIFEST_FILE_NAME), FIRST_MANIFEST);
    write_source_file(import_root_b / std::string(driver::DRIVER_PACKAGE_MANIFEST_FILE_NAME), SECOND_MANIFEST);
    write_source_file(import_root_a / "shared" / "util.ax", IMPORT_SOURCE);
    write_source_file(import_root_b / "shared" / "util.ax", IMPORT_SOURCE);

    driver::Compiler compiler;
    driver::CompilerInvocation first_invocation;
    first_invocation.input_path = source;
    first_invocation.emit_kind = driver::EmitKind::check;
    first_invocation.incremental_cache_path = cache_a;
    first_invocation.import_paths.push_back(import_root_a);
    auto first = compiler.run(first_invocation);
    ASSERT_TRUE(first) << first.error().message;

    driver::clear_file_cache();
    driver::CompilerInvocation second_invocation;
    second_invocation.input_path = source;
    second_invocation.emit_kind = driver::EmitKind::check;
    second_invocation.incremental_cache_path = cache_b;
    second_invocation.import_paths.push_back(import_root_b);
    auto second = compiler.run(second_invocation);
    ASSERT_TRUE(second) << second.error().message;

    const std::array<std::string_view, 1> module_parts{"incremental_cache_import_target_package"};
    const std::optional<CacheTestQueryResultFingerprint> first_graph =
        cache_test_module_query_result(read_text(cache_a), CACHE_TEST_QUERY_MODULE_GRAPH, module_parts);
    const std::optional<CacheTestQueryResultFingerprint> second_graph =
        cache_test_module_query_result(read_text(cache_b), CACHE_TEST_QUERY_MODULE_GRAPH, module_parts);
    ASSERT_TRUE(first_graph.has_value());
    ASSERT_TRUE(second_graph.has_value());
    EXPECT_FALSE(*first_graph == *second_graph);

    driver::clear_file_cache();
}

TEST_F(AurexIntegrationTest, IncrementalCacheModuleGraphSortsImportTargetPackagesDeterministically)
{
    constexpr std::string_view MODULE_NAME = "incremental_cache_import_package_sort";
    constexpr std::string_view SOURCE = "module incremental_cache_import_package_sort;\n";

    const fs::path cache_dir = tmp_root() / "incremental-cache-import-package-sort";
    const fs::path source = cache_dir / "main.ax";
    const fs::path first_cache = cache_dir / "first.axic";
    const fs::path second_cache = cache_dir / "second.axic";
    const query::PackageKey package_a = driver::package_key_from_identity("sort.package.a");
    const query::PackageKey package_b = driver::package_key_from_identity("sort.package.b");
    const std::array<std::string_view, 1> module_parts{MODULE_NAME};

    driver::CompilerInvocation invocation;
    invocation.input_path = source;
    invocation.emit_kind = driver::EmitKind::check;

    const auto write_cache_with_imports =
        [&](const fs::path& cache_path,
            const std::array<query::PackageKey, 2>& import_packages) -> std::optional<CacheTestQueryResultFingerprint> {
        base::SourceManager sources;
        static_cast<void>(sources.add_source(source.string(), std::string(SOURCE)));

        driver::ModuleRecord module;
        module.name = std::string(MODULE_NAME);
        module.path = source;
        module.package = cache_test_default_package();
        module.id = syntax::ModuleId{0};
        module.parts.push_back(driver::ModulePartRecord{
            {},
            source,
            0,
            driver::ModulePartRecordKind::primary,
            {},
        });
        for (const query::PackageKey package : import_packages) {
            module.imports.push_back(driver::ModuleImportRecord{
                {},
                "shared.util",
                "util",
                package,
                true,
                syntax::Visibility::private_,
                false,
            });
        }
        const std::array<driver::ModuleRecord, 1> modules{{std::move(module)}};

        sema::CheckedModule checked;
        driver::CompilerInvocation cache_invocation = invocation;
        cache_invocation.incremental_cache_path = cache_path;
        auto write_result = driver::write_incremental_cache(cache_invocation, sources, modules, checked);
        EXPECT_TRUE(write_result) << write_result.error().message;
        if (!write_result) {
            return std::nullopt;
        }
        return cache_test_module_query_result(read_text(cache_path), CACHE_TEST_QUERY_MODULE_GRAPH, module_parts);
    };

    const std::optional<CacheTestQueryResultFingerprint> first_graph =
        write_cache_with_imports(first_cache, std::array<query::PackageKey, 2>{package_a, package_b});
    const std::optional<CacheTestQueryResultFingerprint> second_graph =
        write_cache_with_imports(second_cache, std::array<query::PackageKey, 2>{package_b, package_a});
    ASSERT_TRUE(first_graph.has_value());
    ASSERT_TRUE(second_graph.has_value());
    EXPECT_TRUE(*first_graph == *second_graph);
}

TEST_F(AurexIntegrationTest, IncrementalCacheModuleGraphFingerprintsSourceRootTopologyRecords)
{
    driver::clear_file_cache();

    constexpr std::string_view MODULE_NAME = "incremental_cache_source_root_topology";
    constexpr std::string_view PACKAGE_IDENTITY = "source.root.topology.cache.pkg";
    constexpr std::string_view SOURCE = "module incremental_cache_source_root_topology;\n";

    const fs::path cache_dir = tmp_root() / "incremental-cache-source-root-topology";
    const fs::path source = cache_dir / "main.ax";
    const fs::path source_root = cache_dir / "topology-root";
    const fs::path source_relative = fs::path("app") / "main.ax";
    const fs::path no_topology_cache = cache_dir / "no-topology.axic";
    const fs::path no_topology_repeat_cache = cache_dir / "no-topology-repeat.axic";
    const fs::path topology_cache = cache_dir / "with-topology.axic";
    fs::create_directories(source.parent_path());
    fs::create_directories(source_root);
    {
        std::ofstream out(source, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << SOURCE;
    }

    driver::CompilerInvocation invocation;
    invocation.input_path = source;
    invocation.emit_kind = driver::EmitKind::check;
    invocation.package_identity = PACKAGE_IDENTITY;

    const query::PackageKey package = driver::package_key_from_identity(PACKAGE_IDENTITY);
    const std::array<std::string_view, 1> module_parts{MODULE_NAME};
    const std::string encoded_module_key = cache_test_encoded_module_key(package, module_parts);
    const fs::path canonical_source_root = fs::weakly_canonical(source_root);

    const auto write_cache_for_topology = [&](const fs::path& cache_path,
                                              std::optional<driver::ModuleSourceRootTopologyRecord> topology)
        -> std::optional<CacheTestQueryResultFingerprint> {
        base::SourceManager sources;
        static_cast<void>(sources.add_source(source.string(), std::string(SOURCE)));

        driver::ModuleRecord module;
        module.name = std::string(MODULE_NAME);
        module.path = source;
        module.package = package;
        module.id = syntax::ModuleId{0};
        module.parts.push_back(driver::ModulePartRecord{
            {},
            source,
            0,
            driver::ModulePartRecordKind::primary,
            {},
        });
        module.source_root_topology = std::move(topology);
        const std::array<driver::ModuleRecord, 1> modules{{std::move(module)}};

        sema::CheckedModule checked;
        driver::CompilerInvocation cache_invocation = invocation;
        cache_invocation.incremental_cache_path = cache_path;
        auto write_result = driver::write_incremental_cache(cache_invocation, sources, modules, checked);
        EXPECT_TRUE(write_result) << write_result.error().message;
        if (!write_result) {
            return std::nullopt;
        }
        return cache_test_query_result(read_text(cache_path), CACHE_TEST_QUERY_MODULE_GRAPH, encoded_module_key);
    };

    const std::optional<CacheTestQueryResultFingerprint> no_topology =
        write_cache_for_topology(no_topology_cache, std::nullopt);
    const std::optional<CacheTestQueryResultFingerprint> no_topology_repeat =
        write_cache_for_topology(no_topology_repeat_cache, std::nullopt);
    const std::optional<CacheTestQueryResultFingerprint> with_topology = write_cache_for_topology(topology_cache,
        driver::ModuleSourceRootTopologyRecord{
            canonical_source_root,
            source_relative,
        });
    ASSERT_TRUE(no_topology.has_value());
    ASSERT_TRUE(no_topology_repeat.has_value());
    ASSERT_TRUE(with_topology.has_value());
    EXPECT_EQ(*no_topology, *no_topology_repeat);
    EXPECT_FALSE(*no_topology == *with_topology);

    const std::string no_topology_text = read_text(no_topology_cache);
    const std::string topology_text = read_text(topology_cache);
    const std::string encoded_source_root = hex_encode_cache_test_field(canonical_source_root.string());
    const std::string encoded_source_relative = hex_encode_cache_test_field(source_relative.string());
    EXPECT_EQ(no_topology_text.find(encoded_source_root), std::string::npos);
    expect_contains(topology_text, encoded_source_root + "\t" + encoded_source_relative);

    driver::CompilerInvocation reuse_invocation = invocation;
    reuse_invocation.incremental_cache_path = topology_cache;
    auto reuse = driver::try_reuse_incremental_check_cache(reuse_invocation);
    ASSERT_TRUE(reuse) << reuse.error().message;
    EXPECT_TRUE(reuse.value());

    driver::clear_file_cache();
}

TEST_F(AurexIntegrationTest, IncrementalCacheSemanticSubjectsUseModuleIdPackagesForSplitLogicalNames)
{
    driver::clear_file_cache();

    constexpr std::string_view MODULE_TEXT = "module shared.util;\n";
    constexpr std::string_view FUNCTION_NAME = "value";
    constexpr std::string_view LOCAL_PACKAGE = "local.package";
    constexpr std::string_view IMPORT_ROOT = "/virtual/import/root";
    const fs::path cache_dir = tmp_root() / "incremental-cache-split-package-subjects";
    fs::create_directories(cache_dir);
    const fs::path local_source = cache_dir / "local.ax";
    const fs::path import_source = cache_dir / "import.ax";
    const fs::path cache = cache_dir / "main.axic";
    {
        std::ofstream local_out(local_source, std::ios::binary | std::ios::trunc);
        std::ofstream import_out(import_source, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(local_out.is_open());
        ASSERT_TRUE(import_out.is_open());
        local_out << MODULE_TEXT;
        import_out << MODULE_TEXT;
    }

    const query::PackageKey local_package = driver::package_key_from_identity(LOCAL_PACKAGE);
    const query::PackageKey import_package = driver::package_key_for_import_root(IMPORT_ROOT);
    const std::array<std::string_view, 2> module_parts{"shared", "util"};
    const sema::StableModuleId stable_module = sema::stable_module_id(module_parts);
    const query::ModuleKey local_module = query::module_key_from_stable_id(local_package, stable_module);
    const query::ModuleKey import_module = query::module_key_from_stable_id(import_package, stable_module);
    const std::vector<driver::ModulePartRecord> local_parts{driver::ModulePartRecord{
        {},
        local_source,
        0,
        driver::ModulePartRecordKind::primary,
        cache_test_primary_module_part_key(local_module, local_package, local_source),
    }};
    const std::vector<driver::ModulePartRecord> import_parts{driver::ModulePartRecord{
        {},
        import_source,
        0,
        driver::ModulePartRecordKind::primary,
        cache_test_primary_module_part_key(import_module, import_package, import_source),
    }};
    const std::array<driver::ModuleRecord, 2> modules{{
        driver::ModuleRecord{std::string("shared.util"), local_source, local_package, syntax::ModuleId{0}, local_parts,
            {}, std::nullopt},
        driver::ModuleRecord{std::string("shared.util"), import_source, import_package, syntax::ModuleId{1},
            import_parts, {}, std::nullopt},
    }};

    base::SourceManager sources;
    static_cast<void>(sources.add_source(local_source.string(), std::string(MODULE_TEXT)));
    static_cast<void>(sources.add_source(import_source.string(), std::string(MODULE_TEXT)));

    sema::CheckedModule checked;
    const sema::InternedText name = checked.intern_text(FUNCTION_NAME);
    const sema::StableDefId stable_id =
        sema::stable_definition_id(stable_module, sema::StableSymbolKind::function, FUNCTION_NAME);
    for (base::u32 module_index = 0; module_index < modules.size(); ++module_index) {
        sema::FunctionSignature signature = checked.make_function_signature();
        signature.name = name;
        signature.name_id = name.id;
        signature.module = syntax::ModuleId{module_index};
        signature.stable_id = stable_id;
        signature.incremental_key = sema::stable_incremental_key(
            stable_id, "signature:" + std::to_string(static_cast<unsigned long long>(module_index)));
        checked.functions.emplace(sema::FunctionLookupKey{module_index, 0, name.id}, std::move(signature));
    }

    driver::CompilerInvocation invocation;
    invocation.input_path = local_source;
    invocation.emit_kind = driver::EmitKind::check;
    invocation.incremental_cache_path = cache;
    invocation.package_identity = LOCAL_PACKAGE;

    driver::CompilationProfiler profiler(true);
    auto write_result = driver::write_incremental_cache(invocation, sources, modules, checked, &profiler);
    ASSERT_TRUE(write_result) << write_result.error().message;

    const std::string cache_text = read_text(cache);
    const query::DefKey local_def =
        query::def_key_from_stable_id(local_package, stable_id, query::DefNamespace::value, query::DefKind::function);
    const query::DefKey import_def =
        query::def_key_from_stable_id(import_package, stable_id, query::DefNamespace::value, query::DefKind::function);
    EXPECT_TRUE(cache_test_query_result(
        cache_text, CACHE_TEST_QUERY_ITEM_SIGNATURE, hex_encode_cache_test_field(query::stable_serialize(local_def)))
            .has_value());
    EXPECT_TRUE(cache_test_query_result(
        cache_text, CACHE_TEST_QUERY_ITEM_SIGNATURE, hex_encode_cache_test_field(query::stable_serialize(import_def)))
            .has_value());
    EXPECT_NE(local_def, import_def);

    driver::clear_file_cache();
}

TEST_F(AurexIntegrationTest, IncrementalCacheModuleGraphUsesStableLogicalModuleShape)
{
    driver::clear_file_cache();

    constexpr std::string_view GRAPH_PRIMARY_FIRST = "module incremental_cache_graph_parts;\n"
                                                     "part alpha;\n"
                                                     "part beta;\n"
                                                     "pub fn exported() -> i32 {\n"
                                                     "  return alpha_value() + beta_value();\n"
                                                     "}\n";
    constexpr std::string_view GRAPH_PRIMARY_REORDERED = "module incremental_cache_graph_parts;\n"
                                                         "part beta;\n"
                                                         "part alpha;\n"
                                                         "pub fn exported() -> i32 {\n"
                                                         "  return alpha_value() + beta_value();\n"
                                                         "}\n";
    constexpr std::string_view GRAPH_PRIMARY_ADDED_PART = "module incremental_cache_graph_parts;\n"
                                                          "part beta;\n"
                                                          "part alpha;\n"
                                                          "part gamma;\n"
                                                          "pub fn exported() -> i32 {\n"
                                                          "  return alpha_value() + beta_value() + gamma_value();\n"
                                                          "}\n";
    constexpr std::string_view GRAPH_PRIMARY_RENAMED_PART = "module incremental_cache_graph_parts;\n"
                                                            "part delta;\n"
                                                            "part beta;\n"
                                                            "pub fn exported() -> i32 {\n"
                                                            "  return delta_value() + beta_value();\n"
                                                            "}\n";
    constexpr std::string_view GRAPH_ALPHA_PART = "module incremental_cache_graph_parts part alpha;\n"
                                                  "fn alpha_value() -> i32 {\n"
                                                  "  return 1;\n"
                                                  "}\n";
    constexpr std::string_view GRAPH_BETA_PART = "module incremental_cache_graph_parts part beta;\n"
                                                 "fn beta_value() -> i32 {\n"
                                                 "  return 2;\n"
                                                 "}\n";
    constexpr std::string_view GRAPH_GAMMA_PART = "module incremental_cache_graph_parts part gamma;\n"
                                                  "fn gamma_value() -> i32 {\n"
                                                  "  return 3;\n"
                                                  "}\n";
    constexpr std::string_view GRAPH_DELTA_PART = "module incremental_cache_graph_parts part delta;\n"
                                                  "fn delta_value() -> i32 {\n"
                                                  "  return 4;\n"
                                                  "}\n";

    const fs::path cache_dir = tmp_root() / "incremental-cache-module-graph-parts";
    fs::create_directories(cache_dir);
    const fs::path source = cache_dir / "main.ax";
    const fs::path cache = cache_dir / "main.axic";
    const fs::path parts_dir = cache_dir / "main.parts";
    const auto write_source_file = [](const fs::path& path, const std::string_view text) {
        fs::create_directories(path.parent_path());
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << text;
    };
    const std::array<std::string_view, 1> module_parts{"incremental_cache_graph_parts"};

    write_source_file(parts_dir / "alpha.ax", GRAPH_ALPHA_PART);
    write_source_file(parts_dir / "beta.ax", GRAPH_BETA_PART);

    driver::CompilerInvocation invocation;
    invocation.input_path = source;
    invocation.emit_kind = driver::EmitKind::check;
    invocation.incremental_cache_path = cache;

    write_source_file(source, GRAPH_PRIMARY_FIRST);
    driver::Compiler compiler;
    auto first = compiler.run(invocation);
    ASSERT_TRUE(first) << first.error().message;
    const std::string first_cache_text = read_text(cache);
    expect_contains(first_cache_text, "query\tmodule_part");
    expect_contains(first_cache_text, "query_edge\tmodule_part");
    const std::optional<CacheTestQueryResultFingerprint> first_graph_result =
        cache_test_module_query_result(first_cache_text, CACHE_TEST_QUERY_MODULE_GRAPH, module_parts);
    ASSERT_TRUE(first_graph_result.has_value());

    write_source_file(source, GRAPH_PRIMARY_REORDERED);
    driver::clear_file_cache();
    auto reordered = compiler.run(invocation);
    ASSERT_TRUE(reordered) << reordered.error().message;
    const std::optional<CacheTestQueryResultFingerprint> reordered_graph_result =
        cache_test_module_query_result(read_text(cache), CACHE_TEST_QUERY_MODULE_GRAPH, module_parts);
    ASSERT_TRUE(reordered_graph_result.has_value());
    EXPECT_EQ(*first_graph_result, *reordered_graph_result);

    write_source_file(parts_dir / "delta.ax", GRAPH_DELTA_PART);
    write_source_file(source, GRAPH_PRIMARY_RENAMED_PART);
    driver::clear_file_cache();
    auto renamed = compiler.run(invocation);
    ASSERT_TRUE(renamed) << renamed.error().message;
    const std::optional<CacheTestQueryResultFingerprint> renamed_graph_result =
        cache_test_module_query_result(read_text(cache), CACHE_TEST_QUERY_MODULE_GRAPH, module_parts);
    ASSERT_TRUE(renamed_graph_result.has_value());
    EXPECT_FALSE(*first_graph_result == *renamed_graph_result);

    write_source_file(parts_dir / "gamma.ax", GRAPH_GAMMA_PART);
    write_source_file(source, GRAPH_PRIMARY_ADDED_PART);
    driver::clear_file_cache();
    auto added = compiler.run(invocation);
    ASSERT_TRUE(added) << added.error().message;
    const std::optional<CacheTestQueryResultFingerprint> added_graph_result =
        cache_test_module_query_result(read_text(cache), CACHE_TEST_QUERY_MODULE_GRAPH, module_parts);
    ASSERT_TRUE(added_graph_result.has_value());
    EXPECT_FALSE(*first_graph_result == *added_graph_result);

    driver::clear_file_cache();
}

TEST_F(AurexIntegrationTest, IncrementalCacheModuleExportsRecordsOnlyPrimaryReexportEdges)
{
    driver::clear_file_cache();

    constexpr std::string_view REEXPORT_INNER_SOURCE = "module lib.inner;\n"
                                                       "pub type Count = i32;\n"
                                                       "pub fn value() -> Count {\n"
                                                       "  return 3;\n"
                                                       "}\n";
    constexpr std::string_view REEXPORT_EXTRA_SOURCE = "module lib.extra;\n"
                                                       "pub fn extra() -> i32 {\n"
                                                       "  return 4;\n"
                                                       "}\n";
    constexpr std::string_view PRIMARY_REEXPORT_SOURCE = "module incremental_cache_primary_reexport;\n"
                                                         "pub import lib.inner as inner;\n"
                                                         "pub import lib.extra as extra;\n"
                                                         "pub fn value() -> inner.Count {\n"
                                                         "  return inner.value() + extra.extra();\n"
                                                         "}\n";
    constexpr std::string_view PART_REEXPORT_PRIMARY_SOURCE = "module incremental_cache_part_reexport;\n"
                                                              "part exports;\n"
                                                              "pub fn value() -> i32 {\n"
                                                              "  return part_value();\n"
                                                              "}\n";
    constexpr std::string_view PART_REEXPORT_PART_SOURCE = "module incremental_cache_part_reexport part exports;\n"
                                                           "pub import lib.inner as inner;\n"
                                                           "fn part_value() -> inner.Count {\n"
                                                           "  return inner.value();\n"
                                                           "}\n";

    const fs::path cache_dir = tmp_root() / "incremental-cache-module-reexports";
    const fs::path import_dir = cache_dir / "imports";
    const fs::path primary_source = cache_dir / "primary.ax";
    const fs::path primary_cache = cache_dir / "primary.axic";
    const fs::path part_source = cache_dir / "part.ax";
    const fs::path part_cache = cache_dir / "part.axic";
    const auto write_source_file = [](const fs::path& path, const std::string_view text) {
        fs::create_directories(path.parent_path());
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << text;
    };

    write_source_file(import_dir / "lib" / "inner.ax", REEXPORT_INNER_SOURCE);
    write_source_file(import_dir / "lib" / "extra.ax", REEXPORT_EXTRA_SOURCE);
    write_source_file(primary_source, PRIMARY_REEXPORT_SOURCE);

    driver::CompilerInvocation primary_invocation;
    primary_invocation.input_path = primary_source;
    primary_invocation.emit_kind = driver::EmitKind::check;
    primary_invocation.incremental_cache_path = primary_cache;
    primary_invocation.import_paths.push_back(import_dir);

    driver::Compiler compiler;
    auto primary = compiler.run(primary_invocation);
    ASSERT_TRUE(primary) << primary.error().message;
    const std::string primary_cache_text = read_text(primary_cache);
    EXPECT_EQ(cache_test_query_edge_count(
                  primary_cache_text, CACHE_TEST_QUERY_MODULE_EXPORTS, CACHE_TEST_QUERY_MODULE_EXPORTS),
        static_cast<base::usize>(2));

    write_source_file(part_source, PART_REEXPORT_PRIMARY_SOURCE);
    write_source_file(cache_dir / "part.parts" / "exports.ax", PART_REEXPORT_PART_SOURCE);
    driver::clear_file_cache();

    driver::CompilerInvocation part_invocation;
    part_invocation.input_path = part_source;
    part_invocation.emit_kind = driver::EmitKind::check;
    part_invocation.incremental_cache_path = part_cache;
    part_invocation.import_paths.push_back(import_dir);

    auto part = compiler.run(part_invocation);
    ASSERT_TRUE(part) << part.error().message;
    const std::string part_cache_text = read_text(part_cache);
    EXPECT_EQ(
        cache_test_query_edge_count(part_cache_text, CACHE_TEST_QUERY_MODULE_EXPORTS, CACHE_TEST_QUERY_MODULE_EXPORTS),
        static_cast<base::usize>(0));

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
            cache_test_def_key_from_stable_id(stable_id, query::DefNamespace::value, query::DefKind::function);
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

    write_cache(cache_test_header(canonical_source) + "import_packages\t1\nsources\t0\nmodules\t0\ndefinitions\t0\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source)
        + "import_packages\t1\nimport_package\tzz\nsources\t0\nmodules\t0\ndefinitions\t0\n");
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

    write_cache(cache_test_header(canonical_source)
        + "sources\t0\nmodules\t0\nmodule_source_root_topologies\t1\ndefinitions\t0\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t1\nmodule\t"
        + hex_encode_cache_test_field("incremental_cache_malformed") + "\t"
        + hex_encode_cache_test_field(canonical_source.string()) + "\tzz\t"
        + hex_encode_cache_test_field((fs::path("app") / "main.ax").string()) + "\ndefinitions\t0\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t1\nmodule\t"
        + hex_encode_cache_test_field("incremental_cache_malformed") + "\t"
        + hex_encode_cache_test_field(canonical_source.string()) + "\t"
        + hex_encode_cache_test_field(cache_dir.string()) + "\t"
        + hex_encode_cache_test_field(canonical_source.string()) + "\ndefinitions\t0\n");
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

    write_cache(
        cache_test_header(canonical_source, {cache_dir / "imports"}) + "sources\t0\nmodules\t0\ndefinitions\t0\n");
    driver::CompilerInvocation missing_import_package_identity = invocation;
    missing_import_package_identity.import_paths.push_back(cache_dir / "imports");
    auto missing_import_package_identity_reuse =
        driver::try_reuse_incremental_check_cache(missing_import_package_identity);
    ASSERT_TRUE(missing_import_package_identity_reuse) << missing_import_package_identity_reuse.error().message;
    EXPECT_FALSE(missing_import_package_identity_reuse.value());

    write_cache(cache_test_header(canonical_source, {cache_dir / "imports"})
        + "import_packages\t0\nsources\t0\nmodules\t0\ndefinitions\t0\n");
    driver::CompilerInvocation wrong_import_package_count = invocation;
    wrong_import_package_count.import_paths.push_back(cache_dir / "imports");
    auto wrong_import_package_count_reuse = driver::try_reuse_incremental_check_cache(wrong_import_package_count);
    ASSERT_TRUE(wrong_import_package_count_reuse) << wrong_import_package_count_reuse.error().message;
    EXPECT_FALSE(wrong_import_package_count_reuse.value());

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
    const auto write_import_source = [&](const std::string_view text) {
        std::ofstream out(import_file, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << text;
    };

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

    write_import_source(DRIVER_INCREMENTAL_CACHE_IMPORT_COMMENT_ONLY_SOURCE);
    driver::clear_file_cache();

    driver::CompilerInvocation coarse_invocation = invocation;
    coarse_invocation.query_pruning_enabled = false;
    auto comment_only_stale = driver::try_reuse_incremental_check_cache(coarse_invocation);
    ASSERT_TRUE(comment_only_stale) << comment_only_stale.error().message;
    EXPECT_FALSE(comment_only_stale.value());

    driver::CompilationProfiler import_green_profiler(true);
    auto import_green_reuse = driver::try_reuse_incremental_check_cache(invocation, &import_green_profiler);
    ASSERT_TRUE(import_green_reuse) << import_green_reuse.error().message;
    EXPECT_TRUE(import_green_reuse.value());
    expect_project_inputs_and_source_stage_profile_phases(
        import_green_profiler, CACHE_TEST_SOURCE_STAGE_REUSE_IMPORT_GREEN_DETAIL);

    write_import_source(DRIVER_INCREMENTAL_CACHE_IMPORT_SECOND_SOURCE);
    driver::clear_file_cache();
    auto stale = driver::try_reuse_incremental_check_cache(coarse_invocation);
    ASSERT_TRUE(stale) << stale.error().message;
    EXPECT_FALSE(stale.value());

    driver::CompilationProfiler import_changed_profiler(true);
    auto import_changed_reuse = driver::try_reuse_incremental_check_cache(invocation, &import_changed_profiler);
    ASSERT_TRUE(import_changed_reuse) << import_changed_reuse.error().message;
    EXPECT_FALSE(import_changed_reuse.value());
    expect_project_inputs_and_source_stage_profile_phases(
        import_changed_profiler, CACHE_TEST_SOURCE_STAGE_REUSE_IMPORT_CHANGED_DETAIL);

    auto second = compiler.run(invocation);
    ASSERT_TRUE(second) << second.error().message;
    auto second_reuse = driver::try_reuse_incremental_check_cache(invocation);
    ASSERT_TRUE(second_reuse) << second_reuse.error().message;
    EXPECT_TRUE(second_reuse.value());
    EXPECT_NE(first_cache, read_text(cache));

    driver::clear_file_cache();
}

} // namespace aurex::test
