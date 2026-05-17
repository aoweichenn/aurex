#include <aurex/driver/compiler.hpp>

#include <aurex/base/diagnostic.hpp>
#include <aurex/base/source.hpp>
#include <aurex/backend/llvm_backend.hpp>
#include <aurex/driver/driver_messages.hpp>
#include <aurex/driver/module_loader.hpp>
#include <aurex/driver/file_cache.hpp>
#include <aurex/driver/incremental_cache.hpp>
#include <aurex/driver/native_toolchain.hpp>
#include <aurex/driver/profile.hpp>
#include <aurex/ir/ir_dump.hpp>
#include <aurex/ir/lower_ast.hpp>
#include <aurex/ir/pass_pipeline.hpp>
#include <aurex/lex/lexer.hpp>
#include <aurex/sema/sema.hpp>
#include <aurex/syntax/ast_dump.hpp>

#include <algorithm>
#include <chrono>
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
constexpr char DRIVER_JSON_QUOTE = '"';
constexpr char DRIVER_JSON_BACKSLASH = '\\';
constexpr char DRIVER_JSON_NEWLINE = '\n';
constexpr char DRIVER_JSON_CARRIAGE_RETURN = '\r';
constexpr char DRIVER_JSON_TAB = '\t';
constexpr unsigned int DRIVER_JSON_CONTROL_CHAR_LIMIT = 0x20U;
constexpr unsigned int DRIVER_JSON_BYTE_MASK = 0xffU;
constexpr unsigned int DRIVER_JSON_NIBBLE_BITS = 4U;
constexpr unsigned int DRIVER_JSON_LOW_NIBBLE_MASK = 0x0fU;
constexpr char DRIVER_JSON_HEX_DIGITS[] = "0123456789abcdef";

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
    const auto write_result = write_file(path, text);
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

void print_json_escaped(const std::string_view text) {
    std::cerr << DRIVER_JSON_QUOTE;
    for (const unsigned char byte : text) {
        switch (byte) {
        case DRIVER_JSON_QUOTE:
            std::cerr << "\\\"";
            break;
        case DRIVER_JSON_BACKSLASH:
            std::cerr << "\\\\";
            break;
        case DRIVER_JSON_NEWLINE:
            std::cerr << "\\n";
            break;
        case DRIVER_JSON_CARRIAGE_RETURN:
            std::cerr << "\\r";
            break;
        case DRIVER_JSON_TAB:
            std::cerr << "\\t";
            break;
        default:
            if (byte < DRIVER_JSON_CONTROL_CHAR_LIMIT) {
                std::cerr << "\\u00"
                          << DRIVER_JSON_HEX_DIGITS[(byte >> DRIVER_JSON_NIBBLE_BITS) & DRIVER_JSON_LOW_NIBBLE_MASK]
                          << DRIVER_JSON_HEX_DIGITS[byte & DRIVER_JSON_LOW_NIBBLE_MASK];
            } else {
                std::cerr << static_cast<char>(byte & DRIVER_JSON_BYTE_MASK);
            }
            break;
        }
    }
    std::cerr << DRIVER_JSON_QUOTE;
}

void print_json_string_field(
    const std::string_view key,
    const std::string_view value,
    const bool trailing_comma
) {
    print_json_escaped(key);
    std::cerr << ": ";
    print_json_escaped(value);
    if (trailing_comma) {
        std::cerr << ",";
    }
    std::cerr << "\n";
}

void print_json_range(const base::SourceFile& file, const base::SourceRange& range) {
    const base::LineColumn start = file.line_column(range.begin);
    const base::LineColumn end = file.line_column(range.end);
    std::cerr << "      \"range\": {\n";
    std::cerr << "        \"source_id\": " << range.source.value << ",\n";
    std::cerr << "        \"path\": ";
    print_json_escaped(file.path());
    std::cerr << ",\n";
    std::cerr << "        \"start\": {\"byte\": " << range.begin
              << ", \"line\": " << start.line
              << ", \"column\": " << start.column << "},\n";
    std::cerr << "        \"end\": {\"byte\": " << range.end
              << ", \"line\": " << end.line
              << ", \"column\": " << end.column << "}\n";
    std::cerr << "      }\n";
}

