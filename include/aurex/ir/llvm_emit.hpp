#pragma once

#include "aurex/base/result.hpp"
#include "aurex/ir/ir.hpp"

#include <string>

namespace aurex::ir {

struct LlvmIrOutput {
    std::string text;
};

[[nodiscard]] base::Result<LlvmIrOutput> emit_llvm_ir(const Module& module, std::string module_name);

} // namespace aurex::ir
