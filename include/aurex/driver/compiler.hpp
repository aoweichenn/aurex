#pragma once

#include <aurex/base/result.hpp>
#include <aurex/driver/llvm_ir_emitter.hpp>

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
