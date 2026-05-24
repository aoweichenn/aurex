#pragma once

#include <aurex/base/diagnostic.hpp>
#include <aurex/base/result.hpp>
#include <aurex/base/source.hpp>
#include <aurex/driver/llvm_ir_emitter.hpp>
#include <aurex/driver/profile.hpp>

namespace aurex::driver {

struct CompilerInvocation;

class CompilationSession final {
public:
    CompilationSession(const CompilerInvocation& invocation, LlvmIrEmitter llvm_ir_emitter) noexcept;

    [[nodiscard]] const CompilerInvocation& invocation() const noexcept;
    [[nodiscard]] LlvmIrEmitter llvm_ir_emitter() const noexcept;
    [[nodiscard]] base::SourceManager& sources() noexcept;
    [[nodiscard]] base::DiagnosticSink& diagnostics() noexcept;
    [[nodiscard]] CompilationProfiler* profiler() noexcept;

    void render_diagnostics_to_stderr() const;
    [[nodiscard]] base::Result<void> finish(base::Result<void> result) const;

private:
    const CompilerInvocation& invocation_;
    LlvmIrEmitter llvm_ir_emitter_ = nullptr;
    base::SourceManager sources_;
    base::DiagnosticSink diagnostics_;
    CompilationProfiler profiler_;
};

} // namespace aurex::driver
