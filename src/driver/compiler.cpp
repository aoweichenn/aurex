#include <aurex/base/diagnostic.hpp>
#include <aurex/base/source.hpp>
#include <aurex/driver/compiler.hpp>
#include <aurex/driver/diagnostic_renderer.hpp>
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
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace aurex::driver {

namespace {

[[nodiscard]] bool emit_kind_requires_ir_lowering(const EmitKind emit_kind) noexcept
{
    return emit_kind == EmitKind::ir || emit_kind == EmitKind::llvm_ir || emit_kind == EmitKind::assembly
        || emit_kind == EmitKind::object || emit_kind == EmitKind::executable;
}

[[nodiscard]] bool emit_kind_is_native_artifact(const EmitKind emit_kind) noexcept
{
    return emit_kind == EmitKind::assembly || emit_kind == EmitKind::object || emit_kind == EmitKind::executable;
}

[[nodiscard]] ir::PassPipelineOptions make_pass_pipeline_options(
    const ir::OptimizationLevel optimization_level) noexcept
{
    return ir::PassPipelineOptions{
        optimization_level,
        true,
        true,
        true,
        true,
    };
}

[[nodiscard]] sema::SemanticOptions make_semantic_options(const EmitKind emit_kind) noexcept
{
    sema::SemanticOptions sema_options;
    sema_options.retain_generic_side_tables = emit_kind_requires_ir_lowering(emit_kind) || emit_kind == EmitKind::typed;
    return sema_options;
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

class TemporaryFile final {
public:
    explicit TemporaryFile(std::filesystem::path path) noexcept : path_(std::move(path))
    {
    }

    TemporaryFile(const TemporaryFile&) = delete;
    TemporaryFile& operator=(const TemporaryFile&) = delete;

    TemporaryFile(TemporaryFile&& other) noexcept : path_(std::move(other.path_))
    {
        other.path_.clear();
    }

    TemporaryFile& operator=(TemporaryFile&& other) noexcept
    {
        if (this != &other) {
            this->remove();
            this->path_ = std::move(other.path_);
            other.path_.clear();
        }
        return *this;
    }

    ~TemporaryFile()
    {
        this->remove();
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept
    {
        return this->path_;
    }

private:
    void remove() noexcept
    {
        if (this->path_.empty()) {
            return;
        }
        std::error_code remove_error;
        std::filesystem::remove(this->path_, remove_error);
        this->path_.clear();
    }

    std::filesystem::path path_;
};

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

struct LoadedFrontend {
    syntax::AstModule ast;
    std::vector<ModuleRecord> modules;
};

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

[[nodiscard]] base::Result<bool> try_reuse_check_cache(
    const CompilerInvocation& invocation, CompilationProfiler* const profiler)
{
    if (invocation.emit_kind != EmitKind::check) {
        return base::Result<bool>::ok(false);
    }
    ScopedCompilationPhase phase(profiler, "incremental_cache.lookup");
    return try_reuse_incremental_check_cache(invocation, profiler);
}

[[nodiscard]] base::Result<void> emit_token_or_lossless_output(const CompilerInvocation& invocation,
    base::SourceManager& sources, base::DiagnosticSink& diagnostics, CompilationProfiler* const profiler)
{
    auto source_result = [&] {
        ScopedCompilationPhase phase(profiler, "source.read");
        return read_text_file(invocation.input_path);
    }();
    if (!source_result) {
        return base::Result<void>::fail(source_result.error());
    }

    const base::SourceId source_id = sources.add_source(invocation.input_path.string(), source_result.take_value());
    lex::LexerOptions lexer_options;
    lexer_options.emit_trivia_tokens = invocation.emit_kind == EmitKind::lossless;
    lex::Lexer lexer(source_id, sources.text(source_id), diagnostics, lexer_options);
    auto token_result = [&] {
        ScopedCompilationPhase phase(profiler, "tokens.lex");
        return lexer.tokenize();
    }();
    if (!token_result) {
        render_diagnostics(std::cerr, sources, diagnostics, invocation.diagnostic_format);
        return base::Result<void>::fail(token_result.error());
    }

    ScopedCompilationPhase phase(
        profiler, invocation.emit_kind == EmitKind::lossless ? "lossless.dump" : "tokens.dump");
    if (invocation.emit_kind == EmitKind::lossless) {
        const syntax::LosslessSyntaxTree tree = syntax::build_lossless_syntax_tree(token_result.value().span());
        std::cout << syntax::dump_lossless_syntax_tree(tree);
        return base::Result<void>::ok();
    }

    std::cout << syntax::dump_tokens(token_result.value());
    return base::Result<void>::ok();
}

[[nodiscard]] base::Result<LoadedFrontend> load_frontend_modules(const CompilerInvocation& invocation,
    base::SourceManager& sources, base::DiagnosticSink& diagnostics, CompilationProfiler* const profiler)
{
    ModuleLoader loader(invocation, sources, diagnostics, profiler);
    auto ast_result = loader.load_root();
    if (!ast_result) {
        return base::Result<LoadedFrontend>::fail(
            diagnostics.diagnostics().empty() ? ast_result.error() : remap_diagnostic_loader_error(ast_result.error()));
    }

    auto module_records = loader.modules();
    LoadedFrontend frontend{
        ast_result.take_value(),
        std::vector<ModuleRecord>(module_records.begin(), module_records.end()),
    };
    return base::Result<LoadedFrontend>::ok(std::move(frontend));
}

[[nodiscard]] base::Result<void> dump_ast_output(const syntax::AstModule& ast, CompilationProfiler* const profiler)
{
    ScopedCompilationPhase phase(profiler, "ast.dump");
    std::cout << syntax::dump_ast(ast);
    return base::Result<void>::ok();
}

[[nodiscard]] base::Result<void> dump_module_graph_output(
    const std::vector<ModuleRecord>& modules, CompilationProfiler* const profiler)
{
    ScopedCompilationPhase phase(profiler, "modules.dump");
    std::cout << "modules\n";
    for (const ModuleRecord& record : modules) {
        std::cout << "  " << record.name << " " << record.path.string() << "\n";
    }
    return base::Result<void>::ok();
}

[[nodiscard]] base::Result<sema::CheckedModule> run_semantic_analysis(syntax::AstModule& ast,
    base::DiagnosticSink& diagnostics, const EmitKind emit_kind, CompilationProfiler* const profiler)
{
    ScopedCompilationPhase phase(profiler, "sema.analyze");
    sema::SemanticAnalyzer analyzer(ast, diagnostics, make_semantic_options(emit_kind));
    return analyzer.analyze();
}

[[nodiscard]] base::Result<void> write_checked_incremental_cache(const CompilerInvocation& invocation,
    const base::SourceManager& sources, const std::vector<ModuleRecord>& modules, const syntax::AstModule& ast,
    const sema::CheckedModule& checked, CompilationProfiler* const profiler)
{
    ScopedCompilationPhase phase(profiler, "incremental_cache.write");
    return write_incremental_cache(
        invocation, sources, std::span<const ModuleRecord>(modules.data(), modules.size()), ast, checked, profiler);
}

[[nodiscard]] base::Result<void> dump_checked_output(
    const sema::CheckedModule& checked, CompilationProfiler* const profiler)
{
    ScopedCompilationPhase phase(profiler, "checked.dump");
    std::cout << sema::dump_checked_module(checked);
    return base::Result<void>::ok();
}

[[nodiscard]] base::Result<ir::Module> lower_and_optimize_ir(const CompilerInvocation& invocation,
    const syntax::AstModule& ast, const sema::CheckedModule& checked, CompilationProfiler* const profiler)
{
    auto ir_result = [&] {
        ScopedCompilationPhase phase(profiler, "ir.lower");
        return ir::lower_ast(ast, checked);
    }();
    if (!ir_result) {
        return base::Result<ir::Module>::fail(ir_result.error());
    }

    auto pipeline_result = [&] {
        ScopedCompilationPhase phase(profiler, "ir.pass_pipeline");
        return ir::run_pass_pipeline(ir_result.value(), make_pass_pipeline_options(invocation.optimization_level));
    }();
    if (!pipeline_result) {
        return base::Result<ir::Module>::fail(pipeline_result.error());
    }

    return base::Result<ir::Module>::ok(ir_result.take_value());
}

[[nodiscard]] base::Result<std::string> emit_llvm_ir_text(
    const LlvmIrEmitter emitter, const ir::Module& module, std::string module_name, CompilationProfiler* const profiler)
{
    auto llvm_result = [&] {
        ScopedCompilationPhase phase(profiler, "llvm.emit_ir");
        return emit_llvm_ir(emitter, module, std::move(module_name));
    }();
    if (!llvm_result) {
        return base::Result<std::string>::fail(llvm_result.error());
    }

    LlvmIrOutput output = llvm_result.take_value();
    return base::Result<std::string>::ok(std::move(output.text));
}

[[nodiscard]] NativeCompileRequest make_native_compile_request(
    const CompilerInvocation& invocation, const std::filesystem::path& input_path)
{
    NativeCompileRequest request;
    request.clang_path = invocation.clang_path;
    request.clang_args = invocation.clang_args;
    request.input_path = input_path;
    request.output_path = invocation.output_path;
    request.emit_kind = invocation.emit_kind;
    request.input_is_llvm_ir = true;
    return request;
}

[[nodiscard]] base::Result<void> emit_ir_or_llvm_output(const CompilerInvocation& invocation,
    const syntax::AstModule& ast, const sema::CheckedModule& checked, const LlvmIrEmitter llvm_ir_emitter,
    CompilationProfiler* const profiler)
{
    auto ir_result = lower_and_optimize_ir(invocation, ast, checked, profiler);
    if (!ir_result) {
        return base::Result<void>::fail(ir_result.error());
    }

    if (invocation.emit_kind == EmitKind::ir) {
        ScopedCompilationPhase phase(profiler, "ir.dump");
        std::cout << ir::dump_module(ir_result.value());
        return base::Result<void>::ok();
    }

    auto llvm_result =
        emit_llvm_ir_text(llvm_ir_emitter, ir_result.value(), invocation.input_path.stem().string(), profiler);
    if (!llvm_result) {
        return base::Result<void>::fail(llvm_result.error());
    }

    ScopedCompilationPhase phase(profiler, "llvm_ir.dump");
    std::cout << llvm_result.value();
    return base::Result<void>::ok();
}

[[nodiscard]] base::Result<void> emit_native_output(const CompilerInvocation& invocation, const syntax::AstModule& ast,
    const sema::CheckedModule& checked, const LlvmIrEmitter llvm_ir_emitter, CompilationProfiler* const profiler)
{
    if (invocation.output_path.empty()) {
        return base::Result<void>::fail(
            {base::ErrorCode::io_error, std::string(DRIVER_NATIVE_OUTPUT_REQUIRES_OUTPUT_PATH)});
    }

    auto ir_result = lower_and_optimize_ir(invocation, ast, checked, profiler);
    if (!ir_result) {
        return base::Result<void>::fail(ir_result.error());
    }

    auto llvm_result =
        emit_llvm_ir_text(llvm_ir_emitter, ir_result.value(), invocation.input_path.stem().string(), profiler);
    if (!llvm_result) {
        return base::Result<void>::fail(llvm_result.error());
    }

    auto temp_ir_result = [&] {
        ScopedCompilationPhase phase(profiler, "llvm.write_temp");
        return write_temporary_llvm_file(llvm_result.value());
    }();
    if (!temp_ir_result) {
        return base::Result<void>::fail(temp_ir_result.error());
    }

    TemporaryFile temp_ir(temp_ir_result.take_value());
    const NativeCompileRequest request = make_native_compile_request(invocation, temp_ir.path());
    auto native_result = [&] {
        ScopedCompilationPhase phase(profiler, "native.clang");
        return invoke_clang(request);
    }();
    if (!native_result) {
        return base::Result<void>::fail(native_result.error());
    }

    return base::Result<void>::ok();
}

} // namespace

Compiler::Compiler(const LlvmIrEmitter llvm_ir_emitter) noexcept : llvm_ir_emitter_(llvm_ir_emitter)
{
}

base::Result<void> Compiler::run(const CompilerInvocation& invocation) const
{
    CompilerRunProfile run_profile(invocation);

    auto cache_result = try_reuse_check_cache(invocation, run_profile.profiler());
    if (!cache_result) {
        return run_profile.finish(base::Result<void>::fail(cache_result.error()));
    }
    if (cache_result.value()) {
        return run_profile.finish(base::Result<void>::ok());
    }

    base::SourceManager sources;
    base::DiagnosticSink diagnostics;

    if (invocation.emit_kind == EmitKind::tokens || invocation.emit_kind == EmitKind::lossless) {
        return run_profile.finish(
            emit_token_or_lossless_output(invocation, sources, diagnostics, run_profile.profiler()));
    }

    auto frontend_result = load_frontend_modules(invocation, sources, diagnostics, run_profile.profiler());
    if (!frontend_result) {
        render_diagnostics(std::cerr, sources, diagnostics, invocation.diagnostic_format);
        return run_profile.finish(base::Result<void>::fail(frontend_result.error()));
    }
    LoadedFrontend frontend = frontend_result.take_value();

    if (invocation.emit_kind == EmitKind::ast) {
        return run_profile.finish(dump_ast_output(frontend.ast, run_profile.profiler()));
    }

    if (invocation.emit_kind == EmitKind::modules) {
        return run_profile.finish(dump_module_graph_output(frontend.modules, run_profile.profiler()));
    }

    auto checked_result =
        run_semantic_analysis(frontend.ast, diagnostics, invocation.emit_kind, run_profile.profiler());
    if (!checked_result) {
        render_diagnostics(std::cerr, sources, diagnostics, invocation.diagnostic_format);
        return run_profile.finish(base::Result<void>::fail(checked_result.error()));
    }
    sema::CheckedModule checked = checked_result.take_value();

    auto incremental_cache_result = write_checked_incremental_cache(
        invocation, sources, frontend.modules, frontend.ast, checked, run_profile.profiler());
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
        return run_profile.finish(dump_checked_output(checked, run_profile.profiler()));
    }

    if (invocation.emit_kind == EmitKind::ir || invocation.emit_kind == EmitKind::llvm_ir) {
        return run_profile.finish(
            emit_ir_or_llvm_output(invocation, frontend.ast, checked, this->llvm_ir_emitter_, run_profile.profiler()));
    }

    if (emit_kind_is_native_artifact(invocation.emit_kind)) {
        return run_profile.finish(
            emit_native_output(invocation, frontend.ast, checked, this->llvm_ir_emitter_, run_profile.profiler()));
    }

    return run_profile.finish(
        base::Result<void>::fail({base::ErrorCode::codegen_error, std::string(DRIVER_UNSUPPORTED_EMISSION_MODE)}));
}

} // namespace aurex::driver
