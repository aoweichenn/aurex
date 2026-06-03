#pragma once

#include <aurex/application/driver/llvm_ir_emitter.hpp>
#include <aurex/infrastructure/base/result.hpp>

namespace aurex::driver {

struct CompilerInvocation;

class Compiler final {
public:
    explicit Compiler(LlvmIrEmitter llvm_ir_emitter = nullptr) noexcept;

    [[nodiscard]] base::Result<void> run(const CompilerInvocation& invocation) const;

private:
    LlvmIrEmitter llvm_ir_emitter_ = nullptr;
};

} // namespace aurex::driver
