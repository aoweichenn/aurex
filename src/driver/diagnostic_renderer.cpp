#include <aurex/driver/diagnostic_renderer.hpp>
#include <aurex/driver/driver_messages.hpp>

#include <aurex/query/diagnostics_query.hpp>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <unistd.h>

namespace aurex::driver {
namespace {

constexpr base::usize DRIVER_MAX_PRINTED_DIAGNOSTICS = 128;
constexpr base::usize DRIVER_MAX_DIAGNOSTIC_SPAN_LINES = 8;
constexpr std::string_view DRIVER_COLOR_ENV = "AUREX_COLOR_DIAGNOSTICS";
constexpr std::string_view DRIVER_COLOR_ALWAYS = "always";
constexpr std::string_view DRIVER_COLOR_NEVER = "never";
constexpr std::string_view DRIVER_COLOR_AUTO = "auto";
constexpr std::string_view DRIVER_NO_COLOR_ENV = "NO_COLOR";
constexpr std::string_view DRIVER_COLOR_RESET = "\033[0m";
constexpr std::string_view DRIVER_COLOR_ERROR = "\033[1;31m";
constexpr std::string_view DRIVER_COLOR_WARNING = "\033[1;33m";
constexpr std::string_view DRIVER_COLOR_NOTE = "\033[1;36m";
constexpr std::string_view DRIVER_COLOR_HELP = "\033[1;32m";
constexpr std::string_view DRIVER_COLOR_CARET = "\033[1;32m";
constexpr char DRIVER_JSON_QUOTE = '"';
constexpr char DRIVER_JSON_BACKSLASH = '\\';
constexpr char DRIVER_JSON_NEWLINE = '\n';
constexpr char DRIVER_JSON_CARRIAGE_RETURN = '\r';
constexpr char DRIVER_JSON_TAB = '\t';
constexpr unsigned int DRIVER_JSON_CONTROL_CHAR_LIMIT = 0x20U;
constexpr unsigned int DRIVER_JSON_BYTE_MASK = 0xffU;
constexpr unsigned int DRIVER_JSON_NIBBLE_BITS = 4U;
constexpr unsigned int DRIVER_JSON_LOW_NIBBLE_MASK = 0x0fU;
constexpr char DRIVER_JSON_HEX_DIGITS[] = "0123456789abcdef";
constexpr std::string_view DRIVER_JSON_FORMAT = "aurex-diagnostics-v1";

[[nodiscard]] bool env_equals(const char* const value, const std::string_view expected) noexcept
{
    return value != nullptr && std::string_view{value} == expected;
}

[[nodiscard]] bool diagnostic_color_enabled() noexcept
{
    const char* const color_env = std::getenv(DRIVER_COLOR_ENV.data());
    if (env_equals(color_env, DRIVER_COLOR_ALWAYS)) {
        return true;
    }
    if (env_equals(color_env, DRIVER_COLOR_NEVER)) {
        return false;
    }
    if (color_env != nullptr && std::string_view{color_env} != DRIVER_COLOR_AUTO) {
        return false;
    }
    if (std::getenv(DRIVER_NO_COLOR_ENV.data()) != nullptr) {
        return false;
    }
    return ::isatty(STDERR_FILENO) != 0;
}

[[nodiscard]] std::string_view severity_color(const base::Severity severity) noexcept
{
    switch (severity) {
        case base::Severity::error:
        case base::Severity::fatal:
            return DRIVER_COLOR_ERROR;
        case base::Severity::warning:
            return DRIVER_COLOR_WARNING;
        case base::Severity::note:
            return DRIVER_COLOR_NOTE;
        case base::Severity::help:
            return DRIVER_COLOR_HELP;
    }
    return {};
}

void print_colored(std::ostream& out, const bool color, const std::string_view color_code, const std::string_view text)
{
    if (color && !color_code.empty()) {
        out << color_code << text << DRIVER_COLOR_RESET;
        return;
    }
    out << text;
}

[[nodiscard]] base::usize diagnostic_span_end(const base::SourceRange& range, const std::string_view text) noexcept
{
    if (range.end > range.begin) {
        return std::min(range.end, text.size());
    }
    return std::min(range.begin + 1, text.size());
}

void print_diagnostic_source_line(std::ostream& out, const base::SourceFile& file, const base::SourceRange& range,
    const base::usize line_offset, const base::usize span_end, const bool color)
{
    const std::string_view text = file.text();
    const base::SourceLineExtent line = file.line_extent(line_offset);
    const std::string_view source_line = text.substr(line.begin, line.end - line.begin);
    out << "  " << source_line << "\n";
    out << "  ";

    const base::usize highlight_begin = std::max(range.begin, line.begin);
    const base::usize highlight_end = std::max(highlight_begin + 1, std::min(span_end, line.end));
    const base::usize caret_column = std::min(highlight_begin, line.end) - line.begin;
    for (base::usize i = 0; i < caret_column && i < source_line.size(); ++i) {
        out << (source_line[i] == '\t' ? '\t' : ' ');
    }
    print_colored(out, color, DRIVER_COLOR_CARET, "^");
    for (base::usize i = highlight_begin + 1; i < highlight_end && i < line.end; ++i) {
        print_colored(out, color, DRIVER_COLOR_CARET, "~");
    }
    out << "\n";
}

void print_diagnostic_source(
    std::ostream& out, const base::SourceFile& file, const query::QueryDiagnosticEvent& event, const bool color)
{
    const std::string_view text = file.text();
    if (text.empty()) {
        return;
    }
    const base::usize span_begin = std::min(event.range.begin, text.size());
    const base::usize span_end = diagnostic_span_end(event.range, text);
    base::usize line_offset = span_begin;
    base::usize printed_lines = 0;
    while (line_offset <= span_end && printed_lines < DRIVER_MAX_DIAGNOSTIC_SPAN_LINES) {
        const base::SourceLineExtent line = file.line_extent(line_offset);
        print_diagnostic_source_line(out, file, event.range, line_offset, span_end, color);
        printed_lines += 1;
        if (span_end <= line.end || line.end >= text.size()) {
            return;
        }
        line_offset = line.end + 1;
    }
    if (line_offset <= span_end) {
        out << "  ...\n";
    }
}

void print_json_escaped(std::ostream& out, const std::string_view text)
{
    out << DRIVER_JSON_QUOTE;
    for (const unsigned char byte : text) {
        switch (byte) {
            case DRIVER_JSON_QUOTE:
                out << "\\\"";
                break;
            case DRIVER_JSON_BACKSLASH:
                out << "\\\\";
                break;
            case DRIVER_JSON_NEWLINE:
                out << "\\n";
                break;
            case DRIVER_JSON_CARRIAGE_RETURN:
                out << "\\r";
                break;
            case DRIVER_JSON_TAB:
                out << "\\t";
                break;
            default:
                if (byte < DRIVER_JSON_CONTROL_CHAR_LIMIT) {
                    out << "\\u00"
                        << DRIVER_JSON_HEX_DIGITS[(byte >> DRIVER_JSON_NIBBLE_BITS) & DRIVER_JSON_LOW_NIBBLE_MASK]
                              << DRIVER_JSON_HEX_DIGITS[byte & DRIVER_JSON_LOW_NIBBLE_MASK];
                } else {
                    out << static_cast<char>(byte & DRIVER_JSON_BYTE_MASK);
                }
                break;
        }
    }
    out << DRIVER_JSON_QUOTE;
}

void print_json_string_field(
    std::ostream& out, const std::string_view key, const std::string_view value, const bool trailing_comma)
{
    print_json_escaped(out, key);
    out << ": ";
    print_json_escaped(out, value);
    if (trailing_comma) {
        out << ",";
    }
    out << "\n";
}

void print_json_range(std::ostream& out, const base::SourceFile& file, const base::SourceRange& range)
{
    const base::LineColumn start = file.line_column(range.begin);
    const base::LineColumn end = file.line_column(range.end);
    out << "      \"range\": {\n";
    out << "        \"source_id\": " << range.source.value << ",\n";
    out << "        \"path\": ";
    print_json_escaped(out, file.path());
    out << ",\n";
    out << "        \"start\": {\"byte\": " << range.begin << ", \"line\": " << start.line
        << ", \"column\": " << start.column << "},\n";
    out << "        \"end\": {\"byte\": " << range.end << ", \"line\": " << end.line
        << ", \"column\": " << end.column << "}\n";
    out << "      }\n";
}

void print_json_null_range(std::ostream& out)
{
    out << "      \"range\": null\n";
}

void print_json_diagnostic_entry(std::ostream& out, const base::Severity severity,
    const base::DiagnosticCategory category, const base::DiagnosticCode code, const std::string_view message,
    const base::SourceFile* file, const base::SourceRange* range)
{
    out << "    {\n";
    out << "      ";
    print_json_string_field(out, "severity", base::severity_name(severity), true);
    out << "      ";
    print_json_string_field(out, "category", base::diagnostic_category_name(category), true);
    out << "      ";
    print_json_string_field(out, "code", base::diagnostic_code_name(code), true);
    out << "      ";
    print_json_string_field(out, "message", message, true);
    if (file != nullptr && range != nullptr) {
        print_json_range(out, *file, *range);
    } else {
        print_json_null_range(out);
    }
    out << "    }";
}

void print_json_diagnostics(
    std::ostream& out, const base::SourceManager& sources, const query::DiagnosticsEventStream& diagnostics)
{
    const std::span<const query::QueryDiagnosticEvent> all = diagnostics.events;
    const base::usize count = std::min<base::usize>(all.size(), DRIVER_MAX_PRINTED_DIAGNOSTICS);
    out << "{\n";
    out << "  \"format\": ";
    print_json_escaped(out, DRIVER_JSON_FORMAT);
    out << ",\n";
    out << "  \"diagnostics\": [\n";
    for (base::usize index = 0; index < count; ++index) {
        const query::QueryDiagnosticEvent& diagnostic = all[index];
        const base::SourceFile& file = sources.get(diagnostic.range.source);
        print_json_diagnostic_entry(out, diagnostic.severity, diagnostic.category, diagnostic.code, diagnostic.message,
            &file, &diagnostic.range);
        if (index + 1 < count) {
            out << ",";
        }
        out << "\n";
    }
    out << "  ],\n";
    out << "  \"suppressed\": "
        << (all.size() > DRIVER_MAX_PRINTED_DIAGNOSTICS ? all.size() - DRIVER_MAX_PRINTED_DIAGNOSTICS : 0)
        << "\n";
    out << "}\n";
}

void print_text_diagnostics(
    std::ostream& out, const base::SourceManager& sources, const query::DiagnosticsEventStream& diagnostics)
{
    const std::span<const query::QueryDiagnosticEvent> all = diagnostics.events;
    const base::usize count = std::min<base::usize>(all.size(), DRIVER_MAX_PRINTED_DIAGNOSTICS);
    const bool color = diagnostic_color_enabled();
    for (base::usize index = 0; index < count; ++index) {
        const query::QueryDiagnosticEvent& diagnostic = all[index];
        const base::SourceFile& file = sources.get(diagnostic.range.source);
        const base::LineColumn location = file.line_column(diagnostic.range.begin);
        out << file.path() << ":" << location.line << ":" << location.column << ": ";
        print_colored(out, color, severity_color(diagnostic.severity), base::severity_name(diagnostic.severity));
        out << ": " << diagnostic.message << "\n";
        print_diagnostic_source(out, file, diagnostic, color);
    }
    if (all.size() > DRIVER_MAX_PRINTED_DIAGNOSTICS) {
        out << "error: too many diagnostics; suppressing " << (all.size() - DRIVER_MAX_PRINTED_DIAGNOSTICS)
            << " additional diagnostics\n";
    }
}

void print_json_driver_error(std::ostream& out, const std::string_view message)
{
    out << "{\n";
    out << "  \"format\": ";
    print_json_escaped(out, DRIVER_JSON_FORMAT);
    out << ",\n";
    out << "  \"diagnostics\": [\n";
    print_json_diagnostic_entry(out, base::Severity::fatal, base::DiagnosticCategory::general,
        base::DiagnosticCode::none, message, nullptr, nullptr);
    out << "\n";
    out << "  ],\n";
    out << "  \"suppressed\": 0\n";
    out << "}\n";
}

void print_text_driver_error(std::ostream& out, const std::string_view message)
{
    out << DRIVER_ERROR_PREFIX << message << "\n";
}

} // namespace

void render_diagnostics(std::ostream& out, const base::SourceManager& sources,
    const base::DiagnosticSink& diagnostics, const DiagnosticOutputFormat format)
{
    if (diagnostics.diagnostics().empty()) {
        return;
    }
    const query::DiagnosticsEventStream stream = query::diagnostic_events_from_sink(diagnostics.diagnostics());
    if (format == DiagnosticOutputFormat::json) {
        print_json_diagnostics(out, sources, stream);
        return;
    }
    print_text_diagnostics(out, sources, stream);
}

void render_driver_error(std::ostream& out, const std::string_view message, const DiagnosticOutputFormat format)
{
    if (format == DiagnosticOutputFormat::json) {
        print_json_driver_error(out, message);
        return;
    }
    print_text_driver_error(out, message);
}

} // namespace aurex::driver
