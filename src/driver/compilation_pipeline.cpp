#include "compilation_pipeline.hpp"

#include <aurex/driver/driver_messages.hpp>
#include <aurex/driver/invocation.hpp>
#include <aurex/driver/native_toolchain.hpp>
#include <aurex/driver/profile.hpp>
#include <aurex/ir/ir_dump.hpp>
#include <aurex/ir/lower_ast.hpp>
#include <aurex/ir/pass_pipeline.hpp>
#include <aurex/sema/checked_module.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "compilation_session.hpp"
#include "frontend_pipeline.hpp"

namespace aurex::driver {

namespace {

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
    TemporaryFile(TemporaryFile&&) = delete;
    TemporaryFile& operator=(TemporaryFile&&) = delete;

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

[[nodiscard]] base::Result<void> emit_ir_or_llvm_output(
    CompilationSession& session, const syntax::AstModule& ast, const sema::CheckedModule& checked)
{
    auto ir_result = lower_and_optimize_ir(session.invocation(), ast, checked, session.profiler());
    if (!ir_result) {
        return base::Result<void>::fail(ir_result.error());
    }

    if (session.invocation().emit_kind == EmitKind::ir) {
        ScopedCompilationPhase phase(session.profiler(), "ir.dump");
        std::cout << ir::dump_module(ir_result.value());
        return base::Result<void>::ok();
    }

    auto llvm_result = emit_llvm_ir_text(session.llvm_ir_emitter(), ir_result.value(),
        session.invocation().input_path.stem().string(), session.profiler());
    if (!llvm_result) {
        return base::Result<void>::fail(llvm_result.error());
    }

    ScopedCompilationPhase phase(session.profiler(), "llvm_ir.dump");
    std::cout << llvm_result.value();
    return base::Result<void>::ok();
}

[[nodiscard]] base::Result<void> emit_native_output(
    CompilationSession& session, const syntax::AstModule& ast, const sema::CheckedModule& checked)
{
    if (session.invocation().output_path.empty()) {
        return base::Result<void>::fail(
            {base::ErrorCode::io_error, std::string(DRIVER_NATIVE_OUTPUT_REQUIRES_OUTPUT_PATH)});
    }

    auto ir_result = lower_and_optimize_ir(session.invocation(), ast, checked, session.profiler());
    if (!ir_result) {
        return base::Result<void>::fail(ir_result.error());
    }

    auto llvm_result = emit_llvm_ir_text(session.llvm_ir_emitter(), ir_result.value(),
        session.invocation().input_path.stem().string(), session.profiler());
    if (!llvm_result) {
        return base::Result<void>::fail(llvm_result.error());
    }

    auto temp_ir_result = [&] {
        ScopedCompilationPhase phase(session.profiler(), "llvm.write_temp");
        return write_temporary_llvm_file(llvm_result.value());
    }();
    if (!temp_ir_result) {
        return base::Result<void>::fail(temp_ir_result.error());
    }

    TemporaryFile temp_ir(temp_ir_result.take_value());
    const NativeCompileRequest request = make_native_compile_request(session.invocation(), temp_ir.path());
    auto native_result = [&] {
        ScopedCompilationPhase phase(session.profiler(), "native.clang");
        return invoke_clang(request);
    }();
    if (!native_result) {
        return base::Result<void>::fail(native_result.error());
    }

    return base::Result<void>::ok();
}

} // namespace

CompilationPipeline::CompilationPipeline(
    const CompilerInvocation& invocation, const LlvmIrEmitter llvm_ir_emitter) noexcept
    : invocation_(invocation), llvm_ir_emitter_(llvm_ir_emitter)
{
}

base::Result<void> CompilationPipeline::run()
{
    CompilationSession session(this->invocation_, this->llvm_ir_emitter_);
    FrontendPipeline frontend_pipeline(session);

    auto cache_result = frontend_pipeline.try_reuse_check_cache();
    if (!cache_result) {
        return session.finish(base::Result<void>::fail(cache_result.error()));
    }
    if (cache_result.value()) {
        return session.finish(base::Result<void>::ok());
    }

    if (this->invocation_.emit_kind == EmitKind::tokens || this->invocation_.emit_kind == EmitKind::lossless) {
        return session.finish(frontend_pipeline.emit_token_or_lossless_output());
    }

    auto frontend_result = frontend_pipeline.load_modules();
    if (!frontend_result) {
        session.render_diagnostics_to_stderr();
        return session.finish(base::Result<void>::fail(frontend_result.error()));
    }
    FrontendModuleOutput frontend = frontend_result.take_value();

    if (this->invocation_.emit_kind == EmitKind::ast) {
        return session.finish(frontend_pipeline.dump_ast_output(frontend.ast));
    }

    if (this->invocation_.emit_kind == EmitKind::modules) {
        return session.finish(frontend_pipeline.dump_module_graph_output(frontend.modules));
    }

    auto checked_result = frontend_pipeline.run_semantic_analysis(frontend.ast);
    if (!checked_result) {
        session.render_diagnostics_to_stderr();
        return session.finish(base::Result<void>::fail(checked_result.error()));
    }
    sema::CheckedModule checked = checked_result.take_value();

    auto incremental_cache_result =
        frontend_pipeline.write_checked_incremental_cache(frontend.modules, frontend.ast, checked);
    if (!incremental_cache_result) {
        return session.finish(base::Result<void>::fail(incremental_cache_result.error()));
    }

    if (this->invocation_.emit_kind == EmitKind::check) {
        return session.finish(base::Result<void>::ok());
    }

    if (this->invocation_.emit_kind == EmitKind::typed) {
        return session.finish(base::Result<void>::ok());
    }

    if (this->invocation_.emit_kind == EmitKind::checked) {
        return session.finish(dump_checked_output(checked, session.profiler()));
    }

    if (this->invocation_.emit_kind == EmitKind::ir || this->invocation_.emit_kind == EmitKind::llvm_ir) {
        return session.finish(emit_ir_or_llvm_output(session, frontend.ast, checked));
    }

    if (emit_kind_is_native_artifact(this->invocation_.emit_kind)) {
        return session.finish(emit_native_output(session, frontend.ast, checked));
    }

    return session.finish(
        base::Result<void>::fail({base::ErrorCode::codegen_error, std::string(DRIVER_UNSUPPORTED_EMISSION_MODE)}));
}

} // namespace aurex::driver
