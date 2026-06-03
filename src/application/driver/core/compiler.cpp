#include <aurex/application/driver/compiler.hpp>

#include <application/driver/pipeline/private/compilation_pipeline.hpp>

namespace aurex::driver {

Compiler::Compiler(const LlvmIrEmitter llvm_ir_emitter) noexcept : llvm_ir_emitter_(llvm_ir_emitter)
{
}

base::Result<void> Compiler::run(const CompilerInvocation& invocation) const
{
    return CompilationPipeline(invocation, this->llvm_ir_emitter_).run();
}

} // namespace aurex::driver
