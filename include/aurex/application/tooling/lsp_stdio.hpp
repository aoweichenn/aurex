#pragma once

#include <aurex/application/tooling/session.hpp>
#include <aurex/infrastructure/base/result.hpp>

#include <iosfwd>
#include <span>
#include <string_view>

namespace aurex::tooling {

struct LspStdioOptions {
    ToolingProjectConfig config;
    bool help = false;
};

[[nodiscard]] base::Result<LspStdioOptions> parse_lsp_stdio_arguments(std::span<const std::string_view> arguments);
void print_lsp_stdio_usage(std::ostream& out, std::string_view tool_name);
[[nodiscard]] int run_lsp_stdio(
    std::span<const std::string_view> arguments, std::istream& input, std::ostream& output, std::ostream& error);

} // namespace aurex::tooling
