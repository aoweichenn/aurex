#include "aurex/backend/codegen_backend.hpp"

namespace aurex::backend {

std::unique_ptr<CodeGenBackend> create_llvm_backend();
std::unique_ptr<CodeGenBackend> create_aurora_backend();

std::unique_ptr<CodeGenBackend> create_codegen_backend(const driver::BackendKind kind) {
    switch (kind) {
    case driver::BackendKind::llvm:
        return create_llvm_backend();
    case driver::BackendKind::aurora:
        return create_aurora_backend();
    }
    return nullptr;
}

} // namespace aurex::backend
