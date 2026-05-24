#include "backend_pipeline.hpp"

#include <aurex/driver/driver_messages.hpp>
#include <aurex/driver/invocation.hpp>
#include <aurex/driver/native_toolchain.hpp>
#include <aurex/driver/profile.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "pipeline_stage.hpp"

namespace aurex::driver {
namespace {

[[nodiscard]] bool emit_kind_is_native_artifact(const EmitKind emit_kind) noexcept
{
    return emit_kind == EmitKind::assembly || emit_kind == EmitKind::object || emit_kind == EmitKind::executable;
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

} // namespace

BackendPipeline::BackendPipeline(CompilationSession& session) noexcept : session_(session)
{
}

bool BackendPipeline::can_emit_native_artifact() const noexcept
{
    return emit_kind_is_native_artifact(this->session_.invocation().emit_kind);
}

base::Result<void> BackendPipeline::validate_native_output_request() const
{
    if (this->session_.invocation().output_path.empty()) {
        return base::Result<void>::fail(
            {base::ErrorCode::io_error, std::string(DRIVER_NATIVE_OUTPUT_REQUIRES_OUTPUT_PATH)});
    }
    return base::Result<void>::ok();
}

base::Result<std::string> BackendPipeline::emit_llvm_ir_text(const ir::Module& module)
{
    auto llvm_result = [&] {
        ScopedCompilationPhase phase(
            this->session_.profiler(), pipeline_stage_profile_name(PipelineStageId::llvm_emit_ir));
        return emit_llvm_ir(
            this->session_.llvm_ir_emitter(), module, this->session_.invocation().input_path.stem().string());
    }();
    if (!llvm_result) {
        return base::Result<std::string>::fail(llvm_result.error());
    }

    LlvmIrOutput output = llvm_result.take_value();
    return base::Result<std::string>::ok(std::move(output.text));
}

base::Result<void> BackendPipeline::emit_llvm_ir_output(const ir::Module& module)
{
    auto llvm_result = this->emit_llvm_ir_text(module);
    if (!llvm_result) {
        return base::Result<void>::fail(llvm_result.error());
    }

    ScopedCompilationPhase phase(this->session_.profiler(), pipeline_stage_profile_name(PipelineStageId::llvm_ir_dump));
    std::cout << llvm_result.value();
    return base::Result<void>::ok();
}

base::Result<void> BackendPipeline::emit_native_output(const ir::Module& module)
{
    auto llvm_result = this->emit_llvm_ir_text(module);
    if (!llvm_result) {
        return base::Result<void>::fail(llvm_result.error());
    }

    auto temp_ir_result = [&] {
        ScopedCompilationPhase phase(
            this->session_.profiler(), pipeline_stage_profile_name(PipelineStageId::llvm_write_temp));
        return write_temporary_llvm_file(llvm_result.value());
    }();
    if (!temp_ir_result) {
        return base::Result<void>::fail(temp_ir_result.error());
    }

    TemporaryFile temp_ir(temp_ir_result.take_value());
    const NativeCompileRequest request = make_native_compile_request(this->session_.invocation(), temp_ir.path());
    auto native_result = [&] {
        ScopedCompilationPhase phase(
            this->session_.profiler(), pipeline_stage_profile_name(PipelineStageId::native_clang));
        return invoke_clang(request);
    }();
    if (!native_result) {
        return base::Result<void>::fail(native_result.error());
    }

    return base::Result<void>::ok();
}

} // namespace aurex::driver