void print_json_diagnostics(const base::SourceManager& sources, const base::DiagnosticSink& diagnostics) {
    const std::span<const base::Diagnostic> all = diagnostics.diagnostics();
    const base::usize count = std::min<base::usize>(all.size(), DRIVER_MAX_PRINTED_DIAGNOSTICS);
    std::cerr << "{\n";
    std::cerr << "  \"format\": \"aurex-diagnostics-v1\",\n";
    std::cerr << "  \"diagnostics\": [\n";
    for (base::usize index = 0; index < count; ++index) {
        const base::Diagnostic& diagnostic = all[index];
        const base::SourceFile& file = sources.get(diagnostic.range.source);
        std::cerr << "    {\n";
        std::cerr << "      ";
        print_json_string_field("severity", base::severity_name(diagnostic.severity), true);
        std::cerr << "      ";
        print_json_string_field("category", base::diagnostic_category_name(diagnostic.category), true);
        std::cerr << "      ";
        print_json_string_field("code", base::diagnostic_code_name(diagnostic.code), true);
        std::cerr << "      ";
        print_json_string_field("message", diagnostic.message, true);
        print_json_range(file, diagnostic.range);
        std::cerr << "    }";
        if (index + 1 < count) {
            std::cerr << ",";
        }
        std::cerr << "\n";
    }
    std::cerr << "  ],\n";
    std::cerr << "  \"suppressed\": "
              << (all.size() > DRIVER_MAX_PRINTED_DIAGNOSTICS ? all.size() - DRIVER_MAX_PRINTED_DIAGNOSTICS : 0)
              << "\n";
    std::cerr << "}\n";
}

void print_text_diagnostics(const base::SourceManager& sources, const base::DiagnosticSink& diagnostics) {
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

void print_diagnostics(
    const base::SourceManager& sources,
    const base::DiagnosticSink& diagnostics,
    const DiagnosticOutputFormat format
) {
    if (diagnostics.diagnostics().empty()) {
        return;
    }
    if (format == DiagnosticOutputFormat::json) {
        print_json_diagnostics(sources, diagnostics);
        return;
    }
    print_text_diagnostics(sources, diagnostics);
}

[[nodiscard]] base::Error remap_diagnostic_loader_error(const base::Error& error) {
    return {
        error.code == base::ErrorCode::io_error ? base::ErrorCode::parse_error : error.code,
        error.message,
    };
}

class CompilerRunProfile final {
public:
    explicit CompilerRunProfile(const CompilerInvocation& invocation)
        : invocation_(invocation),
          profiler_(!invocation.profile_output_path.empty()) {}

    [[nodiscard]] CompilationProfiler* profiler() noexcept {
        return &this->profiler_;
    }

    [[nodiscard]] base::Result<void> finish(base::Result<void> result) const {
        if (this->invocation_.profile_output_path.empty()) {
            return result;
        }
        const auto profile_result = this->profiler_.write_json(this->invocation_.profile_output_path);
        if (!profile_result && result) {
            return base::Result<void>::fail(profile_result.error());
        }
        return result;
    }

private:
    const CompilerInvocation& invocation_;
    CompilationProfiler profiler_;
};

} // namespace

