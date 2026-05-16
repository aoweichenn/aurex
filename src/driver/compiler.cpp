#include <aurex/driver/compiler.hpp>

#include <aurex/base/diagnostic.hpp>
#include <aurex/base/source.hpp>
#include <aurex/backend/llvm_backend.hpp>
#include <aurex/driver/driver_messages.hpp>
#include <aurex/driver/module_loader.hpp>
#include <aurex/driver/file_cache.hpp>
#include <aurex/driver/incremental_cache.hpp>
#include <aurex/driver/native_toolchain.hpp>
#include <aurex/ir/ir_dump.hpp>
#include <aurex/ir/lower_ast.hpp>
#include <aurex/ir/pass_pipeline.hpp>
#include <aurex/lex/lexer.hpp>
#include <aurex/sema/sema.hpp>
#include <aurex/syntax/ast_dump.hpp>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>

#include <unistd.h>

namespace aurex::driver {

namespace {

constexpr base::usize DRIVER_MAX_PRINTED_DIAGNOSTICS = 128;
constexpr base::usize DRIVER_MAX_DIAGNOSTIC_SPAN_LINES = 8;
constexpr std::string_view DRIVER_COLOR_ENV = "AUREX_COLOR_DIAGNOSTICS";
constexpr std::string_view DRIVER_COLOR_ALWAYS = "always";
constexpr std::string_view DRIVER_COLOR_NEVER = "never";
constexpr std::string_view DRIVER_COLOR_AUTO = "auto";
constexpr std::string_view DRIVER_NO_COLOR_ENV = "NO_COLOR";
constexpr std::string_view DRIVER_COLOR_RESET = "\033[0m";
constexpr std::string_view DRIVER_COLOR_ERROR = "\033[1;31m";
constexpr std::string_view DRIVER_COLOR_WARNING = "\033[1;33m";
constexpr std::string_view DRIVER_COLOR_NOTE = "\033[1;36m";
constexpr std::string_view DRIVER_COLOR_HELP = "\033[1;32m";
constexpr std::string_view DRIVER_COLOR_CARET = "\033[1;32m";

[[nodiscard]] bool emit_kind_requires_ir_lowering(const EmitKind emit_kind) noexcept {
    return emit_kind == EmitKind::ir ||
           emit_kind == EmitKind::llvm_ir ||
           emit_kind == EmitKind::assembly ||
           emit_kind == EmitKind::object ||
           emit_kind == EmitKind::executable;
}

[[nodiscard]] base::Result<void> write_file(const std::filesystem::path& path, const std::string_view text) {
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        return base::Result<void>::fail({base::ErrorCode::io_error, std::string(DRIVER_OUTPUT_OPEN_FAILED)});
    }
    output << text;
    if (!output) {
        return base::Result<void>::fail({base::ErrorCode::io_error, std::string(DRIVER_OUTPUT_WRITE_FAILED)});
    }
    return base::Result<void>::ok();
}

[[nodiscard]] base::Result<std::filesystem::path> write_temporary_llvm_file(const std::string_view text) {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        ("aurex_llvm_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".ll");
    auto write_result = write_file(path, text);
    if (!write_result) {
        return base::Result<std::filesystem::path>::fail(write_result.error());
    }
    return base::Result<std::filesystem::path>::ok(path);
}

[[nodiscard]] bool env_equals(const char* const value, const std::string_view expected) noexcept {
    return value != nullptr && std::string_view {value} == expected;
}

[[nodiscard]] bool diagnostic_color_enabled() noexcept {
    const char* const color_env = std::getenv(DRIVER_COLOR_ENV.data());
    if (env_equals(color_env, DRIVER_COLOR_ALWAYS)) {
        return true;
    }
    if (env_equals(color_env, DRIVER_COLOR_NEVER)) {
        return false;
    }
    if (color_env != nullptr && std::string_view {color_env} != DRIVER_COLOR_AUTO) {
        return false;
    }
    if (std::getenv(DRIVER_NO_COLOR_ENV.data()) != nullptr) {
        return false;
    }
    return ::isatty(STDERR_FILENO) != 0;
}

[[nodiscard]] std::string_view severity_color(const base::Severity severity) noexcept {
    switch (severity) {
    case base::Severity::error:
    case base::Severity::fatal:
        return DRIVER_COLOR_ERROR;
    case base::Severity::warning:
        return DRIVER_COLOR_WARNING;
    case base::Severity::note:
        return DRIVER_COLOR_NOTE;
    case base::Severity::help:
        return DRIVER_COLOR_HELP;
    }
    return {};
}

void print_colored(
    const bool color,
    const std::string_view color_code,
    const std::string_view text
) {
    if (color && !color_code.empty()) {
        std::cerr << color_code << text << DRIVER_COLOR_RESET;
        return;
    }
    std::cerr << text;
}

[[nodiscard]] base::usize diagnostic_span_end(
    const base::SourceRange& range,
    const std::string_view text
) noexcept {
    if (range.end > range.begin) {
        return std::min(range.end, text.size());
    }
    return std::min(range.begin + 1, text.size());
}

void print_diagnostic_source_line(
    const base::SourceFile& file,
    const base::SourceRange& range,
    const base::usize line_offset,
    const base::usize span_end,
    const bool color
) {
    const std::string_view text = file.text();
    const base::SourceLineExtent line = file.line_extent(line_offset);
    const std::string_view source_line = text.substr(line.begin, line.end - line.begin);
    std::cerr << "  " << source_line << "\n";
    std::cerr << "  ";

    const base::usize highlight_begin = std::max(range.begin, line.begin);
    const base::usize highlight_end = std::max(
        highlight_begin + 1,
        std::min(span_end, line.end)
    );
    const base::usize caret_column = std::min(highlight_begin, line.end) - line.begin;
    for (base::usize i = 0; i < caret_column && i < source_line.size(); ++i) {
        std::cerr << (source_line[i] == '\t' ? '\t' : ' ');
    }
    print_colored(color, DRIVER_COLOR_CARET, "^");
    for (base::usize i = highlight_begin + 1; i < highlight_end && i < line.end; ++i) {
        print_colored(color, DRIVER_COLOR_CARET, "~");
    }
    std::cerr << "\n";
}

void print_diagnostic_source(
    const base::SourceFile& file,
    const base::Diagnostic& diagnostic,
    const bool color
) {
    const std::string_view text = file.text();
    if (text.empty()) {
        return;
    }
    const base::usize span_begin = std::min(diagnostic.range.begin, text.size());
    const base::usize span_end = diagnostic_span_end(diagnostic.range, text);
    base::usize line_offset = span_begin;
    base::usize printed_lines = 0;
    while (line_offset <= span_end && printed_lines < DRIVER_MAX_DIAGNOSTIC_SPAN_LINES) {
        const base::SourceLineExtent line = file.line_extent(line_offset);
        print_diagnostic_source_line(file, diagnostic.range, line_offset, span_end, color);
        printed_lines += 1;
        if (span_end <= line.end || line.end >= text.size()) {
            return;
        }
        line_offset = line.end + 1;
    }
    if (line_offset <= span_end) {
        std::cerr << "  ...\n";
    }
}

void print_diagnostics(const base::SourceManager& sources, const base::DiagnosticSink& diagnostics) {
    const std::span<const base::Diagnostic> all = diagnostics.diagnostics();
    const base::usize count = std::min<base::usize>(all.size(), DRIVER_MAX_PRINTED_DIAGNOSTICS);
    const bool color = diagnostic_color_enabled();
    for (base::usize index = 0; index < count; ++index) {
        const base::Diagnostic& diagnostic = all[index];
        const base::SourceFile& file = sources.get(diagnostic.range.source);
        const base::LineColumn location = file.line_column(diagnostic.range.begin);
        std::cerr << file.path() << ":" << location.line << ":" << location.column << ": ";
        print_colored(color, severity_color(diagnostic.severity), base::severity_name(diagnostic.severity));
        std::cerr << ": " << diagnostic.message << "\n";
        print_diagnostic_source(file, diagnostic, color);
    }
    if (all.size() > DRIVER_MAX_PRINTED_DIAGNOSTICS) {
        std::cerr << "error: too many diagnostics; suppressing "
                  << (all.size() - DRIVER_MAX_PRINTED_DIAGNOSTICS)
                  << " additional diagnostics\n";
    }
}

} // namespace

base::Result<void> Compiler::run(const CompilerInvocation& invocation) const
{
    if (invocation.emit_kind == EmitKind::check) {
        auto cache_result = try_reuse_incremental_check_cache(invocation);
        if (!cache_result) {
            return base::Result<void>::fail(cache_result.error());
        }
        if (cache_result.value()) {
            return base::Result<void>::ok();
        }
    }

    base::SourceManager sources;
    base::DiagnosticSink diagnostics;

    if (invocation.emit_kind == EmitKind::tokens) {
        auto source_result = read_text_file(invocation.input_path);
        if (!source_result) {
            return base::Result<void>::fail(source_result.error());
        }
        const base::SourceId source_id = sources.add_source(invocation.input_path.string(), source_result.take_value());
        lex::Lexer lexer(source_id, sources.text(source_id), diagnostics);
        auto token_result = lexer.tokenize();
        if (!token_result) {
            print_diagnostics(sources, diagnostics);
            return base::Result<void>::fail(token_result.error());
        }
        std::cout << syntax::dump_tokens(token_result.value());
        return base::Result<void>::ok();
    }

    ModuleLoader loader(invocation, sources, diagnostics);
    auto ast_result = loader.load_root();

    if (!ast_result) {
        print_diagnostics(sources, diagnostics);
        return base::Result<void>::fail(ast_result.error());
    }
    syntax::AstModule ast = ast_result.take_value();

    if (invocation.emit_kind == EmitKind::ast) {
        std::cout << syntax::dump_ast(ast);
        return base::Result<void>::ok();
    }

    if (invocation.emit_kind == EmitKind::modules) {
        std::cout << "modules\n";
        for (const ModuleRecord& record : loader.modules()) {
            std::cout << "  " << record.name << " " << record.path.string() << "\n";
        }
        return base::Result<void>::ok();
    }

    sema::SemanticOptions sema_options;
    sema_options.retain_generic_side_tables =
        emit_kind_requires_ir_lowering(invocation.emit_kind) ||
        invocation.emit_kind == EmitKind::typed;
    sema::SemanticAnalyzer analyzer(ast, diagnostics, sema_options);
    auto checked_result = analyzer.analyze();
    if (!checked_result) {
        print_diagnostics(sources, diagnostics);
        return base::Result<void>::fail(checked_result.error());
    }

    auto incremental_cache_result =
        write_incremental_cache(invocation, sources, loader.modules(), checked_result.value());
    if (!incremental_cache_result) {
        return base::Result<void>::fail(incremental_cache_result.error());
    }

    if (invocation.emit_kind == EmitKind::check) {
        return base::Result<void>::ok();
    }

    if (invocation.emit_kind == EmitKind::typed) {
        return base::Result<void>::ok();
    }

    if (invocation.emit_kind == EmitKind::checked) {
        std::cout << sema::dump_checked_module(checked_result.value());
        return base::Result<void>::ok();
    }

    if (invocation.emit_kind == EmitKind::ir || invocation.emit_kind == EmitKind::llvm_ir) {
        auto ir_result = ir::lower_ast(ast, checked_result.value());
        if (!ir_result) {
            return base::Result<void>::fail(ir_result.error());
        }
        auto pipeline_result = ir::run_pass_pipeline(ir_result.value(), ir::PassPipelineOptions {
            invocation.optimization_level,
            true,
            true,
            true,
            true,
        });
        if (!pipeline_result) {
            return pipeline_result;
        }
        if (invocation.emit_kind == EmitKind::ir) {
            std::cout << ir::dump_module(ir_result.value());
            return base::Result<void>::ok();
        }
        auto llvm_result = backend::emit_llvm_ir(backend::LlvmEmitRequest {
            &ir_result.value(),
            invocation.input_path.stem().string(),
        });
        if (!llvm_result) {
            return base::Result<void>::fail(llvm_result.error());
        }
        std::cout << llvm_result.value().text;
        return base::Result<void>::ok();
    }

    if (invocation.emit_kind == EmitKind::assembly ||
        invocation.emit_kind == EmitKind::object ||
        invocation.emit_kind == EmitKind::executable) {
        if (invocation.output_path.empty()) {
            return base::Result<void>::fail({base::ErrorCode::io_error, std::string(DRIVER_NATIVE_OUTPUT_REQUIRES_OUTPUT_PATH)});
        }
        auto ir_result = ir::lower_ast(ast, checked_result.value());
        if (!ir_result) {
            return base::Result<void>::fail(ir_result.error());
        }
        auto pipeline_result = ir::run_pass_pipeline(ir_result.value(), ir::PassPipelineOptions {
            invocation.optimization_level,
            true,
            true,
            true,
            true,
        });
        if (!pipeline_result) {
            return pipeline_result;
        }
        auto llvm_result = backend::emit_llvm_ir(backend::LlvmEmitRequest {
            &ir_result.value(),
            invocation.input_path.stem().string(),
        });
        if (!llvm_result) {
            return base::Result<void>::fail(llvm_result.error());
        }
        auto temp_ir_result = write_temporary_llvm_file(llvm_result.value().text);
        if (!temp_ir_result) {
            return base::Result<void>::fail(temp_ir_result.error());
        }
        NativeCompileRequest request;
        request.clang_path = invocation.clang_path;
        request.clang_args = invocation.clang_args;
        request.input_path = temp_ir_result.value();
        request.output_path = invocation.output_path;
        request.emit_kind = invocation.emit_kind;
        request.input_is_llvm_ir = true;
        auto native_result = invoke_clang(request);
        std::error_code remove_error;
        std::filesystem::remove(temp_ir_result.value(), remove_error);
        if (!native_result) {
            return native_result;
        }
        return base::Result<void>::ok();
    }

    return base::Result<void>::fail({base::ErrorCode::codegen_error, std::string(DRIVER_UNSUPPORTED_EMISSION_MODE)});
}

} // namespace aurex::driver
