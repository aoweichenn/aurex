#include <aurex/base/integer.hpp>
#include <aurex/tooling/lsp.hpp>
#include <aurex/tooling/lsp_stdio.hpp>

#include <algorithm>
#include <charconv>
#include <ios>
#include <istream>
#include <limits>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace aurex::tooling {
namespace {

inline constexpr int LSP_STDIO_SUCCESS_EXIT_CODE = 0;
inline constexpr int LSP_STDIO_RUNTIME_FAILURE_EXIT_CODE = 1;
inline constexpr int LSP_STDIO_ARGUMENT_FAILURE_EXIT_CODE = 2;
inline constexpr std::string_view LSP_STDIO_DEFAULT_TOOL_NAME = "aurex-lsp";
inline constexpr std::string_view LSP_STDIO_CONTENT_LENGTH_HEADER = "Content-Length";
inline constexpr std::string_view LSP_STDIO_HEADER_SEPARATOR = ":";
inline constexpr std::string_view LSP_STDIO_OPTION_HELP = "--help";
inline constexpr std::string_view LSP_STDIO_OPTION_HELP_SHORT = "-h";
inline constexpr std::string_view LSP_STDIO_OPTION_ROOT = "--root";
inline constexpr std::string_view LSP_STDIO_OPTION_SOURCE_ROOT = "--source-root";
inline constexpr std::string_view LSP_STDIO_OPTION_PACKAGE = "--package";
inline constexpr std::string_view LSP_STDIO_OPTION_IMPORT_PATH = "--import-path";
inline constexpr std::string_view LSP_STDIO_OPTION_IMPORT_PATH_SHORT = "-I";

[[nodiscard]] base::Error lsp_stdio_error(const base::ErrorCode code, const std::string_view message)
{
    return {code, std::string(message)};
}

[[nodiscard]] base::Result<LspStdioOptions> fail_lsp_stdio_parse(const std::string_view message)
{
    return base::Result<LspStdioOptions>::fail(lsp_stdio_error(base::ErrorCode::invalid_source, message));
}

[[nodiscard]] base::Result<void> fail_lsp_stdio_io(const std::string_view message)
{
    return base::Result<void>::fail(lsp_stdio_error(base::ErrorCode::io_error, message));
}

[[nodiscard]] bool lsp_stdio_ascii_equal_ignore_case(const char lhs, const char rhs) noexcept
{
    if (lhs == rhs) {
        return true;
    }
    if (lhs >= 'A' && lhs <= 'Z') {
        return static_cast<char>(lhs - 'A' + 'a') == rhs;
    }
    if (rhs >= 'A' && rhs <= 'Z') {
        return lhs == static_cast<char>(rhs - 'A' + 'a');
    }
    return false;
}

[[nodiscard]] bool lsp_stdio_header_name_equals(const std::string_view header, const std::string_view expected) noexcept
{
    return header.size() == expected.size()
        && std::equal(
            header.begin(), header.end(), expected.begin(), expected.end(), lsp_stdio_ascii_equal_ignore_case);
}

[[nodiscard]] std::string_view lsp_stdio_trim_ascii_spaces(const std::string_view value) noexcept
{
    base::usize begin = 0;
    base::usize end = value.size();
    while (begin < end && (value[begin] == ' ' || value[begin] == '\t')) {
        ++begin;
    }
    while (end > begin && (value[end - 1U] == ' ' || value[end - 1U] == '\t')) {
        --end;
    }
    return value.substr(begin, end - begin);
}

[[nodiscard]] base::Result<base::usize> lsp_stdio_parse_content_length_value(const std::string_view value)
{
    const std::string_view trimmed = lsp_stdio_trim_ascii_spaces(value);
    base::usize length = 0;
    const auto [ptr, error] = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), length);
    if (error != std::errc{} || ptr != trimmed.data() + trimmed.size()) {
        return base::Result<base::usize>::fail(
            lsp_stdio_error(base::ErrorCode::invalid_source, "invalid LSP Content-Length header"));
    }
    if (length > LSP_MAX_MESSAGE_BYTES) {
        return base::Result<base::usize>::fail(
            lsp_stdio_error(base::ErrorCode::invalid_source, "LSP message body is too large"));
    }
    return base::Result<base::usize>::ok(length);
}

[[nodiscard]] base::Result<std::optional<base::usize>> lsp_stdio_read_content_length(std::istream& input)
{
    std::string line;
    std::optional<base::usize> content_length;
    bool read_any_header_line = false;
    while (std::getline(input, line)) {
        read_any_header_line = true;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            if (!content_length.has_value()) {
                return base::Result<std::optional<base::usize>>::fail(
                    lsp_stdio_error(base::ErrorCode::invalid_source, "missing LSP Content-Length header"));
            }
            return base::Result<std::optional<base::usize>>::ok(content_length);
        }

        const base::usize separator = line.find(LSP_STDIO_HEADER_SEPARATOR);
        if (separator == std::string::npos) {
            continue;
        }
        const std::string_view name = lsp_stdio_trim_ascii_spaces(std::string_view{line}.substr(0, separator));
        if (!lsp_stdio_header_name_equals(name, LSP_STDIO_CONTENT_LENGTH_HEADER)) {
            continue;
        }
        base::Result<base::usize> parsed =
            lsp_stdio_parse_content_length_value(std::string_view{line}.substr(separator + 1U));
        if (!parsed) {
            return base::Result<std::optional<base::usize>>::fail(parsed.error());
        }
        content_length = parsed.value();
    }

    if (read_any_header_line) {
        return base::Result<std::optional<base::usize>>::fail(
            lsp_stdio_error(base::ErrorCode::invalid_source, "unexpected EOF while reading LSP headers"));
    }
    return base::Result<std::optional<base::usize>>::ok(std::nullopt);
}

