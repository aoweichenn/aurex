#include <aurex/application/tooling/lsp.hpp>
#include <aurex/infrastructure/base/diagnostic.hpp>

#include <algorithm>
#include <array>
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
constexpr std::string_view LSP_METHOD_COMPLETION = "textDocument/completion";
constexpr std::string_view LSP_METHOD_RENAME = "textDocument/rename";
constexpr std::string_view LSP_METHOD_SEMANTIC_TOKENS_FULL = "textDocument/semanticTokens/full";
constexpr std::string_view LSP_METHOD_CODE_ACTION = "textDocument/codeAction";
constexpr std::string_view LSP_METHOD_WORKSPACE_SYMBOL = "workspace/symbol";
constexpr std::string_view LSP_METHOD_INLAY_HINT = "textDocument/inlayHint";
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
constexpr std::string_view LSP_PROP_NEW_NAME = "newName";
constexpr std::string_view LSP_PROP_QUERY = "query";
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
constexpr unsigned int LSP_JSON_SURROGATE_SHIFT = 10U;
constexpr unsigned char LSP_JSON_HEX_NIBBLE_MASK = 0x0FU;
constexpr int LSP_DECIMAL_DIGIT_COUNT = 10;
constexpr unsigned int LSP_JSON_CONTROL_LIMIT = 0x20U;
constexpr unsigned int LSP_JSON_ASCII_LIMIT = 0x7FU;
constexpr unsigned int LSP_JSON_UTF8_TWO_BYTE_LIMIT = 0x7FFU;
constexpr unsigned int LSP_JSON_UTF8_THREE_BYTE_LIMIT = 0xFFFFU;
constexpr unsigned int LSP_UNICODE_HIGH_SURROGATE_MIN = 0xD800U;
constexpr unsigned int LSP_UNICODE_HIGH_SURROGATE_MAX = 0xDBFFU;
constexpr unsigned int LSP_UNICODE_LOW_SURROGATE_MIN = 0xDC00U;
constexpr unsigned int LSP_UNICODE_LOW_SURROGATE_MAX = 0xDFFFU;
constexpr unsigned int LSP_UNICODE_SUPPLEMENTARY_CODE_POINT_MIN = 0x10000U;
constexpr unsigned int LSP_UNICODE_MAX_CODE_POINT = 0x10FFFFU;
constexpr unsigned int LSP_UNICODE_SURROGATE_CODE_POINT_MASK = 0x3FFU;
constexpr unsigned int LSP_UTF8_CONTINUATION_VALUE_MASK = 0x3FU;
constexpr unsigned int LSP_UTF8_CONTINUATION_BYTE_TAG = 0x80U;
constexpr unsigned int LSP_UTF8_CONTINUATION_BYTE_MASK = 0xC0U;
constexpr unsigned int LSP_UTF8_TWO_BYTE_TAG = 0xC0U;
constexpr unsigned int LSP_UTF8_THREE_BYTE_TAG = 0xE0U;
constexpr unsigned int LSP_UTF8_FOUR_BYTE_TAG = 0xF0U;
constexpr unsigned int LSP_UTF8_TWO_BYTE_LEAD_MIN = 0xC2U;
constexpr unsigned int LSP_UTF8_TWO_BYTE_LEAD_MAX = 0xDFU;
constexpr unsigned int LSP_UTF8_THREE_BYTE_LEAD_MIN = 0xE0U;
constexpr unsigned int LSP_UTF8_THREE_BYTE_LEAD_MAX = 0xEFU;
constexpr unsigned int LSP_UTF8_SURROGATE_LEAD = 0xEDU;
constexpr unsigned int LSP_UTF8_FOUR_BYTE_LEAD_MIN = 0xF0U;
constexpr unsigned int LSP_UTF8_FOUR_BYTE_LEAD_MAX = 0xF4U;
constexpr unsigned int LSP_UTF8_TWO_BYTE_LEAD_VALUE_MASK = 0x1FU;
constexpr unsigned int LSP_UTF8_THREE_BYTE_LEAD_VALUE_MASK = 0x0FU;
constexpr unsigned int LSP_UTF8_FOUR_BYTE_LEAD_VALUE_MASK = 0x07U;
constexpr unsigned int LSP_UTF8_E0_SECOND_MIN = 0xA0U;
constexpr unsigned int LSP_UTF8_ED_SECOND_MAX = 0x9FU;
constexpr unsigned int LSP_UTF8_F0_SECOND_MIN = 0x90U;
constexpr unsigned int LSP_UTF8_F4_SECOND_MAX = 0x8FU;
constexpr unsigned int LSP_UTF8_CONTINUATION_SHIFT = 6U;
constexpr unsigned int LSP_UTF8_TWO_BYTE_SHIFT = 6U;
constexpr unsigned int LSP_UTF8_THREE_BYTE_SHIFT = 12U;
constexpr unsigned int LSP_UTF8_FOUR_BYTE_SHIFT = 18U;
constexpr base::usize LSP_UTF8_TWO_BYTE_LENGTH = 2;
constexpr base::usize LSP_UTF8_THREE_BYTE_LENGTH = 3;
constexpr base::usize LSP_UTF8_FOUR_BYTE_LENGTH = 4;
constexpr base::usize LSP_ONE_BASED_MINIMUM = 1;
constexpr unsigned int LSP_UTF16_SURROGATE_UNIT_COUNT = 2U;
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
constexpr int LSP_SYMBOL_KIND_INTERFACE = 11;
constexpr int LSP_SYMBOL_KIND_FUNCTION = 12;
constexpr int LSP_SYMBOL_KIND_VARIABLE = 13;
constexpr int LSP_SYMBOL_KIND_CONSTANT = 14;
constexpr int LSP_SYMBOL_KIND_ENUM_MEMBER = 22;
constexpr int LSP_SYMBOL_KIND_STRUCT = 23;
constexpr int LSP_SYMBOL_KIND_TYPE_PARAMETER = 26;
constexpr int LSP_COMPLETION_ITEM_KIND_METHOD = 2;
constexpr int LSP_COMPLETION_ITEM_KIND_FUNCTION = 3;
constexpr int LSP_COMPLETION_ITEM_KIND_FIELD = 5;
constexpr int LSP_COMPLETION_ITEM_KIND_VARIABLE = 6;
constexpr int LSP_COMPLETION_ITEM_KIND_CLASS = 7;
constexpr int LSP_COMPLETION_ITEM_KIND_INTERFACE = 8;
constexpr int LSP_COMPLETION_ITEM_KIND_MODULE = 9;
constexpr int LSP_COMPLETION_ITEM_KIND_VALUE = 12;
constexpr int LSP_COMPLETION_ITEM_KIND_ENUM = 13;
constexpr int LSP_COMPLETION_ITEM_KIND_KEYWORD = 14;
constexpr int LSP_COMPLETION_ITEM_KIND_CONSTANT = 21;
constexpr int LSP_COMPLETION_ITEM_KIND_ENUM_MEMBER = 20;
constexpr int LSP_COMPLETION_ITEM_KIND_TYPE_PARAMETER = 25;
constexpr int LSP_INLAY_HINT_KIND_TYPE = 1;
constexpr int LSP_SEMANTIC_TOKEN_MODIFIER_DECLARATION_BIT = 1;
constexpr int LSP_SEMANTIC_TOKEN_MODIFIER_DEFINITION_BIT = 2;
constexpr int LSP_SEMANTIC_TOKEN_MODIFIER_READONLY_BIT = 4;

