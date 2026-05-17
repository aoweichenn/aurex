#pragma once

#include <aurex/base/result.hpp>
#include <aurex/driver/invocation.hpp>
#include <aurex/ir/ir.hpp>

#include <string>

namespace aurex::driver {

struct LlvmIrOutput {
    std::string text;
};

struct LlvmIrEmitRequest {
    const ir::Module* module = nullptr;
    std::string module_name;
};

using LlvmIrEmitter = base::Result<LlvmIrOutput> (*)(const LlvmIrEmitRequest&);

class Compiler final {
public:
    explicit Compiler(LlvmIrEmitter llvm_ir_emitter = nullptr) noexcept;

    [[nodiscard]] base::Result<void> run(const CompilerInvocation& invocation) const;

private:
    LlvmIrEmitter llvm_ir_emitter_ = nullptr;
};

} // namespace aurex::driver
