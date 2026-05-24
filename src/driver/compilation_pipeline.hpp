#pragma once

#include <aurex/base/result.hpp>
#include <aurex/driver/llvm_ir_emitter.hpp>

namespace aurex::driver {

struct CompilerInvocation;

class CompilationPipeline final {
public:
    CompilationPipeline(const CompilerInvocation& invocation, LlvmIrEmitter llvm_ir_emitter) noexcept;

    [[nodiscard]] base::Result<void> run();

private:
    const CompilerInvocation& invocation_;
    LlvmIrEmitter llvm_ir_emitter_ = nullptr;
};

} // namespace aurex::driver
