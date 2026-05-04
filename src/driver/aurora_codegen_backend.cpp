#include "aurex/backend/codegen_backend.hpp"
#include "aurex/backend/aurora_backend.hpp"

namespace aurex::backend {
namespace {

class AuroraCodeGenBackend final : public CodeGenBackend {
public:
    base::Result<std::string> emit_ir_text(const ir::Module&) override {
        return base::Result<std::string>::fail({
            base::ErrorCode::codegen_error,
            "Aurora IR text not available; use --emit=asm for assembly dump"
        });
    }

    base::Result<std::string> emit_assembly(const ir::Module& module) override {
        auto r = emit_aurora_asm(AuroraEmitRequest{&module, "aurex", "", ir::OptimizationLevel::none});
        if (!r) return base::Result<std::string>::fail(r.error());
        return base::Result<std::string>::ok(r.value().text);
    }

    base::Result<void> emit_object(const ir::Module& module, const std::string& out) override {
        return emit_aurora_obj(AuroraEmitRequest{&module, "aurex", out, ir::OptimizationLevel::none});
    }
};

} // namespace

std::unique_ptr<CodeGenBackend> create_aurora_backend() {
    return std::make_unique<AuroraCodeGenBackend>();
}

} // namespace aurex::backend
