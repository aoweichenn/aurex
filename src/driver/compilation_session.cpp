#include "compilation_session.hpp"

#include <aurex/driver/diagnostic_renderer.hpp>
#include <aurex/driver/invocation.hpp>

#include <iostream>

namespace aurex::driver {

CompilationSession::CompilationSession(
    const CompilerInvocation& invocation, const LlvmIrEmitter llvm_ir_emitter) noexcept
    : invocation_(invocation), llvm_ir_emitter_(llvm_ir_emitter), profiler_(!invocation.profile_output_path.empty())
{
}

const CompilerInvocation& CompilationSession::invocation() const noexcept
{
    return this->invocation_;
}

LlvmIrEmitter CompilationSession::llvm_ir_emitter() const noexcept
{
    return this->llvm_ir_emitter_;
}

base::SourceManager& CompilationSession::sources() noexcept
{
    return this->sources_;
}

base::DiagnosticSink& CompilationSession::diagnostics() noexcept
{
    return this->diagnostics_;
}

CompilationProfiler* CompilationSession::profiler() noexcept
{
    return &this->profiler_;
}

void CompilationSession::render_diagnostics_to_stderr() const
{
    render_diagnostics(std::cerr, this->sources_, this->diagnostics_, this->invocation_.diagnostic_format);
}

base::Result<void> CompilationSession::finish(base::Result<void> result) const
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

} // namespace aurex::driver
