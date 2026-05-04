#pragma once

#include "aurex/base/result.hpp"
#include "aurex/driver/invocation.hpp"
#include "aurex/ir/ir.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace aurex::backend {

struct CodeGenBackend {
    virtual ~CodeGenBackend() = default;

    [[nodiscard]] virtual base::Result<std::string> emit_ir_text(const ir::Module& module) = 0;

    [[nodiscard]] virtual base::Result<std::string> emit_assembly(const ir::Module& module) = 0;

    [[nodiscard]] virtual base::Result<void> emit_object(
        const ir::Module& module,
        const std::string& output_path) = 0;
};

[[nodiscard]] std::unique_ptr<CodeGenBackend> create_codegen_backend(driver::BackendKind kind);

} // namespace aurex::backend
