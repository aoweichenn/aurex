#include "aurex/driver/compiler.hpp"

#include "aurex/base/diagnostic.hpp"
#include "aurex/base/source.hpp"
#include "aurex/base/text.hpp"
#include "aurex/backend/llvm_backend.hpp"
#include "aurex/driver/module_loader.hpp"
#include "aurex/driver/native_toolchain.hpp"
#include "aurex/driver/standard_library.hpp"
#include "aurex/ir/ir_dump.hpp"
#include "aurex/ir/lower_ast.hpp"
#include "aurex/ir/pass_pipeline.hpp"
#include "aurex/lex/lexer.hpp"
#include "aurex/sema/sema.hpp"
#include "aurex/syntax/ast_dump.hpp"

#ifdef AUREX_HAS_AURORA_BACKEND
#include "aurex/backend/aurora_backend.hpp"
#endif

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

    if (invocation.emit_kind == EmitKind::ir || invocation.emit_kind == EmitKind::llvm_ir) {
        if (invocation.emit_kind == EmitKind::llvm_ir &&
            invocation.backend == BackendKind::aurora) {
            return base::Result<void>::fail({
                base::ErrorCode::codegen_error,
                "--emit=llvm-ir requires the LLVM backend; use --backend llvm"
            });
        }
        auto ir_result = ir::lower_ast(ast_result.value(), checked_result.value());
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
#ifdef AUREX_HAS_AURORA_BACKEND
        if (invocation.backend == BackendKind::aurora) {
            if (invocation.output_path.empty()) {
                return base::Result<void>::fail({base::ErrorCode::io_error, "native output requires -o"});
            }
            auto ir_result = ir::lower_ast(ast_result.value(), checked_result.value());
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
            backend::AuroraEmitRequest aurora_req;
            aurora_req.module = &ir_result.value();
            aurora_req.module_name = invocation.input_path.stem().string();
            aurora_req.output_path = invocation.output_path.string();
            aurora_req.opt_level = invocation.optimization_level;

            if (invocation.emit_kind == EmitKind::assembly) {
                auto aurora_result = backend::emit_aurora_asm(aurora_req);
                if (!aurora_result) {
                    return base::Result<void>::fail(aurora_result.error());
                }
                auto write_result = write_file(invocation.output_path, aurora_result.value().text);
                if (!write_result) {
                    return base::Result<void>::fail(write_result.error());
                }
                return base::Result<void>::ok();
            }

            const std::filesystem::path obj_path =
                invocation.emit_kind == EmitKind::executable
                    ? (std::filesystem::temp_directory_path() /
                       ("aurex_aurora_" +
                        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                        ".o"))
                    : invocation.output_path;

            aurora_req.output_path = obj_path.string();
            auto aurora_result = backend::emit_aurora_obj(aurora_req);
            if (!aurora_result) {
                return base::Result<void>::fail(aurora_result.error());
            }

            if (invocation.emit_kind == EmitKind::object) {
                return base::Result<void>::ok();
            }

            NativeCompileRequest link_req;
            link_req.clang_path = invocation.clang_path;
            link_req.clang_args = invocation.clang_args;
            link_req.input_path = obj_path;
            link_req.output_path = invocation.output_path;
            link_req.emit_kind = EmitKind::executable;
            link_req.input_is_llvm_ir = false;
            if (invocation.use_standard_library) {
                const std::optional<StandardLibraryLayout> standard_library = find_standard_library(invocation);
                if (!standard_library) {
                    std::error_code remove_error;
                    std::filesystem::remove(obj_path, remove_error);
                    return base::Result<void>::fail({
                        base::ErrorCode::io_error,
                        "failed to locate Aurex standard library; set AUREX_STDLIB or pass --no-stdlib"
                    });
                }
                link_req.support_source_paths = standard_library_support_sources(
                    *standard_library,
                    invocation.standard_library_backend
                );
            }
            auto link_result = invoke_clang(link_req);
            std::error_code remove_error;
            std::filesystem::remove(obj_path, remove_error);
            if (!link_result) {
                return link_result;
            }
            return base::Result<void>::ok();
        }
#endif

        if (invocation.output_path.empty()) {
            return base::Result<void>::fail({base::ErrorCode::io_error, "native output requires -o"});
        }
        auto ir_result = ir::lower_ast(ast_result.value(), checked_result.value());
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
        if (invocation.use_standard_library && invocation.emit_kind == EmitKind::executable) {
            const std::optional<StandardLibraryLayout> standard_library = find_standard_library(invocation);
            if (!standard_library) {
                std::error_code remove_error;
                std::filesystem::remove(temp_ir_result.value(), remove_error);
                return base::Result<void>::fail({
                    base::ErrorCode::io_error,
                    "failed to locate Aurex standard library; set AUREX_STDLIB or pass --no-stdlib"
                });
            }
            request.support_source_paths = standard_library_support_sources(
                *standard_library,
                invocation.standard_library_backend
            );
        }
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

    return base::Result<void>::fail({base::ErrorCode::codegen_error, "unsupported emission mode"});
}

} // namespace aurex::driver
