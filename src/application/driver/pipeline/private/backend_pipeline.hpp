#pragma once

#include <aurex/infrastructure/base/result.hpp>
#include <aurex/midend/ir/ir.hpp>

#include <string>

#include <application/driver/pipeline/private/compilation_session.hpp>

namespace aurex::driver {

class BackendPipeline final {
public:
    explicit BackendPipeline(CompilationSession& session) noexcept;

    [[nodiscard]] bool can_emit_native_artifact() const noexcept;
    [[nodiscard]] base::Result<void> validate_native_output_request() const;
    [[nodiscard]] base::Result<void> emit_llvm_ir_output(const ir::Module& module);
    [[nodiscard]] base::Result<void> emit_native_output(const ir::Module& module);

private:
    [[nodiscard]] base::Result<std::string> emit_llvm_ir_text(const ir::Module& module);

    CompilationSession& session_;
};

} // namespace aurex::driver
