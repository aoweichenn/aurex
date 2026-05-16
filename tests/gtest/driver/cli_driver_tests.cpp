#include <aurex/driver/cli.hpp>
#include <aurex/base/config.hpp>
#include <aurex/driver/compiler.hpp>
#include <aurex/driver/file_cache.hpp>
#include <aurex/driver/incremental_cache.hpp>
#include <support/test_support.hpp>

#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <thread>
#include <string>
#include <string_view>
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

[[nodiscard]] driver::CliParseResult require_parse_cli(const std::vector<std::string_view>& args) {
    auto result = driver::parse_cli_arguments(args);
    EXPECT_TRUE(result) << result.error().message;
    return result.value();
}

[[nodiscard]] std::string hex_encode_cache_test_field(const std::string_view value) {
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

[[nodiscard]] std::string uppercase_hex_cache_test_field(std::string encoded) {
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

[[nodiscard]] std::string cache_test_header(
    const fs::path& root,
    const std::vector<fs::path>& import_paths = {}
) {
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
    const fs::path& root,
    const std::string_view compiler_version,
    const std::string_view mode
) {
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
    const fs::path& root,
    const std::vector<fs::path>& import_paths = {}
) {
    std::string cache = cache_test_header(root, import_paths);
    cache += "sources\t0\n";
    cache += "modules\t0\n";
    cache += "definitions\t0\n";
    return cache;
}

[[nodiscard]] std::string cache_test_source_row(
    const fs::path& path,
    const std::string_view size,
    const std::string_view primary,
    const std::string_view secondary,
    const std::string_view byte_count
) {
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

[[nodiscard]] std::string cache_test_def_row(
    const std::string_view category,
    const std::string_view stable_kind,
    const std::string_view stable_global,
    const std::string_view stable_primary,
    const std::string_view stable_secondary,
    const std::string_view stable_bytes,
    const std::string_view incremental_global,
    const std::string_view incremental_primary,
    const std::string_view incremental_secondary,
    const std::string_view incremental_bytes,
    const std::string_view encoded_name
) {
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

} // namespace

TEST(CoreUnit, CliParserIsTableDrivenAndSupportsModernDriverForms) {
    const std::vector<std::string_view> object_args {
        "aurexc",
        "-c",
        "-Itests/samples/imports",
        "--clang=/usr/bin/clang",
        "--clang-arg=-fno-color-diagnostics",
        "--opt-level=O2",
        "--incremental-cache",
        "build/hello.axic",
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

    const std::vector<std::string_view> assembly_args {
        "aurexc",
        "-S",
        "examples/hello.ax",
    };
    const driver::CliParseResult assembly_parse = require_parse_cli(assembly_args);
    EXPECT_EQ(assembly_parse.invocation.emit_kind, driver::EmitKind::assembly);
    EXPECT_EQ(assembly_parse.invocation.output_path, fs::path("hello.s"));

    const std::vector<std::string_view> separate_emit_args {
        "aurexc",
        "--emit",
        "llvm-ir",
        "-fsyntax-only",
        "examples/hello.ax",
    };
    const driver::CliParseResult separate_emit_parse = require_parse_cli(separate_emit_args);
    EXPECT_EQ(separate_emit_parse.invocation.emit_kind, driver::EmitKind::check);

    const std::vector<std::string_view> typed_emit_args {
        "aurexc",
        "--emit=typed",
        "examples/hello.ax",
    };
    const driver::CliParseResult typed_emit_parse = require_parse_cli(typed_emit_args);
    EXPECT_EQ(typed_emit_parse.invocation.emit_kind, driver::EmitKind::typed);

    const std::vector<std::string_view> inference_reset_args {
        "aurexc",
        "-S",
        "--emit=ir",
        "examples/hello.ax",
    };
    const driver::CliParseResult inference_reset_parse = require_parse_cli(inference_reset_args);
    EXPECT_EQ(inference_reset_parse.invocation.emit_kind, driver::EmitKind::ir);
    EXPECT_TRUE(inference_reset_parse.invocation.output_path.empty());

    const std::vector<std::string_view> dump_llvm_ir_args {
        "aurexc",
        "--dump-llvm-ir",
        "examples/hello.ax",
    };
    const driver::CliParseResult dump_llvm_ir_parse = require_parse_cli(dump_llvm_ir_args);
    EXPECT_EQ(dump_llvm_ir_parse.invocation.emit_kind, driver::EmitKind::llvm_ir);

    const std::vector<std::string_view> option_end_args {
        "aurexc",
        "--",
        "-strange.ax",
    };
    const driver::CliParseResult option_end_parse = require_parse_cli(option_end_args);
    EXPECT_EQ(option_end_parse.invocation.input_path, fs::path("-strange.ax"));
}

TEST(CoreUnit, CliParserReportsTableDrivenArgumentErrors) {
    const std::vector<std::string_view> invalid_emit_args {
        "aurexc",
        "--emit=not-a-kind",
        "examples/hello.ax",
    };
    const auto invalid_emit = driver::parse_cli_arguments(invalid_emit_args);
    ASSERT_FALSE(invalid_emit);
    expect_contains(invalid_emit.error().message, "invalid emit kind");

    const std::vector<std::string_view> unexpected_value_args {
        "aurexc",
        "--help=true",
    };
    const auto unexpected_value = driver::parse_cli_arguments(unexpected_value_args);
    ASSERT_FALSE(unexpected_value);
    expect_contains(unexpected_value.error().message, "does not take a value");

    const std::vector<std::string_view> unknown_equal_args {
        "aurexc",
        "--unknown=value",
    };
    const auto unknown_equal = driver::parse_cli_arguments(unknown_equal_args);
    ASSERT_FALSE(unknown_equal);
    expect_contains(unknown_equal.error().message, "unknown option: --unknown");

    const std::vector<std::string_view> inapplicable_native_backend_args {
        "aurexc",
        "--clang=/usr/bin/clang",
        "--emit=ir",
        "examples/hello.ax",
    };
    const auto inapplicable_native_backend = driver::parse_cli_arguments(inapplicable_native_backend_args);
    ASSERT_FALSE(inapplicable_native_backend);
    expect_contains(inapplicable_native_backend.error().message, "option requires native output: --clang");

    std::ostringstream out;
    std::ostringstream err;
    const std::vector<std::string_view> missing_file_args {
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

TEST_F(AurexIntegrationTest, CliAndFrontendDumps) {
    expect_contains(require_success(aurexc() + " --version").output, "0.1.2");

    const std::string help = require_success(aurexc() + " --help").output;
    expect_contains_all(help, {
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

    const std::string eval_order =
        require_success(aurexc() + " --emit=ir --opt-level O1 " + q(positive_sample("evaluation", "eval_order_assign.ax"))).output;
    expect_contains(eval_order, "call m0_eval_order_assign_next(%");

    const std::string pointer_field =
        require_success(aurexc() + " --emit=ir " + q(positive_sample("pointers", "pointer_field_write.ax"))).output;
    expect_contains(pointer_field, "field_addr ");
    expect_contains(pointer_field, ".value");
}

TEST_F(AurexIntegrationTest, CompilerDriverErrorBranches) {
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
        driver::Compiler compiler;
        const auto result = compiler.run(invocation);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code, base::ErrorCode::codegen_error);
        expect_contains(result.error().message, "exit code 127");
    }

}

TEST_F(AurexIntegrationTest, CliFileCacheCoversMissHitClearAndEmptyFiles) {
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

TEST_F(AurexIntegrationTest, IncrementalCacheWritesValidatesInvalidatesAndReusesCheck) {
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

TEST_F(AurexIntegrationTest, IncrementalCacheRejectsMalformedMismatchedAndBlockedCacheFiles) {
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

    write_cache(cache_test_header(canonical_source) + "mode\t"
        + hex_encode_cache_test_field("semantic-ok") + "\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "root\t"
        + hex_encode_cache_test_field(canonical_source.string()) + "\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t1\nsource\tbad\t0\t0\t0\t"
        + hex_encode_cache_test_field(canonical_source.string()) + "\nmodules\t0\ndefinitions\t0\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t1\nsource\t1\t0\t0\tbad\t"
        + hex_encode_cache_test_field(canonical_source.string()) + "\nmodules\t0\ndefinitions\t0\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t1\n"
        + cache_test_source_row(canonical_source, "1", "bad", "0", "0")
        + "modules\t0\ndefinitions\t0\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t1\n"
        + cache_test_source_row(canonical_source, "1", "0", "bad", "0")
        + "modules\t0\ndefinitions\t0\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t1\n"
        + cache_test_source_row(canonical_source, "1", "0", "0", "4294967296")
        + "modules\t0\ndefinitions\t0\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t1\nsource\t1\t0\t0\t1\t0\n"
        + "modules\t0\ndefinitions\t0\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t1\nsource\ttoo-few\n"
        + "modules\t0\ndefinitions\t0\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t1\nmodule\tbad\n"
        + "definitions\t0\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t1\nmodule\t00\t0\n"
        + "definitions\t0\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t1\nmodule\t0\t00\n"
        + "definitions\t0\n");
    expect_not_reused();

    write_cache(cache_test_header(canonical_source) + "sources\t0\nmodules\t0\ndefinitions\t1\n"
        + "def\ttoo-few\n");
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

    const fs::path null_device("/dev/null");
    if (fs::exists(null_device)) {
        write_cache(cache_test_header(canonical_source) + "sources\t1\n"
            + cache_test_source_row(null_device, "1", "0", "0", "1")
            + "modules\t0\ndefinitions\t0\n");
        expect_not_reused();
    }

    write_cache(cache_test_header(canonical_source, {cache_dir / "imports-a"}) +
        "sources\t0\nmodules\t0\ndefinitions\t0\n");
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

TEST_F(AurexIntegrationTest, IncrementalCacheTracksImportPathAndDependencyFingerprints) {
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
