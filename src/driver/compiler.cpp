#include <aurex/base/diagnostic.hpp>
#include <aurex/base/source.hpp>
#include <aurex/driver/diagnostic_renderer.hpp>
#include <aurex/driver/compiler.hpp>
#include <aurex/driver/driver_messages.hpp>
#include <aurex/driver/file_cache.hpp>
#include <aurex/driver/incremental_cache.hpp>
#include <aurex/driver/module_loader.hpp>
#include <aurex/driver/native_toolchain.hpp>
#include <aurex/driver/profile.hpp>
#include <aurex/ir/ir_dump.hpp>
#include <aurex/ir/lower_ast.hpp>
#include <aurex/ir/pass_pipeline.hpp>
#include <aurex/lex/lexer.hpp>
#include <aurex/query/diagnostics_query.hpp>
#include <aurex/sema/sema.hpp>
#include <aurex/syntax/ast_dump.hpp>
#include <aurex/syntax/lossless.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>

namespace aurex::driver {

namespace {

[[nodiscard]] bool emit_kind_requires_ir_lowering(const EmitKind emit_kind) noexcept
{
    return emit_kind == EmitKind::ir || emit_kind == EmitKind::llvm_ir || emit_kind == EmitKind::assembly
        || emit_kind == EmitKind::object || emit_kind == EmitKind::executable;
}

[[nodiscard]] base::Result<void> write_file(const std::filesystem::path& path, const std::string_view text)
{
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

[[nodiscard]] base::Result<std::filesystem::path> write_temporary_llvm_file(const std::string_view text)
{
    const std::filesystem::path path = std::filesystem::temp_directory_path()
        / ("aurex_llvm_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".ll");
    const auto write_result = write_file(path, text);
    if (!write_result) {
        return base::Result<std::filesystem::path>::fail(write_result.error());
    }
    return base::Result<std::filesystem::path>::ok(path);
}

[[nodiscard]] base::Result<LlvmIrOutput> emit_llvm_ir(
    const LlvmIrEmitter emitter, const ir::Module& module, std::string module_name)
{
    if (emitter == nullptr) {
        return base::Result<LlvmIrOutput>::fail({
            base::ErrorCode::codegen_error,
            std::string(DRIVER_LLVM_BACKEND_UNAVAILABLE),
        });
    }
    return emitter(LlvmIrEmitRequest{&module, std::move(module_name)});
}

[[nodiscard]] base::Error remap_diagnostic_loader_error(const base::Error& error)
{
    return {
        error.code == base::ErrorCode::io_error ? base::ErrorCode::parse_error : error.code,
        error.message,
    };
}

class CompilerRunProfile final {
public:
    explicit CompilerRunProfile(const CompilerInvocation& invocation)
        : invocation_(invocation), profiler_(!invocation.profile_output_path.empty())
    {
    }

    [[nodiscard]] CompilationProfiler* profiler() noexcept
    {
        return &this->profiler_;
    }

    [[nodiscard]] base::Result<void> finish(base::Result<void> result) const
    {
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

Compiler::Compiler(const LlvmIrEmitter llvm_ir_emitter) noexcept : llvm_ir_emitter_(llvm_ir_emitter)
{
}

base::Result<void> Compiler::run(const CompilerInvocation& invocation) const
{
    CompilerRunProfile run_profile(invocation);

    if (invocation.emit_kind == EmitKind::check) {
        auto cache_result = [&] {
            ScopedCompilationPhase phase(run_profile.profiler(), "incremental_cache.lookup");
            return try_reuse_incremental_check_cache(invocation, run_profile.profiler());
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

    if (invocation.emit_kind == EmitKind::tokens || invocation.emit_kind == EmitKind::lossless) {
        auto source_result = [&] {
            ScopedCompilationPhase phase(run_profile.profiler(), "source.read");
            return read_text_file(invocation.input_path);
        }();
        if (!source_result) {
            return run_profile.finish(base::Result<void>::fail(source_result.error()));
        }
        const base::SourceId source_id = sources.add_source(invocation.input_path.string(), source_result.take_value());
        lex::LexerOptions lexer_options;
        lexer_options.emit_trivia_tokens = invocation.emit_kind == EmitKind::lossless;
        lex::Lexer lexer(source_id, sources.text(source_id), diagnostics, lexer_options);
        auto token_result = [&] {
            ScopedCompilationPhase phase(run_profile.profiler(), "tokens.lex");
            return lexer.tokenize();
        }();
        if (!token_result) {
            render_diagnostics(std::cerr, sources, diagnostics, invocation.diagnostic_format);
            return run_profile.finish(base::Result<void>::fail(token_result.error()));
        }
        {
            ScopedCompilationPhase phase(
                run_profile.profiler(), invocation.emit_kind == EmitKind::lossless ? "lossless.dump" : "tokens.dump");
            if (invocation.emit_kind == EmitKind::lossless) {
                const syntax::LosslessSyntaxTree tree = syntax::build_lossless_syntax_tree(token_result.value().span());
                std::cout << syntax::dump_lossless_syntax_tree(tree);
            } else {
                std::cout << syntax::dump_tokens(token_result.value());
            }
        }
        return run_profile.finish(base::Result<void>::ok());
    }

    ModuleLoader loader(invocation, sources, diagnostics, run_profile.profiler());
    auto ast_result = loader.load_root();

    if (!ast_result) {
        render_diagnostics(std::cerr, sources, diagnostics, invocation.diagnostic_format);
        return run_profile.finish(base::Result<void>::fail(diagnostics.diagnostics().empty()
                ? ast_result.error()
                : remap_diagnostic_loader_error(ast_result.error())));
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
        emit_kind_requires_ir_lowering(invocation.emit_kind) || invocation.emit_kind == EmitKind::typed;
    auto checked_result = [&] {
        ScopedCompilationPhase phase(run_profile.profiler(), "sema.analyze");
        sema::SemanticAnalyzer analyzer(ast, diagnostics, sema_options);
        return analyzer.analyze();
    }();
    if (!checked_result) {
        render_diagnostics(std::cerr, sources, diagnostics, invocation.diagnostic_format);
        return run_profile.finish(base::Result<void>::fail(checked_result.error()));
    }

    auto incremental_cache_result = [&] {
        ScopedCompilationPhase phase(run_profile.profiler(), "incremental_cache.write");
        return write_incremental_cache(
            invocation, sources, loader.modules(), ast, checked_result.value(), run_profile.profiler());
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
            return ir::run_pass_pipeline(ir_result.value(),
                ir::PassPipelineOptions{
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
            return emit_llvm_ir(this->llvm_ir_emitter_, ir_result.value(), invocation.input_path.stem().string());
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

    if (invocation.emit_kind == EmitKind::assembly || invocation.emit_kind == EmitKind::object
        || invocation.emit_kind == EmitKind::executable) {
        if (invocation.output_path.empty()) {
            return run_profile.finish(base::Result<void>::fail(
                {base::ErrorCode::io_error, std::string(DRIVER_NATIVE_OUTPUT_REQUIRES_OUTPUT_PATH)}));
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
            return ir::run_pass_pipeline(ir_result.value(),
                ir::PassPipelineOptions{
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
            return emit_llvm_ir(this->llvm_ir_emitter_, ir_result.value(), invocation.input_path.stem().string());
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

    return run_profile.finish(
        base::Result<void>::fail({base::ErrorCode::codegen_error, std::string(DRIVER_UNSUPPORTED_EMISSION_MODE)}));
}

} // namespace aurex::driver
