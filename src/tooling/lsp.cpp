#include <aurex/base/diagnostic.hpp>
#include <aurex/tooling/lsp.hpp>

#include <charconv>
#include <optional>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

namespace aurex::tooling {
namespace {

constexpr std::string_view LSP_HEADER_CONTENT_LENGTH = "Content-Length";
constexpr std::string_view LSP_HEADER_SEPARATOR = "\r\n\r\n";
constexpr std::string_view LSP_LINE_SEPARATOR = "\r\n";
constexpr std::string_view LSP_JSONRPC_VERSION_FIELD = "\"jsonrpc\":\"2.0\"";
constexpr std::string_view LSP_NULL = "null";
constexpr std::string_view LSP_SOURCE_NAME = "aurex";
constexpr std::string_view LSP_METHOD_INITIALIZE = "initialize";
constexpr std::string_view LSP_METHOD_INITIALIZED = "initialized";
constexpr std::string_view LSP_METHOD_SHUTDOWN = "shutdown";
constexpr std::string_view LSP_METHOD_EXIT = "exit";
constexpr std::string_view LSP_METHOD_CANCEL = "$/cancelRequest";
constexpr std::string_view LSP_METHOD_DID_OPEN = "textDocument/didOpen";
constexpr std::string_view LSP_METHOD_DID_CHANGE = "textDocument/didChange";
constexpr std::string_view LSP_METHOD_DID_CLOSE = "textDocument/didClose";
constexpr std::string_view LSP_METHOD_HOVER = "textDocument/hover";
constexpr std::string_view LSP_METHOD_DEFINITION = "textDocument/definition";
constexpr std::string_view LSP_METHOD_REFERENCES = "textDocument/references";
constexpr std::string_view LSP_METHOD_DOCUMENT_SYMBOL = "textDocument/documentSymbol";
constexpr std::string_view LSP_METHOD_PUBLISH_DIAGNOSTICS = "textDocument/publishDiagnostics";
constexpr std::string_view LSP_PROP_ID = "id";
constexpr std::string_view LSP_PROP_METHOD = "method";
constexpr std::string_view LSP_PROP_PARAMS = "params";
constexpr std::string_view LSP_PROP_TEXT_DOCUMENT = "textDocument";
constexpr std::string_view LSP_PROP_CONTENT_CHANGES = "contentChanges";
constexpr std::string_view LSP_PROP_POSITION = "position";
constexpr std::string_view LSP_PROP_URI = "uri";
constexpr std::string_view LSP_PROP_TEXT = "text";
constexpr std::string_view LSP_PROP_VERSION = "version";
constexpr std::string_view LSP_PROP_LINE = "line";
constexpr std::string_view LSP_PROP_CHARACTER = "character";
constexpr std::string_view LSP_MARKUP_KIND_PLAINTEXT = "plaintext";
constexpr std::string_view LSP_JSON_ESCAPE_HEX_DIGITS = "0123456789ABCDEF";
constexpr char LSP_JSON_QUOTE = '"';
constexpr char LSP_JSON_ESCAPE = '\\';
constexpr char LSP_JSON_OBJECT_OPEN = '{';
constexpr char LSP_JSON_OBJECT_CLOSE = '}';
constexpr char LSP_JSON_ARRAY_OPEN = '[';
constexpr char LSP_JSON_ARRAY_CLOSE = ']';
constexpr char LSP_JSON_COLON = ':';
constexpr char LSP_JSON_COMMA = ',';
constexpr char LSP_JSON_SPACE = ' ';
constexpr char LSP_JSON_TAB = '\t';
constexpr char LSP_JSON_NEWLINE = '\n';
constexpr char LSP_JSON_RETURN = '\r';
constexpr char LSP_JSON_BACKSPACE = '\b';
constexpr char LSP_JSON_FORM_FEED = '\f';
constexpr char LSP_JSON_SLASH = '/';
constexpr char LSP_HEADER_COLON = ':';
constexpr base::usize LSP_HEADER_SEPARATOR_SIZE = 4;
constexpr base::usize LSP_JSON_UNICODE_ESCAPE_HEX_DIGITS = 4;
constexpr base::usize LSP_JSON_HEX_NIBBLE_SHIFT = 4;
constexpr unsigned char LSP_JSON_HEX_NIBBLE_MASK = 0x0FU;
constexpr int LSP_DECIMAL_DIGIT_COUNT = 10;
constexpr unsigned int LSP_JSON_CONTROL_LIMIT = 0x20U;
constexpr unsigned int LSP_JSON_ASCII_LIMIT = 0x7FU;
constexpr int LSP_ERROR_METHOD_NOT_FOUND = -32601;
constexpr int LSP_ERROR_INVALID_REQUEST = -32600;
constexpr int LSP_TEXT_DOCUMENT_SYNC_FULL = 1;
constexpr int LSP_DIAGNOSTIC_SEVERITY_ERROR = 1;
constexpr int LSP_DIAGNOSTIC_SEVERITY_WARNING = 2;
constexpr int LSP_DIAGNOSTIC_SEVERITY_INFORMATION = 3;
constexpr int LSP_DIAGNOSTIC_SEVERITY_HINT = 4;
constexpr int LSP_SYMBOL_KIND_CLASS = 5;
constexpr int LSP_SYMBOL_KIND_METHOD = 6;
constexpr int LSP_SYMBOL_KIND_FIELD = 8;
constexpr int LSP_SYMBOL_KIND_ENUM = 10;
constexpr int LSP_SYMBOL_KIND_FUNCTION = 12;
constexpr int LSP_SYMBOL_KIND_VARIABLE = 13;
constexpr int LSP_SYMBOL_KIND_CONSTANT = 14;
constexpr int LSP_SYMBOL_KIND_ENUM_MEMBER = 22;
constexpr int LSP_SYMBOL_KIND_STRUCT = 23;
constexpr int LSP_SYMBOL_KIND_TYPE_PARAMETER = 26;

struct JsonSlice {
    std::string_view text;
    bool found = false;
};

[[nodiscard]] bool lsp_json_is_whitespace(const char ch) noexcept
{
    return ch == LSP_JSON_SPACE || ch == LSP_JSON_TAB || ch == LSP_JSON_NEWLINE || ch == LSP_JSON_RETURN;
}

[[nodiscard]] base::usize lsp_json_skip_whitespace(const std::string_view text, base::usize index) noexcept
{
    while (index < text.size() && lsp_json_is_whitespace(text[index])) {
        ++index;
    }
    return index;
}

[[nodiscard]] std::optional<base::usize> lsp_json_string_end(
    const std::string_view text, const base::usize begin) noexcept
{
    if (begin >= text.size() || text[begin] != LSP_JSON_QUOTE) {
        return std::nullopt;
    }
    bool escaped = false;
    for (base::usize index = begin + 1U; index < text.size(); ++index) {
        const char ch = text[index];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (ch == LSP_JSON_ESCAPE) {
            escaped = true;
            continue;
        }
        if (ch == LSP_JSON_QUOTE) {
            return index + 1U;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<base::usize> lsp_json_container_end(
    const std::string_view text, const base::usize begin) noexcept
{
    base::usize depth = 0;
    bool in_string = false;
    bool escaped = false;
    for (base::usize index = begin; index < text.size(); ++index) {
        const char ch = text[index];
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (ch == LSP_JSON_ESCAPE) {
                escaped = true;
            } else if (ch == LSP_JSON_QUOTE) {
                in_string = false;
            }
            continue;
        }
        if (ch == LSP_JSON_QUOTE) {
            in_string = true;
            continue;
        }
        if (ch == LSP_JSON_OBJECT_OPEN || ch == LSP_JSON_ARRAY_OPEN) {
            ++depth;
            continue;
        }
        if (ch == LSP_JSON_OBJECT_CLOSE || ch == LSP_JSON_ARRAY_CLOSE) {
            if (depth == 0U) {
                return std::nullopt;
            }
            --depth;
            if (depth == 0U) {
                return index + 1U;
            }
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<base::usize> lsp_json_scalar_end(
    const std::string_view text, base::usize begin) noexcept
{
    while (begin < text.size() && text[begin] != LSP_JSON_COMMA && text[begin] != LSP_JSON_OBJECT_CLOSE
           && text[begin] != LSP_JSON_ARRAY_CLOSE && !lsp_json_is_whitespace(text[begin])) {
        ++begin;
    }
    return begin;
}

[[nodiscard]] std::optional<base::usize> lsp_json_value_end(
    const std::string_view text, const base::usize begin) noexcept
{
    if (begin >= text.size()) {
        return std::nullopt;
    }
    if (text[begin] == LSP_JSON_QUOTE) {
        return lsp_json_string_end(text, begin);
    }
    if (text[begin] == LSP_JSON_OBJECT_OPEN || text[begin] == LSP_JSON_ARRAY_OPEN) {
        return lsp_json_container_end(text, begin);
    }
    return lsp_json_scalar_end(text, begin);
}

[[nodiscard]] int lsp_hex_value(const char ch) noexcept
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + LSP_DECIMAL_DIGIT_COUNT;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + LSP_DECIMAL_DIGIT_COUNT;
    }
    return -1;
}

[[nodiscard]] std::optional<std::string> lsp_json_unescape_string(const std::string_view value)
{
    if (value.size() < 2U || value.front() != LSP_JSON_QUOTE || value.back() != LSP_JSON_QUOTE) {
        return std::nullopt;
    }
    std::string result;
    result.reserve(value.size() - 2U);
    for (base::usize index = 1U; index + 1U < value.size(); ++index) {
        char ch = value[index];
        if (ch != LSP_JSON_ESCAPE) {
            result.push_back(ch);
            continue;
        }
        ++index;
        if (index + 1U >= value.size()) {
            return std::nullopt;
        }
        ch = value[index];
        switch (ch) {
            case LSP_JSON_QUOTE:
            case LSP_JSON_ESCAPE:
            case LSP_JSON_SLASH:
                result.push_back(ch);
                break;
            case 'b':
                result.push_back(LSP_JSON_BACKSPACE);
                break;
            case 'f':
                result.push_back(LSP_JSON_FORM_FEED);
                break;
            case 'n':
                result.push_back(LSP_JSON_NEWLINE);
                break;
            case 'r':
                result.push_back(LSP_JSON_RETURN);
                break;
            case 't':
                result.push_back(LSP_JSON_TAB);
                break;
            case 'u': {
                if (index + LSP_JSON_UNICODE_ESCAPE_HEX_DIGITS >= value.size()) {
                    return std::nullopt;
                }
                unsigned int code = 0;
                for (base::usize digit = 0; digit < LSP_JSON_UNICODE_ESCAPE_HEX_DIGITS; ++digit) {
                    const int nibble = lsp_hex_value(value[index + 1U + digit]);
                    if (nibble < 0) {
                        return std::nullopt;
                    }
                    code = (code << LSP_JSON_HEX_NIBBLE_SHIFT) | static_cast<unsigned int>(nibble);
                }
                result.push_back(code <= LSP_JSON_ASCII_LIMIT ? static_cast<char>(code) : '?');
                index += LSP_JSON_UNICODE_ESCAPE_HEX_DIGITS;
                break;
            }
            default:
                return std::nullopt;
        }
    }
    return result;
}

[[nodiscard]] JsonSlice lsp_json_property(const std::string_view object, const std::string_view name)
{
    base::usize index = lsp_json_skip_whitespace(object, 0);
    if (index >= object.size() || object[index] != LSP_JSON_OBJECT_OPEN) {
        return {};
    }
    ++index;
    while (index < object.size()) {
        index = lsp_json_skip_whitespace(object, index);
        if (index >= object.size() || object[index] == LSP_JSON_OBJECT_CLOSE) {
            return {};
        }
        const std::optional<base::usize> key_end = lsp_json_string_end(object, index);
        if (!key_end.has_value()) {
            return {};
        }
        const std::optional<std::string> key = lsp_json_unescape_string(object.substr(index, *key_end - index));
        if (!key.has_value()) {
            return {};
        }
        index = lsp_json_skip_whitespace(object, *key_end);
        if (index >= object.size() || object[index] != LSP_JSON_COLON) {
            return {};
        }
        const base::usize value_begin = lsp_json_skip_whitespace(object, index + 1U);
        const std::optional<base::usize> value_end = lsp_json_value_end(object, value_begin);
        if (!value_end.has_value()) {
            return {};
        }
        if (*key == name) {
            return JsonSlice{object.substr(value_begin, *value_end - value_begin), true};
        }
        index = lsp_json_skip_whitespace(object, *value_end);
        if (index < object.size() && object[index] == LSP_JSON_COMMA) {
            ++index;
        }
    }
    return {};
}

[[nodiscard]] std::optional<std::string> lsp_json_string_property(
    const std::string_view object, const std::string_view name)
{
    const JsonSlice value = lsp_json_property(object, name);
    if (!value.found) {
        return std::nullopt;
    }
    return lsp_json_unescape_string(value.text);
}

[[nodiscard]] std::optional<base::i64> lsp_json_i64_property(
    const std::string_view object, const std::string_view name)
{
    const JsonSlice value = lsp_json_property(object, name);
    if (!value.found) {
        return std::nullopt;
    }
    base::i64 parsed = 0;
    const auto [ptr, error] = std::from_chars(value.text.data(), value.text.data() + value.text.size(), parsed);
    if (error != std::errc{} || ptr != value.text.data() + value.text.size()) {
        return std::nullopt;
    }
    return parsed;
}

[[nodiscard]] std::optional<base::usize> lsp_json_usize_property(
    const std::string_view object, const std::string_view name)
{
    const std::optional<base::i64> parsed = lsp_json_i64_property(object, name);
    if (!parsed.has_value() || *parsed < 0) {
        return std::nullopt;
    }
    return static_cast<base::usize>(*parsed);
}

[[nodiscard]] JsonSlice lsp_json_first_array_element(const std::string_view array)
{
    base::usize index = lsp_json_skip_whitespace(array, 0);
    if (index >= array.size() || array[index] != LSP_JSON_ARRAY_OPEN) {
        return {};
    }
    index = lsp_json_skip_whitespace(array, index + 1U);
    if (index >= array.size() || array[index] == LSP_JSON_ARRAY_CLOSE) {
        return {};
    }
    const std::optional<base::usize> end = lsp_json_value_end(array, index);
    if (!end.has_value()) {
        return {};
    }
    return JsonSlice{array.substr(index, *end - index), true};
}

void lsp_append_json_escaped(std::string& out, const std::string_view text)
{
    out.push_back(LSP_JSON_QUOTE);
    for (const unsigned char ch : text) {
        switch (ch) {
            case LSP_JSON_QUOTE:
                out.append("\\\"");
                break;
            case LSP_JSON_ESCAPE:
                out.append("\\\\");
                break;
            case LSP_JSON_BACKSPACE:
                out.append("\\b");
                break;
            case LSP_JSON_FORM_FEED:
                out.append("\\f");
                break;
            case LSP_JSON_NEWLINE:
                out.append("\\n");
                break;
            case LSP_JSON_RETURN:
                out.append("\\r");
                break;
            case LSP_JSON_TAB:
                out.append("\\t");
                break;
            default:
                if (ch < LSP_JSON_CONTROL_LIMIT) {
                    out.append("\\u00");
                    out.push_back(
                        LSP_JSON_ESCAPE_HEX_DIGITS[(ch >> LSP_JSON_HEX_NIBBLE_SHIFT) & LSP_JSON_HEX_NIBBLE_MASK]);
                    out.push_back(LSP_JSON_ESCAPE_HEX_DIGITS[ch & LSP_JSON_HEX_NIBBLE_MASK]);
                } else {
                    out.push_back(static_cast<char>(ch));
                }
                break;
        }
    }
    out.push_back(LSP_JSON_QUOTE);
}

[[nodiscard]] std::string_view lsp_id_or_null(const std::string_view id) noexcept
{
    return id.empty() ? LSP_NULL : id;
}

[[nodiscard]] std::string lsp_response(const std::string_view id, const std::string_view result)
{
    std::string out;
    out.reserve(result.size() + id.size() + 32U);
    out.push_back(LSP_JSON_OBJECT_OPEN);
    out.append(LSP_JSONRPC_VERSION_FIELD);
    out.append(",\"id\":");
    out.append(lsp_id_or_null(id));
    out.append(",\"result\":");
    out.append(result);
    out.push_back(LSP_JSON_OBJECT_CLOSE);
    return out;
}

[[nodiscard]] std::string lsp_error_response(
    const std::string_view id, const int code, const std::string_view message)
{
    std::string out;
    out.push_back(LSP_JSON_OBJECT_OPEN);
    out.append(LSP_JSONRPC_VERSION_FIELD);
    out.append(",\"id\":");
    out.append(lsp_id_or_null(id));
    out.append(",\"error\":{\"code\":");
    out.append(std::to_string(code));
    out.append(",\"message\":");
    lsp_append_json_escaped(out, message);
    out.append("}}");
    return out;
}

[[nodiscard]] std::string lsp_notification(const std::string_view method, const std::string_view params)
{
    std::string out;
    out.push_back(LSP_JSON_OBJECT_OPEN);
    out.append(LSP_JSONRPC_VERSION_FIELD);
    out.append(",\"method\":");
    lsp_append_json_escaped(out, method);
    out.append(",\"params\":");
    out.append(params);
    out.push_back(LSP_JSON_OBJECT_CLOSE);
    return out;
}

[[nodiscard]] base::usize lsp_zero_based(const base::usize one_based) noexcept
{
    return one_based == 0U ? 0U : one_based - 1U;
}

void lsp_append_position(std::string& out, const base::LineColumn position)
{
    out.append("{\"line\":");
    out.append(std::to_string(lsp_zero_based(position.line)));
    out.append(",\"character\":");
    out.append(std::to_string(lsp_zero_based(position.column)));
    out.push_back(LSP_JSON_OBJECT_CLOSE);
}

void lsp_append_range(std::string& out, const ToolingTextRange& range)
{
    out.append("{\"start\":");
    lsp_append_position(out, range.start);
    out.append(",\"end\":");
    lsp_append_position(out, range.end);
    out.push_back(LSP_JSON_OBJECT_CLOSE);
}

[[nodiscard]] int lsp_diagnostic_severity(const base::Severity severity) noexcept
{
    switch (severity) {
        case base::Severity::error:
        case base::Severity::fatal:
            return LSP_DIAGNOSTIC_SEVERITY_ERROR;
        case base::Severity::warning:
            return LSP_DIAGNOSTIC_SEVERITY_WARNING;
        case base::Severity::note:
            return LSP_DIAGNOSTIC_SEVERITY_INFORMATION;
        case base::Severity::help:
            return LSP_DIAGNOSTIC_SEVERITY_HINT;
    }
    return LSP_DIAGNOSTIC_SEVERITY_ERROR;
}

[[nodiscard]] int lsp_symbol_kind(const std::string_view kind) noexcept
{
    if (kind == "function") {
        return LSP_SYMBOL_KIND_FUNCTION;
    }
    if (kind == "method") {
        return LSP_SYMBOL_KIND_METHOD;
    }
    if (kind == "const") {
        return LSP_SYMBOL_KIND_CONSTANT;
    }
    if (kind == "local" || kind == "value" || kind == "parameter" || kind == "global") {
        return LSP_SYMBOL_KIND_VARIABLE;
    }
    if (kind == "struct" || kind == "opaque_struct") {
        return LSP_SYMBOL_KIND_STRUCT;
    }
    if (kind == "enum") {
        return LSP_SYMBOL_KIND_ENUM;
    }
    if (kind == "enum_case") {
        return LSP_SYMBOL_KIND_ENUM_MEMBER;
    }
    if (kind == "struct_field") {
        return LSP_SYMBOL_KIND_FIELD;
    }
    if (kind == "generic_template") {
        return LSP_SYMBOL_KIND_TYPE_PARAMETER;
    }
    return LSP_SYMBOL_KIND_CLASS;
}

void lsp_append_owner_stages(std::string& out, const std::vector<IdePipelineStageOwner>& stages)
{
    out.push_back(LSP_JSON_ARRAY_OPEN);
    for (base::usize index = 0; index < stages.size(); ++index) {
        if (index != 0U) {
            out.push_back(LSP_JSON_COMMA);
        }
        const IdePipelineStageOwner& stage = stages[index];
        out.append("{\"id\":");
        lsp_append_json_escaped(out, stage.id);
        out.append(",\"profileName\":");
        lsp_append_json_escaped(out, stage.profile_name);
        out.append(",\"diagnosticOwnership\":");
        lsp_append_json_escaped(out, stage.diagnostic_ownership);
        out.push_back(LSP_JSON_OBJECT_CLOSE);
    }
    out.push_back(LSP_JSON_ARRAY_CLOSE);
}

void lsp_append_module_part(std::string& out, const IdeModulePartContext& context)
{
    out.append("{\"moduleName\":");
    lsp_append_json_escaped(out, context.module_name);
    out.append(",\"partName\":");
    lsp_append_json_escaped(out, context.part_name);
    out.append(",\"partIndex\":");
    out.append(std::to_string(context.part_index));
    out.append(",\"resolved\":");
    out.append(context.resolved ? "true" : "false");
    out.append(",\"valid\":");
    out.append(context.valid ? "true" : "false");
    out.push_back(LSP_JSON_OBJECT_CLOSE);
}

void lsp_append_diagnostic(std::string& out, const ToolingDiagnostic& diagnostic)
{
    out.append("{\"range\":");
    lsp_append_range(out, diagnostic.range);
    out.append(",\"severity\":");
    out.append(std::to_string(lsp_diagnostic_severity(diagnostic.severity)));
    out.append(",\"code\":");
    lsp_append_json_escaped(out, diagnostic.code_name);
    out.append(",\"source\":");
    lsp_append_json_escaped(out, LSP_SOURCE_NAME);
    out.append(",\"message\":");
    lsp_append_json_escaped(out, diagnostic.message);
    out.append(",\"data\":{\"category\":");
    lsp_append_json_escaped(out, diagnostic.category_name);
    out.append(",\"severity\":");
    lsp_append_json_escaped(out, diagnostic.severity_name);
    out.append(",\"ownerStages\":");
    lsp_append_owner_stages(out, diagnostic.owner_stages);
    out.append(",\"modulePart\":");
    lsp_append_module_part(out, diagnostic.source_part);
    out.append("}}");
}

[[nodiscard]] std::string lsp_publish_diagnostics(
    const std::string_view uri, const std::vector<ToolingDiagnostic>& diagnostics)
{
    std::string params;
    params.append("{\"uri\":");
    lsp_append_json_escaped(params, uri);
    params.append(",\"diagnostics\":[");
    for (base::usize index = 0; index < diagnostics.size(); ++index) {
        if (index != 0U) {
            params.push_back(LSP_JSON_COMMA);
        }
        lsp_append_diagnostic(params, diagnostics[index]);
    }
    params.append("]}");
    return lsp_notification(LSP_METHOD_PUBLISH_DIAGNOSTICS, params);
}

[[nodiscard]] std::vector<std::string> lsp_publish_current_diagnostics(
    ToolingSession& session, const ToolingDocumentId& document)
{
    base::Result<std::vector<ToolingDiagnostic>> diagnostics = session.diagnostics(document);
    if (!diagnostics) {
        return {};
    }
    return {lsp_publish_diagnostics(document.uri, diagnostics.take_value())};
}

[[nodiscard]] std::vector<std::string> lsp_publish_empty_diagnostics(const std::string_view uri)
{
    return {lsp_publish_diagnostics(uri, {})};
}

[[nodiscard]] std::optional<ToolingSourcePosition> lsp_position_from_params(const std::string_view params)
{
    const JsonSlice position = lsp_json_property(params, LSP_PROP_POSITION);
    if (!position.found) {
        return std::nullopt;
    }
    const std::optional<base::usize> line = lsp_json_usize_property(position.text, LSP_PROP_LINE);
    const std::optional<base::usize> character = lsp_json_usize_property(position.text, LSP_PROP_CHARACTER);
    if (!line.has_value() || !character.has_value()) {
        return std::nullopt;
    }
    return ToolingSourcePosition{*line, *character};
}

[[nodiscard]] std::optional<ToolingDocumentId> lsp_document_from_params(
    const std::string_view params, const ToolingProjectConfig& config)
{
    const JsonSlice text_document = lsp_json_property(params, LSP_PROP_TEXT_DOCUMENT);
    if (!text_document.found) {
        return std::nullopt;
    }
    const std::optional<std::string> uri = lsp_json_string_property(text_document.text, LSP_PROP_URI);
    if (!uri.has_value()) {
        return std::nullopt;
    }
    return tooling_document_id_from_uri(*uri, config);
}

[[nodiscard]] std::optional<base::i64> lsp_document_version_from_text_document(const std::string_view text_document)
{
    return lsp_json_i64_property(text_document, LSP_PROP_VERSION);
}

[[nodiscard]] std::optional<base::i64> lsp_document_version_from_params(const std::string_view params)
{
    const JsonSlice text_document = lsp_json_property(params, LSP_PROP_TEXT_DOCUMENT);
    if (!text_document.found) {
        return std::nullopt;
    }
    return lsp_document_version_from_text_document(text_document.text);
}

[[nodiscard]] std::optional<std::string> lsp_content_change_full_text(const std::string_view params)
{
    const JsonSlice changes = lsp_json_property(params, LSP_PROP_CONTENT_CHANGES);
    if (!changes.found) {
        return std::nullopt;
    }
    const JsonSlice first = lsp_json_first_array_element(changes.text);
    if (!first.found) {
        return std::nullopt;
    }
    return lsp_json_string_property(first.text, LSP_PROP_TEXT);
}

[[nodiscard]] std::string lsp_location_json(const std::string_view uri, const ToolingTextRange& range)
{
    std::string out;
    out.append("{\"uri\":");
    lsp_append_json_escaped(out, range.path.empty() ? uri : tooling_file_uri_from_path(range.path));
    out.append(",\"range\":");
    lsp_append_range(out, range);
    out.push_back(LSP_JSON_OBJECT_CLOSE);
    return out;
}

[[nodiscard]] std::string lsp_hover_result(const ToolingHover& hover)
{
    std::string out;
    out.append("{\"contents\":{\"kind\":");
    lsp_append_json_escaped(out, LSP_MARKUP_KIND_PLAINTEXT);
    out.append(",\"value\":");
    lsp_append_json_escaped(out, hover.label);
    out.append("},\"range\":");
    lsp_append_range(out, hover.range);
    if (!hover.semantic_fact_key.empty() || !hover.semantic_fact_detail.empty()) {
        out.append(",\"data\":{\"semanticFactKey\":");
        lsp_append_json_escaped(out, hover.semantic_fact_key);
        out.append(",\"semanticFactDetail\":");
        lsp_append_json_escaped(out, hover.semantic_fact_detail);
        out.push_back(LSP_JSON_OBJECT_CLOSE);
    }
    out.push_back(LSP_JSON_OBJECT_CLOSE);
    return out;
}

[[nodiscard]] std::string lsp_document_symbol_result(const std::vector<ToolingDocumentSymbol>& symbols)
{
    std::string out;
    out.push_back(LSP_JSON_ARRAY_OPEN);
    for (base::usize index = 0; index < symbols.size(); ++index) {
        if (index != 0U) {
            out.push_back(LSP_JSON_COMMA);
        }
        const ToolingDocumentSymbol& symbol = symbols[index];
        out.append("{\"name\":");
        lsp_append_json_escaped(out, symbol.name);
        out.append(",\"kind\":");
        out.append(std::to_string(lsp_symbol_kind(symbol.kind)));
        out.append(",\"detail\":");
        lsp_append_json_escaped(out, symbol.detail);
        out.append(",\"range\":");
        lsp_append_range(out, symbol.range);
        out.append(",\"selectionRange\":");
        lsp_append_range(out, symbol.selection_range);
        out.append(",\"data\":{\"aurexKind\":");
        lsp_append_json_escaped(out, symbol.kind);
        out.append(",\"stableQueryKey\":");
        lsp_append_json_escaped(out, symbol.stable_query_key);
        out.append(",\"stableDefinitionKey\":");
        lsp_append_json_escaped(out, symbol.stable_definition_key);
        out.append(",\"partIndex\":");
        out.append(std::to_string(symbol.part_index));
        out.append(",\"checked\":");
        out.append(symbol.checked ? "true" : "false");
        out.append("}}");
    }
    out.push_back(LSP_JSON_ARRAY_CLOSE);
    return out;
}

[[nodiscard]] bool lsp_header_name_matches(const std::string_view line, const std::string_view name) noexcept
{
    return line.size() >= name.size() && line.substr(0, name.size()) == name;
}

[[nodiscard]] base::Result<base::usize> lsp_parse_content_length(const std::string_view header)
{
    base::usize search = 0;
    while (search <= header.size()) {
        const base::usize line_end = header.find(LSP_LINE_SEPARATOR, search);
        const std::string_view line = line_end == std::string_view::npos ? header.substr(search)
                                                                         : header.substr(search, line_end - search);
        if (lsp_header_name_matches(line, LSP_HEADER_CONTENT_LENGTH)) {
            const base::usize colon = line.find(LSP_HEADER_COLON);
            if (colon == std::string_view::npos) {
                break;
            }
            std::string_view number = line.substr(colon + 1U);
            const base::usize begin = lsp_json_skip_whitespace(number, 0);
            number = number.substr(begin);
            base::usize length = 0;
            const auto [ptr, error] = std::from_chars(number.data(), number.data() + number.size(), length);
            if (error == std::errc{}) {
                return base::Result<base::usize>::ok(length);
            }
            break;
        }
        if (line_end == std::string_view::npos) {
            break;
        }
        search = line_end + LSP_LINE_SEPARATOR.size();
    }
    return base::Result<base::usize>::fail({base::ErrorCode::invalid_source, "missing LSP Content-Length header"});
}

} // namespace

base::Result<std::vector<LspContentMessage>> parse_lsp_content_messages(const std::string_view bytes)
{
    std::vector<LspContentMessage> messages;
    base::usize cursor = 0;
    while (cursor < bytes.size()) {
        const base::usize header_end = bytes.find(LSP_HEADER_SEPARATOR, cursor);
        if (header_end == std::string_view::npos) {
            return base::Result<std::vector<LspContentMessage>>::fail(
                {base::ErrorCode::invalid_source, "incomplete LSP header"});
        }
        const std::string_view header = bytes.substr(cursor, header_end - cursor);
        base::Result<base::usize> content_length = lsp_parse_content_length(header);
        if (!content_length) {
            return base::Result<std::vector<LspContentMessage>>::fail(content_length.error());
        }
        const base::usize body_begin = header_end + LSP_HEADER_SEPARATOR_SIZE;
        const base::usize body_end = body_begin + content_length.value();
        if (body_end > bytes.size()) {
            return base::Result<std::vector<LspContentMessage>>::fail(
                {base::ErrorCode::invalid_source, "incomplete LSP body"});
        }
        messages.push_back(LspContentMessage{std::string(bytes.substr(body_begin, content_length.value()))});
        cursor = body_end;
    }
    return base::Result<std::vector<LspContentMessage>>::ok(std::move(messages));
}

std::string write_lsp_content_message(const std::string_view body)
{
    std::string frame;
    frame.append(LSP_HEADER_CONTENT_LENGTH);
    frame.append(": ");
    frame.append(std::to_string(body.size()));
    frame.append(LSP_HEADER_SEPARATOR);
    frame.append(body);
    return frame;
}

LspServer::LspServer() = default;

LspServer::LspServer(ToolingProjectConfig config) : session_(std::move(config))
{
}

std::vector<std::string> LspServer::handle_json_message(const std::string_view body)
{
    const std::optional<std::string> method = lsp_json_string_property(body, LSP_PROP_METHOD);
    const JsonSlice id = lsp_json_property(body, LSP_PROP_ID);
    const JsonSlice params = lsp_json_property(body, LSP_PROP_PARAMS);
    if (!method.has_value()) {
        return id.found ? std::vector<std::string>{lsp_error_response(id.text, LSP_ERROR_INVALID_REQUEST,
                              "JSON-RPC request is missing method")}
                        : std::vector<std::string>{};
    }
    if (*method == LSP_METHOD_INITIALIZE) {
        return this->handle_initialize(id.text);
    }
    if (*method == LSP_METHOD_INITIALIZED) {
        this->initialized_ = true;
        return {};
    }
    if (*method == LSP_METHOD_SHUTDOWN) {
        return this->handle_shutdown(id.text);
    }
    if (*method == LSP_METHOD_EXIT) {
        this->exited_ = true;
        return {};
    }
    if (*method == LSP_METHOD_CANCEL) {
        return {};
    }
    if (*method == LSP_METHOD_DID_OPEN) {
        return params.found ? this->handle_did_open(params.text) : std::vector<std::string>{};
    }
    if (*method == LSP_METHOD_DID_CHANGE) {
        return params.found ? this->handle_did_change(params.text) : std::vector<std::string>{};
    }
    if (*method == LSP_METHOD_DID_CLOSE) {
        return params.found ? this->handle_did_close(params.text) : std::vector<std::string>{};
    }
    if (*method == LSP_METHOD_HOVER) {
        return params.found ? this->handle_hover(id.text, params.text)
                            : std::vector<std::string>{lsp_response(id.text, LSP_NULL)};
    }
    if (*method == LSP_METHOD_DEFINITION) {
        return params.found ? this->handle_definition(id.text, params.text)
                            : std::vector<std::string>{lsp_response(id.text, LSP_NULL)};
    }
    if (*method == LSP_METHOD_REFERENCES) {
        return params.found ? this->handle_references(id.text, params.text)
                            : std::vector<std::string>{lsp_response(id.text, "[]")};
    }
    if (*method == LSP_METHOD_DOCUMENT_SYMBOL) {
        return params.found ? this->handle_document_symbols(id.text, params.text)
                            : std::vector<std::string>{lsp_response(id.text, "[]")};
    }
    return id.found
        ? std::vector<std::string>{lsp_error_response(id.text, LSP_ERROR_METHOD_NOT_FOUND, "method not found")}
        : std::vector<std::string>{};
}

base::Result<std::string> LspServer::handle_framed_messages(const std::string_view bytes)
{
    base::Result<std::vector<LspContentMessage>> parsed = parse_lsp_content_messages(bytes);
    if (!parsed) {
        return base::Result<std::string>::fail(parsed.error());
    }
    std::string output;
    for (const LspContentMessage& message : parsed.value()) {
        const std::vector<std::string> responses = this->handle_json_message(message.body);
        for (const std::string& response : responses) {
            output.append(write_lsp_content_message(response));
        }
    }
    return base::Result<std::string>::ok(std::move(output));
}

bool LspServer::initialized() const noexcept
{
    return this->initialized_;
}

bool LspServer::shutdown_requested() const noexcept
{
    return this->shutdown_requested_;
}

bool LspServer::exited() const noexcept
{
    return this->exited_;
}

ToolingSession& LspServer::session() noexcept
{
    return this->session_;
}

const ToolingSession& LspServer::session() const noexcept
{
    return this->session_;
}

std::vector<std::string> LspServer::handle_initialize(const std::string_view id)
{
    std::ostringstream result;
    result << "{\"capabilities\":{\"textDocumentSync\":{\"openClose\":true,\"change\":"
           << LSP_TEXT_DOCUMENT_SYNC_FULL
           << "},\"hoverProvider\":true,\"definitionProvider\":true,\"referencesProvider\":true,"
              "\"documentSymbolProvider\":true}}";
    return {lsp_response(id, result.str())};
}

std::vector<std::string> LspServer::handle_shutdown(const std::string_view id)
{
    this->shutdown_requested_ = true;
    return {lsp_response(id, LSP_NULL)};
}

std::vector<std::string> LspServer::handle_did_open(const std::string_view params)
{
    const JsonSlice text_document = lsp_json_property(params, LSP_PROP_TEXT_DOCUMENT);
    if (!text_document.found) {
        return {};
    }
    const std::optional<std::string> uri = lsp_json_string_property(text_document.text, LSP_PROP_URI);
    const std::optional<std::string> text = lsp_json_string_property(text_document.text, LSP_PROP_TEXT);
    if (!uri.has_value() || !text.has_value()) {
        return {};
    }
    ToolingDocumentId document = tooling_document_id_from_uri(*uri, this->session_.project_config());
    const std::optional<base::i64> version = lsp_document_version_from_text_document(text_document.text);
    base::Result<ToolingDocumentVersion> opened = this->session_.open_document(document, *text, version);
    if (!opened) {
        return {};
    }
    return lsp_publish_current_diagnostics(this->session_, document);
}

std::vector<std::string> LspServer::handle_did_change(const std::string_view params)
{
    const std::optional<ToolingDocumentId> document = lsp_document_from_params(params, this->session_.project_config());
    const std::optional<std::string> text = lsp_content_change_full_text(params);
    if (!document.has_value() || !text.has_value()) {
        return {};
    }
    const std::optional<base::i64> version = lsp_document_version_from_params(params);
    base::Result<ToolingDocumentVersion> changed = this->session_.change_document(*document, *text, version);
    if (!changed) {
        return {};
    }
    return lsp_publish_current_diagnostics(this->session_, *document);
}

std::vector<std::string> LspServer::handle_did_close(const std::string_view params)
{
    const std::optional<ToolingDocumentId> document = lsp_document_from_params(params, this->session_.project_config());
    if (!document.has_value()) {
        return {};
    }
    static_cast<void>(this->session_.close_document(*document));
    return lsp_publish_empty_diagnostics(document->uri);
}

std::vector<std::string> LspServer::handle_hover(const std::string_view id, const std::string_view params)
{
    const std::optional<ToolingDocumentId> document = lsp_document_from_params(params, this->session_.project_config());
    const std::optional<ToolingSourcePosition> position = lsp_position_from_params(params);
    if (!document.has_value() || !position.has_value()) {
        return {lsp_response(id, LSP_NULL)};
    }
    base::Result<std::optional<ToolingHover>> hover = this->session_.hover_at_position(*document, *position);
    if (!hover || !hover.value().has_value()) {
        return {lsp_response(id, LSP_NULL)};
    }
    return {lsp_response(id, lsp_hover_result(*hover.value()))};
}

std::vector<std::string> LspServer::handle_definition(const std::string_view id, const std::string_view params)
{
    const std::optional<ToolingDocumentId> document = lsp_document_from_params(params, this->session_.project_config());
    const std::optional<ToolingSourcePosition> position = lsp_position_from_params(params);
    if (!document.has_value() || !position.has_value()) {
        return {lsp_response(id, LSP_NULL)};
    }
    base::Result<std::optional<ToolingDefinition>> definition =
        this->session_.definition_at_position(*document, *position);
    if (!definition || !definition.value().has_value()) {
        return {lsp_response(id, LSP_NULL)};
    }
    return {lsp_response(id, lsp_location_json(document->uri, definition.value()->range))};
}

std::vector<std::string> LspServer::handle_references(const std::string_view id, const std::string_view params)
{
    const std::optional<ToolingDocumentId> document = lsp_document_from_params(params, this->session_.project_config());
    const std::optional<ToolingSourcePosition> position = lsp_position_from_params(params);
    if (!document.has_value() || !position.has_value()) {
        return {lsp_response(id, "[]")};
    }
    base::Result<std::vector<ToolingReference>> references = this->session_.references_at_position(*document, *position);
    if (!references) {
        return {lsp_response(id, "[]")};
    }
    std::string result;
    result.push_back(LSP_JSON_ARRAY_OPEN);
    for (base::usize index = 0; index < references.value().size(); ++index) {
        if (index != 0U) {
            result.push_back(LSP_JSON_COMMA);
        }
        result.append(lsp_location_json(document->uri, references.value()[index].range));
    }
    result.push_back(LSP_JSON_ARRAY_CLOSE);
    return {lsp_response(id, result)};
}

std::vector<std::string> LspServer::handle_document_symbols(const std::string_view id, const std::string_view params)
{
    const std::optional<ToolingDocumentId> document = lsp_document_from_params(params, this->session_.project_config());
    if (!document.has_value()) {
        return {lsp_response(id, "[]")};
    }
    base::Result<std::vector<ToolingDocumentSymbol>> symbols = this->session_.document_symbols(*document);
    if (!symbols) {
        return {lsp_response(id, "[]")};
    }
    return {lsp_response(id, lsp_document_symbol_result(symbols.value()))};
}

} // namespace aurex::tooling
