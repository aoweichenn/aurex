#include <aurex/backend/llvm_backend.hpp>
#include <aurex/driver/cli.hpp>
#include <aurex/driver/cli_llvm.hpp>

#include <ostream>
#include <utility>

namespace aurex::driver {

namespace {

[[nodiscard]] base::Result<LlvmIrOutput> emit_with_llvm_backend(const LlvmIrEmitRequest& request)
{
    auto backend_result = backend::emit_llvm_ir(backend::LlvmEmitRequest{
        request.module,
        request.module_name,
    });
    if (!backend_result) {
        return base::Result<LlvmIrOutput>::fail(backend_result.error());
    }
    return base::Result<LlvmIrOutput>::ok(LlvmIrOutput{std::move(backend_result.value().text)});
}

} // namespace

LlvmIrEmitter llvm_backend_ir_emitter() noexcept
{
    return emit_with_llvm_backend;
}

int run_cli_with_llvm_backend(const std::span<const std::string_view> arguments, std::ostream& out, std::ostream& err)
{
    return run_cli(arguments, out, err, llvm_backend_ir_emitter());
}

} // namespace aurex::driver
