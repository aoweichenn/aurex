#pragma once

#include <aurex/base/result.hpp>
#include <aurex/ir/ir.hpp>

#include <string>
#include <vector>

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
