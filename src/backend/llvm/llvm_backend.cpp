#include <aurex/backend/llvm_backend.hpp>

#include <aurex/backend/backend_messages.hpp>

#include <backend/llvm/llvm_backend_internal.hpp>

namespace aurex::backend {

base::Result<LlvmIrOutput> emit_llvm_ir(const LlvmEmitRequest& request) {
    if (request.module == nullptr) {
        return base::Result<LlvmIrOutput>::fail({base::ErrorCode::internal_error, std::string(BACKEND_LLVM_NULL_MODULE)});
    }
    LlvmEmitter emitter(*request.module, request.module_name);
    return emitter.run();
}

} // namespace aurex::backend
