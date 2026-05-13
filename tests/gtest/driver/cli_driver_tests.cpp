#include <aurex/driver/cli.hpp>
#include <aurex/driver/compiler.hpp>
#include <aurex/driver/file_cache.hpp>
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

[[nodiscard]] driver::CliParseResult require_parse_cli(const std::vector<std::string_view>& args) {
    auto result = driver::parse_cli_arguments(args);
    EXPECT_TRUE(result) << result.error().message;
    return result.value();
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
        "--check",
        "--emit=ast",
        "--emit=ir",
        "--emit=llvm-ir",
        "--emit=asm",
        "--emit=obj",
        "--emit=exe",
        "--dump-modules",
        "--opt-level",
    });

    const fs::path hello = source_root() / "examples" / "hello.ax";
    require_success(aurexc() + " --check " + q(hello));
    require_success(aurexc() + " --emit=check " + q(hello));

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

} // namespace aurex::test
