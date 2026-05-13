#pragma once

#include <aurex/base/result.hpp>
#include <aurex/driver/invocation.hpp>

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
    std::span<const std::string_view> arguments,
    std::ostream& out,
    std::ostream& err
);

} // namespace aurex::driver
