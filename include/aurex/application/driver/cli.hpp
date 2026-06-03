#pragma once

#include <aurex/application/driver/invocation.hpp>
#include <aurex/application/driver/llvm_ir_emitter.hpp>
#include <aurex/infrastructure/base/result.hpp>

#include <iosfwd>
#include <span>
#include <string_view>

namespace aurex::driver {

enum class CliAction {
    compile,
    help,
    version,
};

struct CliParseResult {
    CliAction action = CliAction::compile;
    CompilerInvocation invocation;
};

[[nodiscard]] base::Result<CliParseResult> parse_cli_arguments(std::span<const std::string_view> arguments);
void print_cli_usage(std::ostream& out, std::string_view tool_name);
[[nodiscard]] int run_cli(
    std::span<const std::string_view> arguments, std::ostream& out, std::ostream& err, LlvmIrEmitter llvm_ir_emitter);
[[nodiscard]] int run_cli(std::span<const std::string_view> arguments, std::ostream& out, std::ostream& err);

} // namespace aurex::driver