base::Result<void> Compiler::run(const CompilerInvocation& invocation) const
{
    CompilerRunProfile run_profile(invocation);

    if (invocation.emit_kind == EmitKind::check) {
        auto cache_result = [&] {
            ScopedCompilationPhase phase(run_profile.profiler(), "incremental_cache.lookup");
            return try_reuse_incremental_check_cache(invocation);
        }();
        if (!cache_result) {
            return run_profile.finish(base::Result<void>::fail(cache_result.error()));
        }
        if (cache_result.value()) {
            return run_profile.finish(base::Result<void>::ok());
        }
    }

    base::SourceManager sources;
    base::DiagnosticSink diagnostics;

    if (invocation.emit_kind == EmitKind::tokens) {
        auto source_result = [&] {
            ScopedCompilationPhase phase(run_profile.profiler(), "source.read");
            return read_text_file(invocation.input_path);
        }();
        if (!source_result) {
            return run_profile.finish(base::Result<void>::fail(source_result.error()));
        }
        const base::SourceId source_id = sources.add_source(invocation.input_path.string(), source_result.take_value());
        lex::Lexer lexer(source_id, sources.text(source_id), diagnostics);
        auto token_result = [&] {
            ScopedCompilationPhase phase(run_profile.profiler(), "tokens.lex");
            return lexer.tokenize();
        }();
        if (!token_result) {
            print_diagnostics(sources, diagnostics, invocation.diagnostic_format);
            return run_profile.finish(base::Result<void>::fail(token_result.error()));
        }
        {
            ScopedCompilationPhase phase(run_profile.profiler(), "tokens.dump");
            std::cout << syntax::dump_tokens(token_result.value());
        }
        return run_profile.finish(base::Result<void>::ok());
    }

    ModuleLoader loader(invocation, sources, diagnostics, run_profile.profiler());
    auto ast_result = loader.load_root();

    if (!ast_result) {
        print_diagnostics(sources, diagnostics, invocation.diagnostic_format);
        return run_profile.finish(base::Result<void>::fail(
            diagnostics.diagnostics().empty() ? ast_result.error() : remap_diagnostic_loader_error(ast_result.error())
        ));
    }
    syntax::AstModule ast = ast_result.take_value();

    if (invocation.emit_kind == EmitKind::ast) {
        {
            ScopedCompilationPhase phase(run_profile.profiler(), "ast.dump");
            std::cout << syntax::dump_ast(ast);
        }
        return run_profile.finish(base::Result<void>::ok());
    }

    if (invocation.emit_kind == EmitKind::modules) {
        {
            ScopedCompilationPhase phase(run_profile.profiler(), "modules.dump");
            std::cout << "modules\n";
            for (const ModuleRecord& record : loader.modules()) {
                std::cout << "  " << record.name << " " << record.path.string() << "\n";
            }
        }
        return run_profile.finish(base::Result<void>::ok());
    }

    sema::SemanticOptions sema_options;
    sema_options.retain_generic_side_tables =
        emit_kind_requires_ir_lowering(invocation.emit_kind) ||
        invocation.emit_kind == EmitKind::typed;
    auto checked_result = [&] {
        ScopedCompilationPhase phase(run_profile.profiler(), "sema.analyze");
        sema::SemanticAnalyzer analyzer(ast, diagnostics, sema_options);
        return analyzer.analyze();
    }();
    if (!checked_result) {
        print_diagnostics(sources, diagnostics, invocation.diagnostic_format);
        return run_profile.finish(base::Result<void>::fail(checked_result.error()));
    }

    auto incremental_cache_result = [&] {
        ScopedCompilationPhase phase(run_profile.profiler(), "incremental_cache.write");
        return write_incremental_cache(invocation, sources, loader.modules(), checked_result.value());
    }();
    if (!incremental_cache_result) {
        return run_profile.finish(base::Result<void>::fail(incremental_cache_result.error()));
    }

    if (invocation.emit_kind == EmitKind::check) {
        return run_profile.finish(base::Result<void>::ok());
    }

    if (invocation.emit_kind == EmitKind::typed) {
        return run_profile.finish(base::Result<void>::ok());
    }

    if (invocation.emit_kind == EmitKind::checked) {
        {
            ScopedCompilationPhase phase(run_profile.profiler(), "checked.dump");
            std::cout << sema::dump_checked_module(checked_result.value());
        }
        return run_profile.finish(base::Result<void>::ok());
    }

    if (invocation.emit_kind == EmitKind::ir || invocation.emit_kind == EmitKind::llvm_ir) {
        auto ir_result = [&] {
            ScopedCompilationPhase phase(run_profile.profiler(), "ir.lower");
            return ir::lower_ast(ast, checked_result.value());
        }();
        if (!ir_result) {
            return run_profile.finish(base::Result<void>::fail(ir_result.error()));
        }
        auto pipeline_result = [&] {
            ScopedCompilationPhase phase(run_profile.profiler(), "ir.pass_pipeline");
            return ir::run_pass_pipeline(ir_result.value(), ir::PassPipelineOptions {
                invocation.optimization_level,
                true,
                true,
                true,
                true,
            });
        }();
        if (!pipeline_result) {
            return run_profile.finish(base::Result<void>::fail(pipeline_result.error()));
        }
        if (invocation.emit_kind == EmitKind::ir) {
            {
                ScopedCompilationPhase phase(run_profile.profiler(), "ir.dump");
                std::cout << ir::dump_module(ir_result.value());
            }
            return run_profile.finish(base::Result<void>::ok());
        }
        auto llvm_result = [&] {
            ScopedCompilationPhase phase(run_profile.profiler(), "llvm.emit_ir");
            return backend::emit_llvm_ir(backend::LlvmEmitRequest {
                &ir_result.value(),
                invocation.input_path.stem().string(),
            });
        }();
        if (!llvm_result) {
            return run_profile.finish(base::Result<void>::fail(llvm_result.error()));
        }
        {
            ScopedCompilationPhase phase(run_profile.profiler(), "llvm_ir.dump");
            std::cout << llvm_result.value().text;
        }
        return run_profile.finish(base::Result<void>::ok());
    }

    if (invocation.emit_kind == EmitKind::assembly ||
        invocation.emit_kind == EmitKind::object ||
        invocation.emit_kind == EmitKind::executable) {
        if (invocation.output_path.empty()) {
            return run_profile.finish(base::Result<void>::fail({base::ErrorCode::io_error, std::string(DRIVER_NATIVE_OUTPUT_REQUIRES_OUTPUT_PATH)}));
        }
        auto ir_result = [&] {
            ScopedCompilationPhase phase(run_profile.profiler(), "ir.lower");
            return ir::lower_ast(ast, checked_result.value());
        }();
        if (!ir_result) {
            return run_profile.finish(base::Result<void>::fail(ir_result.error()));
        }
        auto pipeline_result = [&] {
            ScopedCompilationPhase phase(run_profile.profiler(), "ir.pass_pipeline");
            return ir::run_pass_pipeline(ir_result.value(), ir::PassPipelineOptions {
                invocation.optimization_level,
                true,
                true,
                true,
                true,
            });
        }();
        if (!pipeline_result) {
            return run_profile.finish(base::Result<void>::fail(pipeline_result.error()));
        }
        auto llvm_result = [&] {
            ScopedCompilationPhase phase(run_profile.profiler(), "llvm.emit_ir");
            return backend::emit_llvm_ir(backend::LlvmEmitRequest {
                &ir_result.value(),
                invocation.input_path.stem().string(),
            });
        }();
        if (!llvm_result) {
            return run_profile.finish(base::Result<void>::fail(llvm_result.error()));
        }
        auto temp_ir_result = [&] {
            ScopedCompilationPhase phase(run_profile.profiler(), "llvm.write_temp");
            return write_temporary_llvm_file(llvm_result.value().text);
        }();
        if (!temp_ir_result) {
            return run_profile.finish(base::Result<void>::fail(temp_ir_result.error()));
        }
        NativeCompileRequest request;
        request.clang_path = invocation.clang_path;
        request.clang_args = invocation.clang_args;
        request.input_path = temp_ir_result.value();
        request.output_path = invocation.output_path;
        request.emit_kind = invocation.emit_kind;
        request.input_is_llvm_ir = true;
        auto native_result = [&] {
            ScopedCompilationPhase phase(run_profile.profiler(), "native.clang");
            return invoke_clang(request);
        }();
        std::error_code remove_error;
        std::filesystem::remove(temp_ir_result.value(), remove_error);
        if (!native_result) {
            return run_profile.finish(base::Result<void>::fail(native_result.error()));
        }
        return run_profile.finish(base::Result<void>::ok());
    }

    return run_profile.finish(base::Result<void>::fail({base::ErrorCode::codegen_error, std::string(DRIVER_UNSUPPORTED_EMISSION_MODE)}));
}

} // namespace aurex::driver
