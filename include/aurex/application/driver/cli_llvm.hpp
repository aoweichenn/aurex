#pragma once

#include <aurex/application/driver/llvm_ir_emitter.hpp>

#include <iosfwd>
#include <span>
#include <string_view>

namespace aurex::driver {

[[nodiscard]] LlvmIrEmitter llvm_backend_ir_emitter() noexcept;
[[nodiscard]] int run_cli_with_llvm_backend(
    std::span<const std::string_view> arguments, std::ostream& out, std::ostream& err);

} // namespace aurex::driver
