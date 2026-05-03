#include "aurex/ir/llvm_emit.hpp"

#include "llvm_emit_internal.hpp"

#include <utility>

namespace aurex::ir {

base::Result<LlvmIrOutput> emit_llvm_ir(const Module& module, std::string module_name) {
    LlvmEmitter emitter(module, std::move(module_name));
    return emitter.run();
}

} // namespace aurex::ir
