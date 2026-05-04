#include "aurex/driver/compiler.hpp"

#include "aurex/base/diagnostic.hpp"
#include "aurex/base/source.hpp"
#include "aurex/base/text.hpp"
#include "aurex/backend/codegen_backend.hpp"
#include "aurex/driver/module_loader.hpp"
#include "aurex/driver/native_toolchain.hpp"
#include "aurex/driver/standard_library.hpp"
#include "aurex/ir/ir_dump.hpp"
#include "aurex/ir/lower_ast.hpp"
#include "aurex/ir/pass_pipeline.hpp"
#include "aurex/lex/lexer.hpp"
#include "aurex/sema/sema.hpp"
#include "aurex/syntax/ast_dump.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace aurex::driver {

namespace {

[[nodiscard]] base::Result<std::string> read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return base::Result<std::string>::fail({base::ErrorCode::io_error, "failed to open input file"});
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return base::Result<std::string>::ok(buffer.str());
}

[[nodiscard]] base::Result<void> write_file(const std::filesystem::path& path, const std::string_view text) {
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        return base::Result<void>::fail({base::ErrorCode::io_error, "failed to open output file"});
    }
    output << text;
    if (!output) {
        return base::Result<void>::fail({base::ErrorCode::io_error, "failed to write output file"});
    }
    return base::Result<void>::ok();
}

void print_diagnostics(const base::SourceManager& sources, const base::DiagnosticSink& diagnostics) {
    for (const base::Diagnostic& diagnostic : diagnostics.diagnostics()) {
        const base::SourceFile& file = sources.get(diagnostic.range.source);
        const base::LineColumn location = base::line_column(file.text(), diagnostic.range.begin);
        std::cerr << file.path() << ":" << location.line << ":" << location.column << ": "
                  << base::severity_name(diagnostic.severity) << ": "
                  << diagnostic.message << "\n";

        const std::string_view text = file.text();
        base::usize line_begin = diagnostic.range.begin;
        while (line_begin > 0 && text[line_begin - 1] != '\n') {
            --line_begin;
        }
        base::usize line_end = diagnostic.range.begin;
        while (line_end < text.size() && text[line_end] != '\n') {
            ++line_end;
        }
        const std::string_view source_line = text.substr(line_begin, line_end - line_begin);
        if (!source_line.empty()) {
            std::cerr << "  " << source_line << "\n";
            std::cerr << "  ";
            const base::usize caret_column = diagnostic.range.begin - line_begin;
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
}

[[nodiscard]] base::Result<ir::Module> generate_ir(
    const syntax::AstModule& ast,
    const sema::CheckedModule& checked,
    const ir::OptimizationLevel opt_level)
{
    auto ir_result = ir::lower_ast(ast, checked);
    if (!ir_result) return base::Result<ir::Module>::fail(ir_result.error());
    auto pipeline_result = ir::run_pass_pipeline(ir_result.value(), ir::PassPipelineOptions{
        opt_level, true, true, true, true
    });
    if (!pipeline_result) return base::Result<ir::Module>::fail(pipeline_result.error());
    return base::Result<ir::Module>::ok(std::move(ir_result.value()));
}

[[nodiscard]] base::Result<void> emit_with_backend(
    const CompilerInvocation& invocation,
    const ir::Module& ir_module,
    backend::CodeGenBackend& backend)
{
    switch (invocation.emit_kind) {
    case EmitKind::assembly: {
        auto asm_result = backend.emit_assembly(ir_module);
        if (!asm_result) return base::Result<void>::fail(asm_result.error());
        return write_file(invocation.output_path, asm_result.value());
    }
    case EmitKind::object: {
        return backend.emit_object(ir_module, invocation.output_path.string());
    }
    case EmitKind::executable: {
        auto obj_path = std::filesystem::temp_directory_path() /
            ("aurex_obj_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".o");
        auto obj_res = backend.emit_object(ir_module, obj_path.string());
        if (!obj_res) return obj_res;

        NativeCompileRequest nc;
        nc.clang_path = invocation.clang_path;
        nc.clang_args = invocation.clang_args;
        nc.input_path = obj_path;
        nc.output_path = invocation.output_path;
        nc.emit_kind = EmitKind::executable;
        nc.input_is_llvm_ir = false;

        if (invocation.use_standard_library) {
            const auto stdlib = find_standard_library(invocation);
            if (!stdlib) {
                std::error_code ec;
                std::filesystem::remove(obj_path, ec);
                return base::Result<void>::fail({
                    base::ErrorCode::io_error,
                    "failed to locate Aurex standard library; set AUREX_STDLIB or pass --no-stdlib"
                });
            }
            nc.support_source_paths = standard_library_support_sources(
                *stdlib, invocation.standard_library_backend);
        }

        auto cres = invoke_clang(nc);
        std::error_code ec;
        std::filesystem::remove(obj_path, ec);
        return cres;
    }
    default:
        return base::Result<void>::fail({base::ErrorCode::codegen_error, "unsupported emission mode"});
    }
}

} // namespace

base::Result<void> Compiler::run(const CompilerInvocation& invocation) {
    base::SourceManager sources;
    base::DiagnosticSink diagnostics;

    if (invocation.emit_kind == EmitKind::tokens) {
        auto source_result = read_file(invocation.input_path);
        if (!source_result) {
            return base::Result<void>::fail(source_result.error());
        }
        const base::SourceId source_id = sources.add_source(invocation.input_path.string(), std::move(source_result.value()));
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

    sema::SemanticAnalyzer analyzer(ast_result.value(), diagnostics);
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

    auto ir_result = generate_ir(ast_result.value(), checked_result.value(), invocation.optimization_level);
    if (!ir_result) {
        return base::Result<void>::fail(ir_result.error());
    }

    auto backend = backend::create_codegen_backend(invocation.backend);
    if (!backend) {
        return base::Result<void>::fail({base::ErrorCode::codegen_error, "unknown backend"});
    }

    if (invocation.emit_kind == EmitKind::ir) {
        std::cout << ir::dump_module(ir_result.value());
        return base::Result<void>::ok();
    }

    if (invocation.emit_kind == EmitKind::llvm_ir) {
        auto ir_text = backend->emit_ir_text(ir_result.value());
        if (!ir_text) return base::Result<void>::fail(ir_text.error());
        std::cout << ir_text.value();
        return base::Result<void>::ok();
    }

    if (invocation.output_path.empty()) {
        return base::Result<void>::fail({base::ErrorCode::io_error, "native output requires -o"});
    }

    return emit_with_backend(invocation, ir_result.value(), *backend);
}

} // namespace aurex::driver
