#include <aurex/driver/compiler.hpp>

#include <aurex/base/diagnostic.hpp>
#include <aurex/base/source.hpp>
#include <aurex/backend/llvm_backend.hpp>
#include <aurex/driver/driver_messages.hpp>
#include <aurex/driver/module_loader.hpp>
#include <aurex/driver/file_cache.hpp>
#include <aurex/driver/native_toolchain.hpp>
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

namespace aurex::driver {

namespace {

constexpr base::usize DRIVER_MAX_PRINTED_DIAGNOSTICS = 128;

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

void print_diagnostics(const base::SourceManager& sources, const base::DiagnosticSink& diagnostics) {
    const std::span<const base::Diagnostic> all = diagnostics.diagnostics();
    const base::usize count = std::min<base::usize>(all.size(), DRIVER_MAX_PRINTED_DIAGNOSTICS);
    for (base::usize index = 0; index < count; ++index) {
        const base::Diagnostic& diagnostic = all[index];
        const base::SourceFile& file = sources.get(diagnostic.range.source);
        const base::LineColumn location = file.line_column(diagnostic.range.begin);
        std::cerr << file.path() << ":" << location.line << ":" << location.column << ": "
                  << base::severity_name(diagnostic.severity) << ": "
                  << diagnostic.message << "\n";

        const std::string_view text = file.text();
        const base::SourceLineExtent line = file.line_extent(diagnostic.range.begin);
        const std::string_view source_line = text.substr(line.begin, line.end - line.begin);
        if (!source_line.empty()) {
            std::cerr << "  " << source_line << "\n";
            std::cerr << "  ";
            const base::usize caret_column = std::min(diagnostic.range.begin, line.end) - line.begin;
            for (base::usize i = 0; i < caret_column; ++i) {
                std::cerr << (source_line[i] == '\t' ? '\t' : ' ');
            }
            const base::usize caret_count = diagnostic.range.empty() ? 1 : diagnostic.range.length();
            for (base::usize i = 0; i < caret_count && caret_column + i < source_line.size(); ++i) {
                std::cerr << '^';
            }
            std::cerr << "\n";
        }
    }
    if (all.size() > DRIVER_MAX_PRINTED_DIAGNOSTICS) {
        std::cerr << "error: too many diagnostics; suppressing "
                  << (all.size() - DRIVER_MAX_PRINTED_DIAGNOSTICS)
                  << " additional diagnostics\n";
    }
}

} // namespace

base::Result<void> Compiler::run(const CompilerInvocation& invocation) {
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

    if (invocation.emit_kind == EmitKind::ast) {
        std::cout << syntax::dump_ast(ast_result.value());
        return base::Result<void>::ok();
    }

    if (invocation.emit_kind == EmitKind::modules) {
        std::cout << "modules\n";
        for (const ModuleRecord& record : loader.modules()) {
            std::cout << "  " << record.name << " " << record.path.string() << "\n";
        }
        return base::Result<void>::ok();
    }

    sema::SemanticAnalyzer analyzer(ast_result.take_value(), diagnostics);
    auto checked_result = analyzer.analyze();
    if (!checked_result) {
        print_diagnostics(sources, diagnostics);
        return base::Result<void>::fail(checked_result.error());
    }

    if (invocation.emit_kind == EmitKind::check) {
        return base::Result<void>::ok();
    }

    if (invocation.emit_kind == EmitKind::checked) {
        std::cout << sema::dump_checked_module(checked_result.value());
        return base::Result<void>::ok();
    }

    if (invocation.emit_kind == EmitKind::ir || invocation.emit_kind == EmitKind::llvm_ir) {
        auto ir_result = ir::lower_ast(checked_result.value().normalized_ast.value(), checked_result.value());
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
        auto ir_result = ir::lower_ast(checked_result.value().normalized_ast.value(), checked_result.value());
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
