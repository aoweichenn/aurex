#pragma once

#include <aurex/infrastructure/base/result.hpp>

#include <string>

namespace aurex::ir {
struct Module;
} // namespace aurex::ir

namespace aurex::backend {

struct LlvmIrOutput {
    std::string text;
};

struct LlvmEmitRequest {
    const aurex::ir::Module* module = nullptr;
    std::string module_name;
};

[[nodiscard]] base::Result<LlvmIrOutput> emit_llvm_ir(const LlvmEmitRequest& request);

} // namespace aurex::backend