constexpr auto LSP_SEMANTIC_TOKEN_TYPES = std::to_array<std::string_view>({
    "namespace",
    "type",
    "class",
    "enum",
    "interface",
    "struct",
    "typeParameter",
    "parameter",
    "variable",
    "property",
    "enumMember",
    "event",
    "function",
    "method",
    "macro",
    "keyword",
    "modifier",
    "comment",
    "string",
    "number",
    "regexp",
    "operator",
    "decorator",
    "punctuation",
});

constexpr auto LSP_SEMANTIC_TOKEN_MODIFIERS = std::to_array<std::string_view>({
    "declaration",
    "definition",
    "readonly",
});

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
        if (static_cast<unsigned char>(ch) < LSP_JSON_CONTROL_LIMIT) {
            return std::nullopt;
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
    if (begin >= text.size() || (text[begin] != LSP_JSON_OBJECT_OPEN && text[begin] != LSP_JSON_ARRAY_OPEN)) {
        return std::nullopt;
    }
    std::vector<char> closers;
    closers.reserve(1U);
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
            } else if (static_cast<unsigned char>(ch) < LSP_JSON_CONTROL_LIMIT) {
                return std::nullopt;
            }
            continue;
        }
        if (ch == LSP_JSON_QUOTE) {
            in_string = true;
            continue;
        }
        if (ch == LSP_JSON_OBJECT_OPEN) {
            closers.push_back(LSP_JSON_OBJECT_CLOSE);
            continue;
        }
        if (ch == LSP_JSON_ARRAY_OPEN) {
            closers.push_back(LSP_JSON_ARRAY_CLOSE);
            continue;
        }
        if (ch == LSP_JSON_OBJECT_CLOSE || ch == LSP_JSON_ARRAY_CLOSE) {
            if (closers.empty() || closers.back() != ch) {
                return std::nullopt;
            }
            closers.pop_back();
            if (closers.empty()) {
                return index + 1U;
            }
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<base::usize> lsp_json_scalar_end(const std::string_view text, base::usize begin) noexcept
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

[[nodiscard]] bool lsp_json_is_high_surrogate(const unsigned int code) noexcept
{
    return code >= LSP_UNICODE_HIGH_SURROGATE_MIN && code <= LSP_UNICODE_HIGH_SURROGATE_MAX;
}

[[nodiscard]] bool lsp_json_is_low_surrogate(const unsigned int code) noexcept
{
    return code >= LSP_UNICODE_LOW_SURROGATE_MIN && code <= LSP_UNICODE_LOW_SURROGATE_MAX;
}

[[nodiscard]] bool lsp_append_utf8_code_point(std::string& out, const unsigned int code)
{
    if (code > LSP_UNICODE_MAX_CODE_POINT || lsp_json_is_high_surrogate(code) || lsp_json_is_low_surrogate(code)) {
        return false;
    }
    if (code <= LSP_JSON_ASCII_LIMIT) {
        out.push_back(static_cast<char>(code));
        return true;
    }
    if (code <= LSP_JSON_UTF8_TWO_BYTE_LIMIT) {
        out.push_back(static_cast<char>(LSP_UTF8_TWO_BYTE_TAG | (code >> LSP_UTF8_TWO_BYTE_SHIFT)));
        out.push_back(static_cast<char>(LSP_UTF8_CONTINUATION_BYTE_TAG | (code & LSP_UTF8_CONTINUATION_VALUE_MASK)));
        return true;
    }
    if (code <= LSP_JSON_UTF8_THREE_BYTE_LIMIT) {
        out.push_back(static_cast<char>(LSP_UTF8_THREE_BYTE_TAG | (code >> LSP_UTF8_THREE_BYTE_SHIFT)));
        out.push_back(static_cast<char>(LSP_UTF8_CONTINUATION_BYTE_TAG
            | ((code >> LSP_UTF8_CONTINUATION_SHIFT) & LSP_UTF8_CONTINUATION_VALUE_MASK)));
        out.push_back(static_cast<char>(LSP_UTF8_CONTINUATION_BYTE_TAG | (code & LSP_UTF8_CONTINUATION_VALUE_MASK)));
        return true;
    }
    out.push_back(static_cast<char>(LSP_UTF8_FOUR_BYTE_TAG | (code >> LSP_UTF8_FOUR_BYTE_SHIFT)));
    out.push_back(static_cast<char>(
        LSP_UTF8_CONTINUATION_BYTE_TAG | ((code >> LSP_UTF8_THREE_BYTE_SHIFT) & LSP_UTF8_CONTINUATION_VALUE_MASK)));
    out.push_back(static_cast<char>(
        LSP_UTF8_CONTINUATION_BYTE_TAG | ((code >> LSP_UTF8_CONTINUATION_SHIFT) & LSP_UTF8_CONTINUATION_VALUE_MASK)));
    out.push_back(static_cast<char>(LSP_UTF8_CONTINUATION_BYTE_TAG | (code & LSP_UTF8_CONTINUATION_VALUE_MASK)));
    return true;
}

[[nodiscard]] std::optional<unsigned int> lsp_json_read_unicode_escape(
    const std::string_view value, const base::usize marker)
{
    if (marker >= value.size() || value[marker] != 'u' || value.size() - marker <= LSP_JSON_UNICODE_ESCAPE_HEX_DIGITS) {
        return std::nullopt;
    }
    unsigned int code = 0;
    for (base::usize digit = 0; digit < LSP_JSON_UNICODE_ESCAPE_HEX_DIGITS; ++digit) {
        const int nibble = lsp_hex_value(value[marker + 1U + digit]);
        if (nibble < 0) {
            return std::nullopt;
        }
        code = (code << LSP_JSON_HEX_NIBBLE_SHIFT) | static_cast<unsigned int>(nibble);
    }
    return code;
}

[[nodiscard]] unsigned int lsp_json_combine_surrogates(const unsigned int high, const unsigned int low) noexcept
{
    return LSP_UNICODE_SUPPLEMENTARY_CODE_POINT_MIN
        + (((high - LSP_UNICODE_HIGH_SURROGATE_MIN) & LSP_UNICODE_SURROGATE_CODE_POINT_MASK)
            << LSP_JSON_SURROGATE_SHIFT)
        + ((low - LSP_UNICODE_LOW_SURROGATE_MIN) & LSP_UNICODE_SURROGATE_CODE_POINT_MASK);
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
            if (static_cast<unsigned char>(ch) < LSP_JSON_CONTROL_LIMIT) {
                return std::nullopt;
            }
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
                const std::optional<unsigned int> first = lsp_json_read_unicode_escape(value, index);
                if (!first.has_value()) {
                    return std::nullopt;
                }
                unsigned int code = *first;
                if (lsp_json_is_high_surrogate(code)) {
                    const base::usize low_escape = index + LSP_JSON_UNICODE_ESCAPE_HEX_DIGITS + 1U;
                    if (low_escape + 1U >= value.size() || value[low_escape] != LSP_JSON_ESCAPE
                        || value[low_escape + 1U] != 'u') {
                        return std::nullopt;
                    }
                    const std::optional<unsigned int> low = lsp_json_read_unicode_escape(value, low_escape + 1U);
                    if (!low.has_value() || !lsp_json_is_low_surrogate(*low)) {
                        return std::nullopt;
                    }
                    code = lsp_json_combine_surrogates(code, *low);
                    index = low_escape + 1U + LSP_JSON_UNICODE_ESCAPE_HEX_DIGITS;
                } else {
                    if (lsp_json_is_low_surrogate(code)) {
                        return std::nullopt;
                    }
                    index += LSP_JSON_UNICODE_ESCAPE_HEX_DIGITS;
                }
                if (!lsp_append_utf8_code_point(result, code)) {
                    return std::nullopt;
                }
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

[[nodiscard]] std::optional<base::i64> lsp_json_i64_property(const std::string_view object, const std::string_view name)
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

[[nodiscard]] std::string lsp_error_response(const std::string_view id, const int code, const std::string_view message)
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

struct LspUtf8CodePoint {
    unsigned int code = 0;
    base::usize next = 0;
};

[[nodiscard]] bool lsp_utf8_is_continuation(const unsigned char ch) noexcept
{
    return (static_cast<unsigned int>(ch) & LSP_UTF8_CONTINUATION_BYTE_MASK) == LSP_UTF8_CONTINUATION_BYTE_TAG;
}

[[nodiscard]] bool lsp_utf8_continuation_available(
    const std::string_view text, const base::usize index, const base::usize end) noexcept
{
    return index < end && lsp_utf8_is_continuation(static_cast<unsigned char>(text[index]));
}

[[nodiscard]] LspUtf8CodePoint lsp_decode_utf8_code_point(
    const std::string_view text, const base::usize index, const base::usize end) noexcept
{
    const unsigned int first = static_cast<unsigned char>(text[index]);
    if (first <= LSP_JSON_ASCII_LIMIT) {
        return {first, index + 1U};
    }
    if (first >= LSP_UTF8_TWO_BYTE_LEAD_MIN && first <= LSP_UTF8_TWO_BYTE_LEAD_MAX
        && lsp_utf8_continuation_available(text, index + 1U, end)) {
        const unsigned int second = static_cast<unsigned char>(text[index + 1U]);
        const unsigned int code = ((first & LSP_UTF8_TWO_BYTE_LEAD_VALUE_MASK) << LSP_UTF8_TWO_BYTE_SHIFT)
            | (second & LSP_UTF8_CONTINUATION_VALUE_MASK);
        return {code, index + LSP_UTF8_TWO_BYTE_LENGTH};
    }
    if (first >= LSP_UTF8_THREE_BYTE_LEAD_MIN && first <= LSP_UTF8_THREE_BYTE_LEAD_MAX
        && lsp_utf8_continuation_available(text, index + 1U, end)
        && lsp_utf8_continuation_available(text, index + 2U, end)) {
        const unsigned int second = static_cast<unsigned char>(text[index + 1U]);
        const unsigned int third = static_cast<unsigned char>(text[index + 2U]);
        const bool valid_second = (first != LSP_UTF8_THREE_BYTE_LEAD_MIN || second >= LSP_UTF8_E0_SECOND_MIN)
            && (first != LSP_UTF8_SURROGATE_LEAD || second <= LSP_UTF8_ED_SECOND_MAX);
        if (valid_second) {
            const unsigned int code = ((first & LSP_UTF8_THREE_BYTE_LEAD_VALUE_MASK) << LSP_UTF8_THREE_BYTE_SHIFT)
                | ((second & LSP_UTF8_CONTINUATION_VALUE_MASK) << LSP_UTF8_CONTINUATION_SHIFT)
                | (third & LSP_UTF8_CONTINUATION_VALUE_MASK);
            return {code, index + LSP_UTF8_THREE_BYTE_LENGTH};
        }
    }
    if (first >= LSP_UTF8_FOUR_BYTE_LEAD_MIN && first <= LSP_UTF8_FOUR_BYTE_LEAD_MAX
        && lsp_utf8_continuation_available(text, index + 1U, end)
        && lsp_utf8_continuation_available(text, index + 2U, end)
        && lsp_utf8_continuation_available(text, index + 3U, end)) {
        const unsigned int second = static_cast<unsigned char>(text[index + 1U]);
        const unsigned int third = static_cast<unsigned char>(text[index + 2U]);
        const unsigned int fourth = static_cast<unsigned char>(text[index + 3U]);
        const bool valid_second = (first != LSP_UTF8_FOUR_BYTE_LEAD_MIN || second >= LSP_UTF8_F0_SECOND_MIN)
            && (first != LSP_UTF8_FOUR_BYTE_LEAD_MAX || second <= LSP_UTF8_F4_SECOND_MAX);
        if (valid_second) {
            const unsigned int code = ((first & LSP_UTF8_FOUR_BYTE_LEAD_VALUE_MASK) << LSP_UTF8_FOUR_BYTE_SHIFT)
                | ((second & LSP_UTF8_CONTINUATION_VALUE_MASK) << LSP_UTF8_THREE_BYTE_SHIFT)
                | ((third & LSP_UTF8_CONTINUATION_VALUE_MASK) << LSP_UTF8_CONTINUATION_SHIFT)
                | (fourth & LSP_UTF8_CONTINUATION_VALUE_MASK);
            return {code, index + LSP_UTF8_FOUR_BYTE_LENGTH};
        }
    }
    return {first, index + 1U};
}

[[nodiscard]] base::usize lsp_utf16_units_for_code_point(const unsigned int code) noexcept
{
    return code >= LSP_UNICODE_SUPPLEMENTARY_CODE_POINT_MIN ? LSP_UTF16_SURROGATE_UNIT_COUNT : 1U;
}

struct LspLineBounds {
    base::usize begin = 0;
    base::usize end = 0;
    base::usize line = 0;
};

[[nodiscard]] LspLineBounds lsp_line_bounds_for_line(
    const std::string_view text, const base::usize target_line) noexcept
{
    base::usize line = 0;
    base::usize begin = 0;
    for (base::usize index = 0; index < text.size() && line < target_line; ++index) {
        if (text[index] == LSP_JSON_NEWLINE) {
            ++line;
            begin = index + 1U;
        }
    }
    if (line < target_line) {
        return {text.size(), text.size(), line};
    }
    base::usize end = text.size();
    for (base::usize index = begin; index < text.size(); ++index) {
        if (text[index] == LSP_JSON_NEWLINE) {
            end = index;
            break;
        }
    }
    return {begin, end, line};
}

[[nodiscard]] LspLineBounds lsp_line_bounds_for_offset(const std::string_view text, const base::usize offset) noexcept
{
    const base::usize clamped = std::min(offset, text.size());
    base::usize line = 0;
    base::usize begin = 0;
    for (base::usize index = 0; index < clamped; ++index) {
        if (text[index] == LSP_JSON_NEWLINE) {
            ++line;
            begin = index + 1U;
        }
    }
    base::usize end = text.size();
    for (base::usize index = begin; index < text.size(); ++index) {
        if (text[index] == LSP_JSON_NEWLINE) {
            end = index;
            break;
        }
    }
    return {begin, end, line};
}

[[nodiscard]] base::usize lsp_utf16_character_for_byte_offset(
    const std::string_view text, const base::usize offset) noexcept
{
    const LspLineBounds bounds = lsp_line_bounds_for_offset(text, offset);
    const base::usize clamped = std::min(offset, bounds.end);
    base::usize character = 0;
    for (base::usize index = bounds.begin; index < clamped;) {
        const LspUtf8CodePoint decoded = lsp_decode_utf8_code_point(text, index, bounds.end);
        if (decoded.next > clamped) {
            break;
        }
        character += lsp_utf16_units_for_code_point(decoded.code);
        index = decoded.next;
    }
    return character;
}

[[nodiscard]] ToolingSourcePosition lsp_position_for_byte_offset(
    const std::string_view text, const base::usize offset) noexcept
{
    const LspLineBounds bounds = lsp_line_bounds_for_offset(text, offset);
    return ToolingSourcePosition{bounds.line, lsp_utf16_character_for_byte_offset(text, offset)};
}

[[nodiscard]] base::usize lsp_byte_offset_for_utf16_position(
    const std::string_view text, const ToolingSourcePosition position) noexcept
{
    const LspLineBounds bounds = lsp_line_bounds_for_line(text, position.line);
    if (bounds.begin == text.size() && position.line > bounds.line) {
        return text.size();
    }
    base::usize character = 0;
    base::usize index = bounds.begin;
    while (index < bounds.end && character < position.character) {
        const LspUtf8CodePoint decoded = lsp_decode_utf8_code_point(text, index, bounds.end);
        const base::usize width = lsp_utf16_units_for_code_point(decoded.code);
        if (character + width > position.character) {
            break;
        }
        character += width;
        index = decoded.next;
    }
    return index;
}

[[nodiscard]] ToolingSourcePosition lsp_tooling_position_from_protocol(
    const ToolingSession& session, const ToolingDocumentId& document, const ToolingSourcePosition position)
{
    const std::optional<ToolingDocumentState> state = session.document_state(document);
    if (!state.has_value()) {
        return position;
    }
    const base::usize offset = lsp_byte_offset_for_utf16_position(state->text, position);
    return tooling_position_for_offset(state->text, offset);
}

void lsp_append_position(std::string& out, const ToolingSourcePosition position)
{
    out.append("{\"line\":");
    out.append(std::to_string(position.line));
    out.append(",\"character\":");
    out.append(std::to_string(position.character));
    out.push_back(LSP_JSON_OBJECT_CLOSE);
}

void lsp_append_position(std::string& out, const base::LineColumn position)
{
    const ToolingSourcePosition zero_based{
        std::max(position.line, LSP_ONE_BASED_MINIMUM) - LSP_ONE_BASED_MINIMUM,
        std::max(position.column, LSP_ONE_BASED_MINIMUM) - LSP_ONE_BASED_MINIMUM,
    };
    lsp_append_position(out, zero_based);
}

void lsp_append_range(std::string& out, const ToolingTextRange& range, const std::string* const text)
{
    out.append("{\"start\":");
    if (text != nullptr) {
        lsp_append_position(out, lsp_position_for_byte_offset(*text, range.range.begin));
    } else {
        lsp_append_position(out, range.start);
    }
    out.append(",\"end\":");
    if (text != nullptr) {
        lsp_append_position(out, lsp_position_for_byte_offset(*text, range.range.end));
    } else {
        lsp_append_position(out, range.end);
    }
    out.push_back(LSP_JSON_OBJECT_CLOSE);
}

void lsp_append_source_position(std::string& out, const ToolingSourcePosition position, const std::string* const text)
{
    if (text == nullptr) {
        lsp_append_position(out, position);
        return;
    }
    const base::usize offset = tooling_offset_for_position(*text, position);
    lsp_append_position(out, lsp_position_for_byte_offset(*text, offset));
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
    if (kind == "method" || kind == "impl_method" || kind == "trait_method") {
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
    if (kind == "trait") {
        return LSP_SYMBOL_KIND_INTERFACE;
    }
    if (kind == "associated_type") {
        return LSP_SYMBOL_KIND_FIELD;
    }
    return LSP_SYMBOL_KIND_CLASS;
}

[[nodiscard]] int lsp_completion_item_kind(const std::string_view kind) noexcept
{
    if (kind == "keyword") {
        return LSP_COMPLETION_ITEM_KIND_KEYWORD;
    }
    if (kind == "function") {
        return LSP_COMPLETION_ITEM_KIND_FUNCTION;
    }
    if (kind == "method" || kind == "impl_method" || kind == "trait_method") {
        return LSP_COMPLETION_ITEM_KIND_METHOD;
    }
    if (kind == "const") {
        return LSP_COMPLETION_ITEM_KIND_CONSTANT;
    }
    if (kind == "struct_field") {
        return LSP_COMPLETION_ITEM_KIND_FIELD;
    }
    if (kind == "local" || kind == "parameter" || kind == "value" || kind == "global") {
        return LSP_COMPLETION_ITEM_KIND_VARIABLE;
    }
    if (kind == "struct" || kind == "opaque_struct") {
        return LSP_COMPLETION_ITEM_KIND_CLASS;
    }
    if (kind == "enum") {
        return LSP_COMPLETION_ITEM_KIND_ENUM;
    }
    if (kind == "enum_case") {
        return LSP_COMPLETION_ITEM_KIND_ENUM_MEMBER;
    }
    if (kind == "type_alias") {
        return LSP_COMPLETION_ITEM_KIND_VALUE;
    }
    if (kind == "generic_template") {
        return LSP_COMPLETION_ITEM_KIND_TYPE_PARAMETER;
    }
    if (kind == "trait") {
        return LSP_COMPLETION_ITEM_KIND_INTERFACE;
    }
    if (kind == "associated_type") {
        return LSP_COMPLETION_ITEM_KIND_FIELD;
    }
    return LSP_COMPLETION_ITEM_KIND_MODULE;
}

[[nodiscard]] base::usize lsp_semantic_token_type_index(const std::string_view token_type) noexcept
{
    const auto found = std::ranges::find(LSP_SEMANTIC_TOKEN_TYPES, token_type);
    if (found == LSP_SEMANTIC_TOKEN_TYPES.end()) {
        return static_cast<base::usize>(std::ranges::find(LSP_SEMANTIC_TOKEN_TYPES, std::string_view{"variable"})
            - LSP_SEMANTIC_TOKEN_TYPES.begin());
    }
    return static_cast<base::usize>(found - LSP_SEMANTIC_TOKEN_TYPES.begin());
}

[[nodiscard]] int lsp_semantic_token_modifier_bits(const std::vector<std::string>& modifiers) noexcept
{
    int bits = 0;
    for (const std::string& modifier : modifiers) {
        if (modifier == "declaration") {
            bits |= LSP_SEMANTIC_TOKEN_MODIFIER_DECLARATION_BIT;
        } else if (modifier == "definition") {
            bits |= LSP_SEMANTIC_TOKEN_MODIFIER_DEFINITION_BIT;
        } else if (modifier == "readonly") {
            bits |= LSP_SEMANTIC_TOKEN_MODIFIER_READONLY_BIT;
        }
    }
    return bits;
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

void lsp_append_diagnostic(std::string& out, const ToolingDiagnostic& diagnostic, const std::string* const text)
{
    out.append("{\"range\":");
    lsp_append_range(out, diagnostic.range, text);
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
    const std::string_view uri, const std::vector<ToolingDiagnostic>& diagnostics, const std::string* const text)
{
    std::string params;
    params.append("{\"uri\":");
    lsp_append_json_escaped(params, uri);
    params.append(",\"diagnostics\":[");
    for (base::usize index = 0; index < diagnostics.size(); ++index) {
        if (index != 0U) {
            params.push_back(LSP_JSON_COMMA);
        }
        lsp_append_diagnostic(params, diagnostics[index], text);
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
    const std::optional<ToolingDocumentState> state = session.document_state(document);
    const std::string* const text = state.has_value() ? &state->text : nullptr;
    return {lsp_publish_diagnostics(document.uri, diagnostics.take_value(), text)};
}

[[nodiscard]] std::vector<std::string> lsp_publish_empty_diagnostics(const std::string_view uri)
{
    return {lsp_publish_diagnostics(uri, {}, nullptr)};
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

[[nodiscard]] ToolingDocumentId lsp_document_for_range(
    const ToolingSession& session, const ToolingDocumentId& fallback, const ToolingTextRange& range)
{
    return range.path.empty() ? fallback : tooling_document_id_from_path(range.path, session.project_config());
}

[[nodiscard]] std::string lsp_uri_for_range(const ToolingDocumentId& fallback, const ToolingTextRange& range)
{
    return range.path.empty() ? fallback.uri : tooling_file_uri_from_path(range.path);
}

[[nodiscard]] std::string lsp_location_json(
    const ToolingSession& session, const ToolingDocumentId& fallback, const ToolingTextRange& range)
{
    const ToolingDocumentId range_document = lsp_document_for_range(session, fallback, range);
    const std::optional<ToolingDocumentState> state = session.document_state(range_document);
    const std::string* const text = state.has_value() ? &state->text : nullptr;
    std::string out;
    out.append("{\"uri\":");
    lsp_append_json_escaped(out, lsp_uri_for_range(fallback, range));
    out.append(",\"range\":");
    lsp_append_range(out, range, text);
    out.push_back(LSP_JSON_OBJECT_CLOSE);
    return out;
}

[[nodiscard]] std::string lsp_hover_result(const ToolingHover& hover, const std::string* const text)
{
    std::string out;
    out.append("{\"contents\":{\"kind\":");
    lsp_append_json_escaped(out, LSP_MARKUP_KIND_PLAINTEXT);
    out.append(",\"value\":");
    lsp_append_json_escaped(out, hover.label);
    out.append("},\"range\":");
    lsp_append_range(out, hover.range, text);
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

[[nodiscard]] std::string lsp_document_symbol_result(
    const std::vector<ToolingDocumentSymbol>& symbols, const std::string* const text)
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
        lsp_append_range(out, symbol.range, text);
        out.append(",\"selectionRange\":");
        lsp_append_range(out, symbol.selection_range, text);
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

[[nodiscard]] std::optional<ToolingDocumentVersion> lsp_document_generation(
    const ToolingSession& session, const ToolingDocumentId& document)
{
    const std::optional<ToolingDocumentState> state = session.document_state(document);
    if (!state.has_value()) {
        return std::nullopt;
    }
    return state->version;
}

[[nodiscard]] bool lsp_generation_is_current(
    const ToolingSession& session, const ToolingDocumentId& document, const ToolingDocumentVersion version)
{
    return session.is_generation_current(document, version);
}

[[nodiscard]] std::string lsp_completion_result(
    const std::vector<ToolingCompletionItem>& completions, const std::string* const text)
{
    std::string out;
    out.append("{\"isIncomplete\":false,\"items\":[");
    for (base::usize index = 0; index < completions.size(); ++index) {
        if (index != 0U) {
            out.push_back(LSP_JSON_COMMA);
        }
        const ToolingCompletionItem& item = completions[index];
        out.append("{\"label\":");
        lsp_append_json_escaped(out, item.label);
        out.append(",\"kind\":");
        out.append(std::to_string(lsp_completion_item_kind(item.kind)));
        out.append(",\"detail\":");
        lsp_append_json_escaped(out, item.detail);
        out.append(",\"textEdit\":{\"range\":");
        lsp_append_range(out, item.replacement_range, text);
        out.append(",\"newText\":");
        lsp_append_json_escaped(out, item.label);
        out.append("},\"data\":{\"aurexKind\":");
        lsp_append_json_escaped(out, item.kind);
        out.append(",\"stableDefinitionKey\":");
        lsp_append_json_escaped(out, item.stable_definition_key);
        out.append(",\"stableMemberKey\":");
        lsp_append_json_escaped(out, item.stable_member_key);
        out.append(",\"partIndex\":");
        out.append(std::to_string(item.part_index));
        out.append(",\"checked\":");
        out.append(item.checked ? "true" : "false");
        out.append(",\"fromWorkspace\":");
        out.append(item.from_workspace ? "true" : "false");
        out.append("}}");
    }
    out.append("]}");
    return out;
}

void lsp_append_text_edit(
    std::string& out, const ToolingTextRange& range, const std::string_view new_text, const std::string* const text)
{
    out.append("{\"range\":");
    lsp_append_range(out, range, text);
    out.append(",\"newText\":");
    lsp_append_json_escaped(out, new_text);
    out.push_back(LSP_JSON_OBJECT_CLOSE);
}

template <typename EditRange>
void lsp_append_workspace_changes(std::string& out, const ToolingSession& session, const std::vector<EditRange>& edits)
{
    out.append("{\"changes\":{");
    std::vector<std::string> uris;
    uris.reserve(edits.size());
    for (const EditRange& edit : edits) {
        const std::string& uri = edit.document.uri;
        if (std::ranges::find(uris, uri) == uris.end()) {
            uris.push_back(uri);
        }
    }
    std::ranges::sort(uris);
    for (base::usize uri_index = 0; uri_index < uris.size(); ++uri_index) {
        if (uri_index != 0U) {
            out.push_back(LSP_JSON_COMMA);
        }
        lsp_append_json_escaped(out, uris[uri_index]);
        out.append(":[");
        bool first_edit = true;
        for (const EditRange& edit : edits) {
            if (edit.document.uri != uris[uri_index]) {
                continue;
            }
            if (!first_edit) {
                out.push_back(LSP_JSON_COMMA);
            }
            first_edit = false;
            const std::optional<ToolingDocumentState> state = session.document_state(edit.document);
            const std::string* const text = state.has_value() ? &state->text : nullptr;
            lsp_append_text_edit(out, edit.range, edit.new_text, text);
        }
        out.push_back(LSP_JSON_ARRAY_CLOSE);
    }
    out.append("}}");
}

[[nodiscard]] std::string lsp_rename_result(const ToolingSession& session, const ToolingRenamePlan& plan)
{
    std::string out;
    lsp_append_workspace_changes(out, session, plan.edits);
    return out;
}

[[nodiscard]] base::usize lsp_semantic_token_length(const ToolingTextRange& range, const std::string& text) noexcept
{
    const ToolingSourcePosition start = lsp_position_for_byte_offset(text, range.range.begin);
    const ToolingSourcePosition end = lsp_position_for_byte_offset(text, range.range.end);
    if (start.line == end.line && end.character >= start.character) {
        return end.character - start.character;
    }
    return end.character;
}

[[nodiscard]] std::string lsp_semantic_tokens_result(
    const std::vector<ToolingSemanticToken>& tokens, const std::string& text)
{
    std::string out;
    out.append("{\"data\":[");
    base::usize previous_line = 0;
    base::usize previous_start = 0;
    for (base::usize index = 0; index < tokens.size(); ++index) {
        if (index != 0U) {
            out.push_back(LSP_JSON_COMMA);
        }
        const ToolingSemanticToken& token = tokens[index];
        const ToolingSourcePosition position = lsp_position_for_byte_offset(text, token.range.range.begin);
        const base::usize line = position.line;
        const base::usize start = position.character;
        const base::usize delta_line = index == 0U ? line : line - previous_line;
        const base::usize delta_start = delta_line == 0U ? start - previous_start : start;
        const base::usize length = lsp_semantic_token_length(token.range, text);
        out.append(std::to_string(delta_line));
        out.push_back(LSP_JSON_COMMA);
        out.append(std::to_string(delta_start));
        out.push_back(LSP_JSON_COMMA);
        out.append(std::to_string(length));
        out.push_back(LSP_JSON_COMMA);
        out.append(std::to_string(lsp_semantic_token_type_index(token.token_type)));
        out.push_back(LSP_JSON_COMMA);
        out.append(std::to_string(lsp_semantic_token_modifier_bits(token.modifiers)));
        previous_line = line;
        previous_start = start;
    }
    out.append("]}");
    return out;
}

[[nodiscard]] std::string lsp_code_action_result(
    const ToolingSession& session, const std::vector<ToolingCodeAction>& actions)
{
    std::string out;
    out.push_back(LSP_JSON_ARRAY_OPEN);
    for (base::usize index = 0; index < actions.size(); ++index) {
        if (index != 0U) {
            out.push_back(LSP_JSON_COMMA);
        }
        const ToolingCodeAction& action = actions[index];
        out.append("{\"title\":");
        lsp_append_json_escaped(out, action.title);
        out.append(",\"kind\":");
        lsp_append_json_escaped(out, action.kind);
        out.append(",\"isPreferred\":");
        out.append(action.preferred ? "true" : "false");
        out.append(",\"edit\":");
        lsp_append_workspace_changes(out, session, action.edits);
        out.append(",\"data\":");
        lsp_append_json_escaped(out, action.data);
        out.push_back(LSP_JSON_OBJECT_CLOSE);
    }
    out.push_back(LSP_JSON_ARRAY_CLOSE);
    return out;
}

[[nodiscard]] std::string lsp_workspace_symbol_result(
    const ToolingSession& session, const std::vector<ToolingWorkspaceSymbol>& symbols)
{
    std::string out;
    out.push_back(LSP_JSON_ARRAY_OPEN);
    for (base::usize index = 0; index < symbols.size(); ++index) {
        if (index != 0U) {
            out.push_back(LSP_JSON_COMMA);
        }
        const ToolingWorkspaceSymbol& symbol = symbols[index];
        out.append("{\"name\":");
        lsp_append_json_escaped(out, symbol.name);
        out.append(",\"kind\":");
        out.append(std::to_string(lsp_symbol_kind(symbol.kind)));
        out.append(",\"location\":");
        out.append(lsp_location_json(session, symbol.document, symbol.range));
        out.append(",\"containerName\":");
        lsp_append_json_escaped(out, symbol.container_name);
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

[[nodiscard]] std::string lsp_inlay_hint_result(
    const std::vector<ToolingInlayHint>& hints, const std::string* const text)
{
    std::string out;
    out.push_back(LSP_JSON_ARRAY_OPEN);
    for (base::usize index = 0; index < hints.size(); ++index) {
        if (index != 0U) {
            out.push_back(LSP_JSON_COMMA);
        }
        const ToolingInlayHint& hint = hints[index];
        out.append("{\"position\":");
        lsp_append_source_position(out, hint.position, text);
        out.append(",\"label\":");
        lsp_append_json_escaped(out, hint.label);
        out.append(",\"kind\":");
        out.append(std::to_string(LSP_INLAY_HINT_KIND_TYPE));
        out.append(",\"data\":{\"aurexKind\":");
        lsp_append_json_escaped(out, hint.kind);
        out.append(",\"checked\":");
        out.append(hint.checked ? "true" : "false");
        out.append("}}");
    }
    out.push_back(LSP_JSON_ARRAY_CLOSE);
    return out;
}

[[nodiscard]] std::string_view lsp_trim_header_spaces(const std::string_view value) noexcept
{
    base::usize begin = 0;
    base::usize end = value.size();
    while (begin < end && (value[begin] == LSP_JSON_SPACE || value[begin] == LSP_JSON_TAB)) {
        ++begin;
    }
    while (end > begin && (value[end - 1U] == LSP_JSON_SPACE || value[end - 1U] == LSP_JSON_TAB)) {
        --end;
    }
    return value.substr(begin, end - begin);
}

[[nodiscard]] bool lsp_ascii_equal_ignore_case(const char lhs, const char rhs) noexcept
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

[[nodiscard]] bool lsp_header_name_equals(const std::string_view actual, const std::string_view expected) noexcept
{
    return actual.size() == expected.size()
        && std::equal(actual.begin(), actual.end(), expected.begin(), expected.end(), lsp_ascii_equal_ignore_case);
}

[[nodiscard]] base::Result<base::usize> lsp_parse_content_length(const std::string_view header)
{
    base::usize search = 0;
    while (search <= header.size()) {
        const base::usize line_end = header.find(LSP_LINE_SEPARATOR, search);
        const std::string_view line =
            line_end == std::string_view::npos ? header.substr(search) : header.substr(search, line_end - search);
        const base::usize colon = line.find(LSP_HEADER_COLON);
        if (colon != std::string_view::npos
            && lsp_header_name_equals(lsp_trim_header_spaces(line.substr(0, colon)), LSP_HEADER_CONTENT_LENGTH)) {
            const std::string_view number = lsp_trim_header_spaces(line.substr(colon + 1U));
            base::usize length = 0;
            const auto [ptr, error] = std::from_chars(number.data(), number.data() + number.size(), length);
            if (error != std::errc{} || ptr != number.data() + number.size()) {
                return base::Result<base::usize>::fail(
                    {base::ErrorCode::invalid_source, "invalid LSP Content-Length header"});
            }
            if (length > LSP_MAX_MESSAGE_BYTES) {
                return base::Result<base::usize>::fail(
                    {base::ErrorCode::invalid_source, "LSP message body is too large"});
            }
            return base::Result<base::usize>::ok(length);
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
        if (content_length.value() > bytes.size() - body_begin) {
            return base::Result<std::vector<LspContentMessage>>::fail(
                {base::ErrorCode::invalid_source, "incomplete LSP body"});
        }
        const base::usize body_end = body_begin + content_length.value();
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
        return id.found ? std::vector<std::string>{lsp_error_response(
                              id.text, LSP_ERROR_INVALID_REQUEST, "JSON-RPC request is missing method")}
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
    if (*method == LSP_METHOD_COMPLETION) {
        return params.found ? this->handle_completion(id.text, params.text)
                            : std::vector<std::string>{lsp_response(id.text, "{\"isIncomplete\":false,\"items\":[]}")};
    }
    if (*method == LSP_METHOD_RENAME) {
        return params.found ? this->handle_rename(id.text, params.text)
                            : std::vector<std::string>{lsp_response(id.text, LSP_NULL)};
    }
    if (*method == LSP_METHOD_SEMANTIC_TOKENS_FULL) {
        return params.found ? this->handle_semantic_tokens(id.text, params.text)
                            : std::vector<std::string>{lsp_response(id.text, "{\"data\":[]}")};
    }
    if (*method == LSP_METHOD_CODE_ACTION) {
        return params.found ? this->handle_code_actions(id.text, params.text)
                            : std::vector<std::string>{lsp_response(id.text, "[]")};
    }
    if (*method == LSP_METHOD_WORKSPACE_SYMBOL) {
        return params.found ? this->handle_workspace_symbols(id.text, params.text)
                            : std::vector<std::string>{lsp_response(id.text, "[]")};
    }
    if (*method == LSP_METHOD_INLAY_HINT) {
        return params.found ? this->handle_inlay_hints(id.text, params.text)
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
    result << "{\"capabilities\":{\"textDocumentSync\":{\"openClose\":true,\"change\":" << LSP_TEXT_DOCUMENT_SYNC_FULL
           << "},\"hoverProvider\":true,\"definitionProvider\":true,\"referencesProvider\":true,"
              "\"documentSymbolProvider\":true,\"completionProvider\":{\"triggerCharacters\":[\".\",\":\"]},"
              "\"renameProvider\":true,\"semanticTokensProvider\":{\"legend\":{\"tokenTypes\":[";
    for (base::usize index = 0; index < LSP_SEMANTIC_TOKEN_TYPES.size(); ++index) {
        if (index != 0U) {
            result << ',';
        }
        std::string escaped;
        lsp_append_json_escaped(escaped, LSP_SEMANTIC_TOKEN_TYPES[index]);
        result << escaped;
    }
    result << "],\"tokenModifiers\":[";
    for (base::usize index = 0; index < LSP_SEMANTIC_TOKEN_MODIFIERS.size(); ++index) {
        if (index != 0U) {
            result << ',';
        }
        std::string escaped;
        lsp_append_json_escaped(escaped, LSP_SEMANTIC_TOKEN_MODIFIERS[index]);
        result << escaped;
    }
    result << "]},\"full\":true},\"codeActionProvider\":true,\"workspaceSymbolProvider\":true,"
              "\"inlayHintProvider\":true}}";
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
    const ToolingSourcePosition tooling_position =
        lsp_tooling_position_from_protocol(this->session_, *document, *position);
    base::Result<std::optional<ToolingHover>> hover = this->session_.hover_at_position(*document, tooling_position);
    if (!hover || !hover.value().has_value()) {
        return {lsp_response(id, LSP_NULL)};
    }
    const std::optional<ToolingDocumentState> state = this->session_.document_state(*document);
    const std::string* const text = state.has_value() ? &state->text : nullptr;
    return {lsp_response(id, lsp_hover_result(*hover.value(), text))};
}

std::vector<std::string> LspServer::handle_definition(const std::string_view id, const std::string_view params)
{
    const std::optional<ToolingDocumentId> document = lsp_document_from_params(params, this->session_.project_config());
    const std::optional<ToolingSourcePosition> position = lsp_position_from_params(params);
    if (!document.has_value() || !position.has_value()) {
        return {lsp_response(id, LSP_NULL)};
    }
    const ToolingSourcePosition tooling_position =
        lsp_tooling_position_from_protocol(this->session_, *document, *position);
    base::Result<std::optional<ToolingDefinition>> definition =
        this->session_.definition_at_position(*document, tooling_position);
    if (!definition || !definition.value().has_value()) {
        return {lsp_response(id, LSP_NULL)};
    }
    return {lsp_response(id, lsp_location_json(this->session_, *document, definition.value()->range))};
}

std::vector<std::string> LspServer::handle_references(const std::string_view id, const std::string_view params)
{
    const std::optional<ToolingDocumentId> document = lsp_document_from_params(params, this->session_.project_config());
    const std::optional<ToolingSourcePosition> position = lsp_position_from_params(params);
    if (!document.has_value() || !position.has_value()) {
        return {lsp_response(id, "[]")};
    }
    const ToolingSourcePosition tooling_position =
        lsp_tooling_position_from_protocol(this->session_, *document, *position);
    base::Result<std::vector<ToolingReference>> references =
        this->session_.references_at_position(*document, tooling_position);
    if (!references) {
        return {lsp_response(id, "[]")};
    }
    std::string result;
    result.push_back(LSP_JSON_ARRAY_OPEN);
    for (base::usize index = 0; index < references.value().size(); ++index) {
        if (index != 0U) {
            result.push_back(LSP_JSON_COMMA);
        }
        result.append(lsp_location_json(this->session_, *document, references.value()[index].range));
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
    const std::optional<ToolingDocumentState> state = this->session_.document_state(*document);
    const std::string* const text = state.has_value() ? &state->text : nullptr;
    return {lsp_response(id, lsp_document_symbol_result(symbols.value(), text))};
}

std::vector<std::string> LspServer::handle_completion(const std::string_view id, const std::string_view params)
{
    const std::optional<ToolingDocumentId> document = lsp_document_from_params(params, this->session_.project_config());
    const std::optional<ToolingSourcePosition> position = lsp_position_from_params(params);
    if (!document.has_value() || !position.has_value()) {
        return {lsp_response(id, "{\"isIncomplete\":false,\"items\":[]}")};
    }
    const std::optional<ToolingDocumentVersion> generation = lsp_document_generation(this->session_, *document);
    if (!generation.has_value()) {
        return {lsp_response(id, "{\"isIncomplete\":false,\"items\":[]}")};
    }
    const ToolingSourcePosition tooling_position =
        lsp_tooling_position_from_protocol(this->session_, *document, *position);
    base::Result<std::vector<ToolingCompletionItem>> completions =
        this->session_.completion_at_position(*document, tooling_position);
    if (!completions || !lsp_generation_is_current(this->session_, *document, *generation)) {
        return {lsp_response(id, "{\"isIncomplete\":false,\"items\":[]}")};
    }
    const std::optional<ToolingDocumentState> state = this->session_.document_state(*document);
    const std::string* const text = state.has_value() ? &state->text : nullptr;
    return {lsp_response(id, lsp_completion_result(completions.value(), text))};
}

std::vector<std::string> LspServer::handle_rename(const std::string_view id, const std::string_view params)
{
    const std::optional<ToolingDocumentId> document = lsp_document_from_params(params, this->session_.project_config());
    const std::optional<ToolingSourcePosition> position = lsp_position_from_params(params);
    const std::optional<std::string> new_name = lsp_json_string_property(params, LSP_PROP_NEW_NAME);
    if (!document.has_value() || !position.has_value() || !new_name.has_value()) {
        return {lsp_response(id, LSP_NULL)};
    }
    const std::optional<ToolingDocumentVersion> generation = lsp_document_generation(this->session_, *document);
    if (!generation.has_value()) {
        return {lsp_response(id, LSP_NULL)};
    }
    const ToolingSourcePosition tooling_position =
        lsp_tooling_position_from_protocol(this->session_, *document, *position);
    base::Result<ToolingRenamePlan> rename = this->session_.rename_at_position(*document, tooling_position, *new_name);
    if (!rename || !rename.value().valid || !lsp_generation_is_current(this->session_, *document, *generation)) {
        return {lsp_response(id, LSP_NULL)};
    }
    return {lsp_response(id, lsp_rename_result(this->session_, rename.value()))};
}

std::vector<std::string> LspServer::handle_semantic_tokens(const std::string_view id, const std::string_view params)
{
    const std::optional<ToolingDocumentId> document = lsp_document_from_params(params, this->session_.project_config());
    if (!document.has_value()) {
        return {lsp_response(id, "{\"data\":[]}")};
    }
    const std::optional<ToolingDocumentVersion> generation = lsp_document_generation(this->session_, *document);
    if (!generation.has_value()) {
        return {lsp_response(id, "{\"data\":[]}")};
    }
    base::Result<std::vector<ToolingSemanticToken>> tokens = this->session_.semantic_tokens(*document);
    if (!tokens || !lsp_generation_is_current(this->session_, *document, *generation)) {
        return {lsp_response(id, "{\"data\":[]}")};
    }
    const std::optional<ToolingDocumentState> state = this->session_.document_state(*document);
    if (!state.has_value()) {
        return {lsp_response(id, "{\"data\":[]}")};
    }
    return {lsp_response(id, lsp_semantic_tokens_result(tokens.value(), state->text))};
}

std::vector<std::string> LspServer::handle_code_actions(const std::string_view id, const std::string_view params)
{
    const std::optional<ToolingDocumentId> document = lsp_document_from_params(params, this->session_.project_config());
    if (!document.has_value()) {
        return {lsp_response(id, "[]")};
    }
    const std::optional<ToolingDocumentVersion> generation = lsp_document_generation(this->session_, *document);
    if (!generation.has_value()) {
        return {lsp_response(id, "[]")};
    }
    base::Result<std::vector<ToolingCodeAction>> actions = this->session_.code_actions(*document);
    if (!actions || !lsp_generation_is_current(this->session_, *document, *generation)) {
        return {lsp_response(id, "[]")};
    }
    return {lsp_response(id, lsp_code_action_result(this->session_, actions.value()))};
}

std::vector<std::string> LspServer::handle_workspace_symbols(const std::string_view id, const std::string_view params)
{
    const std::optional<std::string> query = lsp_json_string_property(params, LSP_PROP_QUERY);
    base::Result<std::vector<ToolingWorkspaceSymbol>> symbols =
        this->session_.workspace_symbols(query.value_or(std::string{}));
    if (!symbols) {
        return {lsp_response(id, "[]")};
    }
    return {lsp_response(id, lsp_workspace_symbol_result(this->session_, symbols.value()))};
}

std::vector<std::string> LspServer::handle_inlay_hints(const std::string_view id, const std::string_view params)
{
    const std::optional<ToolingDocumentId> document = lsp_document_from_params(params, this->session_.project_config());
    if (!document.has_value()) {
        return {lsp_response(id, "[]")};
    }
    const std::optional<ToolingDocumentVersion> generation = lsp_document_generation(this->session_, *document);
    if (!generation.has_value()) {
        return {lsp_response(id, "[]")};
    }
    base::Result<std::vector<ToolingInlayHint>> hints = this->session_.inlay_hints(*document);
    if (!hints || !lsp_generation_is_current(this->session_, *document, *generation)) {
        return {lsp_response(id, "[]")};
    }
    const std::optional<ToolingDocumentState> state = this->session_.document_state(*document);
    const std::string* const text = state.has_value() ? &state->text : nullptr;
    return {lsp_response(id, lsp_inlay_hint_result(hints.value(), text))};
}

} // namespace aurex::tooling
