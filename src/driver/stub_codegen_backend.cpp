#include "aurex/backend/codegen_backend.hpp"

namespace aurex::backend {
namespace {

class StubCodeGenBackend final : public CodeGenBackend {
public:
    base::Result<std::string> emit_ir_text(const ir::Module&) override {
        return base::Result<std::string>::fail({
            base::ErrorCode::codegen_error,
            "Aurora backend not available; rebuild with -DAUREX_USE_AURORA_BACKEND=ON"
        });
    }

    base::Result<std::string> emit_assembly(const ir::Module&) override {
        return base::Result<std::string>::fail({
            base::ErrorCode::codegen_error,
            "Aurora backend not available; rebuild with -DAUREX_USE_AURORA_BACKEND=ON"
        });
    }

    base::Result<void> emit_object(const ir::Module&, const std::string&) override {
        return base::Result<void>::fail({
            base::ErrorCode::codegen_error,
            "Aurora backend not available; rebuild with -DAUREX_USE_AURORA_BACKEND=ON"
        });
    }
};

} // namespace

std::unique_ptr<CodeGenBackend> create_aurora_backend() {
    return std::make_unique<StubCodeGenBackend>();
}

} // namespace aurex::backend