[[nodiscard]] base::Result<std::string> lsp_stdio_read_body(std::istream& input, const base::usize content_length)
{
    if (content_length > static_cast<base::usize>(std::numeric_limits<std::streamsize>::max())) {
        return base::Result<std::string>::fail(
            lsp_stdio_error(base::ErrorCode::invalid_source, "LSP message body is too large"));
    }

    std::string body(content_length, '\0');
    input.read(body.data(), static_cast<std::streamsize>(body.size()));
    if (input.gcount() != static_cast<std::streamsize>(body.size())) {
        return base::Result<std::string>::fail(
            lsp_stdio_error(base::ErrorCode::invalid_source, "unexpected EOF while reading LSP body"));
    }
    return base::Result<std::string>::ok(std::move(body));
}

[[nodiscard]] base::Result<void> lsp_stdio_run_loop(std::istream& input, std::ostream& output, LspServer& server)
{
    while (!server.exited()) {
        base::Result<std::optional<base::usize>> content_length = lsp_stdio_read_content_length(input);
        if (!content_length) {
            return base::Result<void>::fail(content_length.error());
        }
        if (!content_length.value().has_value()) {
            return base::Result<void>::ok();
        }

        base::Result<std::string> body = lsp_stdio_read_body(input, *content_length.value());
        if (!body) {
            return base::Result<void>::fail(body.error());
        }

        const std::vector<std::string> responses = server.handle_json_message(body.value());
        for (const std::string& response : responses) {
            output << write_lsp_content_message(response);
        }
        output.flush();
        if (!output) {
            return fail_lsp_stdio_io("failed to write LSP response");
        }
    }
    return base::Result<void>::ok();
}

[[nodiscard]] base::Result<std::string_view> lsp_stdio_required_option_value(
    const std::span<const std::string_view> arguments, const base::usize option_index)
{
    if (option_index + 1U >= arguments.size()) {
        return base::Result<std::string_view>::fail(
            lsp_stdio_error(base::ErrorCode::invalid_source, "missing value for LSP option"));
    }
    return base::Result<std::string_view>::ok(arguments[option_index + 1U]);
}

} // namespace

base::Result<LspStdioOptions> parse_lsp_stdio_arguments(const std::span<const std::string_view> arguments)
{
    LspStdioOptions options;
    for (base::usize index = 1; index < arguments.size(); ++index) {
        const std::string_view argument = arguments[index];
        if (argument == LSP_STDIO_OPTION_HELP || argument == LSP_STDIO_OPTION_HELP_SHORT) {
            options.help = true;
            continue;
        }

        if (argument == LSP_STDIO_OPTION_ROOT || argument == LSP_STDIO_OPTION_SOURCE_ROOT
            || argument == LSP_STDIO_OPTION_PACKAGE || argument == LSP_STDIO_OPTION_IMPORT_PATH
            || argument == LSP_STDIO_OPTION_IMPORT_PATH_SHORT) {
            base::Result<std::string_view> value = lsp_stdio_required_option_value(arguments, index);
            if (!value) {
                return base::Result<LspStdioOptions>::fail(value.error());
            }
            if (argument == LSP_STDIO_OPTION_ROOT) {
                options.config.root_path = std::string(value.value());
            } else if (argument == LSP_STDIO_OPTION_SOURCE_ROOT) {
                options.config.source_root = std::string(value.value());
            } else if (argument == LSP_STDIO_OPTION_PACKAGE) {
                options.config.package_identity = std::string(value.value());
            } else {
                options.config.import_paths.emplace_back(value.value());
            }
            ++index;
            continue;
        }

        return fail_lsp_stdio_parse("unknown LSP option");
    }
    return base::Result<LspStdioOptions>::ok(std::move(options));
}

void print_lsp_stdio_usage(std::ostream& out, const std::string_view tool_name)
{
    out << "usage: " << (tool_name.empty() ? LSP_STDIO_DEFAULT_TOOL_NAME : tool_name)
        << " [--root path] [--source-root path] [--package name] [-I path] [--import-path path]\n"
           "\n"
           "Runs the Aurex language server over stdin/stdout using LSP content-length framing.\n";
}

int run_lsp_stdio(
    const std::span<const std::string_view> arguments, std::istream& input, std::ostream& output, std::ostream& error)
{
    const std::string_view tool_name = arguments.empty() ? LSP_STDIO_DEFAULT_TOOL_NAME : arguments.front();
    base::Result<LspStdioOptions> options = parse_lsp_stdio_arguments(arguments);
    if (!options) {
        error << "argument error: " << options.error().message << '\n';
        print_lsp_stdio_usage(error, tool_name);
        return LSP_STDIO_ARGUMENT_FAILURE_EXIT_CODE;
    }
    if (options.value().help) {
        print_lsp_stdio_usage(output, tool_name);
        return LSP_STDIO_SUCCESS_EXIT_CODE;
    }

    LspServer server(std::move(options.value().config));
    base::Result<void> result = lsp_stdio_run_loop(input, output, server);
    if (!result) {
        error << "LSP error: " << result.error().message << '\n';
        return LSP_STDIO_RUNTIME_FAILURE_EXIT_CODE;
    }
    return LSP_STDIO_SUCCESS_EXIT_CODE;
}

} // namespace aurex::tooling
